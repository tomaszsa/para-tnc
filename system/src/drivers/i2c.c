#include "drivers/i2c.h"
#include "drivers/gpio_conf.h"
#include <stm32f10x.h>

int i2cPinRemap = 0;
//int i2cClockSpeed = 8;  // w realu tu powinno by� 5 przy moich ustawieniach zegara
int i2cClockSpeed = 5;
int i2cCCRegisterValue = 0x78;
//int i2cRiseRegisterValue = 0x09;	// w realu 6
int i2cRiseRegisterValue = 0x06;

volatile int i2cRemoteAddr = 0;			// adres zdalnego urz�dzenia
volatile int i2cTXData[32] = {'\0'};		// dane do wys�ania do zdalnego urz�dzenia
volatile int i2cRXData[32] = {'\0'};		// dane odebrane od zdalnego urz�dzenia
volatile int i2cRXing = 0;				// ustawiony na 1 kiedy trwa odbi�r danych
volatile int i2cTXing = 0;				// ustawiony na 1 kiedy trwa wysy�anie danych
volatile int i2cDone = 0;				// ustawiany na jeden w momencie zako�czenia wysy�ania/odbioru
volatile int i2cTXQueueLen = 0;			// liczba bajt�w w kolejce do wys�ania
volatile int i2cTRXDataCounter = 0;		// licznik odebranych/wyslanych danych
volatile int i2cRXBytesNumber = 0;		// liczba bajtow do odebrania
volatile int i2cErrorCounter = 0;		// liczbnik b��d�w transmisji

void i2cConfigure() {			// funkcja konfiguruje pierwszy kontroler i2c!!!
	I2C_InitTypeDef I2C_InitStructure;

	RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;	// w��czenie zegara dla i2c
	NVIC_EnableIRQ( I2C1_EV_IRQn );		// w��czenie w kontrolerze przerwan
	NVIC_EnableIRQ( I2C1_ER_IRQn );
	DBGMCU->CR |= DBGMCU_CR_DBG_I2C1_SMBUS_TIMEOUT; // wy��czanie timeout�w podczas debugowania
	if (i2cPinRemap == 0) {
		Configure_GPIO(GPIOB,6,AFOD_OUTPUT_2MHZ);		//SCL
		Configure_GPIO(GPIOB,7,AFOD_OUTPUT_2MHZ);		//SDA
	}
	else {
		AFIO->MAPR |= AFIO_MAPR_I2C1_REMAP;
		Configure_GPIO(GPIOB,8,AFOD_OUTPUT_2MHZ);
		Configure_GPIO(GPIOB,9,AFOD_OUTPUT_2MHZ);				
	}
//	NVIC->IP[31] = 0x55;
	NVIC_SetPriority(I2C1_EV_IRQn, 1);

	I2C_StructInit(&I2C_InitStructure);

	/* Konfiguracja I2C */
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed = 50000;

	/* Potwierdzamy konfigurację przed włączeniem I2C */
	I2C_Init(I2C1, &I2C_InitStructure);

	/* Włączenie I2C */
	I2C_Cmd(I2C1, ENABLE);

	I2C1->CR2 |= I2C_CR2_ITEVTEN;		// w��czenie generowanie przerwania od zdarzen i2c
	I2C1->CR2 |= I2C_CR2_ITBUFEN;
	I2C1->CR2 |= I2C_CR2_ITERREN;

}

int i2cSendData(int addr, int* data, int null) {
	int i;
	for (i = 0; (i<32 && *(data+i) != '\0'); i++)
		i2cTXData[i]=data[i];
	if (null == 0x01) {					// je�eli do slave trzeba wys�a� 0x00
		i2cTXData[0] = 0x00;
		i = 1;
	}
	i2cTXQueueLen = i;
	i2cRemoteAddr = addr;
	
	i2cTXing = 1;
	i2cErrorCounter = 0;
	I2C1->CR1 |= I2C_CR1_START;			// zadanie warunkow startowych
	return 0;
}

int i2cReceiveData(int addr, int* data, int num) {
	i2cRXBytesNumber = num;
	i2cRemoteAddr = addr;
	i2cTRXDataCounter = 0;
	i2cRXing = 1;
	I2C1->CR1 |= I2C_CR1_START;			// zadanie warunkow startowych
	return 0;		 
}

