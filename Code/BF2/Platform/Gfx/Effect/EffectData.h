// [CHOSEN, not recovered] CPU effect cache contract for the owner-requested macOS port.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace bf2fx {
using Words = std::vector<uint32_t>;
using Bytes = std::vector<uint8_t>;
// Version 2: entry kinds (refusal and reflection-only records), MojoShader patch 0005.
// Version 3: MojoShader patches 0006 (log2|x|) and 0007 (vs_1_x expp): MSL changes.
// Version 4: the native identity hashes the linked vkd3d/MojoShader archives, and a failed
// background build of a D3DX-accepted request is no longer published as a refusal. Version-3
// caches may hold such refusals from stale deps builds; they are not read.
// Request envelopes keep their version-1 layout and are still read for --retarget.
constexpr uint32_t CacheVersion = 4;
// Numeric enum values in this file are MojoShader's pinned FX2 enums, not D3D states.
enum : uint32_t { Bool = 1, Int = 2, Float = 3, String = 4, Texture = 5,
    Sampler = 10, PixelShader = 15, VertexShader = 16 };
struct Type {
    std::string name;
    uint32_t type = 0, cls = 0, rows = 0, columns = 0, elements = 0;
    std::vector<Type> members;
    template<class A> void io(A& a) { a(name,type,cls,rows,columns,elements,members); }
};
struct Binding {
    Type type;
    uint32_t set = 0, index = 0, count = 0;
    template<class A> void io(A& a) { a(type,set,index,count); }
};
struct Operand {
    uint32_t kind = 0, index = 0;
    Words arrays;
    template<class A> void io(A& a) { a(kind,index,arrays); }
};
struct Instruction {
    uint32_t opcode = 0, elements = 0;
    std::vector<Operand> operands;
    template<class A> void io(A& a) { a(opcode,elements,operands); }
};
struct Program {
    uint32_t temps = 0, inputs = 0;
    // IEEE-754 double bit patterns, serialized as little-endian uint64 values.
    std::vector<uint64_t> literals;
    std::vector<Binding> bindings;
    std::vector<Instruction> instructions;
    template<class A> void io(A& a) { a(temps,inputs,literals,bindings,instructions); }
};
struct Value {
    Type type;
    std::string semantic, text, reference;
    Words words; // FX2 numeric rows padded to four components; matrices are logical row-major.
    uint32_t expressionType = 0;
    Bytes expression; // Original payload retained alongside the executable IR.
    Program program;
    Words stateIds;
    std::vector<Value> states; // Sampler states, in original order.
    template<class A> void io(A& a) {
        a(type,semantic,text,reference,words,expressionType,expression,program,stateIds,states);
    }
};
struct Parameter {
    Value value;
    uint32_t flags = 0;
    std::vector<Value> annotations;
    template<class A> void io(A& a) { a(value,flags,annotations); }
};
struct State {
    uint32_t id = 0;
    Value value;
    uint32_t index = 0; // FX2 indexed state target, independently of its dense state ID.
    template<class A> void io(A& a) { a(id,index,value); }
};
struct Pass {
    std::string name;
    std::vector<State> states;
    std::vector<Value> annotations;
    template<class A> void io(A& a) { a(name,states,annotations); }
};
struct Technique {
    std::string name;
    std::vector<Pass> passes;
    std::vector<Value> annotations;
    template<class A> void io(A& a) { a(name,passes,annotations); }
};
struct Uniform {
    uint32_t kind = 0, index = 0, count = 0, constant = 0;
    std::string name;
    template<class A> void io(A& a) { a(kind,index,count,constant,name); }
};
struct Attribute {
    uint32_t usage = 0, index = 0, format = 0;
    std::string name;
    template<class A> void io(A& a) { a(usage,index,format,name); }
};
struct Shader {
    uint32_t object = 0, stage = 0, major = 0, minor = 0;
    // selectorKind: 0 compiled shader, 1 preshader selector, 2 fixed parameter/element.
    uint32_t selectorKind = 0, selectorParameter = 0, selectorElement = 0;
    std::string entry, msl;
    Bytes bytecode;
    std::vector<Binding> bindings;
    std::vector<Uniform> uniforms;
    std::vector<Attribute> attributes, outputs;
    // Sampler: kind = dimension, index = both MSL texture/sampler slot, name = symbol.
    std::vector<Uniform> samplers;
    std::vector<State> constants; // id = register, value.type.type = Bool/Int/Float.
    Program preshader;
    template<class A> void io(A& a) {
        a(object,stage,major,minor,selectorKind,selectorParameter,selectorElement,
          entry,msl,bytecode,bindings,uniforms,attributes,outputs,samplers,constants,preshader);
    }
};
struct Description {
    std::vector<Parameter> parameters;
    std::vector<Technique> techniques;
    std::vector<Shader> shaders;
    // Object-to-parameter names include sampler Texture=<name> references and strings.
    std::vector<std::string> objects;
    template<class A> void io(A& a) { a(parameters,techniques,shaders,objects); }
};
struct Define {
    std::string name, value;
    template<class A> void io(A& a) { a(name,value); }
};
struct Include {
    uint32_t parent = 0, kind = 0, found = 0;
    std::string name;
    Bytes bytes;
    template<class A> void io(A& a) { a(parent,kind,found,name,bytes); }
};
struct Request {
    Bytes source;
    std::vector<Define> defines;
    uint32_t flags = 0;
    std::string dllHash, translator;
    std::vector<Include> includes;
    template<class A> void io(A& a) { a(source,defines,flags,dllHash,translator,includes); }
};
// [CHOSEN, not recovered] Effect: FX2 plus description. Refused: D3DX9_25 refused the request;
// parseResult/compileResult are its D3DXCreateEffectCompiler/CompileEffect HRESULTs and log
// its error text, so the facade fails exactly as D3DX did. ReflectionOnly: the compiler object
// parses (parseResult 0) but CompileEffect fails (an .mfx without techniques); description
// holds the compiler's parameters and annotations, fx2 is empty.
enum EntryKind : uint32_t { EffectEntry = 0, RefusedEntry = 1, ReflectionOnlyEntry = 2 };
struct Entry {
    std::string key;
    uint32_t kind = EffectEntry;
    uint32_t parseResult = 0, compileResult = 0;
    std::string log;
    Bytes fx2;
    Description description;
    template<class A> void io(A& a) { a(key,kind,parseResult,compileResult,log,fx2,description); }
};
} // namespace bf2fx
