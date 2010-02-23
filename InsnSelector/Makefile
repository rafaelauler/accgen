CXX = g++

objects = Semantic.o TransformationRules.o Search.o

all: $(objects)

%.o: %.cpp %.h
	$(CXX) $^ -g -c

clean:
	rm -f *.o *.gch
