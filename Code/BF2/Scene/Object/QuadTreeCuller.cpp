// Path: [INFERRED] - see QuadTreeCuller.h.

#include "QuadTreeCuller.h"

#include <algorithm>
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

// [CHOSEN names, shipped side effects] Counts direction queries and visited nodes.
// Readers remain unidentified; preserve both increments.
volatile unsigned int g_directionQueries;
volatile unsigned int g_directionNodes;

inline float distanceSquared3(const Vec3& a, const Vec3& b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return x * x + y * y + z * z;
}

inline float distanceSquaredXZ(const Vec3& a, const Vec3& b)
{
    const float x = a.x - b.x;
    const float z = a.z - b.z;
    return x * x + z * z;
}

inline Vec3 matrixPosition(const Mat4& transform)
{
    Vec3 result = { transform.m[12], transform.m[13], transform.m[14] };
    return result;
}

} // anonymous namespace

QuadTreeCuller::Node::Node()
    : count(0)
    , parent(0)
    , radius(0.0f)
{
    children[0] = children[1] = children[2] = children[3] = 0;
    position.x = position.y = position.z = 0.0f;
}

QuadTreeCuller::ObjectHolder::ObjectHolder(QuadTreeCuller* owner,
                                           const Vec3& p, float r, IObject* o)
    : CullerObject(owner)
    , position(p)
    , object(o)
    , radius(r)
    , generation(0)
{
}

QuadTreeCuller::QuadTreeCuller()
    : m_refCount(1)
    , m_numberOfObjects(0)
    , m_initialized(false)
    , m_insertingObject(0)
    , m_queryOrigin(0)
    , m_queryRadius(0.0f)
    , m_queryOut(0)
    , m_queryPredicator(0)
    , m_queryGeneration(0)
    , m_root(0)
    , m_horizontalHalfLength(0.0f)
    , m_directionOut(0)
{
    m_directionStart.x = m_directionStart.y = m_directionStart.z = 0.0f;
    m_directionEnd.x = m_directionEnd.y = m_directionEnd.z = 0.0f;
    m_directionNormalized.x = m_directionNormalized.y = m_directionNormalized.z = 0.0f;
    m_horizontalDirectionNormalized.x = 0.0f;
    m_horizontalDirectionNormalized.y = 0.0f;
    m_horizontalDirectionNormalized.z = 0.0f;
    m_horizontalCentre.x = m_horizontalCentre.y = m_horizontalCentre.z = 0.0f;
}

QuadTreeCuller::~QuadTreeCuller()
{
    if (m_root)
    {
        for (unsigned int i = 0; i != 4; ++i)
            freeNode(m_root->children[i]);
        delete m_root;
    }
}

IBase* QuadTreeCuller::create(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    QuadTreeCuller* self = new QuadTreeCuller;
    IBase* result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result)
        self->release();
    return result;
}

void QuadTreeCuller::addRef() { ++m_refCount; }
int QuadTreeCuller::getRef() { return m_refCount; }

int QuadTreeCuller::release()
{
    const int result = --m_refCount;
    if (result == 0 && this)
        delete this;
    return result;
}

void* QuadTreeCuller::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_ICuller)
        return const_cast<QuadTreeCuller*>(this);
    return 0;
}

unsigned int QuadTreeCuller::getClassID() const
{
    return CID_QuadTreeCuller;
}

int QuadTreeCuller::getNumberOfObjects()
{
    return m_numberOfObjects;
}

// PE 0x0072ABA0; ELF 0x0831D470. Reinitializing a live root is a shipped invariant failure.
void QuadTreeCuller::initQuadTree(const Vec3& centre, float childOffset,
                                  float minimumWidth, float width)
{
    if (m_root)
        std::abort();

    width += 2.0f;
    m_root = new Node;

    Vec3 rootCentre = { centre.x, 0.0f, centre.z };
    m_initialized = true;
    const float firstChildOffset = static_cast<float>(
        std::sqrt(width * width * 0.5f) * 0.5f);
    initNode(m_root, rootCentre, firstChildOffset, minimumWidth, width);
    m_root->radius *= 100.0f;
}

