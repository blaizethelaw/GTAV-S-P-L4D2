#include "source_content.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>

namespace lsc::content {
namespace {
constexpr std::size_t MaxPayload = 256u * 1024u * 1024u;
struct View {
    const Bytes& b;
    void bounds(std::uint64_t at, std::uint64_t len) const {
        if (at > b.size() || len > b.size() - at) throw ContentError("Truncated Source content");
    }
    std::uint8_t u8(std::size_t at) const { bounds(at,1); return b[at]; }
    std::uint16_t u16(std::size_t at) const { bounds(at,2); return b[at] | (std::uint16_t(b[at+1])<<8); }
    std::uint32_t u32(std::size_t at) const {
        bounds(at,4); return b[at] | (std::uint32_t(b[at+1])<<8) | (std::uint32_t(b[at+2])<<16) | (std::uint32_t(b[at+3])<<24);
    }
    std::int32_t i32(std::size_t at) const { return static_cast<std::int32_t>(u32(at)); }
    float f32(std::size_t at) const {
        const auto bits=u32(at); float f; std::memcpy(&f,&bits,4);
        if (!std::isfinite(f)) throw ContentError("Non-finite model coordinate");
        return f;
    }
    std::size_t count(std::size_t at, std::size_t max=1000000) const {
        auto n=i32(at); if (n<0 || std::size_t(n)>max) throw ContentError("Invalid Source count"); return std::size_t(n);
    }
    std::size_t rel(std::size_t base, std::size_t field) const {
        auto n=std::int64_t(base)+i32(field);
        if(n<0 || std::uint64_t(n)>b.size()) throw ContentError("Invalid Source relative offset");
        return std::size_t(n);
    }
    std::string str(std::size_t at, std::size_t max=4096) const {
        bounds(at,1); std::string s;
        while(at<b.size() && s.size()<max) { auto c=b[at++]; if(!c) return s; s.push_back(char(c)); }
        throw ContentError("Unterminated Source string");
    }
};
std::string normalized(std::string p) {
    std::replace(p.begin(),p.end(),'\\','/');
    for(auto& c:p) if(c>='A' && c<='Z') c=char(c-'A'+'a');
    if(p.empty() || p.front()=='/' || p.find(':')!=std::string::npos || p.find('\0')!=std::string::npos)
        throw ContentError("Unsafe content path");
    std::istringstream parts(p); std::string part;
    while(std::getline(parts,part,'/')) if(part.empty() || part=="." || part=="..") throw ContentError("Unsafe content path segment");
    return p;
}
Bytes readRange(const std::filesystem::path& path, std::uint64_t offset, std::uint64_t length) {
    if(length>MaxPayload) throw ContentError("Content payload exceeds memory limit");
    std::ifstream f(path,std::ios::binary);
    if(!f) throw ContentError("Cannot open required companion content");
    f.seekg(0,std::ios::end); auto end=f.tellg();
    if(end<0 || offset>std::uint64_t(end) || length>std::uint64_t(end)-offset) throw ContentError("Truncated companion archive");
    f.seekg(static_cast<std::streamoff>(offset)); Bytes out(static_cast<std::size_t>(length));
    if(length && !f.read(reinterpret_cast<char*>(out.data()),static_cast<std::streamsize>(length))) throw ContentError("Cannot read companion content");
    return out;
}
std::uint32_t crc32(const Bytes& bytes) {
    std::uint32_t crc=0xffffffffu;
    for(auto b:bytes) { crc^=b; for(int n=0;n<8;++n) crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u); }
    return crc^0xffffffffu;
}
void appendTriangle(Model& out,std::uint32_t a,std::uint32_t b,std::uint32_t c) {
    if(a>=out.vertices.size() || b>=out.vertices.size() || c>=out.vertices.size()) throw ContentError("Triangle vertex outside model");
    if(a!=b && b!=c && a!=c) { out.indices.push_back(a); out.indices.push_back(b); out.indices.push_back(c); }
}
std::vector<std::string> kvTokens(const Bytes& bytes) {
    std::vector<std::string> out; std::size_t i=0;
    while(i<bytes.size()) {
        auto c=bytes[i];
        if(c<=32) { ++i; continue; }
        if(c=='/' && i+1<bytes.size() && bytes[i+1]=='/') { while(i<bytes.size() && bytes[i]!='\n')++i; continue; }
        if(c=='{'||c=='}') { out.emplace_back(1,char(c));++i;continue; }
        std::string s;
        if(c=='"') { ++i; while(i<bytes.size() && bytes[i]!='"') s.push_back(char(bytes[i++])); if(i==bytes.size())throw ContentError("Unterminated material value");++i; }
        else while(i<bytes.size() && bytes[i]>32 && bytes[i]!='{' && bytes[i]!='}')s.push_back(char(bytes[i++]));
        for(auto& x:s)if(x>='A'&&x<='Z')x=char(x-'A'+'a'); out.push_back(std::move(s));
    }
    return out;
}
std::size_t imageBytes(unsigned format,unsigned w,unsigned h) {
    if(format==13||format==20)return std::size_t((w+3)/4)*((h+3)/4)*8;
    if(format==14||format==15)return std::size_t((w+3)/4)*((h+3)/4)*16;
    if(format==0||format==12)return std::size_t(w)*h*4;
    throw ContentError("Unsupported VTF image format");
}
} // namespace

