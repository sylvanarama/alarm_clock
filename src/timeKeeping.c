/*
 * timeKeeping.c
 *
 *  Created on: Mar 15, 2017
 *      Author: brent
 */


//This code is used with the AlarmClockMp3 project
//it is called when incrementing the time manually and
//it ensures the time values stay within the bounds of a 12 hour clock

#include "stm32f4xx_hal.h"
#include "timeKeeping.h"

void timeHourCheck(void)
{
	 //incrementing 23 --> 24, rolls over to 0, set am
	 if(ClockTime.Hours >= 24)
	 {
			 ClockTime.Hours = ClockTime.Hours % 24;
			 ClockTime.TimeFormat = RTC_HOURFORMAT12_AM;

	 }

	 //incrementing passes 12 -> toggles am/pm
	 if( ClockTime.Hours == 12)
	 {
			 ClockTime.TimeFormat = RTC_HOURFORMAT12_PM;
	 }


}

void timeMinuteCheck(void)
{
        //allows 9 to roll over to 0 and 60 to 0
         if(ClockTime.Minutes >= 60)
         {
                 ClockTime.Minutes = ClockTime.Minutes % 60;
         }
}

void alarmHourCheck(void)
{
	 //incrementing 23 --> 24, rolls over to 0, set am
	 if(ClockAlarm.AlarmTime.Hours >= 24)
	 {
		 ClockAlarm.AlarmTime.Hours = ClockAlarm.AlarmTime.Hours % 24;
		 ClockAlarm.AlarmTime.TimeFormat = RTC_HOURFORMAT12_AM;

	 }

	 //incrementing passes 12 -> toggles am/pm
	 if( ClockAlarm.AlarmTime.Hours == 12)
	 {
		 ClockAlarm.AlarmTime.TimeFormat = RTC_HOURFORMAT12_PM;
	 }

}

void alarmMinuteCheck(void)
{
	//allows 60 to roll over to 0 a
	 if(ClockAlarm.AlarmTime.Minutes >= 60)
	 {
			 ClockAlarm.AlarmTime.Minutes = ClockAlarm.AlarmTime.Minutes % 60;
	 }
}

