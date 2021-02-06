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
#define UDP_BUFFER_SIZE 2000




TaskHandle_t moveTetris = NULL;
TaskHandle_t drawTask = NULL; 
TaskHandle_t singleGameHandle = NULL;
TaskHandle_t menuHandle = NULL;
TaskHandle_t stateMachineHandle = NULL;
TaskHandle_t demoTaskHandle = NULL;
TaskHandle_t multiGameHandle = NULL;


SemaphoreHandle_t ScreenLock = NULL;
SemaphoreHandle_t DrawSignal = NULL;
SemaphoreHandle_t lockTetris = NULL; 
SemaphoreHandle_t lockStats = NULL; 
SemaphoreHandle_t lockMode = NULL;
SemaphoreHandle_t lockShape = NULL;
SemaphoreHandle_t HandleUDP = NULL;
SemaphoreHandle_t lockType = NULL;



aIO_handle_t receiveHandle = NULL;



static QueueHandle_t ModeQueue = NULL;
static QueueHandle_t NextQueue = NULL;



int field[fieldHeight][fieldWidth];

unsigned int color[] = {Red, Blue, Green, Yellow, Aqua, Fuchsia, White, Gray};

int level = 1;
int score = 0;
int lines = 0;
char scorestring[7];
char linesString[7];
char levelString[7];
char mode[5];
char shape;
int gameType;










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



enum modes {
    FAIR = 0,
    RANDOM = 1,
    EASY = 2,
    HARD = 3,
    OK = 4
};

typedef enum shapes {
    T = 0,
    S = 1,
    O = 2,
    I = 3,
    Z = 4,
    J = 5,
    L = 6
}shapes;



typedef enum modes modes;



typedef struct buttons_buffer {
    unsigned char buttons[SDL_NUM_SCANCODES];
    SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };

typedef struct booleans{
    bool value;
    SemaphoreHandle_t lock;
} booleans;

static booleans bools;

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

void restart(bool);

void clearMap();

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
        default:
            break;
    }
}

bool checkGameOver(tetromino* figure){
    for(int i = 0; i < 4; i++){
        if(figure->coords[i].y == 30) return true;
    }
    return false;
}

