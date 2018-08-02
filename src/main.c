//
// This file is part of the GNU ARM Eclipse distribution.
// Copyright (c) 2014 Liviu Ionescu.
//

// ----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "diag/Trace.h"
#include "fatfs.h"
#include "usb_host.h"
#include "stm32f4xx_hal.h"
#include "Timer.h"
#include "BlinkLed.h"
#include "PlayMP3.h"
#include "cortexm/ExceptionHandlers.h"
#include "generic.h"
#include "timeKeeping.h"
#include "DebugPort.h"
#include "AudioChip.h"

//
// Disable specific warnings
//

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"



// ----------------------------------------------------------------------------
//
// Standalone STM32F4 Simple Alarm Clock Stub Code
//
// This code just plays an MP3 file off of a connected USB flash drive.
//
// Trace support is enabled by adding the TRACE macro definition.
// By default the trace messages are forwarded to the DEBUG output,
// but can be rerouted to any device or completely suppressed, by
// changing the definitions required in system/src/diag/trace_impl.c
// (currently OS_USE_TRACE_ITM, OS_USE_TRACE_SEMIHOSTING_DEBUG/_STDOUT).
//

static TIM_HandleTypeDef debounce_timer = {
		.Instance = TIM3
};

static TIM_HandleTypeDef alarm_timer = {
		.Instance = TIM4
};

static TIM_HandleTypeDef display_timer = {
		.Instance = TIM5
};

// Alarm clock states
#define INIT 	 	0
#define TIME		1
#define ALARM 	 	2
#define MODE	 	3

// Set time modes
#define SET_TIME    0
#define SET_ALARM   1
#define SET_24HR    2
#define TELL_TIME   3

#define SET_ALARM_TITLE   4
#define SET_TIME_TITLE    5

#define ALPHA	11
#define CHARLIE	12
#define KILO	13
#define LIMA	14
#define PAPA	15


// Global and state variable
volatile int state = INIT;
volatile int mode = TELL_TIME;
volatile int led1 = 0;
volatile int led2 = 0;
volatile int led3 = 0;
volatile int pm = 0;
volatile int alarm_set = 0;
volatile int read_buttons = 1;
int b1 = 0;
int b2 = 0;
int b3 = 0;
int b4 = 0;
int b5 = 0;

void
	update_display( int digit ),
//	Display7Segment( void ),
//	DisplayClockMode( void ),
//	ConvertTo24Hour( void ),
//	SetTime( void ),
//	SetAlarm( void ),
//	Snooze( void ),
//	ProcessButtons( void ),
	GetCurrentTime( int time_type ),
	RealTimeClockInit(void),
	ConfigureAudioDma( void ),
	SystemClock_Config( void ),
	MX_GPIO_Init( void ),
	MX_I2C1_Init( void ),
	MX_USB_HOST_Process( void );

//uint16_t
//	CheckButtons( void );


// STMCube Example declarations.
// static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);

static void
	MSC_Application(void);

//static void
	//Error_Handler(void);

//
// Global variables
//

//RTC_InitTypeDef
//	ClockInit;				// Structure used to initialize the real time clock
//
RTC_TimeTypeDef
	ClockTime;				// Structure to hold/store the current time

RTC_DateTypeDef
	ClockDate;				// Structure to hold the current date

RTC_AlarmTypeDef
	ClockAlarm;				// Structure to hold/store the current alarm time

TIM_HandleTypeDef
	Timer6_44Khz,			// Structure for the audio play back timer subsystem
	DisplayTimer;			// Structure for the LED display timer subsystem

DAC_HandleTypeDef
	AudioDac;				// Structure for the audio digital to analog converter subsystem

DMA_HandleTypeDef
	AudioDma;				// Structure for the audio DMA direct memory access controller subsystem

RTC_HandleTypeDef
	RealTimeClock;			// Structure for the real time clock subsystem

I2C_HandleTypeDef			// Structure for I2C subsystem. Used for external audio chip
	I2c;

volatile int
	DisplayClockModeCount,	// Number of display ticks to show the current clock mode time format
	PlayMusic = FALSE,		// Flag indicating if music should be played
	DebounceCount = 0;		// Buttons debounce count

volatile uint16_t
	ButtonsPushed;			// Bit field containing the bits of which buttons have been pushed

FATFS
	UsbDiskFatFs;			// File system object for USB disk logical drive

char
	UsbDiskPath[4];			// USB Host logical drive path

int
	BcdTime[4],				// Array to hold the hours and minutes in BCD format
	DisplayedDigit = 0,		// Current digit being displayed on the LED display

							// Current format for the displayed time ( IE 12 or 24 hour format )
	ClockHourFormat = CLOCK_HOUR_FORMAT_12,
	AlarmPmFlag = 0,
	TimePmFlag = 0;


//
// Functions required for long files names on fat32 partitions
//

WCHAR ff_convert (WCHAR wch, UINT dir)
{
	if (wch < 0x80)
	{
//
// ASCII Char
//
		return wch;
	}

//
// unicode not supported
//
	return 0;
}