void QuadTreeCuller::initNode(Node* node, const Vec3& centre, float childOffset,
                              float minimumWidth, float width)
{
    node->position = centre;
    node->radius = width;
    if (minimumWidth >= width)
        return;

    const float nextWidth = width * 0.5f;
    const float nextOffset = childOffset * 0.5f;

    const Vec3 childCentres[4] = {
        { centre.x - childOffset, 0.0f, centre.z - childOffset },
        { centre.x + childOffset, 0.0f, centre.z - childOffset },
        { centre.x + childOffset, 0.0f, centre.z + childOffset },
        { centre.x - childOffset, 0.0f, centre.z + childOffset }
    };

    for (unsigned int i = 0; i != 4; ++i)
    {
        node->children[i] = new Node;
        node->children[i]->parent = node;
        initNode(node->children[i], childCentres[i], nextOffset, minimumWidth, nextWidth);
    }
}

unsigned int QuadTreeCuller::insertObject(Node* node)
{
    const float combinedRadius = m_queryRadius + node->radius;
    if (distanceSquaredXZ(*m_queryOrigin, node->position) > combinedRadius * combinedRadius)
        return 0;

    unsigned int inserted = 0;
    if (node->children[0] && m_queryRadius < node->radius)
    {
        for (unsigned int i = 0; i != 4; ++i)
            inserted += insertObject(node->children[i]);

        if (inserted == 0 && node == m_root)
        {
            node->objects.push_front(m_insertingObject);
            NodeObject link = { node, node->objects.begin() };
            m_insertingObject->nodes.push_back(link);
            inserted = 1;
        }
    }
    else
    {
        node->objects.push_front(m_insertingObject);
        NodeObject link = { node, node->objects.begin() };
        m_insertingObject->nodes.push_back(link);
        inserted = 1;
    }

    node->count += inserted;
    return inserted;
}

CullerObject* QuadTreeCuller::createObject(const Vec3& position, float radius, IObject* object)
{
    // The exact shipped sentinel: objects at exactly y == 0 are refused.  NaNs take the live arm.
    if (position.y == 0.0f)
        return 0;

    m_queryOrigin = &position;
    if (radius <= 0.0f)
        radius = 1.0f;
    ++m_numberOfObjects;
    m_queryPredicator = 0;
    m_queryRadius = radius;

    ObjectHolder* holder = new ObjectHolder(this, position, radius, object);
    m_insertingObject = holder;
    if (insertObject(m_root))
        return holder->getCullerObject();

    --m_numberOfObjects;
    delete holder;
    m_insertingObject = 0;
    return 0;
}

void QuadTreeCuller::destroyObject(CullerObject* object)
{
    ObjectHolder* holder = static_cast<ObjectHolder*>(object);
    for (std::vector<NodeObject>::iterator it = holder->nodes.begin();
         it != holder->nodes.end(); ++it)
    {
        it->node->objects.erase(it->iterator);
        for (Node* node = it->node; node; node = node->parent)
            --node->count;
    }
    --m_numberOfObjects;
    delete holder;
}

CullerObject* QuadTreeCuller::updateObject(CullerObject* object,
                                            const Vec3& position, float radius)
{
    if (!object)
        return 0;

    ObjectHolder* holder = static_cast<ObjectHolder*>(object);
    IObject* worldObject = holder->object;
    const Vec3 currentPosition = matrixPosition(worldObject->getAbsoluteTransformation());
    if (distanceSquared3(holder->position, currentPosition) < 0.1f)
        return holder;

    destroyObject(holder);
    if (radius <= 0.0f)
        radius = std::min(worldObject->getVisualBoundingRadius(), 1.0f);
    CullerObject* replacement = createObject(position, radius, worldObject);
    return replacement ? replacement->getCullerObject() : 0;
}

void QuadTreeCuller::optimize()
{
    // genuinely empty.
}

void QuadTreeCuller::freeNode(Node* node)
{
    if (!node)
        return;
    for (unsigned int i = 0; i != 4; ++i)
        freeNode(node->children[i]);
    delete node;
}

void QuadTreeCuller::clear()
{
    if (m_root)
    {
        for (unsigned int i = 0; i != 4; ++i)
            freeNode(m_root->children[i]);
        delete m_root;
    }
    m_root = 0;
}

void QuadTreeCuller::getAllObjects(std::vector<IObject*>&)
{
    // genuinely empty, despite the name.
}

