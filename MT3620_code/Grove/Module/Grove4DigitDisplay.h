//GROVE_NAME        "Grove - 4-Digit Display"
//SKU               104030003
//WIKI_URL          http://wiki.seeedstudio.com/Grove-4-Digit_Display/

#pragma once

#include "../../applibs_versions.h"
#include <applibs/gpio.h>
#include <stdbool.h>

void* Grove4DigitDisplay_Open(GPIO_Id pin1Id, GPIO_Id pin2Id);
void Grove4DigitDisplay_DisplayOneSegment(void* inst, int bitAddr, int dispData);
void Grove4DigitDisplay_DisplayValue(void* inst, int value);
void Grove4DigitDisplay_DisplayClockPoint(bool clockpoint);
