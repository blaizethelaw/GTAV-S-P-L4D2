#include "../src/content/skate_content.hpp"
#include <iostream>
#include <map>

using namespace lsc::content;

int main(int argc,char** argv){
    try {
        // Inputs with absent/invalid structures must fail before returning data.
        for(const Bytes& b : {Bytes{},Bytes(128),Bytes{'R','I','F','F',0,0,0,0,'E','B','X',0}}){
            bool rejected=false;try{decodeSkateScorable(b,"synthetic");}catch(const ContentError&){rejected=true;}
            if(!rejected)throw ContentError("Malformed EBX accepted");
        }
        if(argc!=4){std::cout<<"Malformed EBX rejection checks passed; installed game check requires root/toc/bundle.\n";return 0;}
        auto tricks=loadSkateScorables(argv[1],argv[2],argv[3]);
        std::map<std::string,unsigned> categories;
        bool varied=false;auto first=tricks.front().baseScore;
        for(const auto& t:tricks){++categories[t.category];varied|=t.baseScore!=first;}
        if(tricks.size()<100||!varied||categories.size()<3)throw ContentError("Insufficient scorable coverage");
        // Counts are audit metadata only; never output or persist the trick roster.
        std::cout<<"PASS: "<<tricks.size()<<" scorable rule records decoded in memory; "<<categories.size()<<" categories; varied scores.\n";
        std::cout<<"No extracted asset or catalog files were written.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
