#include "../../inc/StmtNode.hpp"
#include "../../inc/ExprNode.hpp"
#include "../../inc/symtable.hpp"
#include "../../inc/irNode.hpp"
#include "../../inc/utility.hpp"
#include <assert.h>

using namespace std;

BlockNode::BlockNode(Symtable* symtable):
    symtable(symtable){}

BlockNode::~BlockNode(){}

FunctionDeclNode::FunctionDeclNode
    (string name, string type, int argc, Symtable* symtable):
    BlockNode(symtable),name(name),type(type),argc(argc){}

FunctionDeclNode::~FunctionDeclNode(){}

vector<IrNode*>& FunctionDeclNode::translate(){
    
    vector<IrNode*>* ir = new vector<IrNode*>;

    irBlockInsert(*ir, new LabelIrNode("FUNC_"+name));
    irBlockInsert(*ir,new LinkIrNode(symtable->size() - argc));         // temporary, size of link should be adjust after 
                                                                        // register allocation

    for(auto stmt: stmt_list){
        vector<IrNode*> code_block = stmt->translate();                 // translate one statment
        code_block.front()->setPre(ir->back());                         // link the statment code block to the end of ir block
        ir->insert(ir->end(),code_block.begin(),code_block.end());      // insert the new  block into ir block
    }

    // return if reach the end of function
    irBlockInsert(*ir, new IrNode("UNLINK"));
    irBlockInsert(*ir, new IrNode("RET"));

    return *ir;
}

string FunctionDeclNode::getNextAvaTemp() {
    return "!T" + to_string(nextAvaTemp++);                             // get a new temporary
}

IfStmtNode::IfStmtNode(CondExprNode* cond, Symtable* symtable, string index):
    BlockNode(symtable),
    cond(cond),elseNode(NULL),index(index){}

IfStmtNode::~IfStmtNode(){}

vector<IrNode*>& IfStmtNode::translate(){
    vector<IrNode*>* ir = new vector<IrNode*>;
    cond->setOutLabel("ELSE_"+index);                           // give condition node label proper index
    cond->translate(*ir);                                       
    CondIrNode* condNode = static_cast<CondIrNode*>(ir->back());

    for(auto stmt: stmt_list){                                  // translate IF block
        vector<IrNode*> code_block = stmt->translate();
        irBlockCascade(*ir, code_block);
    }

    JumpIrNode* jmp = new JumpIrNode("END_IF_ELSE_"+index);
    irBlockInsert(*ir, jmp);

    LabelIrNode* elseLabelNode = new LabelIrNode("ELSE_"+index);
    irBlockInsert(*ir, elseLabelNode); 
    elseLabelNode->setPre(condNode);    // manually reset predecessor
    condNode->setSuc2(elseLabelNode);

    if(elseNode){   // translate else block
        vector<IrNode*> code_block = elseNode->translate();
        irBlockCascade(*ir, code_block);
    }
    
    LabelIrNode* endLabelNode = new LabelIrNode("END_IF_ELSE_"+index);
    irBlockInsert(*ir, endLabelNode);
    endLabelNode->setPre2(jmp);
    jmp->setSuc(endLabelNode);

    return *ir;
}

ElseStmtNode::ElseStmtNode(Symtable* symtable):
    BlockNode(symtable)
{}

ElseStmtNode::~ElseStmtNode(){}

vector<IrNode*>& ElseStmtNode::translate(){
    vector<IrNode*>* ir = new vector<IrNode*>;

    for(auto stmt: stmt_list){
        vector<IrNode*> code_block = stmt->translate();
        irBlockCascade(*ir, code_block);
    }

    return *ir;
}

WhileStmtNode::WhileStmtNode(CondExprNode* cond, Symtable* symtable, string index):
    BlockNode(symtable),
    cond(cond),index(index){}

WhileStmtNode::~WhileStmtNode(){}

vector<IrNode*>& WhileStmtNode::translate(){
    vector<IrNode*>* ir = new vector<IrNode*>;

    LabelIrNode* begin = new  LabelIrNode("WHILE_START_"+index);     // save the begin node of the loop
    irBlockInsert(*ir, begin);
    cond->setOutLabel("END_WHILE_"+index);
    cond->translate(*ir);
    CondIrNode* condNode = static_cast<CondIrNode*>(ir->back());  // save the condNode

    for(auto stmt: stmt_list){
        vector<IrNode*> code_block = stmt->translate();
        irBlockCascade(*ir, code_block);
    }
    
    JumpIrNode* jmp = new JumpIrNode("WHILE_START_"+index); 
    irBlockInsert(*ir, jmp);
    begin->setPre2(jmp);    // link the begin label node with jump node 
                            // jmp label ----> label

    LabelIrNode* endLabelNode = new LabelIrNode("END_WHILE_"+index);
    irBlockInsert(*ir, endLabelNode);
    endLabelNode->setPre2(condNode);
    condNode->setSuc2(endLabelNode);
    return *ir;
}


