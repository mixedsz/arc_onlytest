#pragma once
#include "Platform.hpp"

namespace Offsets
{
    namespace Steam
    {


        constexpr uintptr_t UViewPort_Uworld = 0x140;           // GameViewportClient::World CL-1372005
        constexpr uintptr_t UViewPort_GameInstance = 0x1A8;    // GameViewportClient::GameInstance
        constexpr uintptr_t UViewPort_ViewIndex = 0x0;         // unused; prior 0x140 is now World


        // CL-1372005 / buildid 25163933 (2026-09-09). Steam ArcOffsets dump + live pins.
        constexpr uintptr_t UGameInstance_LocalPlayers = 0x38;   // GameInstance::LocalPlayers CL-1372005


        constexpr uintptr_t UWorld_PersistentLevel = 0x30;      // World::PersistentLevel CL-1372005
        constexpr uintptr_t UWorld_Levels = 0x328;               // World::Levels CL-1372005
        constexpr uintptr_t UWorld_LevelCollections = 0x3A8;     // World::LevelCollections CL-1372005 (0x240 is TimeSeconds)
        constexpr uintptr_t UWorld_GameState = 0x0;
        constexpr uintptr_t UWorld_OwningGameInstance = 0x1A8;   // World::OwningGameInstance


        constexpr uintptr_t FLevelCollection_GameState = 0x8;


        constexpr uintptr_t AGameState_PlayerArray = 0x4B8;      // GameStateBase::PlayerArray CL-1372005
        constexpr uintptr_t AGameState_StageInfo = 0x818;        // PioneerGameState::StageInfo CL-1372005


        constexpr uintptr_t StageInfo_TimeLeft = 0x18;
        constexpr uintptr_t StageInfo_GraceTime = 0x20;


        // ULevel::Actors is not a UPROPERTY so the dump does not name it. Live-pinned
        // CL-1341255: Actors 0x108 / count 0x110, OwningWorld 0x130.
        constexpr uintptr_t ULevel_ActorArray = 0x110;           // live-pinned CL-1372005
        constexpr uintptr_t ULevel_WorldSettings = 0x318;        // UNVERIFIED +0x8 with ULevel OwningWorld/Actors
        constexpr uintptr_t ULevel_OwningWorld = 0x138;          // Level::OwningWorld CL-1372005


        constexpr uintptr_t UWorldSettings_WorldGravity = 0x420;


        constexpr uintptr_t ULocalPlayer_PlayerController = 0x30;


        constexpr uintptr_t UPlayerController_AcknowledgedPawn = 0x418; // CL-1372005
        constexpr uintptr_t UPlayerController_ControlRotation = 0x438; // PC ns; Controller ns lists 0x450 — FLAGGED conflict
        // sdk_wanted CL-1341255 APlayerController. Ack is often still 0 at spawn;
        // PlayerState / Character / PlayerCameraManager identify the PC without it.
        constexpr uintptr_t UPlayerController_PlayerState = 0x408; // CL-1372005
        constexpr uintptr_t UPlayerController_Character = 0x420; // Controller::Character (PC dump wrongly aliases PlayerState 0x408)
        constexpr uintptr_t UPlayerController_PlayerCameraManager = 0x4E0; // FLAGGED: dump marks WRONG (CheatClass); hold pending live pin


        constexpr uintptr_t CameraManager_DefaultFOV = 0x3F0;    // PlayerCameraManager::DefaultFOV CL-1372005
        // DefaultFOV+4, matching Xbox (0x3C0/0x3C4). The previous 0x42C accepted
        // writes and never affected anything on screen; 0x3EC reads back 0.00,
        // which is what an unlocked LockedFov holds.
        constexpr uintptr_t CameraManager_LockedFov = 0x3F4;     // CL-1372005 — RE-VERIFY LIVE
        constexpr uintptr_t CameraManager_ViewTarget = 0xD60;    // PlayerCameraManager::ViewTarget CL-1372005


        constexpr uintptr_t FTViewTarget_POV = 0x10;             // TViewTarget::POV


        // CL-1372005: Location +0x8 into FMinimalViewInfo (PCM+0xD78 = VT 0xD60 + POV 0x10 + Loc 0x8).
        // Rotation +0x30, FOV +0x50.
        constexpr uintptr_t FMinimalViewInfo_Location = 0x8;     // MinimalViewInfo::Location CL-1372005
        constexpr uintptr_t FMinimalViewInfo_Rotation = 0x30;    // MinimalViewInfo::Rotation
        constexpr uintptr_t FMinimalViewInfo_FOV = 0x50;         // MinimalViewInfo::FOV CL-1372005


        constexpr uintptr_t UObject_RootComponent = 0x250;       // AActor::RootComponent CL-1372005
        constexpr uintptr_t UObject_Owner = 0x1E0;               // AActor::Owner CL-1372005


        constexpr uintptr_t UObject_UIHoverData = 0x0;


        constexpr uintptr_t UObject_InteractionComponent = 0xB58;


        constexpr uintptr_t UObject_IsDestroyed = 0x1210;


        // Every USceneComponent field moved by exactly +0x18 in this patch, which is what
        // makes the derived ComponentToWorld below trustworthy despite not being dumped:
        // RelativeLocation 0x248 -> 0x260, ComponentVelocity 0x290 -> 0x2A8, AttachChildren
        // 0x1D0 -> 0x1E8. RelativeLocation itself is inferred from RelativeRotation 0x278.
        constexpr uintptr_t USceneComponent_RelativeLocation = 0x260;
        constexpr uintptr_t USceneComponent_ComponentVelocity = 0x2F0;  // SceneComponent::ComponentVelocity CL-1372005
        constexpr uintptr_t USceneComponent_AttachChildren = 0x238;     // SceneComponent::AttachChildren CL-1372005
        // 0x370, from two independent reports: one gives the rotation and translation as a
        // pair at 0x370 and 0x390, which is self-consistent because translation sits 0x20
        // into an FTransform, behind the four-double quaternion. The previous 0x208 came from
        // a CTW_Translation report of 0x228 and it read a plausible position, which is
        // exactly why it went unnoticed - GetSceneComponentWorldLocation returned early and
        // the pawn appeared in the right place. Only the skeleton exposed it: reading the
        // transform 0x168 bytes early takes the scale from unrelated memory, and every bone
        // is multiplied by it, so the pose came out roughly 100x too large and every player
        // was rejected on coherence before a skeleton could ever be drawn.
        constexpr uintptr_t USceneComponent_ComponentToWorld = 0x350; // CL-1372005 live-verified


        constexpr uintptr_t UPawn_PlayerState = 0x3F0;            // APawn::PlayerState CL-1372005
        constexpr uintptr_t UPawn_Controller = 0x400;            // APawn::Controller CL-1372005
        constexpr uintptr_t UPawn_Mesh = 0x450;                  // ACharacter::Mesh CL-1372005


        constexpr uintptr_t UPawn_HealthComponent = 0xDD8;
        constexpr uintptr_t UPawn_InventoryComponent = 0xC90;    // PioneerPlayerCharacter CL-1372005
        constexpr uintptr_t UPawn_FlashlightComponent = 0xDF0;   // PioneerPlayerCharacter CL-1372005


