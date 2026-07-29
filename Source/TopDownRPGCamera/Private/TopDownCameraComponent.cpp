#include "TopDownCameraComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TopDownRPGCameraModule.h"

UTopDownCameraComponent::UTopDownCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	// We drive position and rotation ourselves.
	bUsePawnControlRotation = false;
	bInheritPitch = false;
	bInheritYaw = false;
	bInheritRoll = false;

	// Our own focus interpolation replaces the spring arm's lag.
	bEnableCameraLag = false;
	bEnableCameraRotationLag = false;

	// Top-down cameras usually shouldn't pull in on geometry; the fade
	// component handles obstruction instead. Turn this on if you want both.
	bDoCollisionTest = false;

	TargetArmLength = DefaultArmLength;
}

void UTopDownCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// Absolute transform lets the arm lag behind or detach from its owner
	// without needing a separate camera actor.
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);

	MaxArmLength = FMath::Max(MaxArmLength, MinArmLength);

	ResolveManagedCamera();

	if (bUseDiscreteZoomLevels)
	{
		CurrentZoomLevel = FMath::Clamp(DefaultZoomLevel, 0, FMath::Max(0, GetNumZoomLevels() - 1));
		GoalArmLength = GetZoomLevelDistance(CurrentZoomLevel);
	}
	else
	{
		GoalArmLength = FMath::Clamp(DefaultArmLength, MinArmLength, MaxArmLength);
	}
	TargetArmLength = GoalArmLength;

	CurrentYaw = DefaultYaw;
	GoalYaw = DefaultYaw;

	GoalFOV = DefaultFOV;
	CurrentFOV = DefaultFOV;
	UpdateFOV(0.f);

	if (!FollowTarget.IsValid())
	{
		FollowTarget = GetOwner();
	}

	UpdatePitch();

	CurrentFocus = ComputeFollowFocus(0.f);
	FreeRoamFocus = CurrentFocus;
	bHasInitializedFocus = true;

	SetWorldLocationAndRotation(CurrentFocus, FRotator(CurrentPitch, CurrentYaw, 0.f));
}

#if WITH_EDITOR
void UTopDownCameraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	MaxArmLength = FMath::Max(MaxArmLength, MinArmLength);
	DefaultArmLength = FMath::Clamp(DefaultArmLength, MinArmLength, MaxArmLength);

	// Give designers a live preview in the viewport.
	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		TargetArmLength = DefaultArmLength;
		SetRelativeRotation(FRotator(bLinkPitchToZoom ? PitchAtMaxZoom : DefaultPitch, DefaultYaw, 0.f));
	}
}
#endif

void UTopDownCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (DeltaTime <= 0.f)
	{
		return;
	}

	ApplyEdgePan(DeltaTime);

	UpdateZoom(DeltaTime);
	UpdateYaw(DeltaTime);
	UpdatePitch();
	UpdateFOV(DeltaTime);
	UpdateFocus(DeltaTime);

	SetWorldLocationAndRotation(CurrentFocus, FRotator(CurrentPitch, CurrentYaw, 0.f));

	// Consume this frame's buffered input.
	PendingPanInput = FVector2D::ZeroVector;
	PendingYawInput = 0.f;
}

// ---------------------------------------------------------------------------
// Zoom
// ---------------------------------------------------------------------------

void UTopDownCameraComponent::AddZoomInput(float ZoomDelta)
{
	if (FMath::IsNearlyZero(ZoomDelta))
	{
		return;
	}

	if (bUseDiscreteZoomLevels)
	{
		// Positive input zooms in, which moves toward level 0.
		const int32 Steps = ZoomDelta > 0.f
			? -FMath::CeilToInt(ZoomDelta)
			: -FMath::FloorToInt(ZoomDelta);

		SetZoomLevel(CurrentZoomLevel + Steps, false);
		return;
	}

	// Positive input zooms in, which shortens the arm.
	SetZoomDistance(GoalArmLength - (ZoomDelta * ZoomStep), false);
}

int32 UTopDownCameraComponent::GetNumZoomLevels() const
{
	if (CustomZoomLevels.Num() > 0)
	{
		return CustomZoomLevels.Num();
	}
	return FMath::Max(2, ZoomLevelCount);
}

