/* ============================================================================
*  guid.h
* ============================================================================

*  Authors:         (c) 2015 Sergio Pedri and Andrea Salvati */

#ifndef TYPES_H
#define TYPES_H

// =================== Boolean type ====================
typedef enum { FALSE, TRUE } bool_t;

// ==================== Constants ======================
#define STRING_TERMINATOR '\0'

// ============ Serialization separators ===============
#define INTERNAL_SEPARATOR '~'
#define EXTERNAL_SEPARATOR '|'

#endif