void QuadTreeCuller::getPosAndRadiusObjects(Node* node)
{
    const float combinedRadius = m_queryRadius + node->radius;
    if (distanceSquaredXZ(*m_queryOrigin, node->position) > combinedRadius * combinedRadius)
        return;

    for (std::list<ObjectHolder*>::iterator it = node->objects.begin();
         it != node->objects.end(); ++it)
    {
        ObjectHolder* holder = *it;
        if (holder->generation == m_queryGeneration)
            continue;
        holder->generation = m_queryGeneration;

        IObject* object = holder->object;
        if (m_queryPredicator && !m_queryPredicator->predicate(object))
            continue;

        const Vec3 objectPosition = matrixPosition(object->getAbsoluteTransformation());
        const float objectRadius = m_queryRadius + holder->radius;
        if (distanceSquared3(*m_queryOrigin, objectPosition) < objectRadius * objectRadius)
            m_queryOut->push_back(object);
    }

    if (node->children[0])
    {
        for (unsigned int i = 0; i != 4; ++i)
        {
            if (node->children[i]->count)
                getPosAndRadiusObjects(node->children[i]);
        }
    }
}

void QuadTreeCuller::getObjects(const Vec3& centre, float radius,
                                std::vector<IObject*>& out,
                                ObjectPredicator* predicator)
{
    if (!m_root)
        return;
    ++m_queryGeneration;
    m_queryOut = &out;
    m_queryPredicator = predicator;
    m_queryOrigin = &centre;
    m_queryRadius = radius;
    getPosAndRadiusObjects(m_root);
}

void QuadTreeCuller::clearGetObjects()
{
    ++m_queryGeneration;
}

void QuadTreeCuller::appendGetObjects(const Vec3& centre, float radius,
                                      std::vector<IObject*>& out,
                                      ObjectPredicator* predicator)
{
    m_queryOut = &out;
    m_queryPredicator = predicator;
    m_queryOrigin = &centre;
    m_queryRadius = radius;
    getPosAndRadiusObjects(m_root);
}

// Uses horizontal X/Z cells for the broad phase, then tests live object transforms in
// 3-D. Generation stamps suppress duplicates for objects stored in multiple cells.
void QuadTreeCuller::getDirectionObjects(Node* node)
{
    ++g_directionNodes;

    const float horizontalX = node->position.x - m_horizontalCentre.x;
    const float horizontalZ = node->position.z - m_horizontalCentre.z;
    const float broadRadius = node->radius + m_horizontalHalfLength;
    if (horizontalX * horizontalX + horizontalZ * horizontalZ > broadRadius * broadRadius)
        return;

    const float radiusSquared = node->radius * node->radius;
    const float fromStartX = node->position.x - m_directionStart.x;
    const float fromStartZ = node->position.z - m_directionStart.z;
    const float horizontalProjection =
        fromStartX * m_horizontalDirectionNormalized.x
      + fromStartZ * m_horizontalDirectionNormalized.z;

    const float closestX = m_directionStart.x
                         + m_horizontalDirectionNormalized.x * horizontalProjection;
    const float closestZ = m_directionStart.z
                         + m_horizontalDirectionNormalized.z * horizontalProjection;
    const float closestDx = closestX - node->position.x;
    const float closestDy = -node->position.y;
    const float closestDz = closestZ - node->position.z;
    if (closestDx * closestDx + closestDy * closestDy + closestDz * closestDz > radiusSquared)
        return;

    if (horizontalProjection < 0.0f)
    {
        if (fromStartX * fromStartX + fromStartZ * fromStartZ > radiusSquared)
            return;
    }
    else if (horizontalProjection > m_horizontalHalfLength + m_horizontalHalfLength)
    {
        const float fromEndX = node->position.x - m_directionEnd.x;
        const float fromEndZ = node->position.z - m_directionEnd.z;
        if (fromEndX * fromEndX + fromEndZ * fromEndZ > radiusSquared)
            return;
    }

    for (std::list<ObjectHolder*>::iterator it = node->objects.begin();
         it != node->objects.end(); ++it)
    {
        ObjectHolder* holder = *it;
        if (holder->generation == m_queryGeneration)
            continue;
        holder->generation = m_queryGeneration;

        IObject* object = holder->object;
        const Vec3 objectPosition = matrixPosition(object->getAbsoluteTransformation());
        const float objectRadiusSquared = holder->radius * holder->radius;
        const Vec3 objectFromStart = {
            objectPosition.x - m_directionStart.x,
            objectPosition.y - m_directionStart.y,
            objectPosition.z - m_directionStart.z
        };
        const float projection = objectFromStart.x * m_directionNormalized.x
                               + objectFromStart.y * m_directionNormalized.y
                               + objectFromStart.z * m_directionNormalized.z;

        bool intersects = objectRadiusSquared >
            objectFromStart.x * objectFromStart.x
          + objectFromStart.y * objectFromStart.y
          + objectFromStart.z * objectFromStart.z;
        if (!intersects)
        {
            const Vec3 objectFromEnd = {
                objectPosition.x - m_directionEnd.x,
                objectPosition.y - m_directionEnd.y,
                objectPosition.z - m_directionEnd.z
            };
            intersects = objectRadiusSquared >
                objectFromEnd.x * objectFromEnd.x
              + objectFromEnd.y * objectFromEnd.y
              + objectFromEnd.z * objectFromEnd.z;

            if (!intersects && projection >= 0.0f)
            {
                const Vec3 closest = {
                    m_directionStart.x + m_directionNormalized.x * projection,
                    m_directionStart.y + m_directionNormalized.y * projection,
                    m_directionStart.z + m_directionNormalized.z * projection
                };
                intersects = distanceSquared3(closest, objectPosition) < objectRadiusSquared;
            }
        }

        if (intersects)
            m_directionOut->push_back(std::make_pair(projection, object));
    }

    if (node->children[0])
    {
        for (unsigned int i = 0; i != 4; ++i)
        {
            if (node->children[i]->count)
                getDirectionObjects(node->children[i]);
        }
    }
}

