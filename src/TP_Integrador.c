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

void TIMER1_IRQHandler(){ //interrupcion por evento en CAP1.0
	pulsaciones++;
	TIM_ClearIntCapturePending(LPC_TIM1, TIM_CR0_INT);
}

void TIMER0_IRQHandler(){ //interrupcion cada 10 segundos
	uint8_t aux;

	aux = (pulsaciones*6);
	ppm = aux/60;
//lo que hizo en 10 segundos multiplicado por 6, dividido 60

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

