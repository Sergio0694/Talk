/* ===========================================================================
*  client_graphics.c
* ============================================================================

*  Authors:         (c) 2016 Sergio Pedri and Andrea Salvati */

#include <stdlib.h>
#include <time.h>
#include <conio.h>
#include <Windows.h>

#include "client_graphics.h"

// Sets a new color combination for each character foreground/background
void change_console_color(int color)
{
    HANDLE st = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(st, color | color);
}
