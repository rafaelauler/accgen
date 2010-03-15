//===- acllvm.y - Bison input file for parser generation  --*- Bison -*----===//
//
//              The ArchC Project - Compiler Backend Generation
//
//===----------------------------------------------------------------------===//
//
// This bison input file is used to build a LALR(1) parser for transformation
// rules, instruction semantics and other information we may need that is
// not already in the ArchC model.
//
//===----------------------------------------------------------------------===//

%{

#include "InsnSelector/TransformationRules.h"
#include "InsnSelector/Semantic.h"
#include "InsnSelector/Search.h"
#include "Instruction.h"
#include <stack>
#include <map>

using namespace backendgen::expression;
using namespace backendgen;

OperandTableManager OperandTable;
OperatorTableManager OperatorTable;
TransformationRules RuleManager;
RegClassManager RegisterManager;
InstrManager InstructionManager;
FragmentManager FragMan;
std::map<std::string,unsigned> InsnOccurrencesMap;
unsigned LineNumber = 1;
// String list used to store fragment instance parameters
std::list<std::string> StrList;
// Expression stack used to process operators parameters
std::stack<Node *> Stack;
// Register stack used to process register lists, when defining register classes
std::stack<Register *> RegStack;
std::stack<Register *> SubRegStack;
// Clear stack and cleanly deallocated elements when an error occur
void ClearStack();

%}

%locations
%pure_parser
%error-verbose
%union {
  backendgen::expression::Node *treenode;
  int num;
  char *str;
}

%{
#include "lex.h"
void yyerror(char *error);
%}

%token<str> ID OPERATOR
%token<num> NUM LPAREN RPAREN DEFINE OPERATOR2 ARITY SEMICOLON CONST
%token<num> EQUIVALENCE LEADSTO ALIAS OPERAND SIZE LIKE COLON ANY ABI
%token<num> REGISTERS AS COMMA SEMANTIC INSTRUCTION TRANSLATE IMM COST
%token<num> CALLEE SAVE RESERVED RETURN CONVENTION FOR STACK ALIGNMENT
%token<num> CALLING FRAGMENT EQUALS LESS GREATER LESSOREQUAL GREATEROREQUAL
%token<num> LBRACE RBRACE PARAMETERS LEADSTO2

%type<treenode> exp operator;
%type<str> oper;
%type<num> explist convtype strlist comparator;

%%

/* A statement list, our initial rule */ 

statementlist:     /* empty */
                   | statementlist statement
                   ;

statement:         ruledef       {}
                   | opdef       {}
                   | opalias     {}
                   | operanddef  {}
                   | regclassdef {}
                   | semanticdef {}
                   | fragdef     {}
                   | abidef      {}
                   | translate   {}
                   | error       {} 
                   ;

/* Translation request */

translate:         TRANSLATE exp SEMICOLON
                   {
		     Search S(RuleManager, InstructionManager);
		     unsigned SearchDepth = 5;
		     SearchResult *R = NULL;
		     while (R == NULL || R->Instructions->size() == 0) {
		       if (SearchDepth == 10)
			 break;
		       std::cout << "Trying search with depth " << SearchDepth
				 << "\n";
		       S.setMaxDepth(SearchDepth++);
		       R = S($2, 0);
		     }
		     std::cout << "Translation results\n=======\n";
		     if (R == NULL)
		       std::cout << "Not found!\n";
		     else {
		       if (R->Instructions->size() == 0)
			 std::cout << "Not found!\n";
		       else {
			 OperandsDefsType::const_iterator I1 =
			   R->OperandsDefs->begin();
			 for (InstrList::iterator I = 
				R->Instructions->begin(), 
				E = R->Instructions->end(); I != E; ++I)
			   {			     
			     //std::cout << "Operands for this insn: ";
			     //for (NameListType::const_iterator I2 = 
			     //    (*I1)->begin(), E2 = (*I1)->end();
			     //	  I2 != E2; ++I2) {
			     //  std::cout << *I2 << " ";
			     //}
			     //std::cout << "\n";
			     I->first->emitAssembly(*I1, I->second, std::cout);
			     ++I1;
			     //(I->first)->print(std::cout);
			   }
		       }
		       delete R;
		     }
		     delete $2;
		     std::cout << "\n";
                   }
                   ;

