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

#define MMFONT  "IBMPlexSans-SemiBold.ttf"
#define MMFONTSIZE  30

#define HSFONT "IBMPlexSans-Regular.ttf"
#define HSFONTSIZE 50

#define GAMEFONT "IBMPlexSans-Medium.ttf"
#define GAMEFONTSIZE 30

#define GAMEOVERFONT "IBMPlexSans-Bold.ttf"
#define GAMEOVERFONTSIZE 60

#define fieldHeight 22
#define fieldWidth 14

#define boxLength 30
#define ground 630

#define UDP_RECEIVE_PORT 1234
#define UDP_TRANSMIT_PORT 1235
#define IPv4_addr "127.0.0.1"
#define UDP_BUFFER_SIZE 2000

#define TETRIS_LOGO "TetrisLogo.jpg"



TaskHandle_t moveTetris = NULL;
TaskHandle_t drawTask = NULL; 
TaskHandle_t singleGameHandle = NULL;
TaskHandle_t menuHandle = NULL;
TaskHandle_t stateMachineHandle = NULL;
TaskHandle_t demoTaskHandle = NULL;
TaskHandle_t multiGameHandle = NULL;
TaskHandle_t gameOverHandle = NULL;

SemaphoreHandle_t ScreenLock = NULL;
SemaphoreHandle_t DrawSignal = NULL;
SemaphoreHandle_t lockTetris = NULL;  
SemaphoreHandle_t lockMode = NULL;
SemaphoreHandle_t lockShape = NULL;
SemaphoreHandle_t HandleUDP = NULL;
SemaphoreHandle_t lockType = NULL;
SemaphoreHandle_t lockBool = NULL;
SemaphoreHandle_t lockField = NULL;


aIO_handle_t receiveHandle = NULL;



static QueueHandle_t ModeQueue = NULL;
static QueueHandle_t NextQueue = NULL;
static QueueHandle_t ListQueue = NULL;



int field[fieldHeight][fieldWidth];

unsigned int color[] = {Red, Blue, Green, Yellow, Aqua, Fuchsia, White};




typedef struct stats{
    char scorestring[7];
    char linesString[7];
    char levelString[7];
    int level;
    int score;
    int lines;
    int highscore[3];
    
    SemaphoreHandle_t lockStats;
}stats;

stats gamestats;

char mode[7];
char shape;
int gameType;
bool pauseflag;
bool GameOverFlag = false;











