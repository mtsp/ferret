ifeq ($(HOSTNAME), chiclete)
        MTSP_INC = $(HOME)/mtsp/v06
else
        MTSP_INC = $(HOME)/Isaland/mtsp/integration
endif

CPPFLAGS	= -g -std=c++11 -I$(MTSP_INC) -fPIC
LIB_FLAGS  	= -lm -ldl -lboost_serialization -lboost_filesystem -lboost_system
OPT_LVL		= -O0

OBJS		= tasklab.o
LIB_NAME 	= tasklab.so

CPP		    = g++

all: 
	@make ferret 
	@make tasklab

ferret: $(OBJS)
	$(CPP) $(CPPFLAGS) $(OBJS) $@.cpp -o $@ $(LIB_FLAGS) 

tasklab: $(OBJS)
	$(CPP) $(CPPFLAGS) $(OBJS) -shared -o $(LIB_NAME) $(LIB_FLAGS) 

%.o: %.cpp
	$(CPP) -c $(CPPFLAGS) $(OPT_LVL) $< -o $@

clean:
	rm -f *.o *.so ferret

 
rebuild: clean all
