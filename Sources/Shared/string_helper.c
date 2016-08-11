#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string_helper.h"
#include "types.h"

// Creates an empty string
string_t create_empty_string()
{
    string_t empty = (string_t)malloc(sizeof(char));
    empty[0] = STRING_TERMINATOR;
    return empty;
}

// Concats two strings using the given separator
string_t string_concat(string_t s1, string_t s2, char separator)
{
    // Check the length of the two source strings
    int len1 = strlen(s1), len2 = strlen(s2);

    // If both the strings are empty, just return an empty string
    if (len1 + len2 == 0) return create_empty_string();

    // If the first string is empty, just return the second one
    if (len1 == 0) return s2;

    // Calculate the final string length and allocate the buffer
    int final_len = len1 == 0 ? len2 + 1 : len1 + len2 + 2;
    string_t out = (string_t)malloc(sizeof(char) * final_len);

    // Copy the first string and add the separator, then the second string
    strcpy(out, s1);
    out[len1] = separator;
    strcpy(out + len1 + 1, s2);

    // Free the source buffers and return the result
    free(s1);
    free(s2);
    return out;
}

// Clones a string
string_t string_clone(string_t source)
{
    int len = strlen(source) + 1;
    int i;
    string_t out = (string_t)malloc(sizeof(char) * len);
    for (i = 0; i < len; i++) out[i] = source[i];
    return out;
}
