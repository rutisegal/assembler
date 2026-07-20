/**
 * @file second_pass_utils.h
 * @brief Utilities and public API for the assembler’s second pass.
 *
 * This header exposes:
 *  - Machine/format constants (origin address, base-4 alphabet, bit layouts).
 *  - The label/pending types shared with Pass 1 (kept in sync).
 *  - File I/O helpers for `.ob`, `.ent`, `.ext`.
 *  - Base-4 formatting and object writing helpers.
 *  - Symbol lookup and patching (“fixups”) for unresolved labels.
 *  - The `second_pass(...)` entry point.
 *
 * Addressing model:
 *  - Memory origin is `ORG_ADDRESS = 100`.
 *  - Absolute address of instruction word i: `100 + i`.
 *  - Absolute address of data word j:     `100 + IC + j`.
 *  - Internal label in INS section → absolute `100 + offset`.
 *  - Internal label in DATA section → absolute `100 + IC + offset`.
 *  - External label → operand value `0`, ARE=E and a usage line to `<base>.ext`.
 *
 * Output format:
 *  - `.ob` header: **one leading space**, then `IC DC` in base-4 (4 digits each).
 *  - `.ob` body:   `ADDR<TAB>WORD`, where ADDR is 4 base-4 digits, WORD is 5.
 *  - `.ent`: `<LABEL><space><ABS_ADDR>` (4 base-4 digits).
 *  - `.ext`: `<EXT_LABEL><space><USE_ADDR>` (4 base-4 digits).
 *
 * Error policy (non-fatal vs fatal):
 *  - Non-fatal (e.g., undefined label): report, keep scanning, suppress outputs.
 *  - Fatal I/O (open/write): close & remove outputs and return `1`.
 */

#ifndef SECOND_PASS_UTILS_H
#define SECOND_PASS_UTILS_H



/* ===== Machine/encoding constants ===== */

#define ORG_ADDRESS           100


#define WORD_BITS             10
#define WORD_MASK             0x3FF

#define ARE_MASK              0x3
#define ARE_SHIFT             0

#define OPCODE_SHIFT          6
#define SRC_SHIFT             4
#define DST_SHIFT             2

/* Next-word operand layout: 8-bit value + 2-bit ARE */
#define ADDR_VALUE_MASK       0xFF
#define ADDR_VALUE_SHIFT      2
#define ADDR_VALUE_MAX        ADDR_VALUE_MASK

/* Base-4 alphabet (a/b/c/d) and fixed widths */
#define QUAD_DIGITS           "abcd"
#define WORD_BASE4_DIGITS     5
#define ADDR_BASE4_DIGITS     4
#define MAX_BASE4_WORD_LEN    (WORD_BASE4_DIGITS + 1) /* 5 + '\0' */
#define MAX_BASE4_ADDR_LEN    (ADDR_BASE4_DIGITS + 1) /* 4 + '\0' */

/* ===== ARE (2-bit attribute) ===== */
typedef enum {
    ARE_A = 0, /* Absolute */
    ARE_E = 1, /* External */
    ARE_R = 2  /* Relocatable */
} ARE;

/* ===== API ===== */

/**
 * @brief Open `<base>.ob` and prepare lazy handles for `<base>.ent/.ext`.
 * @param basename  Output prefix (e.g., "mini").
 * @param pob       Out: opened `.ob` file (must be closed by caller).
 * @param pent      Out: initially NULL; will be opened on first entry write.
 * @param pext      Out: initially NULL; will be opened on first external use.
 * @return 0 on success, 1 on failure (fatal).
 */
int  open_outputs(const char *basename, FILE **pob, FILE **pent, FILE **pext);

/** @brief Close any non-NULL output streams. */
void close_outputs(FILE *ob, FILE *ent, FILE *ext);

/**
 * @brief Convert a 10-bit word to fixed-width base-4 string (5 chars).
 * @param value  Only the low 10 bits are used.
 * @param out    Buffer of size 6 (`5 + '\0'`).
 */
void to_base4_word(int value, char out[MAX_BASE4_WORD_LEN]);

/**
 * @brief Convert an absolute address to fixed-width base-4 string (4 chars).
 * @param value  Address (non-negative).
 * @param out    Buffer of size 5 (`4 + '\0'`).
 */