/* Semantic fragments defintion. */

fragdef:           DEFINE SEMANTIC FRAGMENT ID AS LPAREN explist SEMICOLON
                   RPAREN SEMICOLON
                   {
		     int I = Stack.size() - 1;
		     while (I >= 0) {
		       FragMan.addFragment($4, Stack.top());
		       Stack.pop();
		       --I;
		     }
		     free($4);
                   }

/* Instruction semantic description. */

semanticdef:       DEFINE INSTRUCTION ID SEMANTIC AS LPAREN semanticlist
                   RPAREN COST NUM SEMICOLON
                   {
		     if (InsnOccurrencesMap.find($3) 
			 == InsnOccurrencesMap.end())
		       InsnOccurrencesMap[$3] = 1;
                     Instruction *Instr = 
		       InstructionManager.getInstruction
		       ($3, InsnOccurrencesMap[$3]++);
		     if (Instr == NULL) {
		       if (InsnOccurrencesMap[$3] > 2) {
			 std::cerr << "Line " << LineNumber << 
			   ": Excessive semantic overload for" <<
			   " instruction \"" << $3 << "\".\n";
		       } else {
			 std::cerr << "Line " << LineNumber << 
			   ": Undefined instruction \"" << $3 << "\".\n";
		       }
		       free($3);
		       ClearStack();
		       YYERROR;
		     }
		     Instr->setCost($10);
                     int I = Stack.size() - 1;
                     while (I >= 0) {
		       Tree* root = Stack.top();
		       if (!FragMan.expandTree(&root))
			 {
			   std::cerr << "Line " << LineNumber <<
			     ": Failure to expand pattern fragments into " 
				     << "instruction \"" << $3 << 
			     "\" semantic.\n"; 
			   free($3);
			   ClearStack();
			   YYERROR;
			 }
                       Instr->addSemantic(Stack.top());
                       Stack.pop();
                       --I;
                     }
		     free($3);
                   }
                   ;

semanticlist:      /* empty */
                   | semanticlist semantic
                   {
                   }
                   ;

semantic:          exp SEMICOLON
                   {
                     Stack.push($1);
                   }
                   ;

/* New operator definition. */

opdef:    DEFINE OPERATOR2 oper AS ARITY NUM SEMICOLON
             {
               OperatorType NewType = OperatorTable.getType($3);
               OperatorTable.updateArity(NewType, $6);
	       free($3);
             }
          ;

/* Operator alias definition. */

oper: OPERATOR | ID;

opalias:  DEFINE OPERATOR2 ALIAS AS oper LEADSTO oper SEMICOLON
             {
	       OperatorTable.setAlias($5, $7);  
	       free($5);
	       free($7);
             }
          ;

/* Operand definition. */

operanddef: DEFINE OPERAND ID AS SIZE NUM SEMICOLON
             {
               OperandType NewType = OperandTable.getType($3); 
	       free($3);
               OperandTable.updateSize(NewType, $6);
             }
            | DEFINE OPERAND ID AS SIZE NUM LIKE ID SEMICOLON
             {
               OperandType NewType = OperandTable.getType($3); 
               OperandTable.updateSize(NewType, $6);
               OperandTable.setCompatible($3, $8);
	       free($3);
	       free($8);
             }
          ;

/* ABI Stuff definition - CalleeSave and Reserved Registers lists */

abidef:      DEFINE ABI AS LPAREN abistufflist RPAREN SEMICOLON
             ;

abistufflist: /* empty */
              | abistufflist abistuff
              ;

