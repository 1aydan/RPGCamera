#include "OcclusionFadeComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "FadeableTarget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TopDownRPGCameraModule.h"

UOcclusionFadeComponent::UOcclusionFadeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UOcclusionFadeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!ViewTarget.IsValid())
	{
		ViewTarget = GetOwner();
	}
}

void UOcclusionFadeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAllFades();
	Super::EndPlay(EndPlayReason);
}

void UOcclusionFadeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bFadeEnabled)
	{
		// Let anything still faded return to normal, then stop.
		UpdateFadeAlphas(DeltaTime);
		return;
	}

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	if (TraceInterval <= 0.f || (Now - LastTraceTime) >= TraceInterval)
	{
		LastTraceTime = Now;
		PerformOcclusionTrace();
	}

	UpdateFadeAlphas(DeltaTime);
}

// ---------------------------------------------------------------------------
// Detection
// ---------------------------------------------------------------------------

void UOcclusionFadeComponent::PerformOcclusionTrace()
{
	UWorld* World = GetWorld();
	if (!World || !ViewTarget.IsValid())
	{
		return;
	}

	FVector CameraLocation;
	if (!GetCameraLocation(CameraLocation))
	{
		return;
	}

	const FVector TargetLocation = GetViewTargetLocation();

	FVector ToCamera = CameraLocation - TargetLocation;
	const float Distance = ToCamera.Size();
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	ToCamera /= Distance;

	// Sweep from the character toward the camera, stopping short of the lens.
	const float SweepLength = FMath::Max(0.f, Distance - CameraPadding);
	const FVector SweepEnd = TargetLocation + ToCamera * SweepLength;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(TopDownOcclusionFade), bTraceComplex);
	Params.AddIgnoredActor(ViewTarget.Get());
	if (GetOwner() != ViewTarget.Get())
	{
		Params.AddIgnoredActor(GetOwner());
	}

	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(
		Hits,
		TargetLocation,
		SweepEnd,
		FQuat::Identity,
		TraceChannel,
		FCollisionShape::MakeSphere(TraceRadius),
		Params);

	// Mark everything as clear, then re-flag what the sweep found.
	for (TPair<TObjectPtr<UPrimitiveComponent>, FTDFadeState>& Pair : FadeStates)
	{
		Pair.Value.bOccluding = false;
	}

	TSet<TWeakObjectPtr<AActor>> CurrentOccluders;

	for (const FHitResult& Hit : Hits)
	{
		UPrimitiveComponent* Primitive = Hit.GetComponent();
		if (!ShouldFadePrimitive(Primitive))
		{
			continue;
		}

		FTDFadeState& State = FadeStates.FindOrAdd(Primitive);
		const bool bIsNew = !State.bCachedOriginals;

		if (bIsNew)
		{
			CacheOriginals(Primitive, State);
		}

		State.bOccluding = true;

		if (AActor* HitActor = Hit.GetActor())
		{
			CurrentOccluders.Add(HitActor);
		}
	}

	// Fire begin/end events at actor granularity.
	for (const TWeakObjectPtr<AActor>& Actor : CurrentOccluders)
	{
		if (!PreviousOccluders.Contains(Actor) && Actor.IsValid())
		{
			if (Actor->GetClass()->ImplementsInterface(UFadeableTarget::StaticClass()))
			{
				IFadeableTarget::Execute_OnFadeOutBegin(Actor.Get());
			}
			OnActorBeganOccluding.Broadcast(Actor.Get());
		}
	}

	for (const TWeakObjectPtr<AActor>& Actor : PreviousOccluders)
	{
		if (!CurrentOccluders.Contains(Actor) && Actor.IsValid())
		{
			if (Actor->GetClass()->ImplementsInterface(UFadeableTarget::StaticClass()))
			{
				IFadeableTarget::Execute_OnFadeInBegin(Actor.Get());
			}
			OnActorStoppedOccluding.Broadcast(Actor.Get());
		}
	}

	PreviousOccluders = MoveTemp(CurrentOccluders);
}