        constexpr uintptr_t UFlashlightComponent_FlashlightColor = 0x548;


        constexpr uintptr_t UPlayerState_PlayerName = 0x468;     // CL-1372005
        constexpr uintptr_t UPlayerState_PawnPrivate = 0x448;    // CL-1372005

        // APlayerState::UniqueID — FUniqueNetIdRepl, dump size 48, UniqueID at 968.
        constexpr uintptr_t UPlayerState_UniqueId = 0x3E0;       // APlayerState::UniqueID CL-1372005
        // Dump EmbarkPlayerStateBase::PlatformIdComponent is 0x488. Live Squad is dump+0x18
        // (0x460 -> 0x478), so the component sits at 0x4A0 on this build.
        constexpr uintptr_t UPlayerState_PlatformIdComponent = 0x4C0; // CL-1372005
        // Dump 0x168. Live [PlatDiag] always classified at dump-0x18 (same shift as Squad).
        constexpr uintptr_t UEmbarkPlatformIdComponent_PlatformId = 0x188; // CL-1372005


        constexpr uintptr_t UPlayerState_Squad = 0x498;          // CL-1372005

        // Offset into EmbarkSquad, not PlayerState. Dump Title is FText at 0x398.
        constexpr uintptr_t UPlayerState_SquadName = 0x398;

        // FPlayerHealthInfo does not exist. The Dumper-7 dump of this build contains no
        // struct by that name anywhere in 750,000 lines, and 0x540 lands inside
        // PioneerPlayerState::RespawnInfo (0x538). That is the whole explanation for
        // [HiScan] reporting hiOff=0x0 on all 699 attempts - it was hunting for something
        // the game does not have. Health comes from the health component instead; these
        // stay only so the probe in SlowCache.hpp keeps compiling while it is retired.
        constexpr uintptr_t UPlayerState_HealthInfo = 0x588;     // HEALTH_INFO_BASE dump (still unused path)
        constexpr uintptr_t FPlayerHealthInfo_Health = 0x00;
        constexpr uintptr_t FPlayerHealthInfo_MaxHealth = 0x08;
        constexpr uintptr_t FPlayerHealthInfo_Armor = 0x10;
        constexpr uintptr_t FPlayerHealthInfo_MaxArmor = 0x18;
        constexpr uintptr_t FPlayerHealthInfo_bIsDbno = 0x21;

        // PioneerPlayerState::PlayerStatus - a uint8_t with OnRep_PlayerStatus, confirmed in
        // the dump at exactly the offset [DbnoScan] found empirically by diffing 1536 bytes
        // across a healthy-to-downed transition. It is a status enum rather than a bool, so
        // the validation in SlowCache.hpp treats any non-zero value as downed instead of
        // rejecting values above 1.
        constexpr uintptr_t UPlayerState_IsDbno = 0x568;         // PlayerStatus CL-1372005
        // PioneerPlayerState::bFinishedRound / CurrentPawn — dump PlayerState, CL-1341255.
        // Extracted PlayerArray entries keep a frozen pawn; this flag is what drops them.
        constexpr uintptr_t UPlayerState_FinishedRound = 0x5D0;  // CL-1372005
        constexpr uintptr_t UPlayerState_CurrentPawn = 0x588;    // CL-1372005


        // HealthComponent::CachedHealth. Armor is a nested ArmorSlot at +0x140, whose
        // Health is at +0x0 and CurrentMaxArmor at +0x10, giving the absolute values below.
        // HealthComponent has no MaxHealth UPROPERTY at all - it lives on the GAS attribute
        // set - so the existing probe list in SlowCache.hpp still carries the search.
        constexpr uintptr_t UHealthComponent_CurrentArmor = 0x190; // ArmorSlot::Health CL-1372005
        constexpr uintptr_t UHealthComponent_MaxArmor = 0x198;     // ArmorSlot::CurrentMaxArmor CL-1372005
        constexpr uintptr_t UHealthComponent_CurrentHealth = 0x6D0; // CachedHealth CL-1372005
        constexpr uintptr_t UHealthComponent_MaxHealth = 0x308;     // double, plaintext


        // 0x4F0 CurrentItemActors / 0x4D0 LocalCurrentItems — CL-1372005 dump.
        // LocalCurrentItems is the fallback when the primary array is empty.
        constexpr uintptr_t UInventoryComponent_CurrentItemActors = 0x4F0; // CL-1372005
        constexpr uintptr_t UInventoryComponent_LocalCurrentItems = 0x4D0; // CL-1372005
        // sdk_wanted InventoryComponent. Used when both item-actor TArrays are empty
        // (InvDiag src=local itemsCount=0 on armed pawns).
        constexpr uintptr_t UInventoryComponent_EquippedPrimaryItem = 0x500;
        constexpr uintptr_t UInventoryComponent_StowedWeapon0 = 0x370; // CL-1372005
        constexpr uintptr_t UInventoryComponent_StowedToolActor = 0x3F0; // CL-1372005
        // Sub-offsets into InventoryStowedWeaponActor (struct size 0x40). Layout
        // from StowedWeaponLayout: StowedWeaponActor 0x10, ItemDataAsset 0x18,
        // QualityVisual 0x20, WeaponQuality 0x38.
        constexpr uintptr_t UInventoryComponent_StowedActorInLayout = 0x10;
        constexpr uintptr_t UInventoryComponent_StowedItemDataAsset = 0x18; // StowedWeaponLayout CL-1372005
        constexpr uintptr_t UInventoryComponent_StowedWeaponQuality = 0x38;  // int32


        constexpr uintptr_t UItemBase_ProjectileOverride = 0x5E8;


        // WeaponActor, CL-1341255 per _dump/sdk_offsets.txt. The 0x470/0x472 pair
        // comes from _dump/ClassesInfo.json.gz, which is the PRIOR build: that dump
        // also reports LandscapeProxy 0x468/0x478/0x70C, the values sdk_offsets.txt
        // lists as superseded by 0x480/0x490/0x724. Reading quality at 0x472 lands on
        // an unrelated byte, so WeaponQualityToLevel fell through to "" every frame.
        constexpr uintptr_t UItemActor_WeaponQuality = 0x4A2;
        // STALE: VisualizedWeaponMods has no CL-1341255 value in any dump artifact.
        // Prior build had 0x488. Zero is handled by the reader; nothing consumes mods
        // since weapon pricing was removed.
        constexpr uintptr_t UItemActor_VisualizedMods = 0x0;
        constexpr uintptr_t UItemActor_WeaponClip = 0x4A0;   // uint16 ClipSize, currently unread

        // Dump EmbarkSquad: Title FText 0x398, Seed int32 0x3C8.
        constexpr uintptr_t UEmbarkSquad_Title = 0x398;
        constexpr uintptr_t UEmbarkSquad_Seed = 0x3C8;


        constexpr uintptr_t UProjectileMovement_InitialSpeed = 0x1B8;
        constexpr uintptr_t UProjectileMovement_MaxSpeed = 0x1BC;
        constexpr uintptr_t UProjectileMovement_ProjectileGravity = 0x1E8;


        constexpr uintptr_t AProjectile_ProjectileMovement = 0xB38;