void fillField(int map[fieldHeight][fieldWidth]){
    for(int i = 0; i < fieldHeight; i++){
        for(int j = 0; j < fieldWidth; j++){
            map[i][j] = 0;
        }
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
    multi,
    gameOver
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
    L = 6,
    NONE
}shapes;



typedef enum modes modes;



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

void setIndex(tetromino*);

void moveFigures(tetromino*, direction);

int getTopRow();

void deleteRowsAndTrackScore();

void copyFigure(tetromino*, tetromino*);

unsigned char xCheckTetrisUDPInput();



void figureShape(tetromino* figure){
    switch(figure->type){
        case 0:{ //T
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x - boxLength; 
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x + boxLength;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x;
            figure->coords[3].y = figure->center.y - boxLength;
            figure->next = 1;
            break;
        }
        case 1:{//T
            figure->coords[0].x = figure->center.x; 
            figure->coords[0].y = figure->center.y; 
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y + boxLength;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y - boxLength;
            figure->coords[3].x = figure->center.x + boxLength;
            figure->coords[3].y = figure->center.y;
            figure->next = 2;
            break;
        }
        case 2:{//T
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x + boxLength;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x - boxLength;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x;
            figure->coords[3].y = figure->center.y + boxLength;
            figure->next = 3;
            break;
        }
        case 3:{//T
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y - boxLength;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y + boxLength;
            figure->coords[3].x = figure->center.x - boxLength;
            figure->coords[3].y = figure->center.y;
            figure->next = 0;
            break;
        }
        case 4:{//S
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x + boxLength;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y + boxLength;
            figure->coords[3].x = figure->center.x - boxLength;
            figure->coords[3].y = figure->center.y + boxLength;
            figure->next = 5;
            break;
        }
        case 5:{//S
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y - boxLength;
            figure->coords[2].x = figure->center.x + boxLength;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x + boxLength;
            figure->coords[3].y = figure->center.y + boxLength;
            figure->next = 4;
            break;
        }
        case 6:{//O
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x - boxLength;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y + boxLength;
            figure->coords[3].x = figure->center.x - boxLength;
            figure->coords[3].y = figure->center.y + boxLength;
            figure->next = 6;
            break;
        }
        case 7:{//I
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x + boxLength;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x - boxLength;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x - 2*boxLength;
            figure->coords[3].y = figure->center.y;
            figure->next = 8;
            break;
        }
        case 8:{//I
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y + boxLength;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y - boxLength;
            figure->coords[3].x = figure->center.x;
            figure->coords[3].y = figure->center.y + 2 * boxLength;
            figure->next = 7;
            break;
        }
        case 9:{//Z
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x - boxLength;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y + boxLength;
            figure->coords[3].x = figure->center.x + boxLength;
            figure->coords[3].y = figure->center.y + boxLength;
            figure->next = 10;
            break;
        }
        case 10:{//Z
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y - boxLength;
            figure->coords[2].x = figure->center.x - boxLength;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x - boxLength;
            figure->coords[3].y = figure->center.y + boxLength;
            figure->next = 9;
            break;
        }
        case 11:{//J
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y - boxLength;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y + boxLength;
            figure->coords[3].x = figure->center.x - boxLength;
            figure->coords[3].y = figure->center.y + boxLength;
            figure->next = 12;
            break;
        }
        case 12:{//J
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x + boxLength;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x - boxLength;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x - boxLength;
            figure->coords[3].y = figure->center.y - boxLength;
            figure->next = 13;
            break;
        }
        case 13:{//J
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y + boxLength;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y - boxLength;
            figure->coords[3].x = figure->center.x + boxLength;
            figure->coords[3].y = figure->center.y - boxLength;
            figure->next = 14;
            break;
        }
        case 14:{//J
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x - boxLength;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x + boxLength;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x + boxLength;
            figure->coords[3].y = figure->center.y + boxLength;
            figure->next = 11;
            break;
        }
        case 15:{//L
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y - boxLength;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y + boxLength;
            figure->coords[3].x = figure->center.x + boxLength;
            figure->coords[3].y = figure->center.y + boxLength;
            figure->next = 16;
            break;
        }
        case 16:{//L
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x + boxLength;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x - boxLength;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x - boxLength;
            figure->coords[3].y = figure->center.y + boxLength;
            figure->next = 17;
            break;
        }
        case 17:{//L
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x;
            figure->coords[1].y = figure->center.y + boxLength;
            figure->coords[2].x = figure->center.x;
            figure->coords[2].y = figure->center.y - boxLength;
            figure->coords[3].x = figure->center.x - boxLength;
            figure->coords[3].y = figure->center.y - boxLength;
            figure->next = 18;
            break;
        }
        case 18:{//L
            figure->coords[0].x = figure->center.x;
            figure->coords[0].y = figure->center.y;
            figure->coords[1].x = figure->center.x - boxLength;
            figure->coords[1].y = figure->center.y;
            figure->coords[2].x = figure->center.x + boxLength;
            figure->coords[2].y = figure->center.y;
            figure->coords[3].x = figure->center.x + boxLength;
            figure->coords[3].y = figure->center.y - boxLength;
            figure->next = 15;
            break;
        }
        default:
            break;
    }
}

bool checkGameOver(tetromino* figure){
    for(int i = 0; i < 4; i++){
        if(figure->coords[i].y == 60) return true;
    }
    return false;
}

void makeFigure(tetromino* figure){
    figure->center.x = 6*boxLength;
    figure->center.y = 2*boxLength;
    int type;
    if(gameType == 1){
        type = rand() % 7 + 1;
        if(type == 1) {
            figure->type = 0;
            
        }
        if(type == 2){
            figure->type = 4;
            
        } 
        if(type == 3) {
            figure->type = 6;
            
        }
        if(type == 4) {
            figure->type = 7;
            
        }
        if(type == 5) {
            figure->type = 9;
            
        }
        if(type == 6) {
            figure->type = 11;
            
        }
        if(type == 7) {
            figure->type = 15;
            
        }
        
    }
    else{
        if(shape == 'T') {
            figure->type = 0;
            printf("TNEW\n");
        }
        else if(shape == 'S') {
            figure->type = 4;
            printf("SNEW\n");
        }
        else if(shape == 'O') {
            figure->type = 6;
            printf("ONEW\n");
        }
        else if(shape == 'I') {
            figure->type = 7;
            printf("INEW\n");
        } 
        else if(shape == 'Z') {
            figure->type = 9;
            printf("ZNEW\n");
        }
        else if(shape == 'J') {
            figure->type = 11;
            printf("JNEW\n");
        }
        else if(shape == 'L') {
            figure->type = 15;
            printf("LNEW\n");
        }
        else{
            printf("NONE\n");
        }
    }
    if(figure->type <=3) figure->color = color[0]; 
    else if(figure->type > 3 && figure->type <= 5) figure->color = color[1];
    else if(figure->type == 6) figure->color = color[2];
    else if(figure->type > 6 && figure->type < 9) figure->color = color[3];
    else if(figure->type >= 9 && figure->type < 11) figure->color = color[4];
    else if(figure->type >= 11 && figure->type < 15) figure->color = color[5];
    else if(figure->type > 14 && figure->type < 19) figure->color = color[6];
    figureShape(figure);
    
}

void drawFigure(tetromino* figure){
    checkDraw(tumDrawFilledBox(figure->coords[0].x, figure->coords[0].y, boxLength, boxLength, figure->color), __FUNCTION__);
    checkDraw(tumDrawFilledBox(figure->coords[1].x, figure->coords[1].y, boxLength, boxLength, figure->color), __FUNCTION__);
    checkDraw(tumDrawFilledBox(figure->coords[2].x, figure->coords[2].y, boxLength, boxLength, figure->color), __FUNCTION__);
    checkDraw(tumDrawFilledBox(figure->coords[3].x, figure->coords[3].y, boxLength, boxLength, figure->color), __FUNCTION__);
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
        if(field[figure->coords[i].y / boxLength + 1][figure->coords[i].x / boxLength - 1]) return true;
    }
    return false;
}

