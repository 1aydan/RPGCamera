// Copyright (c) 2026. Licensed for use in your own projects.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/SpringArmComponent.h"
#include "RPGCameraTypes.h"
#include "RPGCameraComponent.generated.h"

class APlayerController;
class UCameraComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRPGCameraModeChanged, ERPGCameraMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRPGCameraZoomChanged, float, NewArmLength);

/**
 * Drop-in top-down / ARPG camera.
 *
 * This is a USpringArmComponent, so attach a UCameraComponent to it exactly as you
 * would with a normal spring arm. Unlike a normal spring arm it uses absolute
 * location/rotation, which lets it lag behind, lead in front of, or fully detach
 * from its owner without any extra actors.
 *
 * Everything here is BlueprintReadWrite / BlueprintCallable - C++ is never required.
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent, DisplayName = "RPG Camera"))
class RPGCAMERA_API URPGCameraComponent : public USpringArmComponent
{
	GENERATED_BODY()

public:
	URPGCameraComponent();

	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UActorComponent interface

	// ---------------------------------------------------------------------
	// Mode & target
	// ---------------------------------------------------------------------

	/** Follow the target, or roam freely. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RPG Camera|Mode")
	ERPGCameraMode CameraMode = ERPGCameraMode::FollowTarget;

	/** If true, panning while in Follow mode automatically drops the camera into Free Roam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Mode")
	bool bPanSwitchesToFreeRoam = true;

	/** If true, the camera slides back to the follow target after ReturnToTargetDelay seconds without pan input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Mode")
	bool bAutoReturnToTarget = false;

	/** Seconds of no pan input before the camera returns to the follow target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Mode", meta = (EditCondition = "bAutoReturnToTarget", ClampMin = "0.0", Units = "s"))
	float ReturnToTargetDelay = 2.5f;

	/** Actor the camera follows. Defaults to the component owner on BeginPlay. */
	UPROPERTY(BlueprintReadOnly, Category = "RPG Camera|Mode")
	TWeakObjectPtr<AActor> FollowTarget;

	/** World-space offset applied on top of the follow target's location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Mode")
	FVector FocusOffset = FVector(0.f, 0.f, 0.f);

	// ---------------------------------------------------------------------
	// Follow smoothing
	// ---------------------------------------------------------------------

	/** How quickly the focus point catches up. 0 = instant snap (no lag). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Follow", meta = (ClampMin = "0.0"))
	float FollowInterpSpeed = 10.f;

	/** If the focus point is further than this from its goal, snap instead of interpolating. 0 disables. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Follow", meta = (ClampMin = "0.0", Units = "cm"))
	float SnapDistanceThreshold = 2000.f;

	/** Push the focus point ahead of the target based on its velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Follow")
	bool bLeadTarget = false;

	/** Seconds of velocity to lead by. 0.25 means the camera sits a quarter second ahead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Follow", meta = (EditCondition = "bLeadTarget", ClampMin = "0.0", Units = "s"))
	float LeadTime = 0.25f;

	/** Maximum distance the lead offset may reach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Follow", meta = (EditCondition = "bLeadTarget", ClampMin = "0.0", Units = "cm"))
	float MaxLeadDistance = 300.f;

	/** Bias the focus point toward the mouse cursor's world position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Follow")
	bool bMouseInfluence = false;

	/** 0 = ignore cursor, 1 = focus sits on the cursor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Follow", meta = (EditCondition = "bMouseInfluence", ClampMin = "0.0", ClampMax = "1.0"))
	float MouseInfluenceStrength = 0.25f;

	/** Maximum distance the cursor bias may push the focus point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Follow", meta = (EditCondition = "bMouseInfluence", ClampMin = "0.0", Units = "cm"))
	float MaxMouseInfluenceDistance = 400.f;

	// ---------------------------------------------------------------------
	// Zoom
	// ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Zoom", meta = (ClampMin = "1.0", Units = "cm"))
	float MinArmLength = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Zoom", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxArmLength = 2000.f;

	/** Distance added/removed per unit of zoom input (one mouse wheel notch = 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Zoom", meta = (ClampMin = "0.0", Units = "cm"))
	float ZoomStep = 150.f;

	/** How quickly the arm length reaches the requested zoom. 0 = instant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Zoom", meta = (ClampMin = "0.0"))
	float ZoomInterpSpeed = 8.f;

	/** Arm length the camera starts at. Clamped between Min and Max. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Zoom", meta = (ClampMin = "1.0", Units = "cm"))
	float DefaultArmLength = 1000.f;

	/**
	 * Snap zoom to a fixed set of levels instead of sliding continuously.
	 * One notch of zoom input moves one level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Zoom")
	bool bUseDiscreteZoomLevels = false;

	/** Number of evenly spaced levels between MinArmLength and MaxArmLength. Ignored if CustomZoomLevels is populated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Zoom", meta = (EditCondition = "bUseDiscreteZoomLevels", ClampMin = "2", ClampMax = "32"))
	int32 ZoomLevelCount = 5;

	/** Explicit arm lengths, closest first. Overrides ZoomLevelCount when non-empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Zoom", meta = (EditCondition = "bUseDiscreteZoomLevels"))
	TArray<float> CustomZoomLevels;

	/** Zoom level the camera starts on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Zoom", meta = (EditCondition = "bUseDiscreteZoomLevels", ClampMin = "0"))
	int32 DefaultZoomLevel = 2;

	// ---------------------------------------------------------------------
	// Field of view
	// ---------------------------------------------------------------------

	/** Let this component drive the field of view of the camera attached to it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|FOV")
	bool bManageFieldOfView = true;

	/** FOV used when it is not linked to zoom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|FOV", meta = (EditCondition = "bManageFieldOfView", ClampMin = "5.0", ClampMax = "170.0", Units = "deg"))
	float DefaultFOV = 60.f;

	/** Widen or narrow the lens across the zoom range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|FOV", meta = (EditCondition = "bManageFieldOfView"))
	bool bLinkFOVToZoom = false;

	/** FOV at MinArmLength (fully zoomed in). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|FOV", meta = (EditCondition = "bManageFieldOfView && bLinkFOVToZoom", ClampMin = "5.0", ClampMax = "170.0", Units = "deg"))
	float FOVAtMinZoom = 55.f;

	/** FOV at MaxArmLength (fully zoomed out). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|FOV", meta = (EditCondition = "bManageFieldOfView && bLinkFOVToZoom", ClampMin = "5.0", ClampMax = "170.0", Units = "deg"))
	float FOVAtMaxZoom = 70.f;

	/** Optional remap of normalized zoom before blending the two FOV values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|FOV", meta = (EditCondition = "bManageFieldOfView && bLinkFOVToZoom"))
	FRuntimeFloatCurve FOVBlendCurve;

	/** How quickly FOV reaches its goal. 0 = instant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|FOV", meta = (EditCondition = "bManageFieldOfView", ClampMin = "0.0"))
	float FOVInterpSpeed = 8.f;

	// ---------------------------------------------------------------------
	// Pitch
	// ---------------------------------------------------------------------

	/** Pitch used when pitch is not linked to zoom. Negative looks down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Pitch", meta = (ClampMin = "-89.0", ClampMax = "89.0", Units = "deg"))
	float DefaultPitch = -55.f;

	/** Flatten the angle as the camera zooms in, steepen it as it zooms out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Pitch")
	bool bLinkPitchToZoom = true;

	/** Pitch at MinArmLength (fully zoomed in). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Pitch", meta = (EditCondition = "bLinkPitchToZoom", ClampMin = "-89.0", ClampMax = "89.0", Units = "deg"))
	float PitchAtMinZoom = -35.f;

	/** Pitch at MaxArmLength (fully zoomed out). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Pitch", meta = (EditCondition = "bLinkPitchToZoom", ClampMin = "-89.0", ClampMax = "89.0", Units = "deg"))
	float PitchAtMaxZoom = -65.f;

	/**
	 * Optional remap of normalized zoom (0 = min, 1 = max) before blending the two pitch
	 * values. Leave empty for a straight linear blend.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Pitch", meta = (EditCondition = "bLinkPitchToZoom"))
	FRuntimeFloatCurve PitchBlendCurve;

	// ---------------------------------------------------------------------
	// Yaw
	// ---------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Yaw")
	ERPGYawMode YawMode = ERPGYawMode::Continuous;

	/** Yaw the camera rests at, and returns to when recentered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Yaw", meta = (Units = "deg"))
	float DefaultYaw = 0.f;

	/** Degrees per second for continuous rotation (input of 1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Yaw", meta = (ClampMin = "0.0"))
	float YawSpeed = 180.f;

	/** Degrees per step in Stepped mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Yaw", meta = (ClampMin = "1.0", ClampMax = "180.0", Units = "deg"))
	float YawStepAngle = 45.f;

	/** How quickly yaw interpolates toward its goal. 0 = instant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Yaw", meta = (ClampMin = "0.0"))
	float YawInterpSpeed = 10.f;

	/** Drift back to DefaultYaw after the player stops rotating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Yaw")
	bool bAutoRecenterYaw = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Yaw", meta = (EditCondition = "bAutoRecenterYaw", ClampMin = "0.0", Units = "s"))
	float YawRecenterDelay = 3.f;

	// ---------------------------------------------------------------------
	// Panning & edge scrolling
	// ---------------------------------------------------------------------

	/** Pan speed in cm/s at DefaultArmLength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Pan", meta = (ClampMin = "0.0"))
	float PanSpeed = 1600.f;

	/** Pan faster when zoomed out, slower when zoomed in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Pan")
	bool bScalePanSpeedWithZoom = true;

	/** Move the camera when the cursor sits near a viewport edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Pan")
	bool bEnableEdgePan = false;

	/** Thickness in pixels of the edge-pan band. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Pan", meta = (EditCondition = "bEnableEdgePan", ClampMin = "1.0"))
	float EdgePanBorderPixels = 24.f;

	/** Scales PanSpeed while edge panning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Pan", meta = (EditCondition = "bEnableEdgePan", ClampMin = "0.0"))
	float EdgePanSpeedScale = 1.f;

	// ---------------------------------------------------------------------
	// Bounds
	// ---------------------------------------------------------------------

	/** Keep the focus point inside an axis-aligned box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Bounds")
	bool bClampToBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Bounds", meta = (EditCondition = "bClampToBounds"))
	FVector BoundsOrigin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Bounds", meta = (EditCondition = "bClampToBounds"))
	FVector BoundsExtent = FVector(5000.f, 5000.f, 5000.f);

	/** Clamp height as well as X/Y. Usually left off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Bounds", meta = (EditCondition = "bClampToBounds"))
	bool bClampVertical = false;

	// ---------------------------------------------------------------------
	// Material parameters
	// ---------------------------------------------------------------------

	/**
	 * Optional collection updated every frame with the camera values listed below.
	 * Lets materials do their own occlusion work — cylinder cutouts, height clipping,
	 * distance falloff — without any per-mesh traces or dynamic material instances.
	 *
	 * The plugin never assumes parameter names; you map each one yourself.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Material Parameters")
	TObjectPtr<class UMaterialParameterCollection> ParameterCollection;

	/** Vector parameters written to ParameterCollection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Material Parameters", meta = (TitleProperty = "ParameterName"))
	TArray<FRPGCameraVectorParameter> VectorParameters;

	/** Scalar parameters written to ParameterCollection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RPG Camera|Material Parameters", meta = (TitleProperty = "ParameterName"))
	TArray<FRPGCameraScalarParameter> ScalarParameters;

	// ---------------------------------------------------------------------
	// Events
	// ---------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "RPG Camera|Events")
	FRPGCameraModeChanged OnCameraModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "RPG Camera|Events")
	FRPGCameraZoomChanged OnZoomChanged;

	// ---------------------------------------------------------------------
	// API
	// ---------------------------------------------------------------------

	/** Feed one notch of zoom. Positive zooms in, negative zooms out. */
	UFUNCTION(BlueprintCallable, Category = "RPG Camera")
	void AddZoomInput(float ZoomDelta);

	/** Set the goal arm length directly (clamped to Min/Max). */
	UFUNCTION(BlueprintCallable, Category = "RPG Camera")
	void SetZoomDistance(float NewDistance, bool bImmediate = false);

	/** Current goal arm length, before interpolation. */
	UFUNCTION(BlueprintPure, Category = "RPG Camera")
	float GetZoomDistance() const { return GoalArmLength; }

	/** 0 = fully zoomed in, 1 = fully zoomed out. */
	UFUNCTION(BlueprintPure, Category = "RPG Camera")
	float GetNormalizedZoom() const;

	/** Jump to a discrete zoom level. Only meaningful when bUseDiscreteZoomLevels is true. */
	UFUNCTION(BlueprintCallable, Category = "RPG Camera")
	void SetZoomLevel(int32 Level, bool bImmediate = false);

	UFUNCTION(BlueprintPure, Category = "RPG Camera")
	int32 GetZoomLevel() const { return CurrentZoomLevel; }

	UFUNCTION(BlueprintPure, Category = "RPG Camera")
	int32 GetNumZoomLevels() const;

	/** Override the field of view goal. Ignored while bLinkFOVToZoom is true. */
	UFUNCTION(BlueprintCallable, Category = "RPG Camera")
	void SetFieldOfView(float NewFOV, bool bImmediate = false);

	UFUNCTION(BlueprintPure, Category = "RPG Camera")
	float GetFieldOfView() const { return CurrentFOV; }

	/** The camera this component drives. Auto-resolved from attached children on BeginPlay. */
	UFUNCTION(BlueprintPure, Category = "RPG Camera")
	UCameraComponent* GetManagedCamera() const { return ManagedCamera.Get(); }

	UFUNCTION(BlueprintCallable, Category = "RPG Camera")
	void SetManagedCamera(UCameraComponent* NewCamera);

	/** Rotate continuously. AxisValue is typically -1..1 and is scaled by YawSpeed. */
	UFUNCTION(BlueprintCallable, Category = "RPG Camera")
	void AddYawInput(float AxisValue);

	/** Rotate by whole steps of YawStepAngle. Works in Stepped and Continuous modes; ignored while yaw is Locked. */
	UFUNCTION(BlueprintCallable, Category = "RPG Camera")
	void AddYawSteps(int32 Steps);

	UFUNCTION(BlueprintCallable, Category = "RPG Camera")
	void SetYaw(float NewYaw, bool bImmediate = false);

	/** Return yaw to DefaultYaw. */
	UFUNCTION(BlueprintCallable, Category = "RPG Camera")
	void RecenterYaw(bool bImmediate = false);

	UFUNCTION(BlueprintPure, Category = "RPG Camera")
	float GetYaw() const { return CurrentYaw; }

	/** Pan input in screen-relative axes: X = right, Y = forward. */
	UFUNCTION(BlueprintCallable, Category = "RPG Camera")
	void AddPanInput(FVector2D PanInput);

	UFUNCTION(BlueprintCallable, Category = "RPG Camera")
	void SetCameraMode(ERPGCameraMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "RPG Camera")
	void SetFollowTarget(AActor* NewTarget, bool bSnapImmediately = false);

	/** Jump the camera onto its follow target right now, cancelling free roam. */
	UFUNCTION(BlueprintCallable, Category = "RPG Camera")
	void SnapToTarget();

	/** The point the camera is currently looking at. */
	UFUNCTION(BlueprintPure, Category = "RPG Camera")
	FVector GetFocusLocation() const { return CurrentFocus; }

	/** Convenience: forward vector flattened onto the XY plane, useful for movement input. */
	UFUNCTION(BlueprintPure, Category = "RPG Camera")
	FVector GetPlanarForward() const;

	UFUNCTION(BlueprintPure, Category = "RPG Camera")
	FVector GetPlanarRight() const;

	/** World location of the managed camera, falling back to the arm's socket. */
	UFUNCTION(BlueprintPure, Category = "RPG Camera")
	FVector GetCameraLocation() const;

	/** World location of the follow target, falling back to the current focus point. */
	UFUNCTION(BlueprintPure, Category = "RPG Camera")
	FVector GetTargetLocation() const;

	/** Resolve one vector source against the camera's current state. */
	UFUNCTION(BlueprintPure, Category = "RPG Camera|Material Parameters")
	FVector ResolveVectorSource(ERPGCameraVectorSource Source) const;

	/** Resolve one scalar source against the camera's current state. */
	UFUNCTION(BlueprintPure, Category = "RPG Camera|Material Parameters")
	float ResolveScalarSource(ERPGCameraScalarSource Source) const;

	/**
	 * Push every configured parameter to ParameterCollection.
	 * Called automatically each tick; call manually if you need a mid-frame refresh.
	 */
	UFUNCTION(BlueprintCallable, Category = "RPG Camera|Material Parameters")
	void UpdateMaterialParameters();

