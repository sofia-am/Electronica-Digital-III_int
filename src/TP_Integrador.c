/*
===============================================================================
 Nombre      : TP_Integrador.c
 Autores     : Amallo, Sofía; Covacich, Axel; Bonino Francisco Ignacio
 Version     : 1.0
 Copyright   : None
 Description : TP final para Electrónica Digital III
===============================================================================
*/

// Librerías a utilizar
#include "lpc17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_timer.h"

// Definiciones útiles
#define INPUT 0
#define OUTPUT 1
#define RISING 0
#define FALLING 1
#define LOWER 0
#define UPPER 1
#define PORT(x) x
#define BIT(x) (1 << x)
#define ROWS 4
#define COLUMNS 4
#define SIZE (ROWS * COLUMNS)
#define SIZEB 10
#define TIMER 600000
#define MAX_SPEED 15 // [Km/h]

// Prototipado de funciones
void cfg_gpio(void);
void delay(void);
void EINT3_IRQHandler(void);
void cfg_capture(void);
void cfg_pwm(void);

uint8_t get_pressed_key(void);

// Variables globales
uint8_t gilada = 0;
uint8_t on = 0; // Flag para encendido
uint8_t vel_digits[2] = { 0, 0 }; // Arreglo para los dígitos de la velocidad
uint8_t vel_index = 0; // Índice para el arreglo de velocidad
uint8_t velocidad = 0;
uint8_t t_anterior = 0; // Tiempo de medición anterior (para capture)
uint8_t t_actual = 0; // Tiempo de medición actual (para capture)
uint8_t t_final = 0; // Tiempo resultante (para capture)
uint8_t t_index = 0; // Índice para recorrer el arreglo de mediciones (para capture)
uint8_t t_resultado = 0; // Promedio de mediciones (para capture)
uint8_t t_flag = 0;  // Flag para indicar división en promediador móvil (para capture)
uint8_t buff[SIZEB]; // Buffer con mediciones (para capture)
uint8_t keys_hex[SIZE] = // Valores en hexadecimal del teclado matricial
{
	0x06, 0x5b, 0x4f, 0x77, // 1 2 3 A
	0x66, 0x6d, 0x7d, 0x7c, // 4 5 6 B
	0x07, 0x7f, 0x67, 0x39, // 7 8 9 C
	0x79, 0x3f, 0x71, 0x5E  // E 0 F D
};
uint8_t keys_dec[SIZE] = // Valores en decimal del teclado matricial
{
	1, 2, 3, 0, // 1 2 3 X
	4, 5, 6, 0, // 4 5 6 X
	7, 8, 9, 0, // 7 8 9 X
	0, 0, 0, 0  // X 0 X X
};
uint32_t p2aux = 0; // Copia auxiliar de la lectura del puerto 2 para antirrebote

/**
 * @brief Función principal. Acá se configuran
 * 		  todos los periféricos.
 */
int main(void)
{
	cfg_gpio();
	cfg_capture();
	cfg_pwm();

	for (uint8_t i = 0; i < 10; i++)
		buff[i] = 0;

	while (1);

    return 0;
}

/**
 * @brief En esta función se configuran
 * 		  los puertos GPIO a utilizar.
 *
 * @details Se configuran P0.0-6 como outputs para
 * 			mostrar el valor de la tecla presionada.
 * 			Se configuran P2.0-3 como outputs representando
 * 			las filas del teclado matricial.
 * 			Se configuran P2.4-7 como inputs representando
 * 			las columnas del teclado matricial.
 */
void cfg_gpio(void)
{
	PINSEL_CFG_Type cfg;

	/****************************************
	 *								        *
	 *        CONFIGURACIÓN PUERTO 0        *
	 *							        	*
	 ****************************************/
	cfg.Portnum = PORT(0);
	cfg.Funcnum = 0; // Función de GPIO
	cfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	cfg.Pinmode = PINSEL_PINMODE_TRISTATE;

	for (uint8_t i = 0; i < 8; i++)
	{
		cfg.Pinnum = i;

		PINSEL_ConfigPin(&cfg);
	}

	GPIO_SetDir(PORT(0), 0xff, OUTPUT);
	GPIO_ClearValue(PORT(0), 0xff);

	/****************************************
	 *								        *
	 *        CONFIGURACIÓN PUERTO 2        *
	 *							        	*
	 ****************************************/
	cfg.Portnum = PORT(2);

	for (uint8_t i = 0; i < 4; i++) // Filas
	{
		cfg.Pinnum = i;

		PINSEL_ConfigPin(&cfg);
	}

	cfg.Pinmode = PINSEL_PINMODE_PULLUP;

	for (uint8_t i = 4; i < 8; i++) // Columnas
	{
		cfg.Pinnum = i;

		PINSEL_ConfigPin(&cfg);
	}

	GPIO_SetDir(PORT(2), 0xf, OUTPUT);
	GPIO_SetDir(PORT(2), (0xf << 4), INPUT);

	// Habilitación de interrupciones por GPIO2
	FIO_IntCmd(PORT(2), (0xf << 4), FALLING);
	FIO_ClearInt(PORT(2), (0xf << 4));

	NVIC_EnableIRQ(EINT3_IRQn);

	/****************************************
	 *								        *
	 *        CONFIGURACIÓN PUERTO 1        *
	 *							        	*
	 ****************************************/
	cfg.Portnum = 1;
	cfg.Pinnum = 18;
	cfg.Pinmode = PINSEL_PINMODE_TRISTATE;
	cfg.Funcnum = 3; // CAP1.0

	PINSEL_ConfigPin(&cfg);
}

