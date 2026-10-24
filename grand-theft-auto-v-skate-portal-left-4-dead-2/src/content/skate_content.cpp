#include "skate_content.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <map>
#include <set>

namespace lsc::content {
namespace {
constexpr std::size_t MaxBytes=64u*1024u*1024u;
struct V {
    const Bytes& b;
    void bounds(std::uint64_t p,std::uint64_t n) const {
        if(p>b.size()||n>b.size()-p)throw ContentError("Truncated skate content");
    }
    std::uint64_t num(std::size_t p,unsigned n,bool be=false) const {
        bounds(p,n);std::uint64_t x=0;
        for(unsigned i=0;i<n;++i)x|=std::uint64_t(b[p+i])<<(8*(be?n-1-i:i));
        return x;
    }
    std::uint32_t u32(std::size_t p,bool be=false)const{return std::uint32_t(num(p,4,be));}
    std::uint16_t u16(std::size_t p)const{return std::uint16_t(num(p,2));}
    std::size_t count(std::size_t p,std::size_t max=100000)const{
        auto x=u32(p);if(x>max)throw ContentError("Excessive skate count");return x;
    }
    Bytes bytes(std::size_t p,std::size_t n)const{bounds(p,n);return Bytes(b.begin()+p,b.begin()+p+n);}
    std::string str(std::size_t p,std::size_t max=4096)const{
        bounds(p,1);std::string s;
        while(p<b.size()&&s.size()<max){auto c=b[p++];if(!c)return s;if(c<32||c>126)throw ContentError("Non-ASCII skate metadata");s.push_back(char(c));}
        throw ContentError("Unterminated skate string");
    }
};
std::filesystem::path within(const std::filesystem::path& root,const std::filesystem::path& rel){
    if(rel.empty()||rel.is_absolute()||rel.has_root_name())throw ContentError("Unsafe skate content path");
    for(const auto& p:rel)if(p==".."||p==".")throw ContentError("Unsafe skate content path");
    auto base=std::filesystem::canonical(root),target=std::filesystem::canonical(base/rel);
    auto a=base.begin(),b=target.begin();
    for(;a!=base.end();++a,++b)if(b==target.end()||*a!=*b)throw ContentError("Skate content escaped install folder");
    return target;
}
Bytes range(const std::filesystem::path& p,std::uint64_t at,std::uint64_t n){
    auto size=std::filesystem::file_size(p);
    if(n>MaxBytes||at>size||n>size-at)throw ContentError("Invalid skate archive range");
    std::ifstream f(p,std::ios::binary);if(!f)throw ContentError("Cannot open skate content");
    f.seekg(static_cast<std::streamoff>(at));Bytes b(static_cast<std::size_t>(n));
    if(n&&!f.read(reinterpret_cast<char*>(b.data()),std::streamsize(n)))throw ContentError("Cannot read skate content");
    return b;
}
struct Chunk {std::size_t at{},size{};};
std::map<std::string,Chunk> riff(const Bytes& b){
    V v{b};if(v.u32(0)!=0x46464952||v.u32(8)!=0x00584245||std::uint64_t(v.u32(4))+8!=b.size())throw ContentError("Unsupported skate EBX container");
    std::map<std::string,Chunk> chunks;
    for(std::size_t p=12;p<b.size();){v.bounds(p,8);std::string id(b.begin()+p,b.begin()+p+4);auto n=v.u32(p+4);v.bounds(p+8,n);if(!chunks.emplace(id,Chunk{p+8,n}).second)throw ContentError("Duplicate EBX chunk");p+=8+n+(n&1);}
    return chunks;
}
struct Type {std::uint32_t hash;std::size_t first,count;std::uint16_t flags,size,align;};
struct Field {std::uint32_t hash,offset;std::uint16_t flags,type;};
struct Reflection {
    std::vector<Type> types;std::vector<Field> fields;std::map<std::uint32_t,std::string> names;
    explicit Reflection(const Bytes& b){V v{b};std::size_t p=0;
        auto signatures=v.count(p);p+=4;v.bounds(p,signatures*20);p+=signatures*20;
        auto nt=v.count(p,4096);p+=4;v.bounds(p,nt*16);
        for(std::size_t i=0;i<nt;++i,p+=16)types.push_back({v.u32(p),v.u32(p+4),v.u16(p+8),v.u16(p+10),v.u16(p+12),v.u16(p+14)});
        auto nf=v.count(p);p+=4;v.bounds(p,nf*12);
        for(std::size_t i=0;i<nf;++i,p+=12)fields.push_back({v.u32(p),v.u32(p+4),v.u16(p+8),v.u16(p+10)});
        // RFL2's array and boxed-value descriptors precede its name table.
        // Scalar scorable records never dereference these descriptors.
        auto na=v.count(p);p+=4;v.bounds(p,na*12);p+=na*12;
        auto nb=v.count(p);p+=4;v.bounds(p,nb*8);p+=nb*8;
        auto nn=v.count(p);p+=4;v.bounds(p,nn*8);auto strings=p+nn*8;
        for(std::size_t i=0;i<nn;++i,p+=8){auto name=v.str(strings+v.u32(p+4));if(!names.emplace(v.u32(p),name).second)throw ContentError("Duplicate reflection name");}
        for(const auto& t:types)if(t.first>fields.size()||t.count>fields.size()-t.first)throw ContentError("Invalid EBX type fields");
    }
    const std::string& name(std::uint32_t h)const{auto i=names.find(h);if(i==names.end())throw ContentError("Unknown EBX name hash");return i->second;}
    std::optional<Field> find(std::size_t type,const std::string& n,unsigned depth=0)const{
        if(type>=types.size()||depth>16)throw ContentError("Invalid EBX inheritance");
        const auto& t=types[type];
        for(std::size_t i=t.first;i<t.first+t.count;++i){const auto& f=fields[i];
            if(f.flags==0&&name(f.hash).empty()){
                auto sub=find(f.type,n,depth+1);if(sub){sub->offset+=f.offset;return sub;}
            }else if(name(f.hash)==n)return f;
        }return std::nullopt;
    }
    std::string enumName(const Field& f,std::uint32_t value)const{
        if(f.type>=types.size())throw ContentError("Invalid EBX enum");
        const auto& t=types[f.type];
        for(std::size_t i=t.first;i<t.first+t.count;++i)if(fields[i].offset==value)return name(fields[i].hash);
        throw ContentError("Unknown skate enum value");
    }
};
struct Loc {std::uint32_t install{},archive{},offset{},size{};bool patch{};};
std::vector<Loc> locations(const Bytes& b){
    V v{b};auto flagAt=v.u32(8,true),count=v.u32(12,true);std::size_t p=v.u32(16,true);
    if(count>200000||count!=v.u32(32,true)||p!=v.u32(20,true)||p!=v.u32(24,true))throw ContentError("Unsupported skate bundle table");
    v.bounds(flagAt,count);std::vector<Loc> out;Loc active{};bool have=false;
    for(unsigned i=0;i<count;++i){auto flag=b[flagAt+i];
        if(flag){if(flag!=1&&flag!=0x80&&flag!=0x84)throw ContentError("Unsupported skate CAS identity");auto id=v.num(p,flag==1?4:8,true);p+=flag==1?4:8;
            active.patch=flag==1?bool((id>>16)&255):bool((id>>48)&255);
            active.install=flag==1?std::uint32_t((id>>8)&255):std::uint32_t(id>>16);
            active.archive=flag==1?std::uint32_t(id&255):std::uint32_t(id&65535);have=true;}
        if(!have)throw ContentError("Missing skate CAS identity");
        active.offset=v.u32(p,true);active.size=v.u32(p+4,true);p+=8;out.push_back(active);
    }return out;
}
Bytes readCas(const std::filesystem::path& root,const Loc& l){
    if(l.patch||l.install!=3162564249u)throw ContentError("Unsupported skate install group");
    auto n=std::to_string(l.archive);if(n.size()<2)n="0"+n;
    return range(within(root,"Data/Win32/configurations/layout/initialinstallpackage/cas_"+n+".cas"),l.offset,l.size);
}
struct Decoder {
    HMODULE dll{};
    using Fn=std::int64_t(__cdecl*)(const void*,std::int64_t,void*,std::int64_t,int,int,int,void*,std::int64_t,void*,void*,void*,std::int64_t,int);
    Fn fn{};std::filesystem::path root;
    explicit Decoder(std::filesystem::path p):root(std::move(p)){}
    ~Decoder(){if(dll)FreeLibrary(dll);}
    Bytes decode(const Bytes& b){V v{b};Bytes out;
        for(std::size_t p=0;p<b.size();){auto h=v.num(p,8,true);p+=8;auto expanded=(h>>32)&0xffffff,codec=(h>>24)&0x7f,n=h&0xfffff;v.bounds(p,n);
            if(expanded>MaxBytes-out.size())throw ContentError("Skate decode limit exceeded");
            auto dst=out.size();out.resize(dst+std::size_t(expanded));
            if(codec==0){if(n!=expanded)throw ContentError("Invalid raw CAS block");std::copy_n(b.data()+p,std::size_t(n),out.data()+dst);}
            else if(codec==0x11||codec==0x15||codec==0x19){if(!dll){auto path=within(root,"oo2core_9_win64.dll");dll=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);if(!dll)throw ContentError("Cannot load installed skate decoder");auto proc=GetProcAddress(dll,"OodleLZ_Decompress");static_assert(sizeof(proc)==sizeof(fn));std::memcpy(&fn,&proc,sizeof(fn));if(!fn)throw ContentError("Installed decoder lacks OodleLZ_Decompress");}
                if(fn(b.data()+p,std::int64_t(n),out.data()+dst,std::int64_t(expanded),1,0,0,nullptr,0,nullptr,nullptr,nullptr,0,3)!=std::int64_t(expanded))throw ContentError("Skate decoder rejected block");
            }else throw ContentError("Unsupported skate CAS codec");p+=std::size_t(n);
        }return out;
    }
};
std::string tocName(const Bytes& b,std::uint32_t offset){V v{b};auto names=v.u32(32,true),flags=v.u32(44,true);
    if(!(flags&4))return v.str(names+offset);
    auto nw=v.u32(48,true),ns=v.u32(52,true),table=v.u32(56,true);if(!ns||(ns&1)||ns>1000000||nw>1000000)throw ContentError("Invalid skate Huffman table");
    v.bounds(names,std::uint64_t(nw)*4);v.bounds(table,std::uint64_t(ns)*4);std::string out;std::uint64_t bit=offset;
    while(out.size()<4096){auto node=ns/2-1;unsigned depth=0;
        while(node<ns/2){if(bit>=std::uint64_t(nw)*32||++depth>256)throw ContentError("Invalid skate Huffman stream");auto branch=(v.u32(names+std::size_t(bit/32)*4,true)>>(bit%32))&1;++bit;node=v.u32(table+node*8+branch*4,true);}
        auto ch=(~node)&65535;if(!ch)return out;if(ch>127)throw ContentError("Invalid skate bundle name");out.push_back(char(ch));
    }throw ContentError("Skate bundle name too long");
}
} // namespace

