#include "lpc17xx.h"
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

char keys = {
	['1', '2', '3', 'A'],
	['4', '5', '6', 'B'],
	['7', '8', '9', 'C'],
	['*', '0', '#', 'D'],
};

int main(void)
{
	// Otras cosas...

	for (uint8_t i = 0; i < 4; i++) // Filas (P0.0-3) como output
		configGPIO(PORT(0), BIT(i), OUTPUT);

	for (uint8_t i = 4; i < 8; i++) // Columnas (P0.4-7) como input
		configGPIO(PORT(0), BIT(i), INPUT);

	for (uint8_t i = 0; i < 8; i++) // Outputs para display 7 segmentos
		configGPIO(PORT(2), BIT(i), OUTPUT)

	while (1);

	return 0;
}

void configGPIO(uint8_t port, uint32_t pin,, uint8_t dir)
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