        constexpr uintptr_t UInteractionComponent_IsOpened = 0x8D8;


        constexpr uintptr_t UIHoverData_LootAmount = 0x18;
        constexpr uintptr_t UIHoverData_DataAsset = 0x20;


        constexpr uintptr_t UItemDataAsset_ItemCategory = 0x2A8;
        constexpr uintptr_t UItemDataAsset_Weight = 0x298;


        constexpr uintptr_t USkeletalMeshComponent_BoundsScale = 0x4B8;
        constexpr uintptr_t USkeletalMeshComponent_AnimScriptInstance = 0xB88; // CL-1372005
        // USkinnedMeshComponent::bRecentlyRendered. UC 590414 (spacedev99 / qwe900) and
        // dump post 3237: VisibilityBasedAnimTickOption 0xA54, bRecentlyRendered 0xA57.
        // uint8, engine decays ~1s. LastSubmitTime / LastRenderTimeOnScreen flicker on this patch.
        constexpr uintptr_t USkeletalMeshComponent_bRecentlyRendered = 0xA57;


        constexpr uintptr_t UAnimInstance_CrossLayerAnimationPropertiesRef = 0x3B90;


        constexpr uintptr_t UCrossLayerAnimationProperties_EquippedWeaponAmmoCount = 0x674;
        constexpr uintptr_t UCrossLayerAnimationProperties_bIsZooming = 0x678;
        constexpr uintptr_t UCrossLayerAnimationProperties_bIsSprinting = 0x141;
        constexpr uintptr_t UCrossLayerAnimationProperties_bIsDodgeRolling = 0x142;
        constexpr uintptr_t UCrossLayerAnimationProperties_bIsInJump = 0x14A;
        constexpr uintptr_t UCrossLayerAnimationProperties_CharacterStance = 0x610;
        constexpr uintptr_t UCrossLayerAnimationProperties_bIsReloading = 0x671;
        constexpr uintptr_t UCrossLayerAnimationProperties_bIsFiring = 0x66A;
        // CrossLayerAnimationProperties dump (CL-1341255). bIsFiring / bIsReloading
        // stay set through fire-end / reload recovery, so activity tags use these
        // alongside the bools. ELocoMode at 0x170: Still=0 Walk=1 Run=2 Sprint=3.
        // ELoopingReloadState at 0x672: NotActive=0 Entry=1 Loop=2 Exit=3.
        constexpr uintptr_t UCrossLayerAnimationProperties_LocoMode = 0x170;
        constexpr uintptr_t UCrossLayerAnimationProperties_LoopingReloadState = 0x672;
        constexpr uintptr_t UCrossLayerAnimationProperties_FireWeaponCount = 0x66C;
        constexpr uintptr_t UCrossLayerAnimationProperties_FireState = 0x6C0;
        constexpr uintptr_t UCrossLayerAnimationProperties_TimeSinceFire = 0x6F0;
        constexpr uintptr_t UCrossLayerAnimationProperties_GameTimeSeconds = 0xF0;
        constexpr uintptr_t UCrossLayerAnimationProperties_StartReloadTimeStamp = 0x780;
        constexpr uintptr_t UCrossLayerAnimationProperties_WeaponReloadTime = 0x788;
        constexpr uintptr_t UCrossLayerAnimationProperties_TimeSpentReloadingScaled = 0xAB8;
        constexpr uintptr_t UCrossLayerAnimationProperties_bIsTryingToSprint = 0x458;
        constexpr uintptr_t UCrossLayerAnimationProperties_bInSprintState = 0x23A4;
        constexpr uintptr_t UCrossLayerAnimationProperties_CurrentReloadActionIndex = 0x7A4;
        // UNVERIFIED — derived from +0x10 class delta, pending probe confirmation.
        // ClassesInfo.json puts bWasRecentlyRendered at 0x11B, but that dump is a
        // different build: GameTimeSeconds, bIsZooming, bIsFiring and TimeSinceFire
        // all sit exactly 0x10 higher there than the values in use here, so 0x11B-0x10.
        // Occupies a whole byte (the class stores one bool per byte, 0x108..0x10C),
        // so there is no bit mask to guess.
        //
        // Fallback if this proves wrong: USkinnedMeshComponent::bRecentlyRendered at
        // 0x997 (VISIBILITY_BASED_ANIM_TICK_OPTION 0x994 plus the 3-byte gap the dump
        // shows), bit 5 / mask 0x20 — but that bit index is inferred from declaration
        // order, since the dumper emits no UBoolProperty field mask.
        constexpr uintptr_t UCrossLayerAnimationProperties_bWasRecentlyRendered = 0x10B;


        constexpr uintptr_t ASalvageExtractionPointBase_bCanExtract = 0xB38;
        constexpr uintptr_t ASalvageExtractionPointBase_ExtractionDataAsset = 0xB40;
        constexpr uintptr_t ASalvageExtractionPointBase_State = 0xBD2; // CL-1372005
        constexpr uintptr_t ASalvageExtractionPointBase_bIsEnabled = 0xBD0; // CL-1372005


        constexpr uintptr_t ASalvageExtractionPointBase_TimeLeftAfterShutdown = 0xC38; // CL-1372005


        constexpr uintptr_t UExtractionDataAsset_DialogueTag = 0xD8;


        constexpr uintptr_t FGameplayTag_TagName = 0x0;


        constexpr uintptr_t UStaticMeshActor_StaticMeshComponent = 0x398;


        constexpr uintptr_t UStaticMeshComponent_StaticMesh = 0x728; // CL-1372005


        constexpr uintptr_t UStaticMesh_BodySetup = 0x1F0; // CL-1372005
        constexpr uintptr_t UStaticMesh_ExtendedBounds = 0x300;


        constexpr uintptr_t UBodySetup_AggGeom = 0xB0;


        constexpr uintptr_t CameraManager_PCOwner = 0x3E0;  // PlayerCameraManager::PCOwner CL-1372005


        constexpr uintptr_t UPlayerController_bIsLocalPlayerController = 0xD14;



        constexpr uintptr_t Arc_HealthService = 0x1288;
        constexpr uintptr_t Arc_HealthService_DamageDataStore = 0x148;
        constexpr uintptr_t Arc_DDS_ArrayData = 0xA8;
        constexpr uintptr_t Arc_DDS_ArrayNum = 0xB0;
        constexpr uintptr_t Arc_DDS_ElementStride = 0x70;
        constexpr uintptr_t Arc_DDS_Element_CurrentHealthUNorm = 0x34;
        constexpr uintptr_t Arc_ActorHealthComponent = 0x5E0;
        constexpr uintptr_t Arc_ActorHealthComponent_MaxHealth = 0x168;
        constexpr uintptr_t Arc_HealthGroupService = 0x12B0;
        constexpr uintptr_t Arc_HGS_BaseGroup = 0x168;
        constexpr uintptr_t Arc_BaseGroup_PartIDs_SlotsNum = 0x0;
        constexpr uintptr_t Arc_BaseGroup_PartIDs_NumFreeSlots = 0x4;
        constexpr uintptr_t Arc_BaseGroup_NumOfDestroyedParts = 0x8;



    }