WCHAR ff_wtoupper (WCHAR wch)
{
	if (wch < 0x80)
	{
//
// ASCII Char
//
		if (wch >= 'a' && wch <= 'z')
		{
			wch &= ~0x20;
		}

		return wch;
	}

//
// unicode not supported
//
	return 0;
}


//interrupt for snooze timer
void TIM4_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&alarm_timer);
}

//interrupt for updating clock display
void TIM5_IRQHandler(void)
{
 	static int digit = 0;
	digit = (digit+1)%7;
    update_display(digit);
	HAL_TIM_IRQHandler(&display_timer);
}

// Dummy interrupt handler function
void TIM6_DAC_IRQHandler(void)
{
	HAL_NVIC_DisableIRQ( TIM6_DAC_IRQn );
}

// General EXTI (button) handler for debouncing
int debounce(uint16_t GPIO_Pin) {
	int button_pressed = 0;

	if(GPIO_Pin == ALARM_BUTTON)
		HAL_NVIC_DisableIRQ( EXTI4_IRQn );
	else if(GPIO_Pin == MODE_BUTTON || GPIO_Pin == TIME_BUTTON || GPIO_Pin == SELECT_BUTTON )
		HAL_NVIC_DisableIRQ( EXTI9_5_IRQn );
	else if(GPIO_Pin == SNOOZE_BUTTON)
		HAL_NVIC_DisableIRQ( EXTI15_10_IRQn );

	read_buttons = 0;

	HAL_TIM_Base_Start(&debounce_timer);

	while(1)
	{
		if (__HAL_TIM_GET_FLAG(&debounce_timer, TIM_FLAG_UPDATE) != RESET)
		{
			 button_pressed = !HAL_GPIO_ReadPin(GPIOC, GPIO_Pin); //check if debounced button is pressed down
		 	 __HAL_TIM_CLEAR_IT(&debounce_timer, TIM_IT_UPDATE);// clear timer period flag
		 	 HAL_TIM_Base_Stop(&debounce_timer);
		 	 break;
		  }
	}

	if(GPIO_Pin == ALARM_BUTTON)
		HAL_NVIC_EnableIRQ( EXTI4_IRQn );
	else if(GPIO_Pin == MODE_BUTTON || GPIO_Pin == TIME_BUTTON || GPIO_Pin == SELECT_BUTTON )
		HAL_NVIC_EnableIRQ( EXTI9_5_IRQn );
	else if(GPIO_Pin == SNOOZE_BUTTON)
		HAL_NVIC_EnableIRQ( EXTI15_10_IRQn );

	read_buttons = 1;
	return button_pressed;

}//HAL_GPIO_EXTI_IRQHandler

//interrupt for button 1 (alarm on/off)
void EXTI4_IRQHandler(void)
{
	// Alarm button pressed
	if(__HAL_GPIO_EXTI_GET_FLAG(ALARM_BUTTON) && read_buttons && !b1)
	{
		b1 = debounce(ALARM_BUTTON); // is this a bounce or a press?
		if(b1) {
			if(state == ALARM) {
				state = TIME;
				PlayMusic = FALSE;
				alarm_set = 0;
				__HAL_RTC_ALARM_DISABLE_IT(&RealTimeClock, RTC_IT_ALRA);
			}
			else {
				alarm_set = ~alarm_set;
				if(alarm_set) __HAL_RTC_ALARM_ENABLE_IT(&RealTimeClock, RTC_IT_ALRA);
			}
		}
	}
	HAL_GPIO_EXTI_IRQHandler(ALARM_BUTTON);

}//EXTI 4 handler

//interrupt for buttons 2 (mode), 3 (time increment), and 4 (select)
void EXTI9_5_IRQHandler(void)
{
	// Button 2 (mode) pressed
	if(__HAL_GPIO_EXTI_GET_FLAG(MODE_BUTTON))
	{
		b2 = debounce(MODE_BUTTON);
		if(b2) {
			state = MODE;
			mode = (mode+1)%4;
		}
		HAL_GPIO_EXTI_IRQHandler(MODE_BUTTON);
	}

	// Button 3 (increment) pressed
	if(__HAL_GPIO_EXTI_GET_FLAG(TIME_BUTTON))
	{
		b3 = debounce(TIME_BUTTON);
		if(state != MODE) b3 = 0;
		HAL_GPIO_EXTI_IRQHandler(TIME_BUTTON);
	}

	// Button 4 (select) pressed
	if(__HAL_GPIO_EXTI_GET_FLAG(SELECT_BUTTON))
	{
		b4 = debounce(SELECT_BUTTON);
		if(state != MODE) b4 = 0;
		HAL_GPIO_EXTI_IRQHandler(SELECT_BUTTON);
	}


}//EXTI Lines 5-9 handler