bool detectCollisionRight(tetromino* figure){
    for(int i = 0; i < 4; i++){
        if(field[figure->coords[i].y / boxLength][figure->coords[i].x / boxLength]) return true;
    }
    return false;
}

bool detectCollisionLeft(tetromino* figure){
    for(int i = 0; i < 4; i++)
        if(field[figure->coords[i].y / boxLength][figure->coords[i].x / boxLength - 2]) return true;
    return false;
}
void setIndex(tetromino* figure){
    int index;
    if(figure->type <=3) index = 1; 
    else if(figure->type > 3 && figure->type <= 5) index = 2;
    else if(figure->type == 6) index = 3;
    else if(figure->type > 6 && figure->type < 9) index = 4;
    else if(figure->type >= 9 && figure->type < 11) index = 5;
    else if(figure->type >= 11 && figure->type < 15) index = 6;
    else if(figure->type > 14 && figure->type < 19) index = 7;
    for(int i = 0; i < 4; i++){
        field[figure->coords[i].y / boxLength][figure->coords[i].x / boxLength - 1] = index;
    }
}

void moveFigures(tetromino* figure, direction way){
    
    switch(way){
        case down:
            figure->center.y += boxLength; 
            break;
        case left:
            figure->center.x -= boxLength;
            break;
        case right:
            figure->center.x += boxLength;
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


void swap(int* x, int* y){
    int temp = *x;
    *x = *y;
    *y = temp;
}

void sortHighscores(int* hs){
    for(int i = 0; i < 2; i++){
        for(int j = 0; j < 3 - i - 1; j++){
            if(hs[j] < hs[j + 1]) swap(&hs[j], &hs[j + 1]);
        }
    }
}

void addHighscore(int* hs){
    bool sameFlag = false;
    bool zeroFlag = false;
    int lowest = 20000;
    
    for(int i = 0; i < 3; i++){
        if(hs[i] == 0) zeroFlag = true;
        if(hs[i] == gamestats.score) {
            sameFlag = true;
        }
        if(hs[i] < lowest) {
            lowest = hs[i];
        }
    }
    if(!sameFlag){
        for(int i = 0; i < 3; i++){
            if(hs[i] == 0) {
                hs[i] = gamestats.score;
                break;
            }
            if(!zeroFlag){
                if(gamestats.score > hs[i] && hs[i] == lowest) {
                    hs[i] = gamestats.score;
                    break;
                }
            }
        }
    }
}
void singleGame(void* pvParameters){
    bool pressed_down = false, pressed_right = false, pressed_left = false, pressed_rotate = false, 
    pressed_p = false, pressed_m = false, pressed_restart = false, pressed_pause = false, endgameflag = false;
    pauseflag = false;
    
    vTaskSuspend(NULL);
    if(xSemaphoreTake(gamestats.lockStats, 0) == pdTRUE){
        gamestats.score = 0;
        gamestats.level = 1;
        gamestats.lines = 0;
        gamestats.highscore[0] = 0;
        gamestats.highscore[1] = 0;
        gamestats.highscore[2] = 0;
        xSemaphoreGive(gamestats.lockStats);
    }
    if(xSemaphoreTake(lockTetris, 0) == pdTRUE && xSemaphoreTake(lockType, 0) == pdTRUE){
        if(gameType == 1){
            makeFigure(tetrisC);
            makeFigure(tetrisN);
            tetrisC->isMoving = true;
        }
        xSemaphoreGive(lockType);
        xSemaphoreGive(lockTetris);
    }
    

    while(1){
        xGetButtonInput();
        
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE && xSemaphoreTake(lockTetris, 0) == pdTRUE){
            if(buttons.buttons[SDL_SCANCODE_DOWN]){
                if(lowestCoord(tetrisC) < ground){
                    if(!detectCollisionDown(tetrisC)){
                        if(!pressed_down){
                            pressed_down = true;
                            moveFigures(tetrisC, down);
                        }
                    } else {
                        setIndex(tetrisC);
                        tetrisC->isMoving = false;
                    }
                    
                } else {
                    setIndex(tetrisC);
                    tetrisC->isMoving = false;
                }
            } else pressed_down = false;
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
        if(xSemaphoreTake(lockTetris, 0) == pdTRUE){
            if(!tetrisC->isMoving) {
                if(!checkGameOver(tetrisC)){
                    if(xSemaphoreTake(lockType, 0) == pdTRUE){
                        if(gameType == 2) xTaskNotifyGive(demoTaskHandle);
                        xSemaphoreGive(lockType);
                    }
                    copyFigure(tetrisC, tetrisN);
                    makeFigure(tetrisN);
                    tetrisC->isMoving = true;
                }
                else{
                    endgameflag = true;
                }

            }
            xSemaphoreGive(lockTetris);
        }

        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE && xSemaphoreTake(gamestats.lockStats, 0) == pdTRUE) {
            if(buttons.buttons[SDL_SCANCODE_M] && gamestats.level < 7) {
                if(!pressed_p){
                    gamestats.level++;
                    pressed_p = true;
                }
            }else pressed_p = false;
            if(buttons.buttons[SDL_SCANCODE_N] && gamestats.level > 1) {
                if(!pressed_m){
                    gamestats.level--;
                    pressed_m = true;
                }
            } else pressed_m = false;
            xSemaphoreGive(buttons.lock);
            xSemaphoreGive(gamestats.lockStats);
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
                    if(xSemaphoreTake(gamestats.lockStats, 0) == pdTRUE){
                        
                        gamestats.level = 1;
                        gamestats.score = 0;
                        gamestats.lines = 0;
                        
                        xSemaphoreGive(gamestats.lockStats);
                    }
                    pressed_restart = true;
                }
            } else pressed_restart = false;
            xSemaphoreGive(buttons.lock);
        }
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
            if(buttons.buttons[SDL_SCANCODE_P]){
                if(!pressed_pause){
                    if(xSemaphoreTake(lockBool, 0) == pdTRUE){
                        if(!pauseflag) {
                            vTaskSuspend(moveTetris);
                            pauseflag = true;
                            printf("Paused\n");
                        }
                        else { 
                            vTaskResume(moveTetris);
                            pauseflag = false;
                            printf("Unpaused\n");
                        }
                        xSemaphoreGive(lockBool);
                    }
                     
                    pressed_pause = true;
                    }
                    
                } else pressed_pause = false;
            xSemaphoreGive(buttons.lock);
        }
        if(endgameflag) {
            
            if(xSemaphoreTake(gamestats.lockStats, 0) == pdTRUE){
                addHighscore(gamestats.highscore);
                sortHighscores(gamestats.highscore);
                xSemaphoreGive(gamestats.lockStats);
            }
            if(xSemaphoreTake(lockBool,0) == pdTRUE){
                GameOverFlag = true;
                xSemaphoreGive(lockBool);
            }
            endgameflag = false;
            vTaskSuspend(singleGameHandle);
        }
        deleteRowsAndTrackScore();
        vTaskDelay(pdMS_TO_TICKS(50));   
    }
}

