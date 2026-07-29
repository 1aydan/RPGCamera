#include "TopDownCameraManager.h"

#include "OcclusionFadeComponent.h"
#include "TopDownCameraComponent.h"
#include "TopDownRPGCameraModule.h"

ATopDownCameraManager::ATopDownCameraManager()
{
	// The camera component drives pitch itself; don't let the manager clamp it.
	ViewPitchMin = -89.f;
	ViewPitchMax = 89.f;
}

void ATopDownCameraManager::UpdateCamera(float DeltaTime)
{
	Super::UpdateCamera(DeltaTime);

	// Rebuild the cache whenever the view target changes.
	AActor* CurrentViewTarget = GetViewTarget();
	if (CurrentViewTarget != CachedViewTarget.Get())
	{
		RefreshCameraReferences();
	}
}

void ATopDownCameraManager::RefreshCameraReferences()
{
	AActor* CurrentViewTarget = GetViewTarget();
	CachedViewTarget = CurrentViewTarget;

	if (!CurrentViewTarget)
	{
		CachedCamera = nullptr;
		CachedFade = nullptr;
		return;
	}

	CachedCamera = CurrentViewTarget->FindComponentByClass<UTopDownCameraComponent>();
	CachedFade = CurrentViewTarget->FindComponentByClass<UOcclusionFadeComponent>();
}

UTopDownCameraComponent* ATopDownCameraManager::GetTopDownCamera() const
{
	return CachedCamera.Get();
}

UOcclusionFadeComponent* ATopDownCameraManager::GetOcclusionFade() const
{
	return CachedFade.Get();
}

// ---------------------------------------------------------------------------
// Passthroughs
// ---------------------------------------------------------------------------

void ATopDownCameraManager::AddZoomInput(float ZoomDelta)
{
	if (UTopDownCameraComponent* Camera = GetTopDownCamera())
	{
		Camera->AddZoomInput(ZoomDelta);
	}
}

void ATopDownCameraManager::SetZoomLevel(int32 Level, bool bImmediate)
{
	if (UTopDownCameraComponent* Camera = GetTopDownCamera())
	{
		Camera->SetZoomLevel(Level, bImmediate);
	}
}

void ATopDownCameraManager::AddYawInput(float AxisValue)
{
	if (UTopDownCameraComponent* Camera = GetTopDownCamera())
	{
		Camera->AddYawInput(AxisValue);
	}
}

void ATopDownCameraManager::AddYawSteps(int32 Steps)
{
	if (UTopDownCameraComponent* Camera = GetTopDownCamera())
	{
		Camera->AddYawSteps(Steps);
	}
}

void ATopDownCameraManager::RecenterYaw(bool bImmediate)
{
	if (UTopDownCameraComponent* Camera = GetTopDownCamera())
	{
		Camera->RecenterYaw(bImmediate);
	}
}

void ATopDownCameraManager::AddPanInput(FVector2D PanInput)
{
	if (UTopDownCameraComponent* Camera = GetTopDownCamera())
	{
		Camera->AddPanInput(PanInput);
	}
}

void ATopDownCameraManager::SetCameraMode(ETDCameraMode NewMode)
{
	if (UTopDownCameraComponent* Camera = GetTopDownCamera())
	{
		Camera->SetCameraMode(NewMode);
	}
}

void ATopDownCameraManager::SetFollowTarget(AActor* NewTarget, bool bSnapImmediately)
{
	if (UTopDownCameraComponent* Camera = GetTopDownCamera())
	{
		Camera->SetFollowTarget(NewTarget, bSnapImmediately);
	}
}

void ATopDownCameraManager::SnapToTarget()
{
	if (UTopDownCameraComponent* Camera = GetTopDownCamera())
	{
		Camera->SnapToTarget();
	}
}

void ATopDownCameraManager::SetFadeEnabled(bool bEnabled)
{
	if (UOcclusionFadeComponent* Fade = GetOcclusionFade())
	{
		Fade->bFadeEnabled = bEnabled;
	}
}
