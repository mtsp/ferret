MTSP_INC	= $(HOME)/mtsp/v06

CPPFLAGS	= -g -std=c++11 -I$(MTSP_INC) -fPIC
LIB_FLAGS  	= -lm -ldl -lboost_serialization
OPT_LVL		= -O0

OBJS		= tasklab.o
LIB_NAME 	= tasklab.so

all: 
	@make ferret 
	@make tasklab

ferret: $(OBJS)
	clang++ $(CPPFLAGS) $(OBJS) $@.cpp -o $@ $(LIB_FLAGS) 

tasklab: $(OBJS)
	clang++ $(CPPFLAGS) $(OBJS) -shared -o $(LIB_NAME) $(LIB_FLAGS) 

%.o: %.cpp
	clang++ -c $(CPPFLAGS) $(OPT_LVL) $< -o $@

clean:
	rm -f *.o *.so ferret
 
rebuild: clean all

# toy:
# 	clang++ -O3 -fopenmp -g toy1.cpp -o toy -lm