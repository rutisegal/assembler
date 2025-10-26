/**
 * @file first_pass_instr.c
 * @brief Instruction image builders: add_ins + encode title/operands for the first pass.
 *
 * Encodes:
 *   - Title words: opcode|src_addr|dst_addr (using OPCODE_SHIFT/SRC_SHIFT/DST_SHIFT).
 *   - Extra words for immediates, direct labels, matrices (and registers).
 * Maintains:
 *   - `ic` (instruction counter), `was_reg` to pack two registers in one word.
 */

#include "assembler.h"
#include "first_pass.h"

/**
 * @brief Append one encoded instruction word to the instruction image.
 * @return 0 on success; FATAL_ERROR on memory exhaustion (and reports).
 */
int add_ins(int ** ins_set, int word){/*Inserts an instruction to the instruction table*/
        int * ptr;
        if(dc+ic == MEM_AVAIL_WORDS)/*Checking memory space*/{
                error = TRUE;
                report_error_pass(am_file_name, line_count, "There are no free cells in memory");
                return FATAL_ERROR;
        }
        else{
                ic++;
                ptr = realloc(*ins_set,ic*sizeof(int));
                if(!ptr){
                        free(*ins_set);
                        *ins_set = NULL;
                        report_error_pass(am_file_name, line_count,"Memory reallocation failed");
                        return FATAL_ERROR;
                }
                *ins_set = ptr;
                (*ins_set)[ic-1] = word;
                return 0;
        }    
}

/**
 * @brief Handle two-operand instructions (mov/add/sub/cmp/lea).
 *        Builds title and appends necessary extra words.
 * @return 0 on success/non-fatal; FATAL_ERROR on allocation failure.
 */
int handle_two_prms(char * rest, int ** ins_set, Pending ** pending_refs,int * pend_count, int opcode){
        int src_addr = 0,dst_addr = 0,ans,i,ic_title;
        char * token;
        if((add_ins(ins_set,0))==FATAL_ERROR)
                return FATAL_ERROR;
        ic_title = ic-1;
         
        for(i = 1; i<=2;i++){
                if(!(token = strtok(NULL,", \t\n"))){/*No parameter was received*/
                        error = TRUE;
                        if(i == 1)
                                report_error_pass(am_file_name, line_count, "Missing parameters");
                        else
                                report_error_pass(am_file_name, line_count, "Missing parameter");
                        return 0;
                }
                if(i==1){/*Source address*/
                        if((opcode==MOV)||(opcode==CMP)||(opcode==ADD)||(opcode==SUB))
                                ans = parse_encode_arguments(pending_refs,pend_count,ins_set,token,SOURCE,4,IMMEDIATE,DIRECT,MATRIX_ACCESS,DIRECT_REGISTER);
                        
                        else if(opcode==LEA)
                                ans = parse_encode_arguments(pending_refs,pend_count,ins_set,token,SOURCE,2,DIRECT,MATRIX_ACCESS);
                }            
                else{/*Destination address*/
                        if(opcode!=CMP){
                                ans = parse_encode_arguments(pending_refs,pend_count,ins_set,token,DESTINATION,3,DIRECT,MATRIX_ACCESS,DIRECT_REGISTER);
                        }
                        else if(opcode==CMP){
                                ans = parse_encode_arguments(pending_refs,pend_count,ins_set,token,DESTINATION,4,IMMEDIATE,DIRECT,MATRIX_ACCESS,DIRECT_REGISTER);
                        }
                }
                        
                if(ans == FATAL_ERROR)
                        return FATAL_ERROR;
                else if(ans == ERROR_OCCURRED)
                        return 0;
                if(i==1)
                        src_addr = ans;
                else
                        dst_addr = ans;     
        }
        was_reg = FALSE;
        if((token = strtok(NULL,", \t\n"))){/*Extraneous character(s) received after parameters*/
                error = TRUE;
                report_error_pass(am_file_name, line_count, "There are unnecessary parameter(s)");
                return 0;
        }
        (*ins_set)[ic_title] = (opcode<<OPCODE_SHIFT)|(src_addr<<SRC_SHIFT)|(dst_addr<<DST_SHIFT);/*build_title_word*/
        return 0;/*The correct command was processed successfully.*/
}