    namespace Xbox
    {
        // STALE vs Steam CL-1372005 / buildid 25163933 — Xbox dump required for same CL.


        constexpr uintptr_t UViewPort_Uworld = 0x140;
        constexpr uintptr_t UViewPort_GameInstance = 0x1A8;
        constexpr uintptr_t UViewPort_ViewIndex = 0x210;


        constexpr uintptr_t UGameInstance_LocalPlayers = 0xA0;


        constexpr uintptr_t UWorld_PersistentLevel = 0x120;
        constexpr uintptr_t UWorld_Levels = 0x268;
        constexpr uintptr_t UWorld_LevelCollections = 0x2E0;
        constexpr uintptr_t UWorld_GameState = 0x0;   // unknown on Xbox; falls back to LevelCollections
        constexpr uintptr_t UWorld_OwningGameInstance = 0x1A8;   // unknown on Xbox


        constexpr uintptr_t FLevelCollection_GameState = 0x8;


        constexpr uintptr_t AGameState_PlayerArray = 0x3F0;
        constexpr uintptr_t AGameState_StageInfo = 0x808;


        constexpr uintptr_t StageInfo_TimeLeft = 0x18;
        constexpr uintptr_t StageInfo_GraceTime = 0x20;


        constexpr uintptr_t ULevel_OwningWorld = 0x0;   // unknown on Xbox
        constexpr uintptr_t ULevel_ActorArray = 0xC0;
        constexpr uintptr_t ULevel_WorldSettings = 0x2C8;


        constexpr uintptr_t UWorldSettings_WorldGravity = 0x3D8;


        constexpr uintptr_t ULocalPlayer_PlayerController = 0x30;


        constexpr uintptr_t UPlayerController_AcknowledgedPawn = 0x390;
        constexpr uintptr_t UPlayerController_ControlRotation = 0x3C8;


        constexpr uintptr_t CameraManager_DefaultFOV = 0x3C0;
        constexpr uintptr_t CameraManager_LockedFov = 0x3C4;
        constexpr uintptr_t CameraManager_ViewTarget = 0x440;


        constexpr uintptr_t FTViewTarget_POV = 0x10;


        constexpr uintptr_t FMinimalViewInfo_Location = 0x10;
        constexpr uintptr_t FMinimalViewInfo_Rotation = 0x38;
        constexpr uintptr_t FMinimalViewInfo_FOV = 0x60;


        constexpr uintptr_t UObject_RootComponent = 0x1D8;


        constexpr uintptr_t UObject_UIHoverData = 0x0;


        constexpr uintptr_t UObject_InteractionComponent = 0xB58;


        constexpr uintptr_t UObject_IsDestroyed = 0x11C0;


        constexpr uintptr_t USceneComponent_RelativeLocation = 0x1F8;
        constexpr uintptr_t USceneComponent_ComponentVelocity = 0x240;
        constexpr uintptr_t USceneComponent_AttachChildren = 0x180;
        constexpr uintptr_t USceneComponent_ComponentToWorld = 0x310;


        constexpr uintptr_t UPawn_PlayerState = 0x370;
        constexpr uintptr_t UPawn_Mesh = 0x3D8;


        constexpr uintptr_t UPawn_HealthComponent = 0xD78;
        constexpr uintptr_t UPawn_InventoryComponent = 0xC50;
        constexpr uintptr_t UPawn_FlashlightComponent = 0xD98;


        constexpr uintptr_t UFlashlightComponent_FlashlightColor = 0x4F8;


        constexpr uintptr_t UPlayerState_PlayerName = 0x3E8;
        constexpr uintptr_t UPlayerState_PawnPrivate = 0x3C8;

        // Steam UniqueID/PlatformIdComponent minus the 0x48 PlayerState delta.
        constexpr uintptr_t UPlayerState_UniqueId = 0x380;
        constexpr uintptr_t UPlayerState_PlatformIdComponent = 0x458;
        // Dump 0x168. Live Steam component uses 0x150; Xbox keeps the same inner layout.
        constexpr uintptr_t UEmbarkPlatformIdComponent_PlatformId = 0x150;


        constexpr uintptr_t UPlayerState_Squad = 0x418;


        constexpr uintptr_t UPlayerState_SquadName = 0x350;

        constexpr uintptr_t UPlayerState_HealthInfo = 0x4F8;
        constexpr uintptr_t FPlayerHealthInfo_Health = 0x00;
        constexpr uintptr_t FPlayerHealthInfo_MaxHealth = 0x08;
        constexpr uintptr_t FPlayerHealthInfo_Armor = 0x10;
        constexpr uintptr_t FPlayerHealthInfo_MaxArmor = 0x18;
        constexpr uintptr_t FPlayerHealthInfo_bIsDbno = 0x21;

        // Inferred, not observed. Every PlayerState field here sits exactly 0x48 below its
        // Steam counterpart (PlayerName 0x3E8/0x430, HealthInfo 0x4F8/0x540), so the Steam
        // 0x510 maps to 0x4C8. The runtime validation in SlowCache.hpp will simply refuse
        // to trust it if that assumption does not hold on Xbox.
        constexpr uintptr_t UPlayerState_IsDbno = 0x4C8;
        constexpr uintptr_t UPlayerState_FinishedRound = 0x0;
        constexpr uintptr_t UPlayerState_CurrentPawn = 0x0;


        constexpr uintptr_t UHealthComponent_CurrentArmor = 0x150;
        constexpr uintptr_t UHealthComponent_MaxArmor = 0x158;
        constexpr uintptr_t UHealthComponent_CurrentHealth = 0x668;
        constexpr uintptr_t UHealthComponent_MaxHealth = 0x308;


        // Xbox is not touched by the Steam patch. LocalCurrentItems and WeaponClip have no
        // reported Xbox values, so they stay zero and their call sites skip them.
        constexpr uintptr_t UInventoryComponent_CurrentItemActors = 0x4B0;
        constexpr uintptr_t UInventoryComponent_LocalCurrentItems = 0x0;
        constexpr uintptr_t UInventoryComponent_EquippedPrimaryItem = 0x0;
        constexpr uintptr_t UInventoryComponent_StowedWeapon0 = 0x0;
        constexpr uintptr_t UInventoryComponent_StowedToolActor = 0x0;
        constexpr uintptr_t UInventoryComponent_StowedActorInLayout = 0x0;
        constexpr uintptr_t UInventoryComponent_StowedItemDataAsset = 0x0;
        // UNVERIFIED: struct-internal offset, does not shift with class layout, so the
        // Steam value should hold. Inert anyway while StowedWeapon0 is 0 on Xbox.
        constexpr uintptr_t UInventoryComponent_StowedWeaponQuality = 0x38;  // int32


        constexpr uintptr_t UItemBase_ProjectileOverride = 0x5A8;


        // STALE: no Xbox dump exists for CL-1341255 — _dump/sdk_offsets.txt is
        // Steam-only. These are carried forward unverified; Steam moved WeaponQuality
        // 0x472 -> 0x4A2 this build, so these are very likely wrong too.
        constexpr uintptr_t UItemActor_WeaponQuality = 0x422;
        constexpr uintptr_t UItemActor_VisualizedMods = 0x438;
        constexpr uintptr_t UItemActor_WeaponClip = 0x0;


