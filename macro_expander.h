/**
 * @file macro_expander.h
 * @brief Public API for the macro expansion phase (.as -> .am).
 *
 * Responsibilities (as implemented in your project):
 *   - Scan the original ".as" source, collect/define macros, and emit an
 *     expanded intermediate file ".am" with all macros inlined.
 *   - Provide utility functions to create/extend/find macros, manage lifetime,
 *     and validate macro syntax/names.
 *
 * Types Macro/MacroTable are declared in assembler.h and used here by reference.
 */

#ifndef MACRO_EXPANDER_H
#define MACRO_EXPANDER_H

/* Source line and macro name limits used by the expander. */
#define MAX_LINE_LENGTH  100
#define VALID_LINE 80   /* A normal source line will not exceed 80 characters */
#define MAX_MACRO_NAME    31
#define INITIAL_CAPACITY   4

/* ---------------- Error codes returned by the expander ---------------- */
enum {
	MACRO_OK = 0,         /* Success */
	MACRO_SYNTAX_ERROR,   /* Bad 'mcro'/'endmcro' structure or content */
	MACRO_RESERVED,       /* Name collides with reserved word */
	MACRO_ILLEGAL_NAME,   /* Name fails lexical checks */
	MACRO_DUPLICATE       /* A macro with this name already exists */
};

/* ---------------- API declarations ---------------- */
/**
 * @brief Expand macros for the given basename (no extension).
 *        Reads "<name>.as", emits "<name>.am". Returns MACRO_OK on success.
 */
int mcro_exec(MacroTable *state, const char *file_name);

/* Macro object creation/extension/query */
Macro *create_macro(const char *name);
void   add_line_to_macro(Macro *m, const char *line);
Macro *find_macro(MacroTable *state, const char *name);

/* Diagnostics and lifecycle */
void report_error(MacroTable *state, const char *file_name, int line_num, const char *msg);
void init_macros(MacroTable *state);
void free_all_macros(MacroTable *state);

/* Validation helpers used by the expander */
int is_reserved_word(const char *word);
int is_valid_macro_name(const char *name);
int is_valid_macro_start_line(MacroTable *state, const char *line, char *macro_name);

#endif /* MACRO_EXPANDER_H */



