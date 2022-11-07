// Wire cutter project 
// Copyright (c) 2022, Aleksey Sedyshev
// https://github.com/AlekseySedyshev
//
#include "stm32f0xx.h" // Device header
#include "main.h"

#include "LCD_M15SGF.h" // 101 x 80
#include <stdbool.h>

#define FILTR_TIME 50u
#define BACK_CONST 30u
#define KNIFE_OPEN 20U
#define KNIFE_CLOSE 40U
#define KNIFE_TIME 500u

#define SCREEN_UPDATE 100u
#define FLASH_PAGE16 0x8003C00u

#define FLASH_FKEY1 0x45670123u
#define FLASH_FKEY2 0xCDEF89ABu

//--------new config-------------------
// PF1 - Key-Down, PF0 - Key-UP, PA2 - Key-Right, PA9 - Key-Left
// PA1 - Dir1, PA4 - Step1 (Tim14_Ch1), PA3 - ~Enable
// PA5 - SCK, PA6 - RS, PA7 - MOSI, PB1 - CS
// PA10 - PWM1 / DIR2  (Depended wich using for Cut (servo = PWM1, Second step motor = Dir2))
// Not Using - PA0

#define DRV_PIN 0x8 // PA3
#define DRV_ON GPIOA->BRR |= DRV_PIN
#define DRV_OFF GPIOA->BSRR |= DRV_PIN

#define DIR_PIN 0x2 // PA1
#define DIR_FWD GPIOA->BRR |= DIR_PIN
#define DIR_BWD GPIOA->BSRR |= DIR_PIN

#define KEY_UP 0x01	   // PF0
#define KEY_DOWN 0x02  // PF1
#define KEY_RIGHT 0x04 // PA2
#define KEY_LEFT 0x200 // PA9
#define READ_UP() (GPIOF->IDR & KEY_UP)
#define READ_DOWN() (GPIOF->IDR & KEY_DOWN)
#define READ_RIGHT() (GPIOA->IDR & KEY_RIGHT)
#define READ_LEFT() (GPIOA->IDR & KEY_LEFT)

uint16_t press_time = 0, idle_time = 0;
uint8_t pos = 5, lcd_flag = 0, tim_flag = 0;
//---------------Timer settings-------------
uint16_t TimingDelay, lcd_time, sec_count;
uint8_t lcd_time_flag = 0;

uint16_t wire_pre;	// strip first side, mm
uint16_t wire;		// wire lenght
uint16_t wire_post; // strip second side, mm
uint8_t wire_strip; // level for PWM (stripping)
uint16_t wire_qnt;	// Quantity of wires

volatile uint32_t front_counter = 0, dest_count = 0;
typedef enum
{
	ST_OFF = 0,
	ST_PRESS,
	ST_RELEASE
} stat_enum;

stat_enum kbd_up;
stat_enum kbd_right;
stat_enum kbd_down;
stat_enum kbd_left;

bool time_update_flag;
void SysTick_Handler(void)
{
	if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)
	{
		if (TimingDelay != 0x00)
			TimingDelay--;
		if (!lcd_time)
		{
			lcd_time = SCREEN_UPDATE;
			lcd_time_flag = 1;
		}
		if (kbd_up == ST_PRESS || kbd_right == ST_PRESS || kbd_down == ST_PRESS || kbd_left == ST_PRESS)
		{
			if (press_time < 10000)
				press_time++;
		}
		if (kbd_up == ST_OFF && kbd_right == ST_OFF && kbd_down == ST_OFF && kbd_left == ST_OFF)
		{
			if (idle_time < 2000)
				idle_time++;
		}

		if (!sec_count)
		{
			sec_count = 1000;
			time_update_flag = 1;
		}
		lcd_time--;
		sec_count--;
		SysTick->CTRL &= (~SysTick_CTRL_COUNTFLAG_Msk);
	}
}

