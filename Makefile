
CXX = g++
ARCH_INC = -I/usr/local/archc/include/archc/ 
ARCH_ACPP_INC	= -I/p/archc-tools/archc-newbingen-branch/src/acpp/
ARCH_ACPP_LIB = -L/p/archc-tools/archc-newbingen-branch/src/acpp/.libs/

CXXFLAGS = $(ARCH_INC) $(ARCH_ACPP_INC) $(ARCH_ACPP_LIB)

objects = ArchEmitter.o TemplateManager.o
all: $(objects) genllvmbe

%.o: %.cpp %.h
	$(CXX) $^ -g -c

genllvmbe: genllvmbe.cpp InsnFormat.h Insn.h $(objects)
	$(CXX) $(CXXFLAGS) $^ -g -o $@ -lacpp 

clean:
	rm -f *.o *.gch genllvmbe
