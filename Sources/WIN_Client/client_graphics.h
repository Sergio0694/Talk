/* ===========================================================================
*  client_graphics.h
* ============================================================================

*  Authors:         (c) 2016 Sergio Pedri and Andrea Salvati */

#ifndef CLIENT_GRAPHICS_H
#define CLIENT_GRAPHICS_H

#include <stdlib.h>
#include <time.h>
#include <conio.h>
#include <Windows.h>

/* ==================== Change color ===================== */

#define LIGHT_GREY 383
#define BLACK 368
#define DARK_GREEN 370
#define DARK_TEXT 264
#define WHITE_TEXT 271
#define GREEN_TEXT 266
#define RED_TEXT 268

/* =====================================================================
*  ChangeConsoleColor
*  =====================================================================
*  Description:
*    Changes the color in use when printing into the stdout buffer */
void change_console_color(int color);

#endif