void deleteRowsAndTrackScore(){
    bool check;
    int full = 0, first;
    for(int i = fieldHeight - 1; i >= 0; i--){
        check = true;
        for(int j = 0; j < fieldWidth; j++){
            if(!field[i][j]) {
                check = false;
                break;
            }
        }
        if(check) {
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
        if(xSemaphoreTake(gamestats.lockStats, 0) == pdTRUE){
            if(full == 1) gamestats.score += gamestats.level * 40;      
            else if(full == 2) gamestats.score += gamestats.level * 100;
            else if(full == 3) gamestats.score += gamestats.level * 300;
            else if(full == 4) gamestats.score += gamestats.level * 1200;
            gamestats.lines += full;
            if(gamestats.score >= 100 && gamestats.score < 200) gamestats.level = 2;
            else if(gamestats.score >= 200 && gamestats.score < 400) gamestats.level = 3;
            else if(gamestats.score >= 400 && gamestats.score < 800) gamestats.level = 4;
            else if(gamestats.score >= 800 && gamestats.score < 1200) gamestats.level = 5;
            else if(gamestats.score >= 1200 && gamestats.score < 1800) gamestats.level = 6;
            else if(gamestats.score >= 1800) gamestats.score = 7; 
            xSemaphoreGive(gamestats.lockStats);
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

void drawNavigation(){
    int width;
    char text[] = "Welcome to Tetris";
    font_handle_t current_font = tumFontGetCurFontHandle();
    
    tumFontSelectFontFromName(MMFONT);
    checkDraw(tumDrawClear(Silver), __FUNCTION__);
    tumGetTextSize(text, &width, NULL);

    checkDraw(tumDrawText("Welcome to Tetris!", (SCREEN_WIDTH - width - 20)/2, 200, TUMBlue), __FUNCTION__);
    checkDraw(tumDrawText("Pick a game mode:", (SCREEN_WIDTH - width - 20)/2, 240, TUMBlue), __FUNCTION__);
    checkDraw(tumDrawText("S - Single player", (SCREEN_WIDTH - width - 20)/2, 280, TUMBlue), __FUNCTION__);
    checkDraw(tumDrawText("D - Multiplayer", (SCREEN_WIDTH - width - 20)/2, 320, TUMBlue), __FUNCTION__);

    tumFontSelectFontFromHandle(current_font);
    tumFontPutFontHandle(current_font);
}
void drawHighscores(){
    char highscoreString[3][5];
    font_handle_t current_font = tumFontGetCurFontHandle();
    tumFontSelectFontFromName(HSFONT);
    sprintf(highscoreString[0], "%d", gamestats.highscore[0]);
    sprintf(highscoreString[1], "%d", gamestats.highscore[1]);
    sprintf(highscoreString[2], "%d", gamestats.highscore[2]);
    
    int width;
    tumGetTextSize("Highscores:", &width, NULL);


    
    checkDraw(tumDrawText("Highscores:", (SCREEN_WIDTH - width)/2, 360, TUMBlue), __FUNCTION__);
    checkDraw(tumDrawText(highscoreString[0], (SCREEN_WIDTH - width)/2, 420, TUMBlue), __FUNCTION__);
    checkDraw(tumDrawText(highscoreString[1], (SCREEN_WIDTH - width)/2, 480, TUMBlue), __FUNCTION__);
    checkDraw(tumDrawText(highscoreString[2], (SCREEN_WIDTH - width)/2, 540, TUMBlue), __FUNCTION__);
    
    tumFontSelectFontFromHandle(current_font);
    tumFontPutFontHandle(current_font);
    
}

void drawGameMenuTask(void * pvParameters){
    while(1){
        if(DrawSignal){
            if(xSemaphoreTake(DrawSignal, portMAX_DELAY) == pdTRUE){
                if(xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE){
                    drawNavigation();
                    drawHighscores();
                    xSemaphoreGive(ScreenLock);
                }
            }
        }
        
    }
}

bool checkIfStarted();

void stateMachineTask(void * pvParameters){
    bool pressed_a = false, pressed_s = false, pressed_d = false, pressed_restart1 = false,pressed_restart2 = false,pressed_restart3 = false;
    currentState state = mainMenu;
    char list[] = "LIST";
    char next[] = "NEXT";
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
                        if(xSemaphoreTake(gamestats.lockStats, 0) == pdTRUE){
                            sprintf(gamestats.levelString, "%d", 0);
                            sprintf(gamestats.scorestring, "%d", 0);
                            sprintf(gamestats.linesString, "%d", 0);
                            gamestats.level = 1;
                            gamestats.score = 0;
                            gamestats.lines = 0;
                            xSemaphoreGive(gamestats.lockStats);
                        }
                        if(xSemaphoreTake(lockTetris, 0) == pdTRUE){
                            makeFigure(tetrisC);
                            xSemaphoreGive(lockTetris);
                        }
                        pressed_s = true;
                    }
                } else pressed_s = false;
                xSemaphoreGive(buttons.lock);
            }
        }
        if(state != multi ){
            
            
                if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
                if(buttons.buttons[SDL_SCANCODE_D]){
                    aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, list, strlen(list));
                    if(checkIfStarted()){
                        if(!pressed_d){
                            state = multi;
                            if(xSemaphoreTake(lockType, 0) == pdTRUE){
                                gameType = 2;
                                xSemaphoreGive(lockType);
                            }
                            clearMap();
                            if(xSemaphoreTake(lockTetris, 0) == pdTRUE){
                                makeFigure(tetrisC);
                                tetrisC->isMoving = true;
                                makeFigure(tetrisN);
                                xSemaphoreGive(lockTetris);
                            }
                            if(xSemaphoreTake(gamestats.lockStats, 0) == pdTRUE){
                                sprintf(gamestats.levelString, "%d", 0);
                                sprintf(gamestats.scorestring, "%d", 0);
                                sprintf(gamestats.linesString, "%d", 0);
                                gamestats.level = 1;
                                gamestats.score = 0;
                                gamestats.lines = 0;
                                xSemaphoreGive(gamestats.lockStats);
                            }
                            pressed_d = true;
                        }
                    }
                } else pressed_d = false;
                xSemaphoreGive(buttons.lock);
            }
            
            
        }
        if(state == gameOver){
            if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
                if(buttons.buttons[SDL_SCANCODE_S]){
                    if(!pressed_restart1){
                        clearMap();
                        if(xSemaphoreTake(lockTetris, 0) == pdTRUE){
                            makeFigure(tetrisC);
                            tetrisC->isMoving = true;    
                            xSemaphoreGive(lockTetris);
                        }
                        if(xSemaphoreTake(lockBool, 0) == pdTRUE){
                            GameOverFlag = false;
                            xSemaphoreGive(lockBool);
                        }
                        

                        state = single;
                        pressed_restart1 = true;
                    }
                } else pressed_restart1 = false;
                if(buttons.buttons[SDL_SCANCODE_D]){
                    aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, list, strlen(list));
                    if(checkIfStarted()){
                        if(!pressed_restart2){
                            clearMap();
                            if(xSemaphoreTake(lockTetris, 0) == pdTRUE){
                                makeFigure(tetrisC);
                                makeFigure(tetrisN);
                                tetrisC->isMoving = true;    
                                xSemaphoreGive(lockTetris);
                            }
                            if(xSemaphoreTake(lockBool, 0) == pdTRUE){
                                GameOverFlag = false;
                                xSemaphoreGive(lockBool);
                            }

                            state = multi;
                            pressed_restart2 = true;
                        }
                    }
                } else pressed_restart2 = false;
                if(buttons.buttons[SDL_SCANCODE_A]){
                    if(!pressed_restart3){
                        state = mainMenu;
                        pressed_restart3 = true;
                        if(xSemaphoreTake(lockBool, 0) == pdTRUE){
                            GameOverFlag = false;
                            xSemaphoreGive(lockBool);
                        }
                    }
                } else pressed_restart3 = false;
                xSemaphoreGive(buttons.lock);
            }
    
        }
        
        
        switch (state)
        {
        case mainMenu:{
            
            vTaskResume(demoTaskHandle);
            vTaskSuspend(singleGameHandle);
            vTaskSuspend(moveTetris);
            vTaskSuspend(drawTask);
            vTaskResume(menuHandle);
            vTaskSuspend(gameOverHandle);
            clearMap();
            break;
        }
        
        case single:{
            if(xSemaphoreTake(lockType,0) == pdTRUE){
                gameType = 1;
                xSemaphoreGive(lockType);
            }
            vTaskSuspend(demoTaskHandle);
            vTaskSuspend(menuHandle);
            vTaskResume(singleGameHandle);
            vTaskSuspend(gameOverHandle);
            vTaskResume(drawTask);
            if(xSemaphoreTake(lockBool, 0) == pdTRUE){
                if(!pauseflag) vTaskResume(moveTetris);
                xSemaphoreGive(lockBool);
            }
            if(GameOverFlag) state = gameOver;
            break;
        }
        case multi:{
            if(xSemaphoreTake(lockType,0) == pdTRUE){
                gameType = 2;
                xSemaphoreGive(lockType);
            }
            vTaskSuspend(menuHandle);
            vTaskSuspend(gameOverHandle);
            vTaskResume(singleGameHandle);
            vTaskResume(drawTask);
            vTaskResume(demoTaskHandle);
            if(xSemaphoreTake(lockBool, 0) == pdTRUE){
                if(!pauseflag) vTaskResume(moveTetris);
                xSemaphoreGive(lockBool);
            }
            
            
            
            if(GameOverFlag) state = gameOver;
            
            break;
        }
        case gameOver:{
            vTaskResume(gameOverHandle);
            vTaskSuspend(moveTetris);
            vTaskSuspend(singleGameHandle);
            vTaskSuspend(drawTask);
            
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
    bool started;
    BaseType_t xHigherPriorityTaskWoken1 = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken2 = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken3 = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken4 = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken5 = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken6 = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken7 = pdFALSE;

    if (xSemaphoreTakeFromISR(HandleUDP, &xHigherPriorityTaskWoken1) ==
        pdTRUE) {

        char send_command = 0;

        if(strncmp(buffer, "LIST=FAIR,RANDOM,EASY,HARD,DETERMINISTIC", (read_size < 41) ? read_size : 41) == 0){
            started = true;
            send_command = 1;
            
        }

        if (ListQueue && send_command) {
            xQueueSendFromISR(ListQueue, (void *)&started,
                              &xHigherPriorityTaskWoken2);
        }


        xSemaphoreGiveFromISR(HandleUDP, &xHigherPriorityTaskWoken3);
        
        if (strncmp(buffer, "MODE=FAIR", (read_size < 10) ? read_size : 11) ==
            0) {
            printf("FAIR\n");
            next_mode = FAIR;
            send_command = 1;
        }
        else if (strncmp(buffer, "MODE=RANDOM",
                         (read_size < 11) ? read_size : 11) == 0) {
            printf("OK\n");
            next_mode = RANDOM;
            send_command = 1;
        }
        else if (strncmp(buffer, "MODE=EASY",
                         (read_size < 10) ? read_size : 10) == 0) {
            printf("OK\n");
            next_mode = EASY;
            send_command = 1;
        }
        else if (strncmp(buffer, "MODE=HARD",
                         (read_size < 10) ? read_size : 10) == 0) {
            printf("OK\n");
            next_mode = HARD;
            send_command = 1;
        }
        else if(strncmp(buffer, "MODE=OK", (read_size < 8) ? read_size : 8) == 0){
            next_mode = OK;
            printf("OK\n");
            send_command = 1;
        }


        if (ModeQueue && send_command) {
            xQueueSendFromISR(ModeQueue, (void *)&next_mode,
                              &xHigherPriorityTaskWoken4);
        }

        xSemaphoreGiveFromISR(HandleUDP, &xHigherPriorityTaskWoken5);
        
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
                              &xHigherPriorityTaskWoken6);
        }
        
        xSemaphoreGiveFromISR(HandleUDP, &xHigherPriorityTaskWoken7);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken1 |
                           xHigherPriorityTaskWoken2 |
                           xHigherPriorityTaskWoken3 |
                           xHigherPriorityTaskWoken4 |
                           xHigherPriorityTaskWoken5 |
                           xHigherPriorityTaskWoken6 |
                           xHigherPriorityTaskWoken7);
    }
    else {
        fprintf(stderr, "[ERROR] Overlapping UDPHandler call\n");
    }
}