std::optional<SkateTrick> decodeSkateScorable(const Bytes& b,const std::string& source){
    V v{b};auto chunks=riff(b);
    for(const auto* c:{"EBXD","EFIX","RFL2"})if(!chunks.count(c))throw ContentError("Missing skate EBX chunk");
    auto r=chunks.at("RFL2");Reflection reflection(v.bytes(r.at,r.size));
    if(reflection.types.empty())throw ContentError("Empty skate reflection");
    // These catalogs contain one exported object. Group entries are intentionally
    // excluded; their schema does not define one trick's score.
    if(reflection.name(reflection.types[0].hash)!="DingoScorable")return std::nullopt;
    auto fix=chunks.at("EFIX");auto fb=v.bytes(fix.at,fix.size);V f{fb};std::size_t p=16;auto ng=f.count(p);p+=4+ng*16;auto ns=f.count(p);p+=4+ns*4;
    if(f.u32(p)!=1||f.u32(p+4)!=1)throw ContentError("Unsupported scorable instance count");
    p+=8;auto data=chunks.at("EBXD");auto base=((data.at+15)&~std::size_t(15))+f.u32(p);
    if(base<data.at||base+reflection.types[0].size>data.at+data.size)throw ContentError("Scorable instance outside EBXD");
    auto field=[&](const char* name,std::uint16_t flags){auto x=reflection.find(0,name);if(!x||x->flags!=flags)throw ContentError("Unsupported skate property schema");if(x->offset>=reflection.types[0].size)throw ContentError("Scorable field outside instance");return *x;};
    auto text=[&](const char* name){auto x=field(name,0xe0);auto at=base+x.offset;auto delta=static_cast<std::int64_t>(v.num(at,8));auto target=std::int64_t(at)+delta;if(target<std::int64_t(data.at)||target>=std::int64_t(data.at+data.size))throw ContentError("Scorable text outside EBXD");auto s=v.str(std::size_t(target));if(std::uint64_t(target)+s.size()>=data.at+data.size)throw ContentError("Scorable text exceeds EBXD");return s;};
    auto integer=[&](const char* name){return std::int32_t(v.u32(base+field(name,0x200).offset));};
    auto boolean=[&](const char* name){auto at=base+field(name,0x140).offset;auto n=v.num(at,1);if(n>1)throw ContentError("Invalid scorable boolean");return bool(n);};
    auto enumeration=[&](const char* name){auto x=field(name,0x100);return reflection.enumName(x,v.u32(base+x.offset));};
    SkateTrick out;out.sourceAsset=source;out.name=text("TrickName");out.baseScore=integer("BaseScoreValue");out.continuousScore=integer("ContinousScoreValue");out.category=enumeration("TrickCategory");out.difficulty=enumeration("Difficulty");out.continuous=boolean("Continuous");out.hidden=boolean("HideScoreAndScorableName");
    if(out.name.empty()||out.baseScore<0||out.continuousScore<0)throw ContentError("Invalid scorable values");
    return out;
}