void EXTI0_1_IRQHandler(void)
{
	if (EXTI->PR & EXTI_PR_PIF0)
	{
		if (!READ_UP()) // press Up
		{
			kbd_up = ST_PRESS;
		}
		EXTI->PR |= EXTI_PR_PIF0;
	}
	if (EXTI->PR & EXTI_PR_PIF1)
	{
		if (!READ_DOWN()) // press Down PF0
		{
			kbd_down = ST_PRESS;
		}
		EXTI->PR |= EXTI_PR_PIF1;
	}
	idle_time = 0;
}
void EXTI2_3_IRQHandler(void)
{
	if (EXTI->PR & EXTI_PR_PIF2)
	{
		if (!READ_RIGHT()) // press Right
		{
			kbd_right = ST_PRESS;
		}
		EXTI->PR |= EXTI_PR_PIF2;
	}
}
void EXTI4_15_IRQHandler(void)
{
	if (EXTI->PR & EXTI_PR_PIF9)
	{
		if (!READ_LEFT()) // press Left PA0
		{
			kbd_left = ST_PRESS;
		}
		EXTI->PR |= EXTI_PR_PIF9;
	}
	idle_time = 0;
}
void TIM14_IRQHandler(void)
{
	if (TIM14->SR & TIM_SR_UIF)
	{
		front_counter++;
		TIM14->SR &= (~TIM_SR_UIF);
		if (front_counter >= dest_count)
		{
			TIM14->CR1 &= (~TIM_CR1_CEN);
			tim_flag = 1;
			front_counter = 0;
			dest_count = 0;
			TIM14->CNT = 0;
		}
	}
}

void delay_ms(uint16_t DelTime)
{
	TimingDelay = DelTime;
	while (TimingDelay != 0x00)
		;
}

