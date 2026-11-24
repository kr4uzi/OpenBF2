# Air-draft effect bundle

`Game/Objects/Fx/AirDraftEffectBundle.cpp` is attested by the client string at
`0x00900200`. This is recovered client behavior on every rebuilt client platform;
the implementation is compiled under `!DEDICATED`. The runtime dedicated guard
remains in the factory. No protocol, content format or dedicated behavior changes.

Evidence was read directly with LLVM objdump and PE/ELF section readers.
The BF2 1.50 x86 client SHA-256 is
`2f08d8fced27d8512fc0d0681c33a0eb066947af08d156b191744cc89f4ac257`.
The dedicated PE factory `0x005FE490` and ELF `0x081F5180` return null.
The symbolized Linux server ELF also contains the runtime class, although its factory
returns null: constructor `0x081F39F0`, update helper `0x081F4300`, table
`0x088A8FE0`. Its names cross-check the client instructions; its differing code
does not replace the client implementation.

| Client body | Address |
| --- | --- |
| Constructor, allocation size 0x1D0 | 0x00646400 |
| Destructor / deleting wrapper | 0x00646120 / 0x00646510 |
| init, forwarding to SimpleObject::init | 0x005D3DB0 -> 0x00628090 |
| disableCachedObject | 0x00645F10 |
| destroy | 0x00646030 |
| getEngine | 0x00645920 |
| stopEffect | 0x006459C0 |
| handleMessage | 0x00645A00 |
| handleUpdate | 0x00645A50 |
| updateEffectProperties | 0x00645AB0 |
| setCurrentMaterial | 0x006461A0 |
| calculateCurrentDistanceAndEffectPosition | 0x00646540 |
| Template createObject | 0x00646880 |
| handleVisualUpdate | 0x006468B0 |

The typed members retain the constructor's order and ownership. The map owns its
nodes; disable/destroy destroy its values before clearing it, while the destructor
does not destroy values. The engine is borrowed, the current effect aliases the
map, the sound holds a reference, and the fixed sound node is independently owned.
The untouched extent at client +0x168 (ELF +0x150) remains explicitly unresolved;
no source-level type or meaning is claimed. Native STL/pointer sizes are used,
without asserting that the retail allocation size is a native layout.

The ray uses the absolute transform and `startDistance + 5`. The object query's
return value is ignored. Terrain is queried even after an object hit and can
overwrite the shared hit point/normal. The selected material remains the object's
when present and not -1, otherwise the terrain's. Ordered distance <= startDistance
accepts equality and rejects NaN. The terrain material remains an output-only local,
as shipped; no default is fabricated for an object-material -1 plus terrain-miss case.
`maxDistance` and `engineInfluence` are not read by this runtime. The fields are
disambiguated by the template writer `0x00645D80`: it receives the secondary
IConsoleSaveable pointer at primary +8, so its maxDistance load +0x388 is primary
+0x390, while startDistance +0x384 is primary +0x38C. The runtime's unadjusted
template read +0x38C and ELF read +0x2C8 both select startDistance.

Material changes stop and disable the old effect, then reactivate a cached effect
or create one from the material cell. The lifetime diagnostic reads
`EffectBundleTemplate::m_dist2E0.value` (client +0x394); it is not the sampled lifetime.
Sound setup uses the existing typed `SoundObjectTemplate`/`ISoundObjectTemplate`,
`IBfSound`, `ISound` and `ObjectSoundNodeFixed` interfaces. The latter's client
table `0x008F1A3C` targets `0x007D72E0` (absolute transform) and `0x0080D430`
(null velocity). The owned node follows the selected effect, not the aircraft.
Sound volume and pitch both read the bundle's intensity field.

The visual callback renews an unsigned expiry at renderer frame ordinal +130;
the shared `getCurrentFrame` helper `0x006447B0` already adds 100. Expiry is strict
`currentFrame > expiryFrame`. Enter/exit messages 4/5 control flag 0x1000 only for
non-recursive calls. A stopped engine immediately stops both particle and sound
effects. Properties use Speed, Altitude and Engine, in that order, and multiply
range width by Engine, Altitude, Speed before adding the range minimum. Sound's
clamp `0x00402EE0` maps unordered input to zero. This follows the other recovered
effect code's float expression and ordered-comparison conventions.

The client inherits an empty `initNetworkableComp` target (`0x0083D270`, bare ret).
The current shared SimpleObject implementation has the nonempty ELF body, so this
client class supplies the proven empty override without changing the shared base.
Its existing native base ABI, including inherited declarations, is otherwise retained.

Primary table `0x00900060` follows. All targets equal the client's SimpleObject
table `0x008FC4C0` except destructor, init, disable, destroy, visual update, update
and message handling. Names are cross-checked against the ELF table, accounting
for its extra destructor entry. The native implementation uses typed inheritance,
not this table's byte offsets.

