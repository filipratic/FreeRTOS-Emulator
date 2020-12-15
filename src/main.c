#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>


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
#define STATE_COUNT 2

#define STATE_ONE 0
#define STATE_TWO 1

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

static char *mq_one_name = "FreeRTOS_MQ_one_1";
static char *mq_two_name = "FreeRTOS_MQ_two_1";
aIO_handle_t mq_one = NULL;
aIO_handle_t mq_two = NULL;
aIO_handle_t udp_soc_one = NULL;
aIO_handle_t udp_soc_two = NULL;
aIO_handle_t tcp_soc = NULL;

const unsigned char next_state_signal = NEXT_TASK;
const unsigned char prev_state_signal = PREV_TASK;

static TaskHandle_t StateMachine = NULL;
static TaskHandle_t BufferSwap = NULL;
static TaskHandle_t DemoTask1 = NULL;
static TaskHandle_t DemoTask2 = NULL;


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
                    if (DemoTask2) {
                        vTaskSuspend(DemoTask2);
                    }
                    if (DemoTask1) {
                        vTaskResume(DemoTask1);
                    }
                    break;
                case STATE_TWO:
                    if (DemoTask1) {
                        vTaskSuspend(DemoTask1);
                    }
                    if (DemoTask2) {
                        vTaskResume(DemoTask2);
                    }
                    break;
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
    const TickType_t frameratePeriod = 20;

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

void vDrawCaveBoundingBox(void)
{
    checkDraw(tumDrawFilledBox(CAVE_X - CAVE_THICKNESS,
                               CAVE_Y - CAVE_THICKNESS,
                               CAVE_SIZE_X + CAVE_THICKNESS * 2,
                               CAVE_SIZE_Y + CAVE_THICKNESS * 2, TUMBlue),
              __FUNCTION__);

    checkDraw(tumDrawFilledBox(CAVE_X, CAVE_Y, CAVE_SIZE_X, CAVE_SIZE_Y,
                               Aqua),
              __FUNCTION__);
}

void vDrawCave(unsigned char ball_color_inverted)
{
    static unsigned short circlePositionX, circlePositionY;

    vDrawCaveBoundingBox();

    circlePositionX = CAVE_X + tumEventGetMouseX() / 2;
    circlePositionY = CAVE_Y + tumEventGetMouseY() / 2;

    if (ball_color_inverted)
        checkDraw(tumDrawCircle(circlePositionX, circlePositionY, 20,
                                Black),
                  __FUNCTION__);
    else
        checkDraw(tumDrawCircle(circlePositionX, circlePositionY, 20,
                                Silver),
                  __FUNCTION__);
}

void vDrawHelpText(void)
{
    static char str[100] = { 0 };
    static int text_width;
    ssize_t prev_font_size = tumFontGetCurFontSize();

    tumFontSetSize((ssize_t)30);

    sprintf(str, "[Q]uit, [C]hange State");

    if (!tumGetTextSize((char *)str, &text_width, NULL))
        checkDraw(tumDrawText(str, SCREEN_WIDTH - text_width - 10,
                              DEFAULT_FONT_SIZE * 0.5, Black),
                  __FUNCTION__);

    tumFontSetSize(prev_font_size);
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

void vDrawLogo(void)
{
    static int image_height;

    if ((image_height = tumDrawGetLoadedImageHeight(logo_image)) != -1)
        checkDraw(tumDrawLoadedImage(logo_image, 10,
                                     SCREEN_HEIGHT - 10 - image_height),
                  __FUNCTION__);
    else {
        fprints(stderr,
                "Failed to get size of image '%s', does it exist?\n",
                LOGO_FILENAME);
    }
}

void vDrawStaticItems(void)
{
    vDrawHelpText();
    vDrawLogo();
}

void vDrawButtonText(void)
{
    static char str[100] = { 0 };

    sprintf(str, "Axis 1: %5d | Axis 2: %5d", tumEventGetMouseX(),
            tumEventGetMouseY());

    checkDraw(tumDrawText(str, 10, DEFAULT_FONT_SIZE * 0.5, Black),
              __FUNCTION__);

    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        sprintf(str, "W: %d | S: %d | A: %d | D: %d",
                buttons.buttons[KEYCODE(W)],
                buttons.buttons[KEYCODE(S)],
                buttons.buttons[KEYCODE(A)],
                buttons.buttons[KEYCODE(D)]);
        xSemaphoreGive(buttons.lock);
        checkDraw(tumDrawText(str, 10, DEFAULT_FONT_SIZE * 2, Black),
                  __FUNCTION__);
    }

    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        sprintf(str, "UP: %d | DOWN: %d | LEFT: %d | RIGHT: %d",
                buttons.buttons[KEYCODE(UP)],
                buttons.buttons[KEYCODE(DOWN)],
                buttons.buttons[KEYCODE(LEFT)],
                buttons.buttons[KEYCODE(RIGHT)]);
        xSemaphoreGive(buttons.lock);
        checkDraw(tumDrawText(str, 10, DEFAULT_FONT_SIZE * 3.5, Black),
                  __FUNCTION__);
    }
}

