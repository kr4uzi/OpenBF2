// Quad-tree object culler. [INFERRED] Source placement beside ObjectManager and BinTreeCuller.

#ifndef SCENE_OBJECT_QUADTREECULLER_H
#define SCENE_OBJECT_QUADTREECULLER_H

#include <list>
#include <vector>

#include "ICuller.h"
#include "../../Math/Frustum.h"   // Plane3 - the frustum query keeps a reduced plane set

namespace dice {
namespace hfe {
namespace world {

class QuadTreeCuller : public ICuller
{
public:
    QuadTreeCuller();
    static IBase* create(unsigned int iid, IBase* outer);

    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;

    virtual unsigned int getClassID() const;
    virtual CullerObject* createObject(const Vec3& position, float radius, IObject* object);
    virtual int getNumberOfObjects();
    virtual void initQuadTree(const Vec3& centre, float childOffset,
                              float minimumWidth, float width);
    virtual void destroyObject(CullerObject* object);
    virtual CullerObject* updateObject(CullerObject* object,
                                       const Vec3& position, float radius);
    virtual void optimize();
    virtual void clear();
    virtual void getAllObjects(std::vector<IObject*>& out);

    virtual void getObjects(const Vec3& from, const Vec3& to,
                            std::vector<std::pair<float, IObject*> >& out);
    virtual void getObjects(const Vec3& centre, float radius,
                            std::vector<IObject*>& out, ObjectPredicator* predicator);
    virtual void getObjects(const Vec3& minimum, const Vec3& maximum,
                            std::vector<IObject*>& out, ObjectPredicator* predicator);
    virtual void getObjects(const Frustum& frustum, std::vector<IObject*>& visible,
                            std::vector<IObject*>& allVisible, ObjectPredicator* predicator);
    virtual void clearGetObjects();
    virtual void appendGetObjects(const Vec3& centre, float radius,
                                  std::vector<IObject*>& out,
                                  ObjectPredicator* predicator);
    virtual ~QuadTreeCuller();

private:
    struct ObjectHolder;

    struct Node
    {
        Node();

        unsigned int count;                     // +0x00, placements in this subtree
        Node* parent;                            // +0x04
        Node* children[4];                       // +0x08..+0x14
        Vec3 position;                           // +0x18
        float radius;                            // +0x24 (cell width in the shipped source)
        std::list<ObjectHolder*> objects;        // +0x28 in the retail ELF layout
    };

    struct NodeObject
    {
        Node* node;
        std::list<ObjectHolder*>::iterator iterator;
    };

    struct ObjectHolder : public CullerObject
    {
        ObjectHolder(QuadTreeCuller* owner, const Vec3& p, float r, IObject* o);

        Vec3 position;                           // +0x08
        IObject* object;                         // +0x14
        float radius;                            // +0x18
        // Keep generation before the node vector: original offsets are +0x1C and +0x20.
        // Each x86 NodeObject pairs a node pointer with a list iterator.
        unsigned int generation;                 // +0x1C
        std::vector<NodeObject> nodes;           // +0x20
    };

    void initNode(Node* node, const Vec3& centre, float childOffset,
                  float minimumWidth, float width);
    unsigned int insertObject(Node* node);
    void freeNode(Node* node);
    void getPosAndRadiusObjects(Node* node);
    void getBoxObjects(Node* node);
    void getDirectionObjects(Node* node);
    // The frustum walk - all names attested, all four .
    // The third one the old note called "still unplaced" is at : it is the
    // predicator-free twin of getAllObjects(Node*), reached only from getFrustumVisibleObjects.
    void getFrustumObjects(Node* node);
    void getFrustumVisibleObjects(Node* node);
    void getFrustumAllVisibleObjects(Node* node);
    void getAllObjects(Node* node);                   // the Node* overload

    // [CHOSEN name] The placement rule the two wholly-inside collectors share.  It is inlined in
    // both of them in the shipped image; factoring it changes no behaviour and keeps the one
    // genuinely subtle decision - single-node versus multi-node placement - in one place.
    void collectWhollyInside(ObjectHolder* holder, IObject* object);

    int m_refCount;
    int m_numberOfObjects;
    bool m_initialized;

    // Retail uses member traversal scratch rather than passing it through recursive calls.
    ObjectHolder* m_insertingObject;
    const Vec3* m_queryOrigin;
    float m_queryRadius;
    std::vector<IObject*>* m_queryOut;
    ObjectPredicator* m_queryPredicator;
    unsigned int m_queryGeneration;
    Node* m_root;

    // Frustum-query scratch in a compact native layout. Shares m_queryOut with other queries;
    // some original scratch aliases are represented by separate fields. Six reduced planes
    // fit the original plane-array extent.
    std::vector<IObject*>* m_queryAllVisible;   // retail +0x24
    const Frustum*         m_queryFrustum;      // retail +0x2C - a pointer; unlike the BinTree
                                                //   culler, this one does not copy the frustum
    Plane3                 m_queryPlanes[6];    // retail +0x30 - the reduced silhouette set
    int                    m_queryPlaneCount;   // retail +0x90

    // [INFERRED] Instance scratch for direction queries, shared by recursive traversal.
    // PE/ELF +0x94/+0xA0: box limits share the direction-query endpoint scratch.
    union
    {
        Vec3 m_directionStart;
        Vec3 m_boxMinimum;
    };
    union
    {
        Vec3 m_directionEnd;
        Vec3 m_boxMaximum;
    };
    Vec3 m_directionNormalized;
    Vec3 m_horizontalDirectionNormalized;
    Vec3 m_horizontalCentre;
    float m_horizontalHalfLength;
    std::vector<std::pair<float, IObject*> >* m_directionOut;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // SCENE_OBJECT_QUADTREECULLER_H
