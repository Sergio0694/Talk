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
	guid |= int64_time;

	// Move the time to the left of the GUID, use a random padding bit on the right
	srand((unsigned)current_time);
	while (guid > 0)
	{
		// Shifts the GUID to the left
		guid <<= 1;

		// Add the random padding
		int padding = rand() % 2;
		guid |= padding;
	}

	// Finally return the GUID
	return guid;
}