float UTopDownCameraComponent::GetZoomLevelDistance(int32 Level) const
{
	const int32 NumLevels = GetNumZoomLevels();
	const int32 Clamped = FMath::Clamp(Level, 0, NumLevels - 1);

	if (CustomZoomLevels.Num() > 0)
	{
		return FMath::Clamp(CustomZoomLevels[Clamped], MinArmLength, MaxArmLength);
	}

	// Evenly space the levels across the arm length range.
	const float Alpha = static_cast<float>(Clamped) / static_cast<float>(NumLevels - 1);
	return FMath::Lerp(MinArmLength, MaxArmLength, Alpha);
}

void UTopDownCameraComponent::SetZoomLevel(int32 Level, bool bImmediate)
{
	const int32 Clamped = FMath::Clamp(Level, 0, GetNumZoomLevels() - 1);
	if (Clamped == CurrentZoomLevel && !bImmediate)
	{
		return;
	}

	CurrentZoomLevel = Clamped;
	SetZoomDistance(GetZoomLevelDistance(CurrentZoomLevel), bImmediate);
}

// ---------------------------------------------------------------------------
// Field of view
// ---------------------------------------------------------------------------

void UTopDownCameraComponent::SetManagedCamera(UCameraComponent* NewCamera)
{
	ManagedCamera = NewCamera;
}

void UTopDownCameraComponent::ResolveManagedCamera()
{
	if (ManagedCamera.IsValid())
	{
		return;
	}

	// Prefer a camera parented directly to this arm.
	TArray<USceneComponent*> Children;
	GetChildrenComponents(true, Children);

	for (USceneComponent* Child : Children)
	{
		if (UCameraComponent* Cam = Cast<UCameraComponent>(Child))
		{
			ManagedCamera = Cam;
			return;
		}
	}

	// Fall back to any camera on the owning actor.
	if (const AActor* OwnerActor = GetOwner())
	{
		if (UCameraComponent* Cam = OwnerActor->FindComponentByClass<UCameraComponent>())
		{
			ManagedCamera = Cam;
		}
	}
}

void UTopDownCameraComponent::SetFieldOfView(float NewFOV, bool bImmediate)
{
	GoalFOV = FMath::Clamp(NewFOV, 5.f, 170.f);
	if (bImmediate)
	{
		CurrentFOV = GoalFOV;
	}
}

void UTopDownCameraComponent::UpdateFOV(float DeltaTime)
{
	if (!bManageFieldOfView)
	{
		return;
	}

	if (!ManagedCamera.IsValid())
	{
		ResolveManagedCamera();
		if (!ManagedCamera.IsValid())
		{
			return;
		}
	}

	if (bLinkFOVToZoom)
	{
		float Alpha = GetNormalizedZoom();

		if (const FRichCurve* Curve = FOVBlendCurve.GetRichCurveConst())
		{
			if (Curve->GetNumKeys() > 0)
			{
				Alpha = FMath::Clamp(Curve->Eval(Alpha), 0.f, 1.f);
			}
		}

		GoalFOV = FMath::Lerp(FOVAtMinZoom, FOVAtMaxZoom, Alpha);
	}

	if (DeltaTime <= 0.f || FOVInterpSpeed <= 0.f)
	{
		CurrentFOV = GoalFOV;
	}
	else
	{
		CurrentFOV = FMath::FInterpTo(CurrentFOV, GoalFOV, DeltaTime, FOVInterpSpeed);
	}

	ManagedCamera->SetFieldOfView(CurrentFOV);
}

void UTopDownCameraComponent::SetZoomDistance(float NewDistance, bool bImmediate)
{
	const float Clamped = FMath::Clamp(NewDistance, MinArmLength, MaxArmLength);
	if (FMath::IsNearlyEqual(Clamped, GoalArmLength) && !bImmediate)
	{
		return;
	}

	GoalArmLength = Clamped;

	if (bImmediate)
	{
		TargetArmLength = GoalArmLength;
		UpdatePitch();
	}

	OnZoomChanged.Broadcast(GoalArmLength);
}