| PE slot | Client target | Identity |
| --- | --- | --- |
| +0x000 | 0x005C5CB0 | Object::addRef() |
| +0x004 | 0x005D1610 | Object::getRef() |
| +0x008 | 0x005C5CC0 | Object::release() |
| +0x00C | 0x00546770 | Object::queryInterface(unsigned int) const |
| +0x010 | 0x005466D0 | Object::getClassID() const |
| +0x014 | 0x00646510 | AirDraftEffectBundle::~AirDraftEffectBundle() |
| +0x018 | 0x00720890 | IObject::getWeakPtr() |
| +0x01C | 0x005D3DB0 | AirDraftEffectBundle::init() |
| +0x020 | 0x00645F10 | AirDraftEffectBundle::disableCachedObject() |
| +0x024 | 0x00628130 | SimpleObject::enableCachedObject() |
| +0x028 | 0x00433140 | IObject::getObjectCacheID() |
| +0x02C | 0x0071D4E0 | Object::setObjectCacheID(unsigned int) |
| +0x030 | 0x00646030 | AirDraftEffectBundle::destroy(bool) |
| +0x034 | 0x005466A0 | IObject::forceDestroy() |
| +0x038 | 0x005466E0 | Object::isObjectDestroyed() |
| +0x03C | 0x005466F0 | Object::setObjectInDestroyList(bool) |
| +0x040 | 0x00546700 | Object::getObjectInDestroyList() const |
| +0x044 | 0x006CBA10 | IObject::setGridCullerObject(CullerObject*) |
| +0x048 | 0x00689460 | IObject::getGridCullerObject() const |
| +0x04C | 0x006D8770 | IObject::setLatencyCullerObject(CullerObject*) |
| +0x050 | 0x00689490 | IObject::getLatencyCullerObject() const |
| +0x054 | 0x007201D0 | Object::setComponent(unsigned int, IBase*) |
| +0x058 | 0x0071F3B0 | Object::queryComponent(unsigned int, unsigned int) |
| +0x05C | 0x0071D680 | Object::updateFlags(unsigned int, unsigned int) |
| +0x060 | 0x0071D520 | Object::getName() const |
| +0x064 | 0x0071D530 | Object::setName(std::string const&) |
| +0x068 | 0x0071DB10 | Object::getAbsolutePosition() const |
| +0x06C | 0x0071E530 | Object::setAbsolutePosition(Vec3 const&) |
| +0x070 | 0x00682C50 | Object::getAbsolutePositionSecondary() const |
| +0x074 | 0x0071E5B0 | Object::setAbsolutePositionSecondary(Vec3 const&) |
| +0x078 | 0x00682C60 | Object::getRotationSecondary() |
| +0x07C | 0x005468E0 | Object::setRotationSecondary(Vec3 const&) |
| +0x080 | 0x0071DB50 | Object::getAbsoluteTransformation() const |
| +0x084 | 0x0071E7C0 | Object::getInvAbsoluteTransformation() const |
| +0x088 | 0x0071E5E0 | Object::setAbsoluteTransformation(Mat4 const&) |
| +0x08C | 0x0071DB90 | Object::getVisualAbsoluteTransformation() const |
| +0x090 | 0x0071EBB0 | Object::getVisualBoundingRadius() const |
| +0x094 | 0x0071D8B0 | Object::getPhysicBoundingRadius() const |
| +0x098 | 0x007D3950 | Object::getBoundingRadiusModifier() const |
| +0x09C | 0x0071E820 | Object::getFullBoundingBox() |
| +0x0A0 | 0x0071E890 | Object::getMineBoundingBox() |
| +0x0A4 | 0x006468B0 | AirDraftEffectBundle::handleVisualUpdate(float, float) |
| +0x0A8 | 0x005AB2A0 | Object::handleFrameUpdate(float) |
| +0x0AC | 0x00645A50 | AirDraftEffectBundle::handleUpdate(float, unsigned int) |
| +0x0B0 | 0x00627F30 | SimpleObject::handleCollision(IObject*, Vec3 const&, Vec3 const&, Vec3 const&, int, int, CollisionFaceInfo*, CollisionFaceInfo*) |
| +0x0B4 | 0x0071D600 | Object::updateGeometry() |
| +0x0B8 | 0x00751A30 | Object::getUpdateFrequency() const |
| +0x0BC | 0x005C1670 | Object::getUpdateFrequencyType() const |
| +0x0C0 | 0x0063F8F0 | Object::setUpdateFrequencyType(UpdateFreqencyType) |
| +0x0C4 | 0x006273D0 | SimpleObject::getGeometryPart() const |
| +0x0C8 | 0x00720770 | IObject::hasCollisionLod(int) const |
| +0x0CC | 0x00546760 | SimpleObject::getMaterial() const |
| +0x0D0 | 0x0071D570 | Object::SetActiveLod(ObjectLodType) |
| +0x0D4 | 0x005466C0 | IObject::getNetworkable() const |
| +0x0D8 | 0x00504C60 | SimpleObject::getDecalObject() const |
| +0x0DC | 0x0071DAD0 | Object::getRelativePosition() const |
| +0x0E0 | 0x0071E6A0 | Object::setRelativePosition(Vec3 const&) |
| +0x0E4 | 0x0071DAF0 | Object::getRelativeTransformation() const |
| +0x0E8 | 0x0071E730 | Object::setRelativeTransformation(Mat4 const&) |
| +0x0EC | 0x006CFB80 | Object::getChild() const |
| +0x0F0 | 0x0071ED00 | Object::addChild(IObject*) |
| +0x0F4 | 0x0071DA00 | Object::removeChild(IObject*) |
| +0x0F8 | 0x0071D9C0 | Object::resetCachedRootParents() |
| +0x0FC | 0x005B74B0 | Object::handlePlayerInput(IPlayer*, io::PlayerInput const&, float, unsigned int) |
| +0x100 | 0x00645A00 | AirDraftEffectBundle::handleMessage(TemplateMessage, IPlayer*, bool) |
| +0x104 | 0x0071DBB0 | Object::getInputId() const |
| +0x108 | 0x0071DBA0 | Object::setInputId(char) |
| +0x10C | 0x0071DBC0 | Object::getChildId() const |
| +0x110 | 0x0071DBD0 | Object::setChildId(char) |
| +0x114 | 0x00546710 | Object::getAlpha() |
| +0x118 | 0x00546720 | Object::getLayer() |
| +0x11C | 0x005464C0 | Object::setLayer(unsigned char) |
| +0x120 | 0x00546730 | Object::getGroup() |
| +0x124 | 0x00546740 | Object::setGroup(unsigned char) |
| +0x128 | 0x0071DC00 | Object::setIsOvergrowth(bool) |
| +0x12C | 0x0071DC10 | Object::getIsOvergrowth() |
| +0x130 | 0x0071DC20 | Object::setNotInAI(bool) |
| +0x134 | 0x00504BF0 | Object::getNotInAI() const |
| +0x138 | 0x0071DD00 | Object::setLightSourceMask(int) |
| +0x13C | 0x0071DD20 | Object::getLightSourceMask() const |
| +0x140 | 0x00546750 | Object::getDisableChildren() |
| +0x144 | 0x0071DD40 | Object::setDisableChildren(bool) |
| +0x148 | 0x006273F0 | SimpleObject::getArmorInfo() |
| +0x14C | 0x005AB2B0 | IObject::isCreatedByLocalPlayer() const |
| +0x150 | 0x007207E0 | IObject::internal_assignNewObjectID() |
| +0x154 | 0x00720760 | IObject::destroyObject() |
| +0x158 | 0x0071DAB0 | Object::addSibling(IObject*) |
| +0x15C | 0x0071EE00 | Object::removeSibling(IObject*) |
| +0x160 | 0x0071E3D0 | Object::setParent(IObject*) |
| +0x164 | 0x0071DBE0 | Object::makeScript(io::IStream*) |
| +0x168 | 0x0071DBF0 | Object::getLayerId() const |
| +0x16C | 0x0071D660 | Object::setLayerId(char) |
| +0x170 | 0x00627EF0 | Object::updateFlagsRecursive(unsigned int, unsigned int) |
| +0x174 | 0x0083D270 | SimpleObject::initNetworkableComp() |