void makeFigure(tetromino* figure){
    figure->center.x = 6*30;
    figure->center.y = 2*30;
    if(gameType == 1){
        figure->type = rand() % 19;
    }
    else{
        if(shape == 'T') figure->type = rand() % 4;
        else if(shape == 'S') figure->type = rand() % 2 + 4;
        else if(shape == 'O') figure->type = 6;
        else if(shape == 'I') figure->type = rand() % 2 + 7; 
        else if(shape == 'Z') figure->type = rand() % 2 + 9;
        else if(shape == 'J') figure->type = rand() % 4 + 11;
        else if(shape == 'L') figure->type = rand() % 4 + 15;
    }
    figure->color = color[rand() % 8];
    figureShape(figure);
    
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

void singleGame(void* pvParameters){
    bool pressed_down = false, pressed_right = false, pressed_left = false, pressed_rotate = false, pressed_p = false, pressed_m = false, pressed_restart = false, pressed_pause = false;
    bools.value = false;
    vTaskSuspend(NULL);
    if(xSemaphoreTake(lockTetris, 0) == pdTRUE){
        makeFigure(tetrisC);
        makeFigure(tetrisN);
        tetrisC->isMoving = true;
        xSemaphoreGive(lockTetris);
    }

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
            if(buttons.buttons[SDL_SCANCODE_C]){
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
            if(!checkGameOver(tetrisC)){
                copyFigure(tetrisC, tetrisN);
                makeFigure(tetrisN);
                tetrisC->isMoving = true;
            }
            else {
                printf("Game Over\n");
            }
            
        }
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE && xSemaphoreTake(lockStats, 0) == pdTRUE) {
            if(buttons.buttons[SDL_SCANCODE_M] && level < 10) {
                if(!pressed_p){
                    level++;
                    pressed_p = true;
                }
            }else pressed_p = false;
            if(buttons.buttons[SDL_SCANCODE_N] && level > 1) {
                if(!pressed_m){
                    level--;
                    pressed_m = true;
                }
            } else pressed_m = false;
            xSemaphoreGive(buttons.lock);
            xSemaphoreGive(lockStats);
        }
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
            if(buttons.buttons[SDL_SCANCODE_R]){
                if(!pressed_restart){
                    clearMap();
                    if(xSemaphoreTake(lockTetris, 0) == pdTRUE){
                        makeFigure(tetrisC);
                        makeFigure(tetrisN);
                        xSemaphoreGive(lockTetris);
                    }
                    pressed_restart = true;
                }
            } else pressed_restart = false;
        xSemaphoreGive(buttons.lock);
        }
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
            if(buttons.buttons[SDL_SCANCODE_P]){
                if(!pressed_pause){
                    if(xSemaphoreTake(bools.lock, 0) == pdTRUE){
                        if(!bools.value) {
                            vTaskSuspend(moveTetris);
                            bools.value = true;
                            printf("Paused\n");
                        }
                        else { 
                            vTaskResume(moveTetris);
                            bools.value = false;
                            printf("Unpaused\n");
                        }
                        xSemaphoreGive(bools.lock);
                    }
                     
                    pressed_pause = true;
                    }
                    
                } else pressed_pause = false;
            xSemaphoreGive(buttons.lock);
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
        if(xSemaphoreTake(lockStats, 0) == pdTRUE){
            if(full == 1) score += level * 40;      
            else if(full == 2) score += level * 100;
            else if(full == 3) score += level * 300;
            else if(full == 4) score += level * 1200;
            lines += full;
            xSemaphoreGive(lockStats);
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
    currentState state = mainMenu;
    while(1){
        xGetButtonInput();
        if(state != mainMenu){
            if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
                if(buttons.buttons[SDL_SCANCODE_A]){
                    if(!pressed_a){
                        state = mainMenu;
                        pressed_a = true;
                    }
                } else pressed_a = false;
                xSemaphoreGive(buttons.lock);
            }
        }
        if(state != single){
            if(xSemaphoreTake(buttons.lock,0) == pdTRUE){
                if(buttons.buttons[SDL_SCANCODE_S]){
                    if(!pressed_s){
                        state = single;
                        clearMap();
                        if(xSemaphoreTake(lockTetris, 0) == pdTRUE){
                            makeFigure(tetrisC);
                            makeFigure(tetrisN);
                            xSemaphoreGive(lockTetris);
                        }
                        pressed_s = true;
                    }
                } else pressed_s = false;
                xSemaphoreGive(buttons.lock);
            }
        }
        if(state != multi){
            if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
                if(buttons.buttons[SDL_SCANCODE_D]){
                    if(!pressed_d){
                        state = multi;
                        clearMap();
                        if(xSemaphoreTake(lockTetris, 0) == pdTRUE){
                            makeFigure(tetrisC);
                            makeFigure(tetrisN);
                            xSemaphoreGive(lockTetris);
                        }
                        pressed_d = true;
                    }
                } else pressed_d = false;
                xSemaphoreGive(buttons.lock);
            }
        }          
        switch (state)
        {
        case mainMenu:{
            vTaskSuspend(singleGameHandle);
            vTaskSuspend(moveTetris);
            vTaskSuspend(drawTask);
            vTaskResume(menuHandle);
            clearMap();
            break;
        }
        
        case single:{
            
            if(xSemaphoreTake(lockType,0) == pdTRUE){
                gameType = 1;
                xSemaphoreGive(lockType);
            }
            vTaskSuspend(menuHandle);
            vTaskResume(singleGameHandle);
            vTaskResume(drawTask);
            if(xSemaphoreTake(bools.lock, 0) == pdTRUE){
                if(!bools.value) vTaskResume(moveTetris);
                xSemaphoreGive(bools.lock);
            }
            
            
            break;
        }
        case multi:{
            
            if(xSemaphoreTake(lockType,0) == pdTRUE){
                gameType = 2;
                xSemaphoreGive(lockType);
            }
            vTaskSuspend(menuHandle);
            vTaskResume(singleGameHandle);
            vTaskResume(drawTask);
            vTaskResume(moveTetris);
            
            break;
        }
        default:
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}







