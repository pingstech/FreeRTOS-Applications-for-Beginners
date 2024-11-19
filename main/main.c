/* Standart Library Includes*/
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* Platform Includes */
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"

/* Kernel Includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"


/** @brief: Define Example                                                                                                                                      **/
/**    |            Example name            |  Example number                                                                                                   **/
#define TASK_NOTIFTY_EXAMPLE                          0
#define TASK_QUEUE_EXAMPLE                            1
#define TASK_BINARY_SEMAPHORE_EXAMPLE                 2
#define TASK_COUNTING_SEMAPHORE_EXAMPLE               3
#define TASK_EVENT_GROUP_EXAMPLE                      4

/** @brief Describe your example!                                                                                                                               **/
#define EXAMPLE_SELECT TASK_NOTIFTY_EXAMPLE

/* ----------------------------------------- */
/* ----------------------------------------- */
/* ----------------------------------------- */
/* ----------------------------------------- */

/** @brief: Task Notify Example!                                                                                                                                **/
#if(EXAMPLE_SELECT == TASK_NOTIFTY_EXAMPLE)

/** @brief: ESP Log Header **/
static const char *TAG = "notify_task_example";

/** @brief: ESP UART defines. **/
#define EX_UART_NUM                                 UART_NUM_1
#define PATTERN_CHR_NUM                             (3)         
#define ESP_UART_TX_PIN                             16
#define ESP_UART_RX_PIN                             17
#define XI_BUFF_SIZE                                ((int)2048)
#define RD_BUF_SIZE                                 (XI_BUFF_SIZE)


/** @brief: UART Queue Object Create **/
static QueueHandle_t uart_queue;

/** @brief: FreeRTOS Task Handler Create **/
TaskHandle_t    uart_tx_task_handler;

/** @brief: User defines**/
#define MESSAGE_1 (1 << 0)
#define MESSAGE_2 (1 << 1)
#define MESSAGE_3 (1 << 2)

/**
 * @brief Initializes the UART interface for communication.
 * 
 * This function configures the UART settings such as baud rate, data bits,
 * stop bits, and parity. It prepares the UART interface for communication
 * with the Nextion device.
 * 
 * @return None
 */