        constexpr uintptr_t UProjectileMovement_InitialSpeed = 0x1B8;
        constexpr uintptr_t UProjectileMovement_MaxSpeed = 0x1BC;
        constexpr uintptr_t UProjectileMovement_ProjectileGravity = 0x1E8;


        constexpr uintptr_t AProjectile_ProjectileMovement = 0xB38;


        constexpr uintptr_t UInteractionComponent_IsOpened = 0x850;


        constexpr uintptr_t UIHoverData_LootAmount = 0x18;
        constexpr uintptr_t UIHoverData_DataAsset = 0x20;


        constexpr uintptr_t UItemDataAsset_ItemCategory = 0x268;
        constexpr uintptr_t UItemDataAsset_Weight = 0x258;


        constexpr uintptr_t USkeletalMeshComponent_BoundsScale = 0x458;
        constexpr uintptr_t USkeletalMeshComponent_AnimScriptInstance = 0xA68;
        // UNVERIFIED — Steam 0xA57 minus AnimScriptInstance delta (0xAD8-0xA68=0x70).
        constexpr uintptr_t USkeletalMeshComponent_bRecentlyRendered = 0x9E7;


        constexpr uintptr_t UAnimInstance_CrossLayerAnimationPropertiesRef = 0x3B50;


        constexpr uintptr_t UCrossLayerAnimationProperties_EquippedWeaponAmmoCount = 0x634;
        constexpr uintptr_t UCrossLayerAnimationProperties_bIsZooming = 0x638;
        constexpr uintptr_t UCrossLayerAnimationProperties_bIsSprinting = 0x101;
        constexpr uintptr_t UCrossLayerAnimationProperties_bIsDodgeRolling = 0x102;
        constexpr uintptr_t UCrossLayerAnimationProperties_bIsInJump = 0x10A;
        constexpr uintptr_t UCrossLayerAnimationProperties_CharacterStance = 0x5D0;
        constexpr uintptr_t UCrossLayerAnimationProperties_bIsReloading = 0x631;
        constexpr uintptr_t UCrossLayerAnimationProperties_bIsFiring = 0x62A;
        constexpr uintptr_t UCrossLayerAnimationProperties_LocoMode = 0x130;
        constexpr uintptr_t UCrossLayerAnimationProperties_LoopingReloadState = 0x632;
        constexpr uintptr_t UCrossLayerAnimationProperties_FireWeaponCount = 0x62C;
        constexpr uintptr_t UCrossLayerAnimationProperties_FireState = 0x680;
        constexpr uintptr_t UCrossLayerAnimationProperties_TimeSinceFire = 0x6B0;
        constexpr uintptr_t UCrossLayerAnimationProperties_GameTimeSeconds = 0xB0;
        constexpr uintptr_t UCrossLayerAnimationProperties_StartReloadTimeStamp = 0x740;
        constexpr uintptr_t UCrossLayerAnimationProperties_WeaponReloadTime = 0x748;
        constexpr uintptr_t UCrossLayerAnimationProperties_TimeSpentReloadingScaled = 0xA78;
        constexpr uintptr_t UCrossLayerAnimationProperties_bIsTryingToSprint = 0x418;
        constexpr uintptr_t UCrossLayerAnimationProperties_bInSprintState = 0x2364;
        constexpr uintptr_t UCrossLayerAnimationProperties_CurrentReloadActionIndex = 0x764;
        // UNVERIFIED — extrapolated, not dumped. Xbox has no visibility path at all
        // today (Xbox.hpp has no render-time constants and WorldCache gates the whole
        // feature off this platform), so nothing consumes this yet. Steam 0x10B minus
        // the uniform 0x40 this class runs below Steam (GameTimeSeconds 0xB0 vs 0xF0,
        // bIsZooming 0x638 vs 0x678). Confirm before wiring Xbox visibility.
        constexpr uintptr_t UCrossLayerAnimationProperties_bWasRecentlyRendered = 0xCB;


        constexpr uintptr_t ASalvageExtractionPointBase_bCanExtract = 0xB38;
        constexpr uintptr_t ASalvageExtractionPointBase_ExtractionDataAsset = 0xB40;
        constexpr uintptr_t ASalvageExtractionPointBase_State = 0xBCA;
        constexpr uintptr_t ASalvageExtractionPointBase_bIsEnabled = 0xBC8;


        constexpr uintptr_t ASalvageExtractionPointBase_TimeLeftAfterShutdown = 0xC28;


        constexpr uintptr_t UExtractionDataAsset_DialogueTag = 0x98;


        constexpr uintptr_t FGameplayTag_TagName = 0x0;


        constexpr uintptr_t UStaticMeshActor_StaticMeshComponent = 0x350;


        constexpr uintptr_t UStaticMeshComponent_StaticMesh = 0x6B8;


        constexpr uintptr_t UBodySetup_AggGeom = 0x70;


        constexpr uintptr_t UStaticMesh_BodySetup = 0x1A8;
        constexpr uintptr_t UStaticMesh_ExtendedBounds = 0x2C0;


        constexpr uintptr_t CameraManager_PCOwner = 0x3A8;  // Xbox - not touched by the Steam patch


        constexpr uintptr_t UPlayerController_bIsLocalPlayerController = 0xC5C;



        constexpr uintptr_t Arc_HealthService = 0x1238;
        constexpr uintptr_t Arc_HealthService_DamageDataStore = 0x160;
        constexpr uintptr_t Arc_DDS_ArrayData = 0x68;
        constexpr uintptr_t Arc_DDS_ArrayNum = 0x70;
        constexpr uintptr_t Arc_DDS_ElementStride = 0x70;
        constexpr uintptr_t Arc_DDS_Element_CurrentHealthUNorm = 0x34;
        constexpr uintptr_t Arc_ActorHealthComponent = 0x590;
        constexpr uintptr_t Arc_ActorHealthComponent_MaxHealth = 0x0188;
        constexpr uintptr_t Arc_HealthGroupService = 0x1260;
        constexpr uintptr_t Arc_HGS_BaseGroup = 0x188;
        constexpr uintptr_t Arc_BaseGroup_PartIDs_SlotsNum = 0x0;
        constexpr uintptr_t Arc_BaseGroup_PartIDs_NumFreeSlots = 0x4;
        constexpr uintptr_t Arc_BaseGroup_NumOfDestroyedParts = 0x8;

    }