void vUDPControlTask(void *pvParameters)
{
    static char next[] = "NEXT";
    
    char *addr = NULL; 
    in_port_t port = UDP_RECEIVE_PORT;
    bool pressed_one = false, pressed_two = false, pressed_three = false, pressed_four = false, pressed_five = false;
    receiveHandle = aIOOpenUDPSocket(addr, port, UDP_BUFFER_SIZE, UDPHandler, NULL);
    vTaskSuspend(NULL);
    

    printf("UDP socket opened on port %d\n", port);
    
    
    while (1) {
        xGetButtonInput();
        
        
    
        if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
            if(buttons.buttons[SDL_SCANCODE_1]){
                if(!pressed_one){
                    aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, "MODE=FAIR", strlen("MODE=FAIR"));
                    pressed_one = true;
                }
            }else pressed_one = false;
            if(buttons.buttons[SDL_SCANCODE_2]){
                if(!pressed_two){
                    aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, "MODE=RANDOM", strlen("MODE=RANDOM"));
                    pressed_two = true;

                }
            }else pressed_two = false;
            if(buttons.buttons[SDL_SCANCODE_3]){
                if(!pressed_three){
                    aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, "MODE=EASY", strlen("MODE=EASY"));
                    pressed_three = true;

                }
            }else pressed_three = false;
            if(buttons.buttons[SDL_SCANCODE_4]){
                if(!pressed_four){
                    aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, "MODE=HARD", strlen("MODE=HARD"));
                    pressed_four = true;

                }
            }else pressed_four = false;
            if(buttons.buttons[SDL_SCANCODE_5]){
                if(!pressed_five){
                    aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, "MODE", strlen("MODE"));
                    pressed_five = true;
                }
            }else pressed_five = false;
            xSemaphoreGive(buttons.lock);
    }

        if(ulTaskNotifyTake(pdFALSE, 0)) aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, next, strlen(next));
        
        xCheckTetrisUDPInput();
        
        
       
        
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