//interrupt for button 5 (snooze)
void EXTI15_10_IRQHandler(void)
{
	if(__HAL_GPIO_EXTI_GET_FLAG(SNOOZE_BUTTON) && read_buttons && !b5 && (state == ALARM))
	{
		b5 = debounce(SNOOZE_BUTTON);
		if(b5 && (state == ALARM)) {
	    	__HAL_RTC_ALARM_CLEAR_FLAG( &RealTimeClock, RTC_FLAG_ALRAF );
	    	__HAL_RTC_EXTI_CLEAR_FLAG( RTC_EXTI_LINE_ALARM_EVENT );
	    	if (ClockAlarm.AlarmTime.Minutes < 50) ClockAlarm.AlarmTime.Minutes += 10;
	    	else {
	    		ClockAlarm.AlarmTime.Minutes += 10;
	    		alarmTimeCheck();
	    		ClockAlarm.AlarmTime.Hours += 1;
	    		alarmHourCheck();
	    	}
	    	HAL_RTC_SetAlarm_IT( &RealTimeClock, &ClockAlarm, RTC_FORMAT_BIN );
	    	state = TIME;
		}
		else b5 = 0;
	}
	HAL_GPIO_EXTI_IRQHandler(MODE_BUTTON);
}//EXTI 11 handler

/*
 * Function: RTC_Alarm_IRQHandler
 *
 * Description:
 *
 * When alarm occurs, clear all the interrupt bits and flags then start playing music.
 *
 */

void RTC_Alarm_IRQHandler(void)
{

//
// Verify that this is a real time clock interrupt
//
	if( __HAL_RTC_ALARM_GET_IT( &RealTimeClock, RTC_IT_ALRA ) != RESET )
	{

//
// Clear the alarm flag and the external interrupt flag
//
    	__HAL_RTC_ALARM_CLEAR_FLAG( &RealTimeClock, RTC_FLAG_ALRAF );
    	__HAL_RTC_EXTI_CLEAR_FLAG( RTC_EXTI_LINE_ALARM_EVENT );

//
// Restore the alarm to it's original time. This could have been a snooze alarm
//
    	HAL_RTC_SetAlarm_IT( &RealTimeClock, &ClockAlarm, RTC_FORMAT_BIN );

    	state = ALARM;
    	PlayMusic = TRUE;

	}


}//RTC_Alarm_IRQHandler

void init_GPIO() {

	// PORT C: Buttons
	__GPIOC_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStructure4; // handle for pointing GPIO
	GPIO_InitStructure4.Pin = GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_11;
	GPIO_InitStructure4.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStructure4.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure4.Pull = GPIO_PULLUP;
	GPIO_InitStructure4.Alternate = 0;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStructure4);

	 // PORT D: Digits
	__GPIOD_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStructure5; //a handle to initialize GPIO port D
	GPIO_InitStructure5.Pin = GPIO_PIN_12 | GPIO_PIN_10 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_6 | GPIO_PIN_4 | GPIO_PIN_2;
	GPIO_InitStructure5.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure5.Speed = GPIO_SPEED_LOW;
	GPIO_InitStructure5.Pull = GPIO_NOPULL;
	GPIO_InitStructure5.Alternate = 0;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStructure5); // Initialize GPIO port D with above parameters

	// PORT E: LEDs
	__GPIOE_CLK_ENABLE(); // enable clock for GPIO port E
	GPIO_InitTypeDef GPIO_InitStructure3; //a handle to initialize GPIO port E
	GPIO_InitStructure3.Pin = GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_6 | GPIO_PIN_7 ;
	GPIO_InitStructure3.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure3.Speed = GPIO_SPEED_LOW;
	GPIO_InitStructure3.Pull = GPIO_NOPULL;
	GPIO_InitStructure3.Alternate = 0;
	 HAL_GPIO_Init(GPIOD, &GPIO_InitStructure3);  // Initialize GPIO port E with above parameters

}//init_GPIO

void init_timers() {

	// TIMER 3: DEBOUNCE
	__TIM3_CLK_ENABLE();// enable clock for Timer 3
	TIM_HandleTypeDef debounce_timer; // define a handle to initialize timer
	debounce_timer.Instance = TIM3; // Point to Timer 3
	debounce_timer.Init.Prescaler = 843; // Timer clock maximum frequency is 84 MHz.
	debounce_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
	debounce_timer.Init.Period = 200; // To count until 20ms.
	debounce_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	debounce_timer.Init.RepetitionCounter = 0;
	HAL_TIM_Base_Init(&debounce_timer );// Initialize timer with above parameters
	//HAL_TIM_Base_Start(&debounce_timer); // start timer

	// TIMER 5: DISPLAY
	__HAL_RCC_TIM5_CLK_ENABLE();// enable clock for Timer 5
	TIM_HandleTypeDef display_timer; // define a handle to initialize timer
	display_timer.Instance = TIM5; // Point to Timer 5
	display_timer.Init.Prescaler =  843; // Timer clock maximum frequency is 84 MHz.
	display_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
	display_timer.Init.Period = 250; // To count until 2.5 ms
	display_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	display_timer.Init.RepetitionCounter = 0;
	HAL_TIM_Base_Init( &display_timer );// Initialize timer with above parameters
	HAL_TIM_Base_Start_IT(&display_timer); // start timer

	// TIMER 4: SNOOZE?
	__HAL_RCC_TIM4_CLK_ENABLE();
	TIM_HandleTypeDef alarm_timer; // define a handle to initialize timer
	alarm_timer.Instance = TIM4 ; // Point to Timer 4
	alarm_timer.Init.Prescaler = 8399;
	// This prescaler will give 10 kHz timing_tick_frequency
	alarm_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
	alarm_timer.Init.Period = 4999; // To count until 10ms seconds.
	alarm_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	alarm_timer.Init.RepetitionCounter = 0;
	HAL_TIM_Base_Init( &alarm_timer );// Initialize timer with above parameters
	//HAL_TIM_Base_Start_IT(&alarm_timer); // start timer

}//init_timers


