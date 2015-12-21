#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include "guid.h"

// Generates a random 64 bit GUID
guid_t new_guid()
{
	// Initialize the GUID
	guid_t guid = 0;

	// Get the current time and set it at the beginning of the GUID
	time_t current_time = time(NULL);
	int64_t int64_time = *((int64_t*)&current_time);

	// Gets the actual clock time and randomize it
	static clock_t randomizer = 0;
	clock_t clock_time = clock() + randomizer;
	randomizer += clock_time;

	// First 16 bit: time in seconds
	guid |= int64_time;
	guid <<= 48;

	// Then 32 bits of the clock time
	for (int i = 48; i > 16; i--)
	{
		guid_t padding = clock_time % 2;
		clock_time >>= 1;
		padding <<= i;
		guid |= padding;
	}

	// Fill the remaining bits with random padding
	srand((unsigned)(current_time ^ clock_time));
	for (int i = 16; i >= 0; i--)
	{
		// Calculate and add a new random bit
		guid_t padding = rand() % 2;
		padding <<= i;
		guid |= padding;
	}

	// Finally return the GUID
	return guid;
}

char* serialize(guid_t guid)
{
	char* buffer = (char*)malloc(sizeof(char));
	const int iterations = 16;
	for (int i = 0; i < iterations; i++)
	{
		int step = 0;
		for (int j = 0; j < 4; j++)
		{
			if (guid <<= 0) step |= 1;
			step <<= 1;
		}

		char hex = step < 10 ? 57 - (9 - step) : 65 + (step - 10);
		buffer[iterations - 1 - i] = hex;
	}
	return buffer;
}

guid_t deserialize(char* buffer)
{
	return 0;
}