float UTopDownCameraComponent::GetNormalizedZoom() const
{
	const float Range = MaxArmLength - MinArmLength;
	if (Range <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	return FMath::Clamp((TargetArmLength - MinArmLength) / Range, 0.f, 1.f);
}

void UTopDownCameraComponent::UpdateZoom(float DeltaTime)
{
	if (ZoomInterpSpeed <= 0.f)
	{
		TargetArmLength = GoalArmLength;
	}
	else
	{
		TargetArmLength = FMath::FInterpTo(TargetArmLength, GoalArmLength, DeltaTime, ZoomInterpSpeed);
	}
}

// ---------------------------------------------------------------------------
// Pitch
// ---------------------------------------------------------------------------

void UTopDownCameraComponent::UpdatePitch()
{
	if (!bLinkPitchToZoom)
	{
		CurrentPitch = FMath::Clamp(DefaultPitch, -89.f, 89.f);
		return;
	}

	float Alpha = GetNormalizedZoom();

	if (const FRichCurve* Curve = PitchBlendCurve.GetRichCurveConst())
	{
		if (Curve->GetNumKeys() > 0)
		{
			Alpha = FMath::Clamp(Curve->Eval(Alpha), 0.f, 1.f);
		}
	}

	CurrentPitch = FMath::Clamp(FMath::Lerp(PitchAtMinZoom, PitchAtMaxZoom, Alpha), -89.f, 89.f);
}

// ---------------------------------------------------------------------------
// Yaw
// ---------------------------------------------------------------------------

void UTopDownCameraComponent::AddYawInput(float AxisValue)
{
	if (YawMode != ETDYawMode::Continuous || FMath::IsNearlyZero(AxisValue))
	{
		return;
	}

	PendingYawInput += AxisValue;
	LastYawInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UTopDownCameraComponent::AddYawSteps(int32 Steps)
{
	if (YawMode == ETDYawMode::Locked || Steps == 0)
	{
		return;
	}

	GoalYaw += YawStepAngle * static_cast<float>(Steps);
	LastYawInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UTopDownCameraComponent::SetYaw(float NewYaw, bool bImmediate)
{
	GoalYaw = NewYaw;
	if (bImmediate)
	{
		CurrentYaw = NewYaw;
	}
}

void UTopDownCameraComponent::RecenterYaw(bool bImmediate)
{
	// Take the shortest path back rather than unwinding several turns.
	GoalYaw = CurrentYaw + FRotator::NormalizeAxis(DefaultYaw - CurrentYaw);
	if (bImmediate)
	{
		CurrentYaw = GoalYaw;
	}
}

void UTopDownCameraComponent::UpdateYaw(float DeltaTime)
{
	if (YawMode == ETDYawMode::Locked)
	{
		CurrentYaw = DefaultYaw;
		GoalYaw = DefaultYaw;
		return;
	}

	if (YawMode == ETDYawMode::Continuous && !FMath::IsNearlyZero(PendingYawInput))
	{
		GoalYaw += PendingYawInput * YawSpeed * DeltaTime;
	}
	else if (YawMode == ETDYawMode::Stepped && YawStepAngle > KINDA_SMALL_NUMBER)
	{
		// Keep the goal locked to exact increments even if SetYaw was called
		// with an arbitrary angle.
		const float Offset = GoalYaw - DefaultYaw;
		GoalYaw = DefaultYaw + FMath::RoundToFloat(Offset / YawStepAngle) * YawStepAngle;
	}

	if (bAutoRecenterYaw)
	{
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if ((Now - LastYawInputTime) >= YawRecenterDelay)
		{
			GoalYaw = CurrentYaw + FRotator::NormalizeAxis(DefaultYaw - CurrentYaw);
		}
	}

	if (YawInterpSpeed <= 0.f)
	{
		CurrentYaw = GoalYaw;
	}
	else
	{
		// Interpolate along the shortest arc so crossing 180 degrees doesn't spin.
		const float Delta = FRotator::NormalizeAxis(GoalYaw - CurrentYaw);
		CurrentYaw += Delta * FMath::Clamp(YawInterpSpeed * DeltaTime, 0.f, 1.f);
	}
}

// ---------------------------------------------------------------------------
// Panning
// ---------------------------------------------------------------------------

void UTopDownCameraComponent::AddPanInput(FVector2D PanInput)
{
	if (PanInput.IsNearlyZero())
	{
		return;
	}

	PendingPanInput += PanInput;
	LastPanInputTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	if (CameraMode == ETDCameraMode::FollowTarget && bPanSwitchesToFreeRoam)
	{
		SetCameraMode(ETDCameraMode::FreeRoam);
	}
}

void UTopDownCameraComponent::ApplyEdgePan(float DeltaTime)
{
	if (!bEnableEdgePan)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->bShowMouseCursor)
	{
		return;
	}

	float MouseX = 0.f, MouseY = 0.f;
	if (!PC->GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	int32 SizeX = 0, SizeY = 0;
	PC->GetViewportSize(SizeX, SizeY);
	if (SizeX <= 0 || SizeY <= 0)
	{
		return;
	}

	// Ignore a cursor that has left the viewport entirely.
	if (MouseX < 0.f || MouseY < 0.f || MouseX > SizeX || MouseY > SizeY)
	{
		return;
	}

	FVector2D EdgeInput = FVector2D::ZeroVector;

	if (MouseX <= EdgePanBorderPixels)					{ EdgeInput.X = -1.f; }
	else if (MouseX >= SizeX - EdgePanBorderPixels)		{ EdgeInput.X = 1.f; }

	// Screen Y grows downward; the top of the screen should pan forward.
	if (MouseY <= EdgePanBorderPixels)					{ EdgeInput.Y = 1.f; }
	else if (MouseY >= SizeY - EdgePanBorderPixels)		{ EdgeInput.Y = -1.f; }

	if (!EdgeInput.IsNearlyZero())
	{
		AddPanInput(EdgeInput * EdgePanSpeedScale);
	}
}

// ---------------------------------------------------------------------------
// Focus
// ---------------------------------------------------------------------------

FVector UTopDownCameraComponent::ComputeFollowFocus(float DeltaTime) const
{
	const AActor* Target = FollowTarget.Get();
	if (!Target)
	{
		return CurrentFocus;
	}

	FVector Focus = Target->GetActorLocation() + FocusOffset;

	if (bLeadTarget && LeadTime > 0.f)
	{
		FVector Lead = Target->GetVelocity() * LeadTime;
		Lead.Z = 0.f;
		Focus += Lead.GetClampedToMaxSize(MaxLeadDistance);
	}

	if (bMouseInfluence && MouseInfluenceStrength > 0.f)
	{
		FVector CursorPoint;
		if (GetCursorPointOnPlane(Focus.Z, CursorPoint))
		{
			FVector ToCursor = (CursorPoint - Focus) * MouseInfluenceStrength;
			ToCursor.Z = 0.f;
			Focus += ToCursor.GetClampedToMaxSize(MaxMouseInfluenceDistance);
		}
	}

	return Focus;
}

void UTopDownCameraComponent::UpdateFocus(float DeltaTime)
{
	if (CameraMode == ETDCameraMode::FreeRoam)
	{
		if (!PendingPanInput.IsNearlyZero())
		{
			const FRotator YawOnly(0.f, CurrentYaw, 0.f);
			const FRotationMatrix YawMatrix(YawOnly);

			FVector Forward = YawMatrix.GetUnitAxis(EAxis::X);
			FVector Right = YawMatrix.GetUnitAxis(EAxis::Y);
			Forward.Z = 0.f;
			Right.Z = 0.f;
			Forward.Normalize();
			Right.Normalize();

			float Speed = PanSpeed;
			if (bScalePanSpeedWithZoom && DefaultArmLength > KINDA_SMALL_NUMBER)
			{
				Speed *= (TargetArmLength / DefaultArmLength);
			}

			FVector2D Move = PendingPanInput;
			// Prevent diagonals from being faster than cardinals.
			if (Move.SizeSquared() > 1.f)
			{
				Move.Normalize();
			}

			FreeRoamFocus += (Forward * Move.Y + Right * Move.X) * Speed * DeltaTime;
		}

		FreeRoamFocus = ClampToBounds(FreeRoamFocus);
		CurrentFocus = FreeRoamFocus;

		if (bAutoReturnToTarget && FollowTarget.IsValid())
		{
			const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
			if ((Now - LastPanInputTime) >= ReturnToTargetDelay)
			{
				SetCameraMode(ETDCameraMode::FollowTarget);
			}
		}
		return;
	}

	const FVector Desired = ClampToBounds(ComputeFollowFocus(DeltaTime));

	const bool bShouldSnap =
		!bHasInitializedFocus ||
		FollowInterpSpeed <= 0.f ||
		(SnapDistanceThreshold > 0.f && FVector::DistSquared(CurrentFocus, Desired) > FMath::Square(SnapDistanceThreshold));

	CurrentFocus = bShouldSnap
		? Desired
		: FMath::VInterpTo(CurrentFocus, Desired, DeltaTime, FollowInterpSpeed);

	// Keep the free roam seed current so detaching never pops.
	FreeRoamFocus = CurrentFocus;
}

FVector UTopDownCameraComponent::ClampToBounds(const FVector& InLocation) const
{
	if (!bClampToBounds)
	{
		return InLocation;
	}

	const FVector Min = BoundsOrigin - BoundsExtent;
	const FVector Max = BoundsOrigin + BoundsExtent;

	FVector Result = InLocation;
	Result.X = FMath::Clamp(Result.X, Min.X, Max.X);
	Result.Y = FMath::Clamp(Result.Y, Min.Y, Max.Y);
	if (bClampVertical)
	{
		Result.Z = FMath::Clamp(Result.Z, Min.Z, Max.Z);
	}
	return Result;
}

// ---------------------------------------------------------------------------
// Mode & target
// ---------------------------------------------------------------------------

void UTopDownCameraComponent::SetCameraMode(ETDCameraMode NewMode)
{
	if (CameraMode == NewMode)
	{
		return;
	}

	CameraMode = NewMode;

	if (NewMode == ETDCameraMode::FreeRoam)
	{
		// Start free roam exactly where the camera already is.
		FreeRoamFocus = CurrentFocus;
	}

	OnCameraModeChanged.Broadcast(CameraMode);
}

void UTopDownCameraComponent::SetFollowTarget(AActor* NewTarget, bool bSnapImmediately)
{
	FollowTarget = NewTarget;

	if (bSnapImmediately)
	{
		SnapToTarget();
	}
}

void UTopDownCameraComponent::SnapToTarget()
{
	if (!FollowTarget.IsValid())
	{
		return;
	}

	SetCameraMode(ETDCameraMode::FollowTarget);

	CurrentFocus = ClampToBounds(ComputeFollowFocus(0.f));
	FreeRoamFocus = CurrentFocus;

	SetWorldLocationAndRotation(CurrentFocus, FRotator(CurrentPitch, CurrentYaw, 0.f));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

FVector UTopDownCameraComponent::GetPlanarForward() const
{
	FVector Forward = FRotationMatrix(FRotator(0.f, CurrentYaw, 0.f)).GetUnitAxis(EAxis::X);
	Forward.Z = 0.f;
	return Forward.GetSafeNormal();
}

FVector UTopDownCameraComponent::GetPlanarRight() const
{
	FVector Right = FRotationMatrix(FRotator(0.f, CurrentYaw, 0.f)).GetUnitAxis(EAxis::Y);
	Right.Z = 0.f;
	return Right.GetSafeNormal();
}

APlayerController* UTopDownCameraComponent::GetOwningPlayerController() const
{
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			return PC;
		}
	}

	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		return PC;
	}

	return UGameplayStatics::GetPlayerController(this, 0);
}

bool UTopDownCameraComponent::GetCursorPointOnPlane(float PlaneZ, FVector& OutLocation) const
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return false;
	}

	FVector RayOrigin, RayDirection;
	if (!PC->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return false;
	}

	// A near-horizontal ray never meaningfully meets a horizontal plane.
	if (FMath::Abs(RayDirection.Z) < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float Distance = (PlaneZ - RayOrigin.Z) / RayDirection.Z;
	if (Distance <= 0.f)
	{
		return false;
	}

	OutLocation = RayOrigin + RayDirection * Distance;
	return true;
}