bool checkIfStarted(){
    bool check;

    if(ListQueue){
        xQueueReceive(ListQueue, &check, 0);
    }
    return check;
    

}


unsigned char xCheckTetrisUDPInput()
{
    static modes current_mode;
    static shapes current_shape;

    if (ModeQueue) {
        xQueueReceive(ModeQueue, &current_mode, 0);
        
    }
    
    if (current_mode == FAIR) {
        if(xSemaphoreTake(lockMode, 0) == pdTRUE){
            strcpy(mode, "FAIR");
            xSemaphoreGive(lockMode);
        }
    }
    else if (current_mode == RANDOM) {
        if(xSemaphoreTake(lockMode, 0) == pdTRUE){
            strcpy(mode, "RANDOM");
            xSemaphoreGive(lockMode);
        }
        
    }
    else if (current_mode == EASY) {
        if(xSemaphoreTake(lockMode, 0) == pdTRUE){
            strcpy(mode, "EASY");
            xSemaphoreGive(lockMode);
        }
    }
    else if(current_mode == HARD){
        if(xSemaphoreTake(lockMode, 0) == pdTRUE){
            strcpy(mode, "HARD");
            xSemaphoreGive(lockMode);
        }
    }

    if(NextQueue){
        xQueueReceive(NextQueue, &current_shape, 0);
    }

    if (current_shape == T) {
        if(xSemaphoreTake(lockShape, 0) == pdTRUE){
            shape = 'T';
            
            xSemaphoreGive(lockShape);
        }
    }
    else if (current_shape == S) {
        if(xSemaphoreTake(lockShape, 0) == pdTRUE){
            shape = 'S';
            
            xSemaphoreGive(lockShape);
        }
        
    }
    else if (current_shape == O) {
        if(xSemaphoreTake(lockShape, 0) == pdTRUE){
            shape = 'O';
            
            xSemaphoreGive(lockShape);
        }
    }
    else if(current_shape == I){
        if(xSemaphoreTake(lockShape, 0) == pdTRUE){
            shape = 'I';
            
            xSemaphoreGive(lockShape);
        }
    }
    else if(current_shape == Z){
        if(xSemaphoreTake(lockShape, 0) == pdTRUE){
            shape = 'Z';
            
            xSemaphoreGive(lockShape);
        }
    }
    else if(current_shape == J){
        if(xSemaphoreTake(lockShape, 0) == pdTRUE){
            shape = 'J';
            
            xSemaphoreGive(lockShape);
        }
    }
    else if(current_shape == L){
        if(xSemaphoreTake(lockShape, 0) == pdTRUE){
            shape = 'L';
            
            xSemaphoreGive(lockShape);
        }
    }
    return 0;
}

