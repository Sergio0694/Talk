/* ============================================================================
*  guid.h
* ============================================================================

*  Authors:         (c) 2015 Sergio Pedri and Andrea Salvati */

#ifndef GUID_H
#define GUID_H

// =================== Public types ====================
typedef long long guid_t

/* =====================================================================
*  NewGuid
*  =====================================================================
*  Description:
*    Generates a new, pseudo-random 64 bit GUID */
guid_t new_guid();

char* serialize(guid_t guid);

guid_t deserialize(char* buffer);

#endif