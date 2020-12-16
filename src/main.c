#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>


#include <SDL2/SDL_scancode.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"

#include "TUM_Ball.h"
#include "TUM_Draw.h"
#include "TUM_Font.h"
#include "TUM_Event.h"
#include "TUM_Sound.h"
#include "TUM_Utils.h"
#include "TUM_FreeRTOS_Utils.h"
#include "TUM_Print.h"

#include "AsyncIO.h"

#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)

#define STATE_QUEUE_LENGTH 1
#define STACK_SIZE 200
#define STATE_COUNT 3

#define STATE_ONE 0
#define STATE_TWO 1
#define STATE_THREE 2

#define NEXT_TASK 0
#define PREV_TASK 1

#define STARTING_STATE STATE_ONE

#define STATE_DEBOUNCE_DELAY 300

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR
#define CAVE_SIZE_X SCREEN_WIDTH / 2
#define CAVE_SIZE_Y SCREEN_HEIGHT / 2
#define CAVE_X CAVE_SIZE_X / 2
#define CAVE_Y CAVE_SIZE_Y / 2
#define CAVE_THICKNESS 25
#define LOGO_FILENAME "freertos.jpg"
#define UDP_BUFFER_SIZE 2000
#define UDP_TEST_PORT_1 1234
#define UDP_TEST_PORT_2 4321
#define MSG_QUEUE_BUFFER_SIZE 1000
#define MSG_QUEUE_MAX_MSG_COUNT 10
#define TCP_BUFFER_SIZE 2000
#define TCP_TEST_PORT 2222

#ifdef TRACE_FUNCTIONS
#include "tracer.h"
#endif



#define PI 3.14


aIO_handle_t udp_soc_one = NULL;
aIO_handle_t udp_soc_two = NULL;
aIO_handle_t tcp_soc = NULL;

const unsigned char next_state_signal = NEXT_TASK;
const unsigned char prev_state_signal = PREV_TASK;



TaskHandle_t exercise2Handle = NULL;  //exercise 2
TaskHandle_t draw3Handle = NULL; // used for drawing the third exercise

// 3.2.2
StaticTask_t xTaskBuffer;
StackType_t xStack[ STACK_SIZE ];
TaskHandle_t flag1Handle = NULL, flag2Handle = NULL; 
bool check1 = false, check2 = true;

// 3.2.3

TaskHandle_t task1Handle = NULL, task2Handle = NULL, taskNotifHandle = NULL;

TimerHandle_t myTimer = NULL;

SemaphoreHandle_t task2Sem = NULL;

bool start_a = false, start_d = false;

// 3.2.4

TaskHandle_t increment = NULL, susres = NULL;
int a = 0, d = 0;


// 4th exercise


TaskHandle_t print1Handle = NULL, print2Handle = NULL, print3Handle = NULL, print4Handle = NULL, printElementsHandle = NULL; // Exercise 4 
SemaphoreHandle_t wakeThree = NULL, LockBool = NULL, LockOutput = NULL, LockIndex = NULL;
char output[15][25] = {
    {"Tick 1 : "},
    {"Tick 2 : "},
    {"Tick 3 : "},
    {"Tick 4 : "},
    {"Tick 5 : "},
    {"Tick 6 : "},
    {"Tick 7 : "},
    {"Tick 8 : "},
    {"Tick 9 : "},
    {"Tick 10 : "},
    {"Tick 11 : "},
    {"Tick 12 : "},
    {"Tick 13 : "},
    {"Tick 14 : "},
    {"Tick 15 : "},   
};
int k = 0;


static TaskHandle_t StateMachine = NULL;
static TaskHandle_t BufferSwap = NULL;



static QueueHandle_t StateQueue = NULL;
static SemaphoreHandle_t DrawSignal = NULL;
static SemaphoreHandle_t ScreenLock = NULL;

static image_handle_t logo_image = NULL;

typedef struct buttons_buffer {
    unsigned char buttons[SDL_NUM_SCANCODES];
    SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };

void checkDraw(unsigned char status, const char *msg)
{
    if (status) {
        if (msg)
            fprints(stderr, "[ERROR] %s, %s\n", msg,
                    tumGetErrorMessage());
        else {
            fprints(stderr, "[ERROR] %s\n", tumGetErrorMessage());
        }
    }
}

/*
 * Changes the state, either forwards of backwards
 */
void changeState(volatile unsigned char *state, unsigned char forwards)
{
    switch (forwards) {
        case NEXT_TASK:
            if (*state == STATE_COUNT - 1) {
                *state = 0;
            }
            else {
                (*state)++;
            }
            break;
        case PREV_TASK:
            if (*state == 0) {
                *state = STATE_COUNT - 1;
            }
            else {
                (*state)--;
            }
            break;
        default:
            break;
    }
}

