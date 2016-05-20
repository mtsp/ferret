
all: dispatcher gen-dag

dispatcher: dispatcher.cpp common.h
	clang++ $< -o $@ -std=c++11 -lboost_serialization -g -O0 -I$(HOME)/mtsp/v06 -L$(HOME)/.lib -lm -lmtsp

%: %.cpp common.h
	clang++ $< -o $@ -std=c++11 -lboost_serialization -g -O0

clean:
	rm -f dispatcher gen-dag
 
rebuild: clean all