void to_base4_addr(int value, char out[MAX_BASE4_ADDR_LEN]);

/**
 * @brief Write one line to `.ob`: `ADDR<TAB>WORD` (both in base-4).
 * @param ob           Open `.ob` file.
 * @param abs_address  Absolute address to print (origin 100).
 * @param word10bits   Encoded 10-bit word.
 */
void write_word_to_ob(FILE *ob, int abs_address, int word10bits);

/**
 * @brief Lazily open `<base>.ent` (if needed) and append one entry label.
 * @param basename     Output prefix.
 * @param ent          In/Out: file handle (NULL → will be opened).
 * @param label        Entry label name.
 * @param abs_address  Absolute address of the label.
 * @return 0 on success, 1 on failure (fatal I/O).
 */
int  write_entry(const char *basename, FILE **ent, const char *label, int abs_address);

/**
 * @brief Lazily open `<base>.ext` (if needed) and append one external use.
 * @param basename     Output prefix.
 * @param ext          In/Out: file handle (NULL → will be opened).
 * @param label        External label name.
 * @param abs_use_address Absolute address of the instruction that uses it.
 * @return 0 on success, 1 on failure (fatal I/O).
 */
int  write_external(const char *basename, FILE **ext, const char *label, int abs_use_address);

/**
 * @brief Find a label by name in the symbol table.
 * @return Pointer to label or NULL if not found.
 */
const Label* find_label(const Label *labels, int count, const char *name);

/**
 * @brief Patch one operand word in `ins_set` according to `lab`.
 *
 * Behavior:
 *  - Internal label: compute absolute address by section, write 8-bit value
 *    into operand word (masked), set ARE=R.
 *  - External label: write value=0, set ARE=E, log a use site to `<base>.ext`.
 *  - If 8-bit overflow occurs (shouldn’t in valid inputs): value is masked and
 *    `*p_error` is set to 1 (non-fatal).
 *  - Any I/O failure writing `.ext` → return 1 (fatal).
 */
int  patch_word_with_label(int *ins_set,
                           int ic_index,
                           const Label *lab,
                           int ic_final,
                           const char *basename,
                           FILE **ext_file,
                           int *p_error);

/**
 * @brief Emit all `.entry` labels to `<base>.ent` (opened lazily).
 * @param basename   Output prefix.
 * @param ent_file   In/Out: `.ent` handle (NULL → lazily opened if needed).
 * @param labels     Symbol table.
 * @param count      Number of labels.
 * @param ic_final   Final IC (for absolute address of DATA).
 * @param p_error    In/Out: non-fatal warning flag.
 */
void flush_all_entries(const char *basename,
                       FILE **ent_file,
                       const Label *labels,
                       int count,
                       int ic_final,
                       int *p_error);

/**
 * @brief Remove `<base>.ob`, `<base>.ent`, `<base>.ext` (best-effort).
 * @return 0 (return value is not used for control flow).
 */
int  remove_outputs(const char *basename);

/**
 * @brief The assembler’s second pass entry point.
 *
 * Flow:
 *  1) If `p_error && *p_error!=0` on entry → first pass had errors:
 *     we still run consistently but at the end we will remove outputs.
 *  2) Open `.ob`, print header (` IC DC` in base-4, with a leading space).
 *  3) For each pending fixup: resolve label; undefined → report and keep scanning.
 *  4) Write all IC words with absolute addresses starting at 100.
 *  5) Write all DC words with absolute addresses starting at `100 + IC`.
 *  6) Emit all `.entry` labels (if any).
 *  7) If any non-fatal errors in this pass OR first-pass error flag was set:
 *     close & remove outputs, set `*p_error=1`, return 0.
 *  8) Otherwise, keep outputs and return 0.
 *
 * Fatal I/O at any time → close & remove outputs and return 1.
 */

int  second_pass(const char *basename,
                 int *ins_set, int ic_final,
                 int *dataset, int dc_final,
                 const Label *label_set , int label_count,
                 const Pending *pending_refs , int pend_count,
                 int *p_error);

#endif /* SECOND_PASS_UTILS_H */