/*
 * Example basic state machine with sequential states
 */
void basicSequentialStateMachine(void *pvParameters)
{
    unsigned char current_state = STARTING_STATE; // Default state
    unsigned char state_changed =
        1; // Only re-evaluate state if it has changed
    unsigned char input = 0;

    const int state_change_period = STATE_DEBOUNCE_DELAY;

    TickType_t last_change = xTaskGetTickCount();

    while (1) {
        if (state_changed) {
            goto initial_state;
        }

        // Handle state machine input
        if (StateQueue)
            if (xQueueReceive(StateQueue, &input, portMAX_DELAY) ==
                pdTRUE)
                if (xTaskGetTickCount() - last_change >
                    state_change_period) {
                    changeState(&current_state, input);
                    state_changed = 1;
                    last_change = xTaskGetTickCount();
                }

initial_state:
        // Handle current state
        if (state_changed) {
            switch (current_state) {
                case STATE_ONE:
                    if (draw3Handle && printElementsHandle) {
                        vTaskSuspend(draw3Handle);
                        vTaskSuspend(printElementsHandle);
                    }
                    
                    if (exercise2Handle) {
                        vTaskResume(exercise2Handle);
                    }
                    break;
                case STATE_TWO:
                    if (exercise2Handle && printElementsHandle) {
                        vTaskSuspend(exercise2Handle);
                        vTaskSuspend(printElementsHandle);
                    }
                    if (draw3Handle) {
                        vTaskResume(draw3Handle);
                    }
                    break;
                case STATE_THREE:
                    if(draw3Handle && exercise2Handle){
                        vTaskSuspend(draw3Handle);
                        vTaskSuspend(exercise2Handle);
                    }
                    if(printElementsHandle){
                        vTaskResume(printElementsHandle);
                    }

                default:
                    break;
            }
            state_changed = 0;
        }
    }
}

void vSwapBuffers(void *pvParameters)
{
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    const TickType_t frameratePeriod = 10;

    tumDrawBindThread(); // Setup Rendering handle with correct GL context

    while (1) {
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
            tumDrawUpdateScreen();
            tumEventFetchEvents(FETCH_EVENT_BLOCK);
            xSemaphoreGive(ScreenLock);
            xSemaphoreGive(DrawSignal);
            vTaskDelayUntil(&xLastWakeTime,
                            pdMS_TO_TICKS(frameratePeriod));
        }
    }
}

void xGetButtonInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
        xSemaphoreGive(buttons.lock);
    }
}




#define FPS_AVERAGE_COUNT 50
#define FPS_FONT "IBMPlexSans-Bold.ttf"

void vDrawFPS(void)
{
    static unsigned int periods[FPS_AVERAGE_COUNT] = { 0 };
    static unsigned int periods_total = 0;
    static unsigned int index = 0;
    static unsigned int average_count = 0;
    static TickType_t xLastWakeTime = 0, prevWakeTime = 0;
    static char str[10] = { 0 };
    static int text_width;
    int fps = 0;
    font_handle_t cur_font = tumFontGetCurFontHandle();

    if (average_count < FPS_AVERAGE_COUNT) {
        average_count++;
    }
    else {
        periods_total -= periods[index];
    }

    xLastWakeTime = xTaskGetTickCount();

    if (prevWakeTime != xLastWakeTime) {
        periods[index] =
            configTICK_RATE_HZ / (xLastWakeTime - prevWakeTime);
        prevWakeTime = xLastWakeTime;
    }
    else {
        periods[index] = 0;
    }

    periods_total += periods[index];

    if (index == (FPS_AVERAGE_COUNT - 1)) {
        index = 0;
    }
    else {
        index++;
    }

    fps = periods_total / average_count;

    tumFontSelectFontFromName(FPS_FONT);

    sprintf(str, "FPS: %2d", fps);

    if (!tumGetTextSize((char *)str, &text_width, NULL))
        checkDraw(tumDrawText(str, SCREEN_WIDTH - text_width - 10,
                              SCREEN_HEIGHT - DEFAULT_FONT_SIZE * 1.5,
                              Skyblue),
                  __FUNCTION__);

    tumFontSelectFontFromHandle(cur_font);
    tumFontPutFontHandle(cur_font);
}



static int vCheckStateInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(E)]) {
            buttons.buttons[KEYCODE(E)] = 0;
            if (StateQueue) {
                xSemaphoreGive(buttons.lock);
                xQueueSend(StateQueue, &next_state_signal, 0);
                return 0;
            }
            return -1;
        }
        xSemaphoreGive(buttons.lock);
    }

    return 0;
}









