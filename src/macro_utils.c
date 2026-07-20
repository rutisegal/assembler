/**
 * @file macro_utils.c
 * @brief Utility functions for the macro expander: reserved-word checks, macro name
 *        validation, parsing of the macro start line, macro object management, and cleanup.
 *
 * Notes:
 *  - These helpers are used by the pre-assembler (macro expander) phase to validate and
 *    store macros discovered in the original <base>.as file before emitting <base>.am.
 *  - Memory allocation failures are treated as fatal and terminate the program, exactly
 *    as in the original implementation.
 */

#include "assembler.h"
#include "macro_expander.h"

/**
 * @brief Returns non-zero if @p word is a reserved assembler/macro keyword.
 *        The list includes 16 opcodes, macro keywords, and assembler directives.
 */
int is_reserved_word(const char *word) {
	static const char *reserved_words[] = {
		/* assembler opcodes (16) */
		"mov", "cmp", "add", "sub", "not", "clr", "lea",
		"inc", "dec", "jmp", "bne", "red", "prn", "jsr",
		"rts", "stop",

		/* macro keywords */
		"mcro", "mcroend",

		/* assembler directives (without the dot) */
		"data", "string", "mat", "extern", "entry",

		NULL
	};
	int i = 0;
	while (reserved_words[i] != NULL) {
		if (strcmp(word, reserved_words[i]) == 0) {
			return 1;
		}
		i++;
	}
	return 0;
}

/**
 * @brief Validates a macro name according to the project’s lexical rules.
 * @return 1 if valid, 0 otherwise.
 *
 * Rules:
 *  - 1..(MAX_MACRO_NAME-1) characters
 *  - first char must be alphabetic
 *  - subsequent chars alnum or underscore
 */
int is_valid_macro_name(const char *name) {
	int i;
	int len = strlen(name);

	if (len == 0 || len > MAX_MACRO_NAME - 1) {
		return 0;
	}
	if (!isalpha((unsigned char)name[0])) {
		return 0;
	}
	for (i = 1; i < len; i++) {
		if (!isalnum((unsigned char)name[i]) && name[i] != '_') {
			return 0;
		}
	}
	return 1;
}

/**
 * @brief Parses a candidate macro start line. On success, copies the macro name
 *        to @p macro_name and returns MACRO_OK.
 *
 * Expected syntax:
 *   mcro <name>
 *
 * Failure codes:
 *  - MACRO_SYNTAX_ERROR   : line does not match "mcro <name>"
 *  - MACRO_RESERVED       : <name> is a reserved word
 *  - MACRO_ILLEGAL_NAME   : <name> fails lexical checks
 *  - MACRO_DUPLICATE      : macro with the same name already exists in @p state
 */
int is_valid_macro_start_line(MacroTable *state, const char *line, char *macro_name) {
	char token1[MAX_LINE_LENGTH];
	char token2[MAX_LINE_LENGTH];
	char token3[MAX_LINE_LENGTH];
	int count;

	count = sscanf(line, "%s %s %s", token1, token2, token3);

	if (count != 2 || strcmp(token1, "mcro") != 0) {
		return MACRO_SYNTAX_ERROR;
	}
	if (is_reserved_word(token2)) {
		return MACRO_RESERVED;
	}
	if (!is_valid_macro_name(token2)) {
		return MACRO_ILLEGAL_NAME;
	}
	if (find_macro(state, token2)) {
		return MACRO_DUPLICATE;
	}

	strncpy(macro_name, token2, MAX_MACRO_NAME - 1);
	macro_name[MAX_MACRO_NAME - 1] = '\0';

	return MACRO_OK;
}

/**
 * @brief Allocates and initializes a new Macro object with the given @p name.
 *        Exits on allocation failure (as in the original code path).
 */
Macro *create_macro(const char *name) {
	Macro *m = (Macro *)malloc(sizeof(Macro));
	if (!m) {
		fprintf(stderr, "Memory allocation failed\n");
		exit(1);
	}
	strncpy(m->name, name, MAX_MACRO_NAME - 1);
	m->name[MAX_MACRO_NAME - 1] = '\0';
	m->line_count = 0;
	m->capacity = INITIAL_CAPACITY;
	m->lines = (char **)malloc(m->capacity * sizeof(char *));
	if (!m->lines) {
		fprintf(stderr, "Memory allocation failed\n");
		exit(1);
	}
	return m;
}

/**
 * @brief Appends a source line to the given macro, growing its internal storage as needed.
 *        Exits on allocation failure (as in the original code path).
 */
void add_line_to_macro(Macro *m, const char *line) {
	char **temp;
	if (m->line_count >= m->capacity) {
		m->capacity *= 2;
		temp = (char **)realloc(m->lines, m->capacity * sizeof(char *));
		if (!temp) {
			fprintf(stderr, "Memory allocation failed\n");
			exit(1);
		}
		m->lines = temp;
	}
	m->lines[m->line_count] = (char *)malloc(strlen(line) + 1);
	if (!m->lines[m->line_count]) {
		fprintf(stderr, "Memory allocation failed\n");
		exit(1);
	}
	strcpy(m->lines[m->line_count], line);
	m->line_count++;
}

/**
 * @brief Finds a macro by name in @p state. Returns NULL if not found.
 */
Macro *find_macro(MacroTable *state, const char *name) {
	int i;
	for (i = 0; i < state->macro_count; i++) {
		if (strcmp(state->macros[i]->name, name) == 0) {
			return state->macros[i];
		}
	}
	return NULL;
}

/**
 * @brief Uniform error reporter for the macro expander phase.
 *        Prints: "File <file_name>.as, line <n>: <msg>".
 */
void report_error(MacroTable *state, const char *file_name, int line_num, const char *msg) {
	(void)state;
	fprintf(stderr, "File %s.as, line %d: %s\n", file_name, line_num, msg);
}

/**
 * @brief Initializes an empty MacroTable for a new source file.
 */
void init_macros(MacroTable *state) {
	state->macros = NULL;
	state->macro_count = 0;
}

/**
 * @brief Frees all macros and resets the table to an empty state.
 *        Safe to call with an already-empty table.
 */
void free_all_macros(MacroTable *state) {
	int i, j;
	if (state->macros != NULL) {
		for (i = 0; i < state->macro_count; i++) {
			for (j = 0; j < state->macros[i]->line_count; j++) {
				free(state->macros[i]->lines[j]);
			}
			free(state->macros[i]->lines);
			free(state->macros[i]);
		}
		free(state->macros);
	}
	state->macros = NULL;
	state->macro_count = 0;
}



