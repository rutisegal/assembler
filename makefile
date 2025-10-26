assembler: assembler.o data_handling.o first_pass.o first_pass_utils.o instruction_handling.o macro_utils.o pre_assembler.o second_pass.o second_pass_utils.o
	gcc -ansi -pedantic -Wall -o assembler assembler.o data_handling.o first_pass.o first_pass_utils.o instruction_handling.o macro_utils.o pre_assembler.o second_pass.o second_pass_utils.o

assembler.o: assembler.c assembler.h
	gcc -ansi -pedantic -Wall -c assembler.c

data_handling.o: data_handling.c assembler.h
	gcc -ansi -pedantic -Wall -c data_handling.c

first_pass.o: first_pass.c first_pass.h assembler.h
	gcc -ansi -pedantic -Wall -c first_pass.c

first_pass_utils.o: first_pass_utils.c first_pass.h assembler.h
	gcc -ansi -pedantic -Wall -c first_pass_utils.c

instruction_handling.o: instruction_handling.c assembler.h
	gcc -ansi -pedantic -Wall -c instruction_handling.c

macro_utils.o: macro_utils.c macro_expander.h assembler.h
	gcc -ansi -pedantic -Wall -c macro_utils.c

pre_assembler.o: pre_assembler.c assembler.h
	gcc -ansi -pedantic -Wall -c pre_assembler.c

second_pass.o: second_pass.c assembler.h second_pass_utils.h
	gcc -ansi -pedantic -Wall -c second_pass.c

second_pass_utils.o: second_pass_utils.c assembler.h second_pass_utils.h
	gcc -ansi -pedantic -Wall -c second_pass_utils.c