bool UOcclusionFadeComponent::ShouldFadePrimitive(const UPrimitiveComponent* Primitive) const
{
	if (!IsValid(Primitive))
	{
		return false;
	}

	const AActor* Actor = Primitive->GetOwner();
	if (!IsValid(Actor) || Actor == ViewTarget.Get())
	{
		return false;
	}

	for (const FName& Tag : IgnoredComponentTags)
	{
		if (Primitive->ComponentHasTag(Tag))
		{
			return false;
		}
	}

	for (const FName& Tag : IgnoredActorTags)
	{
		if (Actor->ActorHasTag(Tag))
		{
			return false;
		}
	}

	for (const TSubclassOf<AActor>& Class : IgnoredActorClasses)
	{
		if (Class && Actor->IsA(Class))
		{
			return false;
		}
	}

	if (RequiredActorTags.Num() > 0)
	{
		bool bHasRequiredTag = false;
		for (const FName& Tag : RequiredActorTags)
		{
			if (Actor->ActorHasTag(Tag))
			{
				bHasRequiredTag = true;
				break;
			}
		}
		if (!bHasRequiredTag)
		{
			return false;
		}
	}

	const bool bImplementsInterface = Actor->GetClass()->ImplementsInterface(UFadeableTarget::StaticClass());

	if (bRequireFadeableInterface && !bImplementsInterface)
	{
		return false;
	}

	// Let the actor veto on its own terms. Execute_ wrappers are non-const.
	if (bImplementsInterface && !IFadeableTarget::Execute_CanBeFaded(const_cast<AActor*>(Actor)))
	{
		return false;
	}

	return true;
}

// ---------------------------------------------------------------------------
// Fading
// ---------------------------------------------------------------------------

void UOcclusionFadeComponent::UpdateFadeAlphas(float DeltaTime)
{
	if (DeltaTime <= 0.f || FadeStates.Num() == 0)
	{
		return;
	}

	TArray<TObjectPtr<UPrimitiveComponent>> Finished;

	for (TPair<TObjectPtr<UPrimitiveComponent>, FTDFadeState>& Pair : FadeStates)
	{
		UPrimitiveComponent* Primitive = Pair.Key;
		FTDFadeState& State = Pair.Value;

		if (!IsValid(Primitive))
		{
			Finished.Add(Pair.Key);
			continue;
		}

		const bool bWantsFade = State.bOccluding && bFadeEnabled;
		const float GoalAlpha = bWantsFade ? FadedAlpha : 1.f;
		const float Speed = bWantsFade ? FadeOutSpeed : FadeInSpeed;

		if (!FMath::IsNearlyEqual(State.Alpha, GoalAlpha, 0.001f))
		{
			State.Alpha = FMath::FInterpConstantTo(State.Alpha, GoalAlpha, DeltaTime, Speed);
			ApplyFade(Primitive, State);

			if (const AActor* Actor = Primitive->GetOwner())
			{
				if (Actor->GetClass()->ImplementsInterface(UFadeableTarget::StaticClass()))
				{
					IFadeableTarget::Execute_OnFadeAlphaChanged(Primitive->GetOwner(), Primitive, State.Alpha);
				}
			}
		}
		else if (!bWantsFade)
		{
			// Fully restored and no longer occluding: stop tracking it.
			State.Alpha = 1.f;
			RestorePrimitive(Primitive, State);
			Finished.Add(Pair.Key);
		}
	}

	for (const TObjectPtr<UPrimitiveComponent>& Key : Finished)
	{
		FadeStates.Remove(Key);
	}
}

void UOcclusionFadeComponent::CacheOriginals(UPrimitiveComponent* Primitive, FTDFadeState& State)
{
	if (!IsValid(Primitive) || State.bCachedOriginals)
	{
		return;
	}

	State.bOriginalVisibility = Primitive->IsVisible();
	State.bOriginalCastHiddenShadow = Primitive->bCastHiddenShadow;
	State.Alpha = 1.f;
	State.bCachedOriginals = true;

	if (FadeMethod == ETDFadeMethod::MaterialParameter)
	{
		const int32 NumMaterials = Primitive->GetNumMaterials();
		State.DynamicMaterials.Reserve(NumMaterials);

		for (int32 Index = 0; Index < NumMaterials; ++Index)
		{
			if (UMaterialInstanceDynamic* MID = Primitive->CreateAndSetMaterialInstanceDynamic(Index))
			{
				State.DynamicMaterials.Add(MID);
			}
		}
	}
}