/**
 * @brief Handler para las interrupciones por teclado matricial.
 *
 * @details Se deshabilitan las interrupciones por EINT3 y se
 * 			almacena una copia de la lectura del puerto 2.
 * 			Se implementa un antirrebote generado por software.
 * 			Si luego del retardo por software la lectura del
 * 			puerto 2 es la misma (la tecla sigue presionada), se
 * 			interpreta como una pulsación válida y se actúa en
 * 			base a la tecla presionada.
 * 			Finalmente, se limpian las flags de interrupciones y se
 * 			rehabilitan las interrupciones por EINT3.
 */
void EINT3_IRQHandler(void)
{
	NVIC_DisableIRQ(EINT3_IRQn);

	p2aux = GPIO_ReadValue(PORT(2)) & 0xf0;

	delay();

	if (((GPIO_ReadValue(PORT(2)) & 0xf0) - p2aux) == 0)
	{
		uint8_t key = get_pressed_key();
		uint8_t key_hex = keys_hex[key];

		FIO_HalfWordClearValue(PORT(0), LOWER, 0xff);
		FIO_HalfWordSetValue(PORT(0), LOWER, key_hex);

		if (key_hex == 0x77) // Si apreté 'A', enciendo la cinta.
			on = 1;

		if (on)
		{
			switch (key_hex)
			{
				case 0x77:
				{
					//global_init();
					gilada++;
					break;
				}

				case 0x7c: // 'B' = Setear velocidad ingresada
				{
					velocidad = vel_digits[0]*10 + vel_digits[1];

					//set_vel(velocidad);

					vel_index = 0;

					break;
				}

				case 0x39: // 'C' = Resetear todo y apagar
				{
					on = 0;
					vel_index = 0;

					//apagar();

					break;
				}

				case 0x5e: // 'D' = Comenzar a trackear rendimiento
				{
					//track_init();
					TIM_Cmd(LPC_TIM1, ENABLE);
					//TIM_Cmd(LPC_TIM0, ENABLE);
					break;
				}

				case 0x79: // 'E' = Velocidad++
				{
					//set_velocidad(velocidad++);
					gilada *= 2;
					break;
				}

				case 0x71: // 'F' = Velocidad--
				{
					//set_velocidad(velocidad--);
					gilada /= 2;
					break;
				}

				default: // Cualquier número = Velocidad a setear
				{
					if (vel_index < 2)
					{
						vel_digits[vel_index] = keys_dec[key]; // Almaceno el dígito ingresado

						vel_index++;
					}
					else
						//tirar mensaje de error por uart?

					break;
				}
			}
		}
	}

	FIO_ClearInt(PORT(2), (0xf << 4));

	NVIC_EnableIRQ(EINT3_IRQn);
}

/**
 * @brief Esta función representa un pequeño delay para antirrebote.
 */
void delay(void)
{
	for (uint32_t i = 0; i < TIMER; i++);
}

/**
 * @brief Esta función devuelve la coordenada de la tecla pulsada
 * 		  en el teclado matricial de 4x4.
 *
 * @details Para obtener las filas, se envía un '1' por cada fila
 * 			y se analiza el nibble superior del primer byte del
 * 			puerto 2. Si la fila es la correcta, el '1' debería
 * 			llegar y todas las columnas deberían estar en '1',
 * 			por lo que si el nibble superior del primer byte
 * 			es 0xf, estamos en la fila correcta.
 * 			Para obtener la columna, recorremos la copia almacenada
 * 			hasta encontrar un '0' en alguno de los bits del nibble
 * 			inferior del primer byte.
 * 			Teniendo la fila y la columna, se devuelve el valor de
 * 			la tecla resultante mediante la ecuación:
 * 			Key = 4*Row + Column.
 */
uint8_t get_pressed_key(void)
{
	uint8_t row = 0;

	for (uint8_t i = 0; i < ROWS; i++)
	{
		GPIO_SetValue(PORT(2), BIT(i));

		if ((FIO_ByteReadValue(PORT(2), 0) & 0xf0) == 0xf0)
		{
			row = i;

			GPIO_ClearValue(PORT(2), BIT(i));

			break;
		}

		GPIO_ClearValue(PORT(2), BIT(i));
	}

	uint8_t col = 0;

	for (uint8_t i = 0; i < COLUMNS; i++)
		if (!(p2aux & BIT((4 + i))))
		{
			col = i;

			break;
		}

	return ((4 * row) + col);
}

