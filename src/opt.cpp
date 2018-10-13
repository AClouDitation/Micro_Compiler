#include <iostream>
#include "../src/opt.hpp"

// Utils
std::vector<std::string> SplitString(const std::string& s, const std::string& c){

    std::vector<std::string> v;
    std::string::size_type pos1, pos2;
    pos2 = s.find(c);
    pos1 = 0;
    while(std::string::npos != pos2){
        v.push_back(s.substr(pos1, pos2-pos1));

        pos1 = pos2 + c.size();
        pos2 = s.find(c, pos1);
    }
    if(pos1 != s.length()) v.push_back(s.substr(pos1));

    return v;
}

std::vector<std::vector<std::string>>& split_irs(std::vector<std::string>& irs){
    std::vector<std::vector<std::string>>* sp_irs = 
        new std::vector<std::vector<std::string>>;

    for(auto ir:irs){
        sp_irs->push_back(SplitString(ir," "));  
    }

    return *sp_irs;
}

bool _is_constant(std::string lit){
    char have_dot = false;
    for(auto ch:lit){
        if(!std::isdigit(ch) && (ch != '.' || have_dot))return false;
        if(ch == '.') have_dot = true;
    }
    return true;
}

void constant_swap(std::vector<std::vector<std::string>>& irs)
{
    extern int temp_reg_index;
    std::unordered_map<std::string, std::string> const_refs;

    for(auto ir = irs.begin(); ir != irs.end();)
    {
        if((*ir)[0] == "STOREI" || (*ir)[0] == "STOREF")
        {
            if(_is_constant((*ir)[1]))// op1 is a literal
            {
                const_refs[(*ir)[2]] = (*ir)[1];
                ir = irs.erase(ir);
            }
            else if(const_refs.find((*ir)[1]) != const_refs.end()) // or const ref
            {
                const_refs[(*ir)[2]] = const_refs[(*ir)[1]];
                ir = irs.erase(ir);
            }
            else
            {
                const_refs.erase((*ir)[1]);    
                ir++;
            }
        }
        else if((*ir)[0] == "READI" || (*ir)[0] == "READF"){
            const_refs.erase((*ir)[1]);    // not a constant anymore
            ir++;
        }
        else if((*ir)[0] == "MULI" || (*ir)[0] == "MULF" ||
                (*ir)[0] == "ADDI" || (*ir)[0] == "ADDF" ||
                (*ir)[0] == "DIVI" || (*ir)[0] == "DIVF" ||
                (*ir)[0] == "SUBI" || (*ir)[0] == "SUBF")
        {
            if((_is_constant((*ir)[1]) ||   // op1 is a literal
                        const_refs.find((*ir)[1]) != const_refs.end()) && // or const ref
                    (_is_constant((*ir)[2]) || 
                     const_refs.find((*ir)[2]) != const_refs.end()))
            {
                // op1 and op2 are constant
                if((*ir)[0][3] == 'I'){
                    int op1 = _is_constant((*ir)[1])?std::stoi((*ir)[1]):std::stoi(const_refs[(*ir)[1]]);
                    int op2 = _is_constant((*ir)[2])?std::stoi((*ir)[2]):std::stoi(const_refs[(*ir)[2]]);
                    int res;
                    if((*ir)[0] == "MULI") res = op1*op2;
                    if((*ir)[0] == "ADDI") res = op1+op2;
                    if((*ir)[0] == "DIVI") res = op1/op2;
                    if((*ir)[0] == "SUBI") res = op1-op2;

                    const_refs[(*ir)[3]] = std::to_string(res);
                    ir = irs.erase(ir);
                }
                else{
                    double op1 = _is_constant((*ir)[1])?std::stof((*ir)[1]):std::stof(const_refs[(*ir)[1]]);
                    double op2 = _is_constant((*ir)[2])?std::stof((*ir)[2]):std::stof(const_refs[(*ir)[2]]);
                    double res;
                    if((*ir)[0] == "MULF") res = op1*op2;
                    if((*ir)[0] == "ADDF") res = op1+op2;
                    if((*ir)[0] == "DIVF") res = op1/op2;
                    if((*ir)[0] == "SUBF") res = op1-op2;

                    const_refs[(*ir)[3]] = std::to_string(res);
                    ir = irs.erase(ir);

                }
            }
            else{
                if(const_refs.find((*ir)[1]) != const_refs.end())
                    (*ir)[1] = const_refs[(*ir)[1]];
                if(const_refs.find((*ir)[2]) != const_refs.end())
                    (*ir)[2] = const_refs[(*ir)[2]];
                const_refs.erase((*ir)[3]);
                ir++;
            }
        }
        else if((*ir)[0] == "WRITEI" || (*ir)[0] == "WRITEF"){
            if(const_refs.find((*ir)[1]) != const_refs.end()){
                //store back to register for priting
                std::vector<std::string> str_ir;
                if((*ir)[0] == "WRITEI")str_ir.push_back("STOREI");
                else if((*ir)[0] == "WRITEF")str_ir.push_back("STOREF");
                str_ir.push_back(const_refs[(*ir)[1]]);
                str_ir.push_back("$T"+std::to_string(temp_reg_index)); // store to register
                (*ir)[1] = "$T" + std::to_string(temp_reg_index++);
                irs.insert(ir,str_ir);
                ir++;
            }
            ir++;
        }
        else{
            // should be WRITES only
            ir++;
        }


    }
}

