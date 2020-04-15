#CC = x86_64-w64-mingw32-g++
CC = g++
FLAGS = -ggdb -Wall -std=c++11 -lpthread

# default target entry
default: all

all: server client

server: src/server.cpp
	$(CC) $(FLAGS) -o bin/simpleChatServer src/server.cpp include/common.h

client: src/client.cpp 
	$(CC) $(FLAGS) -o bin/simpleChat src/client.cpp include/common.h

clean:
	$(RM) bin/simpleChatServer bin/simpleChat *.o *~