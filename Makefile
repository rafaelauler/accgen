
ARCH_SRC_DIR := /l/home/rafael/disco2/rafael/archc/archc-newbingen-branch/src

CXX = g++
ARCH_INC = -I/l/home/rafael/disco2/rafael/Eval/install/include/archc/ 
ARCH_ACPP_INC	= -I$(ARCH_SRC_DIR)/acpp/
ARCH_ACPP_LIB = -L$(ARCH_SRC_DIR)/acpp/.libs/

CXXFLAGS = $(ARCH_INC) $(ARCH_ACPP_INC) $(ARCH_ACPP_LIB)

objects = ArchEmitter.o TemplateManager.o lex.o parser.o Semantic.o TransformationRules.o Search.o
all: $(objects) genllvmbe

%.o: %.cpp %.h
	$(CXX) $^ -g -c

#
# These entries make sure we produce the correct parser.o and lex.o related
# files
#
Semantic.o: InsnSelector/Semantic.cpp InsnSelector/Semantic.h
	$(CXX) $^ -g -c
TransformationRules.o: InsnSelector/TransformationRules.cpp InsnSelector/TransformationRules.h
	$(CXX) $^ -g -c
Search.o: InsnSelector/Search.cpp InsnSelector/Search.h
	$(CXX) $^ -g -c
parser.o: acllvm.tab.c lex.h InsnSelector/Semantic.h InsnSelector/TransformationRules.h
	$(CXX) $(CXX_FLAGS) -c acllvm.tab.c -o parser.o

lex.o: acllvm.tab.h lex.yybe.c InsnSelector/Semantic.h InsnSelector/TransformationRules.h
	$(CXX) $(CXX_FLAGS) -c lex.yybe.c -o lex.o

acllvm.tab.h: Parser/acllvm.y
	bison --name-prefix=yybe -d Parser/acllvm.y

acllvm.tab.c: Parser/acllvm.y
	bison --name-prefix=yybe -d Parser/acllvm.y

lex.h: Parser/acllvm.l
	flex -Pyybe --header-file=lex.h Parser/acllvm.l

lex.yybe.c: Parser/acllvm.l
	flex -Pyybe --header-file=lex.h Parser/acllvm.l
#
# End of specific entries
#

genllvmbe: genllvmbe.cpp InsnFormat.h Insn.h $(objects)
	$(CXX) $(CXXFLAGS) $^ -g -o $@ -lacpp 

clean:
	rm -f *.o *.gch genllvmbe $(objects) acllvm.tab.h acllvm.tab.c lex.h lex.yybe.c *~ InsnSelector/*.gch
