#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VRInputComboSet.generated.h"

class UInputAction;
class UVRComboCondition;
class UVRComboEdge;

/**
 * One combination: when the Edge fires on EdgeAction and every Condition
 * passes, OutputAction is triggered.
 *
 * Reads as a sentence -- "flick IA_Smooth_Locomotion while IA_Jump is held,
 * fire IA_Dodge".
 */
USTRUCT(BlueprintType)
struct IVORY_BLADE_API FVRInputCombo
{
	GENERATED_BODY()

	/**
	 * The Input Action this combo triggers.
	 *
	 * Must have no key mappings in any mapping context. The whole point of the
	 * system is that a combo output owns no physical source, so nothing can
	 * collide with it and OpenXR has nothing to arbitrate away.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
	TObjectPtr<const UInputAction> OutputAction;

	/**
	 * The action whose value drives the edge. Its value is also what gets
	 * injected into OutputAction, so the two should share a value type.
	 *
	 * Prefer an action with no explicit triggers. FInputActionInstance::GetValue
	 * reports zero unless the trigger event is exactly Triggered, so an action
	 * that sits in Ongoing cannot be read as a value.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
	TObjectPtr<const UInputAction> EdgeAction;

	/** The gesture to look for. Flick, press, release, double tap, hold. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Combo")
	TObjectPtr<UVRComboEdge> Edge;

	/** All of these must pass on the frame the edge fires. An empty list always passes. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Combo")
	TArray<TObjectPtr<UVRComboCondition>> Conditions;

	/**
	 * When several rows match on the same EdgeAction in one frame, only the
	 * highest fires. Lets a longer combo outrank the shorter one it contains
	 * without either row referring to the other.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
	int32 Priority = 0;
};

/**
 * Every input combination in the game, in one openable asset.
 *
 * Lives beside the IMC and IA assets it refers to. Shared and read only at
 * runtime -- the combo component duplicates each row's Edge and Conditions
 * into its own instances, because those carry per-frame state and one asset
 * may drive several pawns.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Input Combo Set"))
class IVORY_BLADE_API UVRInputComboSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combos")
	TArray<FVRInputCombo> Combos;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