/**
 * @brief Handle one-operand instructions (clr/not/inc/dec/jmp/bne/jsr/red/prn).
 * @return 0 on success/non-fatal; FATAL_ERROR on allocation failure.
 */
int handle_one_prm(char * rest, int ** ins_set, Pending ** pending_refs,int * pend_count, int opcode){
        int src_addr = 0,dst_addr = 0,ans,ic_title;
        char * token;
        if((add_ins(ins_set,0))==FATAL_ERROR)
                return FATAL_ERROR;
        ic_title = ic-1;
        
        if(!(token = strtok(NULL,", \t\n"))){/*No parameter was received*/
                error = TRUE;
                report_error_pass(am_file_name, line_count, "Missing parameter");
                return 0;
        }        
        /*Destination address*/
        if(opcode!=PRN){
                ans = parse_encode_arguments(pending_refs,pend_count,ins_set,token,DESTINATION,3,DIRECT,MATRIX_ACCESS,DIRECT_REGISTER);
        }
        else if(opcode==PRN){
                ans = parse_encode_arguments(pending_refs,pend_count,ins_set,token,DESTINATION,4,IMMEDIATE,DIRECT,MATRIX_ACCESS,DIRECT_REGISTER);
        }   
        if(ans == FATAL_ERROR)
                return FATAL_ERROR;
        else if(ans == ERROR_OCCURRED)
                return 0;
        dst_addr = ans;     
        if((token = strtok(NULL,", \t\n"))){/*Extraneous character(s) received after parameters*/
                error = TRUE;
                report_error_pass(am_file_name, line_count, "There are unnecessary parameter(s)");
                return 0;
        }
        (*ins_set)[ic_title] = (opcode<<OPCODE_SHIFT)|(src_addr<<SRC_SHIFT)|(dst_addr<<DST_SHIFT);/*build_title_word*/
        return 0;/*The correct command was processed successfully.*/     
}

/**
 * @brief Handle zero-operand instructions (rts/stop).
 * @return 0 on success/non-fatal; FATAL_ERROR on allocation failure (very rare).
 */
int handle_no_prm(char * rest, int ** ins_set, Pending ** pending_refs,int * pend_count, int opcode){
        char * token;
        if((add_ins(ins_set,(opcode<<OPCODE_SHIFT)))==FATAL_ERROR)/*build_title_word*/
                return FATAL_ERROR;
        if((token = strtok(NULL," \t\n"))){/*התקבל תו/ים מיותר/ים אחרי פרמטרים*/
                if(*token == ','){
                        report_error_pass(am_file_name, line_count, " There is an extra comma after the command name");
                }
                else
                        report_error_pass(am_file_name, line_count, "There are unnecessary parameter(s)");
                error = TRUE;
                return 0;
        }
        return 0;
}

/**
 * @brief Parse one operand token and emit the necessary extra word(s).
 *        Also pushes pending label references for later resolution.
 * @param src_or_dst   SOURCE or DESTINATION (affects allowed modes and packing).
 * @param num_addr_modes Count of allowed modes that follow as varargs.
 * @return The detected addressing mode (IMMEDIATE/DIRECT/MATRIX_ACCESS/DIRECT_REGISTER),
 *         ERROR_OCCURRED on semantic error, or FATAL_ERROR on allocation failure.
 */
