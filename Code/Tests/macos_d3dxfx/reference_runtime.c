/* [TEMP] [CHOSEN, not recovered] Shader-reference execution/ownership control.
 * [RETIRE-WHEN: macos-effect-cache-input-tests] Keep until equivalent runtime gates exist.
 * Callbacks model shader ownership and register storage only; no engine or draws. */
#include <mojoshader.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Shader { const MOJOSHADER_parseData *pd; unsigned refs; } Shader;
static unsigned live;
static void *bound_vertex, *bound_pixel;
static void *compile_shader(const void *ctx, const char *entry, const unsigned char *bytes,
    unsigned len, const MOJOSHADER_swizzle *swiz, unsigned nswiz,
    const MOJOSHADER_samplerMap *smap, unsigned nsmap)
{
    Shader *s = (Shader *) calloc(1, sizeof (*s));
    (void) ctx;
    s->pd = MOJOSHADER_parse("bytecode", entry, bytes, len, swiz, nswiz, smap, nsmap, NULL, NULL, NULL);
    s->refs = 1; ++live;
    return s;
}
static void add_ref(void *ptr) { assert(ptr); ++((Shader *) ptr)->refs; }
static void delete_shader(const void *ctx, void *ptr)
{
    Shader *s = (Shader *) ptr;
    (void) ctx;
    assert(s); /* A partial FX failure must not call us with an uninitialized slot. */
    if (!--s->refs) { MOJOSHADER_freeParseData(s->pd); free(s); --live; }
}
static MOJOSHADER_parseData *parse_data(void *ptr)
{ return (MOJOSHADER_parseData *) ((Shader *) ptr)->pd; }
static void bind_shaders(const void *ctx, void *v, void *p)
{ (void) ctx; bound_vertex = v; bound_pixel = p; }
static void get_bound(const void *ctx, void **v, void **p)
{ (void) ctx; *v = bound_vertex; *p = bound_pixel; }
static void map_registers(const void *ctx, float **vf, int **vi, unsigned char **vb,
                          float **pf, int **pi, unsigned char **pb)
{
    static float floats[2][4096]; static int ints[2][4096]; static unsigned char bools[2][256];
    (void) ctx;
    *vf = floats[0]; *pf = floats[1]; *vi = ints[0]; *pi = ints[1]; *vb = bools[0]; *pb = bools[1];
}
static void unmap_registers(const void *ctx) { (void) ctx; }
static const char *get_error(const void *ctx) { (void) ctx; return "control compile failed"; }
static MOJOSHADER_effectParam *parameter(MOJOSHADER_effect *e, const char *name)
{
    int i;
    for (i = 0; i < e->param_count; ++i)
        if (!strcmp(e->params[i].value.name, name)) return &e->params[i];
    abort();
}
static void exercise(MOJOSHADER_effect *e)
{
    unsigned passes;
    MOJOSHADER_effectStateChanges changes;
    MOJOSHADER_effectParam *selected = parameter(e, "SelectedVertex");
    MOJOSHADER_effectParam *choices = parameter(e, "VertexChoices");
    unsigned original = selected->value.valuesI[0], replacement = choices->value.valuesI[1];
    void *pixel = e->objects[parameter(e, "SelectedPixel")->value.valuesI[0]].shader.shader;
    MOJOSHADER_effectBegin(e, &passes, 0, &changes);
    assert(passes == 3);
    MOJOSHADER_effectBeginPass(e, 0);
    assert(bound_vertex == e->objects[original].shader.shader && bound_pixel == pixel);
    MOJOSHADER_effectSetRawValueName(e, "SelectedVertex", &replacement, 0, sizeof (replacement));
    MOJOSHADER_effectCommitChanges(e);
    assert(bound_vertex == e->objects[replacement].shader.shader && bound_pixel == pixel);
    MOJOSHADER_effectEndPass(e);
    MOJOSHADER_effectBeginPass(e, 1);
    assert(bound_vertex == e->objects[replacement].shader.shader && bound_pixel == pixel);
    MOJOSHADER_effectEndPass(e);
    MOJOSHADER_effectBeginPass(e, 2);
    assert(bound_vertex == NULL && bound_pixel == NULL);
    MOJOSHADER_effectEndPass(e);
    MOJOSHADER_effectEnd(e);
    MOJOSHADER_effectSetRawValueName(e, "SelectedVertex", &original, 0, sizeof (original));
}
int main(int argc, char **argv)
{
    unsigned char *bytes; long size; FILE *file;
    MOJOSHADER_effect *effect, *clone;
    MOJOSHADER_effectShaderContext ctx = {0};
    assert(argc == 2 || argc == 3);
    file = fopen(argv[1], "rb"); assert(file);
    fseek(file, 0, SEEK_END); size = ftell(file); rewind(file);
    bytes = (unsigned char *) malloc(size); assert(fread(bytes, 1, size, file) == (size_t) size); fclose(file);
    ctx.compileShader = compile_shader; ctx.shaderAddRef = add_ref; ctx.deleteShader = delete_shader;
    ctx.getParseData = parse_data; ctx.bindShaders = bind_shaders; ctx.getBoundShaders = get_bound;
    ctx.mapUniformBufferMemory = map_registers; ctx.unmapUniformBufferMemory = unmap_registers;
    ctx.getError = get_error;
    effect = MOJOSHADER_compileEffect(bytes, (unsigned) size, NULL, 0, NULL, 0, &ctx);
    assert(effect);
    if (argc == 3 && (!strcmp(argv[2], "expressions") || !strcmp(argv[2], "frontier"))) {
        unsigned i, count = 0, passes;
        MOJOSHADER_effectStateChanges changes;
        assert(effect->error_count == 0);
        clone = MOJOSHADER_cloneEffect(effect); assert(clone);
        for (i = 0; i < effect->techniques[0].passes[0].state_count; ++i) {
            MOJOSHADER_effectValue *a = &effect->techniques[0].passes[0].states[i].value;
            MOJOSHADER_effectValue *b = &clone->techniques[0].passes[0].states[i].value;
            if (!a->expression_length) continue;
            ++count;
            assert(a->expression != b->expression && a->expression_length == b->expression_length);
            assert(a->expression_type == b->expression_type);
            assert(!memcmp(a->expression, b->expression, a->expression_length));
        }
        assert(count == 2);
        MOJOSHADER_deleteEffect(effect);
        if (!strcmp(argv[2], "frontier")) {
            MOJOSHADER_effectBegin(clone, &passes, 0, &changes);
            MOJOSHADER_effectBeginPass(clone, 0);
            assert(!"Expression execution returned without an evaluator");
        }
        MOJOSHADER_deleteEffect(clone); assert(live == 0); free(bytes);
        puts("Dynamic state payloads retained and deep-cloned."); return 0;
    }
    if (argc == 3) {
        assert(effect->error_count > 0);
        MOJOSHADER_deleteEffect(effect); assert(live == 0); free(bytes);
        puts("Rejected malformed reference/bytecode and cleaned partial shader objects."); return 0;
    }
    assert(effect->error_count == 0);
    exercise(effect);
    clone = MOJOSHADER_cloneEffect(effect); assert(clone && clone->error_count == 0);
    MOJOSHADER_deleteEffect(effect);
    exercise(clone);
    MOJOSHADER_deleteEffect(clone);
    assert(live == 0); free(bytes);
    puts("Scalar/array selection, live CommitChanges, null unbind, clone ownership passed.");
    return 0;
}