void init_interrupts() {
	//configure interrupt for updating display on TIM5
	HAL_NVIC_SetPriority(TIM5_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(TIM5_IRQn);

	//configure interrupt for debounce delay on TIM3
	//HAL_NVIC_SetPriority(TIM3_IRQn, 1, 0);
	//HAL_NVIC_EnableIRQ(TIM3_IRQn);

	// configure interrupt for button 1 (mode) on PC4
	HAL_NVIC_SetPriority(EXTI4_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(EXTI4_IRQn);

	//configure interrupt for button 2 (alarm on/off) on PC5
	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 3, 0);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

	//configure interrupt for alarm time on TIM4
	HAL_NVIC_SetPriority(TIM4_IRQn, 4, 0);
	HAL_NVIC_EnableIRQ(TIM4_IRQn);

}// init_interrupts

void GetCurrentTime(int time_type)
{
	if(time_type == TELL_TIME) {
		// current time
		HAL_RTC_GetDate(&RealTimeClock, &ClockDate, RTC_FORMAT_BIN);
		HAL_RTC_GetTime(&RealTimeClock, &ClockTime, RTC_FORMAT_BIN);
		//timeHourCheck();
		//timeMinuteCheck();
		if(ClockHourFormat == CLOCK_HOUR_FORMAT_12) {
			BcdTime[0] = ((ClockTime.Hours/2) / 10);
			BcdTime[1] = ((ClockTime.Hours/2) % 10);
		}
		else {
			BcdTime[0] = (ClockTime.Hours / 10);
			BcdTime[1] = (ClockTime.Hours % 10);
		}
		BcdTime[2] = (ClockTime.Minutes / 10);
		BcdTime[3] = (ClockTime.Minutes % 10);
	}

	if(time_type == SET_TIME) {
		// current time
		//timeHourCheck();
		//timeMinuteCheck();
		if(ClockHourFormat == CLOCK_HOUR_FORMAT_12) {
			BcdTime[0] = ((ClockTime.Hours/2) / 10);
			BcdTime[1] = ((ClockTime.Hours/2) % 10);
		}
		else {
			BcdTime[0] = (ClockTime.Hours / 10);
			BcdTime[1] = (ClockTime.Hours % 10);
		}
		BcdTime[2] = (ClockTime.Minutes / 10);
		BcdTime[3] = (ClockTime.Minutes % 10);
	}

	else if (time_type == SET_ALARM){
		// alarm time
		//alarmHourCheck();
		//alarmMinuteCheck();
		if(ClockHourFormat == CLOCK_HOUR_FORMAT_12) {
			BcdTime[0] = ((ClockAlarm.AlarmTime.Hours/2) / 10);
			BcdTime[1] = ((ClockAlarm.AlarmTime.Hours/2) % 10);
		}
		else {
			BcdTime[0] = (ClockAlarm.AlarmTime.Hours / 10);
			BcdTime[1] = (ClockAlarm.AlarmTime.Hours % 10);
		}
		BcdTime[2] = (ClockAlarm.AlarmTime.Minutes / 10);
		BcdTime[3] = (ClockAlarm.AlarmTime.Minutes % 10);
	}

	else if (time_type == SET_24HR) {
		BcdTime[0] = 10;
		BcdTime[1] = 10;
		if (ClockHourFormat == CLOCK_HOUR_FORMAT_12) {
			BcdTime[2] = 1;
			BcdTime[3] = 2;
		}
		else {
			BcdTime[2] = 2;
			BcdTime[3] = 4;
		}
	}

}//GetCurrentTime();

void displaynum(int num){
	switch(num){
		case 0:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_SET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_SET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_SET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_SET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_SET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_RESET );//G
		break;

		case 1:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_RESET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_SET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_SET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_RESET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_RESET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_RESET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_RESET );//G
		break;

		case 2:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_SET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_SET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_RESET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_SET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_RESET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_SET );//G
		break;

		case 3:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_SET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_SET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_SET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_RESET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_RESET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_SET );//G
		break;

		case 4:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_RESET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_SET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_SET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_RESET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_RESET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_SET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_SET );//G
		break;

		case 5:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_SET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_RESET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_SET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_RESET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_SET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_SET );//G
		break;

		case 6:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_SET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_RESET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_SET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_SET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_SET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_SET );//G
		break;

		case 7:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_SET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_SET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_SET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_RESET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_RESET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_RESET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_RESET );//G
		break;

		case 8:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_SET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_SET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_SET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_SET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_SET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_SET );//G
		break;

		case 9:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_SET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_SET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_SET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_RESET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_SET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_SET );//G
		break;

		case 10: // off
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_RESET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_RESET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_RESET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_RESET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_RESET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_RESET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_RESET );//G
			break;

		case ALPHA:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_SET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_SET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_SET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_RESET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_SET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_SET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_SET );//G
		break;
		case LIMA:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_RESET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_RESET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_RESET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_SET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_SET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_RESET );//G
		break;
		case CHARLIE:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_SET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_RESET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_RESET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_SET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_SET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_SET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_SET );//G
		break;
		case KILO:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_RESET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_SET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_SET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_RESET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_SET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_SET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_SET );//G
		break;
		case PAPA:
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_RESET );//A
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_SET );//B
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_RESET );//C
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_8, GPIO_PIN_RESET );//D
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_14 , GPIO_PIN_SET );//E
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_15 , GPIO_PIN_SET );//F
			HAL_GPIO_WritePin( GPIOE, GPIO_PIN_10 , GPIO_PIN_SET );//G
		break;

	}//end switch number
}//display_num

