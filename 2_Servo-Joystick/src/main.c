#include <stdio.h>
#include <string.h>
/*Pico Lib*/
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
/*FreeRTOS Lib*/
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
/*Custom Lib*/
#include "Servo.h"
#include "PWMmgr.h"

#define mainECHO_TASK_PRIORITY	(tskIDLE_PRIORITY + 1)
/*Hardware Setup*/
#define SERVO_PIN		20
#define LED_PIN			PICO_DEFAULT_LED_PIN
#define TEMP_SENS_PIN 	4
#define PWM_PIN			21

/*Add Servo Motor*/
Servo_t servo_1;
PWM_t	pwm_1;

static QueueHandle_t xQueueOut = NULL, xQueueIn = NULL;
static SemaphoreHandle_t h_mutex;

extern void vApplicationStackOverflowHook(TaskHandle_t *pxTask,signed portCHAR *pcTaskName);

void vApplicationStackOverflowHook(TaskHandle_t *pxTask,signed portCHAR *pcTaskName) {
	(void)pxTask;
	(void)pcTaskName;
	for(;;);
}

typedef struct {
	uint16_t pos;
	TickType_t elapsed;
} Message_t;

static void mutex_lock(void) {
	// prevents competing tasks from printing in the middle of our own line of text
	xSemaphoreTake(h_mutex,portMAX_DELAY);
}

static void mutex_unlock(void) {
	// prevents competing tasks from printing in the middle of our own line of text
	xSemaphoreGive(h_mutex);
}

float get_temp(){
	adc_select_input(4);
	uint16_t raw = adc_read();
	
	const float conversion_factor = 3.3f/(1<<12);
	float result = raw*conversion_factor;
	float temp = 27 - (result-0.706)/0.001721;
	return temp;
}

static void input_task(void *args) {
	(void)args;
	char ch_buff = 0;
	uint32_t val = 0;

	for (;;) {
		ch_buff = getchar();
		if(ch_buff == '\n'){
			printf("PWM = %lu\n", val);
			xQueueSend(xQueueIn, &val, 0U);
			val = 0;
		}
			
		if(ch_buff >= '0' && ch_buff <= '9'){
			val = val*10 + ch_buff-'0';
		}
	}
}

static void main_task (void *args) {
	/*It will be changed to ISR*/
	(void)args;
	Message_t send_value;
	uint32_t pos = 50;
	send_value.pos = 0;
	send_value.elapsed = 0;

	for(;;) {
		TickType_t t0 = xTaskGetTickCount();

		/*Receive delay from USB*/
		xQueueReceive(xQueueIn, &pos, 0U);

		/*servo*/
		ServoPosition(&servo_1, pos);
		SetPWM_Duty(&pwm_1, pos);

		/*Send Pos*/
		send_value.pos = pos;

		vTaskDelay(pdMS_TO_TICKS(100));

		send_value.elapsed = xTaskGetTickCount()-t0;
		
		/*Send the time value to USB*/
		xQueueSend(xQueueOut, &send_value, 0U);
	}
}

static void output_task (void *args) {
	(void)args;
	TickType_t t0 = 0;
	Message_t received_value;
	bool out_led = 1;
	float V_PWM = 0, servo_duty = 0, period = 0;

	for (;;) {
		t0 = xTaskGetTickCount();
		xQueueReceive(xQueueOut, &received_value, portMAX_DELAY);
		servo_duty = (received_value.pos* ((float)(MAX_DUTY - MIN_DUTY)/100)+MIN_DUTY)/100;
		period = (10*servo_duty)/(float) SERVO_FREQ;
		V_PWM = (servo_duty * 3.3)/100;

		gpio_put(LED_PIN, out_led);
		out_led = !out_led;
		
		mutex_lock();
		printf("Pos:%d, Duty:%.1f, t:%.2f, V:%.3f\n", 
				received_value.pos, 
				servo_duty,
				period,
				V_PWM);
		mutex_unlock();
		
	}
}

static void GPIO_SETUP_INIT(){
	set_sys_clock_khz(125000, true);
	/*Communication*/
	stdio_init_all();
	
	/*LED*/
	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);
	
	/*ADC*/
	adc_init();
	adc_set_temp_sensor_enabled(true);
	adc_select_input(TEMP_SENS_PIN);
	
	/*Servo*/
	ServoInit(&servo_1, SERVO_PIN, false);
	
	/*PWM*/
	PWMInit(&pwm_1, PWM_PIN, 1000, 72.5, false);
}

int main() {
	GPIO_SETUP_INIT();
	ServoOn(&servo_1);
	PWMOn(&pwm_1);
	
	xQueueOut 	= xQueueCreate(10, sizeof(Message_t));
	xQueueIn  	= xQueueCreate(10, sizeof(uint32_t));
	h_mutex 	= xSemaphoreCreateMutex();
	
	xTaskCreate(main_task,"main_task",400,NULL,configMAX_PRIORITIES-2,NULL);
	xTaskCreate(input_task,"input_task",400,NULL,mainECHO_TASK_PRIORITY,NULL);
	xTaskCreate(output_task,"output_task",400,NULL,mainECHO_TASK_PRIORITY,NULL);
	
	vTaskStartScheduler();
	
    for(;;);
}
