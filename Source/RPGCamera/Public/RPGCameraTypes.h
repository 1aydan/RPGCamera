// Copyright (c) 2026. Licensed for use in your own projects.

#pragma once

#include "CoreMinimal.h"
#include "RPGCameraTypes.generated.h"

/** How the camera decides where to sit each frame. */
UENUM(BlueprintType)
enum class ERPGCameraMode : uint8
{
	/** Camera tracks the follow target (usually the player character). */
	FollowTarget	UMETA(DisplayName = "Follow Target"),

	/** Camera is detached and driven purely by pan input / edge scrolling. */
	FreeRoam		UMETA(DisplayName = "Free Roam")
};

/** How yaw rotation input is applied. */
UENUM(BlueprintType)
enum class ERPGYawMode : uint8
{
	/** Yaw is fixed at DefaultYaw and rotation input is ignored. */
	Locked			UMETA(DisplayName = "Locked"),

	/** Yaw follows analog input continuously. */
	Continuous		UMETA(DisplayName = "Continuous"),

	/** Yaw snaps to fixed increments (classic isometric 90/45 degree rotation). */
	Stepped			UMETA(DisplayName = "Stepped")
};

/**
 * View-derived vector value that can be pushed to a material parameter collection.
 * Every source resolves from the active camera and the view target, so these work
 * with any camera, not just URPGCameraComponent.
 */
UENUM(BlueprintType)
enum class ERPGCameraVectorSource : uint8
{
	/** Target location minus camera location, unnormalized. World units. */
	CameraToTarget				UMETA(DisplayName = "Camera To Target"),

	/** As above, normalized to unit length. */
	CameraToTargetNormalized	UMETA(DisplayName = "Camera To Target (Normalized)"),

	/** Camera to target with Z zeroed, for cylinder/capsule cutouts that ignore height. */
	CameraToTargetXY			UMETA(DisplayName = "Camera To Target (Horizontal)"),

	/** Horizontal camera to target, normalized. */
	CameraToTargetXYNormalized	UMETA(DisplayName = "Camera To Target (Horizontal, Normalized)"),

	/** Camera location minus target location. The reverse of Camera To Target. */
	TargetToCamera				UMETA(DisplayName = "Target To Camera"),

	/** As above, normalized to unit length. */
	TargetToCameraNormalized	UMETA(DisplayName = "Target To Camera (Normalized)"),

	/** World location of the camera. */
	CameraLocation				UMETA(DisplayName = "Camera Location"),

	/** World location of the view target. */
	TargetLocation				UMETA(DisplayName = "Target Location"),

	/** Camera forward vector. */
	CameraForward				UMETA(DisplayName = "Camera Forward"),

	/** Fixed value, for tuning constants that live alongside the driven ones. */
	Constant					UMETA(DisplayName = "Constant")
};

/**
 * View-derived scalar value that can be pushed to a material parameter collection.
 * Every source resolves from the active camera and the view target, so these work
 * with any camera, not just URPGCameraComponent.
 */
UENUM(BlueprintType)
enum class ERPGCameraScalarSource : uint8
{
	/** Straight-line distance from camera to target. Stands in for zoom on a follow camera. */
	DistanceToTarget			UMETA(DisplayName = "Distance To Target"),

	/** Distance from camera to target ignoring height. */
	HorizontalDistanceToTarget	UMETA(DisplayName = "Horizontal Distance To Target"),

	/** World Z of the view target, useful as a clip plane height. */
	TargetZ						UMETA(DisplayName = "Target Z"),

	/** World Z of the camera. */
	CameraZ						UMETA(DisplayName = "Camera Z"),

	/** Current camera pitch in degrees. */
	Pitch						UMETA(DisplayName = "Pitch"),

	/** Current camera yaw in degrees. */
	Yaw							UMETA(DisplayName = "Yaw"),

	/** Current field of view in degrees. */
	FieldOfView					UMETA(DisplayName = "Field Of View"),

	/** Fixed value, for tuning constants that live alongside the driven ones. */
	Constant					UMETA(DisplayName = "Constant")
};

/** One vector entry in the collection the camera writes to each frame. */
USTRUCT(BlueprintType)
struct FRPGCameraVectorParameter
{
	GENERATED_BODY()

	/** Name of the vector parameter in the collection. Must match exactly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	FName ParameterName;

	/** Which camera value feeds this parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	ERPGCameraVectorSource Source = ERPGCameraVectorSource::CameraToTarget;

	/** Value written when Source is Constant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter", meta = (EditCondition = "Source == ERPGCameraVectorSource::Constant", EditConditionHides))
	FLinearColor ConstantValue = FLinearColor::Black;
};

/** One scalar entry in the collection the camera writes to each frame. */
USTRUCT(BlueprintType)
struct FRPGCameraScalarParameter
{
	GENERATED_BODY()

	/** Name of the scalar parameter in the collection. Must match exactly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	FName ParameterName;

	/** Which camera value feeds this parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	ERPGCameraScalarSource Source = ERPGCameraScalarSource::DistanceToTarget;

	/** Value written when Source is Constant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter", meta = (EditCondition = "Source == ERPGCameraScalarSource::Constant", EditConditionHides))
	float ConstantValue = 0.f;

	/** Writes the square of the resolved value. Feeds the *Squared parameters that shaders use to skip a sqrt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameter")
	bool bSquareValue = false;
};

/** Technique used to make an obstructing mesh see-through. */
UENUM(BlueprintType)
enum class ERPGFadeMethod : uint8
{
	/**
	 * Writes the fade alpha into a Custom Primitive Data float slot.
	 * Cheapest option and works with instanced meshes, but your materials
	 * must read that slot and dither/blend with it. See README.
	 */
	CustomPrimitiveData	UMETA(DisplayName = "Custom Primitive Data"),

	/**
	 * Creates dynamic material instances and drives a named scalar parameter.
	 * Works with any material that exposes the parameter, costs a little more.
	 */
	MaterialParameter	UMETA(DisplayName = "Material Parameter"),

	/** No material work: the primitive is simply hidden (optionally keeping its shadow). */
	HideComponent		UMETA(DisplayName = "Hide Component"),

	/** Runs no built-in effect; only fires the IFadeableTarget interface events. */
	InterfaceOnly		UMETA(DisplayName = "Interface Only")
};