void update_display(int digit){
	GetCurrentTime(mode);
	switch(digit){
		case 0: //clear
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_1 , GPIO_PIN_RESET );
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_3 , GPIO_PIN_RESET );
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_4 , GPIO_PIN_RESET );
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_6 , GPIO_PIN_RESET );
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_7 , GPIO_PIN_RESET );
				HAL_GPIO_WritePin( GPIOE, GPIO_PIN_2 , GPIO_PIN_RESET );
				HAL_GPIO_WritePin( GPIOE, GPIO_PIN_4 , GPIO_PIN_RESET );
				HAL_GPIO_WritePin( GPIOE, GPIO_PIN_6 , GPIO_PIN_RESET );
			break;

			case 1: //dig1
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_7 , GPIO_PIN_SET );
				displaynum(BcdTime[0]); // 10s digit of hours
			break;

			case 2: //dig2
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_7 , GPIO_PIN_RESET ); // turn off digit 1
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_6 , GPIO_PIN_SET );   // turn on digit 2
				displaynum(BcdTime[1]); // 1s digit of hours
			break;

			case 3: //dig3
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_6 , GPIO_PIN_RESET ); // turn off digit 2
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_4 , GPIO_PIN_SET );   // turn on digit 3
				displaynum(BcdTime[2]); // 10s digit of minutes
			break;

			case 4: //dig4
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_4 , GPIO_PIN_RESET ); // turn off digit 3
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_1 , GPIO_PIN_SET );   // turn on digit 4
				displaynum(BcdTime[3]); // 1s digit of minutes
			break;

			case 5: //colon
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_1 , GPIO_PIN_RESET ); // turn off digit 4
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_3 , GPIO_PIN_SET );   // turn on the colon
				HAL_GPIO_WritePin( GPIOE, GPIO_PIN_12 , GPIO_PIN_SET );	 // turn on L1/A (top dot)
				HAL_GPIO_WritePin( GPIOE, GPIO_PIN_7 , GPIO_PIN_SET );   // turn on L3/B (bottom dot)
				HAL_GPIO_WritePin( GPIOE, GPIO_PIN_13 , GPIO_PIN_RESET );  // turn off L3 (degree dot)

				//displaynum(colon); // middle colon
			break;

			case 6: // alarm mode lights
				HAL_GPIO_WritePin( GPIOD, GPIO_PIN_3 , GPIO_PIN_RESET ); // turn off the colon
				if(alarm_set)
				//if(led1)
					HAL_GPIO_WritePin( GPIOE, GPIO_PIN_2 , GPIO_PIN_SET ); // turn on the alarm light
				if(pm && (ClockHourFormat == CLOCK_HOUR_FORMAT_12))
				//if(led2)
					HAL_GPIO_WritePin( GPIOE, GPIO_PIN_4 , GPIO_PIN_SET ); // turn on the PM light
				if(ClockHourFormat == CLOCK_HOUR_FORMAT_24)
				//if(led3)
					HAL_GPIO_WritePin( GPIOE, GPIO_PIN_6 , GPIO_PIN_SET ); // turn on the 24hr light
		}//switch
}//update_display

void set_time(){
	static int digit = 0;
	if(b4 && read_buttons) {
		digit = (digit+1)%2;
		b4 = 0;
	}
	GetCurrentTime(SET_TIME);
	if(b3 && read_buttons) {
		switch(digit){
				case 0:
					ClockTime.Minutes += 1;
					break;
				case 1:
					ClockTime.Hours += 1;
					break;
		}
		timeMinuteCheck();
		timeHourCheck();
		HAL_RTC_SetDate(&RealTimeClock, &ClockDate, RTC_FORMAT_BIN);
		HAL_RTC_SetTime(&RealTimeClock, &ClockTime, RTC_FORMAT_BIN);
		b3 = 0;
	}//if increment
}//set_time

