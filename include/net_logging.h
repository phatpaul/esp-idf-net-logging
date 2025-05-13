#ifndef NET_LOGGING_H_
#define NET_LOGGING_H_

#include "esp_err.h"
#include <stdbool.h>

// The total number of bytes (not messages) the message buffer will be able to hold at any one time.
// (CONFIG_NET_LOGGING_BUFFER_SIZE) // set this in menuconfig

// The size, in bytes, required to hold each item in the message,
// (CONFIG_NET_LOGGING_MESSAGE_MAX_LENGTH) // set this in menuconfig

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t netlogging_init(bool enableStdout);
esp_err_t netlogging_deinit(void);
esp_err_t netlogging_register_recieveBuffer(void *buffer);
esp_err_t netlogging_unregister_recieveBuffer(void *buffer);

typedef struct {
    char ipv4addr[16];
    unsigned long port;
} udp_logging_param_t;
esp_err_t netlogging_udp_client_init(const udp_logging_param_t *param);

typedef struct {
    char ipv4addr[16];
    unsigned long port;
} multicast_logging_param_t;
esp_err_t netlogging_multicast_sender_init(const multicast_logging_param_t *param);
esp_err_t netlogging_multicast_sender_run(void);
esp_err_t netlogging_multicast_sender_stop(void);
esp_err_t netlogging_multicast_sender_deinit(void);

typedef struct {
    char ipv4addr[16];
    unsigned long port;
} tcp_logging_param_t;
esp_err_t netlogging_tcp_client_init(const tcp_logging_param_t *param);

typedef struct {
    char url[128];
    char topic[64];
} mqtt_logging_param_t;
esp_err_t mqtt_logging_init(const mqtt_logging_param_t *param);

typedef struct {
    char url[128];
} http_logging_param_t;
esp_err_t http_logging_init(const http_logging_param_t *param);

typedef struct {
    unsigned long port;
} sse_logging_param_t;
esp_err_t netlogging_sse_server_init(const sse_logging_param_t *param);
esp_err_t netlogging_sse_server_run(void);
esp_err_t netlogging_sse_server_stop(void);
esp_err_t netlogging_sse_server_deinit(void);


#ifdef __cplusplus
}
#endif

#endif /* NET_LOGGING_H_ */
