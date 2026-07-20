/**
 * @file first_pass_data.c
 * @brief Data and validation helpers used by the first pass.
 *
 * Responsibilities:
 *   - Manage the data segment table (add_data).
 *   - Parse and encode .data, .string, .mat directives.
 *   - Validate numeric ranges for DATA/INS immediates.
 *   - Validate matrix definition syntax.
 *   - Validate comma placement in argument lists.
 *
 * Error policy:
 *   - On input/semantic errors: set global `error = TRUE` and report via
 *     report_error_pass(am_file_name, line_count, "..."). Return 0 to keep
 *     scanning (non-fatal), or FATAL_ERROR where explicitly documented.
 *   - Memory exhaustion returns FATAL_ERROR (caller should abort this file).
 *
 * Side effects:
 *   - Updates globals: dc, error, line_count (reported), and writes to `*dataset`.
 */

#include "assembler.h"
#include "first_pass.h"

/**
 * @brief Append one encoded data word to the data table.
 * @param dataset  Dynamic array of data words (owned by caller).
 * @param word     Encoded 10-bit word to store.
 * @return 0 on success; FATAL_ERROR on memory exhaustion (and reports error).
 */
int add_data(int ** dataset, int word){/*Inserts a data into the data table*/
        int * ptr;   
        if(dc+ic == MEM_AVAIL_WORDS){/*Checking memory space*/
                error = TRUE;
                report_error_pass(am_file_name, line_count, "There are no free cells in memory");
                return FATAL_ERROR;
        }
        else{
                dc++;
                ptr = realloc(*dataset,dc*sizeof(int));
                if(!ptr){
                        free(*dataset);
                        *dataset = NULL;
                        report_error_pass(am_file_name, line_count,"Memory reallocation failed");
                        return FATAL_ERROR;
                }
                *dataset = ptr;
                (*dataset)[dc-1] = word;
                return 0;
        }    
}

/**
 * @brief Parse and encode a .data directive payload into `dataset`.
 *        Accepts comma-separated integers, validates range for 10-bit.
 * @return 0 on success/non-fatal input errors; FATAL_ERROR on allocation failure.
 */
int data_cmd(char * rest, int ** dataset){
        int num;
        char * token;
        if(!is_valid_commas(rest))
                return 0;
        while((token = strtok(NULL,", \t\n"))){
                num = is_valid_num(token,DATA);
                if(num == INT_MIN)/*the number is invalid:*/
                        break;
                /*the number is valid:*/
                if((add_data(dataset,num))==FATAL_ERROR){
                        return FATAL_ERROR;/*Memory reallocation failed/There is no space in the computer's memory.*/
                }
        }
        return 0;
}

/**
 * @brief Parse and encode a .string directive into `dataset` (ASCII + null).
 *        Validates visible chars, opening/closing quotes.
 * @return 0 on success/non-fatal errors; FATAL_ERROR on allocation failure.
 */
int string_cmd(char * rest, int ** dataset){
        int i = 0; 
        int j = strlen(rest)-1;
        if(rest[i]!='"'){
                error = TRUE;
                report_error_pass(am_file_name, line_count, "Invalid string - missing opening quotes");
                return 0;
        }
        i++;
        while(isspace((unsigned char)(rest[j]))){
                rest[j] = '\0';
                j--;
        }
        while(i<j){
                if(rest[i]>31 && rest[i]<127){ 
                        if((add_data(dataset, rest[i]))==FATAL_ERROR){
                                return FATAL_ERROR;/*Memory reallocation failed/There is no space in the computer's memory.*/
                        }
                        i++;/*The character was inserted successfully - we will move on to the next character.*/
                }
                else{/*Invalid string - invisible character*/
                        error = TRUE;
                        report_error_pass(am_file_name, line_count, "Invalid string - invisible character");
                        return 0;
                }
        }
        if(rest[j]!='"'){
                error = TRUE;
                report_error_pass(am_file_name, line_count, "Invalid string - missing closing quotes");
                return 0;
        }
        
        if((add_data(dataset, 0))==FATAL_ERROR){/*The string was correct - we will add word 0 to memory*/
                return FATAL_ERROR;/*Memory reallocation failed/There is no space in the computer's memory.*/
        }
        return 0;
}

