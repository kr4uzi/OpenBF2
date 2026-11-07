// [CHOSEN, not recovered] Offline FX2 -> stable CPU description/MSL exporter.
// MojoShader (zlib, Ryan C. Gordon) supplies parsing and translation; no Apple frameworks.
#include "Cache.h"
#include "Wire.h"
#include <mojoshader.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

using namespace bf2fx;
namespace fs=std::filesystem;
static std::string safe(const char* s) {return s?s:"";}
static Type type(const MOJOSHADER_symbolTypeInfo& t,const char* name="") {
    Type v;v.name=safe(name);v.type=t.parameter_type;v.cls=t.parameter_class;
    v.rows=t.rows;v.columns=t.columns;v.elements=t.elements;
    for(unsigned i=0;i<t.member_count;++i)v.members.push_back(type(t.members[i].info,t.members[i].name));return v;
}
static Binding binding(const MOJOSHADER_symbol& s) {return {type(s.info,s.name),uint32_t(s.register_set),s.register_index,s.register_count};}
static Program program(const MOJOSHADER_preshader* p) {
    Program v;if(!p)return v;v.temps=p->temp_count;v.inputs=p->register_count;
    for(unsigned i=0;i<p->literal_count;++i) {uint64_t w;std::memcpy(&w,p->literals+i,8);v.literals.push_back(w);}
    for(unsigned i=0;i<p->symbol_count;++i)v.bindings.push_back(binding(p->symbols[i]));
    for(unsigned i=0;i<p->instruction_count;++i) {
        const auto& s=p->instructions[i];Instruction inst;inst.opcode=s.opcode;inst.elements=s.element_count;
        for(unsigned j=0;j<s.operand_count;++j) {
            const auto& o=s.operands[j];Operand op;op.kind=o.type;op.index=o.index;
            if(o.array_register_count)op.arrays.assign(o.array_registers,o.array_registers+o.array_register_count);
            inst.operands.push_back(std::move(op));
        }v.instructions.push_back(std::move(inst));
    }
    return v;
}
static Value value(const MOJOSHADER_effectValue& s,const MOJOSHADER_effect& effect) {
    Value v;v.type=type(s.type,s.name);v.semantic=safe(s.semantic);
    if(s.expression_length) {
        v.expressionType=s.expression_type;auto p=static_cast<const uint8_t*>(s.expression);v.expression.assign(p,p+s.expression_length);
        if(s.expression_type==1) {
            if(!std::memchr(p,0,s.expression_length))throw std::runtime_error("Unterminated state reference");
            v.reference=reinterpret_cast<const char*>(p);
        } else if(s.expression_type==0) {
            auto* parsed=MOJOSHADER_parsePreshader(p,s.expression_length,nullptr,nullptr,nullptr);
            if(!parsed)throw std::runtime_error("Invalid state expression bytecode");
            v.program=program(parsed);MOJOSHADER_freePreshader(parsed);
        } else throw std::runtime_error("Unsupported state expression encoding");
    }
    if(s.type.parameter_type>=MOJOSHADER_SYMTYPE_SAMPLER && s.type.parameter_type<=MOJOSHADER_SYMTYPE_SAMPLERCUBE && !s.expression_length) {
        for(unsigned i=0;i<s.value_count;++i) {v.stateIds.push_back(s.valuesSS[i].type);v.states.push_back(value(s.valuesSS[i].value,effect));}
    } else if(s.value_count && s.values) {
        v.words.assign(reinterpret_cast<const uint32_t*>(s.values),reinterpret_cast<const uint32_t*>(s.values)+s.value_count);
        if(s.type.parameter_type==MOJOSHADER_SYMTYPE_STRING && v.words[0]<unsigned(effect.object_count))
            v.text=safe(effect.objects[v.words[0]].string.string);
    }
    // Repack MojoShader's swapped FX2 matrix dimensions into logical rows,
    // matching D3DX9_25 GetParameterDesc/GetMatrix (parameter-contract.fx).
    if(s.type.parameter_class==MOJOSHADER_SYMCLASS_MATRIX_ROWS || s.type.parameter_class==MOJOSHADER_SYMCLASS_MATRIX_COLUMNS) {
        std::swap(v.type.rows,v.type.columns);v.type.cls=2;
        if(!v.words.empty()) {
            Words packed;for(uint32_t e=0;e<std::max(1u,s.type.elements);++e)
                for(uint32_t r=0;r<s.type.rows;++r)for(uint32_t c=0;c<s.type.columns;++c)
                    packed.push_back(v.words.at((e*s.type.rows+r)*4+c));
            v.words.assign(v.type.rows*4*std::max(1u,v.type.elements),0);size_t pos=0;
            for(uint32_t e=0;e<std::max(1u,v.type.elements);++e)
                for(uint32_t r=0;r<v.type.rows;++r)for(uint32_t c=0;c<v.type.columns;++c)v.words[(e*v.type.rows+r)*4+c]=packed.at(pos++);
        }
    }
    // [CHOSEN, not recovered] Struct members: the same logical-row form for matrix members
    // (MojoShader reads member dimensions in the swapped FX2 order too), and zero storage in the
    // runtime's member layout. MojoShader's struct value reader copies member rows from the type
    // record, not the value, so its words are not D3DX's defaults; BF2's struct parameters have
    // no initializers (the native compiler refuses them), so D3DX's defaults are zero.
    if(s.type.parameter_class==MOJOSHADER_SYMCLASS_STRUCT) {
        size_t words=0;
        for(auto& m:v.type.members) {
            if(m.cls==MOJOSHADER_SYMCLASS_MATRIX_ROWS || m.cls==MOJOSHADER_SYMCLASS_MATRIX_COLUMNS) {std::swap(m.rows,m.columns);m.cls=2;}
            words+=size_t(m.rows)*4*std::max(1u,m.elements);
        }
        v.words.assign(words*std::max(1u,v.type.elements),0);
    }
    return v;
}
static std::vector<Value> annotations(const MOJOSHADER_effectAnnotation* src,unsigned count,const MOJOSHADER_effect& effect) {
    std::vector<Value> v;for(unsigned i=0;i<count;++i)v.push_back(value(src[i],effect));return v;
}
struct Captured {
    std::string entry;
    Bytes bytes;
    std::vector<MOJOSHADER_swizzle> swizzles;
    std::vector<MOJOSHADER_samplerMap> samplers;
    const MOJOSHADER_parseData* parsed=nullptr;
};
static void* captureShader(const void*,const char* name,const uint8_t* bytes,unsigned size,
                          const MOJOSHADER_swizzle* swiz,unsigned ns,const MOJOSHADER_samplerMap* samplers,unsigned nm) {
    auto* s=new Captured;s->entry=safe(name);s->bytes.assign(bytes,bytes+size);
    if(ns)s->swizzles.assign(swiz,swiz+ns);if(nm)s->samplers.assign(samplers,samplers+nm);
    s->parsed=MOJOSHADER_parse(MOJOSHADER_PROFILE_BYTECODE,name,bytes,size,swiz,ns,samplers,nm,nullptr,nullptr,nullptr);return s;
}
static void deleteShader(const void*,void* ptr) {auto* s=static_cast<Captured*>(ptr);MOJOSHADER_freeParseData(s->parsed);delete s;}
static MOJOSHADER_parseData* getParseData(void* ptr) {return const_cast<MOJOSHADER_parseData*>(static_cast<Captured*>(ptr)->parsed);}
static const char* error(const void*) {return "FX2 shader parsing failed";}
static uint32_t u32(const Bytes& b,size_t offset) {
    if(offset>b.size() || b.size()-offset<4)throw std::runtime_error("Truncated FX2 parameter table");
    return uint32_t(b[offset])|uint32_t(b[offset+1])<<8|uint32_t(b[offset+2])<<16|uint32_t(b[offset+3])<<24;
}
static Description describe(const Bytes& bytes) {
    if(bytes.size()<24 || u32(bytes,0)!=0xfeff0901)throw std::runtime_error("Expected FX2 header");
    MOJOSHADER_effectShaderContext ctx{};ctx.compileShader=captureShader;ctx.deleteShader=deleteShader;ctx.getParseData=getParseData;ctx.getError=error;
    std::unique_ptr<MOJOSHADER_effect,decltype(&MOJOSHADER_deleteEffect)> effect(
        MOJOSHADER_compileEffect(bytes.data(),unsigned(bytes.size()),nullptr,0,nullptr,0,&ctx),MOJOSHADER_deleteEffect);
    if(!effect)throw std::runtime_error("No parsed effect");
    if(effect->error_count)throw std::runtime_error(safe(effect->errors[0].error));
    Description d;size_t paramPos=8+u32(bytes,4)+16;
    for(int i=0;i<effect->param_count;++i) {
        const auto& p=effect->params[i];Parameter v;v.value=value(p.value,*effect);
        v.flags=u32(bytes,paramPos+8);paramPos+=16+size_t(u32(bytes,paramPos+12))*8;
        v.annotations=annotations(p.annotations,p.annotation_count,*effect);d.parameters.push_back(std::move(v));
    }
    for(int i=0;i<effect->technique_count;++i) {
        const auto& s=effect->techniques[i];Technique v;v.name=safe(s.name);v.annotations=annotations(s.annotations,s.annotation_count,*effect);
        // MojoShader discards this word. Preserve indexed constants/sampler assignments
        // directly from the FX2 state records instead of merging them into state IDs.
        uint32_t techAnnotations=u32(bytes,paramPos+4);paramPos+=12+size_t(techAnnotations)*8;
        for(unsigned j=0;j<s.pass_count;++j) {
            const auto& p=s.passes[j];Pass pass;pass.name=safe(p.name);pass.annotations=annotations(p.annotations,p.annotation_count,*effect);
            uint32_t passAnnotations=u32(bytes,paramPos+4);paramPos+=12+size_t(passAnnotations)*8;
            for(unsigned k=0;k<p.state_count;++k) {
                pass.states.push_back({uint32_t(p.states[k].type),value(p.states[k].value,*effect),u32(bytes,paramPos+4)});paramPos+=16;
            }
            v.passes.push_back(std::move(pass));
        }d.techniques.push_back(std::move(v));
    }
    for(int i=0;i<effect->object_count;++i) {
        const auto& o=effect->objects[i];d.objects.emplace_back();
        if(o.type>=MOJOSHADER_SYMTYPE_TEXTURE && o.type<=MOJOSHADER_SYMTYPE_SAMPLERCUBE)d.objects.back()=safe(o.mapping.name);
        else if(o.type==MOJOSHADER_SYMTYPE_STRING)d.objects.back()=safe(o.string.string);
        if(o.type!=MOJOSHADER_SYMTYPE_VERTEXSHADER && o.type!=MOJOSHADER_SYMTYPE_PIXELSHADER)continue;
        const auto& src=o.shader;Shader s;s.object=i;s.stage=o.type;s.selectorKind=src.is_preshader;
        if(src.is_preshader) {
            s.selectorParameter=src.params[0];
            if(src.is_preshader==1)s.preshader=program(src.preshader);else s.selectorElement=src.params[1];
        } else if(src.shader) {
            const auto& captured=*static_cast<const Captured*>(src.shader);s.entry=captured.entry;s.bytecode=captured.bytes;
            const auto& pd=*captured.parsed;s.major=pd.major_ver;s.minor=pd.minor_ver;s.preshader=program(pd.preshader);
            for(int j=0;j<pd.symbol_count;++j)s.bindings.push_back(binding(pd.symbols[j]));
            auto* raw=MOJOSHADER_parse(MOJOSHADER_PROFILE_METAL,s.entry.c_str(),s.bytecode.data(),unsigned(s.bytecode.size()),
                captured.swizzles.data(),unsigned(captured.swizzles.size()),captured.samplers.data(),unsigned(captured.samplers.size()),nullptr,nullptr,nullptr);
            std::unique_ptr<const MOJOSHADER_parseData,decltype(&MOJOSHADER_freeParseData)> msl(raw,MOJOSHADER_freeParseData);
            if(msl->error_count)throw std::runtime_error(safe(msl->errors[0].error));
            if(!msl->output || msl->output_len<=0)throw std::runtime_error("Empty MSL");s.msl.assign(msl->output,msl->output_len);
            for(int j=0;j<msl->uniform_count;++j) {const auto& u=msl->uniforms[j];s.uniforms.push_back({uint32_t(u.type),uint32_t(u.index),uint32_t(std::max(1,u.array_count)),uint32_t(u.constant),safe(u.name)});}
            for(int j=0;j<msl->sampler_count;++j) {const auto& u=msl->samplers[j];s.samplers.push_back({uint32_t(u.type),uint32_t(u.index),1,uint32_t(u.texbem),safe(u.name)});}
            for(int j=0;j<msl->attribute_count;++j) {const auto& a=msl->attributes[j];s.attributes.push_back({uint32_t(a.usage),uint32_t(a.index),0,safe(a.name)});}
            for(int j=0;j<msl->output_count;++j) {const auto& a=msl->outputs[j];s.outputs.push_back({uint32_t(a.usage),uint32_t(a.index),0,safe(a.name)});}
            for(int j=0;j<pd.constant_count;++j) {
                const auto& c=pd.constants[j];State v;v.id=c.index;v.value.type.type=c.type==MOJOSHADER_UNIFORM_FLOAT?Float:c.type==MOJOSHADER_UNIFORM_INT?Int:Bool;
                uint32_t w[4]{};std::memcpy(w,&c.value,c.type==MOJOSHADER_UNIFORM_BOOL?4:16);v.value.words.assign(w,w+4);s.constants.push_back(v);
            }
        }d.shaders.push_back(std::move(s));
    }return d;
}
struct FileResolver:IncludeResolver {
    fs::path root,virtualRoot,source;
    std::string mode;
    std::map<const void*,fs::path> paths;
    std::map<const void*,std::unique_ptr<Bytes>> allocations;
    bool open(uint32_t,const std::string& name,const void* parent,const void*& bytes,size_t& size) override {
        std::string lower=name;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
        std::replace(lower.begin(),lower.end(),'\\','/');fs::path path;
        if(mode=="baseline") {auto p=paths.find(parent);path=(p==paths.end()?source:p->second).parent_path()/name;if(!fs::is_regular_file(path))path.clear();}
        if(mode=="ra" && (lower=="shaders/radefines.fx" || lower=="mods/xpack/shaders/radefines.fx"))path=virtualRoot/"RaDefines.fx";
        if(path.empty()) {
            for(const auto& prefix:{"shaders/","mods/bf2/shaders/","mods/xpack/shaders/"}) if(lower.rfind(prefix,0)==0) {lower=lower.substr(std::strlen(prefix));break;}
            // The extracted corpus is flat. Unknown virtual prefixes never escape its root.
            if(lower.find('/')!=std::string::npos || lower=="..")return false;
            for(const auto& p:fs::directory_iterator(root)) {auto s=p.path().filename().string();std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::tolower(c));});if(s==lower){path=p.path();break;}}
        }
        if(path.empty() || !fs::is_regular_file(path))return false;
        auto data=std::make_unique<Bytes>(readFile(path.string()));
        // Open/Close identities must remain unique even for empty include files.
        if(data->empty())data->reserve(1);bytes=data->data();size=data->size();paths[bytes]=path;allocations[bytes]=std::move(data);return true;
    }
    void close(const void* bytes) override {paths.erase(bytes);allocations.erase(bytes);}
};
static Request request(const char* path) {return unenvelope<Request>(readFile(path),"request");}
// [CHOSEN, not recovered] fxcompile's status file: ok|fail, parse=0x..., compile=0x....
static void hresults(const char* path,uint32_t& parse,uint32_t& compile) {
    auto b=readFile(path);std::string t(b.begin(),b.end());auto p=t.find("parse=0x"),c=t.find("compile=0x");
    if(p==std::string::npos || c==std::string::npos)throw std::runtime_error("Invalid compiler status");
    parse=uint32_t(std::stoul(t.substr(p+8,8),nullptr,16));compile=uint32_t(std::stoul(t.substr(c+10,8),nullptr,16));
}
// D3DX's error buffer is what fxcompile logged after the failing call's HRESULT line.
static std::string compilerErrors(const char* path) {
    auto b=readFile(path);std::string t(b.begin(),b.end());auto at=t.rfind("HRESULT=0x");
    if(at==std::string::npos)return std::string();at=t.find('\n',at);return at==std::string::npos?std::string():t.substr(at+1);
}
static void writeText(const fs::path& path,const std::string& s) {writeFile(path.string(),Bytes(s.begin(),s.end()));}
int main(int argc,char** argv) {
    try {
        if(argc<3)throw std::runtime_error("cache-tool capture|unpack|publish|key|verify ...");std::string command=argv[1];
        if(command=="capture" && argc==12) {
            FileResolver resolver;resolver.source=argv[2];resolver.root=argv[7];resolver.mode=argv[8];resolver.virtualRoot=argv[9];
            auto source=readFile(argv[2]);std::ifstream defs(argv[3]);if(!defs)throw std::runtime_error("Cannot read defines");std::string line;std::vector<Define> defines;
            while(std::getline(defs,line)) {auto equal=line.find('=');if(equal==std::string::npos)throw std::runtime_error("Invalid defines");defines.push_back({line.substr(0,equal),line.substr(equal+1)});}
            Request r;auto result=capture(source.data(),source.size(),defines,uint32_t(std::stoul(argv[4],nullptr,0)),argv[5],argv[6],&resolver,r);
            if(!result)throw std::runtime_error(result.detail);
            writeFile(argv[10],envelope(r,"request"));writeText(argv[11],cacheKey(r)+"\n");std::cout<<cacheKey(r)<<'\n';
        } else if(command=="unpack" && argc==4) {
            auto r=request(argv[2]);fs::path dir=argv[3];fs::create_directories(dir);writeFile((dir/"source.fx").string(),r.source);
            std::string defs,table;for(const auto& d:r.defines) {if(d.name.find_first_of("\r\n\t")!=std::string::npos || d.value.find_first_of("\r\n\t")!=std::string::npos)throw std::runtime_error("Multiline macros require a binary compiler manifest");defs+=d.name+"="+d.value+"\n";}
            for(size_t i=0;i<r.includes.size();++i) {
                const auto& inc=r.includes[i];if(inc.name.find_first_of("\r\n\t")!=std::string::npos)throw std::runtime_error("Invalid include name");
                if(!inc.found)continue;auto name="include_"+std::to_string(i+1)+".fx";writeFile((dir/name).string(),inc.bytes);
                table+=std::to_string(i+1)+"\t"+std::to_string(inc.parent)+"\t"+std::to_string(inc.kind)+"\t"+inc.name+"\t"+name+"\n";
            }
            writeText(dir/"defines.txt",defs);writeText(dir/"includes.tsv",table);
            writeText(dir/"info.txt",std::to_string(r.flags)+"\n"+r.dllHash+"\n"+r.translator+"\n");std::cout<<cacheKey(r)<<'\n';
        } else if(command=="publish" && argc==5) {
            auto r=request(argv[2]);Entry e;e.key=cacheKey(r);e.fx2=readFile(argv[3]);e.description=describe(e.fx2);
            auto result=publish(argv[4],r,e);if(!result)throw std::runtime_error(result.detail);std::cout<<e.key<<'\n';
        } else if(command=="verify" && argc==4) {
            auto r=request(argv[2]);Entry e;auto result=load(argv[3],r,e);if(!result && result.error!=Error::Refused)throw std::runtime_error(result.detail);
            if(e.kind!=EffectEntry)std::cout<<(e.kind==RefusedEntry?"refused ":"reflection-only ");
            std::cout<<e.description.parameters.size()<<" parameters, "<<e.description.techniques.size()<<" techniques, "<<e.description.shaders.size()<<" shaders\n";
        } else if(command=="inspect" && argc==4) {
            auto r=request(argv[2]);Entry e;auto result=load(argv[3],r,e);if(!result)throw std::runtime_error(result.detail);
            for(const auto& p:e.description.parameters)std::cout<<"parameter "<<p.value.type.name<<" class="<<p.value.type.cls<<" rows="<<p.value.type.rows<<" cols="<<p.value.type.columns<<" elements="<<p.value.type.elements<<" words="<<p.value.words.size()<<'\n';
            for(const auto& s:e.description.shaders) {
                std::cout<<"shader "<<s.object<<" selector="<<s.selectorKind<<" preshader="<<s.preshader.instructions.size()<<'\n';
                for(const auto& b:s.bindings)std::cout<<" binding "<<b.type.name<<" class="<<b.type.cls<<" rows="<<b.type.rows<<" cols="<<b.type.columns<<" elements="<<b.type.elements<<" set="<<b.set<<" reg="<<b.index<<" count="<<b.count<<'\n';
            }
        } else if(command=="publish-refused" && argc==6) {
            // request status compile.log cache: D3DX refused this request; fail as it did.
            auto r=request(argv[2]);Entry e;e.key=cacheKey(r);e.kind=RefusedEntry;hresults(argv[3],e.parseResult,e.compileResult);
            e.log=compilerErrors(argv[4]);auto result=publish(argv[5],r,e);if(!result)throw std::runtime_error(result.detail);std::cout<<e.key<<'\n';
        } else if(command=="publish-reflection" && argc==7) {
            // request status compile.log derived.fxo cache: the source parses but has no technique.
            // derived.fxo compiles the same source plus one synthetic technique, which is removed.
            auto r=request(argv[2]);Entry e;e.key=cacheKey(r);e.kind=ReflectionOnlyEntry;hresults(argv[3],e.parseResult,e.compileResult);
            e.log=compilerErrors(argv[4]);e.description=describe(readFile(argv[5]));
            auto& t=e.description.techniques;
            if(t.size()!=1 || t[0].name!="bf2fx_reflection" || !e.description.shaders.empty())
                throw std::runtime_error("Reflection effect must contain only the synthetic technique");
            t.clear();auto result=publish(argv[6],r,e);if(!result)throw std::runtime_error(result.detail);std::cout<<e.key<<'\n';
        } else if(command=="retarget" && argc==6) {
            // request dllHash translator out.request: same snapshot under the current toolchain.
            auto r=request(argv[2]);r.dllHash=argv[3];r.translator=argv[4];writeFile(argv[5],envelope(r,"request"));std::cout<<cacheKey(r)<<'\n';
        } else if(command=="key" && argc==3)std::cout<<cacheKey(request(argv[2]))<<'\n';
        else throw std::runtime_error("Invalid cache-tool arguments");
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 2;}
}