void exercise2Task(void *pvParameters)
{

    //Array for the coordinates of the triangle
    coord_t trianglecoords[3];  
    trianglecoords[0].x = SCREEN_WIDTH/2 - 30;
    trianglecoords[0].y = 240;
    trianglecoords[1].x = SCREEN_WIDTH/2 + 30;
    trianglecoords[1].y = 240;
    trianglecoords[2].x = SCREEN_WIDTH/2;
    trianglecoords[2].y = 180;
    
    
    unsigned short a = 0, b = 0, c = 0, d = 0, mouse_left = 0, mouse_right = 0, mouse_middle = 0;

    //coordinates of the circle
    signed short circle_x = SCREEN_WIDTH/2 + 100;
    signed short circle_y = 210;
    double angle_circle = atan2(100,0)*180/PI;
    signed short box_x = SCREEN_WIDTH/2 - 80;
    signed short box_y = 300;
    double angle_box = atan2(SCREEN_WIDTH/2 - box_x,210 - 230)*180/PI;
    //strings to print
    char* bottom_string = "Filip Ratic";
    char* top_string = "Hello ESPL!";
    static char abString[50];
    static char cdString[50];
    static char mouseLocation[50];
    static char mouseClick[50];
    //coordinates of the top string, used for moving the string

    signed short string_x = 380;
    signed short string_y = 1;

    // Coordinates for the start and ending of one cycle for moving the top string
    signed short start = 380;
    signed short reset = 480;

    bool flag;  // Boolean variable to check if the string got to the end of the path it's supposed to move in 


        
    signed short mouse_x, mouse_y;
    
   
    
    


    bool pressed_a = false, pressed_b = false, pressed_c = false, pressed_d = false, pressed_LMB = false, pressed_RMB = false, pressed_MMB = false;

    

    while (1) {
        
        xGetButtonInput(); // Update global input

    

        // `buttons` is a global shared variable and as such needs to be
        // guarded with a mutex, mutex must be obtained before accessing the
        // resource and given back when you're finished. If the mutex is not
        // given back then no other task can access the reseource.
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(
                                    Q)]) { // Equiv to SDL_SCANCODE_Q
                exit(EXIT_SUCCESS);
            }
            xSemaphoreGive(buttons.lock);
        }

        

        mouse_x = tumEventGetMouseX();
        mouse_y = tumEventGetMouseY();
       
        
        
        //Moving the text from one end to another
        
        if(string_x == start) flag = true;                   
        else if(string_x == reset) flag = false;
        if(flag) string_x++;
        else string_x--;


        // Counters for button presses, all locked with a mutex
        // Logic i used for debouncing buttons: we have a variable pressed_x, which is set to false, since the button originally isn't pressed. If the button gets pressed, 
        // the value gets changed to true and the if statement blocks it from entering in the next frame.



        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[SDL_SCANCODE_A]) {
                if(!pressed_a){
                    a++;
                    pressed_a = true;
                } 

            }   else{
                pressed_a = false;
            }
            xSemaphoreGive(buttons.lock);
        }

    
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[SDL_SCANCODE_B]) {
                if(!pressed_b){
                    b++;
                    pressed_b = true;
                } 

            }   else{
                pressed_b = false;
            }
            xSemaphoreGive(buttons.lock);
        }
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[SDL_SCANCODE_C]) {
                if(!pressed_c){
                    c++;
                    pressed_c = true;
                } 

            }   else{
                pressed_c = false;
            }
            xSemaphoreGive(buttons.lock);
        }
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[SDL_SCANCODE_D]) {
                if(!pressed_d){
                    d++;
                    pressed_d = true;
                } 

            }   else{
                pressed_d = false;
            }
            xSemaphoreGive(buttons.lock);
        }

        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (tumEventGetMouseRight()) {
                if(!pressed_RMB){
                    mouse_right++;
                    pressed_RMB = true;
                } 

            }   else{
                pressed_RMB = false;
            }
            xSemaphoreGive(buttons.lock);
        }
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (tumEventGetMouseMiddle()) {
                if(!pressed_MMB){
                    mouse_middle++;
                    pressed_MMB = true;
                } 

            }   else{
                pressed_MMB = false;
            }
            xSemaphoreGive(buttons.lock);
        }

        

        //Reseting the values in case LMB gets used

        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (tumEventGetMouseLeft()) { 
               if(!pressed_LMB){
                    mouse_left++;
                    a = 0;
                    b = 0;
                    c = 0;
                    d = 0;
                    pressed_LMB = true;
                } 

            }   else{
                pressed_LMB = false;
            }
            xSemaphoreGive(buttons.lock);
        }

        
        //printing everything on the screen. 


        if(xSemaphoreTake(DrawSignal, portMAX_DELAY) == pdTRUE){
            xSemaphoreTake(ScreenLock, portMAX_DELAY);
            
            //drawing the elements on the screen

            tumDrawClear(White);
            tumDrawCircle(circle_x, circle_y, 40, TUMBlue); // Circle
            tumDrawTriangle(trianglecoords, Red); // Triangle
            tumDrawFilledBox(box_x, box_y, 60, 60, Purple); //Box
            
            
            //drawing the text on the screen
            tumDrawText(bottom_string, SCREEN_WIDTH/2 - 50, SCREEN_HEIGHT - 30, Black);
            tumDrawText(top_string, string_x, string_y, Black);

            //keeping track of button presses

            sprintf(abString,"A was pressed: %d times || B was pressed: %d times", a, b);
            tumDrawText(abString, 5, 5, Black);

            sprintf(cdString,"C was pressed: %d times || D was pressed: %d times", c, d);
            tumDrawText(cdString, 5, 20, Black);
            
            sprintf(mouseLocation, "Mouse X Coordinate: %d || Mouse Y Coordinate: %d", mouse_x, mouse_y);
            tumDrawText(mouseLocation, 5, 40, Black);
            
            sprintf(mouseClick, "Mouse LMB: %d || Mouse RMB: %d || Mouse MMB: %d", mouse_left, mouse_right, mouse_middle);
            tumDrawText(mouseClick, 5, 60, Black); 


            xSemaphoreGive(ScreenLock);


        }
        
        
        
        
        
        
        // part for moving the whole screen. /10 because I don't want the offset to be too strong 
        
        tumDrawSetGlobalXOffset((int)mouse_x/10);
        tumDrawSetGlobalYOffset((int)mouse_y/10);
        
        
        // algorithm for moving the box and circle around the triangle
        
        
        circle_x = circle_x + cos(angle_circle)*100;
        circle_y = circle_y + sin(angle_circle)*100;
        
        box_x = box_x - cos(angle_box)*102;
        box_y = box_y - sin(angle_box)*102;
             
        angle_circle++;
        angle_box++;
        if(angle_circle > 90.0456 + 6.28) {                      // + 6.28 because that is 360 degrees. My circle and box started 'running away'
            angle_circle = 90.0456;                         // from the triangle every time they made a full rotation, so i just reset their values
            circle_x = SCREEN_WIDTH/2 + 100;                // (90.04 and 101.36 are the values that i got as the start values from a previous print statement)
            circle_y = 210;
        }
        
        if(angle_box > 101.3610 + 6.28){
            angle_box = 101.3610;
            box_x = SCREEN_WIDTH/2 - 80;
            box_y = 300;
        }
       

        
        
        vDrawFPS();


        vCheckStateInput();
        vTaskDelay((TickType_t)20);
    }
}