void I2C1_EV_IRQHandler(void) {
NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
//	int i;
	if ((I2C1->SR1 & I2C_SR1_SB) == I2C_SR1_SB && (i2cTXing == 1 || i2cRXing == 1)) {
	// po nadaniu warunk�w startu podczas wysy�ania danych do slave  EV5
		I2C1->DR = i2cRemoteAddr;				// wpisanie do DR adresu slave do nadania		
		I2C1->SR1 &= (0xFFFFFFFF ^ I2C_SR1_SB);	// gaszenie flagi SB



//		I2C1->SR1 &  I2C_SR1_SB;
	
	}
	if ((I2C1->SR1 & I2C_SR1_ADDR) == I2C_SR1_ADDR && (i2cTXing == 1 || i2cRXing == 1)) {
	// po wys�aniu adresu slave      EV6
		I2C1->SR1 &= (0xFFFFFFFF ^ I2C_SR1_ADDR);				
		I2C1->SR2 &= (0xFFFFFFFF ^ I2C_SR2_TRA);

		if (i2cRXBytesNumber == 1 && i2cRXing == 1) {
			/// EV_6_1
			I2C1->CR1 &= (0xFFFFFFFF ^ I2C_CR1_ACK);	// wy�czanie odpowiadania ACK przy odbiorze je�eli tylko 1 bjt
			I2C1->CR1 |= I2C_CR1_STOP;
			}

		if (i2cRXing == 1)
			I2C1->CR1 |= I2C_CR1_ACK;			// ustawienie bity ACK								
	}
	if ((I2C1->SR1 & I2C_SR1_TXE) == I2C_SR1_TXE && i2cTXing == 1) {
	//    EV_8_1
		I2C1->DR = i2cTXData[0];		
		i2cTRXDataCounter++;				
	}
	if (i2cTRXDataCounter == i2cTXQueueLen && i2cTXing == 1) {
	// przes�ano wszystkie dane, czyli teraz trzeba da� warunki STOP
		i2cTXing = 0;
		I2C1->CR1 |= I2C_CR1_STOP;
		while ((I2C1->CR1 & I2C_CR1_STOP) == I2C_CR1_STOP);

		i2cVariableReset();
	}
	if ((I2C1->SR1 & I2C_SR1_BTF) == I2C_SR1_BTF && i2cTXing == 1) { 
	// EV_8
		if ((I2C1->SR1 & I2C_SR1_TXE) == I2C_SR1_TXE && i2cTXing == 1 && i2cTRXDataCounter < i2cTXQueueLen) {	
		I2C1->DR = i2cTXData[i2cTRXDataCounter];
		i2cTRXDataCounter++;
		I2C1->SR1 &= (0xFFFFFFFF ^ I2C_SR1_BTF);
		}					
	}	 
	if ((I2C1->SR1 & I2C_SR1_RXNE) == I2C_SR1_RXNE && i2cRXing == 1) {
	// EV_7
		*(i2cRXData + i2cTRXDataCounter) = I2C1->DR & I2C_DR_DR;
		i2cTRXDataCounter++;
		if (i2cRXBytesNumber-i2cTRXDataCounter == 1) {
			I2C1->CR1 &= (0xFFFFFFFF ^ I2C_CR1_ACK);	//jezeli odebrano przedostatni bajt to trzeba
														// zgasic bit ACK zeby nastepnie wyslano NACK na koniec	
		}
		if (i2cRXBytesNumber-i2cTRXDataCounter == 0) {
			I2C1->CR1 |= I2C_CR1_STOP;		// po odczytaniu z rejestru DR ostatniego bajtu w sekwencji
											// nast�puje wys�anie warunk�w STOP na magistrale
			while ((I2C1->CR1 & I2C_CR1_STOP) == I2C_CR1_STOP);
			i2cRXing = 0;
			*(i2cRXData + i2cTRXDataCounter) = '\0';
			i2cVariableReset();
						
		}
													
	}


	
}

void I2C1_ER_IRQHandler(void) {
	if (((I2C1->SR1 & I2C_SR1_AF) == I2C_SR1_AF) && i2cTRXDataCounter == 0 ) {
		// slave nie odpowiedzia� ack na sw�j adres
		I2C1->SR1 &= (0xFFFFFFFF ^ I2C_SR1_AF);
		I2C1->CR1 |= I2C_CR1_STOP;		// zadawanie warunkow STOP i przerywanie komunikacji
		while ((I2C1->CR1 & I2C_CR1_STOP) == I2C_CR1_STOP);
		i2cErrorCounter++;				// zwieksza wartosc licznika b��d�w transmisji
		if(i2cErrorCounter >= 3) {
		//je�eli wykryto ju� trzy b��dy to przerwij
			i2cRXing = 0;			
			i2cTXing = 0;			
			i2cTXQueueLen = 0;		
			i2cTRXDataCounter = 0;
			i2cRXBytesNumber = 0;
//			NVIC_DisableIRQ( I2C1_ER_IRQn );
//			NVIC_DisableIRQ( I2C1_EV_IRQn );		
		}
		else
			I2C1->CR1 |= I2C_CR1_START;		// ponawianie komunikacji				

	}
	if (((I2C1->SR1 & I2C_SR1_AF) == I2C_SR1_AF) && i2cTRXDataCounter != 0 ) {
		//jezeli slave nie odpowiedzia� ack na wys�any do niego bajt danych
		I2C1->SR1 &= (0xFFFFFFFF ^ I2C_SR1_AF);
		i2cErrorCounter++;
		if(i2cErrorCounter >= 3) {
			i2cRXing = 0;			
			i2cTXing = 0;			
			i2cTXQueueLen = 0;		
			i2cTRXDataCounter = 0;
			i2cRXBytesNumber = 0;
//			NVIC_DisableIRQ( I2C1_ER_IRQn );
//			NVIC_DisableIRQ( I2C1_EV_IRQn );						
		}
		else  
			i2cTRXDataCounter--;	// zmniejszanie warto�ci licznika danych aby nadac jeszcze raz to samo

	}
}

void i2cVariableReset(void) {
	I2C1->DR = 0x00;
	i2cTRXDataCounter = 0;
	i2cTXQueueLen = 0;
	i2cRXBytesNumber = 0;
}
