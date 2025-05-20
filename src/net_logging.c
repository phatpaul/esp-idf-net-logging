#include "net_logging.h" // public header file should stand on its own, so include it first

#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#if CONFIG_NETLOGGING_USE_RINGBUFFER
#include "freertos/ringbuf.h"
#else
#include "freertos/message_buffer.h"
#endif

#define TAG "net_logging: "

SemaphoreHandle_t logBuffersMutex;
void *logBuffers[6] = {};
bool writeToStdout;
vprintf_like_t old_vprintf = NULL;

// Please note that function callback here must be re-entrant as it can be invoked in parallel from multiple thread context.
static int logging_vprintf(const char *fmt, va_list l) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    char *buffer = malloc(CONFIG_NETLOGGING_MESSAGE_MAX_LENGTH);
    if (buffer == NULL) {
        // Don't use ESP_LOGE here, as it will call logging_vprintf again, causing a infinite recursion and stack overflow.
        NETLOGGING_LOGE(TAG, "logging_vprintf malloc fail");
        return 0;
    }
    int len = vsnprintf(buffer, CONFIG_NETLOGGING_MESSAGE_MAX_LENGTH, fmt, l);
    if (len > 0) {
        const int cstr_len = len + 1;

        if (xSemaphoreTake(logBuffersMutex, portMAX_DELAY) == pdTRUE) {
#if CONFIG_NETLOGGING_USE_RINGBUFFER
            // Send to RingBuffers
            for (int i = 0; i < 6; i++) {
                if (logBuffers[i] != NULL) {
                    BaseType_t sent = xRingbufferSendFromISR(logBuffers[i], buffer, cstr_len, &xHigherPriorityTaskWoken);
                    //assert(sended == pdTRUE); -- don't die if buffer overflows
                    (void)sent;
                }
            }
#else
            // Send to MessageBuffers
            for (int i = 0; i < 6; i++) {
                if (logBuffers[i] != NULL) {
                    BaseType_t sent = xMessageBufferSendFromISR(logBuffers[i], buffer, cstr_len, &xHigherPriorityTaskWoken);
                    //assert(sended == pdTRUE); -- don't die if buffer overflows
                    (void)sent;
                }
            }
#endif
            xSemaphoreGive(logBuffersMutex);
        }

        // Write to stdout
        if (writeToStdout) {
            //return vprintf( fmt, l );
            //printf( "%s", buffer ); // we already formatted the string, so just print it
            fwrite(buffer, sizeof(char), cstr_len, stdout);
            //fflush(stdout); // make sure it gets printed immediately
        }
    }

    free(buffer);
    (void)xHigherPriorityTaskWoken;  // unused
    return 0;
}

/**
 * @brief Register a buffer to be used for logging.
 *
 * @param buffer The buffer to register. It must be a valid pointer to a MessageBufferHandle_t or RingbufHandle_t.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if buffer is NULL, ESP_ERR_NO_MEM if no empty slot found.
 */
esp_err_t netlogging_register_recieveBuffer(void *buffer)
{
    if (buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    assert(NULL != logBuffersMutex); // You probably forgot to call netlogging_init() first!

    esp_err_t ret = ESP_ERR_NO_MEM;
    if (xSemaphoreTake(logBuffersMutex, portMAX_DELAY) == pdTRUE) {
        // search for empty slot
        for (int i = 0; i < 6; i++) {
            if (logBuffers[i] == NULL) {
                logBuffers[i] = buffer;
                ret = ESP_OK;
                break;
            }
        }
        xSemaphoreGive(logBuffersMutex);
    }
    return ret;
}

/**
 * @brief Unregister a buffer to be used for logging.
 *
 * @param buffer The buffer to unregister. Note that this function does not free the buffer itself, it just unregisters it from the logging system.
 * It is the caller's responsibility to free the buffer if it was dynamically allocated.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if buffer is NULL, ESP_ERR_NOT_FOUND if buffer was not found.
 */
esp_err_t netlogging_unregister_recieveBuffer(void *buffer)
{
    if (buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    if (xSemaphoreTake(logBuffersMutex, portMAX_DELAY) == pdTRUE) {
        // search for buffer by pointer
        for (int i = 0; i < 6; i++) {
            if (logBuffers[i] == buffer) {
                logBuffers[i] = NULL;
                ret = ESP_OK;
                break;
            }
        }
        xSemaphoreGive(logBuffersMutex);
    }
    return ret;
}

/**
 * @brief Initialize the netlogging component. Must do this before using any other functions.
 * @param enableStdout If true, log entries will also be printed to stdout (UART).
 * @return ESP_OK on success, ESP_ERR_NO_MEM if memory allocation failed.
 */
esp_err_t netlogging_init(bool enableStdout) {
    // Set function used to output log entries.
    writeToStdout = enableStdout;
    logBuffersMutex = xSemaphoreCreateMutex();
    if (logBuffersMutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    // Initialize the log buffers to NULL
    for (int i = 0; i < 6; i++) {
        logBuffers[i] = NULL;
    }
    // Set function used to output log entries to our custom one.
    old_vprintf = esp_log_set_vprintf(logging_vprintf);
    return ESP_OK;
}

/**
 * @brief Deinitialize the netlogging component. 
 * This should be called when the component is no longer needed to free up resources. 
 * Make sure to unregister all buffers before calling this function.
 * @return ESP_OK on success.
 */
esp_err_t netlogging_deinit() {
    // Restore previous function used to output log entries.
    esp_log_set_vprintf(old_vprintf);

    // We assume that all buffers are unregistered at this point.

    // Delete the mutex
    if (logBuffersMutex != NULL) {
        vSemaphoreDelete(logBuffersMutex);
        logBuffersMutex = NULL;
    }
    return ESP_OK;
}

