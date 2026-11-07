// Path: [INFERRED] - see BinTreeCuller.h.

#include "BinTreeCuller.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "../../Core/Ids.h"
#include "../../Game/Objects/IObject.h"
#include "ObjectPredicator.h"

namespace dice {
namespace hfe {
namespace world {

namespace {

inline float distanceSquared(const Vec3& a, const Vec3& b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return x * x + y * y + z * z;
}

} // anonymous namespace

BinTreeCuller::Node::Node(BinTreeCuller* owner)
    : CullerObject(owner)
    , left(0)
    , right(0)
    , parent(0)
    , radius(0.0f)
{
    position.x = position.y = position.z = 0.0f;
    payload.object = 0;
}

BinTreeCuller::BinTreeCuller()
    : m_refCount(1)
    , m_root(0)
    , m_locked(false)
    , m_queryOut(0)
    , m_queryPredicator(0)
    , m_queryRadius(0.0f)
    , m_queryAllVisible(0)
    , m_directionOut(0)
{
    m_queryOrigin.x = m_queryOrigin.y = m_queryOrigin.z = 0.0f;
    m_directionMidpoint.x = m_directionMidpoint.y = m_directionMidpoint.z = 0.0f;
    m_directionStart.x = m_directionStart.y = m_directionStart.z = 0.0f;
    m_directionEnd.x = m_directionEnd.y = m_directionEnd.z = 0.0f;
    m_direction.x = m_direction.y = m_direction.z = 0.0f;
    m_directionOverLengthSquared.x = 0.0f;
    m_directionOverLengthSquared.y = 0.0f;
    m_directionOverLengthSquared.z = 0.0f;
}

BinTreeCuller::~BinTreeCuller()
{
    clear();
}

IBase* BinTreeCuller::create(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    BinTreeCuller* self = new BinTreeCuller;
    IBase* result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result)
        self->release();
    return result;
}

void BinTreeCuller::addRef() { ++m_refCount; }
int BinTreeCuller::getRef() { return m_refCount; }

int BinTreeCuller::release()
{
    const int result = --m_refCount;
    if (result == 0 && this)
        delete this;
    return result;
}

void* BinTreeCuller::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_ICuller)
        return const_cast<BinTreeCuller*>(this);
    return 0;
}

unsigned int BinTreeCuller::getClassID() const
{
    return CID_QuadTreeCuller; // Shipped defect, verified in both implementations.
}

CullerObject* BinTreeCuller::createObject(const Vec3& position, float radius, IObject* object)
{
    if (m_locked)
        return 0;

    Node* node = new Node(this);
    node->position = position;
    node->radius = radius;
    node->payload.object = object;
    insertNode(node);
    return node->getCullerObject();
}

void BinTreeCuller::destroyObject(CullerObject* object)
{
    if (m_locked)
        return;
    Node* node = static_cast<Node*>(object);
    removeNode(node);
    delete node;
}

CullerObject* BinTreeCuller::updateObject(CullerObject* object,
                                           const Vec3& position,
                                           float radius)
{
    if (m_locked)
        return 0;
    Node* node = static_cast<Node*>(object);
    removeNode(node);
    node->position = position;
    node->radius = radius;
    insertNode(node);
    return node->getCullerObject();
}

void BinTreeCuller::initQuadTree(const Vec3&, float, float, float)
{
    // genuinely empty on BinTreeCuller.
}

void BinTreeCuller::clearGetObjects()
{
    // genuinely empty on BinTreeCuller.
}

void BinTreeCuller::insertNode(Node* start, Node* node)
{
    if (start->isInternal())
    {
        const float leftDistance = distanceSquared(node->position, start->left->position);
        const float rightDistance = distanceSquared(node->position, start->right->position);
        insertNode(rightDistance <= leftDistance ? start->right : start->left, node);
        return;
    }

    Node* branch = new Node(this);
    branch->parent = start->parent;
    if (branch->parent->left == start)
        branch->parent->left = branch;
    else
        branch->parent->right = branch;

    start->parent = branch;
    branch->radius = -1.0f;
    branch->left = start;
    branch->right = node;
    node->parent = branch;
    updateNodePosAndRadius(branch);
}

