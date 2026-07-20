/**
 * @file first_pass_utils.c
 * @brief Utility routines used by the first pass: cleanup, error reporting,
 *        label/pending management, and small string utilities.
 */

#include "assembler.h"
#include "first_pass.h"

/**
 * @brief Release all resources allocated during first pass for one file.
 * @param f_am         Opened .am FILE* (will be closed).
 * @param dataset      Data image (malloc'd).
 * @param ins_set      Instruction image (malloc'd).
 * @param label_set    Label table (each l_name malloc'd).
 * @param lc           Number of labels (will be decremented to zero).
 * @param pending_refs Pending reference table (each label_p_name malloc'd).
 * @param pend_count   Number of pending refs (will be decremented to zero).
 */
void first_pass_cleanup(FILE * f_am, int * dataset, int * ins_set, Label * label_set, int * lc, Pending * pending_refs, int * pend_count){
        fclose(f_am);
        free(dataset);
        free(ins_set);
        while(*pend_count){/*Release the strings in the pending labels table*/
                free(pending_refs[(*pend_count)-1].label_p_name);
                (*pend_count)--;
        }
        free(pending_refs);
        while(*lc){/*Release the strings in the label table*/
                free(label_set[(*lc)-1].l_name);
                (*lc)--;
        }
        free(label_set);
}

/**
 * @brief Uniform error reporter for the passes.
 */
void report_error_pass(char * file_name, int line_count, const char *msg){
        fprintf(stderr, "File %s, line %d: %s\n", file_name, line_count, msg);
}

/**
 * @brief Append a pending reference (to be resolved in second pass).
 * @param pending_refs Table pointer (realloc'd as needed).
 * @param pend_count   Count of pending references (in/out).
 * @param ic           Current IC (the referring word index + 1).
 * @param line_num     Source line number of the use (for diagnostics).
 * @param label_name   The unresolved label name (copied).
 * @return 0 on success; FATAL_ERROR on allocation failure.
 */
int add_pending_refs(Pending ** pending_refs,int * pend_count,int ic,int line_num,char * label_name){
        Pending * ptr;
        char * str;
        (*pend_count)++;
        ptr = realloc(*pending_refs,(*pend_count)*sizeof(Pending));
        if(!ptr){
                while((*pend_count)){/*Release the strings in the label table*/
                        free((*pending_refs)[(*pend_count)-1].label_p_name);
                        (*pending_refs)[(*pend_count)-1].label_p_name = NULL;
                        (*pend_count)--;
                }
                free(*pending_refs);
                *pending_refs = NULL;
                report_error_pass(am_file_name, line_count,"Memory reallocation failed");
                return FATAL_ERROR;
        }
        *pending_refs = ptr;
        str = malloc(strlen(label_name) + 1);
        if(!str){ /*Dynamic allocation failed*/
                report_error_pass(am_file_name, line_count,"Dynamic allocation failed");
                return FATAL_ERROR; 
        }
        (*pending_refs)[(*pend_count)-1].label_p_name = str;
        strcpy((*pending_refs)[(*pend_count)-1].label_p_name, label_name);
        (*pending_refs)[(*pend_count)-1].ic_index = ic-1;
        (*pending_refs)[(*pend_count)-1].line_number_use = line_num;
        return 0;
}

/**
 * @brief Insert a new label into the label table, or finalize a previously
 *        seen .entry placeholder (UNKNOWN_LABEL_TYPE).
 * @return 0 on success; FATAL_ERROR on allocation failure.
 */
int add_label(Label ** label_set, int ** lc, char * name, char data_or_ins,char ent_or_ext){/*Inserts a label into the label table*/
        Label * ptr;
        char * str;
        int i;

        if(is_label_name(name,*label_set,**lc)){/*There is a label declared as 'entry')*/
                for(i = 0; i<(**lc); i++){
                        if((*label_set)[i].l_data_or_ins == UNKNOWN_LABEL_TYPE){
                                (*label_set)[i].l_data_or_ins = data_or_ins;
                                if(data_or_ins == DATA){
                                        (*label_set)[i].l_address = (unsigned char)dc;
                                }
                                else if(data_or_ins == INS){
                                        (*label_set)[i].l_address = (unsigned char)ic;
                                }
                                return 0;
                        }
                }
        }
        (**lc)++;
        ptr = realloc(*label_set,((unsigned int)(**lc))*sizeof(Label));
        if(!ptr){
                while(**lc){/*Release the strings in the label table*/
                        free((*label_set)[(**lc)-1].l_name);
                        (*label_set)[(**lc)-1].l_name = NULL;
                        (**lc)--;
                }
                free(*label_set);
                *label_set = NULL;
                report_error_pass(am_file_name, line_count,"Memory reallocation failed");
                return FATAL_ERROR;
        }
        *label_set = ptr;

        str = malloc(strlen(name) + 1);
        if(!str){ /*Dynamic allocation failed*/
                report_error_pass(am_file_name, line_count,"Dynamic allocation failed");
                return FATAL_ERROR; 
        }
        (*label_set)[(**lc)-1].l_name = str;
        strcpy((*label_set)[(**lc)-1].l_name, name);
        if(ent_or_ext == EXTERNAL){
                (*label_set)[(**lc)-1].l_address = 0;
        }
        else if(data_or_ins == UNKNOWN_LABEL_TYPE){/*Save the line number of the entry statement so that you can report an error on the exact line if the label is not set*/
                (*label_set)[(**lc)-1].l_address = (unsigned char)line_count;
        }
        else if(data_or_ins == DATA){
                (*label_set)[(**lc)-1].l_address = (unsigned char)dc;
        }
        else if(data_or_ins == INS){
                (*label_set)[(**lc)-1].l_address = (unsigned char)ic;
        }
        
        
        (*label_set)[(**lc)-1].l_data_or_ins = data_or_ins;
        (*label_set)[(**lc)-1].l_ent_or_ext = ent_or_ext;
        return 0;
}