void QuadTreeCuller::getObjects(const Vec3& from, const Vec3& direction,
                                std::vector<std::pair<float, IObject*> >& out)
{
    if (!m_root)
        return;

    ++g_directionQueries;
    m_directionOut = &out;
    m_directionStart = from;
    m_directionEnd.x = from.x + direction.x;
    m_directionEnd.y = from.y + direction.y;
    m_directionEnd.z = from.z + direction.z;
    m_directionNormalized = direction;

    const float length = std::sqrt(direction.x * direction.x
                                 + direction.y * direction.y
                                 + direction.z * direction.z);
    m_queryRadius = length;
    if (!(length >= 0.001f))
        return;

    const float inverseLength = 1.0f / length;
    m_directionNormalized.x *= inverseLength;
    m_directionNormalized.y *= inverseLength;
    m_directionNormalized.z *= inverseLength;
    m_queryRadius *= 0.5f;

    m_horizontalDirectionNormalized.x = direction.x;
    m_horizontalDirectionNormalized.y = 0.0f;
    m_horizontalDirectionNormalized.z = direction.z;
    const float horizontalLength = std::sqrt(direction.x * direction.x
                                           + direction.z * direction.z);
    float adjustedHorizontalLength = 0.1f;
    if (horizontalLength >= 0.1f)
        adjustedHorizontalLength = horizontalLength;
    m_horizontalHalfLength = adjustedHorizontalLength * 0.5f;
    const float inverseHorizontalLength = 1.0f / adjustedHorizontalLength;
    m_horizontalDirectionNormalized.x *= inverseHorizontalLength;
    m_horizontalDirectionNormalized.z *= inverseHorizontalLength;

    m_horizontalCentre.x = from.x
                         + m_horizontalDirectionNormalized.x * m_horizontalHalfLength;
    m_horizontalCentre.y = 0.0f;
    m_horizontalCentre.z = from.z
                         + m_horizontalDirectionNormalized.z * m_horizontalHalfLength;

    ++m_queryGeneration;
    getDirectionObjects(m_root);
}

// PE 0x0072B6C0; ELF 0x0831EF10.
void QuadTreeCuller::getObjects(const Vec3& minimum, const Vec3& maximum,
                                std::vector<IObject*>& out, ObjectPredicator* predicator)
{
    if (!m_root)
        return;
    ++m_queryGeneration;
    m_queryOut = &out;
    m_boxMinimum = minimum;
    m_boxMaximum = maximum;
    m_queryPredicator = predicator;
    getBoxObjects(m_root);
}