void BinTreeCuller::insertNode(Node* node)
{
    if (!m_root)
    {
        m_root = node;
        node->parent = 0;
        return;
    }

    if (m_root->isInternal())
    {
        insertNode(m_root, node);
        return;
    }

    Node* oldRoot = m_root;
    Node* branch = new Node(this);
    m_root = branch;
    branch->parent = 0;
    branch->left = oldRoot;
    oldRoot->parent = branch;
    branch->right = node;
    node->parent = branch;
    updateNodePosAndRadius(branch);
}

void BinTreeCuller::removeNode(Node* node)
{
    if (node == m_root)
    {
        m_root = 0;
    }
    else
    {
        Node* parent = node->parent;
        Node* sibling = parent->left == node ? parent->right : parent->left;
        Node* grandParent = parent->parent;
        if (grandParent)
        {
            if (grandParent->left == parent)
                grandParent->left = sibling;
            else
                grandParent->right = sibling;
            sibling->parent = grandParent;
            updateNodePosAndRadius(grandParent);
        }
        else
        {
            m_root = sibling;
            sibling->parent = 0;
        }
        delete parent;
    }
    node->parent = 0;
}

bool BinTreeCuller::Node::updateNodePosAndRadius()
{
    const Vec3 oldPosition = position;
    const float oldRadius = radius;

    const Vec3 delta = {
        right->position.x - left->position.x,
        right->position.y - left->position.y,
        right->position.z - left->position.z
    };
    const float distanceSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    const float radiusDifference = right->radius - left->radius;

    if (radiusDifference * radiusDifference <= distanceSq)
    {
        if (distanceSq >= 1.4210855e-14f)
        {
            const float distance = static_cast<float>(std::sqrt(distanceSq));
            const float factor = radiusDifference / distance;
            position.x = (left->position.x + right->position.x + delta.x * factor) * 0.5f;
            position.y = (left->position.y + right->position.y + delta.y * factor) * 0.5f;
            position.z = (left->position.z + right->position.z + delta.z * factor) * 0.5f;
            radius = (distance + right->radius + left->radius) * 0.5f;
        }
        else
        {
            position = left->position;
            radius = left->radius <= right->radius ? right->radius : left->radius;
        }
    }
    else if (radiusDifference >= 0.0f)
    {
        position = right->position;
        radius = right->radius;
    }
    else
    {
        position = left->position;
        radius = left->radius;
    }

    if (oldRadius != radius)
        return true;
    return std::fabs(oldPosition.x - position.x) > FLT_EPSILON
        || std::fabs(oldPosition.y - position.y) > FLT_EPSILON
        || std::fabs(oldPosition.z - position.z) > FLT_EPSILON;
}

void BinTreeCuller::updateNodePosAndRadius(Node* node)
{
    while (node && node->updateNodePosAndRadius())
        node = node->parent;
}

void BinTreeCuller::clear(Node* node)
{
    if (node->isInternal())
    {
        clear(node->left);
        clear(node->right);
    }
    delete node;
}

void BinTreeCuller::clear()
{
    if (m_root)
        clear(m_root);
    m_root = 0;
}

// Collects leaves through the current predicator. getAllObjects(vector&) does not clear it,
// so a full-tree query intentionally inherits the previous query's filter.
void BinTreeCuller::getAllObjects(Node* node)
{
    if (node->isInternal())
    {
        getAllObjects(node->left);
        getAllObjects(node->right);
        return;
    }

    IObject* const object = node->payload.object;
    if (m_queryPredicator && !m_queryPredicator->predicate(object))
        return;
    m_queryOut->push_back(object);
}

void BinTreeCuller::getAllObjects(std::vector<IObject*>& out)
{
    m_queryOut = &out;
    if (m_root)
        getAllObjects(m_root);
}

unsigned short BinTreeCuller::nodeWeight(const Node* node)
{
    return node->isInternal() ? node->payload.metrics.weight : 1;
}

unsigned short BinTreeCuller::nodeMaxDepth(const Node* node)
{
    return node->isInternal() ? node->payload.metrics.maxDepth : 1;
}

void BinTreeCuller::calcWeightsAndMaxDepths(Node* node)
{
    if (!node->isInternal())
        return;
    calcWeightsAndMaxDepths(node->left);
    calcWeightsAndMaxDepths(node->right);
    node->payload.metrics.weight = static_cast<unsigned short>(
        nodeWeight(node->left) + nodeWeight(node->right));
    const unsigned short leftDepth = nodeMaxDepth(node->left);
    const unsigned short rightDepth = nodeMaxDepth(node->right);
    node->payload.metrics.maxDepth = static_cast<unsigned short>(
        (leftDepth < rightDepth ? rightDepth : leftDepth) + 1);
}

