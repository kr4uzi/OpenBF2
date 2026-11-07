// [CHOSEN, not recovered] D3DX9 effect state names and value keywords for the native effect builder.
#pragma once
#include <cstdint>
#include <string>

namespace bf2fx {
enum class StateValue { Int, Float, Bool, Texture, VertexShader, PixelShader, Sampler };
struct StateInfo {
    const char* name;
    uint32_t id;         // FX2 state ID (MojoShader's numbering), first element for indexed states.
    StateValue value;
    uint32_t indexCount; // 1: not indexed.
    const char* keywords;// Value table name, or null.
};
// Pass states (case-insensitive). Indexed texture-stage/sampler states take their index.
const StateInfo* passState(const std::string& name);
// sampler_state members.
const StateInfo* samplerState(const std::string& name);
// Value keyword for the given state's table (case-insensitive); false when unknown.
bool stateKeyword(const StateInfo& state, const std::string& word, uint32_t& value);
} // namespace bf2fx
