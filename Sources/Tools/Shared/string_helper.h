/* ===========================================================================
*  string_helper.h
* ============================================================================

*  Authors:         (c) 2015 Sergio Pedri and Andrea Salvati */

#ifndef STRING_HELPER_H
#define STRING_HELPER_H

// =================== Public types ====================
typedef char* string_t;

// ==================== Functions ======================

/* =====================================================================
*  CreateEmptyString
*  =====================================================================
*  Description:
*    Creates an empty string (a string that only contains the
*    string terminator character) */
string_t create_empty_string();

/* =====================================================================
*  StringConcat
*  =====================================================================
*  Description:
*    Concats two strings using the given separator. If the first string
*    is empty, the function just returns the second string without the
*    separator. If both the strings are empty, the function returns
*    an empty string.
*  Parameters:
*    s1 ---> The first string
*    s2 ---> The string to concat at the end of the first one
*    separator ---> The separator to use between the two strings*/
string_t string_concat(string_t s1, string_t s2, char separator);

/* =====================================================================
*  StringClone
*  =====================================================================
*  Description:
*    Clones a source string
*  Parameters:
*    source ---> The string to clone */
string_t string_clone(string_t source);

#endif