VpkArchive::VpkArchive(const std::filesystem::path& path):directoryFile_(path) {
    auto header=readRange(path,0,12);View h{header};
    if(h.u32(0)!=0x55aa1234u || (h.u32(4)!=1 && h.u32(4)!=2))throw ContentError("Unsupported VPK header");
    auto version=h.u32(4),treeSize=h.u32(8),headerSize=version==1?12u:28u;
    auto tree=readRange(path,headerSize,treeSize);View v{tree};std::size_t p=0;
    embeddedStart_=headerSize+std::uint64_t(treeSize);
    if(version==2){auto ext=readRange(path,12,16);embeddedLength_=View{ext}.u32(0);}
    else {auto size=std::filesystem::file_size(path);if(size<embeddedStart_)throw ContentError("Truncated VPK tree");embeddedLength_=size-embeddedStart_;}
    auto next=[&](){auto s=v.str(p);p+=s.size()+1;return s;};
    for(auto ext=next();!ext.empty();ext=next())
        for(auto dir=next();!dir.empty();dir=next())
            for(auto name=next();!name.empty();name=next()) {
                v.bounds(p,18);Entry e;e.crc=v.u32(p);auto preload=v.u16(p+4);e.archive=v.u16(p+6);e.offset=v.u32(p+8);e.length=v.u32(p+12);
                if(v.u16(p+16)!=0xffff)throw ContentError("Invalid VPK entry terminator");p+=18;v.bounds(p,preload);
                e.preload.assign(tree.begin()+p,tree.begin()+p+preload);p+=preload;
                if(e.length>MaxPayload-preload)throw ContentError("VPK entry exceeds memory limit");
                auto full=normalized((dir==" "?"":dir+"/")+name+(ext==" "?"":"."+ext));
                if(!entries_.emplace(full,std::move(e)).second)throw ContentError("Duplicate VPK entry");
            }
    if(p!=tree.size())throw ContentError("Unexpected VPK directory trailing bytes");
}
bool VpkArchive::contains(const std::string& path)const{return entries_.find(normalized(path))!=entries_.end();}
Bytes VpkArchive::read(const std::string& path)const {
    auto it=entries_.find(normalized(path));if(it==entries_.end())throw ContentError("Required asset missing from companion archive");
    const auto& e=it->second;Bytes out=e.preload;
    if(e.length) {
        auto physical=directoryFile_;std::uint64_t offset=e.offset;
        if(e.archive==0x7fff) {if(e.offset>embeddedLength_ || e.length>embeddedLength_-e.offset)throw ContentError("VPK embedded payload outside data section");offset+=embeddedStart_;}
        else {
            auto n=directoryFile_.filename().string();
            if(n.size()<8||n.substr(n.size()-8)!="_dir.vpk")throw ContentError("Expected VPK directory filename");
            auto suffix=std::to_string(e.archive);while(suffix.size()<3)suffix="0"+suffix;
            physical=directoryFile_.parent_path()/(n.substr(0,n.size()-8)+"_"+suffix+".vpk");
        }
        auto tail=readRange(physical,offset,e.length);out.insert(out.end(),tail.begin(),tail.end());
    }
    if(crc32(out)!=e.crc)throw ContentError("Companion asset CRC mismatch");return out;
}
Bytes readLoose(const std::filesystem::path& root,const std::string& path) {
    auto p=normalized(path);auto base=std::filesystem::canonical(root),full=std::filesystem::canonical(base/p);
    auto b=base.begin(),f=full.begin();for(;b!=base.end();++b,++f)if(f==full.end()||*b!=*f)throw ContentError("Loose content escapes companion root");
    return readRange(full,0,std::filesystem::file_size(full));
}

