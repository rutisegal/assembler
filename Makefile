CC = gcc
CFLAGS = -ansi -pedantic -Wall
SRC_DIR = src

OBJS = $(SRC_DIR)/assembler.o \
       $(SRC_DIR)/data_handling.o \
       $(SRC_DIR)/first_pass.o \
       $(SRC_DIR)/first_pass_utils.o \
       $(SRC_DIR)/instruction_handling.o \
       $(SRC_DIR)/macro_utils.o \
       $(SRC_DIR)/pre_assembler.o \
       $(SRC_DIR)/second_pass.o \
       $(SRC_DIR)/second_pass_utils.o

assembler: $(OBJS)
	$(CC) $(CFLAGS) -o assembler $(OBJS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o assembler
