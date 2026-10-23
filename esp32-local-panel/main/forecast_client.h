#pragma once

#include "esp_err.h"

esp_err_t forecast_client_init(void);
void forecast_client_request_refresh(void);
