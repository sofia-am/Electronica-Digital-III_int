/*
===============================================================================
 Nombre      : TP_Integrador.c
 Autores     : Amallo, Sofía; Covacich, Axel; Bonino Francisco Ignacio
 Version     : 1.0
 Copyright   : None
 Description : TP final para Electrónica Digital III
===============================================================================
*/

#include "lpc17xx.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"

#define INPUT 0
#define OUTPUT 1
#define LOWER 0
#define UPPER 1
#define PORT(x) x
#define BIT(x) (1 << x)
#define ROWS 4
#define COLUMNS 4
#define SIZE 10

void configUART();
void configCapture();
void configDMA();
void configGPIO();
void configPWM();
void configADC();

uint8_t pulsaciones;
uint8_t ppm; //pulsaciones por minuto
uint8_t buffer[10];
uint8_t resultado;
uint8_t acum;
uint8_t index;
uint8_t flag;

/*char keys = {
	["1", "2", "3", "A"],
	["4", "5", "6", "B"],
	["7", "8", "9", "C"],
	["*", "0", "#", "D2"],
};*/

int main(void) {
//---- inicializo todo en 0
	acum = 0;
	resultado = 0;
	ppm = 0;
	index = 0;
	flag = 0;
	for(int i = 0; i<10; i++){
		buff[i] = 0;
	}
//-------------------------

	for (uint8_t i = 0; i < 4; i++) // Filas (P0.0-3) como output
		configGPIO(PORT(0), BIT(i), OUTPUT);

	for (uint8_t i = 4; i < 8; i++) // Columnas (P0.4-7) como input
		configGPIO(PORT(0), BIT(i), INPUT);

	for (uint8_t i = 0; i < 7; i++) // Outputs para display 7 segmentos (P2.0-6)
		configGPIO(PORT(2), BIT(i), OUTPUT);

	while (1);

    return 0 ;
}

void configCapture(){
	//100Mhz por default
	//queremos que el timer funcione con una precision de 0,1 seg
	//configurar el prescaler para 0.1 seg = 100.000 useg

	TIM_TIMERCFG_Type config;
	//TIM_COUNTERCFG_Type config_counter;
	TIM_CAPTURECFG_Type config_capture;

	config.PrescaleOption = TIM_PRESCALE_USVAL;
	config.PrescaleValue = 100000;

	/*//count control register
	config_counter.CounterOption = TIM_COUNTER_INCAP0;
	config_counter.CountInputSelect = 0;*/
	//CAP1.0 -> pin1.18

	//capture control register
	config_capture.CaptureChannel = 0;
	config_capture.RisingEdge = ENABLE;
	config_capture.FallingEdge = DISABLE;
	config_capture.IntOnCaption = ENABLE;

	TIM_Init(LPC_TIM1, TIM_COUNTER_RISING_MODE, config);
	//TIM_ConfigStructInit(TIM_COUNTER_RISING_MODE, config_counter);
	TIM_ConfigCapture(LPC_TIM1, config_capture);
	TIM_Cmd(LPC_TIM1, ENABLE);

//----------------------- END OF COUNTER CONFIG---------------------

	TIM_MATCHCFG_Type config_match;
	config_match.MatchChannel = 0;
	config_match.IntOnMatch = ENABLE;
	config_match.StopOnMatch = DISABLE;
	config_match.ResetOnMatch = ENABLE;
	config_match.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
	config_match.MatchValue = 100;
	//con un PR que se incrementa cada 0.1seg*100 = hace match cada 10 segundos

	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, config);
	TIM_ConfigMatch(LPC_TIM0, config_match);
	TIM_Cmd(LPC_TIM0, ENABLE);

//----------------------- END OF TIMER CONFIG -------------------------

	NVIC_EnableIRQ(TIMER0_IRQn);
	NIVC_EnableIRQ(TIMER1_IRQn);
	NVIC_SetPriority(TIMER0_IRQn, 5);
	NVIC_SetPriority(TIMER1_IRQn, 10);
//tiene mas prioridad la interrupcion por match que por capture
}

void configGPIO(uint8_t port, uint32_t pin, uint8_t dir)
{
	PINSEL_CFG_Type cfg;

	cfg.Portnum = port;
	cfg.Pinnum = pin;
	cfg.Funcnum = 0; // GPIO

	if (port == 2)
		cfg.Pinmode = PINSEL_PINMODE_PULLUP;

	PINSEL_ConfigPin(&pcfg);

	GPIO_SetDir(port, pin, dir);

	if (dir == OUTPUT)
		GPIO_SetValue(port, pin);
}

void TIMER1_IRQHandler(){ //interrupcion por evento en CAP1.0
	pulsaciones++;
	TIM_ClearIntCapturePending(LPC_TIM1, TIM_CR0_INT);
}

void TIMER0_IRQHandler(){ //interrupcion cada 10 segundos
	uint8_t aux;

	//lo que hizo en 10 segundos multiplicado por 6, dividido 60
	aux = (pulsaciones*6);
	ppm = aux/60;

	buff[index] = ppm;

	if(index == SIZE){
		index = 0;
		if(flag == 0){
			flag = 1; //completa la primera vuelta
		}
	}else index++;

	for(i = 0; i < SIZE; i++){
		acum += buff[i];
	}

	if(flag == 0) resultado = acum/index;
	else resultado = acum/SIZE;

	//almaceno resultado con DMA
	acum = 0;
	pulsaciones = 0;
}

/**
 * @brief Este handler detecta la tecla presionada en el teclado matricial
 * 		  cada vez que ocurre una interrupción por P0.4-7.
 *
 * @details Se pone en 0 cada fila y se testea cuál columna pasa a 0.
 * 			Ante la primer coincidencia, se le asigna a la variable 'key'
 * 			el valor tomado de la matriz de teclas y se deja de buscar.
 */
void EINT3_IRQHandler(void)
{
	uint8_t found = 0;
	uint8_t column = 0;
	char key;

	for (uint8_t row = 0; row < ROWS; row++) // Recorro filas
	{
		if (!found)
		{
			GPIO_ClearValue(PORT(0), BIT(row)); // Fila a 0

			for (uint8_t col = 0; col < COLUMNS; col++) // Recorro columnas
				if (!GPIO_ReadValue(PORT(0), BIT(4 + col)))
				{
					key = keys[row][col]; // Almaceno el valor cuando hallé coincidencia

					found = 1;

					column = col;

					break;
					// ABER ESTE COMMIT
				}

			GPIO_SetValue(PORT(0), BIT(row)); // Fila a 1
		}
		else
			break;
	}

	// Muestro 'key' por display 7 segmentos
	FIO_HalfWordClearValue(PORT(2), LOWER, 0xff);
	FIO_HalfWordSetValue(PORT(2), LOWER, key);

	GPIO_ClearInt(PORT(0), column); // Limpio flag de interrupción y termino
}
