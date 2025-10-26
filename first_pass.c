/**
 * @file first_pass.c
 * @brief Implements the first assembler pass and invokes the second pass.
 *
 * Summary:
 *   - Scans "<basename>.am", validates syntax/semantics, builds symbols,
 *     instruction/data images, and the pending-references list.
 *   - On non-fatal source errors → reports and *skips* second pass.
 *   - On success → calls second_pass(basename, ...), which emits .ob/.ent/.ext.
 *
 * Globals used (declared here):
 *   error (int), dc (int), ic (int), line_count (int), was_reg (int),
 *   am_file_name[FILENAME_MAX].
 *
 * Error policy:
 *   - Returns FATAL_ERROR (−4) for unrecoverable issues (I/O/memory).
 *   - Returns 0 otherwise (even if non-fatal source errors occurred — they are
 *     signaled via the global `error`, and the first pass decides whether to
 *     call second pass).
 */

#include "assembler.h"
#include "first_pass.h"
#include "second_pass_utils.h"


int dc = 0; /*Data counter*/
int ic = 0; /*Instruction counter*/
int line_count = 0; /*Will count the lines in the file*/
int was_reg = FALSE; /*The flag indicates whether a register was received as a parameter of an instruction.*/
char am_file_name[FILENAME_MAX] = {0};

/**
 * @brief First pass entry point for given basename and macro state.
 * @param name_of_file Basename without extension (e.g., "mini").
 * @param state        MacroTable pointer (ownership stays with caller).
 * @return 0 on success (including when second pass ran and finished);
 *         FATAL_ERROR on fatal errors (I/O/memory).
 */