void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static – otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task’s
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task’s stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
function then they must be declared static – otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task’s state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task’s stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}


//exercise 3.2.2. Logic is pretty simple. Period tracks the time the circles should be drawn. Every 500ms/250ms the boolean value gets changed -> draws/doesn't draw

void circle1(void * p){
    TickType_t xLastTick;
    const TickType_t freq = 1;
    TickType_t period;
    period = 1000/freq/portTICK_PERIOD_MS;
    xLastTick = xTaskGetTickCount();
    
    while(1){
        if(xSemaphoreTake(LockBool,0) == pdTRUE){
            check1 = !check1;
            xSemaphoreGive(LockBool);
        }


        vTaskDelayUntil(&xLastTick, period/2);
    }
}

void circle2(void * p){
    TickType_t xLastTick;
    const TickType_t freq = 2;
    TickType_t period;
    period = 1000/freq/portTICK_PERIOD_MS;
    xLastTick = xTaskGetTickCount();
    configASSERT( ( uint32_t ) pvParameters == 1UL );           //necessary for static allocation
    
    while(1){
        if(xSemaphoreTake(LockBool,0) == pdTRUE){
            check2 = !check2;
            xSemaphoreGive(LockBool);
        }

        vTaskDelayUntil(&xLastTick, period/2);
    }
}








// timer used for resetting the values to 0

void vTimerCallback(TimerHandle_t xTimer){
    if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
        a = 0;
        d = 0;
        xSemaphoreGive(buttons.lock);
    }
}


/*
****exercise 3.2.3 ***** 

taskNotif sends a notification to task1 every time A gets pressed. Task1 then sets the start_a flag to true and 
 the drawing task starts drawing the text which shows how many times the button got pressed, since the counters should be sleeping before the first button press.
 Task2 is basically the same principle, with the difference that a binary semaphore is used instead of a task notification. 
 We also see that i implemented the same debounce logic for the buttons like in the second exercise. 
*/
void task1(void * p){
    while(1){
        if(ulTaskNotifyTake(pdTRUE, 0) == pdTRUE){
            if(xSemaphoreTake(LockBool, 0) == pdTRUE){
                start_a = true;
                xSemaphoreGive(LockBool);
            }
            if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
                a++;
            }

        } 
        
        
        vTaskDelay(20/portTICK_PERIOD_MS);          
    }
}


