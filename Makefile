CC=gcc
LIB_DIR=./lib
BIN_DIR=./bin
SRC_DIR=./src
INC_DIR=./include

all: $(BIN_DIR)/t2fs.o $(LIB_DIR)/apidisk.o
	ar crs $(LIB_DIR)/libt2fs.a $(BIN_DIR)/t2fs.o $(LIB_DIR)/apidisk.o
$(BIN_DIR)/t2fs.o: $(SRC_DIR)/t2fs.c
	$(CC) -I$(INC_DIR) -c -o $(BIN_DIR)/t2fs.o $(SRC_DIR)/t2fs.c -Wall
clean:
	rm -rf $(LIB_DIR)/*.a $(BIN_DIR)/*.o $(SRC_DIR)/*~ $(INC_DIR)/*~ *~
