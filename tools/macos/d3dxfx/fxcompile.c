/* [CHOSEN, not recovered] Offline D3DX9_25 compiler experiment, outside the engine. */
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <d3dx9.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef HRESULT (WINAPI *CreateCompiler)(const char *, UINT, const D3DXMACRO *,
    ID3DXInclude *, DWORD, ID3DXEffectCompiler **, ID3DXBuffer **);
typedef struct IncludeFile {
    void *data;
    char path[MAX_PATH];
    unsigned snapshot_id;
    struct IncludeFile *next;
} IncludeFile;
typedef struct Includes {
    ID3DXInclude iface;
    const char *root;
    const char *source;
    FILE *log;
    IncludeFile *files;
    const char *mode;
    const char *virtual_root;
    const char *snapshot;
} Includes;

static char *read_line(FILE *file);

static void *read_file(const char *path, UINT *length)
{
    FILE *f = fopen(path, "rb");
    long size;
    void *data;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) || (size = ftell(f)) < 0 || size > INT_MAX ||
        fseek(f, 0, SEEK_SET)) { fclose(f); return NULL; }
    data = malloc((size_t)size + 1);
    if (!data) { fclose(f); return NULL; }
    if (fread(data, 1, size, f) != (size_t)size) { free(data); fclose(f); return NULL; }
    fclose(f);
    ((char *)data)[size] = 0;
    *length = (UINT)size;
    return data;
}

static int beside(char *out, const char *parent, const char *name)
{
    const char *slash = strrchr(parent, '/'), *back = strrchr(parent, '\\');
    size_t length;
    if (back && (!slash || back > slash)) slash = back;
    length = slash ? (size_t)(slash - parent + 1) : 0;
    if (length + strlen(name) >= MAX_PATH) return 0;
    memmove(out, parent, length);
    strcpy(out + length, name);
    return 1;
}

static HRESULT WINAPI include_open(ID3DXInclude *iface, D3DXINCLUDE_TYPE type,
    const char *name, const void *parent, const void **data, UINT *bytes)
{
    Includes *inc = (Includes *)iface;
    IncludeFile *node = (IncludeFile *)calloc(1, sizeof(*node)), *p;
    const char *parent_path = inc->source, *virtual_name = name;
    (void)type;
    if (!node) return E_OUTOFMEMORY;
    /* [CHOSEN, not recovered] s2 compiles only bytes captured by the native
       runtime. No filesystem/include fallback is allowed in snapshot mode. */
    if (inc->snapshot) {
        unsigned parent_id = 0;
        FILE *table;
        char *line;
        for (p = inc->files; p; p = p->next)
            if (p->data == parent) { parent_id = p->snapshot_id; break; }
        table = fopen(inc->snapshot, "rb");
        if (!table) { free(node); return E_FAIL; }
        while ((line = read_line(table))) {
            char *fields[5], *cursor = line;
            int n = 0;
            do { fields[n++] = cursor; cursor = strchr(cursor, '\t'); if (cursor) *cursor++ = 0; }
            while (cursor && n < 5);
            if (!cursor && n == 5 && strtoul(fields[1], NULL, 10) == parent_id &&
                strtoul(fields[2], NULL, 10) == (unsigned)type && !strcmp(fields[3], name)) {
                node->snapshot_id = (unsigned)strtoul(fields[0], NULL, 10);
                if (beside(node->path, inc->snapshot, fields[4])) node->data = read_file(node->path, bytes);
                free(line);
                break;
            }
            free(line);
        }
        fclose(table);
        if (!node->data) {
            fprintf(inc->log, "Snapshot include missing: parent=%u type=%u %s\n", parent_id, (unsigned)type, name);
            fflush(inc->log); free(node); return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }
        node->next = inc->files; inc->files = node; *data = node->data;
        fprintf(inc->log, "Snapshot include: %u parent=%u %s (%u bytes)\n", node->snapshot_id, parent_id, name, *bytes);
        fflush(inc->log);
        return S_OK;
    }
    if (!strcmp(inc->mode, "baseline")) {
        for (p = inc->files; p; p = p->next)
            if (p->data == parent) { parent_path = p->path; break; }
        if (beside(node->path, parent_path, name)) node->data = read_file(node->path, bytes);
    }
    /* Engine callbacks ignore type/parent. Map mounted virtual paths to the
       single extracted BF2 archive; never guess a parent-relative engine path. */
    if (!strcmp(inc->mode, "ra") && inc->virtual_root &&
        (!_stricmp(name, "shaders/RaDefines.fx") || !_stricmp(name, "mods/xpack/shaders/RaDefines.fx"))) {
        if (strlen(inc->virtual_root) + 14 >= MAX_PATH) { free(node); return E_INVALIDARG; }
        strcpy(node->path, inc->virtual_root);
        strcat(node->path, "/RaDefines.fx");
        node->data = read_file(node->path, bytes);
        if (!node->data) { free(node); return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND); }
    }
    if (!_strnicmp(name, "shaders/", 8) || !_strnicmp(name, "shaders\\", 8)) virtual_name += 8;
    else if (!_strnicmp(name, "mods/bf2/shaders/", 17)) virtual_name += 17;
    else if (!_strnicmp(name, "mods/xpack/shaders/", 19)) virtual_name += 19;
    else if (strcmp(inc->mode, "baseline")) {
        fprintf(inc->log, "Unsupported mounted include path [%s]: %s\n", inc->mode, name);
        free(node);
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }
    if (!node->data && strlen(inc->root) + strlen(virtual_name) + 2 < MAX_PATH) {
        strcpy(node->path, inc->root);
        strcat(node->path, "/");
        strcat(node->path, virtual_name);
        node->data = read_file(node->path, bytes);
    }
    if (!node->data) {
        fprintf(inc->log, "Missing include: %s (parent %s)\n", name, parent_path);
        fflush(inc->log);
        free(node);
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }
    node->next = inc->files;
    inc->files = node;
    *data = node->data;
    fprintf(inc->log, "Include [%s]: %s -> %s (%u bytes)\n", inc->mode, name, node->path, *bytes);
    fflush(inc->log);
    return S_OK;
}

