/*
 * ZH.c
 *
 * Created: 12/1/2022 10:47:57 AM
 * Author : Armin
 */ 

#include <avr/io.h>
#include "adc/adc.h"
#include "buttons/button.h"
#include "init/init.h"
#include "lcd/lcd.h"
#include "sevseg/sevseg.h"
#include "uart/uart.h"


void MenuPrint();				// Kiirja a menüt lcd-re
void ButtonController();		// Gombokat iranyitja
void MenuToUart();				// Kiír egy felületet az UART-ra ahol meg lehet adni a dolgokat
void InputFromUart();			// Figyelni a beírt dolgokat
void SendDataToLcd();			// Send the results to the LCD
void SendDataToSevseg();		// Send the results to the sevseg
uint8_t string_comp(char* p_menu, char* p_text);		// Össze hasonlit ket stringet



// For debugging
uint8_t leds = 0x01;
uint8_t i = 0;

// Gomboknak
uint8_t ButtonInput = 0;
uint8_t CurrentMode = 0;
uint8_t ButtonHold = 0;
uint8_t ButtonNewData = 0;

uint8_t IsSevsegEnabled = 0;
uint8_t IsLcdEnabled = 0;



// Usart bejöv? adat
uint8_t UsartInput = 0;
char Szoveg[12] = {0};
uint8_t Szoveg_i = 0;
char* UartPointer = 0;
uint8_t UartEquals = 0;
uint8_t UsartCurrentMode = 0;

// ADC változói
uint16_t AdcResult = 0;

// LCD homerseklet
uint16_t Temp_C = 0;
uint16_t Temp_F = 0;
uint16_t Temp_K = 0;
char TempChar[] = "00°C 000°F 000K ";




// LCD menü
char* Menu[4] = {
	"1 - Homerseklet ",
	"2 - Feszultseg  ",
	"3 - Aram        ",
	"4 - Ellenallas  "
};

char* UartMenu[4] = {
	"Homerseklet",
	"Feszultseg",
	"Aram",
	"Ellenallas"
};

int main(void) {

	InitPorts();
	
	LCD_Initialization();
	LCD_DisplayClear();
	MenuPrint();
	LCD_CursorOff();
	
	UsartInit(MYUBRR);
	UsartCursorBlinkOff();
	UsartClearTerminal();
	
	ADC_Init(0x00, 0x07);
	ADC_EnableInterrupts();
	
	InitTimer();

    while (1) {
		
    }
}



// Timer0 overflow interrupt routine
ISR(TIMER2_OVF_vect) {
	sei();
	
	if (IsSevsegEnabled) {
		SendDataToSevseg();
	}
	
	ButtonController();
}



ISR(TIMER1_COMPA_vect) {
	if (UsartCurrentMode == 1) {
		if (!ADC_IsInConversion()) {
			ADC_ConvStart(0x00);
		}
	}
	
	if (IsLcdEnabled) {
		SendDataToLcd();
	}
}


// Uasrt incoming interrupt
ISR(USART0_RX_vect) {
	UsartInput = UDR0;
	InputFromUart();
}


ISR(ADC_vect) {
	AdcResult = ADC;
	AdcResult = AdcResult >> 2;
	AdcResult = AdcResult/10;
	Temp_C = AdcResult >> 2;
	
	Temp_K = Temp_C + 273;
	Temp_F = (Temp_C * 1.8) + 32;
	
	TempChar[1] = (Temp_C % 10) + '0';
	TempChar[0] = (Temp_C/10 % 10) + '0';

	TempChar[7] = (Temp_F % 10) + '0';
	TempChar[6] = (Temp_F/10 % 10) + '0';
	TempChar[5] = (Temp_F/100 % 10) + '0';

	TempChar[13] = (Temp_K % 10) + '0';
	TempChar[12] = (Temp_K/10 % 10) + '0';
	TempChar[11] = (Temp_K/100 % 10) + '0';
	
	ADCSRA &= ~(1<<ADIF);
}



void SendDataToSevseg() {
	switch (UsartCurrentMode) {
		case 0:
			SevsegOut(0000);
			break;
		case 1:
			SevsegOut(Temp_C);
			break;
		case 2:
			SevsegOut(1234);
			break;
		case 3:
			SevsegOut(8888);
			break;
		case 4:
			SevsegOut(2022);
			break;
	}
}



