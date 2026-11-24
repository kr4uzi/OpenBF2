// Exposes game state and configuration to the AI.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_BFGAME_H
#define AI_AIINTERFACE_BFKNOWLEDGE_BFGAME_H

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "AIAdapterTypes.h"

namespace dice {
namespace hfe {

class IBase;

namespace ai {

class IAIGame
{
public:
    IAIGame();
    virtual ~IAIGame();

    virtual int getMaxNumberOfPlayers() const = 0;
    virtual int getAISettingsFromGame(int& a, bool& b, float& c) const = 0;
    virtual bool isServer() const = 0;
    virtual int getTickets(int team) const = 0;
    virtual int getClosestHostileMine(IAIPlayer* player, Vec3 from, bool& found, Vec3& position, Vec3& normal, unsigned int& id, std::vector<unsigned int>& ignore) const = 0;
    virtual int getNumberOfControlPoints(int team) const = 0;
    virtual int getOwnerOfControlPoint(unsigned int index) const = 0;
    // forward IControlPoint's float result in ST0.
    virtual float getControlPointTakeOverCounter(unsigned int index) const = 0;
    // [CHOSEN, not recovered] "AI control-point queries reject missing IDs" in
    // docs/retail-fixes.md: true for an existing ID; active separately reports capture eligibility.
    // A missing ID returns false and zeros position, radius and active.
    virtual bool getControlPointInfo(unsigned int index, Vec3& position, float& radius, bool& active) const = 0;
    virtual int findControlPoint(const Vec2& a, const Vec2& b) const = 0;
    // Borrowed addresses: , .
    // [INFERRED] const-reference spelling follows the typed forwarding targets.
    virtual const std::list<int>& getSpawnPointList(int team) const = 0;
    virtual const Vec3& getSpawnPointPosition(int index) const = 0;
    virtual int getControlPointId(int index) const = 0;
    virtual int getSpawnPointPositions(std::vector<Vec3>& out, unsigned char team) const = 0;
    virtual void getControlPointPositions(std::vector<Vec3>& out, unsigned char team) const = 0;
    virtual bool isEligibleSpawnpoint(int team, int groupId) const = 0;
    virtual bool isEnterOnSpawnSpawnpoint(int index) const = 0;
    virtual int getEnterOnSpawnSpawnpointObject(int a, int b) const = 0;
    virtual std::string getLevelPath() const = 0;
    virtual int getMaxNumberOfSquads() const = 0;
    virtual int getMaxNumberOfSquadMembers() const = 0;
    virtual float getRecomendedViewDistance() const = 0;
    virtual bool isPlaying() const = 0;
    virtual void setIsPrecaching(bool precaching) = 0;
    virtual bool getIsPrecaching() const = 0;
    virtual void enemySpottedByAI(world::IPlayer* player, world::IObject* object) const = 0;
};

class BFGame : public IAIGame
{
public:
    BFGame();
    virtual ~BFGame();

    virtual int getMaxNumberOfPlayers() const;
    virtual int getAISettingsFromGame(int& a, bool& b, float& c) const;
    virtual bool isServer() const;
    virtual int getTickets(int team) const;
    virtual int getClosestHostileMine(IAIPlayer* player, Vec3 from, bool& found, Vec3& position, Vec3& normal, unsigned int& id, std::vector<unsigned int>& ignore) const;
    virtual int getNumberOfControlPoints(int team) const;
    virtual int getOwnerOfControlPoint(unsigned int index) const;
    virtual float getControlPointTakeOverCounter(unsigned int index) const;
    virtual bool getControlPointInfo(unsigned int index, Vec3& position, float& radius, bool& active) const;
    virtual int findControlPoint(const Vec2& a, const Vec2& b) const;
    virtual const std::list<int>& getSpawnPointList(int team) const;
    virtual const Vec3& getSpawnPointPosition(int index) const;
    virtual int getControlPointId(int index) const;
    virtual int getSpawnPointPositions(std::vector<Vec3>& out, unsigned char team) const;
    virtual void getControlPointPositions(std::vector<Vec3>& out, unsigned char team) const;
    virtual bool isEligibleSpawnpoint(int team, int groupId) const;
    virtual bool isEnterOnSpawnSpawnpoint(int index) const;
    virtual int getEnterOnSpawnSpawnpointObject(int a, int b) const;
    virtual std::string getLevelPath() const;
    virtual int getMaxNumberOfSquads() const;
    virtual int getMaxNumberOfSquadMembers() const;
    virtual float getRecomendedViewDistance() const;
    virtual bool isPlaying() const;
    virtual void setIsPrecaching(bool precaching);
    virtual bool getIsPrecaching() const;
    virtual void enemySpottedByAI(world::IPlayer* player, world::IObject* object) const;

private:
    /* +0x00 */ // vptr
    /* +0x04 */ bool m_isPrecaching;     // the constructor sets it true
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_BFKNOWLEDGE_BFGAME_H