// PE 0x0072B280; ELF 0x0831EFB0.
void QuadTreeCuller::getBoxObjects(Node* node)
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
    // shipped defect: z replaces x, and this same distance carries between objects below.
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

    for (std::list<ObjectHolder*>::const_iterator it = node->objects.begin();
         it != node->objects.end(); ++it)
    {
        ObjectHolder* const holder = *it;
        if (holder->generation == m_queryGeneration)
            continue;
        holder->generation = m_queryGeneration;
        IObject* const object = holder->object;
        if (m_queryPredicator && !m_queryPredicator->predicate(object))
            continue;

        if (m_boxMinimum.x > object->getAbsoluteTransformation().m[12])
        {
            const float delta = m_boxMinimum.x - object->getAbsoluteTransformation().m[12];
            distance = delta * delta;
        }
        else if (object->getAbsoluteTransformation().m[12] > m_boxMaximum.x)
        {
            const float delta = m_boxMaximum.x - object->getAbsoluteTransformation().m[12];
            distance = delta * delta;
        }
        if (m_boxMinimum.z > object->getAbsoluteTransformation().m[14])
        {
            const float delta = m_boxMinimum.z - object->getAbsoluteTransformation().m[14];
            distance = delta * delta;
        }
        else if (object->getAbsoluteTransformation().m[14] > m_boxMaximum.z)
        {
            const float delta = m_boxMaximum.z - object->getAbsoluteTransformation().m[14];
            distance = delta * delta;
        }

        const float radius = object->getPhysicBoundingRadius();
        if (distance <= radius * radius)
            m_queryOut->push_back(object);
    }

    if (node->children[0])
    {
        for (unsigned int i = 0; i != 4; ++i)
        {
            if (node->children[i]->count)
                getBoxObjects(node->children[i]);
        }
    }
}

// Dynamic-object frustum query (RW-121), retaining a pointer to the caller's frustum.
// Projects it into silhouette planes, deduplicates objects with a generation stamp and resets per-query statistics.

// Retail's two frustum-query statistics counters, ELF 0x8AB0A9C and 0x8AB0AA0. They describe the
// last query, not the session: getObjects zeroes both on entry. [INFERRED] names.
int nNodesVisited = 0;                 // attested file-scope name
int nObjectsVisited = 0;               // [INFERRED] twin of the above

// Tests a sphere against the reduced planes and reports whether it is wholly inside.
// An empty plane set returns inside and fullyInside=true. Preserve the plane-expression
// association; outside means n dot center + d > radius.
bool insideFrustum2d(Plane3* planes, int count,
                     const Vec3& centre, float radius, bool& fullyInside)
{
    fullyInside = true;
    for (int i = 0; i < count; ++i)
    {
        const Plane3& pl = planes[i];
        const float d = pl.normal.x * centre.x + pl.normal.y * centre.y
                      + pl.normal.z * centre.z + pl.d;
        if (d > radius)
            return false;
        if (d > -radius)
            fullyInside = false;
    }
    return true;
}