void moveTetrisTask(void *pvParameters){
    vTaskSuspend(NULL);
    while(1){
        if(xSemaphoreTake(lockTetris, 0) == pdTRUE){
            if(lowestCoord(tetrisC) < ground && !detectCollisionDown(tetrisC) && tetrisC->isMoving){
                tetrisC->center.y += boxLength;
                figureShape(tetrisC);
            }
            else {
                setIndex(tetrisC);
                tetrisC->isMoving = false; 
            }
            xSemaphoreGive(lockTetris);
        }
        vTaskDelay(pdMS_TO_TICKS(1000)/gamestats.level);
    }   
}

void drawStationaryBlocks(){   
    for(int col = 0; col < fieldHeight; col++){
        for(int row = 0; row < fieldWidth; row++){
            if(field[col][row] == 1) tumDrawFilledBox((row + 1)* boxLength, col*boxLength, boxLength, boxLength, color[0]);
            else if(field[col][row] == 2) tumDrawFilledBox((row + 1)* boxLength, col*boxLength, boxLength, boxLength, color[1]);
            else if(field[col][row] == 3) tumDrawFilledBox((row + 1)* boxLength, col*boxLength, boxLength, boxLength, color[2]);
            else if(field[col][row] == 4) tumDrawFilledBox((row + 1)* boxLength, col*boxLength, boxLength, boxLength, color[3]);
            else if(field[col][row] == 5) tumDrawFilledBox((row + 1)* boxLength, col*boxLength, boxLength, boxLength, color[4]);
            else if(field[col][row] == 6) tumDrawFilledBox((row + 1)* boxLength, col*boxLength, boxLength, boxLength, color[5]);
            else if(field[col][row] == 7) tumDrawFilledBox((row + 1)* boxLength, col*boxLength, boxLength, boxLength, color[6]);
        }
    }
}


