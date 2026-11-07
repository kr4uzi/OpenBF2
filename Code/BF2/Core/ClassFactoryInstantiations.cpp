// [CHOSEN, not recovered] Explicit factory instantiations for available concrete classes.
// Factory bodies require complete types and all reachable virtual definitions. Keep publication
// scoped to classes that meet that requirement. Move instantiations into their owning module
// translation units when those units are reconstructed, then remove this convenience file.

#include "../Game/Objects/TemplateExtenders.h"
#include "ClassFactory.h"

#include "../Game/Objects/Base/BundleTemplate.h"
#include "../Game/Objects/Base/SimpleObjectTemplate.h"
#include "../Game/Objects/CameraTemplate.h"
#include "../Game/Objects/CompTemplateBases.h"
#include "../Game/Objects/CompTemplateLeaves.h"
#include "../Game/Objects/ControlPointTemplate.h"
#include "../Game/Objects/ObjectSpawnerTemplate.h"   // Split out of ControlPointTemplate.h
#include "../Game/Objects/Fx/ParticleSystemTemplates.h"
#include "../Game/Objects/Fx/SpriteParticle.h"
#include "../Game/Objects/Game/KitTemplate.h"
#include "../Game/Objects/MaskObjectTemplate.h"
#include "../Game/Objects/HudGLCompTemplates.h"
#include "../Game/Objects/GameLogicComp/ArmorGLCompTemplate.h"   // ArmorGLCompTemplate
#include "../Game/Objects/PlayerControlObjectTemplate.h"
#include "../Game/Objects/Projectiles/ProjectileCompTemplates.h"
#include "../Game/Objects/RotationalBundleTemplate.h"
#include "../Game/Objects/TemplateLeaves.h"
#include "../Sound/SoundObjectTemplate.h"   // Dice::hfe:: (not world::) - added
#include "../Game/Objects/Projectiles/GenericProjectileTemplate.h"   // Added
#include "../Game/Objects/Visual/LightSourceTemplate.h"   // Attested path - added
#include "../Game/Objects/Fx/EffectBundleTemplate.h"   // + EffectProperties base - added
#include "../Game/Objects/Fx/DecalTemplate.h"   // Added
#include "../Game/Objects/Fx/ParticleTemplate.h"   // Added
#include "../Game/Objects/Fx/AirDraftEffectBundleTemplate.h"   // Added
#include "../Game/Objects/Fx/WheelEffectBundleTemplate.h"   // Added
#include "../Game/Objects/EngineTemplate.h"                 // Added
#include "../Game/Objects/LandingGearTemplate.h"            // Added
#include "../Game/Objects/RotationalBundleLeaves.h"         // Added
#include "../Game/Objects/RemoteControlledObjectTemplate.h"  // Added

// Explicit instantiation emits ClassFactory<T>::create for each complete, unambiguous T below.
template struct dice::hfe::world::ClassFactory<dice::hfe::world::AbilityGLCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ArmorGLCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::AbilityHudCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::AmbientEffectAreaTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::AreaObjectTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::BundleTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::CameraTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ControlPointTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DefaultAmmoCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DefaultAnimationCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DefaultCollisionCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DefaultFollowCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DefaultPenetrateCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DefaultProjectileSoundCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DefaultRicochetCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DefaultZoomCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DestroyableWindowsBundleTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DestroyableWindowTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DropVehicleTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DynamicBundleTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::EmitterTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::EntryPointTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::EnvMapTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::FixedTargetCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::GroundEffectBundleTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::HelpHudCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::HookLinkTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::IkTemplateComponent>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::KitTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::KitPartTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::SpriteParticleTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::MaskObjectTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::LadderTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::LPFireCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::LPTargetCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::MultiFireArmTargetCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::MultiFireCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ObjectSpawnerTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ObstacleTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::PlayerControlObjectTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::PostProcessCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::PreDestinedFollowCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::RadioGLCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ReplenishCollisionCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ReplenishingAmmoCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ResurrectCollisionCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::RopeLinkTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::RotationalBundleTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::RotorTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::SeekClosestTargetCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::SimpleDeviationCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::SimpleObjectTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::SimpleTargetCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::SoldierBasedRecoilCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::SpringTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::StickyCollisionCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::StrategicObjectCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::SupplyDepotTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::TargetDeviationCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ThrownAnimationCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ThrownFireCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::TriggerableTargetTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::TriggerableTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::TriggerTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::UAVVehicleTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::VehicleHudCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::WarningHudCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::WeaponBasedRecoilCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::WeaponHudCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::WindAffectorTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ZiplineRopeTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ZiplineTemplate>;

// Factories for concrete object and component templates.
template struct dice::hfe::world::ClassFactory<dice::hfe::world::AnimatedBundleTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::AntennaObjectTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DefaultDetonationCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DefaultSoundCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DestroyableObjectTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ExplosivesFireCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ForceObjectTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::FreeCameraTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::GenericFireArmTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::GrapplingHookRopeTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ItemContainerTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ItemTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::OverheadCameraTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::RemoteControlledObjectTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ReplenishDetonationCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::SingleFireCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::SoldierDeviationCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::SoldierTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::SpawnObjectFireCompTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::SupplyObjectTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::TargetObjectTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ToggleCameraCompTemplate>;

// Sound and ladder-container template factories.
template struct dice::hfe::ClassFactory<dice::hfe::SoundObjectTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::LadderContainerTemplate>;

template struct dice::hfe::world::ClassFactory<dice::hfe::world::GenericProjectileTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::LightSourceTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::EffectBundleTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::DecalTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ParticleTemplate>;
// Particle templates must be registered in the same order on client and server to keep template IDs aligned.
template struct dice::hfe::world::ClassFactory<dice::hfe::world::SpriteParticleSystemTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::MeshParticleSystemTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::NonScreenAlignedParticleSystemTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::TrailSystemTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ParticleSystemEmitterTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::AirDraftEffectBundleTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::WheelEffectBundleTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::EngineTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::LandingGearTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::WingTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::TurnableRemoteControlledObjectTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ParachuteTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::ZiplineContainerTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::GrapplingHookRopeContainerTemplate>;
template struct dice::hfe::world::ClassFactory<dice::hfe::world::FloatingBundleTemplate>;