/**
 * @brief Parse and encode a .mat directive: "[r][c], v1, v2, ...".
 *        Fills missing cells with zeros; detects overflow/underflow.
 * @return 0 on success/non-fatal errors; FATAL_ERROR on allocation failure.
 */
int mat_cmd(char * rest, int ** dataset){
        long r = 0, c = 0;
        long * row = &r;
        long * col = &c;
        int num;
        int mat_size;
        char mat_def[WORD_LEN+1];
        char * token;

        copy_first_word(rest,mat_def,WORD_LEN+1);
        if((check_mat_def(mat_def,DATA,row,col))==ERROR_OCCURRED){
                error = TRUE;
                return 0;
        }
        mat_size = (int)(*row)*(int)(*col);
        if(mat_size==0){
                error = TRUE;
                report_error_pass(am_file_name, line_count, "A matrix of size zero is invalid");
                return 0;
        }
        
        strcpy(rest,delete_white(rest + strlen(mat_def)));/*Continuing the sentence after defining the matrix*/

        if(!is_valid_commas(rest))
                return 0;
        else{/*The matrix definition is correct - we will enter the data*/
                strtok(NULL," \t\n");
                while(((token = strtok(NULL,", \t\n"))) && (mat_size>0)){
                        num = is_valid_num(token,DATA);
                        if(num == INT_MIN)/*the number is invalid:*/
                                return 0;            
                        /*the number is valid:*/           
                        if((add_data(dataset,num))==FATAL_ERROR){
                                return FATAL_ERROR;/*Memory reallocation failed/There is no space in the computer's memory.*/
                        }
                        mat_size--;
                }
                if(token!=NULL){
                        error = TRUE;
                        report_error_pass(am_file_name, line_count, "There are unnecessary parameter(s), overflow from the defined matrix");
                        return 0;
                }
                else if(mat_size>0){
                        while(mat_size){             
                                if((add_data(dataset,0))==FATAL_ERROR){
                                        return FATAL_ERROR;/*Memory reallocation failed/There is no space in the computer's memory.*/
                                }
                                mat_size--;
                        }
                }
        }
        return 0;
}

/**
 * @brief Parse decimal integer and validate bit-width per context.
 * @param token    String to parse.
 * @param num_type DATA (10-bit signed) or INS (8-bit signed).
 * @return Parsed value; INT_MIN if invalid (and reports error).
 */
int is_valid_num(char * token, char num_type){
        char * ptr;
        long num;
        num = strtol(token,&ptr,10);
        if(*ptr != '\0'){/*The string is not an integer.*/
                error = TRUE;
                report_error_pass(am_file_name, line_count, "The parameter is invalid - expecting an integer to be received");
                return INT_MIN;/*Indicates that the number is invalid.*/
        }
        if(num_type==DATA){
                if(num>MAX_NUM_D || num<MIN_NUM_D){/*Representing the number requires more than 10 bits.*/
                        error = TRUE;
                        report_error_pass(am_file_name, line_count, "The number is invalid because it requires more than the legal number of bits");
                        return INT_MIN;
                }
        }
        else if(num_type==INS){
                if(num>MAX_NUM_I || num<MIN_NUM_I){/*Representing the number requires more than 10 bits.*/
                        error = TRUE;
                        report_error_pass(am_file_name, line_count, "The number is invalid because it requires more than the legal number of bits");
                        return INT_MIN;
                }
        }    
        return num;
}

/**
 * @brief Validate matrix definition token. For DATA type: "[rows][cols]".
 *        For INS (indexing) supports 'r' registers inside brackets.
 * @return 0 on success; ERROR_OCCURRED on any syntax/semantic issue (reports).
 */