int parse_encode_arguments(Pending ** pending_refs,int * pend_count, int ** ins_set,char * argument, int src_or_dst, int num_addr_modes,...){
        int argument_adrr = -1;
        int i = 0;
        int num;
        va_list arg_p;
        va_start(arg_p,num_addr_modes);
        if(*argument == '#')/*Addressing method - innediate - number*/
                argument_adrr = IMMEDIATE;
        else if(is_reg_name(argument)){/*Addressing method - register*/
                argument_adrr = DIRECT_REGISTER;
        }
        else{
                while(argument[i] != '\0'){
                        if((argument[i]=='[')||(argument[i] == ']')){/*Addressing method - matrix*/
                                argument_adrr = MATRIX_ACCESS;
                                break;
                        }
                        i++;
                }
        }
        if(argument_adrr == -1)
                argument_adrr = DIRECT;/*Addressing method - label*/
        
        while(num_addr_modes){/*Check the allowed addressing methods for this parameter*/
                if((va_arg(arg_p,int))==argument_adrr){
                        break;
                }
                else
                        num_addr_modes--;     
        }
        va_end(arg_p);
        if(!num_addr_modes){/*The argument's addressing method does not match the legal methods for it*/
                error = TRUE;
                if(src_or_dst == DESTINATION){
                        report_error_pass(am_file_name, line_count, "The destination parameter type does not match the command");
                }
                else
                report_error_pass(am_file_name, line_count, "The source parameter type does not match the command");
                return ERROR_OCCURRED;
        }

        switch(argument_adrr){
                case IMMEDIATE:
                        argument++; /*Skip # */
                        num = is_valid_num(argument,INS);
                        if(num == INT_MIN)/*the number is invalid:*/
                                return ERROR_OCCURRED;
                        if((add_ins(ins_set,(num<<NUM_SHIFT)))==FATAL_ERROR)
                                return FATAL_ERROR;
                        break;

                        
                case DIRECT:
                        if((add_ins(ins_set,0))==FATAL_ERROR)/*We will store a zero in place of the label address.*/
                                return FATAL_ERROR;
                        if((add_pending_refs(pending_refs,pend_count,ic,line_count,argument))==FATAL_ERROR)
                                return FATAL_ERROR;            
                        break;

                case MATRIX_ACCESS:{
                        char name_mat[WORD_LEN+1];
                        long r = 0, c = 0;
                        long * row = &r;
                        long * col = &c;
        
                        for(i=0; (i<WORD_LEN)&&(argument[i]!='[')&&(argument[i]!=']'); i++){
                                name_mat[i] = argument[i];
                        }
                        if(i == 0){
                                error = TRUE;
                                report_error_pass(am_file_name, line_count, "Matrix name is missing");
                                return ERROR_OCCURRED;
                        }

                        if((argument[i]!='[')&&(argument[i]!=']')){
                                error = TRUE;
                                report_error_pass(am_file_name, line_count, "Invalid matrix name - too long");
                                return ERROR_OCCURRED;
                        }
                        name_mat[i] = '\0';
                        if((add_ins(ins_set,0))==FATAL_ERROR)/*We will store a zero in place of the label address.*/
                                return FATAL_ERROR;

                        if((add_pending_refs(pending_refs,pend_count,ic,line_count,name_mat))==FATAL_ERROR)
                                return FATAL_ERROR;
                        argument += strlen(name_mat);/**skip the name of the matrix*/
                        if((check_mat_def(argument,INS,row,col))==ERROR_OCCURRED){/*Checking the correctness of a matrix definition*/
                                error = TRUE;
                                return ERROR_OCCURRED;
                        }
                        if((*row)<MIN_NUM_REG || (*row)>MAX_NUM_REG || (*col)<MIN_NUM_REG || (*col)>MAX_NUM_REG){
                                error = TRUE;
                                report_error_pass(am_file_name, line_count, "A register with this name does not exist");
                                return ERROR_OCCURRED;
                        }
                        
                        if((add_ins(ins_set,(((*row)<<ROW_SHIFT)|((*col)<<COL_SHIFT))))==FATAL_ERROR){/*We will insert the register numbers into the next line in memory.*/
                                return FATAL_ERROR;
                        }
                }
                        break;

                case DIRECT_REGISTER:{
                        argument++; /*Skip 'r' */
                        num = atoi(argument);
                        if(src_or_dst == SOURCE){
                                was_reg = TRUE;
                                if((add_ins(ins_set,(num<<SRC_REG_SHIFT)))==FATAL_ERROR){
                                        return FATAL_ERROR;
                                }
                        }
                        else if(src_or_dst == DESTINATION){
                                num<<=DST_SHIFT;
                                if(was_reg){/*Special input that will not overwrite a register that came as the source operand (if any)*/
                                        (*ins_set)[ic-1] |= num;                    
                                }
                                else{                
                                        if((add_ins(ins_set,num))==FATAL_ERROR){
                                                return FATAL_ERROR;/*Memory reallocation failed/There is no space in the computer's memory.*/
                                        }
                                }

                        }
                }
                        break;
        }    
        return argument_adrr;
}

