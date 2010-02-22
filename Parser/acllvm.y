//===- acllvm.y - Bison input file for parser generation  -----------------===//
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

#include "../InsnSelector/TransformationRules.h"
#include "../InsnSelector/Semantic.h"
#include <stack>

using namespace backendgen::expression;
using namespace backendgen;

OperandTableManager OperandTable;
OperatorTableManager OperatorTable;
TransformationRules RuleManager;
RegClassManager RegisterManager;
InstrManager InstructionManager;
unsigned LineNumber = 1;
std::stack<Node *> Stack;
std::stack<Register *> RegStack;
std::stack<Register *> SubRegStack;

void ClearStack();

  /*#define YYPARSE_PARAM scanner
    #define YYLEX_PARAM   scanner*/

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
%token<num> EQUIVALENCE LEADSTO ALIAS OPERAND SIZE LIKE COLON ANY
%token<num> REGISTERS AS COMMA SEMANTIC INSTRUCTION TRANSLATE

%type<treenode> exp operator;
%type<str> oper;
%type<num> explist;

%%

/*
program:  
          | ruledef
          | opdef
	  | error
	  ;
*/

/* A statement list */ 

statementlist:     /* empty */
                   | statementlist statement
                   ;

statement:         ruledef       {}
                   | opdef       {}
                   | opalias     {}
                   | operanddef  {}
                   | regclassdef {}
                   | semanticdef {}
                   | translate   {}
                   | error       {} 
                   ;

/* Translation request */

translate:         TRANSLATE exp SEMICOLON
                   {
                   }
                   ;

/* Instruction semantic description. */

semanticdef:       DEFINE INSTRUCTION ID SEMANTIC AS LPAREN semanticlist
                   RPAREN SEMICOLON
                   {
                     Instruction *Instr = InstructionManager.getInstruction($3);
                     int I = Stack.size() - 1;
                     while (I >= 0) {
                       Instr->addSemantic(Stack.top());
                       Stack.pop();
                       --I;
                     }
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
             }
          ;

/* Operator alias definition. */

oper: OPERATOR | ID;

opalias:  DEFINE OPERATOR2 ALIAS AS oper LEADSTO oper SEMICOLON
             {
	       OperatorTable.setAlias($5, $7);  
             }
          ;

/* Operand definition. */

operanddef: DEFINE OPERAND ID AS SIZE NUM SEMICOLON
             {
               OperandType NewType = OperandTable.getType($3); 
               OperandTable.updateSize(NewType, $6);
             }
            | DEFINE OPERAND ID AS SIZE NUM LIKE ID SEMICOLON
             {
               OperandType NewType = OperandTable.getType($3); 
               OperandTable.updateSize(NewType, $6);
               OperandTable.setCompatible($3, $8);
             }
          ;

/* Register class definition. */

regclassdef: DEFINE REGISTERS ID COLON ID AS LPAREN regdefs RPAREN SEMICOLON
             {
               RegisterClass *RegClass = new RegisterClass($3,
                 OperandTable.getType($5));
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

explist:  /* empty */ {}
          | explist exp  { Stack.push($2); }
          | explist ANY  {
                           OperandType NewType;
                           NewType.Type = 0;
                           NewType.Size = 0;
			   NewType.DataType = 0;
                           Stack.push(new Operand(NewType));
                         }
          ; 

exp:      LPAREN operator explist RPAREN 
                    {
                      int i = dynamic_cast<Operator*>($2)->getArity();         
	              if (i > Stack.size())
                      {
                        std::cerr << "Line " << LineNumber << ": Number"
                          << " of operands does not match \"" << 
                        OperatorTable.getOperatorName(
                          dynamic_cast<Operator*>($2)->getType()) << "\""
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
                          dynamic_cast<Operator*>($2)->getType()) << "\""
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
                      $$ = $2;
                    }
          | LPAREN CONST COLON ID NUM RPAREN    
                    {                       
                      $$ = new Constant($5, OperandTable.getType($4)); 
                    }
          |  ID     { 
	              $$ = new Operand(OperandTable.getType($1));
                    }
          | ID COLON ID
                    { 
                      RegisterClass *RegClass = RegisterManager.getRegClass($3);
                      if (RegClass == NULL) {
                        std::cerr << "Line " << LineNumber << ": Unknown"
                          << " register class \"" << $3 << "\".\n";
                        YYERROR;
                      }
                      else 
                        $$ = new RegisterOperand(RegClass, $1);  
                    }
          ;


operator: OPERATOR      { 
                          $$ = new Operator(OperatorTable.getType($1));
                        }
          | ID          { 
                          $$ = new Operator(OperatorTable.getType($1));
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
    //Stack.top()->print();
    delete Stack.top();
    Stack.pop();
    --I;
  }
}
