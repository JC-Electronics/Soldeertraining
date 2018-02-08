/*
 * controls.c
 *
 *  Created on: 13.04.2017
 *      Author: michaelboeckling
 */

#define Rotary_Encoder_Pin_A  16
#define Rotary_Encoder_Pin_B  17
#define Rotary_Encoder_Button 5

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "controls.h"
int cnt = 0;

#define TAG "ENCODER"

static xQueueHandle gpio_evt_queue = NULL;
static TaskHandle_t *gpio_task;
#define ESP_INTR_FLAG_DEFAULT 0

/* gpio event handler */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t gpio_num = (uint32_t) arg;

    xQueueSendToBackFromISR(gpio_evt_queue, &gpio_num, &xHigherPriorityTaskWoken);

    if(xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static void Rotary_Encoder(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(Rotary_Encoder_Pin_A));
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(Rotary_Encoder_Pin_B));
		if ((gpio_get_level(Rotary_Encoder_Pin_A) == 1)&&(gpio_get_level(Rotary_Encoder_Pin_B)== 0 )&&(cnt >= 0 && cnt <= 99)){
			cnt++ ;
			ESP_LOGI(TAG, "Encoder UP!");
		}
		if ((gpio_get_level(Rotary_Encoder_Pin_A) ==  1 )&&(gpio_get_level(Rotary_Encoder_Pin_B)== 1 )&&(cnt >= 1 && cnt <= 100))
		{
			cnt-- ;
			ESP_LOGI(TAG, "Encoder DOWN!");
		}
	}
}
    
}


void controls_init(TaskFunction_t gpio_handler_task, const uint16_t usStackDepth, void *user_data)
{
    gpio_config_t io_conf;
    //interrupt of rising edge
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    //bit mask of the pins, use GPIO0 here ("Boot" button)
    io_conf.pin_bit_mask = (1 << Rotary_Encoder_Button);
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(2, sizeof(uint32_t));
    gpio_handler_param_t *params = calloc(1, sizeof(gpio_handler_param_t));
    params->gpio_evt_queue = gpio_evt_queue;
    params->user_data = user_data;
    //start gpio task
    xTaskCreatePinnedToCore(gpio_handler_task, "gpio_handler_task", usStackDepth, params, 10, gpio_task, 0);
    xTaskCreate(Rotary_Encoder, "Rotary_Encoder", 2048, NULL, 10, NULL);
    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    // remove existing handler that may be present
    gpio_isr_handler_remove(Rotary_Encoder_Button);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(Rotary_Encoder_Button, gpio_isr_handler, (void*) Rotary_Encoder_Button);
    gpio_set_intr_type(Rotary_Encoder_Pin_A, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(Rotary_Encoder_Pin_A, gpio_isr_handler, (void*) Rotary_Encoder_Pin_A);
    gpio_isr_handler_remove(Rotary_Encoder_Pin_A);
}

void controls_destroy()
{
    gpio_isr_handler_remove(Rotary_Encoder_Button);
    vTaskDelete(gpio_task);
    vQueueDelete(gpio_evt_queue);
    // TODO: free gpio_handler_param_t params
}
