ifeq ($(HOSTNAME), chiclete)
        MTSP_INC = $(HOME)/mtsp/v06
else
        MTSP_INC = $(HOME)/Isaland/mtsp/v06
endif

CPPFLAGS	= -g -std=c++11 -I$(MTSP_INC) -fPIC
LIB_FLAGS  	= -lm -ldl -lboost_serialization
OPT_LVL		= -O0

OBJS		= tasklab.o
LIB_NAME 	= tasklab.so

all: 
	@make ferret 
	@make tasklab

ferret: $(OBJS)
	g++ $(CPPFLAGS) $(OBJS) $@.cpp -o $@ $(LIB_FLAGS) 

tasklab: $(OBJS)
	g++ $(CPPFLAGS) $(OBJS) -shared -o $(LIB_NAME) $(LIB_FLAGS) 

%.o: %.cpp
	g++ -c $(CPPFLAGS) $(OPT_LVL) $< -o $@

clean:
	rm -f *.o *.so ferret

 
rebuild: clean all
