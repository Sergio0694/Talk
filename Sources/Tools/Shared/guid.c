#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include "guid.h"
#include "types.h"

/* ============================================================================
*  GUID internal types
*  ========================================================================= */

// GUID structure
struct guidStruct
{
	int64_t time;
	int64_t random;
};

// Generates a random 64 bit GUID
guid_t new_guid()
{
	// Initialize the GUID
	guid_t guid = (guid_t)malloc(sizeof(struct guidStruct));

	// Get the current time and set it at the beginning of the time GUID field
	time_t current_time = time(NULL);
	int64_t int64_time = *((int64_t*)&current_time);

	// First 32 bit: time in seconds
	int64_time <<= 32;

	// Gets the actual clock time and randomize it
	static clock_t randomizer = 0, clock_backup;
	clock_t clock_time = clock() + randomizer;
	randomizer += clock_time;
	if (clock_backup == clock_time)
	{
		clock_time |= rand();
		clock_backup = clock_time;
	}
	else clock_backup = clock_time;	

	// Add the 32 bits of the clock time
	int64_time |= clock_time;
	guid->time = int64_time;

	// Fill the remaining bits with random padding
	srand((unsigned)clock_backup);
	int64_t random_bits = 0;
	for (int i = 0; i < 63; i++)
	{
		// Calculate and add a new random bit
		if (rand() % 2) random_bits |= 1;
		random_bits <<= 1;
	}
	guid->random = random_bits;

	// Finally return the GUID
	return guid;
}

// Print
void print_guid(guid_t guid)
{
	char* buffer = serialize_guid(guid);
	printf("%s", buffer);
	free(buffer);
}

// Serializes a GUID into an hex char buffer
char* serialize_guid(guid_t guid)
{
	// Allocate the buffer and add the string terminator
	const int iterations = 32;
	char* buffer = (char*)malloc(sizeof(char) * (iterations + 1));
	buffer[iterations] = '\0';

	// Analyze the 4 bit blocks of the source GUID
	int64_t value = guid->time;
	for (int i = 0; i < iterations; i++)
	{
		// Calculate the value of each group of 4 bits, left to right
		int step = 0;
		for (int j = 0; j < 4; j++)
		{
			if (value < 0)
			{
				if (j == 0) step |= 0x8;
				else if (j == 1) step |= 0x4;
				else if (j == 2) step |= 0x2;
				else step |= 0x1;
			}
			value <<= 1;
		}

		// Get the corresponding hex character and add it to the buffer
		char hex = step < 10 ? 57 - (9 - step) : 65 + (step - 10);
		buffer[i] = hex;

		// Switch to the second part of the GUID if necessary
		if (i == 16) value = guid->random;
	}
	return buffer;
}

static int64_t deserialize_int64(char* buffer)
{
	// Create an empty GUID and start analyzing the input buffer
	int64_t guid_part;
	for (int i = 0; i < 16; i++)
	{
		// Calculate the numeric value of the current hex character
		char c = buffer[i];
		int64_t value = c <= 57 ? c - 57 + 9 : c + 10 - 65;

		// Shift the 4 bit block to the right position inside the GUID
		value <<= (4 * (15 - i));
		guid_part |= value;
	}
	return guid_part;
}

// Deserializes a GUID from an hex char buffer
guid_t deserialize_guid(char* buffer)
{
	guid_t guid = (guid_t)malloc(sizeof(struct guidStruct));
	guid->time = deserialize_int64(buffer);
	guid->random = deserialize_int64(buffer + 16);
	return guid;
}