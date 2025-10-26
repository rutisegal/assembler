/**
 * @file second_pass_utils.c
 * @brief Implementation of the utilities used by the second pass.
 *
 * Responsibilities:
 *  - Consistent base-4 formatting for words and addresses.
 *  - Writing `.ob` lines with required spacing (ADDR<TAB>WORD).
 *  - Lazy creation of `.ent`/`.ext` when the first line is needed.
 *  - Symbol table lookup and operand-word patching (ARE and value).
 *  - Centralized file open/close and best-effort cleanup.
 *
 * Notes:
 *  - We never assume `.ent`/`.ext` are needed—files are opened on demand.
 *  - ARE values: A=0, E=1, R=2 (stored in the low 2 bits).
 *  - All writes are checked; any I/O failure is treated as fatal.
 */
#include "assembler.h"
#include "second_pass_utils.h"

/* ---- internal helpers ---- */
static void build_path(char *dst, size_t cap, const char *base, const char *ext)
{
    size_t n = 0;
    dst[0] = '\0';
    if (base) {
        strncat(dst, base, cap - 1);
        n = strlen(dst);
    }
    if (n + 1 < cap) strncat(dst, ".", cap - 1 - n);
    n = strlen(dst);
    if (n + strlen(ext) < cap) strncat(dst, ext, cap - 1 - n);
}

/* ---- public API ---- */

int open_outputs(const char *basename, FILE **pob, FILE **pent, FILE **pext)
{
    char path[512];
    *pent = NULL;
    *pext = NULL;

    build_path(path, sizeof(path), basename, "ob");
    *pob = fopen(path, "w");
    if (!*pob) {
        return 1; /* fatal I/O */
    }
    return 0;
}

void close_outputs(FILE *ob, FILE *ent, FILE *ext)
{
    if (ob)  fclose(ob);
    if (ent) fclose(ent);
    if (ext) fclose(ext);
}

void to_base4_word(int value, char out[MAX_BASE4_WORD_LEN])
{
    int i, v;
    v = value & WORD_MASK;
    for (i = WORD_BASE4_DIGITS - 1; i >= 0; --i) {
        out[i] = QUAD_DIGITS[v % 4];
        v /= 4;
    }
    out[WORD_BASE4_DIGITS] = '\0';
}

void to_base4_addr(int value, char out[MAX_BASE4_ADDR_LEN])
{
    int i, v;
    if (value < 0) value = 0;
    v = value;
    for (i = ADDR_BASE4_DIGITS - 1; i >= 0; --i) {
        out[i] = QUAD_DIGITS[v % 4];
        v /= 4;
    }
    out[ADDR_BASE4_DIGITS] = '\0';
}

void write_word_to_ob(FILE *ob, int abs_address, int word10bits)
{
    char a[MAX_BASE4_ADDR_LEN];
    char w[MAX_BASE4_WORD_LEN];
    to_base4_addr(abs_address, a);
    to_base4_word(word10bits, w);
    /* ADDR<TAB>WORD */
    fprintf(ob, "%s\t%s\n", a, w);
}

int write_entry(const char *basename, FILE **ent, const char *label, int abs_address)
{
    char path[512];
    char a[MAX_BASE4_ADDR_LEN];

    if (!*ent) {
        build_path(path, sizeof(path), basename, "ent");
        *ent = fopen(path, "w");
        if (!*ent) return 1;
    }
    to_base4_addr(abs_address, a);
    if (fprintf(*ent, "%s %s\n", label, a) < 0) return 1;
    return 0;
}

int write_external(const char *basename, FILE **ext, const char *label, int abs_use_address)
{
    char path[512];
    char a[MAX_BASE4_ADDR_LEN];

    if (!*ext) {
        build_path(path, sizeof(path), basename, "ext");
        *ext = fopen(path, "w");
        if (!*ext) return 1;
    }
    to_base4_addr(abs_use_address, a);
    if (fprintf(*ext, "%s %s\n", label, a) < 0) return 1;
    return 0;
}

const Label* find_label(const Label *labels, int count, const char *name)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (labels[i].l_name && name && strcmp(labels[i].l_name, name) == 0) {
            return &labels[i];
        }
    }
    return NULL;
}

int patch_word_with_label(int *ins_set,
                          int ic_index,
                          const Label *lab,
                          int ic_final,
                          const char *basename,
                          FILE **ext_file,
                          int *p_error)
{
    int abs_val;
    int word;

    if (!ins_set || !lab) return 0;

    /* Compute absolute address/value by section */
    if (lab->l_ent_or_ext == EXTERNAL) {
        abs_val = 0; /* value for extern */
    } else {
        if (lab->l_data_or_ins == DATA) {
            abs_val = ORG_ADDRESS + ic_final + lab->l_address;
        } else {
            /* INS (or treated as such) */
            abs_val = ORG_ADDRESS + lab->l_address;
        }
        if (abs_val > ADDR_VALUE_MAX) {
            /* Non-fatal: out of 8-bit range in operand value */
            if (p_error) *p_error = 1;
        }
    }

    word = ins_set[ic_index];

    /* Clear current value and ARE bits in operand word */
    word &= ~((ADDR_VALUE_MASK << ADDR_VALUE_SHIFT) | ARE_MASK);

    if (lab->l_ent_or_ext == EXTERNAL) {
        /* Value=0, ARE=E, and log usage */
        word |= (ARE_E << ARE_SHIFT);
        if (write_external(basename, ext_file, lab->l_name,
                           ORG_ADDRESS + ic_index) != 0) {
            return 1; /* fatal I/O */
        }
    } else {
        /* Internal: set value (8-bit) and ARE=R */
        word |= ((abs_val & ADDR_VALUE_MASK) << ADDR_VALUE_SHIFT);
        word |= (ARE_R << ARE_SHIFT);
    }

    ins_set[ic_index] = word;
    return 0;
}

void flush_all_entries(const char *basename,
                       FILE **ent_file,
                       const Label *labels,
                       int count,
                       int ic_final,
                       int *p_error)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (labels[i].l_ent_or_ext == ENTRY) {
            int abs_addr;
            if (labels[i].l_data_or_ins == DATA) {
                abs_addr = ORG_ADDRESS + ic_final + labels[i].l_address;
            } else {
                abs_addr = ORG_ADDRESS + labels[i].l_address;
            }
            if (write_entry(basename, ent_file, labels[i].l_name, abs_addr) != 0) {
                /* fatal I/O would have returned 1 earlier; here we mark non-fatal */
                if (p_error) *p_error = 1;
            }
        }
    }
}

int remove_outputs(const char *basename)
{
    char path[512];

    build_path(path, sizeof(path), basename, "ob");
    remove(path);
    build_path(path, sizeof(path), basename, "ent");
    remove(path);
    build_path(path, sizeof(path), basename, "ext");
    remove(path);

    return 0;
}





