// Copyright (c) 2026. Licensed for use in your own projects.

#include "RPGCameraManager.h"

#include "OcclusionFadeComponent.h"
#include "RPGCameraComponent.h"
#include "RPGCameraModule.h"

ARPGCameraManager::ARPGCameraManager()
{
	// The camera component drives pitch itself; don't let the manager clamp it.
	ViewPitchMin = -89.f;
	ViewPitchMax = 89.f;
}

void ARPGCameraManager::UpdateCamera(float DeltaTime)
{
	Super::UpdateCamera(DeltaTime);

	// Rebuild the cache whenever the view target changes.
	AActor* CurrentViewTarget = GetViewTarget();
	if (CurrentViewTarget != CachedViewTarget.Get())
	{
		RefreshCameraReferences();
	}
}

void ARPGCameraManager::RefreshCameraReferences()
{
	AActor* CurrentViewTarget = GetViewTarget();
	CachedViewTarget = CurrentViewTarget;

	if (!CurrentViewTarget)
	{
		CachedCamera = nullptr;
		CachedFade = nullptr;
		return;
	}

	CachedCamera = CurrentViewTarget->FindComponentByClass<URPGCameraComponent>();
	CachedFade = CurrentViewTarget->FindComponentByClass<UOcclusionFadeComponent>();
}

URPGCameraComponent* ARPGCameraManager::GetRPGCamera() const
{
	return CachedCamera.Get();
}

UOcclusionFadeComponent* ARPGCameraManager::GetOcclusionFade() const
{
	return CachedFade.Get();
}

// ---------------------------------------------------------------------------
// Passthroughs
// ---------------------------------------------------------------------------

void ARPGCameraManager::AddZoomInput(float ZoomDelta)
{
	if (URPGCameraComponent* Camera = GetRPGCamera())
	{
		Camera->AddZoomInput(ZoomDelta);
	}
}

void ARPGCameraManager::SetZoomLevel(int32 Level, bool bImmediate)
{
	if (URPGCameraComponent* Camera = GetRPGCamera())
	{
		Camera->SetZoomLevel(Level, bImmediate);
	}
}

void ARPGCameraManager::AddYawInput(float AxisValue)
{
	if (URPGCameraComponent* Camera = GetRPGCamera())
	{
		Camera->AddYawInput(AxisValue);
	}
}

void ARPGCameraManager::AddYawSteps(int32 Steps)
{
	if (URPGCameraComponent* Camera = GetRPGCamera())
	{
		Camera->AddYawSteps(Steps);
	}
}

void ARPGCameraManager::RecenterYaw(bool bImmediate)
{
	if (URPGCameraComponent* Camera = GetRPGCamera())
	{
		Camera->RecenterYaw(bImmediate);
	}
}

void ARPGCameraManager::AddPanInput(FVector2D PanInput)
{
	if (URPGCameraComponent* Camera = GetRPGCamera())
	{
		Camera->AddPanInput(PanInput);
	}
}

void ARPGCameraManager::SetCameraMode(ERPGCameraMode NewMode)
{
	if (URPGCameraComponent* Camera = GetRPGCamera())
	{
		Camera->SetCameraMode(NewMode);
	}
}

void ARPGCameraManager::SetFollowTarget(AActor* NewTarget, bool bSnapImmediately)
{
	if (URPGCameraComponent* Camera = GetRPGCamera())
	{
		Camera->SetFollowTarget(NewTarget, bSnapImmediately);
	}
}

void ARPGCameraManager::SnapToTarget()
{
	if (URPGCameraComponent* Camera = GetRPGCamera())
	{
		Camera->SnapToTarget();
	}
}

void ARPGCameraManager::SetFadeEnabled(bool bEnabled)
{
	if (UOcclusionFadeComponent* Fade = GetOcclusionFade())
	{
		Fade->bFadeEnabled = bEnabled;
	}
}
