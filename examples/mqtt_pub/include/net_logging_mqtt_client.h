#pragma once
#include "net_logging.h"

EXTERN_C_BEGIN


typedef struct {
    char url[128];
    char topic[64];
} mqtt_logging_param_t;
esp_err_t netlogging_mqtt_client_init(const mqtt_logging_param_t *param);
esp_err_t netlogging_mqtt_client_run(void);
esp_err_t netlogging_mqtt_client_stop(void);
esp_err_t netlogging_mqtt_client_deinit(void);


EXTERN_C_END

