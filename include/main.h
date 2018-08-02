


#ifndef MAIN_H_
#define MAIN_H_

#include "stm32f4xx_hal_rtc.h"

#define CHECK_TIMER					FALSE			/* Enable code for verifying the audio timer is a the right frequency */
#define SNOOZE_TIME					2  				/* 10 minute snooze time */
#define CLOCK_MODE_DISPLAY_DELAY 	1000			/* 2 second delay time to display the currently selected hour mode ( 12 or 24 hours ) */
#define CLOCK_HOUR_FORMAT_12		0				/* Display the time with AM/PM format */
#define	CLOCK_HOUR_FORMAT_24		1				/* Display the time with 24 Hour format */

//
// Buttons definition
//
#define ALARM_BUTTON			GPIO_PIN_4  	/* GPIOC->IDR & GPIO_PIN_4 */
#define MODE_BUTTON				GPIO_PIN_5  	/* GPIOC->IDR & GPIO_PIN_5 */
#define	TIME_BUTTON				GPIO_PIN_8 		/* GPIOC->IDR & GPIO_PIN_8 */
#define SELECT_BUTTON			GPIO_PIN_9  	/* GPIOC->IDR & GPIO_PIN_9 */
#define	SNOOZE_BUTTON			GPIO_PIN_11  	/* GPIOC->IDR & GPIO_PIN_11 */


//
// Secondary function buttons
//

#define MINUTES_BUTTON				SNOOZE_BUTTON
#define HOURS_BUTTON				MODE_BUTTON

//
// Button mask
//

#define ALL_BUTTONS					( SNOOZE_BUTTON | ALARM_BUTTON | MODE_BUTTON | SELECT_BUTTON | TIME_BUTTON )
#define BUTTONS_MASK 				ALL_BUTTONS

//
// Single button test definitions
//

#define MODE_BUTTON_PUSHED			( 0 != ( MODE_BUTTON & ButtonsPushed ))
#define SNOOZE_BUTTON_PUSHED		( 0 != ( SNOOZE_BUTTON & ButtonsPushed ))
#define ON_OFF_BUTTON_PUSHED		( 0 != ( ON_OFF_BUTTON & ButtonsPushed ))
#define TIME_BUTTON_PUSHED			( 0 != ( TIME_BUTTON & ButtonsPushed ))
#define ALARM_BUTTON_PUSHED			( 0 != ( ALARM_BUTTON & ButtonsPushed ))

#define MINUTES_BUTTON_PUSHED		( 0 != ( MINUTES_BUTTON & ButtonsPushed ))
#define HOURS_BUTTON_PUSHED			( 0 != ( HOURS_BUTTON & ButtonsPushed ))

//
// Multiple button test definitions
//

#define INCREMENT_TIME_MINUTES		( TIME_BUTTON_PUSHED && MINUTES_BUTTON_PUSHED )
#define INCREMENT_TIME_HOURS		( TIME_BUTTON_PUSHED && HOURS_BUTTON_PUSHED )
#define INCREMENT_ALARM_MINUTES		( ALARM_BUTTON_PUSHED && MINUTES_BUTTON_PUSHED )
#define INCREMENT_ALARM_HOURS		( ALARM_BUTTON_PUSHED && HOURS_BUTTON_PUSHED )



extern RTC_InitTypeDef
	ClockInit;

extern RTC_TimeTypeDef
	ClockTime;

extern RTC_AlarmTypeDef
	ClockAlarm;

extern volatile int
	PlayMusic;

#endif /* MAIN_H_ */
