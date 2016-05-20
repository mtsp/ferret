CPPFLAGS	= -g -std=c++11 -I$(HOME)/mtsp/v07
LIB_FLAGS   = -lboost_serialization -L$(HOME)/.lib -lm -lmtsp
OPT_LVL		= -O0

OBJS		= tasklab.o
FILENAME 	= dispatcher

all: $(FILENAME)

$(FILENAME): tasklab.o
	clang++ $(CPPFLAGS) $(LIB_FLAGS) $(OBJS) $@.cpp -o $@

%.o: %.cpp
	clang++ -c $(CPPFLAGS) $(OPT_LVL) $< -o $@

clean:
	rm -f *.o
 
rebuild: clean all

# LD_LIBRARY_PATH=$(HOME)/.lib