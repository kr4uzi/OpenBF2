// [INFERRED] Source placement. One TU with BFGraph: bf2 (ia-32) lays BFGraph (0x842E050..0x842E3F0)
// directly before BFGraphCreator (0x842E420..0x842E610) under one static initialiser
// (_GLOBAL__I__ZN4dice3hfe2ai7BFGraphC2Ev, 0x842E690).

#include "BFGraphCreator.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../AIDLL/AI/AIUtils/DebugRendering/IBFGraph.h"
#include "../../AIDLL/AI/AIUtils/checkPointer.h"
#include "../../../Core/ClassManager.h"
#include "../../../Core/Ids.h"            // geom::CID_DebugLineGraphD3D, geom::IID_IDebugGraph

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// geom::IDebugGraph (IID 80039) as BFGraph calls it: the renderer's line graph (geom::DebugLineGraphD3D,
// GeomMisc/DebugLineGraphD3d.cpp, RendDX9 image, PE table 0x1020C7E8). The engine images share no
// header for it, so the slots BFGraph and Core/DebugCounterManager.cpp use are declared here, in
// table order; parameter types are the ones bf2 (ia-32) and BF2.exe pass. [INFERRED] slot names
// (GeomMisc's), [UNRESOLVED] return types where no caller reads them.
class IDebugGraph : public IBase
{
public:
    virtual char setRect(const float* rect) = 0;                                  // +0x10
    virtual void setColour(unsigned int colour) = 0;                              // +0x14
    virtual void setName(const std::string& name) = 0;                            // +0x18
    virtual void setBounds(float minimum, float maximum) = 0;                     // +0x1C
    virtual void resetExtrema() = 0;                                              // +0x20
    virtual void draw() = 0;                                                      // +0x24
    virtual char setFlag05(char value) = 0;                                       // +0x28
    virtual unsigned char getFlag05() = 0;                                        // +0x2C
    virtual void clearLegend() = 0;                                               // +0x30
    virtual void addLegend(const std::string& name, unsigned int colour) = 0;     // +0x34
    virtual void setLegendPosition(int x, int y) = 0;                             // +0x38
    virtual void setHistorySize(int samples) = 0;                                 // +0x3C
    virtual void addSample(float sample) = 0;                                     // +0x40
    virtual void setColors(unsigned int a, unsigned int b, unsigned int c) = 0;   // +0x44
    virtual void setAutoScale(unsigned char value) = 0;                           // +0x48
    virtual char setRange(float low, float high) = 0;                             // +0x4C
    virtual char setEnabled(char value) = 0;                                      // +0x50
    virtual bool setByte41(unsigned char value) = 0;                              // +0x54
    virtual void setDrawValue(unsigned char value) = 0;                           // +0x58
    virtual unsigned char getDrawValue() = 0;                                     // +0x5C
    virtual void setValuePosition(int x, int y) = 0;                              // +0x60
    virtual void setValueSuffix(const std::string& suffix) = 0;                   // +0x64

protected:
    ~IDebugGraph() {}
};

// The "DebugLineGraphRenderer" singleton (IID_IDebugGraphRenderer 80040): BFGraph uses +0x10 only.
// [INFERRED] name; Core/DebugCounterManager.cpp calls the same slot to add a graph.
class IDebugGraphRenderer : public IBase
{
public:
    virtual void addGraph(IDebugGraph* graph) = 0;                                // +0x10

protected:
    ~IDebugGraphRenderer() {}
};

// The AI's debug graph: an IBFGraph over one renderer line graph. sizeof 8 (`operator new(8)` in
// createGraph): the vptr and the line graph.
class BFGraph : public IBFGraph
{
public:
    BFGraph();
    virtual ~BFGraph();

    virtual void setNumberOfSamples(int samples);
    virtual void setDimensions(float x, float y, float width, float height);
    virtual void setColours(unsigned int first, unsigned int second, unsigned int third);
    virtual void setThresholds(float low, float high);
    virtual void setMinMaxY(float minimum, float maximum);
    virtual void setCurrentSampleEnding(const std::string& ending);
    virtual void setCurrentSampleOffset(int x, int y);
    virtual void addSample(float sample);

private:
    IDebugGraph* m_graph;               // +0x04  a geom::DebugLineGraphD3D (IID_IDebugGraph)
};

