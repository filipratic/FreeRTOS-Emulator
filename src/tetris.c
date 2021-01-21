#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL_scancode.h>
#include <stdbool.h>

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

#define fieldHeight 22
#define fieldWidth 14


TaskHandle_t testTetris = NULL;
TaskHandle_t drawTask = NULL; 
TaskHandle_t gameHandle = NULL;
SemaphoreHandle_t ScreenLock = NULL;
SemaphoreHandle_t DrawSignal = NULL;

int field[fieldHeight + 1][fieldWidth + 2];





void fillField(int map[fieldHeight + 1][fieldWidth + 2]){
    for(int i = 0; i <= fieldHeight; i++){
        for(int j = 1; j <= fieldWidth; j++){
            map[i][j] = 0;
            map[i][0] = 1;
            map[i][fieldWidth + 2] = 1;
            map[fieldHeight + 1][j] = 1;
        }
    }
    map[fieldHeight + 1][fieldWidth + 1] = 1;
    map[fieldHeight + 1][fieldWidth + 2] = 1;
}

void printField(){
    for(int i = 0; i <= 23; i++){
        for(int j = 0; j <= 16; j++){
            printf("%d\t", field[i][j]);
        }
        printf("\n");
    }
}

enum direction {
    down,
    right,
    left
};
    

typedef struct buttons_buffer {
    unsigned char buttons[SDL_NUM_SCANCODES];
    SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };



struct tetromino {
    coord_t coords[4];
    coord_t center;
    unsigned short type;
    unsigned short next;
};

typedef struct tetromino tetromino;
typedef enum direction direction;

void xGetButtonInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
        xSemaphoreGive(buttons.lock);
    }
}

void figureShape(tetromino* figure){
    switch(figure->type){
        case 0:{ //T
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x - 30; 
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x + 30;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x;
            figure->coords[3].y = figure->center.y - 30;
            figure->next = 1;
            break;
        }
        case 1:{//T
            figure->coords[0].x = figure->center.x; 
            figure->coords[0].y = figure->center.y; 
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y + 30;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y - 30;
            figure->coords[3].x = figure->center.x + 30;
            figure->coords[3].y = figure->center.y;
            figure->next = 2;
            break;
        }
        case 2:{//T
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x - 30;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x + 30;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x;
            figure->coords[3].y = figure->center.y - 30;
            figure->next = 3;
            break;
        }
        case 3:{//T
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y - 30;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y + 30;
            figure->coords[3].x = figure->center.x - 30;
            figure->coords[3].y = figure->center.y;
            figure->next = 0;
            break;
        }
        case 4:{//S
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x + 30;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y - 30;
            figure->coords[3].x = figure->center.x - 30;
            figure->coords[3].y = figure->center.y - 30;
            figure->next = 0;
            break;
        }
        case 5:{//S
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y - 30;
            figure->coords[2].x = figure->center.x + 30;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x + 30;
            figure->coords[3].y = figure->center.y + 30;
            figure->next = 6;
            break;
        }
        case 6:{//O
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x - 30;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y + 30;
            figure->coords[3].x = figure->center.x - 30;
            figure->coords[3].y = figure->center.y + 30;
            figure->next = 6;
            break;
        }
        case 7:{//I
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x + 30;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x - 30;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x - 2*30;
            figure->coords[3].y = figure->center.y;
            figure->next = 8;
            break;
        }
        case 8:{//I
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y + 30;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y - 30;
            figure->coords[3].x = figure->center.x;
            figure->coords[3].y = figure->center.y + 2 * 30;
            figure->next = 7;
            break;
        }
        case 9:{//Z
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x - 30;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y + 30;
            figure->coords[3].x = figure->center.x + 30;
            figure->coords[3].y = figure->center.y + 30;
            figure->next = 10;
            break;
        }
        case 10:{//Z
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y - 30;
            figure->coords[2].x = figure->center.x - 30;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x - 30;
            figure->coords[3].y = figure->center.y + 30;
            figure->next = 9;
            break;
        }
        case 11:{//J
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y - 30;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y + 30;
            figure->coords[3].x = figure->center.x - 30;
            figure->coords[3].y = figure->center.y + 30;
            figure->next = 12;
            break;
        }
        case 12:{//J
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x + 30;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x - 30;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x - 30;
            figure->coords[3].y = figure->center.y - 30;
            figure->next = 13;
            break;
        }
        case 13:{//J
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y + 30;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y - 30;
            figure->coords[3].x = figure->center.x + 30;
            figure->coords[3].y = figure->center.y - 30;
            figure->next = 14;
            break;
        }
        case 14:{//J
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x - 30;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x + 30;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x + 30;
            figure->coords[3].y = figure->center.y + 30;
            figure->next = 11;
            break;
        }
        case 15:{//L
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y - 30;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y + 30;
            figure->coords[3].x = figure->center.x + 30;
            figure->coords[3].y = figure->center.y + 30;
            figure->next = 16;
            break;
        }
        case 16:{//L
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x + 30;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x - 30;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x - 30;
            figure->coords[3].y = figure->center.y + 30;
            figure->next = 17;
            break;
        }
        case 17:{//L
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y + 30;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y - 30;
            figure->coords[3].x = figure->center.x - 30;
            figure->coords[3].y = figure->center.y - 30;
            figure->next = 18;
            break;
        }
        case 18:{//L
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x - 30;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x + 30;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x + 30;
            figure->coords[3].y = figure->center.y - 30;
            figure->next = 15;
            break;
        }
    }
}


