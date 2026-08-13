// Copyright (c) 2026. Licensed for use in your own projects.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "RPGCameraTypes.h"
#include "OcclusionFadeComponent.generated.h"

class AOcclusionFadeGroup;
class APlayerController;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UOcclusionSubsystem;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRPGOcclusionActorChanged, AActor*, Actor);

/** Per-primitive fade bookkeeping. */
USTRUCT()
struct FRPGFadeState
{
	GENERATED_BODY()

	/** 1 = fully opaque, 0 = fully faded out. */
	UPROPERTY()
	float Alpha = 1.f;

	/** True while this primitive is still blocking the view. */
	UPROPERTY()
	bool bOccluding = false;

	/** Group this primitive fades with, if any. Supplies setting overrides. */
	UPROPERTY()
	TWeakObjectPtr<AOcclusionFadeGroup> Group;

	/**
	 * Method in force when the originals were cached. Restore always undoes
	 * what apply did, even if the group's override is toggled mid-fade.
	 */
	UPROPERTY()
	ERPGFadeMethod AppliedMethod = ERPGFadeMethod::CustomPrimitiveData;

	UPROPERTY()
	int32 AppliedCustomPrimitiveDataIndex = 0;

	UPROPERTY()
	FName AppliedFadeParameterName;

	/** Cached dynamic materials, only created for the MaterialParameter method. */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;

	/** Original visibility, restored when the fade finishes. */
	UPROPERTY()
	bool bOriginalVisibility = true;

	UPROPERTY()
	bool bOriginalCastHiddenShadow = false;

	/** Custom Primitive Data value the slot held before we started writing to it. */
	UPROPERTY()
	float OriginalCustomPrimitiveData = 1.f;

	UPROPERTY()
	bool bCachedOriginals = false;
};