static HRESULT WINAPI include_close(ID3DXInclude *iface, const void *data)
{
    Includes *inc = (Includes *)iface;
    IncludeFile **link = &inc->files;
    while (*link) {
        IncludeFile *node = *link;
        if (node->data == data) {
            *link = node->next;
            free(node->data); free(node);
            return S_OK;
        }
        link = &node->next;
    }
    return E_INVALIDARG;
}
static ID3DXIncludeVtbl include_vtbl = {include_open, include_close};

static void buffer_log(FILE *log, ID3DXBuffer *buffer)
{
    if (buffer) {
        fwrite(buffer->lpVtbl->GetBufferPointer(buffer), 1,
               buffer->lpVtbl->GetBufferSize(buffer), log);
        fputc('\n', log);
        buffer->lpVtbl->Release(buffer);
    }
    fflush(log);
}

static char *read_line(FILE *file)
{
    size_t size = 256, used = 0;
    char *line = (char *)malloc(size);
    int c;
    if (!line) return NULL;
    while ((c = fgetc(file)) != EOF && c != '\n') {
        if (used + 1 == size) {
            char *more = (char *)realloc(line, size *= 2);
            if (!more) { free(line); fprintf(stderr, "Out of memory\n"); exit(2); }
            line = more;
        }
        line[used++] = (char)c;
    }
    if (!used && c == EOF) { free(line); return NULL; }
    if (used && line[used - 1] == '\r') --used;
    line[used] = 0;
    return line;
}

static int add_define(D3DXMACRO *macros, int *count, const char *text)
{
    char *copy, *equal;
    if (*count >= 1023 || !*text) return 0;
    copy = _strdup(text);
    if (!copy) return 0;
    equal = strchr(copy, '=');
    if (equal) *equal++ = 0;
    if (!*copy) { free(copy); return 0; }
    macros[*count].Name = copy;
    macros[*count].Definition = equal ? equal : "1";
    ++*count;
    return 1;
}

