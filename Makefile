CC=gcc
LIB_DIR=./lib
BIN_DIR=./bin
SRC_DIR=./src
INC_DIR=./include
FLAGS=-m32 -Wall -g
.SUFFIXES:
.SUFFIXES: .o .c
SRCS=$(SRC_DIR)/t2fs.c $(SRC_DIR)/aux.c
OBJS=$(BIN_DIR)/t2fs.o $(BIN_DIR)/aux.o


all: $(BIN_DIR)/aux.o $(BIN_DIR)/t2fs.o $(LIB_DIR)/apidisk.o
	ar crs $(LIB_DIR)/libt2fs.a $(BIN_DIR)/aux.o $(BIN_DIR)/t2fs.o $(LIB_DIR)/apidisk.o
$(BIN_DIR)/aux.o: $(SRC_DIR)/aux.c
	$(CC) -I$(INC_DIR) -o $(BIN_DIR)/aux.o -c $(SRC_DIR)/aux.c $(FLAGS)
$(BIN_DIR)/t2fs.o: $(SRC_DIR)/t2fs.c
	$(CC) -I$(INC_DIR) -o $(BIN_DIR)/t2fs.o -c $(SRC_DIR)/t2fs.c $(FLAGS)
clean:
	rm -rf $(LIB_DIR)/*.a $(BIN_DIR)/*.o $(SRC_DIR)/*~ $(INC_DIR)/*~ *~
