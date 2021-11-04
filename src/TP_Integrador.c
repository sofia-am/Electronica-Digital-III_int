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
#include "lpc17xx_pwm.h"
#include "lpc17xx_uart.h"

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
#define MAX_SPEED 20 // [Km/h]
#define PWMPRESCALE (25-1)

// Prototipado de funciones
void cfg_gpio(void);
void cfg_capture(void);
void cfg_pwm(void);
void cfg_uart(void);
void delay(void);
void stop(void);
void set_vel(uint8_t velocidad);

uint8_t get_pressed_key(void);

// Variables globales
uint8_t on = 0; // Flag para encendido
uint8_t vel_digits[2] = { 0, 0 }; // Arreglo para los dígitos de la velocidad
uint8_t vel_index = 0; // Índice para el arreglo de velocidad
uint8_t velocidad = 0;
uint8_t t_anterior = 0; // Tiempo de medición anterior (para capture)
uint8_t t_actual = 0; // Tiempo de medición actual (para capture)
uint8_t t_final = 0; // Tiempo resultante (para capture)
uint8_t t_index = 0; // Índice para recorrer el arreglo de mediciones (para capture)
uint8_t t_resultado = 0; // Promedio de mediciones (para capture)
uint8_t ppm = 0;
uint8_t t_flag = 0;  // Flag para indicar división en promediador móvil (para capture)
uint8_t	t_clicks = 0;
uint8_t	t_clicksb = 0;
uint8_t buff[SIZEB]; // Buffer con mediciones (para capture)
uint8_t pwm_high = 0;
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
	cfg_uart();

	for (uint8_t i = 0; i < 10; i++)
		buff[i] = 0;

	uint8_t info[] = "Hola mundo\t-\tElectr�nica Digital 3\t-\tFCEFyN-UNC \n\r";

	while (1)
	{
		UART_Send(LPC_UART2, info, sizeof(info), BLOCKING);
	}

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

	cfg.Funcnum = 1;
	cfg.OpenDrain = 0;
	cfg.Pinmode = 0;
	cfg.Pinnum = 10;	//TX

	PINSEL_ConfigPin(&cfg);

	cfg.Pinnum = 11;	//RX
	PINSEL_ConfigPin(&cfg);

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
	cfg.Pinnum = 19;
	cfg.Pinmode = PINSEL_PINMODE_PULLUP;
	cfg.Funcnum = 3; // CAP1.1

	PINSEL_ConfigPin(&cfg);

	cfg.Pinnum = 18;
	cfg.Funcnum = 2;

	PINSEL_ConfigPin(&cfg); // PWM1.1

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

		if (key_hex == 0x77) // Si apreté 'A', enciendo la cinta
			on = 1;

		if (on) // Sólo tomo inputs si la cinta está encendida
		{
			switch (key_hex)
			{
				case 0x77: // 'A' = Habilitamos PWM
				{
					//global_init();
					//PWM_Cmd(LPC_PWM1, ENABLE);
					cfg_pwm();
					LPC_PWM1->TCR = (1<<0) | (1<<3); //enable counters and PWM Mode

					break;
				}

				case 0x7c: // 'B' = Setear velocidad ingresada
				{
					velocidad = vel_digits[0]*10 + vel_digits[1];

					set_vel(velocidad);

					vel_index = 0;

					break;
				}

				case 0x39: // 'C' = Resetear todo y apagar
				{
					on = 0;
					vel_index = 0;

					stop();

					break;
				}

				case 0x5e: // 'D' = Comenzar a trackear rendimiento
				{
					//track_init();
					TIM_Cmd(LPC_TIM1, ENABLE);

					break;
				}

				case 0x79: // 'E' = Velocidad++
				{
					if(velocidad < MAX_SPEED)
					{
						velocidad++;

						set_vel(velocidad);
					}

					break;
				}

				case 0x71: // 'F' = Velocidad--
				{
					if(velocidad > 0)
					{
						velocidad--;

						set_vel(velocidad);
					}

					break;
				}

				default: // Cualquier número = Velocidad a setear
				{
					if (vel_index < 2)
					{
						vel_digits[vel_index] = keys_dec[key]; // Almaceno el dígito ingresado

						vel_index++;
					}
					//else >> tirar mensaje de error por uart?

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
 * 			key = (4 * row) + column
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
	config.PrescaleValue = 100000; //1 mseg

	config_capture.CaptureChannel = 1;
	config_capture.RisingEdge = DISABLE;
	config_capture.FallingEdge = ENABLE;
	config_capture.IntOnCaption = ENABLE;

	TIM_Init(LPC_TIM1, TIM_TIMER_MODE, &config);
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
	//NVIC_EnableIRQ(TIMER0_IRQn);
	NVIC_EnableIRQ(TIMER1_IRQn);

	//Seteamos mayor prioridad al match que al capture
	//NVIC_SetPriority(TIMER0_IRQn, 5);
	NVIC_SetPriority(TIMER1_IRQn, 10);
}

void cfg_uart(void)
{
	UART_CFG_Type UARTConfigStruct;
	UART_FIFO_CFG_Type UARTFIFOConfigStruct;
	//configuraci�n por defecto:
	UART_ConfigStructInit(&UARTConfigStruct);
	//inicializa perif�rico
	UART_Init(LPC_UART2, &UARTConfigStruct);
	UART_FIFOConfigStructInit(&UARTFIFOConfigStruct);
	//Inicializa FIFO
	UART_FIFOConfig(LPC_UART2, &UARTFIFOConfigStruct);
	//Habilita transmisi�n
	UART_TxCmd(LPC_UART2, ENABLE);
	return;
}

/**
 * @brief Handler para las interrupciones por capture en CAP1.1.
 */
void TIMER1_IRQHandler(void)
{
	uint8_t acum = 0;
	t_clicksb++;
	//delay();

	t_anterior = t_actual;
	t_actual = TIM_GetCaptureValue(LPC_TIM1, 1);
	t_final = t_actual - t_anterior;

	if(t_final > 1){
		t_clicks++;
		buff[t_index] = t_final;

		if(t_index == (SIZEB-1))
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
			t_resultado = acum / (t_index + 1);
		else
			t_resultado = acum / SIZEB;

		ppm = 600/t_resultado;
	}

	TIM_ClearIntCapturePending(LPC_TIM1, TIM_CR1_INT);
}

/**
 * @brief Handler para las interrupciones por match en MAT0.0.
 */
void TIMER0_IRQHandler(void)
{
	// Almaceno resultado con DMA
}

void cfg_pwm(void)
{
	PWM_TIMERCFG_Type config;
	config.PrescaleOption = PWM_TIMER_PRESCALE_USVAL;
	config.PrescaleValue = 1;

	PWM_Init(LPC_PWM1, PWM_MODE_TIMER, &config);

	LPC_PWM1->PCR = 0x0; //Select Single Edge PWM - by default its single Edged so this line can be removed
	//LPC_PWM1->PR = PWMPRESCALE; //1 micro-second resolution
	LPC_PWM1->MR0 = 1000; //1000us = 1ms period duration
	LPC_PWM1->MR1 = 1000; //20us - default pulse duration i.e. width
	LPC_PWM1->MCR = (1<<1); //Reset PWM TC on PWM1MR0 match
	LPC_PWM1->LER = (1<<1) | (1<<0); //update values in MR0 and MR1
	LPC_PWM1->PCR = (1<<9); //enable PWM output
	LPC_PWM1->TCR = (1<<1); //Reset PWM TC & PR


}

/**
 * @brief Esta función se encarga de setear la velocidad
 * 		  ingresada por teclado.
 *
 * @details Se hace una regla de 3 simple para un máximo
 * 		  	de 15[Km/h], logrando así que si el usuario
 * 		  	quiere ir a la máxima velocidad, el duty-cycle
 * 		  	del motor controlado por PWM será del 100%.
 */
void set_vel(uint8_t velocidad)
{
	uint32_t cycle_rate = 0;

	if(velocidad <= MAX_SPEED)
	{
		cycle_rate = velocidad * (50);

		LPC_PWM1->MR1 = cycle_rate; //Update MR1 with new value
		LPC_PWM1->LER = (1<<1); //Load the MR1 new value at start of next cycle
	}
}

/**
 * @brief Esta función se encarga de deshabilitar los periféricos y
 * 		  resetear las variables necesarias para llevar el equipo a 0.
 */
void stop(void)
{
	set_vel(0);
	PWM_Cmd(LPC_PWM1, DISABLE);

	TIM_Cmd(LPC_TIM1, DISABLE);
	TIM_Cmd(LPC_TIM0, DISABLE);

}
