# export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH

all:
	g++ -c -Wall -Werror -fpic tasklab.cpp
	g++ -shared -o libtasklab.so tasklab.o
	g++ -c -Wall -Werror -fpic runtime.cpp
	g++ -shared -o libruntime.so runtime.o
	g++ -L. main.c -o main -lruntime -ldl

