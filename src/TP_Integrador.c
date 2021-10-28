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

void configUART();
void configCapture();
void configDMA();
void configGPIO();
void configPWM();
void configADC();

int main(void) {

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
	config_capture.IntOnCaption = DISABLE;

	TIM_Init(LPC_TIM1, TIM_COUNTER_RISING_MODE, config);
	//TIM_ConfigStructInit(TIM_COUNTER_RISING_MODE, config_counter);
	TIM_ConfigCapture(LPC_TIM1, config_capture);
	TIM_Cmd(LPC_TIM1, ENABLE);

}