// Projects a frustum along up into at most six vertical silhouette planes.
// Opposite directed edges cancel using an eight-bit nibble swap. The ten-slot edge table silently
// drops overflow, chooses the last free slot and emits surviving entries from slot 9 down to 0.
// This former [FRONTIER] now has its silhouette construction implemented; preserve those limits.
namespace {

// ds:0x088CA2C8, 24 bytes - the four edges of each of the six frustum planes, as corner-index
// nibble pairs.  Every edge appears exactly twice with opposite orientation; that was checked
// across the whole table before this body was written, and it is what makes step 2 well defined.
const unsigned char kFrustumPlaneEdges[24] =
{
    0x10, 0x04, 0x45, 0x51,   // plane 0
    0x23, 0x37, 0x76, 0x62,   // plane 1
    0x02, 0x26, 0x64, 0x40,   // plane 2
    0x31, 0x15, 0x57, 0x73,   // plane 3
    0x13, 0x32, 0x20, 0x01,   // plane 4
    0x46, 0x67, 0x75, 0x54    // plane 5
};

// FLT_EPSILON, spelled as the shipped immediate rather than by name.
const double kProjectFrustumEpsilon = 0.00000011920929;

int projectFrustum(Plane3* planes, const Frustum& frustum, const Vec3& up)
{
    unsigned char used[10];
    for (int i = 0; i <= 9; ++i)
        used[i] = 0;

    for (int p = 0; p <= 5; ++p)
    {
        const Plane3& face = frustum.m_planes[p];
        if (face.normal.x * up.x + face.normal.y * up.y + face.normal.z * up.z > 0.0f)
        {
            for (int e = 4 * p; e < 4 * p + 4; ++e)
            {
                const unsigned char code = kFrustumPlaneEdges[e];
                const unsigned char reversed =
                    static_cast<unsigned char>((code << 4) | (code >> 4));

                int freeSlot = -1;
                bool cancelled = false;
                for (int k = 0; k <= 9; ++k)
                {
                    if (used[k])
                    {
                        if (used[k] == reversed)
                        {
                            used[k] = 0;
                            cancelled = true;
                            break;
                        }
                    }
                    else
                    {
                        freeSlot = k;       // the last empty slot wins - see the note above
                    }
                }

                if (!cancelled && freeSlot >= 0)
                    used[freeSlot] = code;
            }
        }
    }

    int count = 0;
    for (int j = 9; j >= 0; --j)
    {
        const unsigned char code = used[j];
        if (!code)
            continue;

        const Vec3& c0 = frustum.m_corners[code >> 4];
        const Vec3& c1 = frustum.m_corners[code & 0x0F];

        float dx = c1.x - c0.x;
        float dy = c1.y - c0.y;
        float dz = c1.z - c0.z;

        // Normalise only when the edge is not already unit length; a degenerate edge collapses to
        // the zero vector rather than dividing by ~0.
        const double lengthSq = dz * dz + dy * dy + dx * dx;
        if (std::fabs(lengthSq - 1.0) >= kProjectFrustumEpsilon)
        {
            if (std::fabs(lengthSq) >= kProjectFrustumEpsilon)
            {
                const float inv = static_cast<float>(1.0 / std::sqrt(lengthSq));
                dx *= inv;
                dy *= inv;
                dz *= inv;
            }
            else
            {
                dx = 0.0f;
                dy = 0.0f;
                dz = 0.0f;
            }
        }

        Plane3& out = planes[count];
        ++count;

        out.normal.x = up.y * dz - up.z * dy;
        out.normal.y = up.z * dx - dz * up.x;
        out.normal.z = dy * up.x - dx * up.y;
        out.d = -(out.normal.z * c0.z + out.normal.y * c0.y + out.normal.x * c0.x);

        if (count == 6)
            break;
    }

    return count;
}

} // namespace

// Separates accepted objects from those requiring ObjectManager's full 3D frustum retest.
// Predicate walks stamp before filtering; visibility-flag walks stamp only accepted visible objects.
// Preserve that asymmetry for objects occupying multiple nodes.

// The placement rule both wholly-inside collectors share, inlined in each of them in the shipped
// image.  An object registered in exactly one quadtree node needs no further test and goes to
// m_queryOut; one spanning several nodes is placed by whether the query sphere still reaches its
// origin in the XZ plane, because such an object can be collected more than once.
void QuadTreeCuller::collectWhollyInside(ObjectHolder* holder, IObject* object)
{
    std::vector<IObject*>* out;

    if (holder->nodes.size() == 1)
    {
        out = m_queryOut;
    }
    else
    {
        // ELF slot 33 - getAbsoluteTransformation; the origin is the matrix translation row, and
        // only X and Z are used.  This is a 2D query.
        const Mat4& transform = object->getAbsoluteTransformation();
        const float dx = m_queryOrigin->x - transform.m[12];
        const float dz = m_queryOrigin->z - transform.m[14];
        const float reach = holder->radius + m_queryRadius;
        out = (reach * reach > dx * dx + dz * dz) ? m_queryOut : m_queryAllVisible;
    }

    out->push_back(object);
}

// The subtree collector: everything below `node` with no further frustum test,
// because the caller established the node is wholly inside.
void QuadTreeCuller::getAllObjects(Node* node)
{
    for (std::list<ObjectHolder*>::iterator it = node->objects.begin();
         it != node->objects.end(); ++it)
    {
        ObjectHolder* const holder = *it;
        if (holder->generation == m_queryGeneration)
            continue;
        holder->generation = m_queryGeneration;

        IObject* const object = holder->object;
        if (m_queryPredicator && !m_queryPredicator->predicate(object))
            continue;

        collectWhollyInside(holder, object);
    }

    if (node->children[0])
    {
        for (unsigned int i = 0; i <= 3; ++i)
        {
            if (node->children[i]->count)
                getAllObjects(node->children[i]);
        }
    }
}