void SendDataToLcd() {
	LCD_DisplayClear();
	
	switch (UsartCurrentMode) {
		case 0:
			LCD_SendStringToLine("Nincs mode", 0);
			LCD_SendStringToLine("  kivalasztva!", 1);
			LCD_SendStringToLine("Valassza ki", 2);
			LCD_SendStringToLine("  az uartrol!", 3);
			break;
		case 1:
			LCD_SendStringToLine("Homerseklet:", 0);
			LCD_SendStringToLine(TempChar, 1);
			break;
		case 2:
			LCD_SendStringToLine("Feszultseg:", 0);
			LCD_SendStringToLine("####.# V", 1);
			break;
		case 3:
			LCD_SendStringToLine("Aram:", 0);
			LCD_SendStringToLine("###.## mA", 1);
			break;
		case 4:
			LCD_SendStringToLine("Ellenallas:", 0);
			LCD_SendStringToLine("####.# Ohm", 1);
			break;
	}
}




// Uart beállítás beküldés
void InputFromUart() {
	
	if(UsartInput != '\r' && UsartInput != '\b'){
		UsartTransmit(UsartInput);
		Szoveg[Szoveg_i] = UsartInput;
		Szoveg_i++;
	}
	
	if(Szoveg_i >= 12){
		UsartTransmitString("\r\n\nROSSZ MUVELET!\r");
		UsartTransmitString("\e[11;0H");
		UsartClearLine();
		
		for(i=0; i<10; i++){
			Szoveg[i] = 0;
		}
		
		Szoveg_i = 0;
		UsartInput = 0;
	}
	
	if(UsartInput == '\b'){
		if(Szoveg_i){
			Szoveg_i--;
			Szoveg[Szoveg_i] = 0;
			UsartTransmitString("\b \b");
		}
	}
	
	
	if (UsartInput == '\r') {
		Szoveg[Szoveg_i] = '\0';
		
		for (i=0; i<7; i++) {
			UartPointer = UartMenu[i];
			if (string_comp(UartPointer, Szoveg)) {
				UartEquals = i + 1;
				UsartTransmitString("\r\n\nSIKERES MUVELET!\r");
				UsartTransmitString("\e[11;0H");
				UsartClearLine();
			}
		}
		
		if (!UartEquals) {
			UsartTransmitString("\r\n\nROSSZ MUVELET!\r");
			UsartTransmitString("\e[11;0H");
			UsartClearLine();
		}
		
		switch (UartEquals) {
			case 1:
				UsartCurrentMode = 1;
				break;
			case 2:
				UsartCurrentMode = 2;
				break;
			case 3:
				UsartCurrentMode = 3;
				break;
			case 4:
				UsartCurrentMode = 4;
				break;
			default:
				break;
		}
		
		for (i=0; i<10; i++) {
			Szoveg[i] = 0;
		}
		
		Szoveg_i = 0;
		UsartInput = 0;
		UartEquals = 0;
	}
	
}






// Gomb kontroller
void ButtonController() {
	ButtonInput = ButtonManager();
	
	if (ButtonInput) {
		if (!ButtonHold) {
			ButtonHold = 1;
			ButtonNewData = 1;
			CurrentMode = ButtonInput;
		}
	} else {
		ButtonHold = 0;
	}
	
	if (ButtonNewData) {
		ButtonNewData = 0;
		
		switch (CurrentMode) {
			case 1:
				MenuPrint();
				IsLcdEnabled = 0;
				break;
			case 2:
				MenuToUart();
				break;
			case 3:
				IsSevsegEnabled ^= 0x01;
				if (!IsSevsegEnabled) {
					SevsegOff();
				}
				break;
			case 4:
				IsLcdEnabled ^= 0x01;
				if (IsLcdEnabled) {
					SendDataToLcd();
				} else {
					LCD_DisplayClear();
				}
				break;
			case 5:
				break;
		}
	}
}


// Kiír egy menüt uart-ra
void MenuToUart() {
	uint8_t i = 0;
	
	UsartClearTerminal();
	UsartTransmitString("Valasszon muveletet:\r\n\n");
	
	for (i=0; i<4; i++) {
		UsartTransmitString(UartMenu[i]);
		UsartTransmitString("\r\n");
	}
	
	UsartTransmitString("\r\n\nIrja be a muvelet nevet:\r\n\n");
	UsartCursorBlinkOn();
	
}




// Ossze hasonlit ket stringet
uint8_t string_comp(char* p_menu, char* p_text) {
	
	while(1) {
		
		if (*p_menu != *p_text) {
			return 0;
		}
		
		if (!(*p_menu)) {
			if (*p_text) {
				return 0;
			} else {
				return 1;
			}
		}
		
		p_menu++;
		p_text++;
	}
}





// Kiirja LCD-re a fomenut
void MenuPrint() {
	uint8_t i = 0;
	
	for (i=0; i<4; i++) {
		LCD_SendStringToLine(Menu[i], i);
	}
}

