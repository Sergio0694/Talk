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

// Serializes a GUID into an hex char buffer
char* serialize(guid_t guid)
{
	// Allocate the buffer and add the string terminator
	const int iterations = 16;
	char* buffer = (char*)malloc(sizeof(char) * (iterations + 1));
	buffer[iterations] = '\0';

	// Analyze the 4 bit blocks of the source GUID
	for (int i = 0; i < iterations; i++)
	{
		// Calculate the value of each group of 4 bits, left to right
		int step = 0;
		for (int j = 0; j < 4; j++)
		{
			if (guid < 0)
			{
				if (j == 0) step |= 0x8;
				else if (j == 1) step |= 0x4;
				else if (j == 2) step |= 0x2;
				else step |= 0x1;
			}
			guid <<= 1;
		}

		// Get the corresponding hex character and add it to the buffer
		char hex = step < 10 ? 57 - (9 - step) : 65 + (step - 10);
		buffer[i] = hex;
	}
	return buffer;
}

// Deserializes a GUID from an hex char buffer
guid_t deserialize(char* buffer)
{
	// Create an empty GUID and start analyzing the input buffer
	guid_t guid = 0;
	for (int i = 0; i < 16; i++)
	{
		// Calculate the numeric value of the current hex character
		char c = buffer[i];
		guid_t value = c <= 57 ? c - 57 + 9 : c + 10 - 65;

		// Shift the 4 bit block to the right position inside the GUID
		value <<= (4 * (15 - i));
		guid |= value;
	}
	return guid;
}