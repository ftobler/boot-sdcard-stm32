/*
 * bootloader.c
 *
 *  Created on: Apr 7, 2022
 *      Author: ftobler
 */


#include "bootloader.h"
#include "string.h"
#include "stm32f1xx_hal.h"
#include "ff.h"
#include "gpio_low_level.h"
#include "main.h"


#define FIRSTPAGE 10
#define LASTPAGE 248

typedef enum {
	NO_SDCARD,
	PROGRAMMED,
	JUST_BOOTED,
} sequence_t;



static void boot();
static void erasePage(uint32_t pagenr);
static void programPage(uint32_t pagenr, uint8_t* data, uint32_t size);
static void led_sequence(sequence_t sequence);
static void led_swich(uint8_t led0, uint8_t led1, uint8_t led2, uint8_t led3);

__attribute__((naked)) void startApplication(uint32_t stackPointer, uint32_t startupAddress);


static FATFS fatFs;
static FIL file;
static uint8_t pagedat[2048]; //must be 4 byte aligned


void bootloader() {
	if (gpio_ll_read(CARD_DETECT_GPIO_Port, CARD_DETECT_Pin)) {
		led_sequence(NO_SDCARD);
		boot();
	}
	volatile FRESULT res;
	res = f_mount(&fatFs, "", 0);
	if (res) {
		led_sequence(NO_SDCARD);
		boot();
	}
	res = f_open(&file, "binary.bin", FA_READ);
	if (res) {
		led_sequence(NO_SDCARD);
		boot();
	}
	led_swich(1, 1, 1, 1);

	uint32_t page = FIRSTPAGE;
	uint32_t programmed = 0;
	while (page < LASTPAGE) {
		memset(pagedat, 0xFF, PAGESIZE);   //clear the buffer
		UINT len;
		f_read(&file, pagedat, PAGESIZE, &len); //read from file
		uint8_t* flashpointer = (uint8_t*)(FLASH_BASE + PAGESIZE * page);
		if (memcmp(flashpointer, pagedat, len) != 0) {  //check if there is a difference
			//program page
			if (!programmed) {
				HAL_FLASH_Unlock();
				programmed = 1;
			}
			erasePage(page);
			programPage(page, (uint8_t*)pagedat, 2048);
		}
		if (len < PAGESIZE) {
			break; //file is ending
		}
		page++;
	}
	if (programmed) {
		led_sequence(PROGRAMMED);
	} else {
		led_sequence(JUST_BOOTED);
	}
	boot();
}



static void erasePage(uint32_t pagenr) {
	FLASH_EraseInitTypeDef def;
	def.Banks = FLASH_BANK_1;
	def.NbPages = 1;
	def.PageAddress = FLASH_BASE + PAGESIZE * pagenr;
	def.TypeErase = FLASH_TYPEERASE_PAGES;
	uint32_t err;
	HAL_FLASHEx_Erase(&def, &err);
}


static void programPage(uint32_t pagenr, uint8_t* data, uint32_t size) {
	uint32_t targetaddr = FLASH_BASE + PAGESIZE * pagenr;
	uint32_t* data32 = (uint32_t*)data;
	size = size / 4;
	while (size) {
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, targetaddr, *data32);
		targetaddr += 4;
		data32++;
		size--;
	}
}


void boot() {
	uint32_t* app_start = (uint32_t*)(FLASH_BASE + PAGESIZE * FIRSTPAGE);
	__disable_irq();
	SCB->VTOR = (uint32_t)app_start;
	startApplication(app_start[0], app_start[1]);
}

/**
 * starts an application (sets the main stack pointer to stackPointer and jumps to startupAddress)
 */
__attribute__((naked)) void startApplication(uint32_t stackPointer, uint32_t startupAddress){
	__ASM("msr msp, r0"); //set stack pointer to application stack pointer
	__ASM("bx r1");       //branch to application startup code
}


static void led_sequence(sequence_t sequence) {
	HAL_Delay(300);
	for (int i = 0; i < 4; i++) {
		switch (sequence) {
		case NO_SDCARD:
			led_swich(1, 1, 1, 1);
			led_swich(0, 0, 0, 0);
			led_swich(1, 1, 1, 1);
			led_swich(0, 0, 0, 0);
			break;
		case PROGRAMMED:
			led_swich(1, 0, 0, 0);
			led_swich(0, 1, 0, 0);
			led_swich(0, 0, 1, 0);
			led_swich(0, 0, 0, 1);
			led_swich(0, 0, 1, 0);
			led_swich(0, 1, 0, 0);
			break;
		case JUST_BOOTED:
			led_swich(1, 1, 0, 0);
			led_swich(0, 0, 1, 1);
			led_swich(1, 1, 0, 0);
			led_swich(0, 0, 1, 1);
			break;
		}
	}
	led_swich(0, 0, 0, 0);
	HAL_Delay(300);
}


static void led_swich(uint8_t led0, uint8_t led1, uint8_t led2, uint8_t led3) {
	gpio_ll_write(LED0_GPIO_Port, LED0_Pin, led0);
	gpio_ll_write(LED1_GPIO_Port, LED1_Pin, led1);
	gpio_ll_write(LED2_GPIO_Port, LED2_Pin, led2);
	gpio_ll_write(LED3_GPIO_Port, LED3_Pin, led3);
	HAL_Delay(100);
}
