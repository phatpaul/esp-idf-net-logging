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

#if CONFIG_NETLOGGING_USE_RINGBUFFER
#include "freertos/ringbuf.h"
#else
#include "freertos/message_buffer.h"
#endif

EXTERN_C_BEGIN



EXTERN_C_END

#endif /* NET_LOGGING_PRIV_H_ */
