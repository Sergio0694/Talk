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

/* ============================================================================
*  Generic functions
*  ========================================================================= */

// Generates a random 128 bit GUID
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
    if (clock_backup == clock_time) clock_time |= rand();
    clock_backup = clock_time;

    // Add the 32 bits of the clock time
    int64_time |= clock_time;
    guid->time = int64_time;

    // Only set the random feed at the first call
    static bool_t first_call = TRUE;
    if (first_call)
    {
        srand((unsigned)time(NULL));
        first_call = FALSE;
    }

    // Fill the remaining bits with random padding
    int64_t random_bits = 0;
    int i;
    for (i = 0; i < 55; i++)
    {
        // Calculate and add a new random bit
        if (rand() % 2) random_bits |= 1;
        random_bits <<= 1;
    }
    guid->random = random_bits;

    // Sequential counter
    static char counter = 0;
    guid->random <<= 8;
    guid->random |= counter++;

    // Finally return the GUID
    return guid;
}

// Equals
bool_t guid_equals(guid_t this, guid_t that)
{
    // Check if the two references are valid
    if (this == NULL || that == NULL) return FALSE;

    // Compare the two instances
    return this->time == that->time && this->random == that->random;
}

// Print
void print_guid(guid_t guid)
{
    char* buffer = serialize_guid(guid);
    printf("%s", buffer);
    free(buffer);
}

/* ============================================================================
*  Serialization
*  ========================================================================= */

// Serializes a single 64 bit number
static char* serialize_int64(int64_t value)
{
    char* buffer = (char*)malloc(sizeof(char) * 16);
    int i, j;

    // Analyze the 4 bit blocks of the source 64 bit value
    for (i = 0; i < 16; i++)
    {
        // Calculate the value of each group of 4 bits, left to right
        int step = 0;
        for (j = 0; j < 4; j++)
        {
            if (value < 0) step |= 1 << 3 - j;
            value <<= 1;
        }

        // Get the corresponding hex character and add it to the buffer
        char hex = step < 10 ? 57 - (9 - step) : 65 + (step - 10);
        buffer[i] = hex;
    }
    return buffer;
}

// Copies a buffer inside another one
static void copy_serialized_int64_buffer(const char* source, char* dest)
{
    int i;
    for (i = 0; i < 16; i++) dest[i] = source[i];
}

// Serializes a GUID into an hex char buffer
string_t serialize_guid(guid_t guid)
{
    // Allocate the buffer and add the string terminator
    const int iterations = 32;
    char* buffer = (char*)malloc(sizeof(char) * (iterations + 1));
    buffer[iterations] = STRING_TERMINATOR;

    // Get the serialized buffers
    char* time = serialize_int64(guid->time);
    char* random = serialize_int64(guid->random);

    // Concat them inside the main buffer
    copy_serialized_int64_buffer(time, buffer);
    copy_serialized_int64_buffer(random, buffer + 16);

    // Free the temp buffers
    free(time);
    free(random);

    // Return the serialized buffer
    return buffer;
}

/* ============================================================================
*  Deserialization
*  ========================================================================= */

// Deserializes a 64 bit value from a char buffer
static int64_t deserialize_int64(char* buffer)
{
    // Create an empty GUID and start analyzing the input buffer
    int64_t guid_part = 0;
    int i;
    for (i = 0; i < 16; i++)
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
