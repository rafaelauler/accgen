CXX = g++

objects = Semantic.o TransformationRules.o Search.o

all: $(objects)

%.o: %.cpp %.h
	$(CXX) $^ $(CXX_FLAGS) -c

clean:
	rm -f *.o *.gch
