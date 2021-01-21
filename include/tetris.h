#ifndef __TETRIS_H__
#define __TETRIS_H__

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)

extern TaskHandle_t testTetris;

int tetrisMain(void);

#endif