Model decodeModel(const Bytes& mdl,const Bytes& vvd,const Bytes& vtx,VtxLayout layout,unsigned skin,const std::vector<unsigned>& choices) {
    View m{mdl},v{vvd},x{vtx};m.bounds(0,240);v.bounds(0,64);x.bounds(0,36);
    if(m.u32(0)!=0x54534449u || m.u32(4)!=49 || m.u32(76)!=mdl.size())throw ContentError("Expected Source MDL version 49");
    if(v.u32(0)!=0x56534449u || v.u32(4)!=4 || x.u32(0)!=7)throw ContentError("Unsupported VVD/VTX version");
    if(m.u32(8)!=v.u32(8)||m.u32(8)!=x.u32(16))throw ContentError("Companion model checksum mismatch");
    auto vertexCount=v.count(16,1000000),data=v.count(56,vvd.size()),fixups=v.count(48,1000000),fixupStart=v.count(52,vvd.size());
    v.bounds(data,vertexCount*48);std::vector<std::size_t> remap;remap.reserve(vertexCount);
    if(fixups) {
        v.bounds(fixupStart,fixups*12);
        for(std::size_t i=0;i<fixups;++i) {
            auto p=fixupStart+i*12;auto source=v.count(p+4,vertexCount),n=v.count(p+8,vertexCount);
            if(source>vertexCount||n>vertexCount-source||remap.size()+n>vertexCount)throw ContentError("Invalid VVD fixup");
            for(std::size_t k=0;k<n;++k)remap.push_back(source+k);
        }
        if(remap.size()!=vertexCount)throw ContentError("Incomplete VVD fixup table");
    } else for(std::size_t i=0;i<vertexCount;++i)remap.push_back(i);
    Model out;out.checksum=m.u32(8);out.vertices.reserve(vertexCount);
    auto boneCount=m.count(156,256),boneStart=m.count(160,mdl.size());m.bounds(boneStart,boneCount*216);
    for(std::size_t i=0;i<boneCount;++i) {
        auto p=boneStart+i*216;Bone b;b.name=m.str(m.rel(p,p));b.parent=m.i32(p+4);
        if(b.parent<-1||b.parent>=static_cast<std::int32_t>(boneCount)||b.parent==static_cast<std::int32_t>(i))throw ContentError("Invalid model bone parent");
        for(unsigned k=0;k<3;++k)b.localPosition[k]=m.f32(p+32+k*4);
        for(unsigned k=0;k<4;++k)b.localQuaternion[k]=m.f32(p+44+k*4);
        for(unsigned k=0;k<12;++k)b.poseToBone[k]=m.f32(p+96+k*4);out.bones.push_back(std::move(b));
    }
    for(auto source:remap) {
        auto p=data+source*48;Vertex vert;vert.boneCount=v.u8(p+15);
        if(vert.boneCount>3)throw ContentError("Invalid vertex bone count");
        for(unsigned j=0;j<3;++j){vert.weights[j]=v.f32(p+j*4);vert.bones[j]=v.u8(p+12+j);vert.position[j]=v.f32(p+16+j*4);vert.normal[j]=v.f32(p+28+j*4);
            if(j<vert.boneCount && vert.bones[j]>=boneCount)throw ContentError("Vertex references missing bone");}
        for(unsigned j=0;j<2;++j)vert.uv[j]=v.f32(p+40+j*4);out.vertices.push_back(vert);
    }
    auto materials=m.count(204,4096),materialStart=m.count(208,mdl.size()),dirs=m.count(212,4096),dirStart=m.count(216,mdl.size());
    m.bounds(materialStart,materials*64);m.bounds(dirStart,dirs*4);
    std::vector<std::string> materialDirs;for(std::size_t i=0;i<dirs;++i)materialDirs.push_back(m.str(m.count(dirStart+i*4,mdl.size())));
    for(std::size_t i=0;i<materials;++i){auto p=materialStart+i*64;out.materials.push_back(m.str(m.rel(p,p)));}
    // Each material stores candidates separated by newline until loadModel resolves
    // them against the archive. No guessed replacement textures are substituted.
    for(auto& name:out.materials) {
        auto original=name;name="materials/"+normalized(original)+".vmt";
        for(auto dir:materialDirs)if(!dir.empty()){std::replace(dir.begin(),dir.end(),'\\','/');if(dir.back()!='/')dir+='/';name+="\nmaterials/"+normalized(dir+original)+".vmt";}
    }
    auto skinRefs=m.count(220,4096),skinCount=m.count(224,4096),skinStart=m.count(228,mdl.size());
    if(skinCount==0 || skin>=skinCount)throw ContentError("Requested model skin absent");m.bounds(skinStart,skinRefs*skinCount*2);
    auto bodies=m.count(232,1024),bodyStart=m.count(236,mdl.size());
    if(bodies!=x.count(28,1024))throw ContentError("MDL/VTX body count mismatch");
    auto xbody=x.count(32,vtx.size());m.bounds(bodyStart,bodies*16);x.bounds(xbody,bodies*8);
    const std::size_t groupSize=layout==VtxLayout::Portal2?33:25,stripSize=layout==VtxLayout::Portal2?35:27;
    for(std::size_t b=0;b<bodies;++b) {
        auto bp=bodyStart+b*16,xb=xbody+b*8;auto models=m.count(bp+4,4096);
        if(models!=x.count(xb,4096))throw ContentError("MDL/VTX model count mismatch");
        auto choice=b<choices.size()?choices[b]:0u;if(choice>=models)throw ContentError("Requested body model absent");
        auto mp=m.rel(bp,bp+12)+choice*148,xm=x.rel(xb,xb+4)+choice*8;m.bounds(mp,148);x.bounds(xm,8);
        auto modelVertices=m.count(mp+80,vertexCount),baseVertex=m.count(mp+84,vertexCount*48);
        if(baseVertex%48 || baseVertex/48+modelVertices>vertexCount)throw ContentError("Invalid model vertex range");baseVertex/=48;
        auto meshes=m.count(mp+72,4096),meshStart=m.rel(mp,mp+76);
        if(!meshes)continue;if(x.count(xm,8)==0)throw ContentError("Missing VTX LOD0");
        auto lod=x.rel(xm,xm+4);x.bounds(lod,12);if(meshes!=x.count(lod,4096))throw ContentError("MDL/VTX mesh count mismatch");
        auto xmeshes=x.rel(lod,lod+4);m.bounds(meshStart,meshes*116);x.bounds(xmeshes,meshes*9);
        for(std::size_t j=0;j<meshes;++j) {
            auto mesh=meshStart+j*116,xmesh=xmeshes+j*9;auto material=m.count(mesh,materials),meshVerts=m.count(mesh+8,modelVertices),offset=m.count(mesh+12,modelVertices);
            if(material>=skinRefs||offset+meshVerts>modelVertices)throw ContentError("Invalid mesh material or vertices");
            auto materialIndex=m.u16(skinStart+(skin*skinRefs+material)*2);if(materialIndex>=materials)throw ContentError("Invalid skin material index");
            DrawRange draw{static_cast<std::uint32_t>(out.indices.size()),0,materialIndex};
            auto groups=x.count(xmesh,4096),groupStart=x.rel(xmesh,xmesh+4);x.bounds(groupStart,groups*groupSize);
            for(std::size_t g=0;g<groups;++g) {
                auto gp=groupStart+g*groupSize;auto nverts=x.count(gp,65536),verts=x.rel(gp,gp+4),nindices=x.count(gp+8,10000000),indices=x.rel(gp,gp+12),nstrips=x.count(gp+16,100000),strips=x.rel(gp,gp+20);
                x.bounds(verts,nverts*9);x.bounds(indices,nindices*2);x.bounds(strips,nstrips*stripSize);
                auto vertexAt=[&](std::size_t index){if(index>=nindices)throw ContentError("Strip index out of bounds");auto local=x.u16(indices+index*2);if(local>=nverts)throw ContentError("Strip vertex out of bounds");auto orig=x.u16(verts+local*9+4);if(orig>=meshVerts)throw ContentError("Original mesh vertex out of bounds");return static_cast<std::uint32_t>(baseVertex+offset+orig);};
                for(std::size_t s=0;s<nstrips;++s) {
                    auto sp=strips+s*stripSize;auto count=x.count(sp,nindices),first=x.count(sp+4,nindices),flags=x.u8(sp+18);
                    if(first+count>nindices)throw ContentError("Strip exceeds index array");
                    if((flags&3)==1) {if(count%3)throw ContentError("Incomplete triangle list");for(std::size_t k=0;k<count;k+=3)appendTriangle(out,vertexAt(first+k),vertexAt(first+k+1),vertexAt(first+k+2));}
                    else if((flags&3)==2)for(std::size_t k=2;k<count;++k){auto a=vertexAt(first+k-2),bb=vertexAt(first+k-1),c=vertexAt(first+k);if(k&1)std::swap(a,bb);appendTriangle(out,a,bb,c);}
                    else throw ContentError("Unsupported strip flags");
                }
            }
            draw.indexCount=static_cast<std::uint32_t>(out.indices.size())-draw.firstIndex;out.draws.push_back(draw);
        }
    }
    if(out.indices.empty())throw ContentError("Selected model has no triangles");return out;
}
Model loadModel(const VpkArchive& archive,const std::string& path,VtxLayout layout,unsigned skin,const std::vector<unsigned>& choices) {
    auto p=normalized(path);if(p.size()<4||p.substr(p.size()-4)!=".mdl")throw ContentError("Expected MDL path");auto stem=p.substr(0,p.size()-4);
    auto out=decodeModel(archive.read(p),archive.read(stem+".vvd"),archive.read(stem+".dx90.vtx"),layout,skin,choices);
    for(auto& candidates:out.materials) {
        std::istringstream names(candidates);std::string name;bool found=false;
        while(std::getline(names,name))if(archive.contains(name)){candidates=name;found=true;break;}
        // Some optional skin names have no VMT in the base archive. Only materials
        // used by selected draw ranges are required here.
        if(!found)candidates.clear();
    }
    for(auto draw:out.draws)if(out.materials[draw.material].empty())throw ContentError("Selected material missing from companion archive");
    return out;
}

