
CXX = g++
ARCH_INC = -I/usr/local/archc/include/archc/ 
ARCH_ACPP_INC	= -I/p/archc-tools/archc-newbingen-branch/src/acpp/
ARCH_ACPP_LIB = -L/p/archc-tools/archc-newbingen-branch/src/acpp/.libs/

CXXFLAGS = $(ARCH_INC) $(ARCH_ACPP_INC) $(ARCH_ACPP_LIB)

all: genllvmbe

genllvmbe: genllvmbe.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ -lacpp

clean:
	rm -f *.o genllvmbe
