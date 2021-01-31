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

#define UDP_RECEIVE_PORT 1234
#define UDP_TRANSMIT_PORT 1235
#define IPv4_addr "127.0.0.1"




TaskHandle_t moveTetris = NULL;
TaskHandle_t drawTask = NULL; 
TaskHandle_t gameHandle = NULL;
TaskHandle_t menuHandle = NULL;
TaskHandle_t stateMachineHandle = NULL;
TaskHandle_t demoTaskHandle = NULL;


SemaphoreHandle_t ScreenLock = NULL;
SemaphoreHandle_t DrawSignal = NULL;
SemaphoreHandle_t LevelLock = NULL;
SemaphoreHandle_t lockTetris = NULL; 
SemaphoreHandle_t lockScore = NULL;
SemaphoreHandle_t lockLines = NULL; 

aIO_handle_t receiveHandle = NULL;
aIO_handle_t sendHandle = NULL; 

int field[fieldHeight][fieldWidth];

unsigned int color[] = {Red, Blue, Green, Yellow, Aqua, Fuchsia, White, Gray};
int level = 1;
int score = 0;
int lines = 0;
char scorestring[7];
char linesString[7];
char levelString[7];







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
    
enum currentState {
    mainMenu,
    single,
    multi
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
typedef enum currentState currentState;

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

void EmulatorRecv(size_t, char*, void*);

void figureShape(tetromino*);

int highestCoord(tetromino*);

bool checkPosition(tetromino*);

void makeFigure(tetromino*);

void drawFigure(tetromino*);

int leftCoord(tetromino*);

int rightCoord(tetromino*);

int lowestCoord(tetromino*);

int highestCoord(tetromino*);

bool detectCollisionDown(tetromino*);

bool detectCollisionRight(tetromino*);

bool detectCollisionLeft(tetromino*);

void setIndexToOne(tetromino*);

void moveFigures(tetromino*, direction);

int getTopRow();

void deleteRowsAndTrackScore();

void copyFigure(tetromino*, tetromino*);


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

bool checkPosition(tetromino* figure){
    for(int i = 0; i < 4; i++){
        if(figure->coords[i].y == 30) return true;
    }
    return false;
}

void makeFigure(tetromino* figure){
    figure->center.x = 6*30;
    figure->center.y = 2*30;
    figure->type = rand() % 19;
    figureShape(figure);
    figure->color = color[rand() % 8];
}

void drawFigure(tetromino* figure){
    checkDraw(tumDrawFilledBox(figure->coords[0].x, figure->coords[0].y, 30, 30, figure->color), __FUNCTION__);
    checkDraw(tumDrawFilledBox(figure->coords[1].x, figure->coords[1].y, 30, 30, figure->color), __FUNCTION__);
    checkDraw(tumDrawFilledBox(figure->coords[2].x, figure->coords[2].y, 30, 30, figure->color), __FUNCTION__);
    checkDraw(tumDrawFilledBox(figure->coords[3].x, figure->coords[3].y, 30, 30, figure->color), __FUNCTION__);
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

int highestCoord(tetromino* figure){
    int max = 1000;
    for(int i = 0; i < 4; i++){
        if(figure->coords[i].x < max) max = figure->coords[i].x;
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

void moveFigures(tetromino* figure, direction way){
    
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

void copyFigure(tetromino* figureC, tetromino* figureN){
    figureC->center.x = figureN->center.x;
    figureC->center.y = figureN->center.y;
    figureC->type = figureN->type;
    figureC->next = figureN->next;
    figureC->color = figureN->color;
    figureShape(figureC);
}

void game(void* pvParameters){
    bool pressed_down = false, pressed_right = false, pressed_left = false, pressed_rotate = false, pressed_p = false, pressed_m = false;
    vTaskSuspend(NULL);
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
                            moveFigures(tetrisC, down);
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
                        moveFigures(tetrisC, right);
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
                                moveFigures(tetrisC, left);
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
            if(!checkPosition(tetrisC)){
                copyFigure(tetrisC, tetrisN);
                makeFigure(tetrisN);
                tetrisC->isMoving = true;
            }
            else {
                printf("Game Over\n");
            }
            
        }
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE && xSemaphoreTake(LevelLock, 0) == pdTRUE) {
            if(buttons.buttons[SDL_SCANCODE_P] && level < 10) {
                if(!pressed_p){
                    level++;
                    pressed_p = true;
                }
            }else pressed_p = false;
            if(buttons.buttons[SDL_SCANCODE_M] && level > 1) {
                if(!pressed_m){
                    level--;
                    pressed_m = true;
                }
            } else pressed_m = false;
            xSemaphoreGive(buttons.lock);
            xSemaphoreGive(LevelLock);
        }
        
        deleteRowsAndTrackScore();
        vTaskDelay(pdMS_TO_TICKS(50));   
    }
}

void deleteRowsAndTrackScore(){
    bool flag;
    int full = 0, first;
    for(int i = fieldHeight - 1; i >= 0; i--){
        flag = true;
        for(int j = 0; j < fieldWidth; j++){
            if(!field[i][j]) {
                flag = false;
                break;
            }
        }
        if(flag) {
            full++;
            first = i;
        }
    }
    if(full > 0){
        for(int i = first; i < first + full; i++){
            for(int j = 0; j < fieldWidth; j++){
                field[i][j] = 0;
            }
        }
        for(int i = first - 1; i >= 0; i--){
            for(int j = 0; j < fieldWidth; j++){
                field[i + full][j] = field[i][j];
            }
        }
        if(xSemaphoreTake(lockScore, 0) == pdTRUE && xSemaphoreTake(LevelLock, 0) == pdTRUE){
            if(full == 1) score += level * 40;      
            else if(full == 2) score += level * 100;
            else if(full == 3) score += level * 300;
            else if(full == 4) score += level * 1200;
            lines += full;
            xSemaphoreGive(lockScore);
            xSemaphoreGive(LevelLock);
        }
        
                          
    }


}

void drawWalls(){
    tumDrawFilledBox(0, 0, 30, 700, Gray);
    tumDrawFilledBox(0, 660, 450, 30, Gray);
    tumDrawFilledBox(450, 0, 30, 700, Gray);
}

void clearMap(){
    for(int i = 0; i < fieldHeight; i++){
        for(int j = 0; j < fieldWidth; j++){
            field[i][j] = 0;
        }
    }
}


void drawGameMenuTask(void * pvParameters){
    while(1){
        if(DrawSignal){
            if(xSemaphoreTake(DrawSignal, portMAX_DELAY) == pdTRUE){
                if(xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE){
                    checkDraw(tumDrawClear(Black), __FUNCTION__);
                    checkDraw(tumDrawText("Welcome to Tetris!", 200, 200, Red), __FUNCTION__);
                    checkDraw(tumDrawText("Pick a game mode:", 200, 250, White), __FUNCTION__);
                    checkDraw(tumDrawText("S - Single player", 200, 300, White), __FUNCTION__);
                    checkDraw(tumDrawText("D - Multiplayer", 200, 350, White), __FUNCTION__);
                    checkDraw(tumDrawText("A - Main Menu", 200, 400, White), __FUNCTION__);

                    xSemaphoreGive(ScreenLock);
                }
            }
        }
        
    }
}

void stateMachineTask(void * pvParameters){
    bool pressed_a = false, pressed_s = false, pressed_d = false;
    currentState state;
    
    while(1){
        xGetButtonInput();
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
            if(buttons.buttons[SDL_SCANCODE_A]){
                if(!pressed_a){
                    state = mainMenu;
                    pressed_a = true;
                }
            } else pressed_a = false;
            xSemaphoreGive(buttons.lock);
        }
        if(xSemaphoreTake(buttons.lock,0) == pdTRUE){
            if(buttons.buttons[SDL_SCANCODE_S]){
                if(!pressed_s){
                    state = single;
                    pressed_s = true;
                    printf("Test\n");
                }
            } else pressed_s = false;
            xSemaphoreGive(buttons.lock);
        }
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
            if(buttons.buttons[SDL_SCANCODE_D]){
                if(!pressed_d){
                    state = multi;
                    pressed_d = true;
                }
            } else pressed_d = false;
            xSemaphoreGive(buttons.lock);
        }       
            
        switch (state)
        {
        case mainMenu:{
            vTaskSuspend(gameHandle);
            vTaskSuspend(moveTetris);
            vTaskSuspend(drawTask);
            vTaskResume(menuHandle);
            break;
        }
        
        case single:{
            vTaskSuspend(menuHandle);
            vTaskResume(gameHandle);
            vTaskResume(drawTask);
            vTaskResume(moveTetris);
            break;
        }
        case multi:{
            vTaskSuspend(gameHandle);
            vTaskSuspend(moveTetris);
            vTaskSuspend(drawTask);
            vTaskSuspend(menuHandle);
            break;
        }
        default:
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


void EmulatorSend(size_t recv_size, char* buffer, void* args){
    char test[5];
    strcpy(test, buffer);
    printf("%s\n", test);
}

void EmulatorRecv(size_t recv_size, char* buffer, void* args){
    char recv_val[10];
    strcpy(recv_val, buffer);

    printf("%s\n", recv_val);
}


void vDemoTask(void * pvParameters){
    //receiveHandle = aIOOpenUDPSocket(IPv4_addr, UDP_RECEIVE_PORT, 5 * sizeof(char), EmulatorRecv, NULL);
    //aIOSocketPut(UDP, IPv4_addr, MOSI_PORT, (char *)&random_value,
				// sizeof(random_value))
    sendHandle = aIOOpenUDPSocket(IPv4_addr, UDP_TRANSMIT_PORT, 6*sizeof(char), EmulatorSend, NULL);
    receiveHandle = aIOOpenUDPSocket(IPv4_addr, UDP_RECEIVE_PORT, 6 * sizeof(char), EmulatorRecv, NULL);
    if(sendHandle == NULL){
        PRINT_ERROR("Failed to open send socket");
        exit(EXIT_FAILURE);
    }

    if(receiveHandle == NULL){
        PRINT_ERROR("Failed to open receive socket");
        exit(EXIT_FAILURE);
    }

    while(1){
        aIOSocketPut(UDP, IPv4_addr, UDP_TRANSMIT_PORT, "MODE", 6*sizeof(char));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}



void moveTetrisTask(void *pvParameters){
    vTaskSuspend(NULL);
    while(1){
        if(xSemaphoreTake(lockTetris, 0) == pdTRUE){
            if(lowestCoord(tetrisC) < 630 && !detectCollisionDown(tetrisC) && tetrisC->isMoving){
                tetrisC->center.y += 30;
                figureShape(tetrisC);
            }
            else {
                setIndexToOne(tetrisC);
                tetrisC->isMoving = false; 
            }
            xSemaphoreGive(lockTetris);    
        } 

        vTaskDelay(pdMS_TO_TICKS(1000)/level);

            
    }   
}



void drawingTask(void * pvParameters){
    vTaskSuspend(NULL);
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
                sprintf(scorestring, "Score: %d", score);
                checkDraw(tumDrawText(scorestring, 530, 100, Red), __FUNCTION__);
                sprintf(levelString,"Level: %d", level);
                checkDraw(tumDrawText(levelString, 530, 170, White), __FUNCTION__);
                sprintf(linesString, "Lines: %d", lines);
                checkDraw(tumDrawText(linesString, 530, 250, White), __FUNCTION__);
                
                tumDrawFilledBox(tetrisN->coords[0].x + 520, tetrisN->coords[0].y + 300, 30, 30, tetrisN->color);
                tumDrawFilledBox(tetrisN->coords[1].x + 520, tetrisN->coords[1].y + 300, 30, 30, tetrisN->color);
                tumDrawFilledBox(tetrisN->coords[2].x + 520, tetrisN->coords[2].y + 300, 30, 30, tetrisN->color);
                tumDrawFilledBox(tetrisN->coords[3].x + 520, tetrisN->coords[3].y + 300, 30, 30, tetrisN->color);
                vDrawFPS();
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

    lockScore = xSemaphoreCreateMutex();

    if(!lockScore){
        PRINT_ERROR("Failed to create score lock");
    }

    LevelLock = xSemaphoreCreateMutex();

    if(!LevelLock){
        PRINT_ERROR("Failed to create level lock");
    }

    lockLines = xSemaphoreCreateMutex();

    if(!lockLines){
        PRINT_ERROR("Failed to create lines lock");
    }

    buttons.lock = xSemaphoreCreateMutex();
    
    if(!buttons.lock){
        PRINT_ERROR("Failed to create buttons lock");
    }
    /*xTaskCreate(drawingTask, "drawing", mainGENERIC_STACK_SIZE, NULL, 1, &drawTask);
    xTaskCreate(game, "game", mainGENERIC_STACK_SIZE, NULL, 2, &gameHandle);
    xTaskCreate(moveTetrisTask, "moveTetris", mainGENERIC_STACK_SIZE, NULL, 1, &moveTetris);
    xTaskCreate(drawGameMenuTask, "mainMenu", mainGENERIC_STACK_SIZE, NULL, 1, &menuHandle);
    xTaskCreate(stateMachineTask, "StateMachine", mainGENERIC_STACK_SIZE, NULL, 3, &stateMachineHandle);
*/
    xTaskCreate(vDemoTask, "demo", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &demoTaskHandle);


    return 0;

}