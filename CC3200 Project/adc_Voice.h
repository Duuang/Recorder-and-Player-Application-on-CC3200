// Standard includes
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>

//#include "simplelink.h"

// Driverlib includes
//#include "hw_types.h"
//#include "hw_ints.h"
//#include "hw_memmap.h"
#include "hw_common_reg.h"
//#include "interrupt.h"
#include "hw_apps_rcm.h"
#include "hw_adc.h"
//#include "prcm.h"
//#include "rom.h"
//#include "rom_map.h"
//#include "gpio.h"
//#include "utils.h"
#include "adc.h"

// Common interface includes
//#include "gpio_if.h"
//#include "pinmux.h"
//#include "common.h"

//#include "uart_if.h"

//#include "LCD_TFT_ILI9341.h"
//#include "Font_lib.h"
//#include "LCD_Display.h"

#include "file_operations.h"

//JP6--voice
//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
unsigned char sample_results[20005];
unsigned char final_results[20005];
unsigned long sample_amount = 20000;

unsigned long amount_average = 30;

#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

//choose the PIN60 CC_GPIO_5

void wait_ms(long i) {
	while (i) {
		__delay_cycles(20000);
		;
		i--;
	}

}

#define NO_OF_SAMPLES 		128

//unsigned long pulAdcSamples[4096];
unsigned int uiIndex = 0;

void Voiceinit(void) {

	//
	// Pinmux for the selected ADC input pin
	//
	MAP_PinTypeADC(PIN_60, PIN_MODE_255);

	//
	// Configure ADC timer which is used to timestamp the ADC data samples
	//
	MAP_ADCTimerConfig(ADC_BASE, 2 ^ 17);

	//
	// Enable ADC timer which is used to timestamp the ADC data samples
	//
	MAP_ADCTimerEnable (ADC_BASE);

	//
	// Enable ADC module
	//
	MAP_ADCEnable(ADC_BASE);

}