// The predicator-free twin of the collector above: same placement rule,
// but filtered on the object's own OFIsVisible bit instead of a predicator.
void QuadTreeCuller::getFrustumAllVisibleObjects(Node* node)
{
    for (std::list<ObjectHolder*>::iterator it = node->objects.begin();
         it != node->objects.end(); ++it)
    {
        ObjectHolder* const holder = *it;
        IObject* const object = holder->object;

        if ((object->getFlags() & IObject::OFIsVisible) == 0)
            continue;
        if (holder->generation == m_queryGeneration)
            continue;
        holder->generation = m_queryGeneration;

        collectWhollyInside(holder, object);
    }

    if (node->children[0])
    {
        for (unsigned int i = 0; i <= 3; ++i)
        {
            if (node->children[i]->count)
                getFrustumAllVisibleObjects(node->children[i]);
        }
    }
}

// The predicated walk.
void QuadTreeCuller::getFrustumObjects(Node* node)
{
    bool fullyInside = false;
    if (!insideFrustum2d(m_queryPlanes, m_queryPlaneCount, node->position, node->radius,
                         fullyInside))
        return;

    if (fullyInside)
    {
        // The wholly-inside arm reuses the radius query's scratch, exactly as retail does - its
        // +0x18/+0x1C are m_queryOrigin and m_queryRadius.
        m_queryOrigin = &node->position;
        m_queryRadius = node->radius;
        getAllObjects(node);
        return;
    }

    for (std::list<ObjectHolder*>::iterator it = node->objects.begin();
         it != node->objects.end(); ++it)
    {
        ObjectHolder* const holder = *it;
        if (holder->generation == m_queryGeneration)
            continue;
        holder->generation = m_queryGeneration;

        IObject* const object = holder->object;
        if (m_queryPredicator && !m_queryPredicator->predicate(object))
            continue;

        m_queryAllVisible->push_back(object);
    }

    if (node->children[0])
    {
        for (unsigned int i = 0; i <= 3; ++i)
        {
            if (node->children[i]->count)
                getFrustumObjects(node->children[i]);
        }
    }
}

// The predicator-free walk, chosen by getObjects when no predicator was
// supplied.  dice's own NonPred convention: the filter is specialised out rather than tested.
void QuadTreeCuller::getFrustumVisibleObjects(Node* node)
{
    bool fullyInside = false;
    if (!insideFrustum2d(m_queryPlanes, m_queryPlaneCount, node->position, node->radius,
                         fullyInside))
        return;

    if (fullyInside)
    {
        m_queryOrigin = &node->position;
        m_queryRadius = node->radius;
        getFrustumAllVisibleObjects(node);
        return;
    }

    for (std::list<ObjectHolder*>::iterator it = node->objects.begin();
         it != node->objects.end(); ++it)
    {
        ObjectHolder* const holder = *it;
        IObject* const object = holder->object;

        if ((object->getFlags() & IObject::OFIsVisible) == 0)
            continue;
        if (holder->generation == m_queryGeneration)
            continue;
        holder->generation = m_queryGeneration;

        m_queryAllVisible->push_back(object);
    }

    if (node->children[0])
    {
        for (unsigned int i = 0; i <= 3; ++i)
        {
            if (node->children[i]->count)
                getFrustumVisibleObjects(node->children[i]);
        }
    }
}

// written in full.
void QuadTreeCuller::getObjects(const Frustum& frustum, std::vector<IObject*>& visible,
                                std::vector<IObject*>& allVisible, ObjectPredicator* predicator)
{
    if (!m_root)
        return;

    nNodesVisited = 0;
    nObjectsVisited = 0;

    ++m_queryGeneration;
    m_queryPredicator = predicator;
    m_queryOut        = &visible;   // retail +0x20 - the one output pointer, see the header
    m_queryFrustum    = &frustum;
    m_queryAllVisible = &allVisible;

    Vec3 up;
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    m_queryPlaneCount = projectFrustum(m_queryPlanes, frustum, up);

    if (predicator)
        getFrustumObjects(m_root);
    else
        getFrustumVisibleObjects(m_root);

    nNodesVisited    += static_cast<int>(m_queryOut->size());
    nObjectsVisited  += static_cast<int>(m_queryAllVisible->size());
}

} // namespace world
} // namespace hfe
} // namespace dice