void task2(void * p){
    
    while(1){
        
        if(xSemaphoreTake(task2Sem, 0) == pdTRUE){
            start_d = true;
            d++;
            
        }   
        vTaskDelay(20/portTICK_PERIOD_MS);
}
}


void taskNotif(void * p){
    bool pressed_a = false, pressed_d = false;

    while(1){
        xGetButtonInput();        
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[SDL_SCANCODE_A]) {
                if(!pressed_a){
                    pressed_a = true;
                    xTaskNotifyGive(task1Handle);
                }
                } else {
                    pressed_a = false;
                }
            xSemaphoreGive(buttons.lock);
            if (buttons.buttons[SDL_SCANCODE_D]) {
                if(!pressed_d){
                    pressed_d = true;
                    xSemaphoreGive(task2Sem);
                }
                } else {
                    pressed_d = false;
                }
            }
        vTaskDelay(20/portTICK_PERIOD_MS);
        }
               
}




/* 
*******exercise 3.2.4*********

task increaseVariable sends a notification every second to the exercise3 drawing task, which in turn increments a variable every time it receives the notification. 
This was done to avoid unnecessary global variables and to also make use of task notifications.
taskSuspendResume keeps track of the state of the button I. Every time the button gets pressed, a flag (susflag) gets changed. It is used for keeping track of the status of 
the increment variable. If it is suspended, the flag is true, if not then false. There is also pressed_i, which again is used for debouncing the button used. 



I thought about removing the tumDraw____ part from the draw task and just to get them into a function and simply call the function every time, but in the demo code almost all of the 
drawing functions are simply sitting in the task function so I decided to leave it as it is. Better safe than sorry.  


*/


void increaseVariable(void * pvParameters){
    TickType_t delay = 1000;
    while(1){
        xTaskNotifyGive(draw3Handle);
        vTaskDelay(delay/portTICK_PERIOD_MS);
        }
}




//Task that resumes/suspends increaseVariable()


void taskSuspendResume(void * pvParameters){  
    bool pressed_i = false, susFlag = false;
    while(1){
        if(DrawSignal){
            if(xSemaphoreTake(DrawSignal, portMAX_DELAY) == pdTRUE){
                xGetButtonInput();
                xSemaphoreTake(ScreenLock, portMAX_DELAY);
                if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) { //standard Mutex lock for pressing the button.
                    if (buttons.buttons[SDL_SCANCODE_I]) { 
                        if(!pressed_i){ 
                            pressed_i = true;
                            if(!susFlag){                         
                                printf("suspending\n");                
                                susFlag = true;                             //flag is true, since the task is now in a suspended state
                                vTaskSuspend(increment);
                            }else{
                                printf("resuming\n");
                                susFlag = false;                    //mark the flag as false, since it gets resumed and is not suspended anymore.
                                vTaskResume(increment);
                            }    
                        }
                    } else {
                        pressed_i = false;
                    }
                    xSemaphoreGive(buttons.lock);
                }  
                xSemaphoreGive(ScreenLock); 
            }
        }                                                      
        
 }
}



/* 

***************task used for drawing the third exercise****************
I thought about removing the tumDraw____ part from the draw task and just to get them into a function and simply call the function every time, but in the demo code almost all of the 
drawing functions are simply sitting in the task function so I decided to leave it as it is. Better safe than sorry.


Most of the code here is already explained in the comments before. I added the tumDrawSetGlobalX(Y)Offset to 0, since the screen got a little bit 'pushed' from the last exercise when 
changing states. 



*/

void drawExercise3(void * p){
    char task1pressed[30];
    char task2pressed[30];
    char var[100];
    int counter = 0;
    while(1){
        sprintf(var, "Increment value : %d", counter);
        if(ulTaskNotifyTake(pdTRUE,0) == 1) counter++;
        if(DrawSignal){
            if (xSemaphoreTake(DrawSignal, portMAX_DELAY) == pdTRUE) {
                xSemaphoreTake(ScreenLock, portMAX_DELAY);
                
                tumDrawSetGlobalXOffset(0);
                tumDrawSetGlobalYOffset(0);
                
                checkDraw(tumDrawClear(White), __FUNCTION__);
                
                if(xSemaphoreTake(LockBool, 0) == pdTRUE){
                    if(start_a){
                        sprintf(task1pressed, "A was pressed %d times", a);
                        checkDraw(tumDrawText(task1pressed, SCREEN_WIDTH/2 - 70, 10, Black), __FUNCTION__);
                        }
                    if(start_d){
                        sprintf(task2pressed, "D was pressed %d times", d);
                        checkDraw(tumDrawText(task2pressed, SCREEN_WIDTH/2 - 70, 30, Black), __FUNCTION__);
                    }
                
                if(check1) checkDraw(tumDrawCircle(SCREEN_WIDTH/4,SCREEN_HEIGHT/2,50,Red), __FUNCTION__);
                if(check2)  checkDraw(tumDrawCircle(3 * SCREEN_WIDTH/4, SCREEN_HEIGHT/2 , 50, Green), __FUNCTION__);

                checkDraw(tumDrawText(var, SCREEN_WIDTH/2 - 70, SCREEN_HEIGHT/2 + 30, Black), __FUNCTION__);
                
                vDrawFPS();
                
                xSemaphoreGive(LockBool);
                xSemaphoreGive(ScreenLock);
                }
            }
        
        }

        vCheckStateInput();
        vTaskDelay(20/portTICK_RATE_MS);
    }
}


