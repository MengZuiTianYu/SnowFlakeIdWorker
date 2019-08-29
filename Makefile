CXX=g++
LDFLAGS=-std=c++11 -O1 -g -w -fopenmp
TARGET=main

CFLAGS = -lpthread -lstdc++ -lglog -lsqlite3

LIBS=`pkg-config --cflags --libs opencv`

# select use which tracker
WHICH_TRACKER=tracking

INCLUDE_BASE = ./
INCLUDE_CORE = INCLUDE_BASE/Core/

INCLUDE_DIR = -I"$(INCLUDE_BASE)" \
			-I"$(INCLUDE_CORE)"

	  
SRC_BASE = ./

SRCS := $(wildcard $(SRC_BASE)/*.cpp)

OBJS := $(SRCS:%.cpp=%.o)

all:$(OBJS)
	@echo "Compilling......"
	$(CXX) $(LDFLAGS) -o $(TARGET) $(OBJS) $(CFLAGS) $(LIBS) 

%.o:%.cpp
	$(CXX) $(LDFLAGS) -c $^ -o $@ $(LIBS) $(CFLAGS) $(INCLUDE_DIR)
	@echo " "
	
.PHONY:clean
clean:
	rm -rf $(PROGRAM) $(OBJS) $(TARGET)