void uart_initialization(void) 
{
    uart_config_t uart_config =
    {
        .baud_rate = 115200, /** @brief: this parameters must be 9600 for the communication with Nextion!                                  **/
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, XI_BUFF_SIZE * 2,0, 20, &uart_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);

    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Set UART pins (using UART1 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, ESP_UART_TX_PIN, ESP_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    //Set uart pattern detect function.
    uart_enable_pattern_det_baud_intr(EX_UART_NUM, '+', PATTERN_CHR_NUM, 9, 0, 0);
    //Reset the pattern queue length to record at most 20 pattern positions.
    uart_pattern_queue_reset(EX_UART_NUM, 20);
}


/**
 * @brief UART event handling task.
 * 
 * This function defines a FreeRTOS task that handles UART events such as data
 * transmission and reception. It processes the incoming data from the Nextion
 * device and manages outgoing data based on specific triggers or events.
 * 
 * @param pvParameters   Pointer to task parameters (not used).
 * @return None
 */
void uart_rx_task (void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);

    for (;;) {
        //Waiting for UART event.
        if (xQueueReceive(uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) 
        {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);
            switch (event.type) 
            {
            //Event of UART receving data
            /*We'd better handler data event fast, there would be much more data events than
            other types of events. If we take too much time on data event, the queue might
            be full.*/
            case UART_DATA:
                ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);

                ESP_LOGI(TAG, "[UART RECEIVED DATA]: %s", dtmp);        
                ESP_LOG_BUFFER_HEXDUMP(TAG, dtmp, event.size, ESP_LOG_INFO);

                if(strstr((char *)dtmp,"Send first tx message!"))
                {
                    xTaskNotify(uart_tx_task_handler, MESSAGE_1, eSetBits);
                }

                else if(strstr((char *)dtmp,"Send second tx message!"))
                {
                    xTaskNotify(uart_tx_task_handler, MESSAGE_2, eSetBits);
                }

                else if(strstr((char *)dtmp,"Send third tx message!"))
                {
                    xTaskNotify(uart_tx_task_handler, MESSAGE_3, eSetBits);
                }

                break;
            //Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart_queue);
                break;
            //Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                // If buffer full happened, you should consider increasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart_queue);
                break;
            //Event of UART RX break detected
            case UART_BREAK:
                ESP_LOGI(TAG, "uart rx break");
                break;
            //Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "uart parity error");
                break;
            //Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "uart frame error");
                break;
            //UART_PATTERN_DET
            case UART_PATTERN_DET:
                uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
                int pos = uart_pattern_pop_pos(EX_UART_NUM);
                ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                if (pos == -1) {
                    // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                    // record the position. We should set a larger queue size.
                    // As an example, we directly flush the rx buffer here.
                    uart_flush_input(EX_UART_NUM);
                } else {
                    uart_read_bytes(EX_UART_NUM, dtmp, pos, 100 / portTICK_PERIOD_MS);
                    uint8_t pat[PATTERN_CHR_NUM + 1];
                    memset(pat, 0, sizeof(pat));
                    uart_read_bytes(EX_UART_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                    ESP_LOGI(TAG, "read data: %s", dtmp);
                    ESP_LOGI(TAG, "read pat : %s", pat);
                }
                break;
            //Others
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

void uart_tx_task(void *pvParameters)
{
    uint32_t ul_received_value = 0;

    for(;;)
    {   
        if(xTaskNotifyWait(0, ULONG_MAX, &ul_received_value, portMAX_DELAY) == pdTRUE)
        //                    ^-------^ bu veri eğer her uyandırmada komple sıfırlanmak isteniliyorsa sabit bir değer verilmeli. örneğin: 3 bitlik kontrol için 0x7, 4 bitlik kontrol için 0xF ... gibi devam edecek
        {
            if(ul_received_value & MESSAGE_1)
            {
                ESP_LOGI(TAG, "First TX task wake up with \"xTaskNotifyWait\"!");

                uart_write_bytes(EX_UART_NUM, 
                                 "First Message: Message received with task notify!",
                                 strlen("First Message: Message received with task notify!"));                
            }

            else if(ul_received_value & MESSAGE_2)
            {
                ESP_LOGI(TAG, "Second TX task wake up with \"xTaskNotifyWait\"!");

             uart_write_bytes(EX_UART_NUM, 
                              "Second Message: Message received with task notify!",
                              strlen("Second Message: Message received with task notify!"));
            }

            else if(ul_received_value & MESSAGE_3)
            {
                ESP_LOGI(TAG, "Third TX task wake up with \"xTaskNotifyWait\"!");

                uart_write_bytes(EX_UART_NUM, 
                                 "Third Message: Message received with task notify!",
                                 strlen("Third Message: Message received with task notify!"));
            }
        }      
    }
}

void app_main(void)
{
    uart_initialization();

    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 2, NULL);
    xTaskCreate(uart_tx_task, "uart_tx_task", 4096, NULL, 2, &uart_tx_task_handler);

}
/** @brief END OF TASK NOTIFY EXAMPLE! **/
#endif

/* ----------------------------------------- */
/* ----------------------------------------- */
/* ----------------------------------------- */
/* ----------------------------------------- */

/** @brief: Queue Read & Write Example!                                                                                                                         **/
#if (EXAMPLE_SELECT == TASK_QUEUE_EXAMPLE)

/** @brief: ESP Log Header **/
static const char *TAG = "queue_task_example";

/** @brief: ESP UART defines. **/
#define EX_UART_NUM                                 UART_NUM_1
#define PATTERN_CHR_NUM                             (3)         
#define ESP_UART_TX_PIN                             16
#define ESP_UART_RX_PIN                             17
#define XI_BUFF_SIZE                                ((int)1024)
#define RD_BUF_SIZE                                 (XI_BUFF_SIZE)

/** @brief: UART Queue Object Create **/
static QueueHandle_t uart_queue;

/** @brief: User Queue Length **/
#define USER_QUEUE_SIZE ((int)2)
#define MAX_DATA_LENGTH ((int)1024)

/** @brief: User Queue Create **/
static QueueHandle_t user_queue;

/**
 * @brief Initializes the UART interface for communication.
 * 
 * This function configures the UART settings such as baud rate, data bits,
 * stop bits, and parity. It prepares the UART interface for communication
 * with the Nextion device.
 * 
 * @return None
 */
void uart_initialization(void) 
{
    uart_config_t uart_config =
    {
        .baud_rate = 115200, /** @brief: this parameters must be 9600 for the communication with Nextion!                                  **/
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, XI_BUFF_SIZE * 2,0, 20, &uart_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);

    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Set UART pins (using UART1 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, ESP_UART_TX_PIN, ESP_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    //Set uart pattern detect function.
    uart_enable_pattern_det_baud_intr(EX_UART_NUM, '+', PATTERN_CHR_NUM, 9, 0, 0);
    //Reset the pattern queue length to record at most 20 pattern positions.
    uart_pattern_queue_reset(EX_UART_NUM, 20);
}


/**
 * @brief UART event handling task.
 * 
 * This function defines a FreeRTOS task that handles UART events such as data
 * transmission and reception. It processes the incoming data from the Nextion
 * device and manages outgoing data based on specific triggers or events.
 * 
 * @param pvParameters   Pointer to task parameters (not used).
 * @return None
 */
void uart_rx_task (void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);

    for (;;) {
        //Waiting for UART event.
        if (xQueueReceive(uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) 
        {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);
            switch (event.type) 
            {
            //Event of UART receving data
            /*We'd better handler data event fast, there would be much more data events than
            other types of events. If we take too much time on data event, the queue might
            be full.*/
            case UART_DATA:
                ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);

                ESP_LOGI(TAG, "[UART RECEIVED DATA]: %s", dtmp);        
                ESP_LOG_BUFFER_HEXDUMP(TAG, dtmp, event.size, ESP_LOG_INFO);

                if(xQueueSend(user_queue, (void *)dtmp, 0) == pdTRUE)
                {
                    ESP_LOGI(TAG, "Message writen into queue!");
                }

                break;
            //Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart_queue);
                break;
            //Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                // If buffer full happened, you should consider increasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart_queue);
                break;
            //Event of UART RX break detected
            case UART_BREAK:
                ESP_LOGI(TAG, "uart rx break");
                break;
            //Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "uart parity error");
                break;
            //Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "uart frame error");
                break;
            //UART_PATTERN_DET
            case UART_PATTERN_DET:
                uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
                int pos = uart_pattern_pop_pos(EX_UART_NUM);
                ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                if (pos == -1) {
                    // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                    // record the position. We should set a larger queue size.
                    // As an example, we directly flush the rx buffer here.
                    uart_flush_input(EX_UART_NUM);
                } else {
                    uart_read_bytes(EX_UART_NUM, dtmp, pos, 100 / portTICK_PERIOD_MS);
                    uint8_t pat[PATTERN_CHR_NUM + 1];
                    memset(pat, 0, sizeof(pat));
                    uart_read_bytes(EX_UART_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                    ESP_LOGI(TAG, "read data: %s", dtmp);
                    ESP_LOGI(TAG, "read pat : %s", pat);
                }
                break;
            //Others
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

void uart_tx_task(void *pvParameters)
{
    uint8_t rx_buffer[MAX_DATA_LENGTH]      = {0};
    uint8_t peek_buffer[MAX_DATA_LENGTH]    = {0};

    for(;;)
    {
        if(xQueuePeek(user_queue, &peek_buffer, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGW(TAG, "Peek from queue!");

            if(strstr((char *)peek_buffer, "Read this message!"))
            {                
                xQueueReceive(user_queue, &rx_buffer, portMAX_DELAY);
                ESP_LOGI(TAG, "Read from queue and restored in rx_buffer!");

                uart_write_bytes(EX_UART_NUM, "Message read from queue!", strlen("Message read from queue!"));
            }
            else
            {
                ESP_LOGE(TAG, "Unknown data!");
                ESP_LOGW(TAG, "Data will be remove from queue!");
                xQueueReceive(user_queue, &rx_buffer, portMAX_DELAY);                
            }

            ESP_LOGW(TAG, "%d data waiting in queue!", uxQueueMessagesWaiting(user_queue));
        }
    }
}

void app_main(void)
{
    uart_initialization();
    
    user_queue = xQueueCreate(USER_QUEUE_SIZE, MAX_DATA_LENGTH);

    if(!user_queue) {ESP_LOGE(TAG, "User queue create failed!");}    

    if(xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 2, NULL) == pdPASS)
    {
        ESP_LOGI(TAG, "\"uart_rx_task\" create!");
    }

    if(xTaskCreate(uart_tx_task, "uart_tx_task", 4096, NULL, 2, NULL) == pdPASS)
    {
        ESP_LOGI(TAG, "\"uart_tx_task\" create!");
    }    
}

/** @brief END OF TASK QUEUE EXAMPLE! **/
#endif

/* ----------------------------------------- */
/* ----------------------------------------- */
/* ----------------------------------------- */
/* ----------------------------------------- */

#if(EXAMPLE_SELECT == TASK_BINARY_SEMAPHORE_EXAMPLE)

/** @brief: ESP Log Header **/
static const char *TAG = "binary_semaphore_task_example";

/** @brief: ESP UART defines. **/
#define EX_UART_NUM                                 UART_NUM_1
#define PATTERN_CHR_NUM                             (3)         
#define ESP_UART_TX_PIN                             16
#define ESP_UART_RX_PIN                             17
#define XI_BUFF_SIZE                                ((int)1024)
#define RD_BUF_SIZE                                 (XI_BUFF_SIZE)

/** @brief: UART Queue Object Create **/
static QueueHandle_t uart_queue;

/** @brief: Semaphore Object Create **/
static SemaphoreHandle_t binary_semaphore;

/**
 * @brief Initializes the UART interface for communication.
 * 
 * This function configures the UART settings such as baud rate, data bits,
 * stop bits, and parity. It prepares the UART interface for communication
 * with the Nextion device.
 * 
 * @return None
 */
void uart_initialization(void) 
{
    uart_config_t uart_config =
    {
        .baud_rate = 115200, /** @brief: this parameters must be 9600 for the communication with Nextion!                                  **/
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, XI_BUFF_SIZE * 2,0, 20, &uart_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);

    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Set UART pins (using UART1 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, ESP_UART_TX_PIN, ESP_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    //Set uart pattern detect function.
    uart_enable_pattern_det_baud_intr(EX_UART_NUM, '+', PATTERN_CHR_NUM, 9, 0, 0);
    //Reset the pattern queue length to record at most 20 pattern positions.
    uart_pattern_queue_reset(EX_UART_NUM, 20);
}


/**
 * @brief UART event handling task.
 * 
 * This function defines a FreeRTOS task that handles UART events such as data
 * transmission and reception. It processes the incoming data from the Nextion
 * device and manages outgoing data based on specific triggers or events.
 * 
 * @param pvParameters   Pointer to task parameters (not used).
 * @return None
 */
void uart_rx_task (void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);

    for (;;) {
        //Waiting for UART event.
        if (xQueueReceive(uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) 
        {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);
            switch (event.type) 
            {
            //Event of UART receving data
            /*We'd better handler data event fast, there would be much more data events than
            other types of events. If we take too much time on data event, the queue might
            be full.*/
            case UART_DATA:
                ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);

                ESP_LOGI(TAG, "[UART RECEIVED DATA]: %s", dtmp);        
                ESP_LOG_BUFFER_HEXDUMP(TAG, dtmp, event.size, ESP_LOG_INFO);

                if(strstr((char *)dtmp, "Semaphore Give!"))
                {
                    ESP_LOGW(TAG, "Semaphore Given!");
                    xSemaphoreGive(binary_semaphore);
                }

                break;
            //Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart_queue);
                break;
            //Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                // If buffer full happened, you should consider increasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart_queue);
                break;
            //Event of UART RX break detected
            case UART_BREAK:
                ESP_LOGI(TAG, "uart rx break");
                break;
            //Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "uart parity error");
                break;
            //Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "uart frame error");
                break;
            //UART_PATTERN_DET
            case UART_PATTERN_DET:
                uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
                int pos = uart_pattern_pop_pos(EX_UART_NUM);
                ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                if (pos == -1) {
                    // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                    // record the position. We should set a larger queue size.
                    // As an example, we directly flush the rx buffer here.
                    uart_flush_input(EX_UART_NUM);
                } else {
                    uart_read_bytes(EX_UART_NUM, dtmp, pos, 100 / portTICK_PERIOD_MS);
                    uint8_t pat[PATTERN_CHR_NUM + 1];
                    memset(pat, 0, sizeof(pat));
                    uart_read_bytes(EX_UART_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                    ESP_LOGI(TAG, "read data: %s", dtmp);
                    ESP_LOGI(TAG, "read pat : %s", pat);
                }
                break;
            //Others
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

void uart_tx_task(void *pvParameters)
{
    const char *tx_message = "Semaphore Take!\n";
    for(;;)
    {
        if(xSemaphoreTake(binary_semaphore, portMAX_DELAY))
        {
            ESP_LOGW(TAG, "Semaphore Taken!");
            uart_write_bytes(EX_UART_NUM, tx_message, strlen(tx_message));            
        }
    }
}

void app_main(void)
{
    /** @brief: Create Binary Semaphore **/
    binary_semaphore = xSemaphoreCreateBinary();
    
    uart_initialization();

    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 2, NULL);
    xTaskCreate(uart_tx_task, "uart_tx_task", 4096, NULL, 2, NULL);
}

/** @brief END OF TASK BINARY SEMAPHORE EXAMPLE! **/

#endif

/* ----------------------------------------- */
/* ----------------------------------------- */
/* ----------------------------------------- */
/* ----------------------------------------- */

#if(EXAMPLE_SELECT == TASK_COUNTING_SEMAPHORE_EXAMPLE)

/** @brief: ESP Log Header **/
static const char *TAG = "counting_semaphore_task_example";

/** @brief: ESP UART defines. **/
#define EX_UART_NUM                                 UART_NUM_1
#define PATTERN_CHR_NUM                             (3)         
#define ESP_UART_TX_PIN                             16
#define ESP_UART_RX_PIN                             17
#define XI_BUFF_SIZE                                ((int)1024)
#define RD_BUF_SIZE                                 (XI_BUFF_SIZE)

/** @brief: UART Queue Object Create **/
static QueueHandle_t uart_queue;

/** @brief: Semaphore Object Create **/
static SemaphoreHandle_t counting_semaphore;

/** @brief: Semaphore Count Define **/
UBaseType_t semaphore_count = 0;

/**
 * @brief Initializes the UART interface for communication.
 * 
 * This function configures the UART settings such as baud rate, data bits,
 * stop bits, and parity. It prepares the UART interface for communication
 * with the Nextion device.
 * 
 * @return None
 */
void uart_initialization(void) 
{
    uart_config_t uart_config =
    {
        .baud_rate = 115200, /** @brief: this parameters must be 9600 for the communication with Nextion!                                  **/
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, XI_BUFF_SIZE * 2,0, 20, &uart_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);

    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Set UART pins (using UART1 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, ESP_UART_TX_PIN, ESP_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    //Set uart pattern detect function.
    uart_enable_pattern_det_baud_intr(EX_UART_NUM, '+', PATTERN_CHR_NUM, 9, 0, 0);
    //Reset the pattern queue length to record at most 20 pattern positions.
    uart_pattern_queue_reset(EX_UART_NUM, 20);
}


/**
 * @brief UART event handling task.
 * 
 * This function defines a FreeRTOS task that handles UART events such as data
 * transmission and reception. It processes the incoming data from the Nextion
 * device and manages outgoing data based on specific triggers or events.
 * 
 * @param pvParameters   Pointer to task parameters (not used).
 * @return None
 */
void uart_rx_task (void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);

    for (;;) {
        //Waiting for UART event.
        if (xQueueReceive(uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) 
        {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);
            switch (event.type) 
            {
            //Event of UART receving data
            /*We'd better handler data event fast, there would be much more data events than
            other types of events. If we take too much time on data event, the queue might
            be full.*/
            case UART_DATA:
                ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);

                ESP_LOGI(TAG, "[UART RECEIVED DATA]: %s", dtmp);        
                ESP_LOG_BUFFER_HEXDUMP(TAG, dtmp, event.size, ESP_LOG_INFO);

                if(strstr((char *)dtmp, "Increase Semaphore Count!"))
                {
                    ESP_LOGW(TAG, "Semaphore Increase!");
                    xSemaphoreGive(counting_semaphore);
                }

                else if(strstr((char *)dtmp, "Decrease Semaphore Count!"))
                {
                    ESP_LOGE(TAG, "Semaphore Decrease!");
                    xSemaphoreTake(counting_semaphore, 0);
                }

                break;
            //Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart_queue);
                break;
            //Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                // If buffer full happened, you should consider increasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart_queue);
                break;
            //Event of UART RX break detected
            case UART_BREAK:
                ESP_LOGI(TAG, "uart rx break");
                break;
            //Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "uart parity error");
                break;
            //Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "uart frame error");
                break;
            //UART_PATTERN_DET
            case UART_PATTERN_DET:
                uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
                int pos = uart_pattern_pop_pos(EX_UART_NUM);
                ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                if (pos == -1) {
                    // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                    // record the position. We should set a larger queue size.
                    // As an example, we directly flush the rx buffer here.
                    uart_flush_input(EX_UART_NUM);
                } else {
                    uart_read_bytes(EX_UART_NUM, dtmp, pos, 100 / portTICK_PERIOD_MS);
                    uint8_t pat[PATTERN_CHR_NUM + 1];
                    memset(pat, 0, sizeof(pat));
                    uart_read_bytes(EX_UART_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                    ESP_LOGI(TAG, "read data: %s", dtmp);
                    ESP_LOGI(TAG, "read pat : %s", pat);
                }
                break;
            //Others
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

void monitor_semaphore_count_task(void *pvParameters)
{
    for(;;)
    {
        semaphore_count = uxSemaphoreGetCount(counting_semaphore);

        if(semaphore_count < 1)
        {
            uart_write_bytes(EX_UART_NUM, "Critical Semaphore Count, Send Increase Message!\n", strlen("Critical Semaphore Count, Send Increase Message!\n"));

            ESP_LOGE(TAG, "Semaphore counter on limit, current semaphore count %d!", semaphore_count);            
        }
        else
        {
            ESP_LOGW(TAG, "Current semaphore count %d!", semaphore_count);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    /** @brief: Create Binary Semaphore **/
    counting_semaphore = xSemaphoreCreateCounting(5, 5);
    
    uart_initialization();

    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 2, NULL);
    xTaskCreate(monitor_semaphore_count_task, "monitor_semaphore_count_task", 4096, NULL, 2, NULL);
}

/** @brief END OF TASK COUNTING SEMAPHORE EXAMPLE! **/
#endif

/* ----------------------------------------- */
/* ----------------------------------------- */
/* ----------------------------------------- */
/* ----------------------------------------- */

#if(EXAMPLE_SELECT == TASK_EVENT_GROUP_EXAMPLE)

/** @brief: ESP Log Header **/
static const char *TAG = "event_group_task_example";

/** @brief: ESP UART defines. **/
#define EX_UART_NUM                                 UART_NUM_1
#define PATTERN_CHR_NUM                             (3)         
#define ESP_UART_TX_PIN                             16
#define ESP_UART_RX_PIN                             17
#define XI_BUFF_SIZE                                ((int)1024)
#define RD_BUF_SIZE                                 (XI_BUFF_SIZE)

/** @brief: UART Queue Object Create **/
static QueueHandle_t uart_queue;

/** @brief: Event Group Object Create **/
EventGroupHandle_t  event_group_obj;

/**
 * @brief Initializes the UART interface for communication.
 * 
 * This function configures the UART settings such as baud rate, data bits,
 * stop bits, and parity. It prepares the UART interface for communication
 * with the Nextion device.
 * 
 * @return None
 */
void uart_initialization(void) 
{
    uart_config_t uart_config =
    {
        .baud_rate = 115200, /** @brief: this parameters must be 9600 for the communication with Nextion!                                  **/
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, XI_BUFF_SIZE * 2,0, 20, &uart_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);

    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Set UART pins (using UART1 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, ESP_UART_TX_PIN, ESP_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    //Set uart pattern detect function.
    uart_enable_pattern_det_baud_intr(EX_UART_NUM, '+', PATTERN_CHR_NUM, 9, 0, 0);
    //Reset the pattern queue length to record at most 20 pattern positions.
    uart_pattern_queue_reset(EX_UART_NUM, 20);
}


/**
 * @brief UART event handling task.
 * 
 * This function defines a FreeRTOS task that handles UART events such as data
 * transmission and reception. It processes the incoming data from the Nextion
 * device and manages outgoing data based on specific triggers or events.
 * 
 * @param pvParameters   Pointer to task parameters (not used).
 * @return None
 */
void uart_rx_task (void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);

    for (;;) {
        //Waiting for UART event.
        if (xQueueReceive(uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) 
        {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);
            switch (event.type) 
            {
            //Event of UART receving data
            /*We'd better handler data event fast, there would be much more data events than
            other types of events. If we take too much time on data event, the queue might
            be full.*/
            case UART_DATA:
                ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);

                ESP_LOGI(TAG, "[UART RECEIVED DATA]: %s", dtmp);        
                ESP_LOG_BUFFER_HEXDUMP(TAG, dtmp, event.size, ESP_LOG_INFO);

                if(strstr((char *)dtmp, "First event wake up!"))
                {
                    xEventGroupSetBits(event_group_obj, 0x01);      // 0000 0001
                }

                else if(strstr((char *)dtmp, "Second event wake up!"))
                {
                    xEventGroupSetBits(event_group_obj, 0x02);      // 0000 0010
                }

                else if(strstr((char *)dtmp, "Third event wake up!"))
                {
                    xEventGroupSetBits(event_group_obj, 0x04);      // 0000 0100
                }

                else if(strstr((char *)dtmp, "Fourth event wake up!"))
                {
                    xEventGroupSetBits(event_group_obj, 0x08);      // 0000 1000
                }

                break;
            //Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart_queue);
                break;
            //Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                // If buffer full happened, you should consider increasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart_queue);
                break;
            //Event of UART RX break detected
            case UART_BREAK:
                ESP_LOGI(TAG, "uart rx break");
                break;
            //Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "uart parity error");
                break;
            //Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "uart frame error");
                break;
            //UART_PATTERN_DET
            case UART_PATTERN_DET:
                uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
                int pos = uart_pattern_pop_pos(EX_UART_NUM);
                ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                if (pos == -1) {
                    // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                    // record the position. We should set a larger queue size.
                    // As an example, we directly flush the rx buffer here.
                    uart_flush_input(EX_UART_NUM);
                } else {
                    uart_read_bytes(EX_UART_NUM, dtmp, pos, 100 / portTICK_PERIOD_MS);
                    uint8_t pat[PATTERN_CHR_NUM + 1];
                    memset(pat, 0, sizeof(pat));
                    uart_read_bytes(EX_UART_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                    ESP_LOGI(TAG, "read data: %s", dtmp);
                    ESP_LOGI(TAG, "read pat : %s", pat);
                }
                break;
            //Others
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}


void event_group_process_task(void *pvParameters)
{
    EventBits_t event_group_value;

    for(;;)
    {
        event_group_value = xEventGroupWaitBits(event_group_obj,        // Event group handler object
                                                0x0F,                   // 1111 -> wait for first three bits
                                                pdTRUE,                 // Clear on exit
                                                pdFALSE,                // Wait for all bits
                                                portMAX_DELAY);

        ESP_LOGW(TAG, "\n\
                       ***************************\n\
                       Value of the event is 0x%02lx!\n\
                       ***************************", event_group_value);

        if((event_group_value & 0x01) != 0)
        {
            uart_write_bytes(EX_UART_NUM, "First bit of the event received!\n", strlen("First bit of the event received!\n"));

            ESP_LOGI(TAG, "First bit of the event received!");
        }
        else if((event_group_value & 0x02) != 0)
        {
            uart_write_bytes(EX_UART_NUM, "Second bit of the event received!\n", strlen("Second bit of the event received!\n"));

            ESP_LOGI(TAG, "Second bit of the event received!");
        }
        else if((event_group_value & 0x04) != 0)
        {
            uart_write_bytes(EX_UART_NUM, "Third bit of the event received!\n", strlen("Third bit of the event received!\n"));

            ESP_LOGI(TAG, "Third bit of the event received!");
        }
        else if((event_group_value & 0x08) != 0)
        {
            uart_write_bytes(EX_UART_NUM, "Fourth bit of the event received!\n", strlen("Fourth bit of the event received!\n"));

            ESP_LOGI(TAG, "Fourth bit of the event received!");
        }                
    }
}

void app_main(void)
{
    /** @brief: Create Event Group **/
    event_group_obj = xEventGroupCreate();
    
    uart_initialization();

    xTaskCreate(uart_rx_task, "uart_rx_task", 4096, NULL, 2, NULL);
    xTaskCreate(event_group_process_task, "event_group_process_task", 4096, NULL, 2, NULL);
}

/** @brief END OF TASK EVENT GROUP EXAMPLE! **/
#endif

/* ----------------------------------------- */
/* ----------------------------------------- */
/* ----------------------------------------- */
/* ----------------------------------------- */