/**
 * @brief Esta función configura el timer 1 en modo capture para
 * 		  tomar mediciones para el cálculo de la frecuencia cardíaca.
 */
void cfg_capture(void)
{
	/* 100Mhz por default
	   Queremos que el timer funcione con una precision de 0.1[s]
	   Configurar el prescaler para 0.1[s] >> 100000[us] */

	TIM_TIMERCFG_Type config;
	TIM_CAPTURECFG_Type config_capture;
	TIM_MATCHCFG_Type config_match;

	/****************************************
	 *								        *
	 *      CONFIGURACIÓN DE CAPTURE        *
	 *							        	*
	 ****************************************/

	config.PrescaleOption = TIM_PRESCALE_USVAL;
	config.PrescaleValue = 100000;

	config_capture.CaptureChannel = 1;
	config_capture.RisingEdge = ENABLE;
	config_capture.FallingEdge = DISABLE;
	config_capture.IntOnCaption = ENABLE;

	TIM_Init(LPC_TIM1, TIM_COUNTER_RISING_MODE, &config);
	TIM_ConfigCapture(LPC_TIM1, &config_capture);

	/****************************************
	 *								        *
	 *       CONFIGURACIÓN DE MATCH         *
	 *							        	*
	 ****************************************/

	config_match.MatchChannel = 0;
	config_match.IntOnMatch = ENABLE;
	config_match.StopOnMatch = DISABLE;
	config_match.ResetOnMatch = ENABLE;
	config_match.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
	config_match.MatchValue = 100; // Hacemos match cada 10 segundos

	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &config);
	TIM_ConfigMatch(LPC_TIM0, &config_match);

	// Habilitación de interrupciones por timers
	NVIC_EnableIRQ(TIMER0_IRQn);
	NVIC_EnableIRQ(TIMER1_IRQn);

	//Seteamos mayor prioridad al match que al capture
	NVIC_SetPriority(TIMER0_IRQn, 5);
	NVIC_SetPriority(TIMER1_IRQn, 10);
}

/**
 * @brief Handler para las interrupciones por capture en CAP1.1.
 */
void TIMER1_IRQHandler(void)
{
	uint8_t acum = 0;

	t_anterior = t_actual;
	t_actual = TIM_GetCaptureValue(LPC_TIM1, 1);
	t_final = t_actual - t_anterior;

	buff[t_index] = t_final;

	if(t_index == SIZEB)
	{
		t_index = 0;

		if(t_flag == 0)
			t_flag = 1; // Completa la primera vuelta
	}
	else
		t_index++;

	for (uint8_t i = 0; i < SIZEB; i++)
		acum += buff[i];

	if(t_flag == 0)
		t_resultado = acum / t_index;
	else
		t_resultado = acum / SIZEB;

	TIM_ClearIntCapturePending(LPC_TIM1, TIM_CR1_INT);
}

/**
 * @brief Handler para las interrupciones por match en MAT0.0.
 */
void TIMER0_IRQHandler(void)
{
	// Almaceno resultado con DMA
}

void cfg_pwm(void){
	PWM_TIMERCFG_Type config;
	config.PrescaleOption = PWM_TIMER_PRESCALE_USVAL;
	config.PrescaleValue = 1; //1 useg
	PWM_MATCHCFG_Type match_config1, match_config0;
	match_config1.IntOnMatch = DISABLE;
	match_config1.StopOnMatch = DISABLE;
	match_config1.ResetOnMatch = ENABLE;
	match_config1.MatchChannel = 1;
	match_config0.IntOnMatch = DISABLE;
	match_config0.StopOnMatch = DISABLE;
	match_config0.ResetOnMatch = ENABLE;
	match_config0.MatchChannel = 0;

	PWM_Init(LPC_PWM1, PWM_MODE_TIMER, &config);
	PWM_Cmd(LPC_PWM1, ENABLE);
	PWM_ConfigMatch(LPC_PWM1, &match_config0);
	PWM_ConfigMatch(LPC_PWM1, &match_config1);
	PWM_MatchUpdate(LPC_PWM1, 0, 1000, PWM_MATCH_UPDATE_NOW); //periodo 1ms
	PWM_MatchUpdate(LPC_PWM1, 1, 250, PWM_MATCH_UPDATE_NOW); //ancho del pulso 250us
	//PWM_ChannelConfig(LPC_PWM1, 1, PWM_CHANNEL_SINGLE_EDGE);
	//PWM_ChannelCmd(LPC_PWM1, 0, ENABLE);
	PWM_ChannelCmd(LPC_PWM1, 1, ENABLE);
}