void go_cut(void) // 1 rotation = 32mm = 200steps
{
	uint16_t qq;

	fillRect(1, 1, 99, 78, RED);
	LCD_Print("Amount", 5, 20, WHITE, RED, 1, 1, 0);
	LCD_Print("Ready", 5, 60, WHITE, RED, 1, 1, 0);
	if (wire == 0 || wire_qnt == 0)
		return;
	DRV_ON;
	delay_ms(1);

	for (qq = wire_qnt; qq > 0; qq--)
	{
		TIM1->CCR3 = KNIFE_OPEN;
		delay_ms(KNIFE_TIME);

		LCD_PrintDec(wire_qnt, 50, 15, WHITE, RED, 2, 2, 0);
		if (wire_qnt < 10)
			LCD_Print("  ", 62, 15, WHITE, RED, 2, 2, 0);
		if (wire_qnt < 100 && wire_qnt > 9)
			LCD_Print(" ", 74, 15, WHITE, RED, 2, 2, 0);

		LCD_PrintDec((wire_qnt - qq), 50, 55, WHITE, RED, 2, 2, 0);
		if ((wire_qnt - qq) < 10)
			LCD_Print("  ", 62, 55, WHITE, RED, 2, 2, 0);
		if ((wire_qnt - qq) < 100 && (wire_qnt - qq) > 9)
			LCD_Print(" ", 74, 55, WHITE, RED, 2, 2, 0);

		DIR_FWD;
		delay_ms(1);

		if (wire_pre > 0)
		{
			dest_count = (uint32_t)((float)wire_pre * (float)6.25);
			tim_flag = 0;
			TIM14->CR1 |= TIM_CR1_CEN;
			while (tim_flag == 0)
				;
			TIM1->CCR3 = wire_strip;
			delay_ms(KNIFE_TIME);
			TIM1->CCR3 = KNIFE_OPEN;
			delay_ms(KNIFE_TIME);
		}
		delay_ms(10);
		if (wire > 0)
		{
			dest_count = (uint32_t)((float)wire * (float)6.25);
			tim_flag = 0;
			TIM14->CR1 |= TIM_CR1_CEN;
			while (tim_flag == 0)
				;
		}
		delay_ms(10);
		if (wire_post > 0)
		{
			TIM1->CCR3 = wire_strip;
			delay_ms(KNIFE_TIME);
			TIM1->CCR3 = KNIFE_OPEN;
			delay_ms(KNIFE_TIME);

			dest_count = (uint32_t)((float)wire_post * (float)6.25);
			tim_flag = 0;
			TIM14->CR1 |= TIM_CR1_CEN;
			while (tim_flag == 0)
				;
		}
		TIM1->CCR3 = KNIFE_CLOSE;
		delay_ms(KNIFE_TIME);
		TIM1->CCR3 = KNIFE_OPEN;
		delay_ms(KNIFE_TIME);
		//-------------Go backwar------
		DIR_BWD;
		delay_ms(10);

		dest_count = (uint32_t)((float)BACK_CONST * (float)6.25);
		tim_flag = 0;
		TIM14->CR1 |= TIM_CR1_CEN;
		while (tim_flag == 0)
			;

		//-------------Go forward------
		DIR_FWD;
		delay_ms(100);
		dest_count = (uint32_t)((float)BACK_CONST * (float)6.25);
		tim_flag = 0;
		TIM14->CR1 |= TIM_CR1_CEN;
		while (tim_flag == 0)
			;
		delay_ms(20);
	}
	DRV_OFF;

	fillRect(1, 1, 99, 78, GREEN);
	LCD_Print("Done!", 3, 28, RED, GREEN, 3, 3, 0);
	LCD_Print("Robots work hard,", 1, 60, RED, GREEN, 1, 1, 0);
	LCD_Print("people are happy", 1, 68, RED, GREEN, 1, 1, 0);

	delay_ms(1000);
	fillRect(1, 1, 99, 78, BLACK);
}
void initial(void)
{								//--------------SysTick------------------
	SysTick->LOAD = (8000 - 1); // HSI 8 MHz - 1 msek
	SysTick->VAL = (8000 - 1);	// HSI 8 MHz - 1 msek
	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
	NVIC_EnableIRQ(SysTick_IRQn);
	__enable_irq();

	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
	RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
	RCC->AHBENR |= RCC_AHBENR_GPIOFEN;
	//---------PA1 - Dir1, PA3 - ~Enable (DRV)---------------
	GPIOA->MODER |= GPIO_MODER_MODER1_0 | GPIO_MODER_MODER3_0;
	DRV_OFF;
	DIR_FWD;
	//--------------TIM14_Ch1------------------------
	GPIOA->MODER |= GPIO_MODER_MODER4_1; // PA4 - Step (TIM14_Ch1)
	GPIOA->AFR[0] |= (4 << (4 * 4));

	RCC->APB1ENR |= RCC_APB1ENR_TIM14EN;
	TIM14->PSC = 2500 - 1;
	TIM14->ARR = 2;
	TIM14->CCR1 = 1;
	TIM14->CCMR1 |= TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_0; // PWM
	TIM14->CCER |= TIM_CCER_CC1E;
	TIM14->DIER |= TIM_DIER_UIE;
	NVIC_SetPriority(TIM14_IRQn, 4);
	NVIC_EnableIRQ(TIM14_IRQn);

	//----------------TIM1_CH3 Servo PWM----------------------
	GPIOA->MODER |= GPIO_MODER_MODER10_1; // PA10
	GPIOA->AFR[1] |= (2 << (2 * 4));	  // TIM1_CH3

	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
	TIM1->PSC = 400 - 1; // TIM1->PSC = 400 - 1;
	TIM1->ARR = 100;
	TIM1->CCR3 = KNIFE_OPEN;
	TIM1->CCMR2 |= TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1; // PWM
	TIM1->CCER |= TIM_CCER_CC3E;
	TIM1->BDTR |= TIM_BDTR_MOE;
	TIM1->CR1 |= TIM_CR1_CEN;
	TIM1->EGR |= TIM_EGR_UG;

	//------------------------SPI-----------------------------------
	GPIOA->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR5 | GPIO_OSPEEDER_OSPEEDR7 | GPIO_OSPEEDER_OSPEEDR6;
	GPIOA->MODER |= GPIO_MODER_MODER5_1 | GPIO_MODER_MODER7_1 | GPIO_MODER_MODER6_0; // Pa5..Pa7 - Alt_mode, Pa6 - RS

	GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR1;
	GPIOB->MODER |= GPIO_MODER_MODER1_0; // CS Pin

	GPIOA->AFR[0] |= (0 << (7 * 4)) | (0 << (5 * 4)); // SPI - Alternative

	RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
	SPI1->CR1 |= SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_MSTR; //| (0 << SPI_CR1_BR_Pos);
	SPI1->CR2 |= SPI_CR2_FRXTH;
	SPI1->CR1 |= SPI_CR1_SPE;

	//-------Keyboard-----------
	GPIOA->MODER &= (~GPIO_MODER_MODER2) & (~GPIO_MODER_MODER9);
	GPIOA->PUPDR |= GPIO_PUPDR_PUPDR2_0 | GPIO_PUPDR_PUPDR9_0; // Pull Up PA2,PA9
	GPIOF->MODER &= (~GPIO_MODER_MODER0) & (~GPIO_MODER_MODER1);
	GPIOF->PUPDR |= GPIO_PUPDR_PUPDR0_0 | GPIO_PUPDR_PUPDR1_0; // Pull Up PF0,PF1

	//----------------------EXTI-----------------------------------
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
	SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI2_PA;
	SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI9_PA;
	SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PF | SYSCFG_EXTICR1_EXTI1_PF;

	EXTI->FTSR |= EXTI_FTSR_TR2 | EXTI_FTSR_TR9 | EXTI_FTSR_TR0 | EXTI_FTSR_TR1; // Falling
	NVIC_SetPriority(EXTI0_1_IRQn, 5);
	NVIC_EnableIRQ(EXTI0_1_IRQn);
	NVIC_SetPriority(EXTI2_3_IRQn, 5);
	NVIC_EnableIRQ(EXTI2_3_IRQn);
	NVIC_SetPriority(EXTI4_15_IRQn, 5);
	NVIC_EnableIRQ(EXTI4_15_IRQn);
}
void read_config(void)
{
	wire_pre = *(__IO uint16_t *)(FLASH_PAGE16);
	wire = *(__IO uint16_t *)(FLASH_PAGE16 + 2);
	wire_post = *(__IO uint16_t *)(FLASH_PAGE16 + 4);
	wire_strip = (*(__IO uint16_t *)(FLASH_PAGE16 + 6)) & 0xff;
	wire_qnt = *(__IO uint16_t *)(FLASH_PAGE16 + 8);

	if (wire == 0 || wire > 999)
	{
		wire_pre = 5;
		wire = 185;
		wire_post = 10;
		wire_qnt = 2;
		wire_strip = 24;
	}
}
void save_config(void)
{
	//----------Flash Unlock----------------
	while ((FLASH->SR & FLASH_SR_BSY) != 0)
	{
		/* For robust implementation, add here time-out management */
	}
	if ((FLASH->CR & FLASH_CR_LOCK) != 0)
	{
		FLASH->KEYR = FLASH_FKEY1;
		FLASH->KEYR = FLASH_FKEY2;
	}
	//-----------Erase Page-----------------
	FLASH->CR |= FLASH_CR_PER;
	FLASH->AR = FLASH_PAGE16; // Page 16
	FLASH->CR |= FLASH_CR_STRT;
	while ((FLASH->SR & FLASH_SR_BSY) != 0)
	{
		/* For robust implementation, add here time-out management */
	}
	if ((FLASH->SR & FLASH_SR_EOP) != 0)
	{
		FLASH->SR |= FLASH_SR_EOP;
	}
	else
	{
		fillRect(1, 1, 99, 78, RED);
		LCD_Print("Error erase", 5, 28, WHITE, RED, 1, 1, 0);
		delay_ms(500);
	}
	FLASH->CR &= ~FLASH_CR_PER;
	//---------Write data--------------------
	FLASH->CR |= FLASH_CR_PG;
	*(__IO uint16_t *)(FLASH_PAGE16) = wire_pre;
	while ((FLASH->SR & FLASH_SR_BSY) != 0)
		;
	if ((FLASH->SR & FLASH_SR_EOP) != 0)
	{
		FLASH->SR |= FLASH_SR_EOP;
	}

	*(__IO uint16_t *)(FLASH_PAGE16 + 2) = wire;
	while ((FLASH->SR & FLASH_SR_BSY) != 0)
		;
	if ((FLASH->SR & FLASH_SR_EOP) != 0)
	{
		FLASH->SR |= FLASH_SR_EOP;
	}

	*(__IO uint16_t *)(FLASH_PAGE16 + 4) = wire_post;
	while ((FLASH->SR & FLASH_SR_BSY) != 0)
		;
	if ((FLASH->SR & FLASH_SR_EOP) != 0)
	{
		FLASH->SR |= FLASH_SR_EOP;
	}
	*(__IO uint16_t *)(FLASH_PAGE16 + 6) = wire_strip;
	while ((FLASH->SR & FLASH_SR_BSY) != 0)
		;
	if ((FLASH->SR & FLASH_SR_EOP) != 0)
	{
		FLASH->SR |= FLASH_SR_EOP;
	}
	*(__IO uint16_t *)(FLASH_PAGE16 + 8) = wire_qnt;
	while ((FLASH->SR & FLASH_SR_BSY) != 0)
		;
	if ((FLASH->SR & FLASH_SR_EOP) != 0)
	{
		FLASH->SR |= FLASH_SR_EOP;
	}
	FLASH->CR &= ~FLASH_CR_PG;
	FLASH->CR |= FLASH_CR_LOCK;
	fillRect(1, 1, 99, 78, GREEN);
	LCD_Print("Save", 4, 20, BLACK, GREEN, 2, 2, 0);
	LCD_Print("config", 15, 40, BLACK, GREEN, 2, 2, 0);
	delay_ms(1500);
	fillRect(1, 1, 99, 78, BLACK);
	pos = 5;
}

