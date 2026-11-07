// Stacked sample-history graph. Private method names are [INFERRED].
#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/ThunkAbi.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendMisc/IRenderView.h"
#include "Debug.h"
#include "DebugTextWriter.h"
#if !defined(_MSC_VER)
#include "../Platform/X87Compat.h"
#endif
#include <string>
#include <vector>
#include <list>
#include <utility>
#include <cstring>
#include <new>
#include <cmath>
#include <climits>
#include <cfenv>

namespace dice { namespace hfe {
namespace dx9rend {
extern void* g_shaderManager;
void* shaderManagerGetEffect(void*, const char*);
int bufferManagerSwapPool(int);
void* bufferManagerCreateVertexBuffer(unsigned int, unsigned int, unsigned int, const char*);
void* vertexBufferLock(void*, unsigned int, unsigned int, unsigned int);
bool vertexBufferUnlock(void*);
void vertexBufferRelease(void*);
void vertexBufferBindStream(void*, unsigned int);
ShaderParameter* effectGetShaderParameterBorrowedTyped(Effect*, const std::string&);
bool effectSetTechniqueByName(void*, const std::string&);
unsigned int effectBegin(void*);
void effectBeginPass(void*, unsigned int);
void effectEnd(void*);
int effectRelease(Effect*);
void rendererDrawPrimitive(unsigned int, unsigned int, unsigned int);
}
namespace geom {

namespace {
struct BlockVertex { float x, y; unsigned int color; };
static_assert(sizeof(BlockVertex) == 12, "block graph vertex stride");

int coordinate(float x)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    unsigned short saved, chop;
    int result;
    __asm {
        fld x
        fnstcw saved
        mov ax, saved
        or ax, 0C00h
        mov chop, ax
        fldcw chop
        fistp result
        fldcw saved
    }
    return result;
#elif !defined(_MSC_VER)
    return bf2x87::integer<int>(bf2x87::wide(x));
#else
    // [CHOSEN, not recovered] MSVC x64 has no x87 inline assembly.
    if (!std::isfinite(x) || x < -2147483648.0 || x >= 2147483648.0)
    { std::feraiseexcept(FE_INVALID); return INT_MIN; }
    return static_cast<int>(x);
#endif
}

int rightCoordinate(float x, float width)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const float gap = 10.0f;
    unsigned short saved, chop;
    int result;
    __asm {
        fld width
        fadd x
        fadd gap
        fnstcw saved
        mov ax, saved
        or ax, 0C00h
        mov chop, ax
        fldcw chop
        fistp result
        fldcw saved
    }
    return result;
#elif !defined(_MSC_VER)
    return bf2x87::integer<int>(bf2x87::add(
        bf2x87::add(bf2x87::wide(width), bf2x87::wide(x)), bf2x87::wide(10.0f)));
#else
    // [CHOSEN, not recovered] MSVC x64 expression substitute for the unspilled sum.
    return coordinate(width + x + 10.0f);
#endif
}

void parameterError(int line, const char* message)
{
    ::renddx9abi::Debug(static_cast<DebugType>(6),
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugBlockGraphD3D.cpp",
        line, "") << message;
}
}

// [INFERRED] Base spelling. Native STL storage replaces VC7.1 container layouts.
class DebugBlockGraphState
{
protected:
    DebugBlockGraphState();
    ~DebugBlockGraphState();
    unsigned char m_enabled; // PE +0x04
    float m_rect[4]; // PE +0x08
    unsigned int* m_colors; // PE +0x18
    unsigned int m_color; // PE +0x1C
    float* m_samples; // PE +0x20
    short m_width, m_layers; // PE +0x24/+0x26
    float m_low, m_high; // PE +0x28/+0x2C
    short m_cursor; // PE +0x30
    std::string m_name; // PE +0x34
    std::vector<std::pair<std::string, unsigned int> > m_legend; // PE +0x50
    int m_legendX, m_legendY; // PE +0x60/+0x64
    unsigned char m_drawLegend; // PE +0x68
};

// PE table 0x1020C978: eighteen slots, no virtual destructor.
// [CHOSEN, not recovered] Native base organization keeps automatic destruction order.
class DebugBlockGraphD3D : public DebugBlockGraphState
{
public:
    DebugBlockGraphD3D();
    virtual int addRef() { return ++m_refCount; } // PE 0x10097380
    virtual int getRef() { return m_refCount; } // PE 0x10178A80
    virtual int release();
    virtual void* queryInterface(unsigned int iid) const;
    virtual bool setRect(const float* rect);
    virtual void setColor(unsigned int color) { m_color = color; } // PE 0x100A2920
    virtual void setName(const std::string& name);
    virtual void setRange(float low, float high) { m_low = low; m_high = high; } // PE 0x100A2900
    virtual void reset();
    virtual void draw();
    virtual void setEnabled(unsigned char value) { m_enabled = value; } // PE 0x10121A00
    virtual unsigned char enabled() const { return m_enabled; } // PE 0x10097320
    virtual void clearLegend();
    virtual void addLegend(const std::string& name, unsigned int color);
    virtual void setLegendPosition(int x, int y);
    virtual void initialize(int width, int layers);
    virtual void addSamples(const float* samples);
    virtual void setColors(const unsigned int* colors);

private:
    ~DebugBlockGraphD3D();
    void releaseResources();
    void initializeHistory(int width, int layers);
    void addSampleRow(const float* samples);
    void fillColumn(int column, BlockVertex* output, int first);
    int m_refCount; // PE +0x6C
    unsigned int m_stride, m_vertexCount, m_bufferBytes, m_bufferPosition; // PE +0x70
    std::string m_shaderName; // PE +0x80
    void* m_buffer; // PE +0x9C
    dx9rend::Effect* m_effect; // PE +0xA0
    dx9rend::ShaderParameter *m_graphSize, *m_viewportSize; // PE +0xA4/+0xA8
};

// PE 0x100A2980
DebugBlockGraphState::DebugBlockGraphState()
    : m_enabled(0), m_color(0), m_width(0), m_layers(0), m_cursor(0),
      m_legendX(0), m_legendY(0), m_drawLegend(1)
{
    std::memset(m_rect, 0, sizeof(m_rect));
    // Shipped defect: the base constructor leaves sample/color pointers and range unwritten.
    // Constructed graphs must be initialized before use or destruction.
}

// PE 0x10096CB0
DebugBlockGraphD3D::DebugBlockGraphD3D()
    : m_refCount(1), m_stride(0), m_vertexCount(0), m_bufferPosition(0),
      m_shaderName("DebugLineGraph"), m_buffer(0), m_effect(0), m_graphSize(0), m_viewportSize(0)
{
}

// PE 0x10096FA0. The resource release runs before the shader-name and base destruction.
DebugBlockGraphD3D::~DebugBlockGraphD3D()
{
    releaseResources();
}

// Releases the vertex buffer, the viewport and graph-size parameters, then the effect. Pointers
// are left as they are.
// PE 0x10096F30. [INFERRED] Name.
void DebugBlockGraphD3D::releaseResources()
{
    if (m_buffer) dx9rend::vertexBufferRelease(m_buffer);
    if (m_viewportSize) m_viewportSize->release();
    if (m_graphSize) m_graphSize->release();
    if (m_effect) dx9rend::effectRelease(m_effect);
}

// PE 0x100A29D0
DebugBlockGraphState::~DebugBlockGraphState()
{
    if (m_colors) ::renddx9abi::engineFree(m_colors);
    if (m_samples) ::renddx9abi::engineFree(m_samples);
    std::vector<std::pair<std::string, unsigned int> >().swap(m_legend);
}

// PE 0x10097390
int DebugBlockGraphD3D::release()
{
    const int count = --m_refCount;
    if (!count)
    {
        this->~DebugBlockGraphD3D();
        ::renddx9abi::engineFree(this);
    }
    return count;
}

// PE 0x10097350
void* DebugBlockGraphD3D::queryInterface(unsigned int iid) const
{
    return iid == 1 || iid == 80039 || iid == 80042 ? const_cast<DebugBlockGraphD3D*>(this) : 0;
}

// PE 0x100A2950
bool DebugBlockGraphD3D::setRect(const float* rect)
{
    m_rect[0] = rect[0]; m_rect[1] = rect[1]; m_rect[2] = rect[2]; m_rect[3] = rect[3];
    return true;
}

// PE 0x10097310
void DebugBlockGraphD3D::setName(const std::string& name) { m_name = name; }

// PE 0x10097330
void DebugBlockGraphD3D::setLegendPosition(int x, int y) { m_legendX = x; m_legendY = y; }

// PE 0x100973C0
void DebugBlockGraphD3D::clearLegend()
{
    std::vector<std::pair<std::string, unsigned int> >().swap(m_legend);
}

// PE 0x100973D0
void DebugBlockGraphD3D::addLegend(const std::string& name, unsigned int color)
{
    const std::pair<std::string, unsigned int> entry(name, color);
    m_legend.push_back(entry);
}

// PE 0x100A2930
void DebugBlockGraphD3D::reset()
{
    // Shipped defect: only width samples are cleared, rather than width * layers.
    std::memset(m_samples, 0, static_cast<unsigned int>(static_cast<int>(m_width)) * 4u);
}

// PE 0x100A27F0
void DebugBlockGraphD3D::initializeHistory(int width, int layers)
{
    m_width = static_cast<short>(width);
    m_layers = static_cast<short>(layers);
    m_cursor = 0;
    m_color = 0xFF000000u;
    std::memset(m_rect, 0, sizeof(m_rect));
    m_enabled = 1;
    // Shipped defect: reinitialization leaks both previous allocations.
    m_samples = static_cast<float*>(::renddx9abi::engineAlloc(
        static_cast<unsigned int>(static_cast<int>(m_width)) * static_cast<unsigned int>(layers) * 4u));
    m_colors = static_cast<unsigned int*>(::renddx9abi::engineAlloc(static_cast<unsigned int>(layers) * 4u));
    reset();
    m_low = m_high = 0.0f;
}

// PE 0x10096D00
void DebugBlockGraphD3D::initialize(int width, int layers)
{
    using namespace dx9rend;
    initializeHistory(width, layers);
    m_vertexCount = static_cast<unsigned int>(width) * static_cast<unsigned int>(layers) * 2u + 6u;
    m_stride = 12;
    const int previousPool = bufferManagerSwapPool(0);
    m_bufferBytes = m_vertexCount * m_stride;
    m_buffer = bufferManagerCreateVertexBuffer(m_vertexCount, m_stride, 0x208, "DebugBlockGraphD3D");
    bufferManagerSwapPool(previousPool);
    {
        const std::string prefix("Shaders/");
        const std::string shader = prefix + m_shaderName;
        m_effect = static_cast<Effect*>(shaderManagerGetEffect(g_shaderManager, shader.c_str()));
    }
    m_graphSize = effectGetShaderParameterBorrowedTyped(m_effect, std::string("GRAPHSIZE"));
    if (m_graphSize) m_graphSize->addRef();
    else parameterError(79, "Could not load m_paramGraphSize parameter from shader");
    m_viewportSize = effectGetShaderParameterBorrowedTyped(m_effect, std::string("VIEWPORTSIZE"));
    if (m_viewportSize) m_viewportSize->addRef();
    else parameterError(87, "Could not load m_paramViewPort parameter from shader");
}

// PE 0x10096B90. Forwards to the shared history body.
void DebugBlockGraphD3D::addSamples(const float* samples)
{
    addSampleRow(samples);
}

// Copies one sample per layer at the cursor, then advances the cursor by the layer count (16-bit
// add) modulo width * layers.
// PE 0x100A2870. [INFERRED] Name.
void DebugBlockGraphD3D::addSampleRow(const float* samples)
{
    for (int i = 0; i < m_layers; ++i)
        m_samples[static_cast<int>(m_cursor) + i] = samples[i];
    const int next = static_cast<short>(static_cast<int>(m_cursor) + m_layers);
    m_cursor = static_cast<short>(next % (static_cast<int>(m_width) * m_layers));
}

// PE 0x100A28D0
void DebugBlockGraphD3D::setColors(const unsigned int* colors)
{
    std::memcpy(m_colors, colors, static_cast<unsigned int>(static_cast<int>(m_layers)) * 4u);
}

// PE 0x10096BA0
void DebugBlockGraphD3D::fillColumn(int column, BlockVertex* output, int first)
{
    const int sample = ((m_cursor / m_layers + m_width + column - 1) % m_width) * m_layers;
#if !defined(_MSC_VER)
    using namespace bf2x87;
    const float scale = store(div(wide(m_rect[3]), sub(wide(m_high), wide(m_low))));
    Real sum = wide(0);
    const auto level = [&](Real value) -> float {
        if (less(value, wide(m_low))) return m_rect[3];
        if (greater(value, wide(m_high))) return 0.0f;
        return store(sub(wide(m_rect[3]), mul(sub(value, wide(m_low)), wide(scale))));
    };
#else
    const float scale = m_rect[3] / (m_high - m_low);
    float sum = 0.0f;
    const auto level = [&](float value) -> float {
        if (value < m_low) return m_rect[3];
        if (value > m_high) return 0.0f;
        return m_rect[3] - (value - m_low) * scale;
    };
#endif
    for (int i = 0; i < m_layers; ++i)
    {
        output[first++] = BlockVertex{static_cast<float>(column), level(sum), m_colors[i]};
#if !defined(_MSC_VER)
        sum = add(sum, wide(m_samples[sample + i]));
#else
        sum += m_samples[sample + i];
#endif
        output[first++] = BlockVertex{static_cast<float>(column), level(sum), m_colors[i]};
    }
}

// PE 0x10096FD0
void DebugBlockGraphD3D::draw()
{
    using namespace dx9rend;
    const unsigned int sampleBytes = m_stride * static_cast<unsigned int>(static_cast<int>(m_width));
    if (m_bufferPosition > m_bufferBytes - sampleBytes) m_bufferPosition = 0;
    BlockVertex* const output = static_cast<BlockVertex*>(vertexBufferLock(m_buffer, 0,
        static_cast<unsigned int>(static_cast<int>(m_width) * m_layers) * 2u + 5u, 0x2000));
    if (!output) return;
    int count = 0;
    for (int x = 0; x < m_width; ++x)
    {
        fillColumn(x, output, count);
        count += m_layers * 2;
    }
    const int border = count;
    output[count++] = BlockVertex{0, 0, 0xFFFFFFFFu};
    output[count++] = BlockVertex{m_rect[2], 0, 0xFFFFFFFFu};
    output[count++] = BlockVertex{m_rect[2], m_rect[3], 0xFFFFFFFFu};
    output[count++] = BlockVertex{0, m_rect[3], 0xFFFFFFFFu};
    output[count++] = BlockVertex{0, 0, 0xFFFFFFFFu};
    vertexBufferUnlock(m_buffer);
    effectSetTechniqueByName(m_effect, std::string("Graph"));
    IRenderView* const view = rendererGetRenderViewDup();
    const float graphSize[4] = {m_rect[0], m_rect[1], 0, 0};
    m_graphSize->setValue(graphSize, 0);
    const float* const firstViewport = view->getViewport();
    const float width = view->getViewport()[2];
    const float viewportSize[4] = {width, firstViewport[3], 0, 0};
    m_viewportSize->setValue(viewportSize, 0);
    vertexBufferBindStream(m_buffer, 0);
    effectBegin(m_effect);
    effectBeginPass(m_effect, 0);
    rendererDrawPrimitive(static_cast<unsigned int>((m_width - 1) * m_layers), 2, 0);
    rendererDrawPrimitive(4, 3, static_cast<unsigned int>(border));
    effectEnd(m_effect);
    const unsigned int previousColor = debugTextWriterGetColour(g_DebugTextWriter);
    debugTextWriterSetColour(g_DebugTextWriter, m_color);
    const int y = coordinate(m_rect[1]) - 10;
    const int x = coordinate(m_rect[0]) + 1;
    debugTextWriterQueueText(g_DebugTextWriter, x, y, m_name, 0);
    if (m_drawLegend)
    {
        const int legendX = rightCoordinate(m_rect[0], m_rect[2]) + m_legendX;
        int legendY = coordinate(m_rect[1]) + m_legendY;
        for (unsigned int i = 0; i < m_legend.size(); ++i)
        {
            debugTextWriterSetColour(g_DebugTextWriter, m_legend[i].second);
            debugTextWriterQueueText(g_DebugTextWriter, legendX, legendY, m_legend[i].first, 0);
            legendY += 10;
        }
    }
    debugTextWriterSetColour(g_DebugTextWriter, previousColor);
    m_bufferPosition += sampleBytes;
}

// PE 0x1007CC70
IBase* __fastcall createDebugBlockGraph(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* const storage = ::renddx9abi::engineAlloc(sizeof(DebugBlockGraphD3D));
    DebugBlockGraphD3D* const self = storage ? new (storage) DebugBlockGraphD3D : 0;
    void* const result = self->queryInterface(iid);
    if (!result) self->release();
    return static_cast<IBase*>(result);
}

// The line-graph slots the manager calls, in table order (DebugLineGraphD3D 0x1020C7E8,
// DebugBlockGraphD3D 0x1020C978); BFGraphCreator.cpp declares the same table for the host side.
// [INFERRED] slot names; only draw (+0x24) and the enabled byte (+0x2C) are called here.
class IManagedDebugGraph : public IBase
{
public:
    virtual char setRect(const float* rect) = 0;                   // +0x10
    virtual void setColour(unsigned int colour) = 0;               // +0x14
    virtual void setName(const std::string& name) = 0;             // +0x18
    virtual void setBounds(float minimum, float maximum) = 0;      // +0x1C
    virtual void resetExtrema() = 0;                               // +0x20
    virtual void draw() = 0;                                       // +0x24
    virtual char setEnabledByte(char value) = 0;                   // +0x28
    virtual unsigned char enabledByte() = 0;                       // +0x2C

protected:
    ~IManagedDebugGraph() {}
};

// The line-graph manager, registered as dice.hfe.geom.DebugGraphRenderer (class 81039, interface
// 80040) and published as the "DebugLineGraphRenderer" singleton. Graph owners (Renderer::initialize,
// DebugCounterManager, the host's NetworkStats and BFGraph) add and remove their graphs; the
// manager holds plain pointers and takes no reference. [INFERRED] class name; placement [INFERRED]
// (address-adjacent to DebugBlockGraphD3D). PE table 0x1020CA48: seven slots, no virtual destructor.
class DebugGraphRenderer
{
public:
    DebugGraphRenderer();
    int   addRef() { return ++m_refCount; }        // PE 0x1000CB20 (shared body)
    int   getRef() const { return m_refCount; }    // PE 0x101A9A10 (shared body)
    int   release();
    void* queryInterface(unsigned int iid) const;
    void  addGraph(IBase* graph);
    void  removeGraph(IBase* graph);
    void  drawGraphs();

private:
    ~DebugGraphRenderer();

public:
    const void**       m_vptr;       // +0x00
    int                m_refCount;   // +0x04
    std::list<IBase*>  m_graphs;     // +0x08 (VC7.1 list: allocator, head node, size)
};

const void** debugGraphRendererVtable();

// PE 0x10097590: vptr, refcount 1, the empty list.
DebugGraphRenderer::DebugGraphRenderer() : m_vptr(debugGraphRendererVtable()), m_refCount(1) {}

// PE 0x100975C0: vptr restored, then the list destroyed.
DebugGraphRenderer::~DebugGraphRenderer() { m_vptr = debugGraphRendererVtable(); }

// PE 0x10097640: the decremented count is kept, the object destroyed and freed at zero.
int DebugGraphRenderer::release()
{
    const int count = --m_refCount;
    if (!count)
    {
        this->~DebugGraphRenderer();
        ::renddx9abi::engineFree(this);
    }
    return count;
}

// PE 0x10097610: IID_IBase (1) or IID_IDebugGraphRenderer (80040), the raw this.
void* DebugGraphRenderer::queryInterface(unsigned int iid) const
{
    return iid == 1u || iid == 80040u ? const_cast<DebugGraphRenderer*>(this) : 0;
}

// PE 0x100975D0: a null graph is ignored; otherwise push_back, duplicates allowed.
void DebugGraphRenderer::addGraph(IBase* graph)
{
    if (graph)
        m_graphs.push_back(graph);
}

// PE 0x10097570: a null graph is ignored; otherwise every entry equal to it is removed.
void DebugGraphRenderer::removeGraph(IBase* graph)
{
    if (graph)
        m_graphs.remove(graph);
}

// PE 0x10097530: inside a StateManager push/pop, each graph whose slot +0x2C (enabled) answers
// true is drawn through slot +0x24. The list holds borrowed pointers; a graph freed without
// removeGraph stays listed, as shipped (BFGraph's destructor does that).
void DebugGraphRenderer::drawGraphs()
{
    dx9rend::stateManagerPush();
    for (std::list<IBase*>::iterator it = m_graphs.begin(); it != m_graphs.end(); ++it)
    {
        IManagedDebugGraph* const graph = static_cast<IManagedDebugGraph*>(*it);
        if (graph->enabledByte())
            graph->draw();
    }
    dx9rend::stateManagerPop();
}

namespace {
int   RD9_THUNK dgr_addRef(void* s RD9_THUNK_EDX) { return static_cast<DebugGraphRenderer*>(s)->addRef(); }
int   RD9_THUNK dgr_getRef(void* s RD9_THUNK_EDX) { return static_cast<DebugGraphRenderer*>(s)->getRef(); }
int   RD9_THUNK dgr_release(void* s RD9_THUNK_EDX) { return static_cast<DebugGraphRenderer*>(s)->release(); }
void* RD9_THUNK dgr_queryInterface(void* s RD9_THUNK_EDX, unsigned int iid)
{
    return static_cast<DebugGraphRenderer*>(s)->queryInterface(iid);
}
void  RD9_THUNK dgr_addGraph(void* s RD9_THUNK_EDX, IBase* graph)
{
    static_cast<DebugGraphRenderer*>(s)->addGraph(graph);
}
void  RD9_THUNK dgr_removeGraph(void* s RD9_THUNK_EDX, IBase* graph)
{
    static_cast<DebugGraphRenderer*>(s)->removeGraph(graph);
}
void  RD9_THUNK dgr_drawGraphs(void* s RD9_THUNK_EDX)
{
    static_cast<DebugGraphRenderer*>(s)->drawGraphs();
}

const void* const kDebugGraphRendererVtable[7] =
{
    reinterpret_cast<const void*>(&dgr_addRef),           // +0x00
    reinterpret_cast<const void*>(&dgr_getRef),           // +0x04
    reinterpret_cast<const void*>(&dgr_release),          // +0x08
    reinterpret_cast<const void*>(&dgr_queryInterface),   // +0x0C
    reinterpret_cast<const void*>(&dgr_addGraph),         // +0x10
    reinterpret_cast<const void*>(&dgr_removeGraph),      // +0x14
    reinterpret_cast<const void*>(&dgr_drawGraphs),       // +0x18
};
} // namespace

const void** debugGraphRendererVtable()
{
    return const_cast<const void**>(kDebugGraphRendererVtable);
}

// PE 0x1007CCC0: refuses an outer object, allocates and constructs, answers the requested
// interface and releases the object when there is none. The allocation is used unchecked, as
// the sibling factories are.
IBase* __fastcall createDebugGraphRenderer(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* const storage = ::renddx9abi::engineAlloc(sizeof(DebugGraphRenderer));
    DebugGraphRenderer* const self = storage ? new (storage) DebugGraphRenderer : 0;
    void* const result = self->queryInterface(iid);
    if (!result) self->release();
    return static_cast<IBase*>(result);
}

} } }
#endif
