// [TEMP] [CHOSEN, not recovered] Offline owner checks. No engine or SDK networking is linked.
// ClassManager is real; the profile and patch collaborators observe only the calls under test.
#include "online_service_layout.h"
#include "../BF2/Game/ProfileManager.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Game/MatchMaking/Server/MatchMakingServer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>

using namespace dice::hfe;
namespace {
unsigned checks, failures, freed;
bool watchAllocation;
void* watched;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL %s\n", label); }
}
[[noreturn]] void unexpected() { std::fputs("unexpected profile operation\n", stderr); std::abort(); }
}
void* operator new(std::size_t n)
{
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    if (watchAllocation) { watched = p; watchAllocation = false; }
    return p;
}
void operator delete(void* p) noexcept
{
    if (p && p == watched) { ++freed; watched = nullptr; }
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }

extern "C" { char gcd_gamename[256]; char gcd_secret_key[256]; }
namespace dice { namespace hfe {
ClassManager* classManager;
IBase* g_profileManager;
namespace gamespy {
extern const char* const g_gameName = "offline-fixture-game";
const char* getSecretKey()
{
    check(std::strcmp(gcd_gamename, g_gameName) == 0, "game name copied before secret retrieval");
    return "fixture-secret";
}
}
} }

namespace {
struct ProfileObserver : IProfileManager {
    OnlineProfile* current = nullptr;
    unsigned reads = 0;
    void addRef() override { unexpected(); }
    int getRef() override { unexpected(); }
    int release() override { unexpected(); }
    void* queryInterface(unsigned int) const override { unexpected(); }
    bool createOnlineAccount(const std::string&, const std::string&, const std::string&, const std::string&) override { unexpected(); }
    bool login(const std::string&, const std::string&) override { unexpected(); }
    bool loginAccount(const std::string&, const std::string&, const std::string&) override { unexpected(); }
    bool loginAlternate(const std::string&, const std::string&) override { unexpected(); }
    bool createProfile(const std::string&, const std::string&) override { unexpected(); }
    bool selectProfile(const std::string&) override { unexpected(); }
    void logout() override { unexpected(); }
    OnlineProfile* getCurrentProfile() override { ++reads; return current; }
    bool getProfileId(int&) override { unexpected(); }
    bool getUniqueNick(std::string&) override { unexpected(); }
    bool deleteProfile() override { unexpected(); }
    bool getProfileInformation(int, ProfileInformation&) override { unexpected(); }
    bool setProfileInformation(const ProfileInformation&) override { unexpected(); }
    bool isLoggedIn() override { unexpected(); }
    bool update() override { unexpected(); }
    bool requestNickSuggestions(const std::string&) override { unexpected(); }
    bool getNickSuggestions(std::vector<std::string>&) override { unexpected(); }
    bool validateProfileName(const std::string&, std::string&) override { unexpected(); }
    bool hasPendingTransition(std::string&) override { unexpected(); }
    void setPendingTransition(bool, const std::string&) override { unexpected(); }
};
struct PatchObserver : AutoPatch {
    PatchDesc* seen = nullptr;
    unsigned calls = 0;
    bool answer = false;
    bool checkForPatch(PatchDesc* desc) override { ++calls; seen = desc; return answer; }
};
struct RegistryObserver : ClassManager {
    IAutoPatch* patch = nullptr;
    unsigned queries = 0;
    IBase* getSingleton(const std::string& name) override
    { ++queries; check(name == "AutoPatch", "news requests named AutoPatch singleton"); return patch; }
};
// The production registration preprocessor check independently verifies these five rows.
// This selected class server exercises ClassManager's real construction/release lifecycle.
struct OnlineClassServer : IClassServer {
    IBase* instances[5] = {};
    int refs = 1;
    void addRef() override { ++refs; }
    int getRef() override { return refs; }
    int release() override { return --refs; }
    void* queryInterface(unsigned int) const override { unexpected(); }
    void getClassList(std::vector<ClassRegInfo>& rows) override
    {
        rows.push_back({CID_RankManager, "dice.hfe.RankManager", &RankManager::create});
        rows.push_back({CID_ClanManager, "dice.hfe.ClanManager", &ClanManager::create});
        rows.push_back({CID_NewsManager, "dice.hfe.NewsManager", &NewsManager::create});
        rows.push_back({CID_AutoPatch, "dice.hfe.AutoPatch", &AutoPatch::create});
        rows.push_back({CID_Persistence, "dice.hfe.Persistence", &Persistence::create});
    }
    void getSingletonList(std::vector<SingletonRegInfo>& rows) override
    {
        rows.push_back({"RankManager", &instances[0], IID_IRankManager, CID_RankManager, 2});
        rows.push_back({"ClanManager", &instances[1], IID_IClanManager, CID_ClanManager, 1});
        rows.push_back({"NewsManager", &instances[2], IID_INewsManager, CID_NewsManager, 1});
        rows.push_back({"AutoPatch", &instances[3], IID_IAutoPatch, CID_AutoPatch, 1});
        rows.push_back({"Persistence", &instances[4], IID_IPersistence, CID_Persistence, 1});
    }
};
template<class Owner> struct DestructionObserver : Owner {
    bool* destroyed;
    explicit DestructionObserver(bool* p) : destroyed(p) {}
    ~DestructionObserver() override { *destroyed = true; }
};
template<class Owner, class Interface> void lifetime(unsigned iid)
{
    static_assert(!std::is_abstract<Owner>::value, "complete owner");
    for (unsigned requested : {IID_IBase, iid}) {
        IBase* object = Owner::create(requested, nullptr);
        Interface* api = static_cast<Interface*>(object);
        check(api && api->getRef() == 1, "factory returns one reference");
        check(api->queryInterface(iid) == api && api->queryInterface(IID_IBase) == api,
              "interface lookup preserves identity without adding references");
        check(!api->queryInterface(iid + 1) && !api->queryInterface(0), "foreign IID and CID refused");
        api->addRef();
        check(api->getRef() == 2 && api->release() == 1, "balanced reference increment/decrement");
        check(api->release() == 0, "last release returns saved zero");
    }
    Owner outer;
    watchAllocation = true;
    check(!Owner::create(iid, &outer) && watchAllocation, "aggregation refused before allocation");
    watchAllocation = false;
    const unsigned before = freed;
    watchAllocation = true;
    check(!Owner::create(0xffffffffu, nullptr) && !watchAllocation && freed == before + 1,
          "unsupported factory IID destroys its allocation");
    bool destroyed = false;
    Interface* api = new DestructionObserver<Owner>(&destroyed);
    check(api->release() == 0 && destroyed, "release dispatches the virtual destructor");
}
}

