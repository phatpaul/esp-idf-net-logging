#ifndef NET_LOGGING_PRIV_H_
#define NET_LOGGING_PRIV_H_

#include "net_logging.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#if CONFIG_NET_LOGGING_USE_RINGBUFFER
#include "freertos/ringbuf.h"
#else
#include "freertos/message_buffer.h"
#endif

EXTERN_C_BEGIN

// Don't use ESP_LOGx in this module, as it will call logging_vprintf again, causing a infinite recursion and stack overflow.
// ESP_EARLY_LOGx macros are used instead.
#define NETLOGGING_LOGE(fmt, args...) ESP_EARLY_LOGE(TAG, fmt, ##args)
#define NETLOGGING_LOGW(fmt, args...) ESP_EARLY_LOGW(TAG, fmt, ##args)
#define NETLOGGING_LOGI(fmt, args...) ESP_EARLY_LOGI(TAG, fmt, ##args)
#define NETLOGGING_LOGD(fmt, args...) ESP_EARLY_LOGD(TAG, fmt, ##args)
#define NETLOGGING_LOGV(fmt, args...) ESP_EARLY_LOGV(TAG, fmt, ##args)

EXTERN_C_END

#endif /* NET_LOGGING_PRIV_H_ */
