#include "GroveShield.h"
#include "../HAL/GroveUART.h"

#include "../../mt3620_rdb.h"

void GroveShield_Initialize(int* i2cFd)
{
	*i2cFd = GroveUART_Open(MT3620_RDB_HEADER2_ISU0_UART, 9600);
}