void kbd_scan(void) // keyboard_scan
{
	if (kbd_left == ST_PRESS && press_time > FILTR_TIME) // Left
	{
		lcd_flag = 1;
		press_time = 0;
		if (pos > 0)
			pos--;
		kbd_left = ST_OFF;
	}
	if (kbd_right == ST_PRESS && press_time > FILTR_TIME) // Right
	{
		lcd_flag = 1;
		press_time = 0;
		if (pos < 6)
			pos++;
		kbd_right = ST_OFF;
	}
	if (kbd_down == ST_PRESS && press_time > FILTR_TIME) // Down
	{
		lcd_flag = 1;
		press_time = 0;
		switch (pos)
		{
		case 0:
		{
			if (wire_pre > 0)
				wire_pre--;
		}
		break;
		case 1:
		{
			if (wire > 0)
				wire--;
		}
		break;
		case 2:
		{
			if (wire_post > 0)
				wire_post--;
		}
		break;
		case 3:
		{
			if (wire_strip < KNIFE_CLOSE)
				wire_strip++;
		}
		break;
		case 4:
		{
			if (wire_qnt > 0)
				wire_qnt--;
		}
		break;
		case 5:
		{
			go_cut();
		}
		break;
		case 6:
		{
			save_config();
		}
		break;
		default:
			break;
		}
	}
	if (kbd_up == ST_PRESS && press_time > FILTR_TIME) // Up
	{
		lcd_flag = 1;
		press_time = 0;
		switch (pos)
		{
		case 0:
		{
			if (wire_pre < 999)
				wire_pre++;
		}
		break;
		case 1:
		{
			if (wire < 999)
				wire++;
		}
		break;
		case 2:
		{
			if (wire_post < 999)
				wire_post++;
		}
		break;
		case 3:
		{
			if (wire_strip > KNIFE_OPEN)
				wire_strip--;
		}
		break;
		case 4:
		{
			if (wire_qnt < 999)
				wire_qnt++;
		}
		break;
		case 5:
		{
			TIM1->CCR3 = KNIFE_OPEN;
		}
		break;
		case 6:
		{
			save_config();
		}
		break;

		default:
			break;
		}
	}

	if (pos == 3)
	{
		TIM1->CCR3 = wire_strip;
	}
	else
	{
		TIM1->CCR3 = KNIFE_OPEN;
	}

	if (READ_RIGHT())
		kbd_right = ST_OFF;
	if (READ_LEFT())
		kbd_left = ST_OFF;
	if (READ_UP())
		kbd_up = ST_OFF;
	if (READ_DOWN())
		kbd_down = ST_OFF;
}
void LCD_Update(void)
{
	// Draw wire
	drawLine(1, 8, 99, 8, WHITE);
	fillRect(25, 3, 50, 10, WHITE);
	// Draw pos

	drawRect(3, 15, 24, 20, (pos == 0) ? RED : WHITE);
	drawRect(38, 15, 24, 20, (pos == 1) ? RED : WHITE);
	drawRect(74, 15, 24, 20, (pos == 2) ? RED : WHITE);

	drawRect(3, 41, 34, 23, (pos == 3) ? RED : WHITE);
	drawRect(40, 41, 24, 23, (pos == 4) ? RED : WHITE);
	drawRect(70, 41, 27, 23, (pos == 5) ? RED : WHITE);
	drawRect(16, 66, 70, 12, (pos == 6) ? RED : WHITE);

	LCD_PrintDec(wire_pre, 5, 17, WHITE, BLACK, 1, 1, 0);
	if (wire_pre < 10)
		LCD_Print("  ", 11, 17, PINK, BLACK, 1, 1, 0);
	if (wire_pre < 100 && wire_pre > 9)
		LCD_Print(" ", 17, 17, PINK, BLACK, 1, 1, 0);

	LCD_PrintDec(wire, 40, 17, WHITE, BLACK, 1, 1, 0);
	if (wire < 10)
		LCD_Print("  ", 46, 17, PINK, BLACK, 1, 1, 0);
	if (wire < 100 && wire > 9)
		LCD_Print(" ", 52, 17, PINK, BLACK, 1, 1, 0);

	LCD_PrintDec(wire_post, 76, 17, WHITE, BLACK, 1, 1, 0);
	if (wire_post < 10)
		LCD_Print("  ", 82, 17, PINK, BLACK, 1, 1, 0);
	if (wire_post < 100 && wire_post > 9)
		LCD_Print(" ", 88, 17, PINK, BLACK, 1, 1, 0);

	LCD_Print("mm", 7, 25, PINK, BLACK, 1, 1, 0);
	LCD_Print("mm", 42, 25, PINK, BLACK, 1, 1, 0);
	LCD_Print("mm", 80, 25, PINK, BLACK, 1, 1, 0);

	LCD_PrintDec(wire_strip, 7, 43, WHITE, BLACK, 1, 1, 0);
	if (wire_strip < 10)
		LCD_Print("  ", 13, 43, PINK, BLACK, 1, 1, 0);
	if (wire_strip < 100 && wire_strip > 9)
		LCD_Print(" ", 20, 43, PINK, BLACK, 1, 1, 0);

	LCD_PrintDec(wire_qnt, 44, 43, WHITE, BLACK, 1, 1, 0);
	if (wire_qnt < 10)
		LCD_Print("  ", 50, 43, PINK, BLACK, 1, 1, 0);
	if (wire_qnt < 100 && wire_qnt > 9)
		LCD_Print(" ", 56, 43, PINK, BLACK, 1, 1, 0);

	LCD_Print("Open", 72, 43, PINK, BLACK, 1, 1, 0);
	LCD_Print("Run", 72, 53, GREEN, BLACK, 1, 1, 0);

	LCD_Print("Knife", 5, 53, PINK, BLACK, 1, 1, 0);
	LCD_Print("Qnt", 42, 53, PINK, BLACK, 1, 1, 0);

	LCD_Print("Save config", 18, 68, PINK, BLACK, 1, 1, 0);
}
int main(void)
{ // main
	//------------------Initial parameters-----------------------------------
	initial();
	delay_ms(100);
	LCD_Init();
	fillRect(0, 0, 101, 80, BLACK);
	EXTI->IMR |= EXTI_IMR_IM0 | EXTI_IMR_IM1 | EXTI_IMR_IM2 | EXTI_IMR_IM9;

	setLcdOn(ON);
	read_config();
	//-----------LoGo--------------------
	fillRect(0, 0, 101, 80, BLUE);
	LCD_Print("DaWinch", 10, 10, WHITE, BLUE, 2, 2, 0);
	LCD_Print("Wire", 10, 30, WHITE, BLUE, 2, 2, 0);
	LCD_Print("Cutter", 20, 50, WHITE, BLUE, 2, 2, 0);

	delay_ms(1500);

	fillRect(1, 1, 99, 78, BLACK);
	drawRect(0, 0, 101, 80, GREEN);
	//------------------Main Loop--------------------------------------------
	while (1)
	{ // Main loop

		kbd_scan();
		if (lcd_time_flag || lcd_flag)
		{
			lcd_time_flag = 0;
			lcd_flag = 0;
			LCD_Update();
		}

	} // end - main loop
} // end - Main

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
	while (1)
	{
	}
}
#endif
