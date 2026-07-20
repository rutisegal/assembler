/**
 * @file second_pass.c
 * @brief Second pass driver: resolves labels, writes `.ob/.ent/.ext`, and enforces error policy.
 *
 * High-level flow:
 *  - Validate error state coming from Pass 1 (`*p_error`).
 *  - Open `.ob` and print the header line: **one leading space**, then `IC DC` (base-4).
 *  - Resolve all `Pending` fixups:
 *      * Internal label → compute absolute address by section; set operand value; ARE=R.
 *      * External label → value=0; ARE=E; log use site to `<base>.ext`.
 *      * Undefined label → report and keep scanning (non-fatal flag).
 *  - Emit instruction words (IC) starting at absolute 100.
 *  - Emit data words (DC) starting at absolute `100 + IC`.
 *  - Emit `.ent` lines for all labels marked ENTRY.
 *  - Finalize: if either pass reported non-fatal errors → close & remove outputs, set `*p_error=1`.
 *    Fatal I/O → close & remove outputs and return 1.
 *
 * Return values:
 *  - `0`: success (files kept) OR non-fatal errors (files removed, `*p_error=1`).
 *  - `1`: fatal I/O error opening/writing/closing outputs.
 */
#include "assembler.h"
#include "second_pass_utils.h"

int second_pass(const char *basename,int *ins_set, int ic_final,int *dataset, int dc_final,const Label *label_set , int label_count, const Pending *pending_refs , int pend_count,int *p_error){
    FILE *ob, *ent, *ext;
    int had_error = 0;              /* non-fatal errors in this pass */
    int had_first_pass_error = 0;   /* flag coming from pass 1 */
    int i;
    char ic4[MAX_BASE4_ADDR_LEN];
    char dc4[MAX_BASE4_ADDR_LEN];

    if (p_error && *p_error) {
        had_first_pass_error = 1;
    }

    /* Open outputs (.ob). ent/ext will be opened lazily only if needed. */
    if (open_outputs(basename, &ob, &ent, &ext) != 0) {
        /* Fatal I/O: nothing to clean yet except best-effort */
        remove_outputs(basename);
        return FATAL_ERROR;
    }

    /* Header line: one leading space, then IC DC (both 4 base-4 digits). */
    to_base4_addr(ic_final, ic4);
    to_base4_addr(dc_final, dc4);
    if (fprintf(ob, " %s %s\n", ic4, dc4) < 0) {
        close_outputs(ob, ent, ext);
        remove_outputs(basename);
        return FATAL_ERROR;
    }

    /* Resolve all pending references */
    for (i = 0; i < pend_count; ++i) {
        const Pending *pen = &pending_refs[i];
        const Label *lab = find_label(label_set, label_count, pen->label_p_name);
        if (!lab) {
            /* Undefined label (non-fatal): report and keep scanning */
            fprintf(stderr, "Error: undefined label '%s' (source line %d)\n",
                    pen->label_p_name ? pen->label_p_name : "(null)",
                    pen->line_number_use);
            had_error = 1;
            continue;
        }
        /* Patch the operand word; may write to .ext lazily. */
        if (patch_word_with_label(ins_set, pen->ic_index, lab, ic_final,
                                  basename, &ext, &had_error) != 0) {
            /* Fatal I/O writing .ext */
            close_outputs(ob, ent, ext);
            remove_outputs(basename);
            return FATAL_ERROR;
        }
    }

    /* Emit instruction words (IC) */
    for (i = 0; i < ic_final; ++i) {
        write_word_to_ob(ob, ORG_ADDRESS + i, ins_set[i]);
    }

    /* Emit data words (DC) */
    for (i = 0; i < dc_final; ++i) {
        write_word_to_ob(ob, ORG_ADDRESS + ic_final + i, dataset[i]);
    }

    /* Emit all entries (if any) */
    flush_all_entries(basename, &ent, label_set, label_count, ic_final, &had_error);

    /* ===== Final decision on outputs =====
       Per your requirement: if this pass had non-fatal errors OR pass 1 had errors,
       remove outputs and set *p_error=1. Otherwise keep outputs. */
    if (had_error || had_first_pass_error) {
        close_outputs(ob, ent, ext);
        remove_outputs(basename);        /* do not leave outputs */
        if (p_error) *p_error = 1;       /* signal to caller that errors occurred */
        return 0;                        /* non-fatal case: no output files */
    }

    /* No errors in both passes → keep outputs */
    close_outputs(ob, ent, ext);
    return 0;
}