/*
***********exercise 4**************

In my implementation, the task used for printing 1 basically controls every other task, which did seem intuitive since it gets called every tick. The other tasks dont have any delay,
they get called with respect to the first task. I set up a variable cnt(counter) which keeps track of the ticks and gets incremented every function call (every tick because of the delay)
All tasks are suspended at the beginning and get suspended at the end of their respective calls, with taskOne being the only task to let them print. Basically task1 is their daddy. 

We also have two global variables, k and output. K is the variable that keeps track of the column at which the values should be added and output is, you guessed it, the output of the 
4th exercise. It is predefined as a matrix of chars and it already has the "Tick number x : " . With the function strncat() i append the appropriate value to the string and every time 
1 gets appended i increment k, since we know that 1 should be at the end of every column. 
In case the tick number gets over 15, we suspend taskOne, which then immediately suspends all other tasks and the printing task is the only that is allowed to run.

I am aware of the fact that the giant wall of tumDraw(...) in the printElements task is ugly to look at. However, I wasn't able to solve it with a for loop which would just iterate
through every value of the output matrix, so I left it as it is.


*/

void taskOne(void * p){
    char toSend = '1';
    int cnt = 1;
    TickType_t LastWakeTime;
    LastWakeTime = xTaskGetTickCount();
    while(1){
        if(cnt % 4 == 0) vTaskResume(print4Handle);
        if(cnt % 2 == 0) vTaskResume(print2Handle);
        if(xSemaphoreTake(wakeThree, 0) == pdTRUE){
            vTaskResume(print3Handle);
        }
        if(xSemaphoreTake(LockOutput, 0) == pdTRUE){
            strncat(output[k], &toSend, 1);
            xSemaphoreGive(LockOutput);
        }
        if(xSemaphoreTake(LockIndex, 0 ) == pdTRUE){
            k++;
            xSemaphoreGive(LockIndex);
        }
        cnt++;
        if(cnt == 16) {
            vTaskSuspend(NULL);
        }
        vTaskDelayUntil(&LastWakeTime,(TickType_t )1);
        LastWakeTime = xTaskGetTickCount();
        
    }
}

void taskTwo(void * p){
    char toSend = '2';
    vTaskSuspend(NULL);
    while(1){
        
        if(xSemaphoreTake(LockOutput, 0) == pdTRUE){
            strncat(output[k], &toSend, 1);
            xSemaphoreGive(LockOutput);
        }
        xSemaphoreGive(wakeThree);
        
        
        vTaskSuspend(NULL);
        
    }
}

void taskThree(void * p){
    char toSend = '3';
    vTaskSuspend(NULL);
    while(1){
        if(xSemaphoreTake(LockOutput, 0) == pdTRUE){
            strncat(output[k], &toSend, 1);
            xSemaphoreGive(LockOutput);
        }
        vTaskSuspend(NULL);
       
    }
}



void taskFour(void * p){
    
    char toSend = '4';
    vTaskSuspend(NULL);
    while(1){
        if(xSemaphoreTake(LockOutput, 0) == pdTRUE){
            strncat(output[k], &toSend, 1);
            xSemaphoreGive(LockOutput);
        }
        vTaskSuspend(NULL);
         
    }
    

}



