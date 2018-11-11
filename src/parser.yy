%{
    #include <cstdlib>
    #include <string>
    #include <stack>
    #include <vector>
    #include <iostream>
    #include <bits/stdc++.h> //cstr
    #include "../src/symtable/symtable.hpp"
    #include "../src/ast/ExprNode.hpp"
    #include "../src/ast/StmtNode.hpp"

    extern int yylex();
    extern int yylineno;
    extern char* yytext;
    void yyerror(const char* s);
%}

%code{
    int block_index = 0;
    Symtable* globalSymtable;
    std::stack<Symtable*> symtable_stack;
    std::stack<std::string> id_stack;
    std::vector<StmtNode*> stmt_list;   //will remove
    std::vector<BlockNode*> block_list;
    std::vector<FunctionDeclNode*> func_list;
    std::stack<ExprNode*>* expr_stack_ptr; // used when calling function...
    std::stack<std::stack<ExprNode*>*> expr_stack_ptr_stack; // this is soooo f**king dumb
                                                             // however has to be here for the f**king sake
                                                             // of nested function call


    // search through the symbol table,
    // return VarRef* with the name and type of the symbol.
    VarRef* find_id(std::string& id){
        std::stack<Symtable*> now_stack = symtable_stack;
        
        SymEntry* entry = NULL;

        while(!entry && !now_stack.empty()){
            Symtable* now = now_stack.top();
            entry = now -> have(id);    
            now_stack.pop();
        }
        
        if(!entry){
            //error handling
            yyerror("does not exist in scope!\n");
        }
        
        VarRef* new_ref = new VarRef(id, entry->type);
        return new_ref;
    }
}

%define api.token.prefix{TOK_}
%union {
    char*           cstr;
	int             ival;
	float           fval;
    char            ch;
    std::string*    sp;
    ExprNode*       en;
    StmtNode*       sn;
}

/* Keywords */
%token          PROGRAM
%token          BEGIN
%token          END
%token          FUNCTION
%token          READ
%token          WRITE
%token          IF
%token          ELSE
%token          ENDIF
%token          WHILE
%token          ENDWHILE
%token          RETURN
%token          INT
%token          VOID
%token          STRING
%token          FLOAT
%token          TRUE
%token          FALSE

/* Data type */
%token <cstr>    IDENTIFIER
%token <ival>    INTLITERAL
%token <fval>    FLOATLITERAL
%token <cstr>    STRINGLITERAL

%token          ASSIGN 
%token <cstr>   NEQ 
%token <cstr>   LEQ
%token <cstr>   GEQ 
%token <cstr>   PLUS
%token <cstr>   MINUS
%token <cstr>   MUL 
%token <cstr>   DIV
%token <cstr>   EQ
%token <cstr>   LT
%token <cstr>   GT 
%token          OPAREN
%token          CPAREN
%token          SEMICOLON
%token          COMMA

/*%type <entry> string_decl*/
%type <sp> id str var_type any_type 
%type <en> expr expr_prefix postfix_expr factor factor_prefix primary call_expr cond 
%type <sn> assign_expr 
%type <ch> addop mulop
%type <cstr> compop
%type <ival> param_decl_list param_decl_tail
%start program
%%
/* Grammar rules */
/* Program */
program             :PROGRAM {
                        Symtable* current = new Symtable("GLOBAL");
                        globalSymtable = current;
                        symtable_stack.push(current);
                    }
                    id{delete $3;} BEGIN pgm_body END;

id                  :IDENTIFIER {
                        $$ = new std::string($1);
                    };
pgm_body            :decl func_declarations;
decl                :string_decl decl|var_decl decl|/* empty */;

/* Global String Declaration */
string_decl         :STRING id ASSIGN str SEMICOLON {
                        Symtable* current = symtable_stack.top();
                        StrEntry* new_entry = new StrEntry(*$2,*$4);
                        current->add(new_entry);
                        delete $2;
                        delete $4;
                    };
str                 :STRINGLITERAL{
                        $$ = new std::string($1);
                    };

/* Variable Declaration */
var_decl            :{
                        //init for reading variables
                        while(!id_stack.empty())id_stack.pop();
                    }
                    var_type id_list SEMICOLON {
                        Symtable* current = symtable_stack.top();
                        while(!id_stack.empty()){
                            if(*$2 == "INT"){
                                IntEntry* new_entry = new IntEntry(id_stack.top());
                                current->add(new_entry);
                            }
                            else{
                                FltEntry* new_entry = new FltEntry(id_stack.top());
                                current->add(new_entry);
                            }
                            id_stack.pop();
                        }
                        delete $2;
                    };
var_type            :FLOAT{
                        $$ = new std::string("FLOAT");  
                    }
                    |INT {
                        $$ = new std::string("INT");  
                    };