    struct Values
    {
        struct { uintptr_t Uworld, GameInstance, ViewIndex; } UViewPort;
        struct { uintptr_t LocalPlayers; } UGameInstance;
        struct { uintptr_t PersistentLevel, Levels, LevelCollections, GameState, OwningGameInstance; } UWorld;
        struct { uintptr_t GameState; } FLevelCollection;
        struct { uintptr_t PlayerArray, StageInfo; } AGameState;
        struct { uintptr_t TimeLeft, GraceTime; } StageInfo;
        struct { uintptr_t ActorArray, WorldSettings, OwningWorld; } ULevel;
        struct { uintptr_t WorldGravity; } UWorldSettings;
        struct { uintptr_t PlayerController; } ULocalPlayer;
        struct { uintptr_t AcknowledgedPawn, ControlRotation; } UPlayerController;
        struct { uintptr_t PCOwner, DefaultFOV, LockedFov, ViewTarget; } CameraManager;
        struct { uintptr_t bIsLocalPlayerController; } PlayerControllerFlags;
        struct { uintptr_t POV; } FTViewTarget;
        struct { uintptr_t Location, Rotation, FOV; } FMinimalViewInfo;
        struct { uintptr_t RootComponent, UIHoverData, InteractionComponent, IsDestroyed; } UObject;
        struct { uintptr_t RelativeLocation, ComponentVelocity, ComponentToWorld, AttachChildren; } USceneComponent;
        struct { uintptr_t StaticMeshComponent; } UStaticMeshActor;
        struct { uintptr_t StaticMesh; } UStaticMeshComponent;
        struct { uintptr_t BodySetup, ExtendedBounds; } UStaticMesh;
        struct { uintptr_t AggGeom; } UBodySetup;
        struct { uintptr_t PlayerState, Mesh, HealthComponent, InventoryComponent, FlashlightComponent; } UPawn;
        struct { uintptr_t FlashlightColor; } UFlashlightComponent;
        struct { uintptr_t PlayerName, Squad, SquadName, PawnPrivate, HealthInfo, IsDbno, FinishedRound, CurrentPawn; } UPlayerState;
        struct { uintptr_t Health, MaxHealth, Armor, MaxArmor, bIsDbno; } FPlayerHealthInfo;
        struct { uintptr_t CurrentArmor, MaxArmor, CurrentHealth, MaxHealth; } UHealthComponent;
        struct { uintptr_t CurrentItemActors, LocalCurrentItems, EquippedPrimaryItem, StowedWeapon0, StowedToolActor, StowedActorInLayout, StowedItemDataAsset, StowedWeaponQuality; } UInventoryComponent;
        struct { uintptr_t ProjectileOverride; } UItemBase;
        struct { uintptr_t WeaponQuality, VisualizedMods, WeaponClip; } UItemActor;
        struct { uintptr_t InitialSpeed, MaxSpeed, ProjectileGravity; } UProjectileMovement;
        struct { uintptr_t ProjectileMovement; } AProjectile;
        struct { uintptr_t IsOpened; } UInteractionComponent;
        struct { uintptr_t LootAmount, DataAsset; } UIHoverData;
        struct { uintptr_t ItemCategory, Weight; } UItemDataAsset;
        struct { uintptr_t BoundsScale, AnimScriptInstance, bRecentlyRendered; } USkeletalMeshComponent;
        struct { uintptr_t CrossLayerAnimationPropertiesRef; } UAnimInstance;
        struct {
            uintptr_t EquippedWeaponAmmoCount, bIsZooming, bIsSprinting, bIsDodgeRolling, bIsInJump,
                CharacterStance, bIsReloading, bIsFiring, LocoMode, LoopingReloadState,
                FireWeaponCount, FireState, TimeSinceFire, GameTimeSeconds,
                StartReloadTimeStamp, WeaponReloadTime, TimeSpentReloadingScaled,
                bIsTryingToSprint, bInSprintState, CurrentReloadActionIndex,
                bWasRecentlyRendered;
        } UCrossLayerAnimationProperties;
        struct { uintptr_t bCanExtract, bIsEnabled, State, ExtractionDataAsset, TimeLeftAfterShutdown; } ASalvageExtractionPointBase;
        struct { uintptr_t DialogueTag; } UExtractionDataAsset;
        struct { uintptr_t TagName; } FGameplayTag;
        struct { uintptr_t HealthService, HealthService_DamageDataStore, DDS_ArrayData, DDS_ArrayNum, DDS_ElementStride, DDS_Element_CurrentHealthUNorm, ActorHealthComponent, ActorHealthComponent_MaxHealth, HealthGroupService, HGS_BaseGroup, BaseGroup_PartIDs_SlotsNum, BaseGroup_PartIDs_NumFreeSlots, BaseGroup_NumOfDestroyedParts; } Arc;
    };

