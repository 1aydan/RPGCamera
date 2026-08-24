// Copyright (c) 2026. Licensed for use in your own projects.

#include "OcclusionFadeGroup.h"

#include "Components/BrushComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "OcclusionSubsystem.h"
#include "RPGCameraModule.h"

AOcclusionFadeGroup::AOcclusionFadeGroup()
{
	PrimaryActorTick.bCanEverTick = false;

	// The camera sweep has to see the brush for the volume to trigger the group.
	UBrushComponent* VolumeBrush = GetBrushComponent();
	if (VolumeBrush)
	{
		VolumeBrush->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		VolumeBrush->SetCollisionResponseToAllChannels(ECR_Overlap);
	}

#if WITH_EDITORONLY_DATA
	bColored = true;
	BrushColor = FColor(120, 190, 255, 255);
#endif
}

void AOcclusionFadeGroup::BeginPlay()
{
	Super::BeginPlay();

	RefreshMembers();
}

void AOcclusionFadeGroup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		if (UOcclusionSubsystem* Registry = World->GetSubsystem<UOcclusionSubsystem>())
		{
			Registry->UnregisterGroup(this);
		}
	}

	Members.Reset();
	MemberCount = 0;
	bOccluding = false;

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Membership
// ---------------------------------------------------------------------------

void AOcclusionFadeGroup::RefreshMembers()
{
	Members.Reset();

	if (bCaptureOverlappingActors)
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Candidate = *It;

				// Group volumes are triggers, not geometry - never swallow one.
				if (!IsValid(Candidate) || Candidate->IsA<AOcclusionFadeGroup>())
				{
					continue;
				}

				if (PassesCaptureFilters(Candidate) && IsActorInsideVolume(Candidate))
				{
					Members.Add(Candidate);
				}
			}
		}
	}

	// Explicit members bypass the filters entirely - you asked for them by name.
	for (const TObjectPtr<AActor>& Entry : AdditionalMembers)
	{
		if (IsValid(Entry))
		{
			Members.Add(Entry);
		}
	}

	for (const TObjectPtr<AActor>& Entry : ExcludedActors)
	{
		if (Entry)
		{
			Members.Remove(Entry);
		}
	}

	MemberCount = Members.Num();

	if (const UWorld* World = GetWorld())
	{
		if (UOcclusionSubsystem* Registry = World->GetSubsystem<UOcclusionSubsystem>())
		{
			Registry->UnregisterGroup(this);
			Registry->RegisterGroup(this);
		}
	}

	UE_LOG(LogRPGCamera, Verbose, TEXT("Occlusion group '%s' resolved %d member(s)."), *GetName(), MemberCount);
}

bool AOcclusionFadeGroup::PassesCaptureFilters(const AActor* Actor) const
{
	if (CaptureActorTags.Num() > 0)
	{
		bool bHasTag = false;
		for (const FName& Tag : CaptureActorTags)
		{
			if (Actor->ActorHasTag(Tag))
			{
				bHasTag = true;
				break;
			}
		}
		if (!bHasTag)
		{
			return false;
		}
	}

	if (CaptureActorClasses.Num() > 0)
	{
		bool bIsClass = false;
		for (const TSubclassOf<AActor>& Class : CaptureActorClasses)
		{
			if (Class && Actor->IsA(Class))
			{
				bIsClass = true;
				break;
			}
		}
		if (!bIsClass)
		{
			return false;
		}
	}

	return true;
}

bool AOcclusionFadeGroup::IsRenderableCandidate(const UPrimitiveComponent* Primitive)
{
	if (!Primitive || !Primitive->IsRegistered())
	{
		return false;
	}

	// IsVisualizationComponent() only exists behind WITH_EDITORONLY_DATA, and
	// visualization components are stripped from cooked builds anyway.
#if WITH_EDITORONLY_DATA
	if (Primitive->IsVisualizationComponent())
	{
		return false;
	}
#endif

	return true;
}

bool AOcclusionFadeGroup::IsActorInsideVolume(const AActor* Actor) const
{
	if (EncompassesPoint(Actor->GetActorLocation(), CaptureTolerance))
	{
		return true;
	}

	// A modular wall's pivot often sits at a corner or on the floor below, so
	// fall back to where its geometry actually is.
	for (const UActorComponent* Component : Actor->GetComponents())
	{
		const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
		if (!IsRenderableCandidate(Primitive))
		{
			continue;
		}

		if (EncompassesPoint(Primitive->Bounds.Origin, CaptureTolerance))
		{
			return true;
		}
	}

	return false;
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

TArray<AActor*> AOcclusionFadeGroup::GetMembers() const
{
	TArray<AActor*> Result;
	Result.Reserve(Members.Num());

	for (const TWeakObjectPtr<AActor>& Member : Members)
	{
		if (AActor* Actor = Member.Get())
		{
			Result.Add(Actor);
		}
	}

	return Result;
}

bool AOcclusionFadeGroup::ContainsActor(AActor* Actor) const
{
	return IsValid(Actor) && Members.Contains(Actor);
}

void AOcclusionFadeGroup::AddMember(AActor* Actor)
{
	if (!IsValid(Actor) || Actor == this)
	{
		return;
	}

	bool bAlreadyPresent = false;
	Members.Add(Actor, &bAlreadyPresent);
	if (bAlreadyPresent)
	{
		return;
	}

	MemberCount = Members.Num();

	if (const UWorld* World = GetWorld())
	{
		if (UOcclusionSubsystem* Registry = World->GetSubsystem<UOcclusionSubsystem>())
		{
			Registry->RefreshGroup(this);
		}
	}
}

void AOcclusionFadeGroup::RemoveMember(AActor* Actor)
{
	if (Members.Remove(Actor) == 0)
	{
		return;
	}

	MemberCount = Members.Num();

	if (const UWorld* World = GetWorld())
	{
		if (UOcclusionSubsystem* Registry = World->GetSubsystem<UOcclusionSubsystem>())
		{
			Registry->RefreshGroup(this);
		}
	}
}

void AOcclusionFadeGroup::AppendMemberPrimitives(TSet<UPrimitiveComponent*>& OutPrimitives) const
{
	for (const TWeakObjectPtr<AActor>& Member : Members)
	{
		const AActor* Actor = Member.Get();
		if (!IsValid(Actor))
		{
			continue;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
			if (!IsRenderableCandidate(Primitive))
			{
				continue;
			}

			// Materials are the cheap tell for "this actually renders" - it
			// skips collision shapes, arrows and other non-visual primitives.
			if (Primitive->GetNumMaterials() <= 0)
			{
				continue;
			}

			OutPrimitives.Add(Primitive);
		}
	}
}

void AOcclusionFadeGroup::NotifyOccluding(bool bNowOccluding)
{
	if (bOccluding == bNowOccluding)
	{
		return;
	}

	bOccluding = bNowOccluding;

	if (bNowOccluding)
	{
		OnGroupBeganOccluding.Broadcast(this);
	}
	else
	{
		OnGroupStoppedOccluding.Broadcast(this);
	}
}