int main()
{
    lifetime<RankManager, IRankManager>(IID_IRankManager);
    lifetime<ClanManager, IClanManager>(IID_IClanManager);
    lifetime<NewsManager, INewsManager>(IID_INewsManager);
    lifetime<AutoPatch, IAutoPatch>(IID_IAutoPatch);
    lifetime<Persistence, IPersistence>(IID_IPersistence);

    AutoPatch patch;
    unsigned char descriptorStorage[32];
    std::memset(descriptorStorage, 0xa5, sizeof(descriptorStorage));
    PatchDesc* descriptor = reinterpret_cast<PatchDesc*>(descriptorStorage);
    IAutoPatch* patchApi = &patch;
    check(!patchApi->checkForPatch(nullptr) && !patchApi->checkForPatch(descriptor), "retail patch refusal");
    for (unsigned char byte : descriptorStorage) check(byte == 0xa5, "descriptor remains untouched");

    RegistryObserver registry;
    PatchObserver first, second;
    second.answer = true;
    classManager = &registry;
    registry.patch = &first;
    NewsManager news;
    INewsManager* newsApi = &news;
    newsApi->checkForPatch(descriptor);
    check(first.seen == descriptor && first.calls == 1, "news forwards descriptor");
    registry.patch = &second;
    newsApi->checkForPatch(nullptr);
    check(second.seen == nullptr && second.calls == 1 && registry.queries == 2,
          "news repeats lookup and forwards null descriptor");
    check(first.getRef() == 1 && second.getRef() == 1, "news borrows patch without reference changes");

    ProfileObserver profile;
    g_profileManager = &profile;
    std::strcpy(gcd_gamename, "old-game"); std::strcpy(gcd_secret_key, "old-secret");
    Persistence persistence;
    IPersistence* persistenceApi = &persistence;
    check(std::strcmp(gcd_gamename, gamespy::g_gameName) == 0 && std::strcmp(gcd_secret_key, "fixture-secret") == 0,
          "constructor replaces SDK identity strings");
    check(!persistenceApi->connect() && profile.reads == 1, "no current profile refuses persistence");
    profile.current = reinterpret_cast<OnlineProfile*>(descriptorStorage);
    check(persistenceApi->connect() && profile.reads == 2, "any current profile enables persistence");
    profile.current = nullptr;
    check(!persistenceApi->connect() && profile.reads == 3, "profile presence is read each time");
    persistenceApi->update();
    check(profile.reads == 3 && persistence.getRef() == 1, "retail update has no effects");

    OnlineClassServer server;
    ClassManager classes;
    classManager = &classes;
    classes.registerServer(&server);
    check(server.instances[0] && !server.instances[1] && !server.instances[2] &&
          !server.instances[3] && !server.instances[4], "rank created during registration, other owners deferred");
    classes.initSingletons();
    for (IBase* object : server.instances) check(object && object->getRef() == 1, "deferred singleton factory succeeds");
    auto* registeredNews = static_cast<INewsManager*>(classes.getSingleton("NewsManager"));
    registeredNews->checkForPatch(descriptor);
    check(classes.getSingleton("AutoPatch")->getRef() == 1, "registered news borrows real patch singleton");
    auto* registeredPersistence = static_cast<IPersistence*>(classes.getSingleton("Persistence"));
    check(!registeredPersistence->connect(), "registered persistence observes current profile");
    IBase* byId = classes.createInstance(CID_ClanManager, IID_IClanManager, nullptr);
    check(byId && byId->release() == 0, "class id creates registered owner through real ClassManager");
    classes.unregisterServer(&server);
    for (IBase* object : server.instances) check(!object, "unregistration releases and clears singleton slot");
    check(server.refs == 1, "class server reference balanced");
    g_profileManager = nullptr; classManager = nullptr;
    std::printf("online services: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
