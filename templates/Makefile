
CPP_FILES := $(wildcard src/*.cpp)
OBJ_FILES := $(addprefix obj/,$(notdir $(CPP_FILES:.cpp=.o)))
LD_FLAGS := `root-config --cflags --glibs` -lMinuit -lMathMore -lMinuit2
LD_FLAGS += -L$(DELPHES_PATH) -lDelphes -L$(DANALYSISPATH) -lDAnalysis
CC_FLAGS := -fPIC -g -Wall `root-config --cflags`
CC_FLAGS += -I$(DELPHES_PATH) -I$(DANALYSISPATH) -I.
#CC_FLAGS += -MMD


all: $(patsubst bin/%.cpp, %, $(wildcard bin/*.cpp))

%: bin/%.cpp Makefile $(OBJ_FILES)
	g++ $(CC_FLAGS) $(LD_FLAGS) $(OBJ_FILES) $< -o $@ 


obj/%.o: src/%.cpp
	g++ $(CC_FLAGS) -c -o $@ $<


clean: 
	rm -f obj/*.o obj/*.d
	rm -f %