void UDPHandler(size_t read_size, char *buffer, void *args)
{
    modes next_mode;
    shapes next_shape;
    BaseType_t xHigherPriorityTaskWoken1 = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken2 = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken3 = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken4 = pdFALSE;

    if (xSemaphoreTakeFromISR(HandleUDP, &xHigherPriorityTaskWoken1) ==
        pdTRUE) {

        char send_command = 0;
        if (strncmp(buffer, "MODE=FAIR", (read_size < 10) ? read_size : 11) ==
            0) {
            next_mode = FAIR;
            send_command = 1;
        }
        else if (strncmp(buffer, "MODE=RANDOM",
                         (read_size < 11) ? read_size : 11) == 0) {
            next_mode = RANDOM;
            send_command = 1;
        }
        else if (strncmp(buffer, "MODE=EASY",
                         (read_size < 10) ? read_size : 10) == 0) {
            next_mode = EASY;
            send_command = 1;
        }
        else if (strncmp(buffer, "MODE=HARD",
                         (read_size < 10) ? read_size : 10) == 0) {
            next_mode = HARD;
            send_command = 1;
        }
        else if(strncmp(buffer, "MODE=OK", (read_size < 8) ? read_size : 8) == 0){
            next_mode = OK;
            send_command = 1;
        }


        if (ModeQueue && send_command) {
            xQueueSendFromISR(ModeQueue, (void *)&next_mode,
                              &xHigherPriorityTaskWoken2);
        }
        
        send_command = 0;

        if (strncmp(buffer, "NEXT=T", (read_size < 7) ? read_size : 7) ==
            0) {
            next_shape = T;
            send_command = 1;
        }
        else if (strncmp(buffer, "NEXT=S",
                         (read_size < 7) ? read_size : 7) == 0) {
            next_shape = S;
            send_command = 1;
        }
        else if (strncmp(buffer, "NEXT=O",
                         (read_size < 7) ? read_size : 7) == 0) {
            next_shape = O;
            send_command = 1;
        }
        else if (strncmp(buffer, "NEXT=I",
                         (read_size < 7) ? read_size : 7) == 0) {
            next_shape = I;
            send_command = 1;
        }
        else if(strncmp(buffer, "NEXT=Z", (read_size < 7) ? read_size : 7) == 0){
            next_shape = Z;
            send_command = 1;
        }
        else if(strncmp(buffer, "NEXT=J", (read_size < 7) ? read_size : 7) == 0){
            next_shape = J;
            send_command = 1;
        }
        else if(strncmp(buffer, "NEXT=L", (read_size < 7) ? read_size : 7) == 0){
            next_shape = L;
            send_command = 1;
        }
        
        if (NextQueue && send_command) {
            xQueueSendFromISR(NextQueue, (void *)&next_shape,
                              &xHigherPriorityTaskWoken3);
        }
        
        xSemaphoreGiveFromISR(HandleUDP, &xHigherPriorityTaskWoken4);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken1 |
                           xHigherPriorityTaskWoken2 |
                           xHigherPriorityTaskWoken3 |
                           xHigherPriorityTaskWoken4);
    }
    else {
        fprintf(stderr, "[ERROR] Overlapping UDPHandler call\n");
    }
}

unsigned char xCheckTetrisUDPInput();

