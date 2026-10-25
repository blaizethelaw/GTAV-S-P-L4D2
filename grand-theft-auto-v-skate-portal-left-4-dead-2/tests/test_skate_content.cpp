#include "../src/content/skate_content.hpp"
#include <iostream>
#include <map>
#include <set>
#include <functional>
#include <limits>

using namespace lsc::content;

namespace {
void put(Bytes& b,std::size_t p,std::uint64_t x,unsigned width=4){
    if(p+width>b.size())b.resize(p+width);
    for(unsigned i=0;i<width;++i)b[p+i]=std::uint8_t(x>>(i*8));
}
void append(Bytes& b,std::uint64_t x,unsigned width=4){put(b,b.size(),x,width);}
void text(Bytes& b,const std::string& s){b.insert(b.end(),s.begin(),s.end());b.push_back(0);}
void chunk(Bytes& b,const char* id,const Bytes& payload){
    b.insert(b.end(),id,id+4);append(b,payload.size());b.insert(b.end(),payload.begin(),payload.end());if(payload.size()&1)b.push_back(0);
}
struct Fixture {Bytes data;std::size_t scoreOffset{},pointerOffset{};};
Fixture synthetic(){
    // Original synthetic metadata and invented values, never extracted game data.
    Bytes reflection;append(reflection,0);append(reflection,3);
    auto type=[&](unsigned hash,unsigned first,unsigned count,unsigned size){append(reflection,hash);append(reflection,first);append(reflection,count,2);append(reflection,0,2);append(reflection,size,2);append(reflection,8,2);};
    type(1,0,7,32);type(2,7,1,4);type(3,8,1,4);append(reflection,9);
    auto field=[&](unsigned hash,unsigned offset,unsigned flags,unsigned typeIndex=0){append(reflection,hash);append(reflection,offset);append(reflection,flags,2);append(reflection,typeIndex,2);};
    field(4,0,0xe0);auto relativeScore=reflection.size()+4;field(5,8,0x200);field(6,12,0x200);field(7,16,0x100,1);field(8,20,0x100,2);field(9,24,0x140);field(10,25,0x140);field(11,0,0);field(12,0,0);
    append(reflection,0);append(reflection,0);
    const std::vector<std::string> names={"DingoScorable","SyntheticCategoryType","SyntheticDifficultyType","TrickName","BaseScoreValue","ContinousScoreValue","TrickCategory","Difficulty","Continuous","HideScoreAndScorableName","SyntheticCategory","SyntheticDifficulty"};
    append(reflection,names.size());Bytes strings;
    for(std::size_t i=0;i<names.size();++i){append(reflection,i+1);append(reflection,strings.size());text(strings,names[i]);}
    reflection.insert(reflection.end(),strings.begin(),strings.end());
    Bytes data(80);put(data,12,32,8);put(data,20,123);put(data,24,10);put(data,36,1,1);put(data,37,0,1);
    const std::string trick="Synthetic Flip";std::copy(trick.begin(),trick.end(),data.begin()+44);
    Bytes fix(16);append(fix,0);append(fix,0);append(fix,1);append(fix,1);append(fix,0);
    Fixture f;f.data={'R','I','F','F',0,0,0,0,'E','B','X',0};chunk(f.data,"EBXD",data);chunk(f.data,"EFIX",fix);f.scoreOffset=f.data.size()+8+relativeScore;f.pointerOffset=32;chunk(f.data,"RFL2",reflection);put(f.data,4,f.data.size()-8);return f;
}
void rejected(const Bytes& b){
    try{decodeSkateScorable(b,"synthetic");}catch(const ContentError&){return;}
    throw ContentError("Malformed EBX accepted");
}
void parserChecks(){
    for(const Bytes& b : {Bytes{},Bytes(128),Bytes{'R','I','F','F',0,0,0,0,'E','B','X',0}})rejected(b);
    const auto f=synthetic();auto result=decodeSkateScorable(f.data,"synthetic");
    if(!result||result->name!="Synthetic Flip"||result->baseScore!=123||result->continuousScore!=10||!result->continuous||result->hidden||result->category!="SyntheticCategory"||result->difficulty!="SyntheticDifficulty")throw ContentError("Synthetic schema decoding mismatch");
    auto b=f.data;put(b,f.scoreOffset,31);rejected(b); // Starts inside instance, extends beyond its end.
    for(auto pointer:{std::uint64_t(0x7fffffffffffffff),std::uint64_t(0x8000000000000000)}){b=f.data;put(b,f.pointerOffset,pointer,8);rejected(b);}
    b=f.data;b.insert(b.end(),{'J','U','N','K',1,0,0,0,42});put(b,4,b.size()-8);rejected(b); // Missing odd-chunk padding.
    for(std::size_t size=0;size<f.data.size();++size){b=Bytes(f.data.begin(),f.data.begin()+size);if(size>=8)put(b,4,size-8);rejected(b);}
}
}

int main(int argc,char** argv){
    try {
        parserChecks();
        if(argc!=2&&argc!=4){std::cout<<"PASS: synthetic schema, complete truncation sweep, field boundary, pointer overflow and RIFF padding checks.\n";return 0;}
        for(const auto* path:{"../escape.toc","C:/outside.toc","Data/../escape.toc"}){
            bool failed=false;try{loadSkateScorables(argv[1],path,"invalid");}catch(const ContentError&){failed=true;}
            if(!failed)throw ContentError("Unsafe content path accepted");
        }
        auto tricks=argc==2?loadSkateScorables(argv[1]):loadSkateScorables(argv[1],argv[2],argv[3]);
        std::map<std::string,unsigned> categories;
        std::set<std::string> sources;
        bool varied=false;auto first=tricks.front().baseScore;
        for(const auto& t:tricks){
            if(t.sourceAsset.rfind("gameplay/scorables/",0)!=0||!sources.insert(t.sourceAsset).second||t.name.empty()||t.category.empty()||t.difficulty.empty()||t.baseScore<0||t.continuousScore<0)throw ContentError("Invalid loaded rule record");
            ++categories[t.category];varied|=t.baseScore!=first;
        }
        if(tricks.size()<100||!varied||categories.size()<3)throw ContentError("Insufficient scorable coverage");
        // Counts are audit metadata only; never output or persist the trick roster.
        std::cout<<"PASS: "<<tricks.size()<<" scorable rule records decoded in memory; "<<categories.size()<<" categories; varied scores.\n";
        std::cout<<"No extracted asset or catalog files were written.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
