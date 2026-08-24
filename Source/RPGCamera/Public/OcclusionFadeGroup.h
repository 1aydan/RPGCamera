// Copyright (c) 2026. Licensed for use in your own projects.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "RPGCameraTypes.h"
#include "OcclusionFadeGroup.generated.h"

class AOcclusionFadeGroup;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRPGOcclusionGroupChanged, AOcclusionFadeGroup*, Group);

/**
 * A volume that makes everything inside it fade as one.
 *
 * Drop one over a building, a room, or a cluster of trees. When the occlusion
 * sweep crosses the volume or hits a member, every member fades together - so
 * a roof, its walls and its chimney disappear as one object instead of the
 * sweep carving a hole through whichever piece happened to be in the way.
 *
 * Membership is the volume's overlap plus AdditionalMembers, minus
 * ExcludedActors. It is resolved once at BeginPlay; call RefreshMembers after
 * spawning or streaming in geometry that should join.
 *
 * The group can also override the fade look for its members, so a roof group
 * can hard-hide while a tree group only ghosts.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Occlusion Fade Group"))
class RPGCAMERA_API AOcclusionFadeGroup : public AVolume
{
	GENERATED_BODY()

public:
	AOcclusionFadeGroup();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------------------------------------------------------------------
	// Membership
	// ---------------------------------------------------------------------

	/** Turn the group off without deleting it. Members then fade individually. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Group")
	bool bGroupEnabled = true;

	/** Fade the group when the sweep crosses the volume itself, not only when it hits a member. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Group")
	bool bVolumeTriggersFade = true;

	/** Take in every actor overlapping the volume. Off = AdditionalMembers only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Group|Membership")
	bool bCaptureOverlappingActors = true;

	/** If non-empty, only actors carrying one of these tags are captured. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Group|Membership", meta = (EditCondition = "bCaptureOverlappingActors"))
	TArray<FName> CaptureActorTags;

	/** If non-empty, only actors of one of these classes are captured. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Group|Membership", meta = (EditCondition = "bCaptureOverlappingActors"))
	TArray<TSubclassOf<AActor>> CaptureActorClasses;

	/**
	 * How far outside the volume a mesh may sit and still be captured. Gives
	 * you slack for eaves and trim that poke through the wall you drew.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Group|Membership", meta = (EditCondition = "bCaptureOverlappingActors", ClampMin = "0.0", Units = "cm"))
	float CaptureTolerance = 50.f;

	/** Members that sit outside the volume entirely, e.g. a balcony or a far chimney. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Group|Membership")
	TArray<TObjectPtr<AActor>> AdditionalMembers;

	/** Actors the volume would otherwise swallow, e.g. the floor you want to keep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Group|Membership")
	TArray<TObjectPtr<AActor>> ExcludedActors;

	/** How many actors the last capture resolved to. Editor readout only. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Occlusion Group|Membership")
	int32 MemberCount = 0;

	// ---------------------------------------------------------------------
	// Appearance
	// ---------------------------------------------------------------------

	/** Use this group's FadeSettings instead of the fade component's own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Group|Appearance")
	bool bOverrideFadeSettings = false;

	/**
	 * Look applied to this group's members. Only read when
	 * bOverrideFadeSettings is on; otherwise members inherit the fade
	 * component's settings.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Group|Appearance", meta = (EditCondition = "bOverrideFadeSettings"))
	FRPGFadeSettings FadeSettings;

	// ---------------------------------------------------------------------
	// Events
	// ---------------------------------------------------------------------

	/** Fired once when any member starts blocking the view. */
	UPROPERTY(BlueprintAssignable, Category = "Occlusion Group|Events")
	FRPGOcclusionGroupChanged OnGroupBeganOccluding;

	/** Fired once when no member blocks the view any more. */
	UPROPERTY(BlueprintAssignable, Category = "Occlusion Group|Events")
	FRPGOcclusionGroupChanged OnGroupStoppedOccluding;

	// ---------------------------------------------------------------------
	// API
	// ---------------------------------------------------------------------

	/**
	 * Re-resolve membership from the volume and the member lists, then
	 * re-index with the group registry. Call after spawning or streaming in
	 * geometry that belongs to this group.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Occlusion Group")
	void RefreshMembers();

	UFUNCTION(BlueprintPure, Category = "Occlusion Group")
	TArray<AActor*> GetMembers() const;

	UFUNCTION(BlueprintPure, Category = "Occlusion Group")
	bool ContainsActor(AActor* Actor) const;

	/** Add an actor to the group at runtime. Survives until the next RefreshMembers. */
	UFUNCTION(BlueprintCallable, Category = "Occlusion Group")
	void AddMember(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Occlusion Group")
	void RemoveMember(AActor* Actor);

	/** True while the fade component reports this group as blocking the view. */
	UFUNCTION(BlueprintPure, Category = "Occlusion Group")
	bool IsOccluding() const { return bOccluding; }

	/** Adds the group's renderable primitives. Used by UOcclusionFadeComponent. */
	void AppendMemberPrimitives(TSet<UPrimitiveComponent*>& OutPrimitives) const;

	/** Called by UOcclusionFadeComponent when the group's occlusion state flips. */
	void NotifyOccluding(bool bNowOccluding);

	const TSet<TWeakObjectPtr<AActor>>& GetMemberSet() const { return Members; }

protected:
	/** Resolved membership. Weak so streamed-out actors drop out on their own. */
	TSet<TWeakObjectPtr<AActor>> Members;

	bool bOccluding = false;

	/** True if Actor passes the capture tag/class filters. */
	bool PassesCaptureFilters(const AActor* Actor) const;

	/** True if any part of Actor sits inside the volume, within CaptureTolerance. */
	bool IsActorInsideVolume(const AActor* Actor) const;

	/** True if Primitive is live geometry rather than an editor gizmo. */
	static bool IsRenderableCandidate(const UPrimitiveComponent* Primitive);
};