int check_mat_def(char * mat_def,char type, long * row, long * col){
        char * ptr1;
        char * ptr2;    
        if(*mat_def != '['){
                report_error_pass(am_file_name, line_count, "Missing opening bracket or another character was received");
                return ERROR_OCCURRED;
        }
        mat_def++;
        if(type==INS){
                if(*mat_def=='r'){
                        mat_def++;
                }
                else{
                        report_error_pass(am_file_name, line_count, "Using an array expects to receive only register names as parameters, inside []");
                        return ERROR_OCCURRED;
                }   
        }
        *row = strtol(mat_def,&ptr1,10);
        if(ptr1==mat_def){
                report_error_pass(am_file_name, line_count, "A number is missing or a different character was received");
                return ERROR_OCCURRED;
        }
        if(*ptr1!=']'){
                report_error_pass(am_file_name, line_count, "Missing closing bracket or another character was received");
                return ERROR_OCCURRED;        
        }
        if(*ptr1 == '\0'){/*The string is over*/
                report_error_pass(am_file_name, line_count, "Column size definition is missing or there is an extra white space");
                return ERROR_OCCURRED;        
        }
        if(*(++ptr1)!='['){
                report_error_pass(am_file_name, line_count, "Missing opening bracket or another character was received");
                return ERROR_OCCURRED;        
        }       
        ptr1++;
        if(type==INS){
                if(*ptr1=='r'){
                        ptr1++;
                }
                else{
                        report_error_pass(am_file_name, line_count, "Using an array expects to receive only register names as parameters, inside []");
                        return ERROR_OCCURRED;    
                }   
        }
        *col = strtol(ptr1,&ptr2,10);
        if(ptr2==ptr1){
                report_error_pass(am_file_name, line_count, "A number is missing or a different character was received");
                return ERROR_OCCURRED;       
        }    
        if(*ptr2!=']'){
                report_error_pass(am_file_name, line_count, "Missing closing bracket or another character was received");
                return ERROR_OCCURRED;       
        }
        ptr2++;
        if(*ptr2 != '\0'){/*The string is not finished - there is a character after the matrix definition*/
                report_error_pass(am_file_name, line_count, "An extra character appears after a matrix definition");
                return ERROR_OCCURRED;       
        }
        if(((*row) > INT_MAX) || ((*row) < 0)){
                report_error_pass(am_file_name, line_count, "Invalid row size (the required size must be positive and not exceed the size of an int)");
                return ERROR_OCCURRED;       
        }
        if(((*col) > INT_MAX) || ((*col) < 0)){
                report_error_pass(am_file_name, line_count, "Invalid column size (the required size must be positive and not exceed the size of an int)");
                return ERROR_OCCURRED;       
        }
        return 0;
}

/**
 * @brief Validate commas between parameters: catches leading/trailing/multiple commas,
 *        and missing commas between tokens separated only by spaces.
 * @return TRUE if the comma structure is valid; FALSE otherwise (and reports).
 */
int is_valid_commas(char * rest){/*Checks the correctness of commas in a sentence*/
        int i = 0;
        int expect_comma = FALSE, was_space = FALSE;
        if(strlen(rest) == 0)
                return TRUE;
        while(rest[i]!= '\0'){
                if(rest[i]==','){
                        if(!expect_comma){/*there was a comma recently*/
                                error = TRUE;
                                if(i == 0)
                                        report_error_pass(am_file_name, line_count, "There is a comma before parameters");                   
                                else                    
                                        report_error_pass(am_file_name, line_count, "There is more than one comma between parameters");
                                return FALSE;
                        }
                        else{/*comma required*/
                                expect_comma = FALSE;
                        }
                }
                else if(isspace((unsigned char)(rest[i]))){/*White character*/
                        was_space = TRUE;
                }
                else{/*Any character that is not a whitespace and is not a comma*/
                        if(!expect_comma){
                                expect_comma = TRUE;
                                was_space = FALSE;
                        }
                        else if(expect_comma && was_space){/*There was no comma after the previous parameter.*/
                                error = TRUE;               
                                report_error_pass(am_file_name, line_count, "Missing comma between parameters");
                                return FALSE;
                        }           
                }
                i++;
        }
        if(!expect_comma){/*There is a comma at the end of a line*/
                error = TRUE;
                report_error_pass(am_file_name, line_count, "There is a comma after all parameters");
                return FALSE;   
        }
        return TRUE;
}

