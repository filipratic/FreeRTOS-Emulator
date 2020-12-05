#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include <stdbool.h>

#include <SDL2/SDL_scancode.h>


#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "TUM_Ball.h"
#include "TUM_Draw.h"
#include "TUM_Event.h"
#include "TUM_Sound.h"
#include "TUM_Utils.h"
#include "TUM_Font.h"

#include "AsyncIO.h"

#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)

#define STACK_SIZE 200

StaticTask_t xTaskBuffer;

StackType_t xStack[ STACK_SIZE ];


typedef struct buttons_buffer {
    unsigned char buttons[SDL_NUM_SCANCODES];
    SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };

void xGetButtonInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
        xSemaphoreGive(buttons.lock);
    }
}

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

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


void vDemoTask(void *pvParameters)
{
    // structure to store time retrieved from Linux kernel
    static struct timespec the_time;
    static char our_time_string[100];
    static int our_time_strings_width = 0;

    // Needed such that Gfx library knows which thread controlls drawing
    // Only one thread can call tumDrawUpdateScreen while and thread can call
    // the drawing functions to draw objects. This is a limitation of the SDL
    // backend.
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

        clock_gettime(CLOCK_REALTIME,
                      &the_time); // Get kernel real time

        // Format our string into our char array
        sprintf(our_time_string,
                "There has been %ld seconds since the Epoch. Press Q to quit",
                (long int)the_time.tv_sec);

        // Get the width of the string on the screen so we can center it
        // Returns 0 if width was successfully obtained
        if (!tumGetTextSize((char *)our_time_string,
                            &our_time_strings_width, NULL))
            tumDrawText(our_time_string,
                        SCREEN_WIDTH / 2 -
                        our_time_strings_width / 2,
                        SCREEN_HEIGHT / 2 - DEFAULT_FONT_SIZE / 2,
                        TUMBlue);

        tumDrawUpdateScreen(); // Refresh the screen to draw string

        // Basic sleep of 1000 milliseconds
        vTaskDelay((TickType_t)1000);
    }
}



TaskHandle_t drawCircleHandle = NULL;
TaskHandle_t notif11 = NULL;
TaskHandle_t notif12 = NULL;
TaskHandle_t notif21 = NULL;
TaskHandle_t notif22 = NULL;
TaskHandle_t increment = NULL;
bool notif1, notif2;

void setFlag1True(void * pvParameters){
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 125;

    xLastWakeTime = xTaskGetTickCount();
    
    while(1){
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        notif1 = true;
       
        
    }
    
}

void setFlag1False(void * pvParameters){
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 251;

    xLastWakeTime = xTaskGetTickCount();
    
    while(1){

        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        notif1 = false;
        
        
    
    }
}

void setFlag2True(void * pvParameters){
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 250;

    xLastWakeTime = xTaskGetTickCount();
    
    while(1){

        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        notif2 = true;
        
        
    
    }
}

void setFlag2False(void * pvParameters){
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 501;

    xLastWakeTime = xTaskGetTickCount();
    
    while(1){

        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        notif2 = false;
        
        
    
    }
}


void drawCircle1(void * pvParameters){
    tumDrawBindThread();

    
    while(1){
        tumEventFetchEvents(FETCH_EVENT_NONBLOCK);
        xGetButtonInput();
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(
                                    Q)]) { // Equiv to SDL_SCANCODE_Q
                exit(EXIT_SUCCESS);
            }
            xSemaphoreGive(buttons.lock);
        }
        tumDrawClear(White);
        
        if(notif1) tumDrawCircle(SCREEN_WIDTH/4, SCREEN_HEIGHT/2, 50, Green);
        tumDrawUpdateScreen();        
    
        
    }

    
}


void drawCircle2(void * pvParameters){

    tumDrawBindThread();

    
    while(1){
        tumEventFetchEvents(FETCH_EVENT_NONBLOCK);
        xGetButtonInput();

        configASSERT( ( uint32_t ) pvParameters == 1UL );

        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(
                                    Q)]) { // Equiv to SDL_SCANCODE_Q
                exit(EXIT_SUCCESS);
            }
            xSemaphoreGive(buttons.lock);
        }
        tumDrawClear(White);
        
        if(notif2) tumDrawCircle(SCREEN_WIDTH/4, SCREEN_HEIGHT/2, 50, Green);
        tumDrawUpdateScreen();        
    
        
    }

}

 void vOtherFunction( void )
    {
        TaskHandle_t xHandle = NULL;

        /* Create the task without using any dynamic memory allocation. */
        xHandle = xTaskCreateStatic(
                      drawCircle2,       /* Function that implements the task. */
                      "RedCirlce",          /* Text name for the task. */
                      STACK_SIZE,      /* Number of indexes in the xStack array. */
                      ( void * ) 1,    /* Parameter passed into the task. */
                      tskIDLE_PRIORITY,/* Priority at which the task is created. */
                      xStack,          /* Array to use as the task's stack. */
                      &xTaskBuffer );  /* Variable to hold the task's data structure. */

        /* puxStackBuffer and pxTaskBuffer were not NULL, so the task will have
        been created, and xHandle will be the task's handle.  Use the handle
        to suspend the task. */
        vTaskSuspend( xHandle );
    }




void increaseVariable(void * pvParameters){
    int a = 0;
    while(1){
        printf("%d\n", a);
        a++;
        vTaskDelay((TickType_t)1000);
    }
}

void vIncrease(){
    xTaskCreate(increaseVariable,"increment", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &increment);
    tumEventFetchEvents(FETCH_EVENT_NONBLOCK); // Query events backend for new events, ie. button presses
    xGetButtonInput();
    if (buttons.buttons[SDL_SCANCODE_I]) { // Equiv to SDL_SCANCODE_Q
        vTaskSuspend(increment);
    }
            
        
        
    

}


int main(int argc, char *argv[])
{
   char *bin_folder_path = tumUtilGetBinFolderPath(argv[0]);

    printf("Initializing: ");

    if (tumDrawInit(bin_folder_path)) {
        PRINT_ERROR("Failed to initialize drawing");
        goto err_init_drawing;
    }

    if (tumEventInit()) {
        PRINT_ERROR("Failed to initialize events");
        goto err_init_events;
    }

    if (tumSoundInit(bin_folder_path)) {
        PRINT_ERROR("Failed to initialize audio");
        goto err_init_audio;
    }

    buttons.lock = xSemaphoreCreateMutex(); // Locking mechanism
    if (!buttons.lock) {
        PRINT_ERROR("Failed to create buttons lock");
        goto err_buttons_lock;
    }
      /*
    xTaskCreate(setFlag1True,"flag1true", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &notif11);
    xTaskCreate(setFlag1False,"flag1false", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &notif12);
    xTaskCreate(setFlag2True,"flag2false", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &notif21);
    xTaskCreate(setFlag2False,"flag2false", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &notif22);
    xTaskCreate(drawCircle1, "GreenCircle", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &drawCircleHandle);
    //xTaskCreateStatic(drawCircle2, "RedCircle", STACK_SIZE, (void *) 1, tskIDLE_PRIORITY, xStack, &xTaskBuffer);
    //xTaskCreate(test,"test", mainGENERIC_STACK_SIZE, NULL, mainGENERIC_PRIORITY, &testHandle);
    vOtherFunction();
    */
    vIncrease();

    


    vTaskStartScheduler();
 
    return EXIT_SUCCESS;


err_buttons_lock:
    tumSoundExit();
err_init_audio:
    tumEventExit();
err_init_events:
    tumDrawExit();
err_init_drawing:
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
