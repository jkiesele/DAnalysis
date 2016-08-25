
CPP_FILES := $(wildcard src/*.cpp)
OBJ_FILES := $(addprefix obj/,$(notdir $(CPP_FILES:.cpp=.o)))
LD_FLAGS := `root-config --cflags --glibs` -lMinuit -lMathMore -lMinuit2
LD_FLAGS += -L$(DELPHES_PATH) -lDelphes
CC_FLAGS := -fPIC -c -g -Wall `root-config --cflags`
CC_FLAGS += -I$(DELPHES_PATH)
CC_FLAGS += -MMD


all: libDAnalysis.so stackPlotter

libDAnalysis.so: $(OBJ_FILES)
	g++ -shared $(LD_FLAGS) -o $@ $^
	
	
stackPlotter: libDAnalysis.so
	g++ -Wall $(LD_FLAGS) -L$(DANALYSISPATH) -o $@ $^  
	

obj/%.o: src/%.cpp
	g++ $(CC_FLAGS) -c -o $@ $<


clean: 
	rm -f obj/*.o obj/*.d
	rm -f libDAnalysis.so
	rm -f stackPlotter

-include $(OBJ_FILES:.o=.d)