protected:
	/** Runtime focus point, interpolated. */
	FVector CurrentFocus = FVector::ZeroVector;

	/** Focus point used while free roaming. */
	FVector FreeRoamFocus = FVector::ZeroVector;

	float GoalArmLength = 1000.f;
	float CurrentYaw = 0.f;
	float GoalYaw = 0.f;
	float CurrentPitch = -55.f;
	float CurrentFOV = 60.f;
	float GoalFOV = 60.f;

	int32 CurrentZoomLevel = 0;

	/** Camera whose FOV we drive. */
	TWeakObjectPtr<UCameraComponent> ManagedCamera;

	/** Accumulated this frame, consumed on tick. */
	FVector2D PendingPanInput = FVector2D::ZeroVector;
	float PendingYawInput = 0.f;

	/** Speed multiplier for this frame's pan, e.g. from edge panning. */
	float PendingPanSpeedScale = 1.f;

	double LastPanInputTime = -1.0e30;
	double LastYawInputTime = -1.0e30;

	bool bHasInitializedFocus = false;

	/** Parameter names already reported as missing, so a typo warns once instead of every frame. */
	TSet<FName> WarnedParameterNames;

	APlayerController* GetOwningPlayerController() const;

	FVector ComputeFollowFocus() const;
	void ApplyEdgePan(float DeltaTime);
	void UpdateZoom(float DeltaTime);
	void UpdateYaw(float DeltaTime);
	void UpdatePitch();
	void UpdateFOV(float DeltaTime);

	/** Finds the first UCameraComponent attached beneath this arm. */
	void ResolveManagedCamera();

	/** Arm length for a given discrete level, clamped to the valid range. */
	float GetZoomLevelDistance(int32 Level) const;
	void UpdateFocus(float DeltaTime);
	FVector ClampToBounds(const FVector& InLocation) const;

	/** Intersects the cursor ray with the horizontal plane at PlaneZ. Returns false if it misses. */
	bool GetCursorPointOnPlane(float PlaneZ, FVector& OutLocation) const;
};
