// Copyright (c) 2026. Licensed for use in your own projects.

#include "OcclusionSubsystem.h"

#include "OcclusionFadeGroup.h"

void UOcclusionSubsystem::Deinitialize()
{
	Groups.Reset();
	ActorToGroups.Reset();

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Groups
// ---------------------------------------------------------------------------

void UOcclusionSubsystem::RegisterGroup(AOcclusionFadeGroup* Group)
{
	if (!IsValid(Group))
	{
		return;
	}

	Groups.AddUnique(Group);
	IndexGroup(Group);
}

void UOcclusionSubsystem::UnregisterGroup(AOcclusionFadeGroup* Group)
{
	if (!Group)
	{
		return;
	}

	Groups.Remove(Group);
	RemoveGroupFromIndex(Group);
}

void UOcclusionSubsystem::RefreshGroup(AOcclusionFadeGroup* Group)
{
	if (!IsValid(Group))
	{
		return;
	}

	RemoveGroupFromIndex(Group);

	if (Groups.Contains(Group))
	{
		IndexGroup(Group);
	}
}

void UOcclusionSubsystem::IndexGroup(AOcclusionFadeGroup* Group)
{
	for (const TWeakObjectPtr<AActor>& Member : Group->GetMemberSet())
	{
		if (const AActor* Actor = Member.Get())
		{
			ActorToGroups.FindOrAdd(FObjectKey(Actor)).AddUnique(Group);
		}
	}
}

void UOcclusionSubsystem::RemoveGroupFromIndex(const AOcclusionFadeGroup* Group)
{
	// Walk the whole index rather than the group's members: membership may
	// already have changed, and stale entries would outlive the group.
	for (auto It = ActorToGroups.CreateIterator(); It; ++It)
	{
		It.Value().RemoveAll([Group](const TWeakObjectPtr<AOcclusionFadeGroup>& Entry)
		{
			return Entry.Get() == Group || !Entry.IsValid();
		});

		if (It.Value().Num() == 0)
		{
			It.RemoveCurrent();
		}
	}
}

bool UOcclusionSubsystem::GetGroupsForActor(const AActor* Actor, TArray<AOcclusionFadeGroup*>& OutGroups) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	const TArray<TWeakObjectPtr<AOcclusionFadeGroup>>* Found = ActorToGroups.Find(FObjectKey(Actor));
	if (!Found)
	{
		return false;
	}

	bool bAdded = false;

	for (const TWeakObjectPtr<AOcclusionFadeGroup>& Entry : *Found)
	{
		AOcclusionFadeGroup* Group = Entry.Get();
		if (IsValid(Group) && Group->bGroupEnabled)
		{
			OutGroups.AddUnique(Group);
			bAdded = true;
		}
	}

	return bAdded;
}

AOcclusionFadeGroup* UOcclusionSubsystem::FindSettingsGroup(const AActor* Actor) const
{
	TArray<AOcclusionFadeGroup*> Found;
	if (!GetGroupsForActor(Actor, Found))
	{
		return nullptr;
	}

	// Overlapping volumes are legal; the one that actually changes the look wins.
	for (AOcclusionFadeGroup* Group : Found)
	{
		if (Group->bOverrideFadeSettings)
		{
			return Group;
		}
	}

	return Found[0];
}

TArray<AOcclusionFadeGroup*> UOcclusionSubsystem::GetGroupsContainingActor(AActor* Actor) const
{
	TArray<AOcclusionFadeGroup*> Result;
	GetGroupsForActor(Actor, Result);
	return Result;
}

TArray<AOcclusionFadeGroup*> UOcclusionSubsystem::GetRegisteredGroups() const
{
	TArray<AOcclusionFadeGroup*> Result;
	Result.Reserve(Groups.Num());

	for (const TObjectPtr<AOcclusionFadeGroup>& Group : Groups)
	{
		if (IsValid(Group))
		{
			Result.Add(Group);
		}
	}

	return Result;
}
