/**
 * @file main.c
 * @brief Top-level driver: macro expansion + first pass (which calls second pass).
 *
 * Usage:
 *   ./assembler <file1> <file2> ...
 * Each <fileX> is a basename *without* extension.
 * The macro expander reads "<fileX>.as" and writes "<fileX>.am".
 * Then first_pass("<fileX>", &state) parses the .am, builds images,
 * and invokes second_pass to emit .ob/.ent/.ext on success.
 *
 * Error policy (as implemented by the existing logic):
 *   - If no filenames are provided → print usage and exit(1).
 *   - If macro expansion fails for a file → report and continue to the next file.
 *   - If first_pass(...) returns 1 → free macros and exit(1).
 *     (Note: first_pass may also use FATAL_ERROR internally; we do not change it here.)
 */

#include "assembler.h"
#include "macro_expander.h"
#include "first_pass.h"

int main(int argc, char *argv[]) {
	int i;

	/* Require at least one basename (without extension). */
	if(argc < 2){
		printf("No files were received. Correct usage: %s <file1>...\n", argv[0]);
		return 1;
	}

	for(i = 1; i < argc; i++){
		MacroTable state;
		int result;

		/* Prepare macro table for this file. */
		init_macros(&state);

		/* Run macro expansion: <base>.as -> <base>.am (in place per your implementation). */
		result = mcro_exec(&state, argv[i]);

		if(result != 0){
			fprintf(stderr, "Error: Failed to process file: %s.as\n", argv[i]);
			free_all_macros(&state);
			continue; /* Move on to next file instead of aborting all. */
		}

		/* First pass (will invoke second pass internally on success). */
		if((first_pass(argv[i],&state))==FATAL_ERROR){
			free_all_macros(&state);
			/* Per existing logic: treat this as fatal for the whole run. */
			exit(1);
		}
		/* Done with this file's macro table. */
		free_all_macros(&state);
	}
	return 0;
}



