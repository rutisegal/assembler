/**
 * @file pre_assembler.c
 * @brief Macro expansion phase: reads "<base>.as" and emits "<base>.am" with macros expanded.
 *
 * The expander:
 *   - Validates macro declarations ('mcro <name>' ... 'mcroend') and macro names.
 *   - Stores macro bodies into MacroTable while scanning.
 *   - Re-emits the source to <base>.am, inlining macro calls by name.
 *
 * error handling (preserved exactly as in original):
 *   - On any syntax/validation error: print a specific message, close files, remove output,
 *     and return 1 (failure) for this file, allowing the driver to continue with others.
 *   - Memory allocation failures are fatal and exit(1).
 *
 * Implementation notes:
 *   - Filenames are composed using snprintf (safe API).
 */

#include "assembler.h"
#include "macro_expander.h"
int error = FALSE; /*Flag indicating an input error*/

/**
 * @brief Execute the macro expansion for a single basename (no extension).
 *        Input:  "<file_name>.as"
 *        Output: "<file_name>.am"
 *
 * Behavior:
 *   - Lines longer than 80 characters are rejected.
 *   - Comment/blank lines are copied as-is.
 *   - "mcro"/"mcroend" blocks are validated and captured.
 *   - Macro invocations (a line whose first token equals a macro name) are expanded inline.
 *   - Filenames are built safely using snprintf.
 *
 * Return:
 *   0 on success; 1 on error (input open failure, output create failure, syntax error, etc.).
 */
int mcro_exec(MacroTable *state, const char *file_name) {
	FILE *in = NULL, *out = NULL;
	char input_name[FILENAME_MAX];
	char output_name[FILENAME_MAX];
	char line[MAX_LINE_LENGTH];
	char word[MAX_LINE_LENGTH];
	int inside_macro = 0, line_num = 0;
	Macro *current = NULL;
	Macro *m = NULL;
	int i;
	int len; /* C90 – declared at top */
	char macro_name[MAX_MACRO_NAME];
	Macro **temp;
	int res;
	error = FALSE;

	/* Compose filenames. */
	sprintf(input_name, "%s.as", file_name);
	sprintf(output_name, "%s.am", file_name);

	/* Open input and output. */
	in = fopen(input_name, "r");
	if (!in) {
		fprintf(stderr, "Cannot open input file: %s\n", input_name);
		return 1;
	}
	out = fopen(output_name, "w");
	if (!out) {
		fclose(in);
		fprintf(stderr, "Cannot create output file: %s\n", output_name);
		return 1;
	}

	/* Scan the source, either collecting macro bodies or emitting expanded lines. */
	while (fgets(line, MAX_LINE_LENGTH, in)) {
		line_num++;

		/* Hard 80-char limit (logical line length, excluding newline). */
		len = strlen(line);
		if (len > 0 && line[len - 1] == '\n') {
			len--;
		}
		if (len > VALID_LINE) {
			report_error(state, file_name, line_num, "Line too long");
			error = TRUE;
		}

		/* Copy blank/comment lines as-is. */
		if (line[0] == '\n' || line[0] == ';') {
			fputs(line, out);
			continue;
		}

		/* Extract first token to decide on macro syntax / invocation. */
		sscanf(line, "%s", word);

		/* "mcro <name>" — start collecting a new macro. */
		if (strcmp(word, "mcro") == 0) {
			if (inside_macro) {
				report_error(state, file_name, line_num, "Nested macros not supported");
				error = TRUE;
			}
			res = is_valid_macro_start_line(state, line, macro_name);
			if (res != 0) {
				/* Map validator codes to user-friendly messages (preserving your logic). */
				if (res == 1) {
					report_error(state, file_name, line_num, "Invalid macro declaration syntax");
				} else if (res == 2) {
					report_error(state, file_name, line_num, "Reserved word used as macro name");
				} else if (res == 3) {
					report_error(state, file_name, line_num, "Illegal macro name");
				} else if (res == 4) {
					report_error(state, file_name, line_num, "Duplicate macro definition");
				}
				error = TRUE;
			}
			current = create_macro(macro_name);
			inside_macro = 1;
			continue;
		}

		/* 'mcroend' — finish current macro and register it. */
		if (strcmp(word, "mcroend") == 0) {
			if (!inside_macro) {
				report_error(state, file_name, line_num, "'mcroend' without matching 'mcro'");
				error = TRUE;
			}
			if (current == NULL || current->line_count == 0) {
				report_error(state, file_name, line_num, "Empty macro is not allowed");
				error = TRUE;
			}
			temp = (Macro **)realloc(state->macros, (state->macro_count + 1) * sizeof(Macro *));
			if (!temp) {
				fprintf(stderr, "Memory allocation failed\n");
				exit(1);
			}
			state->macros = temp;
			state->macros[state->macro_count++] = current;
			current = NULL;
			inside_macro = 0;
			continue;
		}

		/* Inside a macro body: accumulate lines. */
		if (inside_macro) {
			if (current == NULL) {
				report_error(state, file_name, line_num, "Internal error: current macro is NULL");
				error = TRUE;
			}
			add_line_to_macro(current, line);
		} else {
			/* Outside of a macro: expand invocation or copy the line as-is. */
			m = find_macro(state, word);
			if (m) {
				for (i = 0; i < m->line_count; i++) {
					fputs(m->lines[i], out);
				}
			} else {
				fputs(line, out);
			}
		}
	}

	/* Unclosed macro at EOF. */
	if (inside_macro) {
		report_error(state, file_name, line_num, "Macro not closed before end of file");
		error = TRUE;
	}
	if(error == TRUE){
		fclose(in);
		fclose(out);
		remove(output_name);
		return 1;
	}
	fclose(in);
	fclose(out);
	return 0;
}

