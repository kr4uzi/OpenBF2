// [TEMP] Actual native HUD/Ref2System and fullInvert; typed view/camera observers.
// [RETIRE-WHEN: HUD-PROJECTION-FIXTURE] Real HUD/view/camera lifetime covers these methods.
// [RETIRE-WHEN: HUD-PROJECTION-FIXTURE] Replace dependency observers and retained
// pre-init HUD once the complete scene/HUD lifecycles are executable.
#define main unusedChatRegression
#include "hud_chat_send_runtime.cpp"
#undef main
#include <cmath>
#include <cfloat>
#define private public
#include "../BF2/Menu/Ref2System.h"
#undef private
#include "../BF2/RendMisc/IRenderView.h"
#include "../BF2/RendMisc/IRenderer.h"
#include "../BF2/GeomMisc/MenuRend/IQuadDrawer.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Game/Objects/ICameraObject.h"
using world::CameraViewMode;
using world::IObject;
namespace dice { namespace hfe { class ClassManager; ClassManager* classManager=nullptr; System* g_system=nullptr; } }
struct ProjectionView : dx9rend::IRenderView {
    float width=800,height=600,nearPlane=0.5f;
    Mat4 projection=Mat4::getIdentity();
    std::function<void()> onProjection;
    void addRef() override {actionUnsupported("view addref");}
    int getRef() override {actionUnsupported("view getref");}
    int release() override {actionUnsupported("view release");}
    void* queryInterface(unsigned) const override {actionUnsupported("view query");}
    float getOrthoWidth() override {trace.push_back("width");return width;}
    float getOrthoHeight() override {trace.push_back("height");return height;}
    float getNearPlane() override {trace.push_back("near");return nearPlane;}
    Mat4* getProjectionMatrixPtr() override {trace.push_back("projection");if(onProjection)onProjection();return &projection;}
    void setViewport(float x, float y, float width, float height) override { actionUnsupported("unused projection interface method"); }
    float* getViewport() override { actionUnsupported("unused projection interface method"); }
    void setAdjustedFov(float radians) override { actionUnsupported("unused projection interface method"); }
    void setFov(float radians) override { actionUnsupported("unused projection interface method"); }
    float getFov() override { actionUnsupported("unused projection interface method"); }
    float getReferenceFov() override { actionUnsupported("unused projection interface method"); }
    float getZoom() override { actionUnsupported("unused projection interface method"); }
    float getInverseZoom() override { actionUnsupported("unused projection interface method"); }
    void setNearPlane(float value) override { actionUnsupported("unused projection interface method"); }
    void setFarPlane(float value) override { actionUnsupported("unused projection interface method"); }
    float getFarPlane() override { actionUnsupported("unused projection interface method"); }
    void setAspect(float value) override { actionUnsupported("unused projection interface method"); }
    float getAspect() override { actionUnsupported("unused projection interface method"); }
    void setProjectionMode(unsigned int mode) override { actionUnsupported("unused projection interface method"); }
    unsigned int getProjectionMode() override { actionUnsupported("unused projection interface method"); }
    void setOrthoWidth(float value) override { actionUnsupported("unused projection interface method"); }
    void setOrthoHeight(float value) override { actionUnsupported("unused projection interface method"); }
    void setViewMatrix(const float* matrix16) override { actionUnsupported("unused projection interface method"); }
    Mat4* getViewMatrixPtr() override { actionUnsupported("unused projection interface method"); }
    Mat4* getInverseViewMatrixPtr() override { actionUnsupported("unused projection interface method"); }
    Mat4* getProjectionTransposePtr() override { actionUnsupported("unused projection interface method"); }
    Frustum* getFrustumPtr() override { actionUnsupported("unused projection interface method"); }
    IRenderView* clone() override { actionUnsupported("unused projection interface method"); }
    void setFlag42E(bool value) override { actionUnsupported("unused projection interface method"); }
    bool getFlag42E() override { actionUnsupported("unused projection interface method"); }
    Mat4* getViewProjectionMatrixPtr() override { actionUnsupported("unused projection interface method"); }
    Mat4* getCameraMatrixPtr() override { actionUnsupported("unused projection interface method"); }
    Mat4* getLinearProjectionMatrixPtr() override { actionUnsupported("unused projection interface method"); }
    Mat4* getInverseProjectionMatrixPtr() override { actionUnsupported("unused projection interface method"); }
    void setFlag42F(bool value) override { actionUnsupported("unused projection interface method"); }
    bool getFlag42F() override { actionUnsupported("unused projection interface method"); }
};
// [TEMP] Typed observers exercise Ref2System's real paint callbacks and borrowed views.
struct PaintView : ProjectionView {
    std::function<void()> onMode;
    unsigned updates=0;
    void setProjectionMode(unsigned mode) override {
        check(mode==1,"menu projection is orthographic");trace.push_back("mode");
        if(onMode)onMode();
    }
    void setNearPlane(float value) override {++updates;check(value==0.01f,"menu near plane");trace.push_back("set-near");}
    void setFarPlane(float value) override {++updates;check(value==1.0f,"menu far plane");trace.push_back("set-far");}
    void setOrthoWidth(float value) override {++updates;check(value==800.0f,"menu fixed width");trace.push_back("set-width");}
    void setOrthoHeight(float value) override {++updates;check(value==600.0f,"menu fixed height");trace.push_back("set-height");}
    void setViewMatrix(const float* matrix) override {
        ++updates;
        Mat4 expected=Mat4::getIdentity();setRotateXDeg(expected,180.0f);expected.m[14]+=0.02f;
        check(std::memcmp(matrix,expected.m,sizeof(expected.m))==0,"menu rotated and translated view matrix");
        trace.push_back("matrix");
    }
};
struct PaintRenderer : dx9rend::IRenderer {
    dx9rend::IRenderView* previous=nullptr;
    IBase* selected=nullptr;
    bool setRenderView(IBase* view) override {selected=view;trace.push_back("set-view");return false;}
    dx9rend::IRenderView* getRenderView() override {trace.push_back("get-view");return previous;}
    void addRef() override {actionUnsupported("renderer addref");}
    int getRef() override {actionUnsupported("renderer getref");}
    int release() override {actionUnsupported("renderer release");}
    void* queryInterface(unsigned) const override {actionUnsupported("renderer query");}
    bool setDisplayMode(const renddx9abi::HostDisplayMode&) override {actionUnsupported("renderer display mode");}
    bool slot_0x14() override {actionUnsupported("renderer slot14");}
    bool slot_0x18() override {actionUnsupported("renderer slot18");}
    bool initialize() override {actionUnsupported("renderer initialize");}
    void beginFrame() override {actionUnsupported("renderer beginFrame");}
    void update(double) override {actionUnsupported("renderer update");}
    bool renderFrame(double,float) override {actionUnsupported("renderer frame");}
    bool renderScene(double,const dx9rend::RenderFlags*,void*) override {actionUnsupported("renderer scene");}
    bool slot_0x30() override {actionUnsupported("renderer slot30");}
    bool slot_0x34(void*,const dx9rend::RenderFlags*) override {actionUnsupported("renderer slot34");}
    void slot_0x38() override {actionUnsupported("renderer slot38");}
    void* slot_0x3C(const dx9rend::DisplayModeFilter*) override {actionUnsupported("renderer slot3c");}
    bool slot_0x40(unsigned) override {actionUnsupported("renderer slot40");}
    bool setRenderViewDup(IBase*) override {actionUnsupported("renderer duplicate setter");}
    dx9rend::IRenderView* getRenderViewDup() override {actionUnsupported("renderer duplicate getter");}
    unsigned getFrameOrdinal() override {actionUnsupported("renderer frame ordinal");}
    void slot_0x58() override {actionUnsupported("renderer slot58");}
    bool slot_0x5C(const std::string&,void*,bool,bool,unsigned*,unsigned*) override {actionUnsupported("renderer slot5c");}
    void slot_0x60(unsigned,void*,float,float,float,float,bool) override {actionUnsupported("renderer slot60");}
    void slot_0x64(const DemoConfig*) override {actionUnsupported("renderer slot64");}
    void slot_0x68() override {actionUnsupported("renderer slot68");}
    void* getRenderStats() override {actionUnsupported("renderer stats");}
    bool slot_0x70() override {actionUnsupported("renderer slot70");}
    IVideoOptions* getVideoOptions() override {actionUnsupported("renderer video options");}
    void slot_0x78(float) override {actionUnsupported("renderer slot78");}
    float slot_0x7C() override {actionUnsupported("renderer slot7c");}
    void slot_0x80(float) override {actionUnsupported("renderer slot80");}
    float slot_0x84() override {actionUnsupported("renderer slot84");}
    void slot_0x88(float) override {actionUnsupported("renderer slot88");}
    float slot_0x8C() override {actionUnsupported("renderer slot8c");}
    void slot_0x90(bool) override {actionUnsupported("renderer slot90");}
    bool slot_0x94() override {actionUnsupported("renderer slot94");}
    void slot_0x98(bool) override {actionUnsupported("renderer slot98");}
    bool slot_0x9C() override {actionUnsupported("renderer slot9c");}
    double getFrameRate() override {actionUnsupported("renderer frame rate");}
    void setRenderAheadLimit(unsigned) override {actionUnsupported("renderer ahead limit setter");}
    unsigned getRenderAheadLimit() override {actionUnsupported("renderer ahead limit getter");}
    bool isExpansionPack() override {actionUnsupported("renderer expansion mode");}
    unsigned slot_0xB0() override {actionUnsupported("renderer slotb0");}
    void* destroy(unsigned int) override {actionUnsupported("renderer destroy"); return this;}
    void slot_0xB8(bool) override {actionUnsupported("renderer slotb8");}
    bool waitWhileAppInactive() override {actionUnsupported("renderer activation wait");}
    bool isAppInactive() override {actionUnsupported("renderer activation");}
    void refreshShaderDefines() override {actionUnsupported("renderer shader defines");}
    bool beginScene(const unsigned*,char) override {actionUnsupported("renderer beginScene");}
    bool endScene() override {actionUnsupported("renderer endScene");}
    bool present(unsigned) override {actionUnsupported("renderer present");}
    dx9rend::IRenderView* getRenderView2() override {actionUnsupported("renderer second view");}
    void shutdown() override {actionUnsupported("renderer shutdown");}
};
struct PaintDrawer : geom::IQuadDrawer {
    void addRef() override {actionUnsupported("drawer addref");}
    int getRef() override {actionUnsupported("drawer getref");}
    int release() override {actionUnsupported("drawer release");}
    void* queryInterface(unsigned) const override {actionUnsupported("drawer query");}
    void flush() override {trace.push_back("flush");}
    void beginFrame() override {trace.push_back("begin");}
    void resetState() override {trace.push_back("reset");}
    void endFrame() override {trace.push_back("end");}
};
struct PaintClasses : ClassManager {
    PaintDrawer drawer;
    IBase* getSingleton(const std::string& name) override {
        check(name=="QuadDrawer","paint requests quad drawer singleton");
        trace.push_back("drawer");return &drawer;
    }
};
static void paintDispatchChecks(menu::Ref2System& root) {
    PaintClasses classes;classManager=&classes;
    PaintRenderer renderer;
    PaintView first,second,scene;
    renderer.previous=&scene;root.m_renderView=&first;root.m_renderer=&renderer;
    // Swapping the view during the first callback checks that subsequent calls reload it.
    first.onMode=[&]{root.m_renderView=&second;};
    trace.clear();root.beginPaint();
    check(trace==std::vector<std::string>{"mode","set-near","set-far","set-width","set-height","get-view","matrix","set-view","drawer","begin","reset"},"paint setup preserves callback order");
    check(renderer.selected==&second&&root.m_previousRenderView==&scene,"paint borrows previous view and selects replacement view");
    check(first.updates==0&&second.updates==5,"paint reloads the view after the projection-mode callback");
    trace.clear();root.endPaint();
    check(trace==std::vector<std::string>{"drawer","flush","end","set-view"}&&renderer.selected==&scene,"paint flushes before restoring previous view");
    renderer.previous=nullptr;root.m_renderView=&second;
    trace.clear();root.beginPaint();root.endPaint();
    check(!root.m_previousRenderView&&!renderer.selected,"paint preserves a null previous view");
    root.m_renderView=nullptr;root.m_previousRenderView=nullptr;root.m_renderer=nullptr;
    classManager=nullptr;trace.clear();
}
struct ProjectionCamera : world::ICameraObject {
    Mat4 transformation=Mat4::getIdentity();
    void addRef() override {actionUnsupported("camera addref");}
    int getRef() override {actionUnsupported("camera getref");}
    int release() override {actionUnsupported("camera release");}
    void* queryInterface(unsigned) const override {actionUnsupported("camera query");}
    const Mat4& getTransformation(float fraction) override {check(fraction==0.0f,"camera fraction zero");trace.push_back("transform");return transformation;}
    void setInterpolatedMode(bool) override { actionUnsupported("unused projection interface method"); }
    bool setViewMode(CameraViewMode mode, IObject* object, bool) override { actionUnsupported("unused projection interface method"); }
    CameraViewMode getViewMode() const override { actionUnsupported("unused projection interface method"); }
    bool isActive() const override { actionUnsupported("unused projection interface method"); }
    void setIsActive(bool) override { actionUnsupported("unused projection interface method"); }
    void setCameraId(int id) override { actionUnsupported("unused projection interface method"); }
    int  getCameraId() override { actionUnsupported("unused projection interface method"); }
    void prevViewMode() override { actionUnsupported("unused projection interface method"); }
    void setTraceObjectId(unsigned int id) override { actionUnsupported("unused projection interface method"); }
    unsigned int getTraceObjectId() override { actionUnsupported("unused projection interface method"); }
    void setExternCameraTrans(Mat4 transform) override { actionUnsupported("unused projection interface method"); }
    Mat4* getExternCameraTrans() override { actionUnsupported("unused projection interface method"); }
    bool getToggleMouseLook() override { actionUnsupported("unused projection interface method"); }
    int  getPcoId() override { actionUnsupported("unused projection interface method"); }
    void handleExplosion(float) override { actionUnsupported("unused projection interface method"); }
    void resetCameraView(bool) override { actionUnsupported("unused projection interface method"); }
    void updateCameraView(bool) override { actionUnsupported("unused projection interface method"); }
    float getActiveFOV() const override { actionUnsupported("unused projection interface method"); }
    bool getAllowDynamicLights() const override { actionUnsupported("unused projection interface method"); }
    bool getAllowDynamicSun() const override { actionUnsupported("unused projection interface method"); }
    const Vec3& getStaticSunDirection() const override { actionUnsupported("unused projection interface method"); }
    const Vec3& getStaticSunColor() const override { actionUnsupported("unused projection interface method"); }
    const Vec3& getStaticSunDamageColor() const override { actionUnsupported("unused projection interface method"); }
    float getInsideDamageFrequency() const override { actionUnsupported("unused projection interface method"); }
    float getInsideDamageThreshold() const override { actionUnsupported("unused projection interface method"); }
    const Vec3& getStaticSunAmbientColor() const override { actionUnsupported("unused projection interface method"); }
    bool allowCameraModeCycle() override { actionUnsupported("unused projection interface method"); }
    bool isOverheadCamera() const override { actionUnsupported("unused projection interface method"); }
};
struct ProjectionPlayer : Player {
    ProjectionCamera camera;
    world::ICameraObject* getCamera() const override {trace.push_back("camera");return const_cast<ProjectionCamera*>(&camera);}
};
static bool same(Vec3 actual,Vec3 expected) {return actual.x==expected.x&&actual.y==expected.y&&actual.z==expected.z;}
static void inputCopyChecks(menu::Ref2System& root) {
    check(sizeof(io::GameInput)==320,"native GameInput retains 320-byte retail stride");
    io::GameInput input;
    unsigned char* bytes=reinterpret_cast<unsigned char*>(&input);
    for(unsigned i=0;i<sizeof(input);++i)bytes[i]=static_cast<unsigned char>(i*37+11);
    const std::vector<unsigned char> source(bytes,bytes+sizeof(input));
    for(int slot=0;slot<4;++slot)std::memset(&root.m_inputs[slot],0xa0+slot,sizeof(input));
    for(int user:{-1,0,3,4,(std::numeric_limits<int>::min)(),(std::numeric_limits<int>::max)()}) {
        const auto* begin=reinterpret_cast<const unsigned char*>(&root);
        const std::vector<unsigned char> before(begin,begin+sizeof(root));
        root.setInput(input,user);
        if(user<0||user>=4) {
            check(std::memcmp(&root,before.data(),sizeof(root))==0,"out-of-range input user leaves complete real Ref2System unchanged");
        } else {
            check(std::memcmp(&root.m_inputs[user],source.data(),sizeof(input))==0,"setInput copies every byte including flag/alignment/trailing padding");
            const auto offset=reinterpret_cast<const unsigned char*>(&root.m_inputs[user])-begin;
            check(std::memcmp(&root,before.data(),offset)==0&&std::memcmp(begin+offset+sizeof(input),before.data()+offset+sizeof(input),sizeof(root)-offset-sizeof(input))==0,"setInput leaves all other users and adjacent native storage unchanged");
        }
        check(std::memcmp(&input,source.data(),sizeof(input))==0,"setInput preserves complete caller-owned input representation");
    }
}
#ifndef HUD_PROJECTION_FIXTURE_NO_MAIN
// [TEMP] This projection fixture has no HUD information layer to register effects with.
namespace dice { namespace hfe { IBase* g_hudInformationLayer=nullptr; } }
int main() {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(std::getenv("BF2_REF2_PAINT_ONLY")) {
        menu::Ref2System root(800,600);
        paintDispatchChecks(root);
        std::printf("Ref2 paint dispatch: %u/%u checks passed\n",checks-failures,checks);
        return failures?1:0;
    }
    Players players;world::playerManager=&players;ProjectionPlayer player;
    auto* hud=new Hud;hudManager=hud;
    check(same(hud->screenToWorld({5,6}),{0,0,0})&&trace.empty(),"screen missing UI skips player query");
    check(same(hud->worldToScreen({5,6,7}),{0,0,0})&&trace.empty(),"world missing UI skips player query");
    menu::Ref2System root(800,600);hud->ref2System=&root;
    inputCopyChecks(root);
    paintDispatchChecks(root);
    ProjectionView ui,scene;root.m_renderView=&ui;root.m_previousRenderView=&scene;
    check(root.getPreviousRenderView()==&scene,"Ref2 previous-view getter");
    check(same(hud->screenToWorld({5,6}),{0,0,0})&&trace==std::vector<std::string>{"player"},"screen missing player skips views");
    players.local=&player;trace.clear();
    check(same(hud->screenToWorld({400,-300}),{1,1,0.5f}),"identity unprojection preserves near plane");
    check(trace==std::vector<std::string>{"player","width","height","near","projection","camera","transform"},"screen view/camera call order");
    trace.clear();check(same(hud->worldToScreen({1,1,2}),{200,-150,0.5f}),"world projects through transformed Z");
    check(trace==std::vector<std::string>{"player","camera","transform","projection","width","height"},"world camera/projection/view call order");
    player.camera.transformation.m[0]=2;player.camera.transformation.m[5]=4;player.camera.transformation.m[10]=4;
    player.camera.transformation.m[12]=10;player.camera.transformation.m[13]=20;player.camera.transformation.m[14]=30;
    scene.projection=Mat4::getZero();scene.projection.m[0]=2;scene.projection.m[5]=4;
    scene.projection.m[10]=5;scene.projection.m[11]=1;scene.projection.m[14]=3;
    check(same(hud->screenToWorld({400,-300}),{11,21,34}),"general perspective inverse then scaled translated camera");
    check(same(hud->worldToScreen({11,21,34}),{50,-37.5f,0.125f}),"general inverse of non-orthonormal camera");
    player.camera.transformation=Mat4::getIdentity();scene.projection=Mat4::getIdentity();
    check(same(hud->worldToScreen({1,2,0}),{0,0,-1}),"zero depth hidden");
    check(same(hud->worldToScreen({1,2,-2}),{0,0,-1}),"negative depth hidden");
    check(same(hud->worldToScreen({1,2,std::numeric_limits<float>::quiet_NaN()}),{0,0,-1}),"unordered depth hidden");
    scene.onProjection=[&]{player.camera.transformation.m[12]=100;};
    check(same(hud->worldToScreen({1,1,2}),{200,-150,0.5f}),"world inverse snapshot precedes projection callback");
    player.camera.transformation=Mat4::getIdentity();
    check(same(hud->screenToWorld({400,-300}),{101,1,0.5f}),"screen camera read follows projection callback");
    scene.onProjection=nullptr;player.camera.transformation=Mat4::getIdentity();
    unsigned oldControl,ignoredControl;
    _controlfp_s(&oldControl,0,0);_controlfp_s(&ignoredControl,_PC_64,_MCW_PC);
    scene.nearPlane=1;
    for(int cancellation=0;cancellation<2;++cancellation) {
        Mat4 mixed=Mat4::getZero();mixed.m[15]=1;
        for(int component=0;component<3;++component) {
            if(cancellation==0) {
                mixed.m[4+component]=16777216.0f;mixed.m[8+component]=1;mixed.m[component]=-16777216.0f;
            } else {
                mixed.m[4+component]=std::ldexp(1.0f,80);mixed.m[8+component]=-std::ldexp(1.0f,80);mixed.m[component]=1;
            }
        }
        scene.projection=Mat4::getIdentity();player.camera.transformation=mixed;
        check(same(hud->screenToWorld({400,-300}),{1,1,1}),"screen camera affine preserves unspilled x87 accumulation and Y/Z/X addition order");
        scene.projection=mixed;player.camera.transformation=Mat4::getIdentity();
        check(same(hud->worldToScreen({1,1,1}),{400,-300,1}),"world projection affine preserves unspilled x87 accumulation and Y/Z/X addition order");
    }
    _controlfp_s(&ignoredControl,oldControl,_MCW_PC);
    scene.projection=Mat4::getIdentity();scene.nearPlane=0.5f;
    ui.width=0;Vec3 singular=hud->screenToWorld({1,1});check(!std::isfinite(singular.x),"zero screen width has no invented guard");
    ui.width=800;scene.projection=Mat4::getZero();
    singular=hud->screenToWorld({1,1});check(!std::isfinite(singular.x)&&!std::isfinite(singular.y)&&!std::isfinite(singular.z),"singular projection preserves inverse nonfinite results");
    // HUD is retained to process exit; its outer lifetime is still unresolved.
    // Ref2System is really destroyed; UI initialization is outside this fixture.
    std::printf("HUD projection: %u/%u checks passed\n",checks-failures,checks);
    return failures?1:0;
}
#endif