static int vCheckStateInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(C)]) {
            buttons.buttons[KEYCODE(C)] = 0;
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

void UDPHandlerOne(size_t read_size, char *buffer, void *args)
{
    prints("UDP Recv in first handler: %s\n", buffer);
}

void UDPHandlerTwo(size_t read_size, char *buffer, void *args)
{
    prints("UDP Recv in second handler: %s\n", buffer);
}

void vUDPDemoTask(void *pvParameters)
{
    char *addr = NULL; // Loopback
    in_port_t port = UDP_TEST_PORT_1;

    udp_soc_one = aIOOpenUDPSocket(addr, port, UDP_BUFFER_SIZE,
                                   UDPHandlerOne, NULL);

    prints("UDP socket opened on port %d\n", port);
    prints("Demo UDP Socket can be tested using\n");
    prints("*** netcat -vv localhost %d -u ***\n", port);

    port = UDP_TEST_PORT_2;

    udp_soc_two = aIOOpenUDPSocket(addr, port, UDP_BUFFER_SIZE,
                                   UDPHandlerTwo, NULL);

    prints("UDP socket opened on port %d\n", port);
    prints("Demo UDP Socket can be tested using\n");
    prints("*** netcat -vv localhost %d -u ***\n", port);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void MQHandlerOne(size_t read_size, char *buffer, void *args)
{
    prints("MQ Recv in first handler: %s\n", buffer);
}

void MQHanderTwo(size_t read_size, char *buffer, void *args)
{
    prints("MQ Recv in second handler: %s\n", buffer);
}

void vDemoSendTask(void *pvParameters)
{
    static char *test_str_1 = "UDP test 1";
    static char *test_str_2 = "UDP test 2";
    static char *test_str_3 = "TCP test";

    while (1) {
        prints("*****TICK******\n");
        if (mq_one) {
            aIOMessageQueuePut(mq_one_name, "Hello MQ one");
        }
        if (mq_two) {
            aIOMessageQueuePut(mq_two_name, "Hello MQ two");
        }

        if (udp_soc_one)
            aIOSocketPut(UDP, NULL, UDP_TEST_PORT_1, test_str_1,
                         strlen(test_str_1));
        if (udp_soc_two)
            aIOSocketPut(UDP, NULL, UDP_TEST_PORT_2, test_str_2,
                         strlen(test_str_2));
        if (tcp_soc)
            aIOSocketPut(TCP, NULL, TCP_TEST_PORT, test_str_3,
                         strlen(test_str_3));

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vMQDemoTask(void *pvParameters)
{
    mq_one = aIOOpenMessageQueue(mq_one_name, MSG_QUEUE_MAX_MSG_COUNT,
                                 MSG_QUEUE_BUFFER_SIZE, MQHandlerOne, NULL);
    mq_two = aIOOpenMessageQueue(mq_two_name, MSG_QUEUE_MAX_MSG_COUNT,
                                 MSG_QUEUE_BUFFER_SIZE, MQHanderTwo, NULL);

    while (1)

    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void TCPHandler(size_t read_size, char *buffer, void *args)
{
    prints("TCP Recv: %s\n", buffer);
}

void vTCPDemoTask(void *pvParameters)
{
    char *addr = NULL; // Loopback
    in_port_t port = TCP_TEST_PORT;

    tcp_soc =
        aIOOpenTCPSocket(addr, port, TCP_BUFFER_SIZE, TCPHandler, NULL);

    prints("TCP socket opened on port %d\n", port);
    prints("Demo TCP socket can be tested using\n");
    prints("*** netcat -vv localhost %d ***\n", port);

    while (1) {
        vTaskDelay(10);
    }
}

void vDemoTask1(void *pvParameters)
{
    image_handle_t ball_spritesheet =
        tumDrawLoadImage("../resources/images/ball_spritesheet.png");
    animation_handle_t ball_animation =
        tumDrawAnimationCreate(ball_spritesheet, 25, 1);
    tumDrawAnimationAddSequence(ball_animation, "FORWARDS", 0, 0,
                                SPRITE_SEQUENCE_HORIZONTAL_POS, 24);
    tumDrawAnimationAddSequence(ball_animation, "REVERSE", 0, 23,
                                SPRITE_SEQUENCE_HORIZONTAL_NEG, 24);
    sequence_handle_t forward_sequence =
        tumDrawAnimationSequenceInstantiate(ball_animation, "FORWARDS",
                                            40);
    sequence_handle_t reverse_sequence =
        tumDrawAnimationSequenceInstantiate(ball_animation, "REVERSE",
                                            40);
    TickType_t xLastFrameTime = xTaskGetTickCount();

    while (1) {
        if (DrawSignal)
            if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
                pdTRUE) {
                tumEventFetchEvents(FETCH_EVENT_BLOCK |
                                    FETCH_EVENT_NO_GL_CHECK);
                xGetButtonInput(); // Update global input

                xSemaphoreTake(ScreenLock, portMAX_DELAY);

                // Clear screen
                checkDraw(tumDrawClear(White), __FUNCTION__);
                vDrawStaticItems();
                vDrawCave(tumEventGetMouseLeft());
                vDrawButtonText();
                tumDrawAnimationDrawFrame(forward_sequence,
                                          xTaskGetTickCount() -
                                          xLastFrameTime,
                                          SCREEN_WIDTH - 50, SCREEN_HEIGHT - 60);
                tumDrawAnimationDrawFrame(reverse_sequence,
                                          xTaskGetTickCount() -
                                          xLastFrameTime,
                                          SCREEN_WIDTH - 50 - 40, SCREEN_HEIGHT - 60);
                xLastFrameTime = xTaskGetTickCount();

                // Draw FPS in lower right corner
                vDrawFPS();

                xSemaphoreGive(ScreenLock);

                // Get input and check for state change
                vCheckStateInput();
            }
    }
}

void playBallSound(void *args)
{
    tumSoundPlaySample(a3);
}

void vDemoTask2(void *pvParameters)
{
    TickType_t xLastWakeTime, prevWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    prevWakeTime = xLastWakeTime;

    ball_t *my_ball = createBall(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, Black,
                                 20, 1000, &playBallSound, NULL);
    setBallSpeed(my_ball, 250, 250, 0, SET_BALL_SPEED_AXES);

    // Left wall
    wall_t *left_wall =
        createWall(CAVE_X - CAVE_THICKNESS, CAVE_Y, CAVE_THICKNESS,
                   CAVE_SIZE_Y, 0.2, Red, NULL, NULL);
    // Right wall
    wall_t *right_wall =
        createWall(CAVE_X + CAVE_SIZE_X, CAVE_Y, CAVE_THICKNESS,
                   CAVE_SIZE_Y, 0.2, Red, NULL, NULL);
    // Top wall
    wall_t *top_wall =
        createWall(CAVE_X - CAVE_THICKNESS, CAVE_Y - CAVE_THICKNESS,
                   CAVE_SIZE_X + CAVE_THICKNESS * 2, CAVE_THICKNESS,
                   0.2, Blue, NULL, NULL);
    // Bottom wall
    wall_t *bottom_wall =
        createWall(CAVE_X - CAVE_THICKNESS, CAVE_Y + CAVE_SIZE_Y,
                   CAVE_SIZE_X + CAVE_THICKNESS * 2, CAVE_THICKNESS,
                   0.2, Blue, NULL, NULL);
    unsigned char collisions = 0;

    prints("Task 1 init'd\n");

    while (1) {
        if (DrawSignal)
            if (xSemaphoreTake(DrawSignal, portMAX_DELAY) ==
                pdTRUE) {
                xLastWakeTime = xTaskGetTickCount();

                xGetButtonInput(); // Update global button data

                xSemaphoreTake(ScreenLock, portMAX_DELAY);
                // Clear screen
                checkDraw(tumDrawClear(White), __FUNCTION__);

                vDrawStaticItems();

                // Draw the walls
                checkDraw(tumDrawFilledBox(
                              left_wall->x1, left_wall->y1,
                              left_wall->w, left_wall->h,
                              left_wall->colour),
                          __FUNCTION__);
                checkDraw(tumDrawFilledBox(right_wall->x1,
                                           right_wall->y1,
                                           right_wall->w,
                                           right_wall->h,
                                           right_wall->colour),
                          __FUNCTION__);
                checkDraw(tumDrawFilledBox(
                              top_wall->x1, top_wall->y1,
                              top_wall->w, top_wall->h,
                              top_wall->colour),
                          __FUNCTION__);
                checkDraw(tumDrawFilledBox(bottom_wall->x1,
                                           bottom_wall->y1,
                                           bottom_wall->w,
                                           bottom_wall->h,
                                           bottom_wall->colour),
                          __FUNCTION__);

                // Check if ball has made a collision
                collisions = checkBallCollisions(my_ball, NULL,
                                                 NULL);
                if (collisions) {
                    prints("Collision\n");
                }

                // Update the balls position now that possible collisions have
                // updated its speeds
                updateBallPosition(
                    my_ball, xLastWakeTime - prevWakeTime);

                // Draw the ball
                checkDraw(tumDrawCircle(my_ball->x, my_ball->y,
                                        my_ball->radius,
                                        my_ball->colour),
                          __FUNCTION__);

                // Draw FPS in lower right corner
                vDrawFPS();

                xSemaphoreGive(ScreenLock);

                // Check for state change
                vCheckStateInput();

                // Keep track of when task last ran so that you know how many ticks
                //(in our case miliseconds) have passed so that the balls position
                // can be updated appropriatley
                prevWakeTime = xLastWakeTime;
            }
    }
}

TaskHandle_t exercise2Handle = NULL;

TaskHandle_t print1Handle = NULL, print2Handle = NULL, print3Handle = NULL, print4Handle = NULL, printElementsHandle = NULL; // Exercise 4 
SemaphoreHandle_t wakeThree = NULL, wakeFour = NULL, LockBool = NULL;
QueueHandle_t printQ = NULL;


TaskHandle_t draw3Handle = NULL;          // used for drawing the third exercise

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
    signed short reset = 430;

    bool flag;  // Boolean variable to check if the string got to the end of the path it's supposed to move in 


        
    signed short mouse_x, mouse_y;
    
    float firstTime = (float)clock()/CLOCKS_PER_SEC;
    
    

    // Needed such that Gfx library knows which thread controlls drawing
    // Only one thread can call tumDrawUpdateScreen while and thread can call
    // the drawing functions to draw objects. This is a limitation of the SDL
    // backend.

    bool pressed_a = false, pressed_b = false, pressed_c = false, pressed_d = false, pressed_LMB = false, pressed_RMB = false, pressed_MMB = false;

    tumDrawBindThread();

    while (1) {
        tumEventFetchEvents(FETCH_EVENT_NONBLOCK); // Query events backend for new events, ie. button presses
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

        tumDrawClear(White); // Clear screen

        mouse_x = tumEventGetMouseX();
        mouse_y = tumEventGetMouseY();
       
        tumDrawCircle(circle_x, circle_y, 40, TUMBlue); // Circle
        tumDrawTriangle(trianglecoords, Red); // Triangle
        tumDrawFilledBox(box_x, box_y, 60, 60, Purple); //Box
        tumDrawText(bottom_string, SCREEN_WIDTH/2 - 50, SCREEN_HEIGHT - 30, Black);
        tumDrawText(top_string, string_x, string_y, Black);
        
        //Moving the text from one end to another
        
        if(string_x == start) flag = true;                   
        else if(string_x == reset) flag = false;
        if(flag) string_x++;
        else string_x--;


        //Code for using the Semaphore for pressing buttons on the keyboard. Copied from the original clean branch. 
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

        
        //printing out the values of keyboard and mouse on screen


        sprintf(abString,"A was pressed: %d times || B was pressed: %d times", a, b);
        tumDrawText(abString, 5, 5, Black);
        sprintf(cdString,"C was pressed: %d times || D was pressed: %d times", c, d);
        tumDrawText(cdString, 5, 20, Black);
        sprintf(mouseLocation, "Mouse X Coordinate: %d || Mouse Y Coordinate: %d", mouse_x, mouse_y);
        tumDrawText(mouseLocation, 5, 40, Black);
        sprintf(mouseClick, "Mouse LMB: %d || Mouse RMB: %d || Mouse MMB: %d", mouse_left, mouse_right, mouse_middle);
        tumDrawText(mouseClick, 5, 60, Black); 
        
        
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
       

        
        



        tumDrawUpdateScreen();
        // Chose 80 milliseconds because it seemed to be rather smooth
        vTaskDelay((TickType_t)50);
    }
}



/*
void taskOne(void * p){
    char test[20] = "First digit is: ";
    test[17] = 'a';
    char toSend = '1';
    TickType_t LastWakeTime;
    LastWakeTime = xTaskGetTickCount();
    while(1){
        /*
        xQueueSend(printQ, &toSend, 0);        
        vTaskResume(print2Handle);

        if(xSemaphoreTake(wakeThree, 0) == pdTRUE){
            vTaskResume(print3Handle);
        }

        
        vTaskDelayUntil(&LastWakeTime,(TickType_t )1);
        LastWakeTime = xTaskGetTickCount();
        
       printf("%s\n", test);
       vTaskDelay(1000);
    }
}

void taskTwo(void * p){
    TickType_t LastWakeTime = xTaskGetTickCount();
    char toSend = '2';
    vTaskSuspend(NULL);
    while(1){
        
       
        xQueueSend(printQ, &toSend, 0);
        xSemaphoreGive(wakeThree);
        vTaskDelayUntil(&LastWakeTime,(TickType_t )2);
        
        LastWakeTime = xTaskGetTickCount();
        vTaskSuspend(NULL);
    }
}

void taskThree(void * p){
    char toSend = '3';
    vTaskSuspend(NULL);
    while(1){
        
        xQueueSend(printQ, &toSend, 0);
        vTaskResume(print4Handle);
        vTaskSuspend(NULL);
    }
}



void taskFour(void * p){
    TickType_t xLastWakeTime = xTaskGetTickCount();
    char toSend = '4';
    vTaskSuspend(NULL);
    while(1){
            
        xQueueSend(printQ, &toSend, 0);
        vTaskDelayUntil(&xLastWakeTime, 4);
        vTaskSuspend(NULL);
    }
    

}
*/
/*
void printElements(void * p){
    char element[4];
    char output[15][25];
    signed short text_x = SCREEN_WIDTH/2 - 150;
    signed short text_y = SCREEN_HEIGHT/2 - 150;
    
    TickType_t xLastTick = xTaskGetTickCount();
    int cnt = 0;
    while(1){
        for(int i = 0; i < 4; i++){
            xQueueReceive(printQ, &element[i], 0);
        }
        if(cnt != 15){
            sprintf(output[cnt], "Tick number: %d : %s", cnt + 1, element);
            printf("%s\n", output[cnt]);
            cnt++;

        }else {
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

        }
        vTaskDelayUntil(&xLastTick,(TickType_t)1);
        
        
    }
    
}


*/

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
    configASSERT( ( uint32_t ) pvParameters == 1UL );
    
    while(1){
        if(xSemaphoreTake(LockBool,0) == pdTRUE){
            check2 = !check2;
            xSemaphoreGive(LockBool);
        }

        vTaskDelayUntil(&xLastTick, period/2);
    }
}









void vTimerCallback(TimerHandle_t xTimer){
    if(xSemaphoreTake(buttons.lock, 0) == pdTRUE){
        a = 0;
        d = 0;
        xSemaphoreGive(buttons.lock);
    }
}


/*
void drawTask(void * p){
    char task1pressed[30];
    char task2pressed[30];
    while(1){
        if(DrawSignal){
            if(xSemaphoreTake(DrawSignal, portMAX_DELAY) == pdTRUE){
                xSemaphoreTake(ScreenLock, portMAX_DELAY);
                checkDraw(tumDrawClear(White), __FUNCTION__);
                if(xSemaphoreTake(LockBool, 0) == pdTRUE){
                    if(start_a){
                        sprintf(task1pressed, "A was pressed : %d times", a);
                        checkDraw(tumDrawText(task1pressed, SCREEN_WIDTH/2 - 30, SCREEN_HEIGHT - 30, Black), __FUNCTION__);
                        }
                    if(start_d){
                        sprintf(task2pressed, "D was pressed %d times.", d);
                        checkDraw(tumDrawText(task2pressed, SCREEN_WIDTH/2, SCREEN_HEIGHT/2, Black), __FUNCTION__);
                    }
                vDrawFPS();
                xSemaphoreGive(LockBool);  
                }   
                xSemaphoreGive(ScreenLock);
            }  
        }
    vTaskDelay(50/portTICK_PERIOD_MS);
    }
}

*/

void task1(void * p){
    while(1){
        if(ulTaskNotifyTake(pdTRUE, 0) == pdTRUE){
            start_a = true;
            a++;
        } 
        
        
        vTaskDelay(100/portTICK_PERIOD_MS);          
    }
}


void task2(void * p){
    
    while(1){
        
        if(xSemaphoreTake(task2Sem, 0) == pdTRUE){
            start_d = true;
            d++;
            
        }   
        vTaskDelay(100/portTICK_PERIOD_MS);
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
        vTaskDelay(100/portTICK_PERIOD_MS);
        }
               
}







void increaseVariable(void * pvParameters){
    TickType_t delay = 1000;
    while(1){
        xTaskNotifyGive(draw3Handle);
        vTaskDelay(delay/portTICK_PERIOD_MS);
        }
}




//Task that resumes/suspends increaseVariable()
//pressed_s and pressed_r are used for debouncing. if it wasn't pressed, it can be pressed.
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
        }                                                      //True == suspended
        
 }
}



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
                checkDraw(tumDrawClear(White), __FUNCTION__);
                if(xSemaphoreTake(LockBool, 0) == pdTRUE){
                    if(start_a){
                        sprintf(task1pressed, "A was pressed %d times", a);
                        checkDraw(tumDrawText(task1pressed, SCREEN_WIDTH/2 - 70, 10, Black), __FUNCTION__);
                        }
                    if(start_d){
                        sprintf(task2pressed, "D was pressed %d times.", d);
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

    // Message sending
    StateQueue = xQueueCreate(STATE_QUEUE_LENGTH, sizeof(unsigned char));
    if (!StateQueue) {
        PRINT_ERROR("Could not open state queue");
        goto err_state_queue;
    }

    printQ = xQueueCreate(4, sizeof( int ));
    if(!printQ){
        PRINT_ERROR("Could not open queue for printing task 4");
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
/*
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
    */

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

err_demotask2:
    vTaskDelete(DemoTask1);
err_demotask1:
    vTaskDelete(BufferSwap);
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