/** @brief Skip leading whitespace (returns pointer inside the same buffer). */
char * delete_white(char * str){/*Deleting whitespace characters at the beginning of a string*/
        while((*str!='\0')&&(isspace((unsigned char)(*str))))str++;
        return str;
} 

/**
 * @brief Copy the first token (non-space run) into `word` up to max_len-1,
 *        null-terminating the result.
 */
void copy_first_word(char * sentence, char * word, size_t max_len){
        int i = 0;
        while(sentence[i]!='\0' && !isspace((unsigned char)sentence[i]) && i<max_len-1){
                word[i]=sentence[i];
                i++;
        }
        word[i]='\0';
}

/** @brief Detects a colon ':' before whitespace (used to reject long labels). */
int there_is_colon(char * line_p){
        while((*line_p!='\0')&&(!(isspace((unsigned char)(*line_p))))){
                if(*line_p == ':'){
                        return TRUE;
                }
                line_p++;
        }
        return FALSE;
}

/** @brief Return TRUE if label exists already in label_set. */
int is_label_name(char * label, Label * label_set, int lc){
        int j;
        for(j = 0; j<lc;j++){/*go over the names of the labels stored in the table*/
                if(strcmp(label, label_set[j].l_name))/*The names are different*/
                        continue;
                else{
                        return TRUE;
                }
        }
        return FALSE;
}

/** @brief Return TRUE if token is one of r0..r7. */
int is_reg_name(char * label){
        char * reg_set[] = {"r0","r1","r2","r3","r4","r5","r6","r7",NULL};/*register list*/
        int i = 0;
        while(reg_set[i]!=NULL){
                if(!strcmp(label,reg_set[i])){/*The strings are identical*/
                        return TRUE;
                }
                i++;
        }
        return FALSE;
}
  
/**
 * @brief Validate that a prospective label name is legal and non-conflicting.
 *        Rejects: non-alpha first char, non-alnum chars, duplicates (unless
 *        UNKNOWN placeholder), register names, reserved words, macro names.
 * @return TRUE if valid; FALSE otherwise (and reports error).
 */
int is_valid_label(char * label, Label * label_set, MacroTable * state,int lc){
        int i = 1;
        
        if(!isalpha(label[0])){/*The first character of a label must be a letter.*/
                error = TRUE;
                report_error_pass(am_file_name, line_count, "Invalid label name - first character must be a letter");
                return FALSE;
        }    
        while(label[i]!='\0'){/*A label made up of letters or numbers*/
                if(isdigit(label[i]) || isalpha(label[i]))
                        i++;
                else{
                        error = TRUE;
                        report_error_pass(am_file_name, line_count, "Invalid label name - A valid label name contains only numbers or letters");
                        return FALSE;
                }
        }
        if(is_label_name(label,label_set,lc)){/*Check if a label with this name is already defined*/
                for(i = 0; i<lc; i++){
                        if(strcmp(label, label_set[i].l_name) == 0){
                                if(label_set[i].l_data_or_ins == UNKNOWN_LABEL_TYPE){
                                        break;
                                }
                                else{/*There is a label with this name already defined (not just declared as 'entry')*/
                                        error = TRUE;
                                        report_error_pass(am_file_name, line_count, "A label with the same name already exists");
                                        return FALSE;
                                }
                        }
                }
        }

        if(is_reg_name(label)){
                error = TRUE;
                report_error_pass(am_file_name, line_count, "The label name is invalid - it is a register name");
                return FALSE;    
        }
        if(is_reserved_word(label)){
                error = TRUE;
                report_error_pass(am_file_name, line_count, "The label name is invalid - it is a reserved word");
                return FALSE;
        }
        if(find_macro(state,label)){
                error = TRUE;
                report_error_pass(am_file_name, line_count, "The label name is invalid - it is a macro name");
                return FALSE;
        }
        return TRUE;
}

