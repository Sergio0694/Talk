/* ============================================================================
*  guid.h
* ============================================================================

*  Authors:         (c) 2015 Sergio Pedri and Andrea Salvati */

#ifndef GUID_H
#define GUID_H

// =================== Public types ====================
typedef struct guidStruct* guid_t;

/* =====================================================================
*  NewGuid
*  =====================================================================
*  Description:
*    Generates a new, pseudo-random 128 bit GUID */
guid_t new_guid();

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
char* serialize_guid(guid_t guid);

/* =====================================================================
*  DeserializeGUID
*  =====================================================================
*  Description:
*    Deserializes a GUID from an hex char buffer
*  Parameters:
*    buffer ---> The char buffer that contains the serialized GUID */
guid_t deserialize_guid(char* buffer);

#endif