void BinTreeCuller::Node::propagateWeightAndMaxDepth()
{
    Node* node = this;
    do
    {
        if (node->isInternal())
        {
            node->payload.metrics.weight = static_cast<unsigned short>(
                BinTreeCuller::nodeWeight(node->left) + BinTreeCuller::nodeWeight(node->right));
            const unsigned short leftDepth = BinTreeCuller::nodeMaxDepth(node->left);
            const unsigned short rightDepth = BinTreeCuller::nodeMaxDepth(node->right);
            node->payload.metrics.maxDepth = static_cast<unsigned short>(
                (leftDepth < rightDepth ? rightDepth : leftDepth) + 1);
        }
        node = node->parent;
    }
    while (node);
}

void BinTreeCuller::balanceTree(Node* node, int depth)
{
    (void)depth; // Present in the shipped signature but not otherwise read.
    for (;;)
    {
        Node* left = node->left;
        if (!left)
            return;

        int iteration = 0;
        for (;;)
        {
            const int previous = iteration++;
            const int limit = left ? node->payload.metrics.weight : 1;
            if (previous > limit)
                break;

            Node* right = node->right;
            const unsigned short leftWeight = nodeWeight(left);
            const unsigned short rightWeight = nodeWeight(right);
            if (static_cast<unsigned short>(rightWeight & ~leftWeight) > leftWeight)
            {
                removeNode(left);
                insertNode(right, left);
                if (left->isInternal())
                {
                    left->payload.metrics.weight = static_cast<unsigned short>(
                        nodeWeight(left->left) + nodeWeight(left->right));
                    const unsigned short a = nodeMaxDepth(left->left);
                    const unsigned short b = nodeMaxDepth(left->right);
                    left->payload.metrics.maxDepth = static_cast<unsigned short>((a < b ? b : a) + 1);
                }
                if (left->parent)
                    left->parent->propagateWeightAndMaxDepth();
                node = right;
            }
            else
            {
                if (static_cast<unsigned short>(~rightWeight & leftWeight) <= rightWeight)
                    break;
                removeNode(right);
                insertNode(left, right);
                if (right->isInternal())
                {
                    right->payload.metrics.weight = static_cast<unsigned short>(
                        nodeWeight(right->left) + nodeWeight(right->right));
                    const unsigned short a = nodeMaxDepth(right->left);
                    const unsigned short b = nodeMaxDepth(right->right);
                    right->payload.metrics.maxDepth = static_cast<unsigned short>((a < b ? b : a) + 1);
                }
                if (right->parent)
                    right->parent->propagateWeightAndMaxDepth();
                node = left;
            }
            left = node->left;
        }

        balanceTree(node->left, depth + 1);
        node = node->right;
        ++depth;
    }
}

void BinTreeCuller::optimize()
{
    if (!m_root)
        return;
    calcWeightsAndMaxDepths(m_root);
    balanceTree(m_root, 1);
}

void BinTreeCuller::getObjectsByOriginAndRadius(Node* node)
{
    const float combinedRadius = m_queryRadius + node->radius;
    if (distanceSquared(m_queryOrigin, node->position) > combinedRadius * combinedRadius)
        return;

    if (node->isInternal())
    {
        getObjectsByOriginAndRadius(node->left);
        getObjectsByOriginAndRadius(node->right);
        return;
    }

    IObject* object = node->payload.object;
    if (!m_queryPredicator || m_queryPredicator->predicate(object))
        m_queryOut->push_back(object);
}

void BinTreeCuller::getObjects(const Vec3& centre, float radius,
                               std::vector<IObject*>& out,
                               ObjectPredicator* predicator)
{
    if (!m_root)
        return;
    m_queryPredicator = predicator;
    m_queryOrigin = centre;
    m_queryRadius = radius;
    m_queryOut = &out;
    getObjectsByOriginAndRadius(m_root);
}

void BinTreeCuller::appendGetObjects(const Vec3& centre, float radius,
                                     std::vector<IObject*>& out,
                                     ObjectPredicator* predicator)
{
    getObjects(centre, radius, out, predicator);
}

