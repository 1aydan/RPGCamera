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