// ELF 0x842E110 (C1; C2 0x842E050), PE 0x815AE0. ClassManager +0x34 createInstance(
// CID_DebugLineGraphD3D, IID_IDebugGraph, 0), no null check; the graph's +0x58 (1); then the
// "DebugLineGraphRenderer" singleton, when there is one, takes the graph (+0x10).
BFGraph::BFGraph()
{
    // createInstance answers the IID_IDebugGraph interface pointer.
    m_graph = static_cast<IDebugGraph*>(
        classManager->createInstance(geom::CID_DebugLineGraphD3D, geom::IID_IDebugGraph, 0));
    m_graph->setDrawValue(1);
    IDebugGraphRenderer* const renderer = static_cast<IDebugGraphRenderer*>(
        classManager->getSingleton(std::string("DebugLineGraphRenderer")));
    if (renderer)
        renderer->addGraph(m_graph);
}

// ELF 0x842E200 (D1; D2 0x842E1D0, D0 0x842E230), PE 0x815B60. The line graph is freed with plain
// operator delete - not released, and not removed from the graph renderer - in both images.
BFGraph::~BFGraph()
{
    operator delete(m_graph);
}

// ELF 0x842E270, PE 0x8159D0: the graph's +0x3C.
void BFGraph::setNumberOfSamples(int samples)
{
    m_graph->setHistorySize(samples);
}

// ELF 0x842E2A0, PE 0x815BA0: the four floats as one rectangle to the graph's +0x10.
void BFGraph::setDimensions(float x, float y, float width, float height)
{
    const float rect[4] = { x, y, width, height };
    m_graph->setRect(rect);
}

// ELF 0x842E2E0, PE 0x8159E0: the graph's +0x44.
void BFGraph::setColours(unsigned int first, unsigned int second, unsigned int third)
{
    m_graph->setColors(first, second, third);
}

// ELF 0x842E310, PE 0x8159F0: the graph's +0x4C (low, high), then +0x50 (1).
void BFGraph::setThresholds(float low, float high)
{
    m_graph->setRange(low, high);
    m_graph->setEnabled(1);
}

// ELF 0x842E360, PE 0x826460 (folded): the graph's +0x1C.
void BFGraph::setMinMaxY(float minimum, float maximum)
{
    m_graph->setBounds(minimum, maximum);
}

// ELF 0x842E390, PE 0x826180 (folded): the graph's +0x64.
void BFGraph::setCurrentSampleEnding(const std::string& ending)
{
    m_graph->setValueSuffix(ending);
}

// ELF 0x842E3C0, PE 0x815A20: the graph's +0x60.
void BFGraph::setCurrentSampleOffset(int x, int y)
{
    m_graph->setValuePosition(x, y);
}

// ELF 0x842E3F0, PE 0x815A30: the graph's +0x40.
void BFGraph::addSample(float sample)
{
    m_graph->addSample(sample);
}

// First wins in, exact match out.
IBFGraphCreator::IBFGraphCreator()
{
    if (!CommonSingletonServer::m_iBFGraphCreator)
        classManager->registerSimpleSingleton(std::string("IBFGraphCreator"), this);
}

IBFGraphCreator::~IBFGraphCreator()
{
    if (CommonSingletonServer::m_iBFGraphCreator == this)
        classManager->unregisterSimpleSingleton(std::string("IBFGraphCreator"));
}

BFGraphCreator::BFGraphCreator()
{
}

BFGraphCreator::~BFGraphCreator()
{
}

// ELF 0x842E4F0, PE 0x815C00: a new BFGraph through checkPointer.
IBFGraph* BFGraphCreator::createGraph()
{
    return checkPointer(new BFGraph);
}

void BFGraphCreator::destroyGraph(IBFGraph* graph)
{
    // ELF 0x842E610: `if (graph) graph->[vtable +0x04]` - Itanium's deleting destructor, which is what
    // `delete` compiles to. The null guard is dice's and is kept.
    if (graph)
        delete graph;
}

} // namespace ai
} // namespace hfe
} // namespace dice
