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


TaskHandle_t moveTetris = NULL;
TaskHandle_t drawTask = NULL; 
TaskHandle_t gameHandle = NULL;

SemaphoreHandle_t ScreenLock = NULL;
SemaphoreHandle_t DrawSignal = NULL;
SemaphoreHandle_t LevelLock = NULL;


SemaphoreHandle_t lockTetris = NULL; 

int field[fieldHeight][fieldWidth];

unsigned int color[] = {Red, Blue, Green, Yellow, Aqua, Fuchsia, White, Gray};
int level = 1;






void fillField(int map[fieldHeight][fieldWidth]){
    for(int i = 0; i < fieldHeight; i++){
        for(int j = 0; j < fieldWidth; j++){
            map[i][j] = 0;
        }
    }
    
}

void printField(){
    for(int i = 0; i < 22; i++){
        for(int j = 0; j < 14; j++){
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
    bool isMoving;
    unsigned int color;
};

typedef struct tetromino tetromino;
typedef enum direction direction;

tetromino tetrisCurrent;
tetromino tetrisNext;
tetromino* tetrisC = &tetrisCurrent;
tetromino* tetrisN = &tetrisNext;

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
            figure->coords[1].x = figure->center.x + 30;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x - 30;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x;
            figure->coords[3].y = figure->center.y + 30;
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
            figure->next = 5;
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
            figure->next = 4;
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
    figure->color = color[rand() %8];
}

void drawFigure(tetromino* figure){
    tumDrawFilledBox(figure->coords[0].x, figure->coords[0].y, 30, 30, figure->color);
    tumDrawFilledBox(figure->coords[1].x, figure->coords[1].y, 30, 30, figure->color);
    tumDrawFilledBox(figure->coords[2].x, figure->coords[2].y, 30, 30, figure->color);
    tumDrawFilledBox(figure->coords[3].x, figure->coords[3].y, 30, 30, figure->color);
}

int leftCoord(tetromino* figure){
    int min = 500;
    for(int i = 0; i < 4; i++){
        if(figure->coords[i].x < min) min = figure->coords[i].x;
    }
    return min;
}

int rightCoord(tetromino* figure){
    int max = 0;
    for(int i = 0; i < 4; i++){
        if(figure->coords[i].x > max) max = figure->coords[i].x;
    }
    return max;
}

int lowestCoord(tetromino* figure){
    int max = 0;
    for(int i = 0; i < 4; i++){
        if(figure->coords[i].y > max) max = figure->coords[i].y;
    }
    return max;
}

bool detectCollisionDown(tetromino* figure){
    for(int i = 0; i < 4; i++){
        if(field[figure->coords[i].y / 30 + 1][figure->coords[i].x / 30 - 1]) return true;
    }
    return false;
}

bool detectCollisionRight(tetromino* figure){
    for(int i = 0; i < 4; i++){
        if(field[figure->coords[i].y / 30][figure->coords[i].x / 30]) return true;
    }
    return false;
}

bool detectCollisionLeft(tetromino* figure){
    for(int i = 0; i < 4; i++)
        if(field[figure->coords[i].y / 30][figure->coords[i].x / 30 - 2]) return true;
    return false;
}
void setIndexToOne(tetromino* figure){
    for(int i = 0; i < 4; i++){
        field[figure->coords[i].y / 30][figure->coords[i].x / 30 - 1] = 1;
    }
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



void deleteFilledRow();

void game(void* pvParameters){

    bool pressed_down = false, pressed_right = false, pressed_left = false, pressed_rotate = false;
    makeFigure(tetrisC);
    makeFigure(tetrisN);
    tetrisC->isMoving = true;

    while(1){
        xGetButtonInput();
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE && xSemaphoreTake(lockTetris, 0) == pdTRUE){
            if(buttons.buttons[SDL_SCANCODE_DOWN]){
                if(lowestCoord(tetrisC) < 630){
                    if(!detectCollisionDown(tetrisC)){
                        if(!pressed_down){
                            pressed_down = true;
                            moveFigures(tetrisC, field[fieldHeight][fieldWidth], down);
                        }
                    } else {
                        setIndexToOne(tetrisC);
                        tetrisC->isMoving = false;
                    }
                    
                } else {
                    setIndexToOne(tetrisC);
                    printField();
                    tetrisC->isMoving = false;
                }
            }   
                else pressed_down = false;
            xSemaphoreGive(lockTetris);
            xSemaphoreGive(buttons.lock);
        }
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE && xSemaphoreTake(lockTetris, 0) == pdTRUE){
            
            if(buttons.buttons[SDL_SCANCODE_RIGHT]){
                if(rightCoord(tetrisC) < 420){
                    if(!detectCollisionRight(tetrisC)){
                        if(!pressed_right){
                        pressed_right = true;
                        moveFigures(tetrisC, field[fieldHeight][fieldWidth], right);
                    }
                    }
                }    
            } else pressed_right = false;
            

            xSemaphoreGive(lockTetris);
            xSemaphoreGive(buttons.lock);
        }
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE && xSemaphoreTake(lockTetris, 0) == pdTRUE){
                if(leftCoord(tetrisC) > 30){
                    if(buttons.buttons[SDL_SCANCODE_LEFT]){
                        if(!detectCollisionLeft(tetrisC)){
                            if(!pressed_left){
                                pressed_left = true;
                                moveFigures(tetrisC, field[fieldHeight][fieldWidth], left);
                            }
                        }
                        } else {
                            pressed_left = false;
                        }
                }
            
            xSemaphoreGive(lockTetris);    
            xSemaphoreGive(buttons.lock);
        }
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE && xSemaphoreTake(lockTetris, 0) == pdTRUE){
            if(buttons.buttons[SDL_SCANCODE_R]){
                if(!pressed_rotate){
                    unsigned short temp = tetrisC->type;
                    tetrisC->type = tetrisC->next;
                    pressed_rotate = true;
                    figureShape(tetrisC);
                    if(detectCollisionLeft(tetrisC) || detectCollisionRight(tetrisC) || detectCollisionDown(tetrisC) || leftCoord(tetrisC) < 30 || rightCoord(tetrisC) > 420){
                        tetrisC->type = temp;
                        figureShape(tetrisC);

                    }
                }   
            } else pressed_rotate = false;
            xSemaphoreGive(lockTetris);
            xSemaphoreGive(buttons.lock);
        }
        if(!tetrisC->isMoving) {
            tetrisC = tetrisN;
            makeFigure(tetrisN);
            tetrisC->isMoving = true;
        }
        
        deleteFilledRow(getTopRow());
        vTaskDelay(pdMS_TO_TICKS(50));   
    }
}

