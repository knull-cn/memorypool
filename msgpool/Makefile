
CFLAG=-std=c++11 -c -g 
LDFLAG=-g -lpthread

SRC=src/


CXX=g++
LD=g++

OBJ_DIR=.obj
BIN_DIR=.

OBJS=$(OBJ_DIR)/test_mem.o $(OBJ_DIR)/mem_pool.o

ALL: create_dir test

create_dir:
	mkdir -p $(OBJ_DIR)
	mkdir -p $(BIN_DIR)
clean:
	rm -fr $(OBJ_DIR)/*.o
	rm -fr $(BIN_DIR)/test

test:$(OBJS)
	$(LD) $(LDFLAG) $(OBJS) -o $(BIN_DIR)/test 

$(OBJ_DIR)/test_mem.o:$(SRC)/test_mem.cpp
	$(CXX) $(CFLAG) $(SRC)/test_mem.cpp -o $(OBJ_DIR)/test_mem.o 

$(OBJ_DIR)/mem_pool.o:$(SRC)/mem_pool.cpp
	$(CXX) $(CFLAG) $(SRC)/mem_pool.cpp -o $(OBJ_DIR)/mem_pool.o 





