/**
Esti Aker: 328266754. Ruti Segal: 215020975
 * @file assembler.h
 * @brief Common includes, global flags/counters, and shared types (Macro, Label, Pending).
 *
 * This header centralizes:
 *   - Basic project-wide macros (TRUE/FALSE, error codes).
 *   - Public structs for Macro/MacroTable, Label, and Pending reference.
 *   - Extern declarations of global state used by the passes.
 *
 * Notes:
 *   - Label::l_address holds an 8-bit offset within its section.
 *   - Pending keeps the instruction index that will be patched in second pass.
 */

#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>

/* ---- Generic booleans ---- */
#define FALSE 0
#define TRUE  1

/* ---- Label kinds (data/instruction + entry/external/regular) ---- */
#define DATA               'd'
#define INS                'i'
#define EXTERNAL           'x'
#define ENTRY              't'
#define REGULAR            'r'
#define UNKNOWN_LABEL_TYPE '?'

/* ---- Macro subsystem limits ---- */
#define MAX_MACRO_NAME  31

/* ---- Error codes communicated by helpers ---- */
#define ERROR_OCCURRED -3 /* Non-fatal for the current line; keep scanning. */
#define FATAL_ERROR    -4 /* Fatal (e.g., allocation failure / memory full). */

/* ---------------- Macro types ---------------- */

/**
 * @struct Macro
 * @brief Represents a named macro with its captured lines (.as phase).
 */
typedef struct Macro {
	char name[MAX_MACRO_NAME];
	char **lines;
	int line_count;
	int capacity;
} Macro;

/**
 * @struct MacroTable
 * @brief Container of all macros discovered/expanded for a single source file.
 */
typedef struct {
	Macro **macros;
	int macro_count;
} MacroTable;

/* ---------------- Assembly symbol and fixups ---------------- */

/**
 * @struct Label
 * @brief Symbol table entry produced by the first pass.
 *
 * l_address:
 *   - For DATA: offset within the data segment (fits 8-bit).
 *   - For INS : offset within the instruction segment (fits 8-bit).
 *   - For EXTERNAL: 0 (resolved via .ext at use sites).
 *   - For UNKNOWN_LABEL_TYPE: temporarily stores the source line of '.entry'
 *     declaration to enable accurate diagnostics if it remains undefined.
 */
typedef struct{
    char * l_name;            /* label name */
    unsigned char l_address;  /* label address (offset within its section) */
    char l_data_or_ins;       /* DATA / INS / UNKNOWN_LABEL_TYPE */
    char l_ent_or_ext;        /* EXTERNAL / ENTRY / REGULAR */
} Label;

/**
 * @struct Pending
 * @brief Unresolved reference captured during the first pass.
 *
 * ic_index points to the instruction word to be patched in the second pass.
 * line_number_use is kept only for error reporting.
 */
typedef struct{
    char * label_p_name;      /* label name to resolve */
    unsigned char ic_index;   /* index in instruction array to patch */
    int line_number_use;      /* original source line (for diagnostics) */
} Pending;

/* ---------------- Global state (extern) ---------------- */
/* Set and used throughout the passes. */
extern int error;        /* Any non-fatal source error flagged while scanning. */
extern int dc;           /* Data counter (words). */
extern int ic;           /* Instruction counter (words). */
extern int line_count;   /* Current source line index within the .am file. */
extern int was_reg;      /* Internal helper: last operand was a register. */
extern char am_file_name[FILENAME_MAX]; /* Current .am filename (for messages). */

#endif /* ASSEMBLER_H */



