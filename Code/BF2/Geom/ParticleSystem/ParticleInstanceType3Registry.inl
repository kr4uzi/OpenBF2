// Case-insensitive template lookup over the manager's existing name registry.
// Include after g_templatesByName is defined; misses return null.
// PE 0x100D3410; the internal adapter accepts the string's character data.
IParticleSystemTemplate* psmFindTemplateByName(const char* name)
{
    const ParticleSystemNameMap::const_iterator it = g_templatesByName.find(std::string(name));
    return it != g_templatesByName.end() ? it->second : 0;
}
