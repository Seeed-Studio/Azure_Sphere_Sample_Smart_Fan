#pragma once

#include "../../applibs_versions.h"
#include <applibs/gpio.h>

void* GroveAD7992_Open(int i2cFd, GPIO_Id convstId, GPIO_Id alertId);
float GroveAD7992_Read(void* inst, int channel);
