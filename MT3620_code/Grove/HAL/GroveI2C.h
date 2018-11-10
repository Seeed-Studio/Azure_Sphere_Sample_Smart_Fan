#pragma once

#include <stdint.h>
#include <stdbool.h>

void(*GroveI2C_Write)(int fd, uint8_t address, const uint8_t* data, int dataSize);
bool(*GroveI2C_Read)(int fd, uint8_t address, uint8_t* data, int dataSize);

void GroveI2C_WriteReg8(int fd, uint8_t address, uint8_t reg, uint8_t val);
bool GroveI2C_ReadReg8(int fd, uint8_t address, uint8_t reg, uint8_t* val);
bool GroveI2C_ReadReg16(int fd, uint8_t address, uint8_t reg, uint16_t* val);
bool GroveI2C_ReadReg24BE(int fd, uint8_t address, uint8_t reg, uint32_t* val);