static int compile_job(CreateCompiler create, int argc, char **argv)
{
    ID3DXEffectCompiler *compiler = NULL;
    ID3DXBuffer *errors = NULL, *effect = NULL;
    D3DXMACRO macros[1024] = {{0}};
    Includes inc;
    UINT length = 0;
    void *source = NULL;
    FILE *log, *output, *status;
    HRESULT parse_hr = E_FAIL, compile_hr = E_FAIL;
    DWORD flags = 0;
    char status_path[MAX_PATH];
    int count = 0, i, ok = 0;
    if (argc < 4 || strlen(argv[1]) + 8 >= MAX_PATH) return 2;
    snprintf(status_path, sizeof(status_path), "%s.status", argv[1]);
    remove(argv[1]); remove(status_path);
    log = fopen(argv[2], "wb");
    if (!log) { fprintf(stderr, "Cannot write log %s\n", argv[2]); return 2; }
    memset(&inc, 0, sizeof(inc));
    inc.iface.lpVtbl = &include_vtbl; inc.root = argv[3]; inc.source = argv[0]; inc.log = log;
    inc.mode = "baseline";
    for (i = 4; i < argc; ++i) {
        if (!strncmp(argv[i], "--flags=", 8)) {
            char *end;
            flags = strtoul(argv[i] + 8, &end, 0);
            if (!argv[i][8] || *end) { fprintf(log, "Invalid flags\n"); goto done; }
        } else if (!strncmp(argv[i], "--include-mode=", 15)) {
            inc.mode = argv[i] + 15;
            if (strcmp(inc.mode, "baseline") && strcmp(inc.mode, "legacy") && strcmp(inc.mode, "ra")) {
                fprintf(log, "Invalid include mode\n"); goto done;
            }
        } else if (!strncmp(argv[i], "--virtual-root=", 15)) {
            inc.virtual_root = argv[i] + 15;
        } else if (!strncmp(argv[i], "--snapshot=", 11)) {
            inc.snapshot = argv[i] + 11;
        } else if (argv[i][0] == '@') {
            FILE *list = fopen(argv[i] + 1, "rb");
            char *line;
            if (!list) { fprintf(log, "Cannot read defines %s\n", argv[i]); goto done; }
            while ((line = read_line(list))) {
                int valid = !*line || *line == '#' || add_define(macros, &count, line);
                free(line);
                if (!valid) { fclose(list); fprintf(log, "Invalid define list\n"); goto done; }
            }
            fclose(list);
        } else if (!add_define(macros, &count, argv[i])) { fprintf(log, "Invalid define\n"); goto done; }
    }
    if (!strcmp(inc.mode, "ra") && !inc.virtual_root) {
        fprintf(log, "Ra input requires --virtual-root\n"); goto done;
    }
    source = read_file(argv[0], &length);
    if (!source) { fprintf(log, "Cannot read source %s\n", argv[0]); goto done; }
    fprintf(log, "flags=0x%08lx include=%s\n", (unsigned long)flags, inc.mode);
    parse_hr = create((const char *)source, length, macros, &inc.iface, flags, &compiler, &errors);
    fprintf(log, "D3DXCreateEffectCompiler HRESULT=0x%08lx\n", (unsigned long)parse_hr);
    buffer_log(log, errors); errors = NULL;
    if (FAILED(parse_hr) || !compiler) goto done;
    /* [CHOSEN, not recovered] Offline reflection oracle for the CPU cache ABI.
       Getters do not modify the compiler or its emitted binary. */
    {
        D3DXEFFECT_DESC description;
        if (SUCCEEDED(compiler->lpVtbl->GetDesc(compiler, &description))) {
            UINT parameter;
            for (parameter = 0; parameter < description.Parameters; ++parameter) {
                D3DXPARAMETER_DESC d;
                D3DXHANDLE h = compiler->lpVtbl->GetParameter(compiler, NULL, parameter);
                if (SUCCEEDED(compiler->lpVtbl->GetParameterDesc(compiler, h, &d))) {
                    fprintf(log, "Parameter: %s type=%u class=%u rows=%u columns=%u elements=%u bytes=%u flags=%u\n",
                        d.Name, (unsigned)d.Type, (unsigned)d.Class, d.Rows, d.Columns, d.Elements, d.Bytes, (unsigned)d.Flags);
                    if (d.Class == D3DXPC_MATRIX_ROWS || d.Class == D3DXPC_MATRIX_COLUMNS) {
                        D3DXMATRIX matrix;
                        if (SUCCEEDED(compiler->lpVtbl->GetMatrix(compiler, h, &matrix))) {
                            UINT component;
                            fprintf(log, "Matrix: %s", d.Name);
                            for (component = 0; component < 16; ++component)
                                fprintf(log, " %.9g", ((float *)&matrix)[component]);
                            fputc('\n', log);
                        }
                    }
                }
            }
        }
    }
    compile_hr = compiler->lpVtbl->CompileEffect(compiler, flags, &effect, &errors);
    fprintf(log, "CompileEffect HRESULT=0x%08lx\n", (unsigned long)compile_hr);
    buffer_log(log, errors); errors = NULL;
    if (FAILED(compile_hr) || !effect || !effect->lpVtbl->GetBufferSize(effect)) goto done;
    output = fopen(argv[1], "wb");
    if (output) {
        DWORD size = effect->lpVtbl->GetBufferSize(effect);
        int written = fwrite(effect->lpVtbl->GetBufferPointer(effect), 1, size, output) == size;
        int closed = fclose(output) == 0;
        ok = written && closed;
    }
    if (!ok) fprintf(log, "Cannot write binary %s\n", argv[1]);
done:
    if (effect) effect->lpVtbl->Release(effect);
    if (compiler) compiler->lpVtbl->Release(compiler);
    while (inc.files) include_close(&inc.iface, inc.files->data);
    free(source);
    for (i = 0; i < count; ++i) free((void *)macros[i].Name);
    if (fclose(log)) ok = 0;
    if (!ok) remove(argv[1]);
    status = fopen(status_path, "wb");
    if (!status) return 2;
    fprintf(status, "%s\nparse=0x%08lx\ncompile=0x%08lx\n", ok ? "ok" : "fail",
            (unsigned long)parse_hr, (unsigned long)compile_hr);
    if (fclose(status)) return 2;
    printf("%s %s\n", ok ? "ok" : "fail", argv[0]); fflush(stdout);
    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    HMODULE module;
    CreateCompiler create;
    char path[MAX_PATH];
    DWORD length;
    int failed = 0;
    if (argc < 3) {
        fprintf(stderr, "usage: fxcompile SOURCE OUT LOG INCLUDE_ROOT [NAME=VALUE | @DEFINES_FILE ...]\n"
            "       fxcompile --batch JOBS.tsv\n"
            "TSV: SOURCE<TAB>OUT<TAB>LOG<TAB>INCLUDE_ROOT[<TAB>NAME=VALUE | @DEFINES_FILE ...]\n");
        return 2;
    }
    /* Load only the DLL copied beside this executable, never an implicit system substitute. */
    length = GetModuleFileNameA(NULL, path, sizeof(path));
    if (!length || length >= sizeof(path) || !beside(path, path, "d3dx9_25.dll")) return 2;
    module = LoadLibraryA(path);
    if (!module) { fprintf(stderr, "LoadLibrary %s failed: %lu\n", path, GetLastError()); return 2; }
    create = (CreateCompiler)(void *)GetProcAddress(module, "D3DXCreateEffectCompiler");
    if (!create) { fprintf(stderr, "Missing D3DXCreateEffectCompiler\n"); FreeLibrary(module); return 2; }
    if (!strcmp(argv[1], "--batch")) {
        FILE *list = argc == 3 ? fopen(argv[2], "rb") : NULL;
        char *line;
        if (!list) { fprintf(stderr, "Cannot open batch manifest\n"); FreeLibrary(module); return 2; }
        while ((line = read_line(list))) {
            char *fields[1028], *p = line;
            int count = 0;
            if (*line && *line != '#') {
                do {
                    fields[count++] = p;
                    p = strchr(p, '\t');
                    if (p) *p++ = 0;
                } while (p && count < 1028);
                if (p || count < 4 || compile_job(create, count, fields)) failed = 1;
            }
            free(line);
        }
        if (ferror(list)) failed = 1;
        fclose(list);
    } else failed = compile_job(create, argc - 1, argv + 1);
    FreeLibrary(module);
    return failed;
}
