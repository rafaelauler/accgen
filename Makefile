
ARCH_SRC_DIR := ../archc/src

ARCH_INC = -I../install/include/archc/ 
ARCH_ACPP_INC	= -I$(ARCH_SRC_DIR)/acpp/
ARCH_ACPP_LIB = -L$(ARCH_SRC_DIR)/acpp/.libs/

ifeq ($(PARALLEL_SEARCH),1)
  CXXFLAGS1 = $(ARCH_INC) $(ARCH_ACPP_INC) $(ARCH_ACPP_LIB) -fopenmp -DPARALLEL_SEARCH
  FLAGS1 = -fopenmp -DPARALLEL_SEARCH
else
  CXXFLAGS1 = $(ARCH_INC) $(ARCH_ACPP_INC) $(ARCH_ACPP_LIB) -DSTATIC_TRANSCACHE
  FLAGS1 = -DSTATIC_TRANSCACHE
endif

ifeq ($(DEBUG),1)
  CXXFLAGS =  $(CXXFLAGS1) -g
  FLAGS = $(FLAGS1) -g
else
  CXXFLAGS = $(CXXFLAGS1) -O3
  FLAGS = $(FLAGS1) -O3
endif


objects = ArchEmitter.o TemplateManager.o lex.o parser.o Semantic.o TransformationRules.o Search.o Instruction.o CMemWatcher.o PatternTranslator.o LLVMDAGInfo.o SaveAgent.o
all: $(objects) genllvmbe

%.o: %.cpp %.h
	$(CXX) $^ -Wall -Werror $(FLAGS) -c

#
# These entries make sure we produce the correct parser.o and lex.o related
# files
#
Semantic.o: InsnSelector/Semantic.cpp InsnSelector/Semantic.h
	$(CXX) $^ -Wall -Werror $(FLAGS) -c
TransformationRules.o: InsnSelector/TransformationRules.cpp InsnSelector/TransformationRules.h
	$(CXX) $^ -Wall -Werror $(FLAGS) -c
Search.o: InsnSelector/Search.cpp InsnSelector/Search.h
	$(CXX) $^ -Wall -Werror $(FLAGS) -c
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
	$(CXX) $(CXXFLAGS) -Wall -Werror $^ -o $@ -lacpp -lboost_regex

clean:
	rm -f *.o *.gch genllvmbe $(objects) acllvm.tab.h acllvm.tab.c lex.h lex.yybe.c *~ InsnSelector/*.gch