void UOcclusionFadeComponent::ApplyFade(UPrimitiveComponent* Primitive, FTDFadeState& State)
{
	if (!IsValid(Primitive))
	{
		return;
	}

	switch (FadeMethod)
	{
	case ETDFadeMethod::CustomPrimitiveData:
		Primitive->SetCustomPrimitiveDataFloat(CustomPrimitiveDataIndex, State.Alpha);
		break;

	case ETDFadeMethod::MaterialParameter:
		for (UMaterialInstanceDynamic* MID : State.DynamicMaterials)
		{
			if (IsValid(MID))
			{
				MID->SetScalarParameterValue(FadeParameterName, State.Alpha);
			}
		}
		break;

	case ETDFadeMethod::HideComponent:
	{
		// Binary rather than gradual: hide once we're past the midpoint.
		const bool bShouldBeVisible = State.Alpha > 0.5f;
		if (Primitive->IsVisible() != bShouldBeVisible)
		{
			if (bKeepShadowsWhenHidden)
			{
				Primitive->bCastHiddenShadow = !bShouldBeVisible ? true : State.bOriginalCastHiddenShadow;
				Primitive->MarkRenderStateDirty();
			}
			Primitive->SetVisibility(bShouldBeVisible && State.bOriginalVisibility);
		}
		break;
	}

	case ETDFadeMethod::InterfaceOnly:
	default:
		// The interface event in UpdateFadeAlphas does all the work.
		break;
	}
}

void UOcclusionFadeComponent::RestorePrimitive(UPrimitiveComponent* Primitive, FTDFadeState& State)
{
	if (!IsValid(Primitive))
	{
		return;
	}

	switch (FadeMethod)
	{
	case ETDFadeMethod::CustomPrimitiveData:
		Primitive->SetCustomPrimitiveDataFloat(CustomPrimitiveDataIndex, 1.f);
		break;

	case ETDFadeMethod::MaterialParameter:
		for (UMaterialInstanceDynamic* MID : State.DynamicMaterials)
		{
			if (IsValid(MID))
			{
				MID->SetScalarParameterValue(FadeParameterName, 1.f);
			}
		}
		break;

	case ETDFadeMethod::HideComponent:
		Primitive->bCastHiddenShadow = State.bOriginalCastHiddenShadow;
		Primitive->SetVisibility(State.bOriginalVisibility);
		Primitive->MarkRenderStateDirty();
		break;

	default:
		break;
	}

	State.DynamicMaterials.Reset();
}

void UOcclusionFadeComponent::ClearAllFades()
{
	for (TPair<TObjectPtr<UPrimitiveComponent>, FTDFadeState>& Pair : FadeStates)
	{
		if (IsValid(Pair.Key))
		{
			Pair.Value.Alpha = 1.f;
			RestorePrimitive(Pair.Key, Pair.Value);
		}
	}

	FadeStates.Empty();
	PreviousOccluders.Empty();
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

TArray<AActor*> UOcclusionFadeComponent::GetOccludingActors() const
{
	TArray<AActor*> Result;

	for (const TPair<TObjectPtr<UPrimitiveComponent>, FTDFadeState>& Pair : FadeStates)
	{
		if (!Pair.Value.bOccluding || !IsValid(Pair.Key))
		{
			continue;
		}

		if (AActor* Actor = Pair.Key->GetOwner())
		{
			Result.AddUnique(Actor);
		}
	}

	return Result;
}

void UOcclusionFadeComponent::SetViewTarget(AActor* NewTarget)
{
	if (ViewTarget.Get() == NewTarget)
	{
		return;
	}

	ClearAllFades();
	ViewTarget = NewTarget;
}

void UOcclusionFadeComponent::SetCameraOverride(UCameraComponent* NewCamera)
{
	CameraOverride = NewCamera;
}

bool UOcclusionFadeComponent::GetCameraLocation(FVector& OutLocation) const
{
	if (CameraOverride.IsValid())
	{
		OutLocation = CameraOverride->GetComponentLocation();
		return true;
	}

	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (const APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
		{
			OutLocation = CameraManager->GetCameraLocation();
			return true;
		}
	}

	return false;
}

FVector UOcclusionFadeComponent::GetViewTargetLocation() const
{
	const AActor* Target = ViewTarget.Get();
	return Target ? Target->GetActorLocation() + ViewTargetOffset : FVector::ZeroVector;
}