/**
 * Fades out meshes standing between the camera and the character.
 *
 * Put this on the player character (or on whatever the camera is looking at) and
 * it will sweep back toward the camera each interval, fading anything it hits.
 *
 * It is fully independent of URPGCameraComponent - it works with any camera.
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent, DisplayName = "Occlusion Fade"))
class RPGCAMERA_API UOcclusionFadeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOcclusionFadeComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---------------------------------------------------------------------
	// Detection
	// ---------------------------------------------------------------------

	/** Turn the whole system on or off at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	bool bFadeEnabled = true;

	/** Actor the camera is trying to see. Defaults to this component's owner. */
	UPROPERTY(BlueprintReadOnly, Category = "Occlusion Fade")
	TWeakObjectPtr<AActor> ViewTarget;

	/** Offset from the view target's origin, e.g. raise it to chest height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	FVector ViewTargetOffset = FVector(0.f, 0.f, 40.f);

	/** Radius of the sweep. Larger values clear a wider window around the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceRadius = 45.f;

	/** Trace channel used to find obstructions. Visibility works for most projects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/**
	 * Seconds between sweeps. Fades still interpolate every frame; only the
	 * detection is throttled. 0 traces every frame.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade", meta = (ClampMin = "0.0", Units = "s"))
	float TraceInterval = 0.05f;

	/** Stop the sweep short of the camera so you don't fade meshes right at the lens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade", meta = (ClampMin = "0.0", Units = "cm"))
	float CameraPadding = 20.f;

	/** Trace against complex collision. Slower, but catches thin geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade", meta = (AdvancedDisplay))
	bool bTraceComplex = false;

	/**
	 * Expand each hit through any AOcclusionFadeGroup volume it belongs to, so
	 * a whole building fades as one instead of the sweep carving a hole in it.
	 * Off = every mesh fades on its own, as before.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	bool bUseOcclusionGroups = true;

	// ---------------------------------------------------------------------
	// Filtering
	// ---------------------------------------------------------------------

	/** Only fade actors implementing IFadeableTarget. Off = fade anything that blocks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade|Filter")
	bool bRequireFadeableInterface = false;

	/** If non-empty, only actors with one of these tags are faded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade|Filter")
	TArray<FName> RequiredActorTags;

	/**
	 * If non-empty, only primitives with one of these component tags are faded,
	 * letting you opt in individual meshes on a multi-mesh actor. When
	 * RequiredActorTags is also set, both filters must pass.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade|Filter")
	TArray<FName> RequiredComponentTags;

	/** Actors with any of these tags are never faded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade|Filter")
	TArray<FName> IgnoredActorTags;

	/** Component tags that exclude an individual primitive from fading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade|Filter")
	TArray<FName> IgnoredComponentTags;

	/** Classes never faded, e.g. your landscape or character class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade|Filter")
	TArray<TSubclassOf<AActor>> IgnoredActorClasses;

	// ---------------------------------------------------------------------
	// Appearance
	//
	// These are the defaults. A primitive that belongs to an
	// AOcclusionFadeGroup with bOverrideFadeSettings on uses the group's
	// FadeSettings instead.
	// ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade|Appearance")
	ERPGFadeMethod FadeMethod = ERPGFadeMethod::CustomPrimitiveData;

	/** Opacity a fully faded mesh settles at. 0 is invisible, 0.2 leaves a ghost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade|Appearance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FadedAlpha = 0.15f;

	/** Alpha units per second while fading out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade|Appearance", meta = (ClampMin = "0.01"))
	float FadeOutSpeed = 4.f;

	/** Alpha units per second while fading back in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade|Appearance", meta = (ClampMin = "0.01"))
	float FadeInSpeed = 2.f;

	/** Custom Primitive Data float index written to. Must match your material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade|Appearance", meta = (EditCondition = "FadeMethod == ERPGFadeMethod::CustomPrimitiveData", ClampMin = "0", ClampMax = "31"))
	int32 CustomPrimitiveDataIndex = 0;

	/** Scalar parameter driven on dynamic material instances. Must match your material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade|Appearance", meta = (EditCondition = "FadeMethod == ERPGFadeMethod::MaterialParameter"))
	FName FadeParameterName = TEXT("FadeAmount");

	/** Keep casting shadows while hidden, so the world still reads correctly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade|Appearance", meta = (EditCondition = "FadeMethod == ERPGFadeMethod::HideComponent"))
	bool bKeepShadowsWhenHidden = true;

	// ---------------------------------------------------------------------
	// Events & API
	// ---------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Occlusion Fade|Events")
	FRPGOcclusionActorChanged OnActorBeganOccluding;

	UPROPERTY(BlueprintAssignable, Category = "Occlusion Fade|Events")
	FRPGOcclusionActorChanged OnActorStoppedOccluding;

	/** Every actor currently faded, including members pulled in by a group. */
	UFUNCTION(BlueprintPure, Category = "Occlusion Fade")
	TArray<AActor*> GetOccludingActors() const;

	/** Every group with at least one member blocking the view. */
	UFUNCTION(BlueprintPure, Category = "Occlusion Fade")
	TArray<AOcclusionFadeGroup*> GetOccludingGroups() const;

	/** Restore everything to full opacity immediately and clear tracking. */
	UFUNCTION(BlueprintCallable, Category = "Occlusion Fade")
	void ClearAllFades();

	UFUNCTION(BlueprintCallable, Category = "Occlusion Fade")
	void SetViewTarget(AActor* NewTarget);

	/** Camera used as the sweep origin. Defaults to the local player's view. */
	UFUNCTION(BlueprintCallable, Category = "Occlusion Fade")
	void SetCameraOverride(UCameraComponent* NewCamera);

protected:
	/** Primitive -> fade state. */
	UPROPERTY()
	TMap<TObjectPtr<UPrimitiveComponent>, FRPGFadeState> FadeStates;

	UPROPERTY()
	TWeakObjectPtr<UCameraComponent> CameraOverride;

	double LastTraceTime = -1.0e30;

	/** Actors that were occluding as of the last sweep, for change events. */
	TSet<TWeakObjectPtr<AActor>> PreviousOccluders;

	/** Groups that were occluding as of the last sweep, for change events. */
	TSet<TWeakObjectPtr<AOcclusionFadeGroup>> PreviousOccludingGroups;

	void PerformOcclusionTrace();
	void UpdateFadeAlphas(float DeltaTime);

	/** This component's own appearance properties, bundled. */
	FRPGFadeSettings GetDefaultFadeSettings() const;

	/** The settings that actually drive State: the group's override, or ours. */
	FRPGFadeSettings ResolveFadeSettings(const FRPGFadeState& State) const;

	UOcclusionSubsystem* GetOcclusionSubsystem() const;

	bool ShouldFadePrimitive(const UPrimitiveComponent* Primitive) const;
	void ApplyFade(UPrimitiveComponent* Primitive, FRPGFadeState& State, const FRPGFadeSettings& Settings);
	void CacheOriginals(UPrimitiveComponent* Primitive, FRPGFadeState& State);
	void RestorePrimitive(UPrimitiveComponent* Primitive, FRPGFadeState& State);

	bool GetCameraLocation(FVector& OutLocation) const;
	FVector GetViewTargetLocation() const;
	APlayerController* GetRelevantPlayerController() const;
};