The secondary `IConsoleSaveable` table at `0x008EDFB8`, shared with SimpleObject,
has +0x00 `0x005C6050` addRef, +0x04 `0x005C6060` getRef, +0x08 `0x005C6070`
release, +0x0C `0x005F1DB0` queryInterface, and +0x10 `0x0071E900` makeScript.
The compiler supplies the native secondary-base adjustments.

Stock BF2 content was read directly from `Objects_client.zip` and
`Objects_server.zip` in memory, without extracting or changing files. Both define
`ade_heliDraft` under `Effects/vehicles/ADE/ade_heliDraft/`: material 62,
maxDistance 10, startDistance 20, engineInfluence 0.85. Its effect properties use
altitude and engine intensity. Aircraft references are AH-1Z (`ahe_ah1z`),
Mi-28 (`ahe_havoc`), Z-10 (`ahe_z10`), Z-8 (`chthe_z8`), Mi-17 (`the_mi17`),
UH-60 (`usthe_uh60`), and F-35B (`air_f35b`). The six helicopters attach it to
their main rotor; the F-35B attaches it under its helicopter engine.
`Common_client.zip/Material/materialManagerSettings.con` assigns material 62's
cells to `e_ade_helidraft_generic`, `_water`, `_Sand`, `_Gravel`, `_Grass`, `_Dirt`,
`_mud` and `_Leaf`. The default sound is `S_AirDraft`, with `S_WaterDraft` on water;
the existing material manager falls back to cell (62, 0) for missing cell entries.

`Code/Tests/air_draft_effect_runtime.py <macos-build-dir>` compiles the actual new
class, factory and EffectProperties against typed fixture observers. Add
`--sanitize` for AddressSanitizer. The fixture exercises query order, material
cache selection, hit position, distance/NaN gates, alignment dispatch, properties,
expiry and lifecycle ownership. It proves those boundaries, not particle rendering,
audio playback or gameplay. Fresh effect/sound creation with real managers, real
engine/physics traversal, all aircraft, Windows/Linux and native arm64 remain
runtime validation work. No new named frontier is introduced.
