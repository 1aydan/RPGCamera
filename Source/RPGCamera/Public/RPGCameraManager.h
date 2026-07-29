// Copyright (c) 2026. Licensed for use in your own projects.

#pragma once

#include "Camera/PlayerCameraManager.h"
#include "CoreMinimal.h"
#include "RPGCameraTypes.h"
#include "RPGCameraManager.generated.h"

class UOcclusionFadeComponent;
class URPGCameraComponent;

/**
 * Player camera manager that keeps track of the active top-down camera.
 *
 * Set this as PlayerCameraManagerClass on your PlayerController and every
 * Blueprint gets one stable place to reach the camera from, without needing a
 * reference to the pawn. The passthrough functions below are safe to call even
 * when no camera is currently resolved.
 */
UCLASS(Blueprintable, meta = (DisplayName = "RPG Camera Manager"))
class RPGCAMERA_API ARPGCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	ARPGCameraManager();

	virtual void UpdateCamera(float DeltaTime) override;

	/** The camera component on the current view target, if it has one. */
	UFUNCTION(BlueprintPure, Category = "RPG Camera Manager")
	URPGCameraComponent* GetRPGCamera() const;

	/** The fade component on the current view target, if it has one. */
	UFUNCTION(BlueprintPure, Category = "RPG Camera Manager")
	UOcclusionFadeComponent* GetOcclusionFade() const;

	/** Force a re-lookup, e.g. right after possessing a new pawn. */
	UFUNCTION(BlueprintCallable, Category = "RPG Camera Manager")
	void RefreshCameraReferences();

	// ---------------------------------------------------------------------
	// Passthroughs
	// ---------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "RPG Camera Manager|Zoom")
	void AddZoomInput(float ZoomDelta);

	UFUNCTION(BlueprintCallable, Category = "RPG Camera Manager|Zoom")
	void SetZoomLevel(int32 Level, bool bImmediate = false);

	UFUNCTION(BlueprintCallable, Category = "RPG Camera Manager|Rotation")
	void AddYawInput(float AxisValue);

	UFUNCTION(BlueprintCallable, Category = "RPG Camera Manager|Rotation")
	void AddYawSteps(int32 Steps);

	UFUNCTION(BlueprintCallable, Category = "RPG Camera Manager|Rotation")
	void RecenterYaw(bool bImmediate = false);

	UFUNCTION(BlueprintCallable, Category = "RPG Camera Manager|Movement")
	void AddPanInput(FVector2D PanInput);

	UFUNCTION(BlueprintCallable, Category = "RPG Camera Manager|Movement")
	void SetCameraMode(ERPGCameraMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "RPG Camera Manager|Movement")
	void SetFollowTarget(AActor* NewTarget, bool bSnapImmediately = false);

	UFUNCTION(BlueprintCallable, Category = "RPG Camera Manager|Movement")
	void SnapToTarget();

	UFUNCTION(BlueprintCallable, Category = "RPG Camera Manager|Fade")
	void SetFadeEnabled(bool bEnabled);

protected:
	/** Cached components from the current view target. */
	UPROPERTY(Transient)
	TWeakObjectPtr<URPGCameraComponent> CachedCamera;

	UPROPERTY(Transient)
	TWeakObjectPtr<UOcclusionFadeComponent> CachedFade;

	/** View target the cache was built from, so we know when to rebuild. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedViewTarget;
};
