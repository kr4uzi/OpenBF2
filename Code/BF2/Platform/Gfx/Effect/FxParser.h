// [CHOSEN, not recovered] Source-level reader for the D3DX9 effect syntax that BF2's shaders use.
// It reads already preprocessed text (macros and includes expanded by the shader compiler's
// preprocessor) and returns the effect structure: types, global parameters with semantics,
// annotations and initializers (including sampler_state blocks and shader compile
// expressions), function signatures, techniques, passes and state assignments. Function
// bodies are kept as source ranges only: shader code is compiled separately.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace bf2fx {
namespace src {

struct Error {
    std::string message;
    size_t offset = 0;
    int line = 0;
};

// Numeric base types, plus the D3DX object types.
enum class Base { None, Void, Bool, Int, Uint, Half, Float, Double, String, Texture, Texture1D, Texture2D,
    Texture3D, TextureCube, Sampler, Sampler1D, Sampler2D, Sampler3D, SamplerCube, VertexShader,
    PixelShader, Struct };

struct TypeSpec {
    Base base = Base::None;
    // Numeric layout: scalar rows=cols=1 and vector=false; vectors are 1 x cols with vector=true.
    uint32_t rows = 1, cols = 1;
    bool vector = false, matrix = false;
    bool rowMajor = false, columnMajor = false;
    std::string structName;
    bool numeric() const { return base>=Base::Bool && base<=Base::Double; }
    bool object() const { return base>=Base::String && base<=Base::PixelShader; }
};

struct Expr;
using ExprPtr = std::shared_ptr<Expr>;
struct StateAssign;
struct Variable;

struct Expr {
    enum Kind { Number, Bool, String, Ident, Call, Construct, Cast, Unary, Binary, Ternary, Index, Member,
        List, Compile, Asm, SamplerState, Reference, Null };
    Kind kind = Number;
    double number = 0;
    bool integer = false;       // Literal had no '.', exponent or float suffix.
    bool paren = false;         // Written inside parentheses (D3DX: a state expression).
    std::string text;           // Identifier, operator, string, member name, profile, asm body.
    TypeSpec type;              // Construct/Cast target.
    std::vector<ExprPtr> args;  // Operands, call/list items; Compile: arguments.
    std::vector<StateAssign> states; // SamplerState.
    size_t begin = 0, end = 0;  // Source range [begin,end) in the preprocessed text.
};

struct StateAssign {
    std::string name;   // As written.
    ExprPtr index;      // Optional [index].
    ExprPtr value;
    size_t begin = 0, end = 0;
    int line = 0;
};

struct Variable {
    std::vector<std::string> modifiers; // Lowercase storage/type modifiers as written.
    TypeSpec type;
    std::string name, semantic;
    ExprPtr arraySize;          // Null when not an array. A bare [] is an unsized array.
    bool unsizedArray = false;
    std::vector<Variable> annotations;
    ExprPtr init;
    size_t begin = 0, end = 0;  // Whole declaration statement (shared by multiple declarators).
    int line = 0;
    bool has(const char* m) const;
};

struct Param {
    std::vector<std::string> modifiers; // in, out, inout, uniform, const...
    TypeSpec type;
    std::string name, semantic;
    ExprPtr arraySize;
    ExprPtr defaultValue;
    size_t begin = 0, end = 0; // Declaration text without its default value.
    bool uniform() const;
    bool output() const;
};

struct Function {
    TypeSpec returns;
    std::vector<std::string> modifiers;
    std::string name, semantic;
    std::vector<Param> params;
    bool hasBody = false;
    size_t begin = 0, end = 0, bodyBegin = 0, bodyEnd = 0, nameBegin = 0;
    int line = 0;
};

struct StructMember {
    TypeSpec type;
    std::string name, semantic;
    ExprPtr arraySize;
    std::vector<std::string> modifiers;
};
struct StructDef {
    std::string name;
    std::vector<StructMember> members;
    size_t begin = 0, end = 0;
};

struct Pass {
    std::string name;
    std::vector<Variable> annotations;
    std::vector<StateAssign> states;
    int line = 0;
};

struct Technique {
    std::string name;
    std::vector<Variable> annotations;
    std::vector<Pass> passes;
    size_t begin = 0, end = 0;
    int line = 0;
};

// Source ranges whose text the shader compiler must not see (techniques, effect-only
// initializers and declarations). Replacing them with blanks keeps offsets and lines.
struct Range { size_t begin = 0, end = 0; };
// `register(vs_1_1, c12)`: a register reservation for one shader profile only.
struct Reservation { size_t begin = 0, end = 0; std::string profile, reg; };

struct Effect {
    std::vector<Variable> globals;      // In declaration order; static/const ones included.
    std::vector<Function> functions;
    std::vector<StructDef> structs;
    std::map<std::string,TypeSpec> typedefs;
    std::vector<Technique> techniques;
    std::vector<Range> effectOnly;
    std::vector<Reservation> reservations;
    const StructDef* findStruct(const std::string& name) const;
    const Function* findFunction(const std::string& name) const; // Last definition with a body.
};

// Returns false and fills error on the first syntax error.
bool parse(const std::string& text, Effect& effect, Error& error);
// Text with every effectOnly range blanked (newlines kept).
std::string shaderText(const std::string& text, const Effect& effect);
// Case-insensitive ASCII comparison.
bool iequals(const std::string& a, const std::string& b);
std::string lower(std::string s);

} // namespace src
} // namespace bf2fx