void printElements(void * p){
    
    signed short text_x = SCREEN_WIDTH/2 - 150;
    signed short text_y = SCREEN_HEIGHT/2 - 150;
    signed short delta = 0;
    TickType_t xLastTick = 1;
    while(1){
        if(DrawSignal){
            if(xSemaphoreTake(DrawSignal, portMAX_DELAY) == pdTRUE){
                xSemaphoreTake(ScreenLock, portMAX_DELAY);
                
                checkDraw(tumDrawClear(White), __FUNCTION__);
                
                
                checkDraw(tumDrawText(output[0], text_x, text_y,  Black), __FUNCTION__);
                checkDraw(tumDrawText(output[1], text_x, text_y + 20, Black), __FUNCTION__);
                checkDraw(tumDrawText(output[2], text_x, text_y + 40, Black), __FUNCTION__);
                checkDraw(tumDrawText(output[3], text_x, text_y + 60, Black), __FUNCTION__);
                checkDraw(tumDrawText(output[4], text_x, text_y + 80, Black), __FUNCTION__);   
                checkDraw(tumDrawText(output[5], text_x, text_y + 100, Black), __FUNCTION__);
                checkDraw(tumDrawText(output[6], text_x, text_y + 120, Black), __FUNCTION__);
                checkDraw(tumDrawText(output[7], text_x, text_y + 140, Black), __FUNCTION__);
                checkDraw(tumDrawText(output[8], text_x, text_y + 160, Black), __FUNCTION__);
                checkDraw(tumDrawText(output[9], text_x, text_y + 180, Black), __FUNCTION__);
                checkDraw(tumDrawText(output[10], text_x, text_y + 200, Black), __FUNCTION__);
                checkDraw(tumDrawText(output[11], text_x, text_y + 220, Black), __FUNCTION__);
                checkDraw(tumDrawText(output[12], text_x, text_y + 240, Black), __FUNCTION__);
                checkDraw(tumDrawText(output[13], text_x, text_y + 260, Black), __FUNCTION__);
                checkDraw(tumDrawText(output[14], text_x, text_y + 280, Black), __FUNCTION__);
                
                
                vDrawFPS();
                
                
                
                xSemaphoreGive(ScreenLock);
                        
            }
        }

        vCheckStateInput();
        vTaskDelayUntil(&xLastTick,(TickType_t)1);
        
        
    }
    
}



    
    
#define PRINT_TASK_ERROR(task) PRINT_ERROR("Failed to print task ##task");