void vUDPControlTask(void *pvParameters)
{
    static char buf[20];
    bool flag = false;
    char *addr = NULL; 
    in_port_t port = UDP_RECEIVE_PORT;

    receiveHandle =
        aIOOpenUDPSocket(addr, port, UDP_BUFFER_SIZE, UDPHandler, NULL);

    printf("UDP socket opened on port %d\n", port);

    while (1) {
        
        xCheckTetrisUDPInput();

        aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, buf, strlen(buf));
        
        flag = true;
        
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

unsigned char xCheckTetrisUDPInput()
{
    static modes current_mode;
    static shapes current_shape;

    if (ModeQueue) {
        xQueueReceive(ModeQueue, &current_mode, 0);
        
    }
    //printf("%d\n", current_mode);
    if (current_mode == FAIR) {
        if(xSemaphoreTake(lockMode, 0) == pdTRUE){
            strcpy(mode, "FAIR");
        }
    }
    else if (current_mode == RANDOM) {
        if(xSemaphoreTake(lockMode, 0) == pdTRUE){
            strcpy(mode, "RANDOM");
        }
        
    }
    else if (current_mode == EASY) {
        if(xSemaphoreTake(lockMode, 0) == pdTRUE){
            strcpy(mode, "EASY");
        }
    }
    else if(current_mode == HARD){
        if(xSemaphoreTake(lockMode, 0) == pdTRUE){
            strcpy(mode, "HARD");
        }
    }

    if(NextQueue){
        xQueueReceive(NextQueue, &current_shape, 0);
    }

    if (current_shape == T) {
        if(xSemaphoreTake(lockShape, 0) == pdTRUE){
            shape = 'T';
        }
    }
    else if (current_mode == S) {
        if(xSemaphoreTake(lockShape, 0) == pdTRUE){
            shape = 'S';
        }
        
    }
    else if (current_mode == O) {
        if(xSemaphoreTake(lockShape, 0) == pdTRUE){
            shape = 'O';
        }
    }
    else if(current_mode == I){
        if(xSemaphoreTake(lockShape, 0) == pdTRUE){
            shape = 'I';
        }
    }
    else if(current_mode == Z){
        if(xSemaphoreTake(lockShape, 0) == pdTRUE){
            shape = 'Z';
        }
    }
    else if(current_mode == J){
        if(xSemaphoreTake(lockShape, 0) == pdTRUE){
            shape = 'J';
        }
    }
    else if(current_mode == L){
        if(xSemaphoreTake(lockShape, 0) == pdTRUE){
            shape = 'L';
        }
    }
    return 0;
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


    ModeQueue = xQueueCreate(1, sizeof(modes));

    if (!ModeQueue) {
        exit(EXIT_FAILURE);
    }
    
    NextQueue = xQueueCreate(1, sizeof(shapes));

    if(!NextQueue){
        exit(EXIT_FAILURE);
    }

    DrawSignal = xSemaphoreCreateBinary(); // Screen buffer locking
    if (!DrawSignal) {
        PRINT_ERROR("Failed to create draw signal");
    }

    ScreenLock = xSemaphoreCreateMutex();

    if (!ScreenLock) {
        PRINT_ERROR("Failed to create screen lock");
    }


    HandleUDP = xSemaphoreCreateMutex();

    if(!HandleUDP){
        PRINT_ERROR("Failed to create UDP lock");
    }

    lockTetris = xSemaphoreCreateMutex();

    if(!lockTetris){
        PRINT_ERROR("Failed to create tetris lock");
    }

    lockStats = xSemaphoreCreateMutex();

    if(!lockStats){
        PRINT_ERROR("Failed to create score lock");
    }

    lockShape = xSemaphoreCreateMutex();

    if(!lockShape){
        PRINT_ERROR("Failed to create score lock");
    }

    lockType = xSemaphoreCreateMutex();

    if(!lockType){
        PRINT_ERROR("Failed to create type lock");
    }


    lockMode = xSemaphoreCreateMutex();

    if(!lockMode){
        PRINT_ERROR("Failed to create mode lock");
    }
    
    buttons.lock = xSemaphoreCreateMutex();

    if(!buttons.lock){
        PRINT_ERROR("Failed to create buttons lock");
    }

    bools.lock = xSemaphoreCreateMutex();

    if(!bools.lock){
        PRINT_ERROR("Could not make flag lock");
    }
    xTaskCreate(drawingTask, "drawing", mainGENERIC_STACK_SIZE, NULL, 1, &drawTask);
    xTaskCreate(singleGame, "game", mainGENERIC_STACK_SIZE, NULL, 2, &singleGameHandle);
    xTaskCreate(moveTetrisTask, "moveTetris", mainGENERIC_STACK_SIZE, NULL, 1, &moveTetris);
    xTaskCreate(drawGameMenuTask, "mainMenu", mainGENERIC_STACK_SIZE, NULL, 1, &menuHandle);
    xTaskCreate(stateMachineTask, "StateMachine", mainGENERIC_STACK_SIZE, NULL, 3, &stateMachineHandle);
    
    xTaskCreate(vUDPControlTask, "demo", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &demoTaskHandle);


    return 0;

}