// cross_out dead exprs
void _cross_out(std::unordered_map<std::string,std::string>& reg_content,
        std::string& target)
{
    //std::cerr << "Looking for " << target << std::endl;
    for(auto it = reg_content.begin();it != reg_content.end();){
        std::vector<std::string>items = SplitString(it->first," ");
        //std::cerr << items[0] << " " << items[1] << " " << items[2] << std::endl;
        if(target == items[1] || target == items[2]){
            //std::cerr << "RM " << it->first << std::endl;
            it = reg_content.erase(it);        
        }
        else it++;
    }
}

void live_ana(std::vector<std::vector<std::string>>& irs){    

    std::unordered_map<std::string,std::string> reg_content;
    //                  ^expr       ^reg

    for(auto ir = irs.begin(); ir != irs.end();){

        if((*ir)[0] == "READI" || (*ir)[0] == "READF")
        {
            std::string target = (*ir)[1];
            _cross_out(reg_content, target);
        }
        else if((*ir)[0] == "STOREI" || (*ir)[0] == "STOREF")
        {
            std::string target = (*ir)[2];
            _cross_out(reg_content, target);
        }
        else if((*ir)[0] == "MULI" || (*ir)[0] == "MULF" ||
                (*ir)[0] == "ADDI" || (*ir)[0] == "ADDF" ||
                (*ir)[0] == "DIVI" || (*ir)[0] == "DIVF" ||
                (*ir)[0] == "SUBI" || (*ir)[0] == "SUBF")
        {
            std::string target = (*ir)[3];
            std::string content = (*ir)[0] + " " + (*ir)[1] + " " + (*ir)[2];
            if(reg_content.find(content) == reg_content.end())
            {
                reg_content[content] = target;
            }
            else
            {
                std::string target = (*ir)[3];
                (*ir).pop_back();
                (*ir)[0] = "STORE" + (*ir)[0].substr(3);
                (*ir)[1] = reg_content[content];
                (*ir)[2] = target;
            }
            _cross_out(reg_content, target);
        }
        ir++;
    }
}

void rm_useless_move(std::vector<std::vector<std::string>>& irs){
}

void dead_store_eli(std::vector<std::vector<std::string>>& irs){

    for(auto ir = irs.begin(); ir != irs.end();ir++){
        if((*ir)[0] == "STOREI" || (*ir)[0] == "STOREF"){
            // go through the code to see what happens next...
         
            bool need_this_store = false;
            std::string target = (*ir)[2];
            auto nir = ir;
            nir++;
            for(;nir != irs.end();nir++){

                if((*nir)[0] == "READI" || (*nir)[0] == "READF")
                {
                    std::string new_target = (*nir)[1];
                    if(new_target == target)break;
                }
                else if((*nir)[0] == "STOREI" || (*nir)[0] == "STOREF")
                {
                    std::string new_target = (*nir)[2];
                    std::string op1 = (*nir)[1];
                    if(op1 == target){need_this_store = true;break;}
                    if(new_target == target)break;
                }
                else if((*nir)[0] == "MULI" || (*nir)[0] == "MULF" ||
                        (*nir)[0] == "ADDI" || (*nir)[0] == "ADDF" ||
                        (*nir)[0] == "DIVI" || (*nir)[0] == "DIVF" ||
                        (*nir)[0] == "SUBI" || (*nir)[0] == "SUBF")
                {
                    std::string op1 = (*nir)[1];
                    std::string op2 = (*nir)[2];
                    std::string new_target = (*ir)[3];
                    if(op1 == target || op2 == target){need_this_store = true;break;}
                    if(new_target == target)break;
                }
                else if((*nir)[0] == "WRITEI" || (*nir)[0] == "WRITEF"){
                    std::string new_target = (*nir)[1];
                    if(new_target == target){need_this_store = true;break;}
                }
            }
             
            if(!need_this_store){
                ir = irs.erase(ir);
                ir--;
            }
        }
    }
}

void OOOptmize(std::vector<std::vector<std::string>>& irs){
    constant_swap(irs);
    live_ana(irs);

    unsigned int len;
    do{
        len = irs.size();
        dead_store_eli(irs);
    }while(irs.size() < len);
}
