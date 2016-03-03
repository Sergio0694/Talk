#include <stdlib.h>
#include <time.h>
#include <conio.h>
#include <Windows.h>

// ---------------------------------------------------------------------
// clear_screen
// ---------------------------------------------------------------------
// Fills the console with blank spaces to clear its content
void clear_screen()
{
	DWORD n, size;
	COORD coord = { 0, 0 };
	size = csbiInfo.dwSize.X * csbiInfo.dwSize.Y;
	FillConsoleOutputCharacter(st, TEXT(' '), size, coord, &n);
	GetConsoleScreenBufferInfo(st, &csbiInfo);
	FillConsoleOutputAttribute(st, csbiInfo.wAttributes, size, coord, &n);
	SetConsoleCursorPosition(st, coord);
}

// ---------------------------------------------------------------------
// changeColor
// ---------------------------------------------------------------------
// Sets a new color combination for each character foreground/background
void change_console_color(int color)
{
	SetConsoleTextAttribute(st, color | color);
}