abistuff:    DEFINE CALLEE SAVE REGISTERS AS LPAREN regdefs RPAREN SEMICOLON
             {
               int I = RegStack.size() - 1;
               while (I >= 0) {
                 Register *Reg = RegStack.top();
                 RegStack.pop();
                 RegisterManager.addCalleeSaveRegister(Reg);
                 --I;
               }
             }
             | DEFINE RESERVED REGISTERS AS LPAREN regdefs RPAREN SEMICOLON
             {
               int I = RegStack.size() - 1;
               while (I >= 0) {
                 Register *Reg = RegStack.top();
                 RegStack.pop();
                 RegisterManager.addReservedRegister(Reg);
                 --I;
               }
             }
             | DEFINE convtype CONVENTION FOR ID AS LPAREN regdefs RPAREN 
	       SEMICOLON
             {
	       CallingConvention* CC = new CallingConvention(($2 == 1)? 
                                                             true:false, false);
	       CC->Type = OperandTable.getType($5);
	       free($5);
	       int I = RegStack.size() - 1;
	       while (I >= 0) {
		 Register *Reg = RegStack.top();
		 RegStack.pop();
		 CC->addRegister(Reg);
		 --I;
	       }
	       RegisterManager.addCallingConvention(CC);
             }
             | DEFINE convtype CONVENTION FOR ID AS STACK SIZE NUM ALIGNMENT
               NUM SEMICOLON
             {
               CallingConvention* CC = new CallingConvention(($2 == 1)? 
                                                             true:false, true);
	       CC->Type = OperandTable.getType($5);
	       free($5);
	       CC->StackSize = $9;
               CC->StackAlign = $11;
	       RegisterManager.addCallingConvention(CC);
             }
             ;

convtype:    RETURN { $$ = 1; }
             | CALLING { $$ = 0; }
             ;

/* Register class definition. */

regclassdef: DEFINE REGISTERS ID COLON ID AS LPAREN regdefs RPAREN SEMICOLON
             {
               RegisterClass *RegClass = new RegisterClass($3,
                 OperandTable.getType($5));
	       free($3);
	       free($5);
               RegisterManager.addRegClass(RegClass);
               int I = RegStack.size() - 1;
               while (I >= 0) {
                 Register *Reg = RegStack.top();
                 RegStack.pop();
                 RegClass->addRegister(Reg);
                 --I;
               }
             }
             ;

/* Register definitions */

regdefs:     /* empty */
             | regdefs regdef  {
                               }
             ;

regdef:      ID                {
                                 Register* Reg = 
                                   RegisterManager.getRegister($1);
                                 if (Reg == NULL) {
                                   Reg = new Register($1);
                                   RegisterManager.addRegister(Reg);
                                 }
				 free($1);
                                 RegStack.push(Reg);
                               }
             | ID LPAREN subregdefs RPAREN
                               {
                                 Register* Reg = 
                                   RegisterManager.getRegister($1);
                                 if (Reg == NULL) {
                                   Reg = new Register($1);
                                   RegisterManager.addRegister(Reg);
                                 }
				 free($1);
                                 int I = SubRegStack.size() - 1;
                                 while (I >= 0) {
                                   Register *SubReg = SubRegStack.top();
                                   SubRegStack.pop();
                                   Reg->addSubClass(SubReg);
                                   --I;
                                 }
                                 RegStack.push(Reg);
                               }
             ;

subregdefs:  /* empty */
             | subregdefs subregdef
             ;

subregdef:   ID                { 
                                 Register* Reg = 
                                   RegisterManager.getRegister($1);
				 if (Reg == NULL) {                 
                                   Reg = new Register($1);
                                   RegisterManager.addRegister(Reg);
                                 }
				 free($1);
                                 SubRegStack.push(Reg);
                               }

/* New rule definition. */

ruledef:  exp EQUIVALENCE exp SEMICOLON
             {
               RuleManager.createRule($1, $3, true);
             }
          | exp LEADSTO exp SEMICOLON
             {
               RuleManager.createRule($1, $3, false);
             }
          ;

/* Expressions */

comparator: EQUALS | LESS | GREATER | LESSOREQUAL | GREATEROREQUAL;

strlist:   /* empty */ {}
          | strlist ID { StrList.push_back($2); free($2); }
          ; 

explist:  /* empty */ {}
          | explist exp  { Stack.push($2); }         
          ; 

