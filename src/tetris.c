#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL_scancode.h>

#include "TUM_Utils.h"
#include "TUM_Event.h"
#include "TUM_Font.h"
#include "TUM_Sound.h"
#include "TUM_Ball.h"
#include "TUM_Draw.h"

#include "AsyncIO.h"

#include "tetris.h" // Must be before FreeRTOS includes
#include "main.h"
#include "queue.h"

#define OFFSET  20
#define TETRIS_WALL_WIDTH_INNER 20 

#define Gray    (unsigned int)(0x808080)


TaskHandle_t testTetris = NULL;
TaskHandle_t drawTask = NULL; 
SemaphoreHandle_t ScreenLock = NULL;
SemaphoreHandle_t DrawSignal = NULL;



void drawWalls(){
    tumDrawFilledBox(0, 0, 30, 700, Gray);
    tumDrawFilledBox(0, 670, 450, 30, Gray);
    
}





void drawingTask(void * pvParameters){
    while(1){
        if(DrawSignal){
            if(xSemaphoreTake(DrawSignal, portMAX_DELAY) == pdTRUE){
                xSemaphoreTake(ScreenLock, portMAX_DELAY);
                tumDrawClear(Black);
                drawWalls();
                xSemaphoreGive(ScreenLock);
            }
        }        
    }
}





int tetrisInit(void){
    
    

    DrawSignal = xSemaphoreCreateBinary(); // Screen buffer locking
    if (!DrawSignal) {
        PRINT_ERROR("Failed to create draw signal");
    }

    ScreenLock = xSemaphoreCreateMutex();

    if (!ScreenLock) {
        PRINT_ERROR("Failed to create screen lock");
    }

    xTaskCreate(drawingTask, "drawing", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &drawTask);

    
}