any_type            :var_type{
                        $$ = $1;
                    }    
                    |VOID{
                        $$ = new std::string("VOID");
                    }; 

id_list             :id id_tail {
                        id_stack.push(*($1));
                        delete $1;
                    };

id_tail             :COMMA id id_tail{
                        id_stack.push(*($2));
                        delete $2;
                    }
                    |/* empty */;

/* Function Paramater List */

param_decl_list     :param_decl param_decl_tail{$$ = $2 + 1;}
                    |/* empty */{$$ = 0;};

param_decl          :var_type id{
                        Symtable* current = symtable_stack.top(); 
                        if(*$1 == "INT"){
                            IntEntry* new_entry = new IntEntry(*$2);
                            current->add(new_entry);
                        }
                        else{
                            FltEntry* new_entry = new FltEntry(*$2);
                            current->add(new_entry);
                        }

                        delete $1;
                        delete $2;
                    };
param_decl_tail     :COMMA param_decl param_decl_tail{$$ = $3 + 1;}|/* empty */{$$ = 0;};

/* Function Declarations */
func_declarations   :func_decl func_declarations|/* empty */;
func_decl           :FUNCTION any_type id OPAREN param_decl_list CPAREN {
                    
                        // add function declaration to the symbol table
                        Symtable* current = symtable_stack.top();
                        FuncEntry* new_entry = new FuncEntry(*$3, *$2, $5); 
                        current->add(new_entry); 

                        // allocate symboltable for the new function
                        current = new Symtable(*$3);
                        symtable_stack.push(current);
                        //symtable_list.push_back(current);
                        FunctionDeclNode* new_func = new FunctionDeclNode(*$3,*$2,current);
                        block_list.push_back(new_func);
                        func_list.push_back(new_func);
                        // for now
                        delete $2;
                        delete $3;
                    }
                    BEGIN func_body END{
                        symtable_stack.pop();
                        block_list.pop_back();
                    };
func_body           :decl stmt_list;

/* Statement List */
stmt_list           :stmt stmt_list|/* empty */;
stmt                :base_stmt|if_stmt|loop_stmt; 
base_stmt           :assign_stmt|read_stmt|write_stmt|control_stmt;

/* Basic Statements */
assign_stmt         :assign_expr SEMICOLON{
                        block_list.back()->stmt_list.push_back($1); 
                    };

assign_expr         :id ASSIGN expr{
                        AssignStmtNode* new_assign = new AssignStmtNode(); 

                        // search the current symbol stack to find the table;
                        VarRef* to = find_id(*$1);
                        delete $1;  //free memory of id

                        // $3 should return a ExprNode*
                        new_assign -> to = to;
                        new_assign -> from = $3;
                        $$ = new_assign;
                    };
read_stmt           :{
                        while(!id_stack.empty())id_stack.pop();
                    }
                    READ OPAREN id_list CPAREN SEMICOLON{
                        ReadStmtNode* new_read = new ReadStmtNode();
                        while(!id_stack.empty()){
                            new_read->id_list.push_back(find_id(id_stack.top()));
                            id_stack.pop();
                        }
                        block_list.back()->stmt_list.push_back(new_read);
                    };
write_stmt          :{
                        while(!id_stack.empty())id_stack.pop();
                    }WRITE OPAREN id_list CPAREN SEMICOLON{
                        WriteStmtNode* new_write = new WriteStmtNode();
                        while(!id_stack.empty()){
                            new_write->id_list.push_back(find_id(id_stack.top()));
                            id_stack.pop();
                        }
                        block_list.back()->stmt_list.push_back(new_write);

                        //do something
                    };
return_stmt         :RETURN expr SEMICOLON{
                        block_list.back()->stmt_list.push_back(new ReturnStmtNode($2)); 
                    };

/* Expressions */
expr                :expr_prefix factor {
                        if($1){
                            $1 -> rnode = $2; // add right oprand to the exprnode
                            $$ = $1;
                        }
                        else $$ = $2;
                    };
expr_prefix         :expr_prefix factor addop {
                        $$ = new AddExprNode($3);
                        if($1){
                            $1 -> rnode = $2;
                            $$ -> lnode = $1;
                        }
                        else $$ -> lnode = $2; 
                    } | /* empty */{$$ = NULL;};
factor              :factor_prefix postfix_expr {
                        if($1){
                            $$ = $1;
                            $$ -> rnode = $2;
                        }
                        else $$ = $2;
                    };
factor_prefix       :factor_prefix postfix_expr mulop {
                        $$ = new MulExprNode($3);
                        if($1){
                            $$ -> lnode = $1;
                            $1 -> rnode = $2;
                        }
                        else $$ -> lnode = $2; 
                    } | /* empty */{$$ = NULL;};