// Queries a segment using a direction delta, not an endpoint. Bounds nodes by a segment
// sphere, then tests endpoints and forward-ray proximity. Appends raw dot-product/object
// pairs in traversal order without sorting.
void BinTreeCuller::getDirectionObjects(Node* node)
{
    const float midpointDistanceSquared = distanceSquared(node->position, m_directionMidpoint);
    const float broadRadius = node->radius + m_queryRadius;
    if (midpointDistanceSquared > broadRadius * broadRadius)
        return;

    const float radiusSquared = node->radius * node->radius;
    const Vec3 fromStart = {
        node->position.x - m_directionStart.x,
        node->position.y - m_directionStart.y,
        node->position.z - m_directionStart.z
    };
    const Vec3 fromEnd = {
        node->position.x - m_directionEnd.x,
        node->position.y - m_directionEnd.y,
        node->position.z - m_directionEnd.z
    };

    bool intersects = radiusSquared >
        fromStart.x * fromStart.x + fromStart.y * fromStart.y + fromStart.z * fromStart.z;
    if (!intersects)
    {
        intersects = radiusSquared >
            fromEnd.x * fromEnd.x + fromEnd.y * fromEnd.y + fromEnd.z * fromEnd.z;
    }

    const float projection = fromStart.x * m_direction.x
                           + fromStart.y * m_direction.y
                           + fromStart.z * m_direction.z;
    if (!intersects && projection >= 0.0f)
    {
        const Vec3 closest = {
            m_directionStart.x + m_directionOverLengthSquared.x * projection,
            m_directionStart.y + m_directionOverLengthSquared.y * projection,
            m_directionStart.z + m_directionOverLengthSquared.z * projection
        };
        intersects = distanceSquared(closest, node->position) <= radiusSquared;
    }

    if (!intersects)
        return;

    if (node->isInternal())
    {
        getDirectionObjects(node->left);
        getDirectionObjects(node->right);
        return;
    }

    m_directionOut->push_back(std::make_pair(projection, node->payload.object));
}

void BinTreeCuller::getObjects(const Vec3& from, const Vec3& direction,
                               std::vector<std::pair<float, IObject*> >& out)
{
    if (!m_root)
        return;

    m_directionOut = &out;
    m_directionMidpoint.x = from.x + direction.x * 0.5f;
    m_directionMidpoint.y = from.y + direction.y * 0.5f;
    m_directionMidpoint.z = from.z + direction.z * 0.5f;

    const float halfX = direction.x * 0.5f;
    const float halfY = direction.y * 0.5f;
    const float halfZ = direction.z * 0.5f;
    m_queryRadius = std::sqrt(halfX * halfX + halfY * halfY + halfZ * halfZ);
    if (!(m_queryRadius >= 0.001f))
        return;

    m_directionStart = from;
    m_directionEnd.x = from.x + direction.x;
    m_directionEnd.y = from.y + direction.y;
    m_directionEnd.z = from.z + direction.z;
    m_direction = direction;

    const float lengthSquared = direction.x * direction.x
                              + direction.y * direction.y
                              + direction.z * direction.z;
    const float inverseLengthSquared = 1.0f / lengthSquared;
    m_directionOverLengthSquared.x = direction.x * inverseLengthSquared;
    m_directionOverLengthSquared.y = direction.y * inverseLengthSquared;
    m_directionOverLengthSquared.z = direction.z * inverseLengthSquared;
    getDirectionObjects(m_root);
}

// PE 0x0072CA60; ELF 0x08305230. The shipped box query requires a nonempty tree.
void BinTreeCuller::getObjects(const Vec3& minimum, const Vec3& maximum,
                               std::vector<IObject*>& out, ObjectPredicator* predicator)
{
    m_queryPredicator = predicator;
    m_boxMinimum = minimum;
    m_boxMaximum = maximum;
    m_queryOut = &out;
    getObjectsInBoxByStartAndEndPos(m_root);
}