std::vector<SkateTrick> loadSkateScorables(const std::filesystem::path& root,const std::string& sourceToc,const std::string& bundle){
    auto path=within(root,sourceToc);auto raw=range(path,0,std::filesystem::file_size(path));V rv{raw};if(rv.u32(0,true)!=0x00d1ce01)throw ContentError("Unsupported skate TOC envelope");auto b=rv.bytes(556,raw.size()-556);V v{b};auto at=v.u32(4,true),count=v.u32(8,true);if(count>200000)throw ContentError("Excessive skate bundle count");v.bounds(at,std::uint64_t(count)*16);
    std::vector<Loc> locs;Bytes manifest;
    for(unsigned i=0;i<count;++i){auto p=at+i*16;if(tocName(b,v.u32(p,true))!=bundle)continue;auto size=v.u32(p+4,true);if(size>>30!=1)throw ContentError("Unsupported skate bundle storage");auto reg=v.bytes(std::size_t(v.num(p+8,8,true)),size&0x3fffffff);locs=locations(reg);V rr{reg};if(rr.u32(4,true))manifest=rr.bytes(rr.u32(0,true),rr.u32(4,true));break;}
    if(locs.empty())throw ContentError("Required skate bundle not found");
    bool external=manifest.empty();if(external)manifest=readCas(root,locs[0]);V m{manifest};if(std::uint64_t(m.u32(0,true))+4!=manifest.size())throw ContentError("Invalid skate manifest size");auto magic=m.u32(4);bool be;if(magic==0x9d798ed6)be=false;else if(magic==0xd68e799d)be=true;else throw ContentError("Unsupported skate manifest");
    auto total=m.u32(8,be),ebx=m.u32(12,be),res=m.u32(16,be),chunks=m.u32(20,be),strings=m.u32(24,be);
    if(total>200000||std::uint64_t(ebx)+res+chunks!=total||locs.size()!=std::uint64_t(total)+(external?1:0))throw ContentError("Inconsistent skate manifest counts");
    std::size_t entries=36+std::size_t(total)*20;m.bounds(entries,std::uint64_t(ebx)*8);Decoder decoder(root);std::vector<SkateTrick> out;std::set<std::string> seen;
    for(unsigned i=0;i<ebx;++i){auto p=entries+i*8;auto name=m.str(4+strings+m.u32(p,be));if(name.rfind("gameplay/scorables/",0)!=0)continue;auto decoded=decoder.decode(readCas(root,locs[i+(external?1:0)]));if(decoded.size()!=m.u32(p+4,be))throw ContentError("Scorable decoded size mismatch");auto trick=decodeSkateScorable(decoded,name);if(trick&&seen.insert(name).second)out.push_back(std::move(*trick));}
    if(out.empty())throw ContentError("No skate trick rules in required bundle");
    return out;
}
} // namespace lsc::content