postfix_expr        :primary {
                        $$ = $1;
                    } | call_expr {
                        $$ = $1;
                    };
call_expr           :id{
                        // TODO:check if the amount of argument match 
                        if(!globalSymtable->have(*$1)) yyerror("undeclared function"); 
                        if(expr_stack_ptr) expr_stack_ptr_stack.push(expr_stack_ptr);
                        expr_stack_ptr = new std::stack<ExprNode*>();
                    }OPAREN expr_list CPAREN {
                        CallExprNode* new_call = new CallExprNode(*$1);
                        new_call->exprStack = *expr_stack_ptr;
                        delete expr_stack_ptr;
                        if(!expr_stack_ptr_stack.empty()) {
                            expr_stack_ptr = expr_stack_ptr_stack.top();
                            expr_stack_ptr_stack.pop();
                        }

                        delete $1;
                        $$ = new_call;
                    };
expr_list           :expr expr_list_tail {
                        expr_stack_ptr->push($1);  
                    }| /* empty */;
expr_list_tail      :COMMA expr expr_list_tail {
                        expr_stack_ptr->push($2);  
                    }| /* empty */;
primary             :OPAREN expr CPAREN {
                        $$ = $2; 
                    } | id{
                        VarRef* new_var = find_id(*$1);
                        delete $1;
                        $$ = new_var;
                    } | INTLITERAL {
                        LitRef* new_lit = new LitRef("INT",
                            std::to_string(static_cast<long long int>($1)));
                        $$ = new_lit;
                    } | FLOATLITERAL {
                        LitRef* new_lit = new LitRef("FLOAT",
                            std::to_string(static_cast<long double>($1)));
                        $$ = new_lit;
                    };
addop               :PLUS{$$='+';} | MINUS{$$='-';};
mulop               :MUL{$$='*';} | DIV{$$='/';};

/* Complex Statements and Condition */ 
if_stmt             :IF OPAREN cond CPAREN decl{
                        // allocate a new block
                        block_index++;
                        Symtable* current = new Symtable(
                            "BLOCK "+
                            std::to_string(static_cast<long long int>(block_index)));
                        symtable_stack.push(current);
                        //symtable_list.push_back(current);

                        // allocate a new if node
                        IfStmtNode* new_if = new IfStmtNode(dynamic_cast<CondExprNode*>($3),current,
                            std::to_string(static_cast<long long int>(block_index)));
                        block_list.back()->stmt_list.push_back(new_if); 
                        block_list.push_back(new_if);
                    }
                    stmt_list{
                        symtable_stack.pop();                    
                    }
                    else_part ENDIF{
                        block_list.pop_back();
                    };
else_part           :ELSE{
                        block_index++;
                        Symtable* current = new Symtable(
                            "BLOCK "+
                            std::to_string(static_cast<long long int>(block_index)));
                        symtable_stack.push(current);
                        //symtable_list.push_back(current);

                        // allocate a new else node
                        ElseStmtNode* new_else = new ElseStmtNode(current);
                        dynamic_cast<IfStmtNode*>(block_list.back())->elseNode = new_else;
                        block_list.push_back(new_else);
                    }
                    decl stmt_list{
                        symtable_stack.pop(); 
                        block_list.pop_back();
                    } 
                    | /* empty */;
cond                :expr compop expr{
                        CondExprNode* new_cond = new CondExprNode((string)$2);
                        new_cond->lnode = $1;
                        new_cond->rnode = $3;
                        $$ = new_cond;
                    }
                    | TRUE {
                        CondExprNode* new_lit = new CondExprNode("TRUE");
                    }
                    | FALSE{
                        CondExprNode* new_lit = new CondExprNode("FALSE");
                    };
compop              :LT| GT| EQ| NEQ| LEQ| GEQ; /* reutrn $1 by default */
while_stmt          :WHILE OPAREN cond CPAREN {
                        block_index++;
                        Symtable* current = new Symtable(
                            "BLOCK "+
                            std::to_string(static_cast<long long int>(block_index)));
                        symtable_stack.push(current);

                        // allocate a new while node
                        WhileStmtNode* new_while = new WhileStmtNode(dynamic_cast<CondExprNode*>($3),current,
                            std::to_string(static_cast<long long int>(block_index)));
                        block_list.back()->stmt_list.push_back(new_while); 
                        block_list.push_back(new_while);
                    } 
                    decl stmt_list ENDWHILE{
                        symtable_stack.pop(); 
                        block_list.pop_back();
                    };


/*ECE468 ONLY*/
control_stmt        :return_stmt;
loop_stmt           :while_stmt;

%%
//Epilouge
void yyerror (const char* s){
    std::cout << "Not Accepted" << std::endl;
    exit(1);
}
