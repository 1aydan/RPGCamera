#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FadeableTarget.generated.h"

class UPrimitiveComponent;

UINTERFACE(BlueprintType, MinimalAPI)
class UFadeableTarget : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implement on any actor that wants to know when it is blocking the player's view.
 *
 * Implementing this interface is optional. If UOcclusionFadeComponent's
 * bRequireFadeableInterface is false, actors are faded whether or not they
 * implement it - the events simply won't fire.
 */
class RPGCAMERA_API IFadeableTarget
{
	GENERATED_BODY()

public:
	/** Called once when this actor starts blocking the view. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RPG Camera|Fade")
	void OnFadeOutBegin();

	/** Called once when this actor stops blocking the view. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RPG Camera|Fade")
	void OnFadeInBegin();

	/**
	 * Called every frame while the fade alpha is changing.
	 * @param Primitive The component being faded.
	 * @param Alpha     1 = fully opaque, 0 = fully faded.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RPG Camera|Fade")
	void OnFadeAlphaChanged(UPrimitiveComponent* Primitive, float Alpha);

	/**
	 * Return false to have the fade component skip this actor entirely.
	 * Handy for actors that should never disappear, such as the ground.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RPG Camera|Fade")
	bool CanBeFaded();
};