exp:      LBRACE exp comparator exp RBRACE LEADSTO2 LPAREN operator explist 
          RPAREN
                    {
		      AssignOperator *Op = dynamic_cast<AssignOperator*>($8);
		      if (Op == NULL) {
			std::cerr << "Line " << LineNumber << ": Using a "
			       << "guarded assignment in something that is not "
			       << "an assignment.\n";
			ClearStack();
			YYERROR;
		      }
		      if (Stack.size() != 2) {
			std::cerr << "Line " << LineNumber << ": Invalid number"
				  << " of operands for assignment operator.\n";
			ClearStack();
			YYERROR;
		      }
		      (*dynamic_cast<Operator*>($8))[1] = Stack.top();
		      Stack.pop();
		      (*dynamic_cast<Operator*>($8))[0] = Stack.top();
		      Stack.pop();
                      Op->setPredicate(new Predicate($3, $2, $4));
		      $$ = $8;
                    }
          | LPAREN operator explist RPAREN 
                    {
                      int i = dynamic_cast<Operator*>($2)->getArity();         
	              if (i > Stack.size())
                      {
                        std::cerr << "Line " << LineNumber << ": Number"
                          << " of operands does not match \"" << 
                        OperatorTable.getOperatorName(
                          dynamic_cast<Operator*>($2)->getOpType()) << "\""
                          << " arity.\n";
                        ClearStack();
                        YYERROR;
                      }    
	              --i;
                      while (i >= 0) {
                        (*dynamic_cast<Operator*>($2))[i] = Stack.top();
                        Stack.pop();
		        --i;
                      }
                      $$ = $2;
                    }
          | LPAREN operator COLON ID explist RPAREN
                    {
                      int i = dynamic_cast<Operator*>($2)->getArity();
                      if (i > Stack.size())
                      {
                        std::cerr << "Line " << LineNumber << ": Number"
                          << " of operands does not match \"" << 
                        OperatorTable.getOperatorName(
                          dynamic_cast<Operator*>($2)->getOpType()) << "\""
                          << " arity.\n";
                        ClearStack();
                        YYERROR;
                      }
                      --i;
                      while (i >= 0) {
                        (*dynamic_cast<Operator*>($2))[i] = Stack.top();
                        Stack.pop();
                        --i;
                      }
                      dynamic_cast<Operator*>($2)->setReturnType(
                        OperandTable.getType($4));
		      free($4);
                      $$ = $2;
                    }
          | CONST COLON ID COLON NUM
                    {                       
                      $$ = new Constant($5, OperandTable.getType($3)); 
		      free($3);
                    }
          | ID      { 
                      $$ = new Operand(OperandTable.getType($1), "E");
		      free($1);
                    }
          | IMM COLON ID COLON ID
                    {
                      $$ = new ImmediateOperand(OperandTable.getType($5), $3);
		      free($3);
		      free($5);
                    }
          | ID COLON FRAGMENT
	            {
		      StrList.clear();
                      $$ = new FragOperand($1, StrList);
		      free($1);
                    }
          | ID COLON FRAGMENT PARAMETERS LPAREN strlist RPAREN
                    {
                      $$ = new FragOperand($1, StrList);
                      free($1);
                      StrList.clear();
                    }
          | ID COLON ID
                    { 
                      RegisterClass *RegClass = RegisterManager.getRegClass($3);
                      if (RegClass == NULL)
                        $$ = new Operand(OperandTable.getType($3), $1); 
                      else 
                        $$ = new RegisterOperand(RegClass, $1);  
		      free($1);
		      free($3);
                    }
          | ID COLON ANY  
	            {
                      OperandType NewType;
                      NewType.Type = 0;
                      NewType.Size = 0;
		      NewType.DataType = 0;   
		      $$ = new Operand(NewType, $1);
		      free($1);
                    }
          ;


operator: OPERATOR      { 
                          $$ = Operator::BuildOperator
			    (OperatorTable, OperatorTable.getType($1));
			  free($1);
                        }
          | ID          { 
                          $$ = Operator::BuildOperator
			    (OperatorTable, OperatorTable.getType($1));
			  free($1);
                        }
          ;

%%

void yyerror (char *error)
{
  fprintf(stderr, "Line %d: %s\n", LineNumber, error);
}

void ClearStack() {
  int I = Stack.size() - 1;
  while (I >= 0)
  {
    delete Stack.top();
    Stack.pop();
    --I;
  }
}
