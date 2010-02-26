

test: lex.o parser.o main.o
	$(MAKE) -C ../InsnSelector
	g++ $(CXX_FLAGS) main.o lex.o parser.o ../InsnSelector/Semantic.o ../InsnSelector/TransformationRules.o ../InsnSelector/Search.o -o test

main.o: main.cpp
	g++ $(CXX_FLAGS) -c main.cpp -o main.o

parser.o:	acllvm.tab.c lex.h ../InsnSelector/Semantic.h ../InsnSelector/TransformationRules.h
	g++ $(CXX_FLAGS) -c acllvm.tab.c -o parser.o

lex.o:	acllvm.tab.h lex.yybe.c ../InsnSelector/Semantic.h ../InsnSelector/TransformationRules.h
	g++ $(CXX_FLAGS) -c lex.yybe.c -o lex.o

acllvm.tab.h: acllvm.y
	bison --name-prefix=yybe -d acllvm.y

acllvm.tab.c: acllvm.y
	bison --name-prefix=yybe -d acllvm.y

lex.h:	acllvm.l
	flex -Pyybe --header-file=lex.h acllvm.l

lex.yybe.c:	acllvm.l
	flex -Pyybe --header-file=lex.h acllvm.l


clean:
	-rm *~ lex.yybe.c acllvm.tab.h acllvm.tab.c *.o lex.h test
	$(MAKE) clean -C ../InsnSelector