void set_alarm() {
	static int digit = 0;
	if(b4 && read_buttons) {
		digit = (digit+1)%2;
		b4 = 0;
	}
	GetCurrentTime(SET_ALARM);
	if(b3 && read_buttons) {
		switch(digit){
		switch(digit){
				case 0:
					ClockAlarm.AlarmTime.Minutes += 1;
					break;
				case 1:
					ClockAlarm.AlarmTime.Hours += 1;
					break;
		}
		alarmMinuteCheck();
	    alarmHourCheck();
		HAL_RTC_SetAlarm( &RealTimeClock, &ClockAlarm, RTC_FORMAT_BIN );
    	__HAL_RTC_ALARM_CLEAR_FLAG( &RealTimeClock, RTC_FLAG_ALRAF );
    	__HAL_RTC_EXTI_CLEAR_FLAG( RTC_EXTI_LINE_ALARM_EVENT );
    	HAL_RTC_SetAlarm_IT( &RealTimeClock, &ClockAlarm, RTC_FORMAT_BIN );
		b3 = 0;
	}//if increment
 }

 void set_24hr() {
	 GetCurrentTime(SET_24HR);
	if(b4 && read_buttons) {
				ClockHourFormat = ~ClockHourFormat;
				b4 = 0;
	}
 }



 /*
  * Function: Snooze
  *
  * Description:
  *
  * Add 10 Minutes to the current time and validate. Update the alarm and enable.
  *
  */

 //void Snooze(void)
 //{
 //}

int main(int argc, char* argv[])
{

//
// Reset of all peripherals, Initializes the Flash interface and the System timer.
//
	HAL_Init();

//
// Configure the system  and real time clock
//
	SystemClock_Config();
	RealTimeClockInit();

//
// Initialize all configured peripherals
//
	MX_GPIO_Init();

//
// Enable the serial debug port. This allows for text messages to be sent via the STlink virtual communications port to the host computer.
//
	DebugPortInit();

//
// Display project name with version number
//
	trace_puts(
			"*\n"
			"*\n"
			"* Alarm clock project for stm32f4discovery board V2.00\n"
			"*\n"
			"*\n"
			);

//
// Initialize the I2C port for the external CODEC
//
	MX_I2C1_Init();

//
// Configure the CODEC for analog pass through mode.
// This allows for audio to be played out of the stereo jack
//
	InitAudioChip();

//
// Initialize the flash file and the USB host adapter subsystem
//

	MX_FATFS_Init();
	MX_USB_HOST_Init();

//
// Initialize the DMA and DAC systems. This allows for audio to be played out of GPIOA pin 4
//
	ConfigureAudioDma();

//
// Initialize the seven segment display pins, timers, and button interruptd
//
	init_GPIO();
	init_timers();
	init_interrupts();

//
// Send a greeting to the trace device (skipped on Release).
//
	trace_puts("Initialization Complete");

//
// At this stage the system clock should have already been configured at high speed.
//
	trace_printf("System clock: %u Hz\n", SystemCoreClock);

//
// Start the system timer
//
	timer_start();
	blink_led_init();

//
// Wait until the drive is mounted before we can play some music
//
	do {
		MX_USB_HOST_Process();
	} while ( Appli_state != APPLICATION_READY );

	trace_printf( "\n" );

// *** MAIN LOOP *** //

	while ( TRUE )
	{
		switch(state) {
			case TIME:
				//debugging
				led1 = 1;
				led2 = 0;
				led3 = 0;

				mode = TELL_TIME;
				GetCurrentTime(TELL_TIME);

				//Snooze button pressed
				if(b5 && read_buttons) {
					ClockAlarm.AlarmTime.Minutes += 10;
					b5 = 0;
					HAL_NVIC_EnableIRQ( EXTI15_10_IRQn );
				}
				// Alarm turned off
				if(b1 && read_buttons) {
					b1 = 0;
					HAL_NVIC_EnableIRQ( EXTI4_IRQn );
				}
				break;
			case ALARM:
				//debugging
				led1 = 0;
				led2 = 1;
				led3 = 0;

				mode = TELL_TIME;

				// Play the alarm
				MSC_Application();
				break;
			case MODE:
				//debugging
				led1 = 0;
				led2 = 0;
				led3 = 1;

				// cycle through modes
				if (mode == SET_TIME) set_time();
				else if (mode == SET_ALARM) set_alarm();
				else if (mode == SET_24HR)  set_24hr();
				else if (mode == TELL_TIME) state = TIME;
				// re-enable mode button
				if(read_buttons) {
					b2 = 0;
					HAL_NVIC_EnableIRQ( EXTI9_5_IRQn );
				}
				break;
		}//switch

//
// Wait for an interrupt to occur
//
		__asm__ volatile ( "wfi" );
	}//while
}//main

/** System Clock Configuration
 */
void SystemClock_Config(void)
{

	RCC_OscInitTypeDef RCC_OscInitStruct;
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

	__HAL_RCC_PWR_CLK_ENABLE();

	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 168;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	HAL_RCC_OscConfig(&RCC_OscInitStruct);

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
			|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
	HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2S;
	PeriphClkInitStruct.PLLI2S.PLLI2SN = 192;
	PeriphClkInitStruct.PLLI2S.PLLI2SR = 2;
	HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

	HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

	HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

	/* SysTick_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

void RealTimeClockInit( void )
{
	RCC_OscInitTypeDef	RCC_OscInitStruct;

	RCC_PeriphCLKInitTypeDef	PeriphClkInitStruct;

	// Configure LSI as RTC clock source
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;

	if( HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK )
	{
		trace_printf( "HAL_RCC_OscConfig failed\r\n");
		while( TRUE );
	}

	// Assign the LSI clock to the RTC
	PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;

	if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
	{
		trace_printf( "HAL_RCCEx_PeriphCLKConfig failed\r\n");
		while( TRUE );
	}

	// Enable the RTC
	__HAL_RCC_RTC_ENABLE();

	// Configure the RTC format and clock divisor
	RealTimeClock.Instance = RTC;
	RealTimeClock.Init.HourFormat = RTC_HOURFORMAT_24;
	RealTimeClock.Init.AsynchPrediv = 127;
	RealTimeClock.Init.SynchPrediv = 0xFF;
	RealTimeClock.Init.OutPut = RTC_OUTPUT_DISABLE;
	RealTimeClock.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
	RealTimeClock.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
	HAL_RTC_Init(&RealTimeClock );

	// Configure Alarm format
	ClockAlarm.Alarm = RTC_ALARM_A;
	ClockAlarm.AlarmTime.TimeFormat = RTC_HOURFORMAT_24;
	ClockAlarm.AlarmTime.Hours = 8;
	ClockAlarm.AlarmTime.Minutes = 01;
	ClockAlarm.AlarmTime.Seconds = 01;
	ClockAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY;
	ClockAlarm.AlarmDateWeekDay = RTC_WEEKDAY_THURSDAY;
	HAL_RTC_SetAlarm_IT(&RealTimeClock, &ClockAlarm, RTC_FORMAT_BIN);

	__HAL_RTC_ALARM_ENABLE_IT(&RealTimeClock, RTC_IT_ALRA);
	__HAL_RTC_ALARMA_ENABLE(&RealTimeClock);

	HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 0,1);
	HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);

	// Disable the write protection for RTC registers
	__HAL_RTC_WRITEPROTECTION_DISABLE( &RealTimeClock );

	// Structure to set the time in the RTC
	 	ClockTime.Hours = 8;
		ClockTime.Minutes = 00;
		ClockTime.Seconds = 00;
		ClockTime.SubSeconds = 0;
		ClockTime.TimeFormat = RTC_HOURFORMAT_24;
		ClockTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
		ClockTime.StoreOperation = RTC_STOREOPERATION_RESET;

	// Structure to set the date in the RTC
	 	ClockDate.Date = 	2;
		ClockDate.Month = 	RTC_MONTH_AUGUST;
		ClockDate.WeekDay = RTC_WEEKDAY_THURSDAY;
		ClockDate.Year =	18;

	// Set the date and time in the RTC
		HAL_RTC_SetDate(&RealTimeClock, &ClockDate, RTC_FORMAT_BIN);
		HAL_RTC_SetTime(&RealTimeClock, &ClockTime, RTC_FORMAT_BIN);
}//RealTimeClockInit


/*
 * Function: ConfigureAudioDma
 *
 * Description:
 *
 * Initialize DMA, DAC and timer 6 controllers for a mono channel audio to be played on PA4
 *
 */

void ConfigureAudioDma( void )
{

	TIM_MasterConfigTypeDef
		Timer6MasterConfigSync;

	GPIO_InitTypeDef
		GPIO_InitStructure;

	DAC_ChannelConfTypeDef
		DacConfig;

//
// If we have the timer 6 interrupt enabled then disable the timer from running when we halt the processor or hit a breakpoint.
// This also applies to printing using the semihosting method which also uses breakpoints to transfer data to the host computer
//


	__HAL_DBGMCU_UNFREEZE_TIM5();

//
// Enable the clocks for GPIOA, GPIOC and Timer 6
//
	__HAL_RCC_TIM6_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();


//
// Configure PA4 as an analog output ( used for D/A output of the analog signal )
//

	GPIO_InitStructure.Pin = GPIO_PIN_4;
	GPIO_InitStructure.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStructure.Alternate = 0;
	HAL_GPIO_Init( GPIOA, &GPIO_InitStructure);




//
// Configure timer 6 for a clock frequency of 44Khz and a triggered output for the DAC
//
	Timer6_44Khz.Instance = TIM6;
	Timer6_44Khz.Init.Prescaler = 20; //this value may have to be changed
	Timer6_44Khz.Init.CounterMode = TIM_COUNTERMODE_UP;
	Timer6_44Khz.Init.Period = 90; // this value may have to be changed
	Timer6_44Khz.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	HAL_TIM_Base_Init( &Timer6_44Khz );

	Timer6MasterConfigSync.MasterOutputTrigger = TIM_TRGO_UPDATE;
	Timer6MasterConfigSync.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	HAL_TIMEx_MasterConfigSynchronization( &Timer6_44Khz, &Timer6MasterConfigSync );

//
// Set the priority of the interrupt and enable it
//
	NVIC_SetPriority(TIM6_DAC_IRQn, 0);
	NVIC_EnableIRQ(TIM6_DAC_IRQn);

//
// Clear any pending interrupts
//
	__HAL_TIM_CLEAR_FLAG( &Timer6_44Khz, TIM_SR_UIF );



//
// Enable the timer interrupt and the DAC Trigger
//

	__HAL_TIM_ENABLE_DMA( &Timer6_44Khz, TIM_DIER_UDE );


//
// Enable the clocks for the DAC
//
	__HAL_RCC_DAC_CLK_ENABLE();

	AudioDac.Instance = DAC;
	if ( HAL_OK != HAL_DAC_Init( &AudioDac ))
	{
		trace_printf("DAC initialization failure\n");
		return;
	}

//
// Enable the trigger from the DMA controller and the output buffer of the DAC
//
	DacConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
	DacConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;

	if ( HAL_DAC_ConfigChannel(&AudioDac, &DacConfig, DAC_CHANNEL_1) != HAL_OK )
	{
		trace_printf("DAC configuration failure\n");
		return;
	}

//
// Enable the clock for the DMA controller
//
	__HAL_RCC_DMA1_CLK_ENABLE();

//
// Initialize the stream and channel number and the memory transfer settings
//

    AudioDma.Instance = DMA1_Stream5;
    AudioDma.Init.Channel = DMA_CHANNEL_7;
    AudioDma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    AudioDma.Init.PeriphInc = DMA_PINC_DISABLE;
    AudioDma.Init.MemInc = DMA_MINC_ENABLE;
    AudioDma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    AudioDma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    AudioDma.Init.Mode = DMA_NORMAL;
    AudioDma.Init.Priority = DMA_PRIORITY_MEDIUM;
    AudioDma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init( &AudioDma );

//
// Link the DMA channel the to the DAC controller
//
    __HAL_LINKDMA( &AudioDac, DMA_Handle1, AudioDma );

//
// Enable the interrupt for the specific stream
//
    HAL_NVIC_SetPriority( DMA1_Stream5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ( DMA1_Stream5_IRQn );

//
// Start the timer
//
	__HAL_TIM_ENABLE( &Timer6_44Khz );

	return;
}// Configure Audio DMA


/* I2C1 init function */
void MX_I2C1_Init(void)
{

	I2c.Instance = I2C1;
	I2c.Init.ClockSpeed = 100000;
	I2c.Init.DutyCycle = I2C_DUTYCYCLE_2;
	I2c.Init.OwnAddress1 = 0;
	I2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	I2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	I2c.Init.OwnAddress2 = 0;
	I2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	I2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	HAL_I2C_Init( &I2c );

}

void MX_GPIO_Init(void)
{

	GPIO_InitTypeDef GPIO_InitStruct;

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();



	GPIO_InitStruct.Pin = GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);


	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOD, LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
			|Audio_RST_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : CS_I2C_SPI_Pin */
	GPIO_InitStruct.Pin = CS_I2C_SPI_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

	HAL_GPIO_Init(CS_I2C_SPI_GPIO_Port, &GPIO_InitStruct);
	HAL_GPIO_Init( GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : OTG_FS_PowerSwitchOn_Pin */
	GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : PDM_OUT_Pin */
	GPIO_InitStruct.Pin = PDM_OUT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(PDM_OUT_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : B1_Pin */
	GPIO_InitStruct.Pin = B1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);


	/*Configure GPIO pins : PA5 PA6 PA7 */
	GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : BOOT1_Pin */
	GPIO_InitStruct.Pin = BOOT1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(BOOT1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : CLK_IN_Pin */
	GPIO_InitStruct.Pin = CLK_IN_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(CLK_IN_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : LD4_Pin LD3_Pin LD5_Pin LD6_Pin
                           Audio_RST_Pin */
	GPIO_InitStruct.Pin = LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
			|Audio_RST_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/*Configure GPIO pin : OTG_FS_OverCurrent_Pin */
	GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : MEMS_INT2_Pin */
	GPIO_InitStruct.Pin = MEMS_INT2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(MEMS_INT2_GPIO_Port, &GPIO_InitStruct);

}

/**
 * @brief  Main routine for Mass Storage Class
 * @param  None
 * @retval None
 */
static void MSC_Application(void)
{
	FRESULT
		Result;                                          /* FatFs function common result code */

//
// Mount the flash drive using a fat file format
//

	Result = f_mount( &UsbDiskFatFs, (TCHAR const*)USBH_Path, 0);
	if( FR_OK == Result )
	{

//
// File system successfully mounted, play all the music files in the directory.
//
		while ( TRUE == PlayMusic )
		{
			PlayDirectory( "", 0 );
		}
	}
	else
	{
//
// FatFs Initialization Error
//
	//	Error_Handler();
	}

//
// Unlink the USB disk I/O driver
//
	FATFS_UnLinkDriver( UsbDiskPath );
}//MSC_Application


#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
