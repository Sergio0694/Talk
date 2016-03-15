/* ===========================================================================
*  guid.h
* ============================================================================

*  Authors:         (c) 2015 Sergio Pedri and Andrea Salvati */

#ifndef GUID_H
#define GUID_H

#include "types.h"
#include "string_helper.h"

// =================== Public types ====================
typedef struct guidStruct* guid_t;

// ==================== Functions ======================

/* =====================================================================
*  NewGuid
*  =====================================================================
*  Description:
*    Generates a new, pseudo-random 128 bit GUID */
guid_t new_guid();

/* =====================================================================
*  GUIDEquals
*  =====================================================================
*  Description:
*    Checks if two GUIDs have the same value
*  Parameters:
*    this ---> The first GUID
*    that ---> The second GUID to check */
bool_t guid_equals(guid_t this, guid_t that);

/* =====================================================================
*  PrintGUID
*  =====================================================================
*  Description:
*    Prints a GUID using the hex format
*  Parameters:
*    guid ---> The GUID to print */
void print_guid(guid_t guid);

/* =====================================================================
*  SerializeGUID
*  =====================================================================
*  Description:
*    Serializes a GUID into an hex char buffer
*  Parameters:
*    guid ---> The GUID to serialize */
string_t serialize_guid(guid_t guid);

/* =====================================================================
*  DeserializeGUID
*  =====================================================================
*  Description:
*    Deserializes a GUID from an hex char buffer
*  Parameters:
*    buffer ---> The char buffer that contains the serialized GUID */
guid_t deserialize_guid(char* buffer);

#endif