    inline const Values SteamValues = {
        { Steam::UViewPort_Uworld, Steam::UViewPort_GameInstance, Steam::UViewPort_ViewIndex },
        { Steam::UGameInstance_LocalPlayers },
        { Steam::UWorld_PersistentLevel, Steam::UWorld_Levels, Steam::UWorld_LevelCollections, Steam::UWorld_GameState, Steam::UWorld_OwningGameInstance },
        { Steam::FLevelCollection_GameState },
        { Steam::AGameState_PlayerArray, Steam::AGameState_StageInfo },
        { Steam::StageInfo_TimeLeft, Steam::StageInfo_GraceTime },
        { Steam::ULevel_ActorArray, Steam::ULevel_WorldSettings, Steam::ULevel_OwningWorld },
        { Steam::UWorldSettings_WorldGravity },
        { Steam::ULocalPlayer_PlayerController },
        { Steam::UPlayerController_AcknowledgedPawn, Steam::UPlayerController_ControlRotation },
        { Steam::CameraManager_PCOwner, Steam::CameraManager_DefaultFOV, Steam::CameraManager_LockedFov, Steam::CameraManager_ViewTarget },
        { Steam::UPlayerController_bIsLocalPlayerController },
        { Steam::FTViewTarget_POV },
        { Steam::FMinimalViewInfo_Location, Steam::FMinimalViewInfo_Rotation, Steam::FMinimalViewInfo_FOV },
        { Steam::UObject_RootComponent, Steam::UObject_UIHoverData, Steam::UObject_InteractionComponent, Steam::UObject_IsDestroyed },
        { Steam::USceneComponent_RelativeLocation, Steam::USceneComponent_ComponentVelocity, Steam::USceneComponent_ComponentToWorld, Steam::USceneComponent_AttachChildren },
        { Steam::UStaticMeshActor_StaticMeshComponent },
        { Steam::UStaticMeshComponent_StaticMesh },
        { Steam::UStaticMesh_BodySetup, Steam::UStaticMesh_ExtendedBounds },
        { Steam::UBodySetup_AggGeom },
        { Steam::UPawn_PlayerState, Steam::UPawn_Mesh, Steam::UPawn_HealthComponent, Steam::UPawn_InventoryComponent, Steam::UPawn_FlashlightComponent },
        { Steam::UFlashlightComponent_FlashlightColor },
        { Steam::UPlayerState_PlayerName, Steam::UPlayerState_Squad, Steam::UPlayerState_SquadName, Steam::UPlayerState_PawnPrivate, Steam::UPlayerState_HealthInfo, Steam::UPlayerState_IsDbno, Steam::UPlayerState_FinishedRound, Steam::UPlayerState_CurrentPawn },
        { Steam::FPlayerHealthInfo_Health, Steam::FPlayerHealthInfo_MaxHealth, Steam::FPlayerHealthInfo_Armor, Steam::FPlayerHealthInfo_MaxArmor, Steam::FPlayerHealthInfo_bIsDbno },
        { Steam::UHealthComponent_CurrentArmor, Steam::UHealthComponent_MaxArmor, Steam::UHealthComponent_CurrentHealth, Steam::UHealthComponent_MaxHealth },
        { Steam::UInventoryComponent_CurrentItemActors, Steam::UInventoryComponent_LocalCurrentItems, Steam::UInventoryComponent_EquippedPrimaryItem, Steam::UInventoryComponent_StowedWeapon0, Steam::UInventoryComponent_StowedToolActor, Steam::UInventoryComponent_StowedActorInLayout, Steam::UInventoryComponent_StowedItemDataAsset, Steam::UInventoryComponent_StowedWeaponQuality },
        { Steam::UItemBase_ProjectileOverride },
        { Steam::UItemActor_WeaponQuality, Steam::UItemActor_VisualizedMods, Steam::UItemActor_WeaponClip },
        { Steam::UProjectileMovement_InitialSpeed, Steam::UProjectileMovement_MaxSpeed, Steam::UProjectileMovement_ProjectileGravity },
        { Steam::AProjectile_ProjectileMovement },
        { Steam::UInteractionComponent_IsOpened },
        { Steam::UIHoverData_LootAmount, Steam::UIHoverData_DataAsset },
        { Steam::UItemDataAsset_ItemCategory, Steam::UItemDataAsset_Weight },
        { Steam::USkeletalMeshComponent_BoundsScale, Steam::USkeletalMeshComponent_AnimScriptInstance, Steam::USkeletalMeshComponent_bRecentlyRendered },
        { Steam::UAnimInstance_CrossLayerAnimationPropertiesRef },
        { Steam::UCrossLayerAnimationProperties_EquippedWeaponAmmoCount, Steam::UCrossLayerAnimationProperties_bIsZooming, Steam::UCrossLayerAnimationProperties_bIsSprinting, Steam::UCrossLayerAnimationProperties_bIsDodgeRolling, Steam::UCrossLayerAnimationProperties_bIsInJump, Steam::UCrossLayerAnimationProperties_CharacterStance, Steam::UCrossLayerAnimationProperties_bIsReloading, Steam::UCrossLayerAnimationProperties_bIsFiring, Steam::UCrossLayerAnimationProperties_LocoMode, Steam::UCrossLayerAnimationProperties_LoopingReloadState, Steam::UCrossLayerAnimationProperties_FireWeaponCount, Steam::UCrossLayerAnimationProperties_FireState, Steam::UCrossLayerAnimationProperties_TimeSinceFire, Steam::UCrossLayerAnimationProperties_GameTimeSeconds, Steam::UCrossLayerAnimationProperties_StartReloadTimeStamp, Steam::UCrossLayerAnimationProperties_WeaponReloadTime, Steam::UCrossLayerAnimationProperties_TimeSpentReloadingScaled, Steam::UCrossLayerAnimationProperties_bIsTryingToSprint, Steam::UCrossLayerAnimationProperties_bInSprintState, Steam::UCrossLayerAnimationProperties_CurrentReloadActionIndex, Steam::UCrossLayerAnimationProperties_bWasRecentlyRendered },
        { Steam::ASalvageExtractionPointBase_bCanExtract, Steam::ASalvageExtractionPointBase_bIsEnabled, Steam::ASalvageExtractionPointBase_State, Steam::ASalvageExtractionPointBase_ExtractionDataAsset, Steam::ASalvageExtractionPointBase_TimeLeftAfterShutdown },
        { Steam::UExtractionDataAsset_DialogueTag },
        { Steam::FGameplayTag_TagName },
        { Steam::Arc_HealthService, Steam::Arc_HealthService_DamageDataStore, Steam::Arc_DDS_ArrayData, Steam::Arc_DDS_ArrayNum, Steam::Arc_DDS_ElementStride, Steam::Arc_DDS_Element_CurrentHealthUNorm, Steam::Arc_ActorHealthComponent, Steam::Arc_ActorHealthComponent_MaxHealth, Steam::Arc_HealthGroupService, Steam::Arc_HGS_BaseGroup, Steam::Arc_BaseGroup_PartIDs_SlotsNum, Steam::Arc_BaseGroup_PartIDs_NumFreeSlots, Steam::Arc_BaseGroup_NumOfDestroyedParts }
    };