void makeFigure(tetromino* figure){
    figure->center.x = 6*30;
    figure->center.y = 2*30;
    figure->type = rand() % 19;
    figureShape(figure);
}

void drawFigure(tetromino* figure){
    tumDrawFilledBox(figure->coords[0].x, figure->coords[0].y, 30, 30, Blue);
    tumDrawFilledBox(figure->coords[1].x, figure->coords[1].y, 30, 30, Blue);
    tumDrawFilledBox(figure->coords[2].x, figure->coords[2].y, 30, 30, Blue);
    tumDrawFilledBox(figure->coords[3].x, figure->coords[3].y, 30, 30, Blue);
}

void rotateFigure(tetromino* figure){
    
}

void moveFigures(tetromino* figure, int map[fieldHeight][fieldWidth], direction way){
    switch(way){
        case down:
            figure->center.y += 30;
            break;
        case left:
            figure->center.x -= 30;
            break;
        case right:
            figure->center.x += 30;
            break;
    }
    figureShape(figure);
}

void game(void* pvParameters){
    tetromino tetris;
    tetromino* tetrisp = &tetris;
    bool pressed_down = false, pressed_right = false, pressed_left = false;
    makeFigure(tetrisp);


    while(1){
        xGetButtonInput();
        drawFigure(tetrisp);
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
            if(buttons.buttons[SDL_SCANCODE_DOWN]){
                if(!pressed_down){
                    pressed_down = true;
                    moveFigures(tetrisp, field[fieldHeight][fieldWidth], down);
                    printf("Test\n");
                }
            }   else {
                    pressed_down = false;
                }
            xSemaphoreGive(buttons.lock);
        }
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
           if(buttons.buttons[SDL_SCANCODE_RIGHT]){
               if(!pressed_right){
                    pressed_right = true;
                    moveFigures(tetrisp, field[fieldHeight][fieldWidth], right);
               }    
           } else {
               pressed_right = false;
           }
           xSemaphoreGive(buttons.lock);
        }
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
            if(buttons.buttons[SDL_SCANCODE_LEFT]){
                if(!pressed_left){
                    pressed_left = true;
                    moveFigures(tetrisp, field[fieldHeight][fieldWidth], left);
                }
                
            } else {
                pressed_left = false;
            }
            xSemaphoreGive(buttons.lock);
        }
        vTaskDelay(10);   
    }
}












void drawWalls(){
    tumDrawFilledBox(0, 0, 30, 700, Gray);
    tumDrawFilledBox(0, 660, 450, 30, Gray);
    tumDrawFilledBox(450, 0, 30, 700, Gray);

    
    
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
        vTaskDelay(pdMS_TO_TICKS(20));        
    }
}





int tetrisMain(void){
    

    fillField(field);
    printField();

    DrawSignal = xSemaphoreCreateBinary(); // Screen buffer locking
    if (!DrawSignal) {
        PRINT_ERROR("Failed to create draw signal");
    }

    ScreenLock = xSemaphoreCreateMutex();

    if (!ScreenLock) {
        PRINT_ERROR("Failed to create screen lock");
    }


    buttons.lock = xSemaphoreCreateMutex();
    if(!buttons.lock){
        PRINT_ERROR("Failed to create buttons lock");
    }


    xTaskCreate(drawingTask, "drawing", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &drawTask);
    xTaskCreate(game, "game", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &gameHandle);
    
}