// PE 0x0072C980; ELF 0x083050D0.
void BinTreeCuller::getObjectsInBoxByStartAndEndPos(Node* node)
{
    float distance = 0.0f;
    if (m_boxMinimum.x > node->position.x)
    {
        const float delta = m_boxMinimum.x - node->position.x;
        distance = delta * delta;
    }
    else if (node->position.x > m_boxMaximum.x)
    {
        const float delta = m_boxMaximum.x - node->position.x;
        distance = delta * delta;
    }

    // shipped defect: z replaces the x contribution; y is never tested.
    if (m_boxMinimum.z > node->position.z)
    {
        const float delta = m_boxMinimum.z - node->position.z;
        distance = delta * delta;
    }
    else if (node->position.z > m_boxMaximum.z)
    {
        const float delta = m_boxMaximum.z - node->position.z;
        distance = delta * delta;
    }

    if (distance > node->radius * node->radius)
        return;
    if (node->left)
    {
        getObjectsInBoxByStartAndEndPos(node->left);
        getObjectsInBoxByStartAndEndPos(node->right);
    }
    else if (!m_queryPredicator || m_queryPredicator->predicate(node->payload.object))
    {
        m_queryOut->push_back(node->payload.object);
    }
}

// Collects fully contained nodes separately from plane-straddling nodes.
// Wholly contained internal nodes contribute their subtree without retesting spheres.
// Choose predicate/nonpredicate walkers once; preserve the resulting predicate call counts.
void BinTreeCuller::getObjects(const Frustum& frustum, std::vector<IObject*>& visible,
                               std::vector<IObject*>& allVisible, ObjectPredicator* predicator)
{
    // An empty tree answers before touching anything.
    if (!m_root)
        return;

    m_queryPredicator = predicator;
    m_queryFrustum = frustum;            // Frustum::operator=, which re-derives planes and corners

    // both caches come OFF the copy, not the argument. operator= runs reSetup(), so the
    // centre and bounding radius read here are the rebuilt ones. Reading them from `frustum`
    // would be a different value whenever the caller's own derived data is stale.
    m_queryOrigin = m_queryFrustum.m_center;              // retail +0x204 out of the embedded copy
    m_queryRadius = m_queryFrustum.m_boundingRadius;      // retail +0x210

    m_queryOut        = &visible;
    m_queryAllVisible = &allVisible;

    if (predicator)
        getObjectsByFrustum(m_root);
    else
        getObjectsByFrustumNonPred(m_root);
}

// The walk, with a predicator.
void BinTreeCuller::getObjectsByFrustum(Node* node)
{
    // deliberately uninitialised. Frustum::inside leaves it untouched on every false
    // return, and retail's local is uninitialised too; it is read only after a true answer.
    bool fullyInside;
    if (!m_queryFrustum.inside(node->position, node->radius, fullyInside))
        return;

    if (node->isInternal())
    {
        if (fullyInside)
        {
            getAllObjects(node);
            return;
        }
        getObjectsByFrustum(node->left);
        getObjectsByFrustum(node->right);
        return;
    }

    if (m_queryPredicator && !m_queryPredicator->predicate(node->payload.object))
        return;

    if (fullyInside)
        m_queryOut->push_back(node->payload.object);
    else
        m_queryAllVisible->push_back(node->payload.object);
}

// The same walk with the predicator specialised away.
void BinTreeCuller::getObjectsByFrustumNonPred(Node* node)
{
    bool fullyInside;
    if (!m_queryFrustum.inside(node->position, node->radius, fullyInside))
        return;

    if (node->isInternal())
    {
        if (fullyInside)
        {
            getAllObjectsNonPred(node);
            return;
        }
        getObjectsByFrustumNonPred(node->left);
        getObjectsByFrustumNonPred(node->right);
        return;
    }

    if (fullyInside)
        m_queryOut->push_back(node->payload.object);
    else
        m_queryAllVisible->push_back(node->payload.object);
}

// The unfiltered twin of getAllObjects(Node*) above.
// The pair was nearly written here as two NEW functions. It is not new: 0x8304CF0 is the
// getAllObjects(Node*) this file has always had, and the ELF's own symbol table names this one
// getAllObjectsNonPred. Both write +0x238 - m_queryOut - because that is the single output
// pointer every query on this class uses.
void BinTreeCuller::getAllObjectsNonPred(Node* node)
{
    if (node->isInternal())
    {
        getAllObjectsNonPred(node->left);
        getAllObjectsNonPred(node->right);
        return;
    }
    m_queryOut->push_back(node->payload.object);
}

} // namespace world
} // namespace hfe
} // namespace dice