long RecordAndSave(unsigned long *ulToken, long *lFileHandle) {

	Voiceinit();
	memset(sample_results, 0, sizeof(sample_results));
	unsigned long ulSample;
	//
	// Initialize Array index for multiple execution
	//
	uiIndex = 0;

	//
	// Enable ADC channel
	//

	MAP_ADCChannelEnable(ADC_BASE, ADC_CH_3);

	//

	long lRetVal = -1;
	unsigned char policyVal;
	//long lFileHandle;
	//unsigned long ulToken;

	unsigned long last_timestamp;
	unsigned long timestamp;

	int iLoopCnt = 0;
	int error_cnt = 0;
	int success_cnt = 0;
	unsigned long timepassed;

	//
	//show recording page
	//
	fillScreen_a();
	drawString("Recording", 0, 100, 4, RED);
	fillCircle(110, 220, 50, RED);
	//
	//init the value of last_timestamp
	//
	if (MAP_ADCFIFOLvlGet(ADC_BASE, ADC_CH_3)) {
		ulSample = MAP_ADCFIFORead(ADC_BASE, ADC_CH_3);
		last_timestamp = ulSample >> 14;
		uiIndex++;
	}
	//
	//start sampling
	//
	Report("start sampling\n\r");
	int base_offset = 44; // because of file_headblock[44], offset starts from 44

	for (iLoopCnt = 0; iLoopCnt < sample_amount - 44;) {  // amount - 44, because of headblock
		if (MAP_ADCFIFOLvlGet(ADC_BASE, ADC_CH_3)) {
			ulSample = MAP_ADCFIFORead(ADC_BASE, ADC_CH_3);
			//UART_PRINT("\n\rbits[13:2] : ADC sample\n\r");
			//UART_PRINT("\n\rbits[31:14]: Time stamp of ADC sample \n\r");
			timestamp = ulSample >> 14;

			if (timestamp < last_timestamp) {
				timepassed = ~(timestamp - last_timestamp - 1); // CAUTION! unsigned long!
			} else {
				timepassed = timestamp - last_timestamp;
			}

			if (timepassed < 15 * 640) {
				//do nothing, continue to loop

			} else if (timepassed <= 17 * 640) {
				success_cnt++; //e.g. samle_result= 0011 0000 1111 1000 0001 1001 0001 1000
				iLoopCnt++;    //                 & 0000 0000 0000 0000 0011 1111 1111 11xx
				last_timestamp = timestamp;
				sample_results[base_offset + iLoopCnt] =
						(unsigned char) (((ulSample >> 2) & 0x0FFF) >> 4); //calculate the sample_result

			} else {
				error_cnt++;    //calculate the error times (times that sampled too slow)
				last_timestamp = timestamp;  //reset the last_timestamp, and continue sampling
			}
		}
	}
	Report("err:%d\n\r", error_cnt);
	Report("success: %d\n\r", success_cnt);
	Report("cnt: %ld\n\r", iLoopCnt);
	//*****************************************
	// sampling completed
	//*****************************************
	// draw saving
	fillScreen_a();
	drawString("Proccesing", 0, 100, 4, YELLOW);
	drawString("and Saving", 0, 180, 4, YELLOW);

	lRetVal = sl_FsOpen((unsigned char *) "aaaa.txt", FS_MODE_OPEN_WRITE,  // open the file
			ulToken, lFileHandle);
	if (lRetVal < 0) {
		lRetVal = sl_FsClose(*lFileHandle, 0, 0, 0);
		ASSERT_ON_ERROR(FILE_OPEN_WRITE_FAILED);
	}

	//
	// write the headblock of WAV into file, 44 bytes
	//
	unsigned char file_headblock[44] = { 'R', 'I', 'F', 'F', 0xFF, 0xFF, 0xFF,
			0xFF, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 0x10, 0xCC, 0xCC,
			0xCC, 0x01, 0xCC, 0x01, 0xCC,    //CC代表00。。串口不接受00啊？，串口接收时候再做特殊处理吧
			0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,   //CC代表待填的采样频率
			0x01, 0xCC, 0x08, 0xCC, 'd', 'a', 't', 'a', 0xFF, 0xFF, 0xFF, 0xFF }; //CC代表00
	unsigned long sample_frequency = 4167;
	// calculate the sample rate into Little-Endian hex format (because it will be played on x86 pc)
	file_headblock[24] = file_headblock[28] = (unsigned char) (sample_frequency & 0x000000FF);
	file_headblock[25] = file_headblock[29] = (unsigned char) ((sample_frequency >> 2) & 0x0000FF);
	file_headblock[26] = file_headblock[30] = (unsigned char) ((sample_frequency >> 4) & 0x00FF);
	file_headblock[27] = file_headblock[31] = (unsigned char) (sample_frequency >> 6);

	int i = 0;
	for (i = 0; i < 44; i++) {    //write the file_headblock[] into first 44 bytes of file
		lRetVal = sl_FsWrite(*lFileHandle, (unsigned int) (i),
				(unsigned char*) &file_headblock[i], sizeof(unsigned char));
	}
	if (lRetVal < 0) {
		lRetVal = sl_FsClose(*lFileHandle, 0, 0, 0);
		ASSERT_ON_ERROR(FILE_WRITE_FAILED);
	}
	//**********************************************************************************
	//start proccessing sample data by calculating the average value of sample data nearby
	//*************************************************************************************
	memset(final_results, 0, sizeof(final_results));
	for (iLoopCnt = 0; iLoopCnt < sample_amount - 44; iLoopCnt++) {
		// if amount_average == 0, don't calculate average value
		if (amount_average == 0) {
			lRetVal = sl_FsWrite(*lFileHandle,
					(unsigned int) (base_offset
							+ iLoopCnt * sizeof(unsigned char)),
					(unsigned char*) (&sample_results[base_offset + iLoopCnt]),
					sizeof(unsigned char));
			if (lRetVal < 0) {
				lRetVal = sl_FsClose(*lFileHandle, 0, 0, 0);
				ASSERT_ON_ERROR(FILE_WRITE_FAILED);
			}
		// calculate average value according to amount_average (default 30)
		} else {
			if (iLoopCnt >= amount_average
					&& iLoopCnt <= sample_amount - 45 - amount_average) {
				int i;
				for (i = -1 * (int) amount_average; i <= (int) amount_average;
						i++) {

					final_results[base_offset + iLoopCnt] +=
							(unsigned char) ((float) sample_results[base_offset
									+ iLoopCnt + i]
									/ ((float) (amount_average * 2 + 1)));
				}
			} else {
				final_results[base_offset + iLoopCnt] =
						sample_results[base_offset + iLoopCnt];
			}
			lRetVal = sl_FsWrite(*lFileHandle,
					(unsigned int) (base_offset
							+ iLoopCnt * sizeof(unsigned char)),
					(unsigned char*) (&final_results[base_offset + iLoopCnt]),
					sizeof(unsigned char));
			if (lRetVal < 0) {
				lRetVal = sl_FsClose(*lFileHandle, 0, 0, 0);
				ASSERT_ON_ERROR(FILE_WRITE_FAILED);
			}
		}
	}
	Report("closing file\n\r");
	//
	// close the user file
	//
	lRetVal = sl_FsClose(*lFileHandle, 0, 0, 0);
	if (SL_RET_CODE_OK != lRetVal) {
		ASSERT_ON_ERROR(FILE_CLOSE_ERROR);
	}

	MAP_ADCChannelDisable(ADC_BASE, ADC_CH_3);

	return 0;

//ends here  ------------------------------------------------------------------------------
	/*
	 //
	 //  open aaaa.txt for writing
	 //
	 long lRetVal = -1;
	 unsigned char policyVal;
	 //long lFileHandle;
	 //unsigned long ulToken;

	 lRetVal = sl_FsOpen((unsigned char *)"aaaa.txt",
	 FS_MODE_OPEN_WRITE,
	 ulToken,
	 lFileHandle);
	 if(lRetVal < 0)
	 {
	 lRetVal = sl_FsClose(*lFileHandle, 0, 0, 0);
	 ASSERT_ON_ERROR(FILE_OPEN_WRITE_FAILED);
	 }

	 unsigned long last_timestamp;
	 unsigned long timestamp;
	 unsigned char sample_value;
	 int file_offset = 0;



	 int iLoopCnt = 0;
	 int error_cnt = 0;
	 int success_cnt = 0;
	 unsigned long timepassed;

	 //
	 // write the headblock of WAV into file, 44 bytes
	 //


	 unsigned char file_headblock[44] = {'R', 'I', 'F', 'F', 0xFF, 0xFF, 0xFF, 0xFF, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
	 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
	 0x01, 0x00, 0x08, 0x00, 'd', 'a', 't', 'a', 0xFF, 0xFF, 0xFF, 0xFF};
	 unsigned long sample_frequency = 625;

	 file_headblock[24] = file_headblock[28] = (unsigned char)(sample_frequency & 0x000000FF);
	 file_headblock[25] = file_headblock[29] = (unsigned char)((sample_frequency >> 2) & 0x0000FF);
	 file_headblock[26] = file_headblock[30] = (unsigned char)((sample_frequency >> 4) & 0x00FF);
	 file_headblock[27] = file_headblock[31] = (unsigned char)(sample_frequency >> 6);

	 lRetVal = sl_FsWrite(*lFileHandle,
	 (unsigned int)(0),
	 (unsigned char*)file_headblock, sizeof(unsigned char) * 44);

	 if (lRetVal < 0)
	 {
	 lRetVal = sl_FsClose(*lFileHandle, 0, 0, 0);
	 ASSERT_ON_ERROR(FILE_WRITE_FAILED);
	 }





	 if(MAP_ADCFIFOLvlGet(ADC_BASE, ADC_CH_3))
	 {
	 ulSample = MAP_ADCFIFORead(ADC_BASE, ADC_CH_3);
	 last_timestamp = ulSample >> 14;
	 uiIndex++;
	 }
	 Report("start sampleing\n\r");
	 int base_offset = 44; // because of file_headblock[44], offset starts from 44

	 for (iLoopCnt = 0; iLoopCnt < 3600 - 44; )
	 {
	 if(MAP_ADCFIFOLvlGet(ADC_BASE, ADC_CH_3))
	 {
	 ulSample = MAP_ADCFIFORead(ADC_BASE, ADC_CH_3);
	 timestamp = ulSample >> 14;

	 //Report("## %ld\n\r", ulSample);
	 //Report(":::%ld,  %ld\n\r", (timestamp - last_timestamp) / 640, timestamp - last_timestamp);
	 //Report("timestamp, value:%ld,  %ld\n\r", timestamp, sample_value);
	 //UART_PRINT("\n\rbits[13:2] : ADC sample\n\r");
	 //UART_PRINT("\n\rbits[31:14]: Time stamp of ADC sample \n\r");
	 if (timestamp < last_timestamp) {
	 //timepassed = timestamp - last_timestamp;
	 timepassed = ~(timestamp - last_timestamp - 1);
	 } else {
	 timepassed = timestamp - last_timestamp;
	 }


	 if ( timepassed / 640 < 100 ) {
	 //do nothing, continue to loop

	 } else if (timepassed / 640 < 200) {
	 success_cnt++;  //0011 0000 1111 1000 0001 1001 0001 1000	,   1606
	 iLoopCnt++;     //0000 0000 0000 0000 0011 1111 1111 11xx
	 last_timestamp = timestamp;
	 sample_value = (unsigned char)(((ulSample >> 2) & 0x0FFF) >> 4);
	 //Report("success\n\r%d\n\r", (int)sample_value);


	 //lRetVal = sl_FsWrite(*lFileHandle,
	 //                   (unsigned int)(iLoopCnt * sizeof(ulSample)),
	 //                  (unsigned long*)ulSample, sizeof(ulSample));
	 lRetVal = sl_FsWrite(*lFileHandle,
	 (unsigned int)(base_offset + iLoopCnt * sizeof(sample_value)),
	 (unsigned char*)&sample_value, sizeof(sample_value));
	 if (lRetVal < 0)
	 {
	 lRetVal = sl_FsClose(*lFileHandle, 0, 0, 0);
	 ASSERT_ON_ERROR(FILE_WRITE_FAILED);
	 }

	 //uiIndex++;
	 } else {

	 //unsigned long a = (timestamp - last_timestamp) / 640;
	 //Message("sample too slow\n\r");
	 //if (error_cnt < 50) {
	 //Report("time passed: %ld\n\r", timepassed);
	 //Report("time passedaaa: %ld\n\r",(unsigned long) (timepassed / 640));
	 //}
	 error_cnt++;
	 last_timestamp = timestamp;
	 //break;


	 }
	 //pulAdcSamples[uiIndex++] = ulSample;
	 }
	 }

	 Report("err:%d\n\r", error_cnt);
	 Report("success: %d\n\r", success_cnt);
	 Report("cnt: %ld\n\r", iLoopCnt);



	 //
	 // close the user file
	 //
	 lRetVal = sl_FsClose(*lFileHandle, 0, 0, 0);
	 if (SL_RET_CODE_OK != lRetVal)
	 {
	 ASSERT_ON_ERROR(FILE_CLOSE_ERROR);
	 }

	 MAP_ADCChannelDisable(ADC_BASE, ADC_CH_3);

	 return 0;
	 */

}

/*
 * main.c
 */

/*
 void StartRecording(void) {

 float voltage;

 //lcd_init();
 //LCD_ILI9341_TFT_background(White);
 //LCD_ILI9341_TFT_foreground(Black);
 //LCD_Show_StandbyPage();
 lcd_clear();

 Voiceinit();
 while(1)
 {
 voltage=Voicegetvalue();
 char string[20];
 sprintf(string, "%f", voltage);
 LCD_StringDisplay(50,50,string);
 }
 }
 */

