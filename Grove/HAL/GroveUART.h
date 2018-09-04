#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "../../applibs_versions.h"
#include <applibs/uart.h>

int GroveUART_Open(UART_Id id, UART_BaudRate_Type baudRate);
void GroveUART_Write(int fd, const uint8_t* data, int dataSize);
bool GroveUART_Read(int fd, uint8_t* data, int dataSize);
