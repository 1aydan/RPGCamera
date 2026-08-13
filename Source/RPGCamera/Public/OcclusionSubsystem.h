// Copyright (c) 2026. Licensed for use in your own projects.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "OcclusionSubsystem.generated.h"

class AOcclusionFadeGroup;

/**
 * World-level bookkeeping for the occlusion system.
 *
 * Today that means the actor -> group index, so the occlusion sweep can ask
 * "who else fades with this?" in constant time instead of walking every group
 * in the level. Groups register themselves on BeginPlay and re-index on
 * RefreshMembers; you rarely need to touch this directly.
 */
UCLASS()
class RPGCAMERA_API UOcclusionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	// ---------------------------------------------------------------------
	// Groups
	// ---------------------------------------------------------------------

	void RegisterGroup(AOcclusionFadeGroup* Group);
	void UnregisterGroup(AOcclusionFadeGroup* Group);

	/** Re-index a group whose membership changed. Safe to call on an unregistered group. */
	void RefreshGroup(AOcclusionFadeGroup* Group);

	/** Appends every enabled group Actor belongs to. Returns true if any were found. */
	bool GetGroupsForActor(const AActor* Actor, TArray<AOcclusionFadeGroup*>& OutGroups) const;

	/**
	 * The group whose settings win for Actor when it fades. Prefers a group
	 * that overrides the fade look; otherwise returns the first match.
	 */
	AOcclusionFadeGroup* FindSettingsGroup(const AActor* Actor) const;

	UFUNCTION(BlueprintPure, Category = "Occlusion")
	TArray<AOcclusionFadeGroup*> GetGroupsContainingActor(AActor* Actor) const;

	UFUNCTION(BlueprintPure, Category = "Occlusion")
	TArray<AOcclusionFadeGroup*> GetRegisteredGroups() const;

private:
	UPROPERTY()
	TArray<TObjectPtr<AOcclusionFadeGroup>> Groups;

	TMap<FObjectKey, TArray<TWeakObjectPtr<AOcclusionFadeGroup>>> ActorToGroups;

	void IndexGroup(AOcclusionFadeGroup* Group);
	void RemoveGroupFromIndex(const AOcclusionFadeGroup* Group);
};
