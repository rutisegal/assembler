/**
 * @file first_pass.h
 * @brief Public API and constants for the assembler's first pass (and its helpers).
 *
 * Overview:
 *   The first pass parses "<base>.am", builds:
 *     - instruction image (ins_set) and data image (dataset),
 *     - label table (Label[]),
 *     - pending references (for second-pass patching),
 *   validates syntax/semantics, then invokes second_pass(...) on success.
 *
 * This header exposes:
 *   - Encoding constants (bit shifts / limits).
 *   - Helper function prototypes used by the first pass.
 *   - Cross-module utilities (macro queries, reserved words, etc.).
 */

#ifndef FIRST_PASS_H
#define FIRST_PASS_H

/* ---- Text and memory model ---- */
#define VALID_LINE 80   /* A normal source line will not exceed 80 characters */
#define MEM_AVAIL_WORDS 156  /* Total words available for INS+DATA combined */

/* ---- Token/identifier limits ---- */
#define WORD_LEN 30
#define CMD_LEN  6

/* ---- Numeric ranges ---- */
#define MIN_NUM_D -512  /* 10-bit signed (data) */
#define MAX_NUM_D  511
#define MIN_NUM_I -128  /* 8-bit signed (instruction immediate) */
#define MAX_NUM_I  127

/* ---- Operand roles ---- */
#define SOURCE 1       /* Source operand marker */
#define DESTINATION 2  /* Destination operand marker */

/* ---- Register indices ---- */
#define MIN_NUM_REG 0
#define MAX_NUM_REG 7

/* ---- Encoding bit shifts ---- */
#define OPCODE_SHIFT   6
#define NUM_SHIFT      2
#define SRC_SHIFT      4
#define DST_SHIFT      2
#define ROW_SHIFT      6
#define COL_SHIFT      2
#define SRC_REG_SHIFT  6
#define DST_REG_SHIFT  2

/* ---- Command dispatch tables ---- */
typedef struct{
    char *data_c_name;
    int (*func)(char * rest, int ** dataset);
}Data_commands;

typedef struct{
    char *ins_c_name;
    int (*func)(char * rest, int ** ins_set, Pending ** pending_refs,int * pend_count, int opcode);
}Ins_commands;

/* ---- Addressing modes / opcodes ---- */
enum ADDRESS{IMMEDIATE,DIRECT,MATRIX_ACCESS,DIRECT_REGISTER};
enum OPCODES{MOV,CMP,ADD,SUB,LEA,CLR,NOT,INC,DEC,JMP,BNE,JSR,RED,PRN,RTS,STOP};

/* ---------------- Public API ---------------- */

/**
 * @brief Run first pass for a given basename (without extension) and macro state.
 *        On success, calls second_pass internally to write outputs.
 * @return 0 on success; FATAL_ERROR (−4) on unrecoverable errors.
 */
int first_pass(char * name_of_file, MacroTable * state);

/**
 * @brief Second pass entry (declared here for the first pass to call).
 * @param basename    Basename (no extension); used for output file names.
 * @param ins_set     Instruction image (from first pass).
 * @param ic_final    Final IC count.
 * @param dataset     Data image (from first pass).
 * @param dc_final    Final DC count.
 * @param label_set   Label table.
 * @param label_count Number of labels.
 * @param pending_refs Pending references (to patch).
 * @param pend_count   Count of pending references.
 * @param p_error      [in/out] set non-zero if *either* pass found non-fatal errors.
 * @return 0 on non-fatal completion; non-zero only for fatal I/O errors.
 */
int second_pass(const char *basename, int *ins_set, int ic_final, int *dataset, int dc_final,  const Label *label_set , int label_count, const Pending *pending_refs, int pend_count, int *p_error);

/**
 * @brief Release all allocations associated with the first pass for one file.
 */
void first_pass_cleanup(FILE * f_am, int * dataset, int * ins_set, Label * label_set, int * lc, Pending * pending_refs, int * pend_count);

/**
 * @brief Uniform error reporter (prints "File <name>, line <n>: <msg>").
 */
void report_error_pass(char *file_name, int line_num, const char *msg);

/* ---- Symbol & pending management ---- */
int add_label(Label ** label_set, int ** lc, char * name, char data_or_ins, char ent_or_ext);
int add_pending_refs(Pending ** pending_refs,int * pend_count,int ic,int line_num,char * label_name);

/* ---- Image writers ---- */
int add_data(int ** dataset, int word);
int add_ins(int ** ins_set, int word);

/* ---- String utilities ---- */
char * delete_white(char * str);
int there_is_colon(char * line_p);
void copy_first_word(char * sentence, char * word, size_t max_len);

/* ---- Name validators ---- */
int is_label_name(char * label, Label * label_set, int lc);
int is_reg_name(char * label);
int is_valid_label(char * label, Label * label_set, MacroTable * state,int lc);

/* ---- Directive handlers ---- */
int data_cmd(char * rest, int ** dataset);
int string_cmd(char * rest, int ** dataset);
int mat_cmd(char * rest, int ** dataset);

/* ---- Lexical/semantic checks ---- */
int is_valid_num(char * token, char num_type);
int check_mat_def(char * mat_def,char type, long * row, long * col);
int is_valid_commas(char * rest);

/* ---- Instruction encoders ---- */
int handle_two_prms(char * rest, int ** ins_set, Pending ** pending_refs,int * pend_count, int opcode);
int handle_one_prm(char * rest, int ** ins_set, Pending ** pending_refs,int * pend_count, int opcode);
int handle_no_prm(char * rest, int ** ins_set, Pending ** pending_refs,int * pend_count, int opcode);

/**
 * @brief Parse one operand token, validate against allowed addressing modes,
 *        and emit the required extra word(s). For DIRECT/MATRIX, pushes a Pending.
 * @param num_addr_modes Count of allowed modes that follow as varargs.
 * @return addressing mode on success; ERROR_OCCURRED / FATAL_ERROR on failure.
 */
int parse_encode_arguments(Pending ** pending_refs,int * pend_count, int ** ins_set,char * argument, int src_or_dst, int num_addr_modes,...);

/* ---- Cross-module helpers from the macro subsystem ---- */
int   is_reserved_word(const char *word);
Macro *find_macro(MacroTable *state, const char *name);

#endif /* FIRST_PASS_H */