    inline const Values XboxValues = {
        { Xbox::UViewPort_Uworld, Xbox::UViewPort_GameInstance, Xbox::UViewPort_ViewIndex },
        { Xbox::UGameInstance_LocalPlayers },
        { Xbox::UWorld_PersistentLevel, Xbox::UWorld_Levels, Xbox::UWorld_LevelCollections, Xbox::UWorld_GameState, Xbox::UWorld_OwningGameInstance },
        { Xbox::FLevelCollection_GameState },
        { Xbox::AGameState_PlayerArray, Xbox::AGameState_StageInfo },
        { Xbox::StageInfo_TimeLeft, Xbox::StageInfo_GraceTime },
        { Xbox::ULevel_ActorArray, Xbox::ULevel_WorldSettings, Xbox::ULevel_OwningWorld },
        { Xbox::UWorldSettings_WorldGravity },
        { Xbox::ULocalPlayer_PlayerController },
        { Xbox::UPlayerController_AcknowledgedPawn, Xbox::UPlayerController_ControlRotation },
        { Xbox::CameraManager_PCOwner, Xbox::CameraManager_DefaultFOV, Xbox::CameraManager_LockedFov, Xbox::CameraManager_ViewTarget },
        { Xbox::UPlayerController_bIsLocalPlayerController },
        { Xbox::FTViewTarget_POV },
        { Xbox::FMinimalViewInfo_Location, Xbox::FMinimalViewInfo_Rotation, Xbox::FMinimalViewInfo_FOV },
        { Xbox::UObject_RootComponent, Xbox::UObject_UIHoverData, Xbox::UObject_InteractionComponent, Xbox::UObject_IsDestroyed },
        { Xbox::USceneComponent_RelativeLocation, Xbox::USceneComponent_ComponentVelocity, Xbox::USceneComponent_ComponentToWorld, Xbox::USceneComponent_AttachChildren },
        { Xbox::UStaticMeshActor_StaticMeshComponent },
        { Xbox::UStaticMeshComponent_StaticMesh },
        { Xbox::UStaticMesh_BodySetup, Xbox::UStaticMesh_ExtendedBounds },
        { Xbox::UBodySetup_AggGeom },
        { Xbox::UPawn_PlayerState, Xbox::UPawn_Mesh, Xbox::UPawn_HealthComponent, Xbox::UPawn_InventoryComponent, Xbox::UPawn_FlashlightComponent },
        { Xbox::UFlashlightComponent_FlashlightColor },
        { Xbox::UPlayerState_PlayerName, Xbox::UPlayerState_Squad, Xbox::UPlayerState_SquadName, Xbox::UPlayerState_PawnPrivate, Xbox::UPlayerState_HealthInfo, Xbox::UPlayerState_IsDbno, Xbox::UPlayerState_FinishedRound, Xbox::UPlayerState_CurrentPawn },
        { Xbox::FPlayerHealthInfo_Health, Xbox::FPlayerHealthInfo_MaxHealth, Xbox::FPlayerHealthInfo_Armor, Xbox::FPlayerHealthInfo_MaxArmor, Xbox::FPlayerHealthInfo_bIsDbno },
        { Xbox::UHealthComponent_CurrentArmor, Xbox::UHealthComponent_MaxArmor, Xbox::UHealthComponent_CurrentHealth, Xbox::UHealthComponent_MaxHealth },
        { Xbox::UInventoryComponent_CurrentItemActors, Xbox::UInventoryComponent_LocalCurrentItems, Xbox::UInventoryComponent_EquippedPrimaryItem, Xbox::UInventoryComponent_StowedWeapon0, Xbox::UInventoryComponent_StowedToolActor, Xbox::UInventoryComponent_StowedActorInLayout, Xbox::UInventoryComponent_StowedItemDataAsset, Xbox::UInventoryComponent_StowedWeaponQuality },
        { Xbox::UItemBase_ProjectileOverride },
        { Xbox::UItemActor_WeaponQuality, Xbox::UItemActor_VisualizedMods, Xbox::UItemActor_WeaponClip },
        { Xbox::UProjectileMovement_InitialSpeed, Xbox::UProjectileMovement_MaxSpeed, Xbox::UProjectileMovement_ProjectileGravity },
        { Xbox::AProjectile_ProjectileMovement },
        { Xbox::UInteractionComponent_IsOpened },
        { Xbox::UIHoverData_LootAmount, Xbox::UIHoverData_DataAsset },
        { Xbox::UItemDataAsset_ItemCategory, Xbox::UItemDataAsset_Weight },
        { Xbox::USkeletalMeshComponent_BoundsScale, Xbox::USkeletalMeshComponent_AnimScriptInstance, Xbox::USkeletalMeshComponent_bRecentlyRendered },
        { Xbox::UAnimInstance_CrossLayerAnimationPropertiesRef },
        { Xbox::UCrossLayerAnimationProperties_EquippedWeaponAmmoCount, Xbox::UCrossLayerAnimationProperties_bIsZooming, Xbox::UCrossLayerAnimationProperties_bIsSprinting, Xbox::UCrossLayerAnimationProperties_bIsDodgeRolling, Xbox::UCrossLayerAnimationProperties_bIsInJump, Xbox::UCrossLayerAnimationProperties_CharacterStance, Xbox::UCrossLayerAnimationProperties_bIsReloading, Xbox::UCrossLayerAnimationProperties_bIsFiring, Xbox::UCrossLayerAnimationProperties_LocoMode, Xbox::UCrossLayerAnimationProperties_LoopingReloadState, Xbox::UCrossLayerAnimationProperties_FireWeaponCount, Xbox::UCrossLayerAnimationProperties_FireState, Xbox::UCrossLayerAnimationProperties_TimeSinceFire, Xbox::UCrossLayerAnimationProperties_GameTimeSeconds, Xbox::UCrossLayerAnimationProperties_StartReloadTimeStamp, Xbox::UCrossLayerAnimationProperties_WeaponReloadTime, Xbox::UCrossLayerAnimationProperties_TimeSpentReloadingScaled, Xbox::UCrossLayerAnimationProperties_bIsTryingToSprint, Xbox::UCrossLayerAnimationProperties_bInSprintState, Xbox::UCrossLayerAnimationProperties_CurrentReloadActionIndex, Xbox::UCrossLayerAnimationProperties_bWasRecentlyRendered },
        { Xbox::ASalvageExtractionPointBase_bCanExtract, Xbox::ASalvageExtractionPointBase_bIsEnabled, Xbox::ASalvageExtractionPointBase_State, Xbox::ASalvageExtractionPointBase_ExtractionDataAsset, Xbox::ASalvageExtractionPointBase_TimeLeftAfterShutdown },
        { Xbox::UExtractionDataAsset_DialogueTag },
        { Xbox::FGameplayTag_TagName },
        { Xbox::Arc_HealthService, Xbox::Arc_HealthService_DamageDataStore, Xbox::Arc_DDS_ArrayData, Xbox::Arc_DDS_ArrayNum, Xbox::Arc_DDS_ElementStride, Xbox::Arc_DDS_Element_CurrentHealthUNorm, Xbox::Arc_ActorHealthComponent, Xbox::Arc_ActorHealthComponent_MaxHealth, Xbox::Arc_HealthGroupService, Xbox::Arc_HGS_BaseGroup, Xbox::Arc_BaseGroup_PartIDs_SlotsNum, Xbox::Arc_BaseGroup_PartIDs_NumFreeSlots, Xbox::Arc_BaseGroup_NumOfDestroyedParts }
    };

    inline const Values& Get() {
        return (g_PlatformVersion == PlatformVersion::Xbox) ? XboxValues : SteamValues;
    }
}

enum Bones
{
    Root = 0,
    pelvis = 1,
    spine_01 = 2,
    spine_02 = 3,
    spine_03 = 4,
    neck_01 = 5,
    neck_02 = 6,
    Head = 7,
    clavicle_l = 8,
    upperarm_l = 9,
    lowerarm_l = 10,
    hand_l = 11,
    ring_01_l = 12,
    ring_02_l = 13,
    ring_03_l = 14,
    ring_04_l = 15,
    index_01_l = 16,
    index_02_l = 17,
    index_03_l = 18,
    index_04_l = 19,
    pinky_01_l = 20,
    pinky_02_l = 21,
    pinky_03_l = 22,
    pinky_04_l = 23,
    thumb_01_l = 24,
    thumb_02_l = 25,
    thumb_03_l = 26,
    middle_01_l = 27,
    middle_02_l = 28,
    middle_03_l = 29,
    middle_04_l = 30,
    clavicle_r = 42,
    upperarm_r = 43,
    lowerarm_r = 44,
    // Was 34 (left-side/prop bone) — crossed to the left when players were downed.
    hand_r = 45,
    ring_01_r = 46,
    ring_02_r = 47,
    ring_03_r = 48,
    ring_04_r = 49,
    pinky_01_r = 50,
    pinky_02_r = 51,
    pinky_03_r = 52,
    pinky_04_r = 53,
    index_01_r = 54,
    index_02_r = 55,
    index_03_r = 56,
    index_04_r = 57,
    thumb_01_r = 58,
    thumb_02_r = 59,
    thumb_03_r = 60,
    middle_01_r = 61,
    middle_02_r = 62,
    middle_03_r = 63,
    middle_04_r = 64,
    weapon_buffer = 54,
    weapon_root = 55,
    weapon_mag = 56,
    weapon_belt_01 = 57,
    weapon_belt_02 = 58,
    weapon_belt_03 = 59,
    weapon_belt_04 = 60,
    weapon_belt_05 = 61,
    weapon_belt_06 = 62,
    weapon_dynamic_part_attach_01 = 63,
    prop_01 = 64,
    thigh_r = 65,
    calf_r = 66,
    foot_r = 67,
    ball_r = 68,
    thigh_l = 69,
    calf_l = 70,
    foot_l = 71,
    ball_l = 72,
    auxroot = 73,
    ik_hand_root = 74,
    ik_hand_l = 75,
    ik_hand_r = 76,
    ik_foot_root = 77,
    ik_foot_r = 78,
    ik_foot_l = 79,
    camera_root = 80,
    Camera = 81,
};