int main(int argc, char *argv[])
{
    char *bin_folder_path = tumUtilGetBinFolderPath(argv[0]);

    prints("Initializing: ");

    //  Note PRINT_ERROR is not thread safe and is only used before the
    //  scheduler is started. There are thread safe print functions in
    //  TUM_Print.h, `prints` and `fprints` that work exactly the same as
    //  `printf` and `fprintf`. So you can read the documentation on these
    //  functions to understand the functionality.

    if (tumDrawInit(bin_folder_path)) {
        PRINT_ERROR("Failed to intialize drawing");
        goto err_init_drawing;
    }
    else {
        prints("drawing");
    }

    if (tumEventInit()) {
        PRINT_ERROR("Failed to initialize events");
        goto err_init_events;
    }
    else {
        prints(", events");
    }

    if (tumSoundInit(bin_folder_path)) {
        PRINT_ERROR("Failed to initialize audio");
        goto err_init_audio;
    }
    else {
        prints(", and audio\n");
    }

    if (safePrintInit()) {
        PRINT_ERROR("Failed to init safe print");
        goto err_init_safe_print;
    }

    logo_image = tumDrawLoadImage(LOGO_FILENAME);

    atexit(aIODeinit);

    //Load a second font for fun
    tumFontLoadFont(FPS_FONT, DEFAULT_FONT_SIZE);

    buttons.lock = xSemaphoreCreateMutex(); // Locking mechanism
    if (!buttons.lock) {
        PRINT_ERROR("Failed to create buttons lock");
        goto err_buttons_lock;
    }

    DrawSignal = xSemaphoreCreateBinary(); // Screen buffer locking
    if (!DrawSignal) {
        PRINT_ERROR("Failed to create draw signal");
        goto err_draw_signal;
    }
    wakeThree = xSemaphoreCreateBinary();
    if(!wakeThree){
        PRINT_ERROR("Failed to create bin semaphore");

    }
    task2Sem = xSemaphoreCreateBinary();
    if(!task2Sem){
        PRINT_ERROR("Failed to create bin semaphore");
    }
    ScreenLock = xSemaphoreCreateMutex();
    if (!ScreenLock) {
        PRINT_ERROR("Failed to create screen lock");
        goto err_screen_lock;
    }

    LockBool = xSemaphoreCreateMutex();
    if(!LockBool){
        PRINT_ERROR("Failed to create bool lock");
    }

    LockIndex = xSemaphoreCreateMutex();
    if(!LockIndex){
        PRINT_ERROR("Failed to create index lock");
    }
    LockOutput = xSemaphoreCreateMutex();
    if(!LockOutput){
        PRINT_ERROR("Failed to create output lock");
    }

    // Message sending
    StateQueue = xQueueCreate(STATE_QUEUE_LENGTH, sizeof(unsigned char));
    if (!StateQueue) {
        PRINT_ERROR("Could not open state queue");
        goto err_state_queue;
    }


    if (xTaskCreate(basicSequentialStateMachine, "StateMachine",
                    mainGENERIC_STACK_SIZE * 2, NULL,
                    configMAX_PRIORITIES - 1, StateMachine) != pdPASS) {
        PRINT_TASK_ERROR("StateMachine");
        goto err_statemachine;
    }
    if (xTaskCreate(vSwapBuffers, "BufferSwapTask",
                    mainGENERIC_STACK_SIZE * 2, NULL, configMAX_PRIORITIES,
                    BufferSwap) != pdPASS) {
        PRINT_TASK_ERROR("BufferSwapTask");
        goto err_bufferswap;
    }

    if(xTaskCreate(taskOne, "task1", mainGENERIC_STACK_SIZE, NULL, 1, &print1Handle) != pdTRUE){
        PRINT_TASK_ERROR("Could not start task for printing 1");
    }
    
    if(xTaskCreate(taskTwo, "task2", mainGENERIC_STACK_SIZE, NULL, 2, &print2Handle) != pdTRUE){
        PRINT_TASK_ERROR("Could not start task for printing 2");
    }
    
    if(xTaskCreate(taskThree, "task3", mainGENERIC_STACK_SIZE, NULL, 3, &print3Handle) != pdTRUE){
        PRINT_TASK_ERROR("Could not start task for printing 3");
    }
    
    if(xTaskCreate(taskFour, "task4", mainGENERIC_STACK_SIZE, NULL, 4, &print4Handle) != pdTRUE){
        PRINT_TASK_ERROR("Could not start task for printing 4");
    }

    if(xTaskCreate(printElements, "printElements", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &printElementsHandle) != pdTRUE){
        PRINT_TASK_ERROR("Could not start task for printing elements in the 4th exercise");
    }


    if(xTaskCreate(exercise2Task, "exercise2", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &exercise2Handle) != pdTRUE){
        PRINT_TASK_ERROR("Could not start task for drawing in the 3rd exercise");
    }


    if(xTaskCreate(drawExercise3, "draw3rdExercise", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &draw3Handle) != pdTRUE){
        PRINT_TASK_ERROR("Could not start task for drawing in the 3rd exercise");
    }

    if(xTaskCreate(circle1, "Circle1", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &flag1Handle) != pdTRUE){
        PRINT_TASK_ERROR("Could not start task for sending flag for circles in 3.2.2");
    }

    flag2Handle = xTaskCreateStatic(circle2,"Circle2",STACK_SIZE,( void * ) 1, tskIDLE_PRIORITY, xStack, &xTaskBuffer);

    if(xTaskCreate(task1, "task1 3.2.3", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &task1Handle) != pdTRUE){
        PRINT_TASK_ERROR("Could not start task in 3.2.3");  
    }
    if(xTaskCreate(task2, "task2 3.2.3", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &task2Handle) != pdTRUE){
        PRINT_TASK_ERROR("Could not start task in 3.2.3");
    }
    if(xTaskCreate(taskNotif, "taskNotif", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &taskNotifHandle) != pdTRUE){
        PRINT_TASK_ERROR("Could not start task in 3.2.3");
    }
    myTimer = xTimerCreate("My timer", pdMS_TO_TICKS(15000), pdTRUE, (void * ) 0, vTimerCallback);
    if(xTimerStart(myTimer, 0) == pdFAIL){
        printf("Couldnt create timer");
    }

    if(xTaskCreate(taskSuspendResume, "taskSusRes", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &susres) != pdTRUE){
        PRINT_TASK_ERROR("Could not start task in 3.2.3");
    }

    if(xTaskCreate(increaseVariable, "taskSusRes", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &increment) != pdTRUE){
        PRINT_TASK_ERROR("Could not start task in 3.2.3");
    }
    
    



    tumFUtilPrintTaskStateList();

    vTaskStartScheduler();

    return EXIT_SUCCESS;


err_bufferswap:
    vTaskDelete(StateMachine);
err_statemachine:
    vQueueDelete(StateQueue);
err_state_queue:
    vSemaphoreDelete(ScreenLock);
err_screen_lock:
    vSemaphoreDelete(DrawSignal);
err_draw_signal:
    vSemaphoreDelete(buttons.lock);
err_buttons_lock:
    tumSoundExit();
err_init_audio:
    tumEventExit();
err_init_events:
    tumDrawExit();
err_init_drawing:
    safePrintExit();
err_init_safe_print:
    return EXIT_FAILURE;
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vMainQueueSendPassed(void)
{
    /* This is just an example implementation of the "queue send" trace hook. */
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vApplicationIdleHook(void)
{
#ifdef __GCC_POSIX__
    struct timespec xTimeToSleep, xTimeSlept;
    /* Makes the process more agreeable when using the Posix simulator. */
    xTimeToSleep.tv_sec = 1;
    xTimeToSleep.tv_nsec = 0;
    nanosleep(&xTimeToSleep, &xTimeSlept);
#endif
}
