// Binary-tree object culler. [INFERRED] Source placement beside ObjectManager.

#ifndef SCENE_OBJECT_BINTREECULLER_H
#define SCENE_OBJECT_BINTREECULLER_H

#include "ICuller.h"
#include "../../Math/Frustum.h"   // the frustum query copies its argument into the instance

namespace dice {
namespace hfe {
namespace world {

class BinTreeCuller : public ICuller
{
public:
    BinTreeCuller();
    static IBase* create(unsigned int iid, IBase* outer);

    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;

    // Shipped copy/paste defect: both culler implementations report CID_QuadTreeCuller.
    virtual unsigned int getClassID() const;
    virtual CullerObject* createObject(const Vec3& position, float radius, IObject* object);
    // getNumberOfObjects intentionally inherited from ICuller (always zero in the BinTree table).
    virtual void initQuadTree(const Vec3&, float, float, float);
    virtual void destroyObject(CullerObject* object);
    virtual CullerObject* updateObject(CullerObject* object, const Vec3& position, float radius);
    virtual void optimize();
    virtual void clear();
    virtual void getAllObjects(std::vector<IObject*>& out);

#if defined(_WIN32)
    virtual void getObjects(const Frustum& frustum, std::vector<IObject*>& visible,
                            std::vector<IObject*>& allVisible, ObjectPredicator* predicator);
    virtual void getObjects(const Vec3& minimum, const Vec3& maximum,
                            std::vector<IObject*>& out, ObjectPredicator* predicator);
    virtual void getObjects(const Vec3& centre, float radius,
                            std::vector<IObject*>& out, ObjectPredicator* predicator);
    virtual void getObjects(const Vec3& from, const Vec3& to,
                            std::vector<std::pair<float, IObject*> >& out);
#else
    virtual void getObjects(const Vec3& from, const Vec3& to,
                            std::vector<std::pair<float, IObject*> >& out);
    virtual void getObjects(const Vec3& centre, float radius,
                            std::vector<IObject*>& out, ObjectPredicator* predicator);
    virtual void getObjects(const Vec3& minimum, const Vec3& maximum,
                            std::vector<IObject*>& out, ObjectPredicator* predicator);
    virtual void getObjects(const Frustum& frustum, std::vector<IObject*>& visible,
                            std::vector<IObject*>& allVisible, ObjectPredicator* predicator);
#endif

    virtual void clearGetObjects();
    virtual void appendGetObjects(const Vec3& centre, float radius,
                                  std::vector<IObject*>& out,
                                  ObjectPredicator* predicator);
    virtual ~BinTreeCuller();

private:
    // Exact 0x28-byte x86 layout.  Internal nodes reuse the leaf object word for two 16-bit
    // balancing metrics, exactly as calcWeightsAndMaxDepths does in the retail implementation.
    struct Node : public CullerObject
    {
        explicit Node(BinTreeCuller* owner);

        bool isInternal() const { return left != 0; }
        bool updateNodePosAndRadius();
        void propagateWeightAndMaxDepth();

        Node* left;                         // +0x08
        Node* right;                        // +0x0C
        Node* parent;                       // +0x10
        Vec3  position;                     // +0x14
        float radius;                       // +0x20
        union
        {
            IObject* object;                // leaf +0x24
            struct
            {
                unsigned short weight;      // internal +0x24
                unsigned short maxDepth;    // internal +0x26
            } metrics;
        } payload;
    };

    void insertNode(Node* start, Node* node);
    void insertNode(Node* node);
    void removeNode(Node* node);
    void updateNodePosAndRadius(Node* node);
    void clear(Node* node);
    void getAllObjects(Node* node);
    void getObjectsByOriginAndRadius(Node* node);
    void getObjectsInBoxByStartAndEndPos(Node* node);
    void getDirectionObjects(Node* node);
    // The frustum walk. Names attested from the symbolised ELF's own symbol table -
    // dice's own `NonPred` suffix confirms the predicator really is specialised out of the hot
    // path rather than tested per node.
    void getObjectsByFrustum(Node* node);
    void getObjectsByFrustumNonPred(Node* node);
    void getAllObjectsNonPred(Node* node);
    // getAllObjects(Node*) above is ELF 0x8304CF0 - the wholly-inside collector this query uses.
    void calcWeightsAndMaxDepths(Node* node);
    void balanceTree(Node* node, int depth);

    static unsigned short nodeWeight(const Node* node);
    static unsigned short nodeMaxDepth(const Node* node);

    int   m_refCount;
    Node* m_root;
    bool  m_locked;

    // Retail stores traversal state in the culler object as well.  Names are inferred; the
    // set-before-walk and recursive-use sequence is exact.
    std::vector<IObject*>* m_queryOut;          // PE/ELF +0x238, all object-vector queries
    ObjectPredicator*      m_queryPredicator;
    // PE/ELF +0x10: sphere/frustum centre and line midpoint share storage.
    union
    {
        Vec3 m_queryOrigin;
        Vec3 m_directionMidpoint;
    };
    float                  m_queryRadius;     // PE/ELF +0x4C, also the line half-length

    // Instance scratch for frustum traversal; recursive walkers only take Node pointers.
    // This compact native layout does not reproduce original embedded-frustum offsets.
    Frustum                m_queryFrustum;      // retail +0x50 - a copy, not the caller's object
    std::vector<IObject*>* m_queryAllVisible;   // retail +0x23C - objects straddling the boundary
    // [INFERRED] Instance scratch for direction traversal. Queries share the existing output
    // pointer, so the recursive walker only needs a Node argument and is not reentrant.
    // PE/ELF +0x1C: line start and box minimum share storage.
    union
    {
        Vec3 m_directionStart;
        Vec3 m_boxMinimum;
    };
    // PE/ELF +0x28: line end and box maximum share storage.
    union
    {
        Vec3 m_directionEnd;
        Vec3 m_boxMaximum;
    };
    Vec3 m_direction;
    Vec3 m_directionOverLengthSquared;
    std::vector<std::pair<float, IObject*> >* m_directionOut;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // SCENE_OBJECT_BINTREECULLER_H
