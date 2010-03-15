
ARCH_SRC_DIR := ../archc/src

CXX = g++
ARCH_INC = -I../install/include/archc/ 
ARCH_ACPP_INC	= -I$(ARCH_SRC_DIR)/acpp/
ARCH_ACPP_LIB = -L$(ARCH_SRC_DIR)/acpp/.libs/

CXXFLAGS = $(ARCH_INC) $(ARCH_ACPP_INC) $(ARCH_ACPP_LIB)

objects = ArchEmitter.o TemplateManager.o lex.o parser.o Semantic.o TransformationRules.o Search.o Instruction.o CMemWatcher.o
all: $(objects) genllvmbe

%.o: %.cpp %.h
	$(CXX) $^ -g -Wall -Werror -c

#
# These entries make sure we produce the correct parser.o and lex.o related
# files
#
Semantic.o: InsnSelector/Semantic.cpp InsnSelector/Semantic.h
	$(CXX) $^ -g -Wall -Werror -c
TransformationRules.o: InsnSelector/TransformationRules.cpp InsnSelector/TransformationRules.h
	$(CXX) $^ -g -Wall -Werror -c
Search.o: InsnSelector/Search.cpp InsnSelector/Search.h
	$(CXX) $^ -g -Wall -Werror -c
parser.o: acllvm.tab.c lex.h InsnSelector/Semantic.h InsnSelector/TransformationRules.h
	$(CXX) $(CXX_FLAGS) -c acllvm.tab.c -o parser.o

lex.o: acllvm.tab.h lex.yybe.c InsnSelector/Semantic.h InsnSelector/TransformationRules.h
	$(CXX) $(CXX_FLAGS) -c lex.yybe.c -o lex.o

acllvm.tab.h: Parser/acllvm.y
	bison -v --name-prefix=yybe -d Parser/acllvm.y

acllvm.tab.c: Parser/acllvm.y
	bison -v --name-prefix=yybe -d Parser/acllvm.y

lex.h: Parser/acllvm.l
	flex -Pyybe --header-file=lex.h Parser/acllvm.l

lex.yybe.c: Parser/acllvm.l
	flex -Pyybe --header-file=lex.h Parser/acllvm.l
#
# End of specific entries
#

genllvmbe: genllvmbe.cpp InsnFormat.h $(objects)
	$(CXX) $(CXXFLAGS) -Wall -Werror $^ -g -o $@ -lacpp 

clean:
	rm -f *.o *.gch genllvmbe $(objects) acllvm.tab.h acllvm.tab.c lex.h lex.yybe.c *~ InsnSelector/*.gch