int first_pass(char * name_of_file, MacroTable * state) {
        int label_flag = FALSE; /*Flag indicating a detected label*/
        int should_continue = FALSE; /*A flag that signals to continue to the next iteration in the outer loop.*/
        char line[VALID_LINE+2] = {0};
        char * line_p;
        char first_word[WORD_LEN+2] = {0};
        char label[WORD_LEN+2] = {0};
        char first[CMD_LEN+1] = {0};
        char rest[VALID_LINE+1] = {0};
        char data_or_ins;
        int opcode;
        size_t len_label;    
        FILE * f_am;

        char * ins_opcode[] = {"mov","cmp","add","sub","lea","clr","not","inc","dec","jmp","bne","jsr","red","prn","rts","stop",NULL}; /*instructions list*/
        int * dataset = NULL; /*Data table*/
        int * ins_set = NULL; /*Instruction table*/
        Label * label_set = NULL; /*Label table*/
        Pending * pending_refs = NULL; /*Pending Labels Table*/
        int p_count = 0;
        int * pend_count = &p_count; /*count pending labels*/
        int label_count = 0;
        int * lc = &label_count; /*Label counter*/
        int i; 
        int ic_final;
        int dc_final;
        int p_error; 

        const Data_commands data_commands[] = {
                {"data",data_cmd},
                {"string",string_cmd},
                {"mat",mat_cmd},
                {"not_valid",NULL}
        };

        const Ins_commands ins_commands[] = { /*For commands with the same addressing type, we will run a function that corresponds to this addressing type.*/
                {"mov",handle_two_prms},{"add",handle_two_prms},{"sub",handle_two_prms},{"cmp",handle_two_prms},{"lea",handle_two_prms},        
                {"clr",handle_one_prm},{"not",handle_one_prm},{"inc",handle_one_prm},{"dec",handle_one_prm},{"jmp",handle_one_prm},{"bne",handle_one_prm},{"jsr",handle_one_prm},{"red",handle_one_prm},{"prn",handle_one_prm},
                {"rts",handle_no_prm},{"stop",handle_no_prm},
                {"not_valid",NULL}
        };

        
        ic = 0;
        dc = 0;
        line_count = 0;
        was_reg = FALSE;

        if (strlen(name_of_file) >= FILENAME_MAX) { /*Check that the file name length is not exceeded*/
                fprintf(stderr,"The file name is too long: %s.am\n",name_of_file);
                return 0;
        }

        sprintf(am_file_name,"%s.am",name_of_file);
        f_am = fopen(am_file_name,"r");
        if (f_am == NULL) {
                fprintf(stderr,"An error occurred while opening the file: %s, The file is not found or is not opening properly\n",am_file_name);
                return FATAL_ERROR;
        }

        while (fgets(line, VALID_LINE+2, f_am) != NULL) { /*start read a new line*/
                line_count++;
                if ((line[VALID_LINE] != '\0') && (line[VALID_LINE] != '\n')) { /*Line length check*/
                        char c;
                        error = TRUE;
                        report_error_pass(am_file_name, line_count, "Invalid line length: over 80 characters");
                        c = fgetc(f_am);
                        while ((c != EOF) && (c != '\n')) /*We will move the pointer to the beginning of the next line so that it reads a new line.*/
                                c = fgetc(f_am);
                        memset(line,0,sizeof(line));
                        continue; /*start read a new line*/
                }

                if (*line == ';') { /*Skip comment line*/
                        continue;
                }

                line_p = delete_white(line);

                if (*line_p == ';') {
                        error = TRUE;
                        report_error_pass(am_file_name, line_count, "A comment line begin with a semicolon, not a blank character");
                        continue;
                }

                if (*line_p == '\0') /*Skip blank line*/
                        continue;      

                copy_first_word(line_p,first_word,WORD_LEN+2); /*Check if it is a label first*/
                len_label = strlen(first_word);

                if ((len_label >= 1) && (first_word[len_label-1] == ':')) { /*This is a label*/
                        first_word[len_label-1] = '\0'; /*Deleting colons*/
                        if (len_label > 0) { /*If the label is not empty, it will be sent for further inspection*/
                                strcpy(label, first_word);
                                if (is_valid_label(label,label_set,state,*lc)) {
                                        label_flag = TRUE;
                                        line_p += len_label; /*Deleting the label from the line*/                  
                                        line_p = delete_white(line_p);
                                } else {
                                        continue;
                                }
                        } else {
                                error = TRUE;
                                report_error_pass(am_file_name, line_count, "Missing name label");
                                continue; /*start read a new line*/
                        }
                }

                if (strlen(line_p) == 0) { /*There is nothing after the label.*/
                        error = TRUE;
                        report_error_pass(am_file_name, line_count, "No content after label");
                        continue; /*start read a new line*/
                }

                if (*line_p == '.') { /*analyze the data (or entry\extern) sentence.*/
                        line_p++;
                        if (*line_p == '\0') {
                                error = TRUE;
                                report_error_pass(am_file_name, line_count, "No command and parameters");
                                continue; /*start read a new line*/
                        }
                        copy_first_word(line_p,first_word,WORD_LEN+2);

                        if ((strcmp(first_word,"entry")) == 0) {
                                label_flag = FALSE;
                                line_p += strlen(first_word);
                                line_p = delete_white(line_p);
                                copy_first_word(line_p,first_word,WORD_LEN+2);

                                if ((strlen(first_word)) == 0) {
                                        error = TRUE;
                                        report_error_pass(am_file_name, line_count, "Missing label name after declaration");
                                        continue;
                                } else if (is_label_name(first_word, label_set, *lc)) {
                                        for (i = 0; i < (*lc); i++) {
                                                if ((strcmp(first_word,label_set[i].l_name)) == 0) {
                                                        if (label_set[i].l_ent_or_ext == EXTERNAL) {
                                                                error = TRUE;
                                                                report_error_pass(am_file_name, line_count, "A label with this name is defined as external");
                                                                should_continue = TRUE;
                                                                break;
                                                        }
                                                        if (label_set[i].l_data_or_ins == UNKNOWN_LABEL_TYPE) {
                                                                should_continue = TRUE;
                                                                break;
                                                        } else {
                                                                label_set[i].l_ent_or_ext = ENTRY;
                                                        }
                                                }
                                        }
                                        if (should_continue) {
                                                should_continue = FALSE;
                                                continue;
                                        }                    
                                } else {
                                        if ((add_label(&label_set, &lc, first_word, UNKNOWN_LABEL_TYPE, ENTRY)) == FATAL_ERROR) {
                                                first_pass_cleanup(f_am, dataset, ins_set, label_set, lc, pending_refs, pend_count);
                                                return FATAL_ERROR;
                                        }
                                }

                                line_p += strlen(first_word);
                                line_p = delete_white(line_p);
                                if (*line_p != '\0') {
                                        error = TRUE;
                                        report_error_pass(am_file_name, line_count, "Additional character(s) received after label name");
                                }
                                continue;
                        }

                        else if ((strcmp(first_word,"extern")) == 0) {
                                label_flag = FALSE;
                                line_p += strlen(first_word);
                                line_p = delete_white(line_p);
                                copy_first_word(line_p,first_word,WORD_LEN+2);

                                if ((strlen(first_word)) == 0) {
                                        error = TRUE;
                                        report_error_pass(am_file_name, line_count, "Missing label name after declaration");
                                        continue;
                                } else if (is_label_name(first_word, label_set, *lc)) {
                                        error = TRUE;
                                        report_error_pass(am_file_name, line_count, "A label with this name is defined as internal");
                                        continue;
                                } else {
                                        if ((add_label(&label_set, &lc, first_word, INS, EXTERNAL)) == FATAL_ERROR) {
                                                first_pass_cleanup(f_am, dataset, ins_set, label_set, lc, pending_refs, pend_count);
                                                return FATAL_ERROR;
                                        }
                                        line_p += strlen(first_word);
                                        line_p = delete_white(line_p);
                                        if (*line_p != '\0') {
                                                error = TRUE;
                                                report_error_pass(am_file_name, line_count, "Additional character(s) received after label name");
                                        }
                                        continue;
                                }
                        }

                        data_or_ins = DATA;            
                        if (isspace((unsigned char)(*line_p))) {
                                error = TRUE;
                                report_error_pass(am_file_name, line_count, "There is a blank character after the period");
                                continue;
                        }
                } else {
                        if ((first_word[len_label] == ':') || (there_is_colon(line_p))) {
                                error = TRUE;
                                report_error_pass(am_file_name, line_count, "Invalid label length");
                                continue;
                        } else {
                                data_or_ins = INS;
                        }
                }

                if (label_flag) {
                        if (is_label_name(label,label_set,*lc)) {
                                for (i = 0; i < (*lc); i++) {
                                        if ((strcmp(label, label_set[i].l_name)) == 0) {
                                                if ((add_label(&label_set, &lc, label, data_or_ins, ENTRY)) == FATAL_ERROR) {
                                                        first_pass_cleanup(f_am, dataset, ins_set, label_set, lc, pending_refs, pend_count);
                                                        return FATAL_ERROR;
                                                }
                                                break;
                                        }
                                }
                        } else {
                                if ((add_label(&label_set, &lc, label, data_or_ins, REGULAR)) == FATAL_ERROR) {
                                        first_pass_cleanup(f_am, dataset, ins_set, label_set, lc, pending_refs, pend_count);
                                        return FATAL_ERROR;
                                }
                        }
                        label_flag = FALSE;
                }

                copy_first_word(line_p,first,CMD_LEN+1);
                strcpy(rest,delete_white(line_p + strlen(first)));
                strtok(line_p," \t\n");

                if (data_or_ins == DATA) {
                        if (strlen(rest) == 0) {
                                error = TRUE;
                                report_error_pass(am_file_name, line_count, "Missing parameters");
                                continue;
                        }

                        for (i = 0; data_commands[i].func != NULL; i++) {
                                if ((strcmp(first, data_commands[i].data_c_name)) == 0) {
                                        if (((*(data_commands[i].func))(rest,&dataset)) == FATAL_ERROR) {
                                                first_pass_cleanup(f_am, dataset, ins_set, label_set, lc, pending_refs, pend_count);
                                                return FATAL_ERROR;
                                        }
                                        should_continue = TRUE;
                                        break;
                                }
                        }
                } else {
                        for (i = 0; ins_commands[i].func != NULL; i++) {
                                if ((strcmp(first, ins_commands[i].ins_c_name)) == 0) {
                                        int k = 0;
                                        while ((strcmp(ins_opcode[k],first))) {
                                                k++;
                                        }
                                        opcode = k;
                                        if ((k < RTS) && (!is_valid_commas(rest))) {
                                                should_continue = TRUE;
                                                break;
                                        }
                                        if (((*(ins_commands[i].func))(rest,&ins_set,&pending_refs,pend_count,opcode)) == FATAL_ERROR) {
                                                first_pass_cleanup(f_am, dataset, ins_set, label_set, lc, pending_refs, pend_count);
                                                return FATAL_ERROR;
                                        }
                                        should_continue = TRUE;
                                        break;
                                }
                        }
                }

                if (should_continue) {
                        should_continue = FALSE;
                        continue;
                } else {
                        error = TRUE;
                        report_error_pass(am_file_name, line_count, "Invalid command name");
                        continue;
                }         
        }

        for (i = 0; i < (*lc); i++) {
                if (label_set[i].l_data_or_ins == UNKNOWN_LABEL_TYPE) {
                        error = TRUE;
                        report_error_pass(am_file_name, (int)label_set[i].l_address, "A label was declared internal and was not defined in this file");
                }
        }

        /* End of first pass: Running second pass with basename without extension */
        ic_final = ic;
        dc_final = dc;
        p_error = error;

        if (second_pass(name_of_file,ins_set, ic_final,dataset, dc_final,label_set, *lc,pending_refs, *pend_count,&p_error) != 0) {
                first_pass_cleanup(f_am, dataset, ins_set, label_set, lc, pending_refs, pend_count);
                return FATAL_ERROR;
        }

        first_pass_cleanup(f_am, dataset, ins_set, label_set, lc, pending_refs, pend_count);
        return 0;  
}