void drawNextFigure(){
    font_handle_t current_font = tumFontGetCurFontHandle();

    
    tumFontSelectFontFromName(GAMEFONT);

    checkDraw(tumDrawText("NEXT", 530, 350, White), __FUNCTION__);

    tumDrawFilledBox(tetrisN->coords[0].x + 420, tetrisN->coords[0].y + 400, boxLength, boxLength, tetrisN->color);
    tumDrawFilledBox(tetrisN->coords[1].x + 420, tetrisN->coords[1].y + 400, boxLength, boxLength, tetrisN->color);
    tumDrawFilledBox(tetrisN->coords[2].x + 420, tetrisN->coords[2].y + 400, boxLength, boxLength, tetrisN->color);
    tumDrawFilledBox(tetrisN->coords[3].x + 420, tetrisN->coords[3].y + 400, boxLength, boxLength, tetrisN->color);

    tumFontSelectFontFromHandle(current_font);
    tumFontPutFontHandle(current_font);
}


void drawStats(){
    if(xSemaphoreTake(gamestats.lockStats, 0) == pdTRUE){

        font_handle_t current_font = tumFontGetCurFontHandle();

        tumFontSelectFontFromName(GAMEFONT);               
        sprintf(gamestats.scorestring, "Score: %d", gamestats.score);
        checkDraw(tumDrawText(gamestats.scorestring, 530, 100, Red), __FUNCTION__);    

        sprintf(gamestats.levelString,"Level: %d", gamestats.level);
        checkDraw(tumDrawText(gamestats.levelString, 530, 170, White), __FUNCTION__);    

        sprintf(gamestats.linesString, "Lines: %d", gamestats.lines);
        checkDraw(tumDrawText(gamestats.linesString, 530, 250, White), __FUNCTION__);

        tumFontSelectFontFromHandle(current_font);
        tumFontPutFontHandle(current_font);
        
        xSemaphoreGive(gamestats.lockStats);
    }
}



void drawGameOver(){
    
    font_handle_t current_font = tumFontGetCurFontHandle();
    
    tumFontSelectFontFromName(GAMEFONT);

    int widthGameOver, widthOption;
    
    char gameOver[] = "GAME OVER";
    
    char pickOption[] = "A - Main Menu  S - Single Player   D - Double Player";

    

    tumGetTextSize(gameOver, &widthGameOver, NULL);
    tumGetTextSize(pickOption, &widthOption, NULL);

    sprintf(gamestats.scorestring, "Score: %d", gamestats.score);

    checkDraw(tumDrawText(gameOver, (SCREEN_WIDTH - widthGameOver)/2, SCREEN_HEIGHT/2 - GAMEFONTSIZE, White), __FUNCTION__);
    checkDraw(tumDrawText(pickOption, (SCREEN_WIDTH - widthOption)/2, SCREEN_HEIGHT/2, White), __FUNCTION__);
    checkDraw(tumDrawText(gamestats.scorestring, SCREEN_WIDTH/2 - 50, SCREEN_HEIGHT/2 + GAMEFONTSIZE, White), __FUNCTION__);
       
    tumFontSelectFontFromHandle(current_font);
    tumFontPutFontHandle(current_font);
}


void gameOverTask(void* pvParameters){
    vTaskSuspend(NULL);
    while(1){
        checkDraw(tumDrawClear(Black), __FUNCTION__);
        clearMap();
        drawGameOver();
        vTaskDelay(pdMS_TO_TICKS(20));
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
                drawStationaryBlocks();
                if(xSemaphoreTake(lockTetris, 0) == pdTRUE){
                    drawFigure(tetrisC); 
                    xSemaphoreGive(lockTetris);
                }
                drawNextFigure();
                drawStats();
            
                
                xSemaphoreGive(ScreenLock);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));        
    }
}


int tetrisMain(void){
    

    fillField(field);
    


    ModeQueue = xQueueCreate(1, sizeof(modes));

    if (!ModeQueue) {
        exit(EXIT_FAILURE);
    }
    
    NextQueue = xQueueCreate(1, sizeof(shapes));

    if(!NextQueue){
        exit(EXIT_FAILURE);
    }

    ListQueue = xQueueCreate(1, sizeof(bool));

    if(!ListQueue){
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

    gamestats.lockStats = xSemaphoreCreateMutex();

    if(!gamestats.lockStats){
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

    lockBool = xSemaphoreCreateMutex();

    if(!lockBool){
        PRINT_ERROR("Could not make flag lock");
    }

    lockField = xSemaphoreCreateMutex();

    if(!lockField){
        PRINT_ERROR("Could not make field lock");
    }

    

    tumFontLoadFont(GAMEFONT, GAMEFONTSIZE);
    tumFontLoadFont(MMFONT, MMFONTSIZE);
    tumFontLoadFont(HSFONT, HSFONTSIZE);

    xTaskCreate(drawingTask, "drawing", mainGENERIC_STACK_SIZE, NULL, 1, &drawTask);
    xTaskCreate(singleGame, "game", mainGENERIC_STACK_SIZE, NULL, 2, &singleGameHandle);
    xTaskCreate(moveTetrisTask, "moveTetris", mainGENERIC_STACK_SIZE, NULL, 1, &moveTetris);
    xTaskCreate(drawGameMenuTask, "mainMenu", mainGENERIC_STACK_SIZE, NULL, 1, &menuHandle);
    xTaskCreate(stateMachineTask, "StateMachine", mainGENERIC_STACK_SIZE, NULL, 3, &stateMachineHandle);
    xTaskCreate(gameOverTask, "GameOver", mainGENERIC_STACK_SIZE, NULL, 1, &gameOverHandle);
    
    xTaskCreate(vUDPControlTask, "demo", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &demoTaskHandle);


    return 0;

}