Texture decodeVtf(const Bytes& bytes) {
    View v{bytes};v.bounds(0,64);if(v.u32(0)!=0x00465456u||v.u32(4)!=7||v.u32(8)>5)throw ContentError("Unsupported VTF header");
    auto minor=v.u32(8),header=v.count(12,bytes.size());unsigned width=v.u16(16),height=v.u16(18),flags=v.u32(20),frames=v.u16(24),format=v.u32(52),mips=v.u8(56),lowFormat=v.u32(57),lowW=v.u8(61),lowH=v.u8(62);
    if(!width||!height||width>16384||height>16384||!frames||!mips||mips>15||(flags&0x4000))throw ContentError("Unsupported VTF dimensions/cubemap");
    if(minor>=2 && v.u16(63)>1)throw ContentError("Volume VTF not supported");
    std::size_t start=header;
    if(minor>=3) {
        v.bounds(0,80);auto resources=v.count(68,32);v.bounds(80,resources*8);bool found=false;
        for(std::size_t i=0;i<resources;++i){auto p=80+i*8;auto tag=v.u32(p);if((tag&0xffffffu)==0x30){if(tag&0x02000000u)throw ContentError("Inline VTF image resource");start=v.u32(p+4);found=true;}}
        if(!found)throw ContentError("VTF high-resolution resource absent");
    } else if(lowW&&lowH)start+=imageBytes(lowFormat,lowW,lowH);
    for(unsigned mip=mips-1;mip>0;--mip)start+=imageBytes(format,std::max(1u,width>>mip),std::max(1u,height>>mip))*frames;
    auto length=imageBytes(format,width,height);v.bounds(start,length);Texture out;out.width=width;out.height=height;
    switch(format){case 0:out.format=TextureFormat::Rgba8;break;case 12:out.format=TextureFormat::Bgra8;break;case 13:case 20:out.format=TextureFormat::Bc1;break;case 14:out.format=TextureFormat::Bc2;break;case 15:out.format=TextureFormat::Bc3;break;default:throw ContentError("Unsupported VTF pixel format");}
    out.pixels.assign(bytes.begin()+start,bytes.begin()+start+length);return out;
}
Material loadMaterial(const VpkArchive& archive,const std::string& vmtPath) {
    auto tokens=kvTokens(archive.read(vmtPath));if(tokens.size()<4)throw ContentError("Invalid VMT material");
    Material m;m.shader=tokens.front();if(m.shader=="patch")throw ContentError("Patch VMT needs explicit include resolution");
    for(std::size_t i=1;i+1<tokens.size();++i){if(tokens[i]=="$basetexture")m.baseTexturePath="materials/"+normalized(tokens[i+1])+".vtf";if(tokens[i]=="$translucent")m.translucent=tokens[i+1]=="1";if(tokens[i]=="$alphatest")m.alphaTest=tokens[i+1]=="1";}
    if(m.baseTexturePath.empty())throw ContentError("Material has no base texture");m.baseTexture=decodeVtf(archive.read(m.baseTexturePath));return m;
}
} // namespace lsc::content