int getTopRow(){
    for(int i = 0; i < fieldHeight; i++){
        for(int j = 0; j < fieldWidth; j++){
            if(field[i][j] == 1) return i;
        }
    }
}

void deleteFilledRow(int top){
    int cnt = 0;
    bool cleared = false;
    for(int i = 0; i < fieldHeight; i++){
        for(int j = 0; j < fieldWidth; j++){
            if(field[i][j]) cnt++; 
        }
        if(cnt == 14) {
            int k = 0;
            while(k < 14) {
                field[i][k] = 0;
                k++;
            }
            cleared = true;
            cnt = 0;
        }
        else {
            cnt = 0;
            cleared = false;
        }
        if(cleared){
            for(int g = i; g >= 0; g--){
                for(int j = fieldWidth - 1; j >= 0; j--){
                    field[i][j] = field[i - 1][j];
                }
            }
            int g = 0;
            while(g < fieldWidth){
                field[top][g] = 0;
                g++;
            }
            
        }
    }
    

}

void drawWalls(){
    tumDrawFilledBox(0, 0, 30, 700, Gray);
    tumDrawFilledBox(0, 660, 450, 30, Gray);
    tumDrawFilledBox(450, 0, 30, 700, Gray);
}

void moveTetrisTask(void *pvParameters){
    TickType_t LastTick;
    LastTick = xTaskGetTickCount();
    while(1){
        if(xSemaphoreTake(lockTetris, 0) == pdTRUE){
            if(lowestCoord(tetrisC) < 630 && !detectCollisionDown(tetrisC)){
                tetrisC->center.y += 30;
                figureShape(tetrisC);
            }
            else {
                setIndexToOne(tetrisC);
                tetrisC->isMoving = false; 
            }
            xSemaphoreGive(lockTetris);    
        } 

        vTaskDelayUntil(&LastTick, pdMS_TO_TICKS(400));

            
    }

        
}



void drawingTask(void * pvParameters){
    while(1){
        if(DrawSignal){
            if(xSemaphoreTake(DrawSignal, portMAX_DELAY) == pdTRUE){
                xSemaphoreTake(ScreenLock, portMAX_DELAY);
                tumDrawClear(Black);
                drawWalls();
                for(int col = 0; col < fieldHeight; col++){
                    for(int row = 0; row < fieldWidth; row++){
                        if(field[col][row]) tumDrawFilledBox((row + 1)* 30, col*30, 30, 30, Blue);
                    }
                }
                drawFigure(tetrisC);
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

    lockTetris = xSemaphoreCreateMutex();

    if(!lockTetris){
        PRINT_ERROR("Failed to create tetris lock");
    }

    buttons.lock = xSemaphoreCreateMutex();
    
    if(!buttons.lock){
        PRINT_ERROR("Failed to create buttons lock");
    }


    xTaskCreate(drawingTask, "drawing", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &drawTask);
    xTaskCreate(game, "game", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &gameHandle);
    xTaskCreate(moveTetrisTask, "moveTetris", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &moveTetris);
}