#pragma once

#include "CoreMinimal.h"
#include "InputTriggers.h"
#include "InputAction.h"
#include "InputTriggerMultiChord.generated.h"

/**
 * Fires only when every action in the ChordActions array is currently active
 * (Ongoing or Triggered). Drop-in replacement for stacking multiple
 * UInputTriggerChordAction triggers, which is bugged in UE 5.3.
 */
UCLASS(meta = (DisplayName = "Multi Chord Action"))
class IVORY_BLADE_API UInputTriggerMultiChord : public UInputTrigger
{
	GENERATED_BODY()

public:
	UInputTriggerMultiChord();

	/** Every action in this array must be active for the trigger to fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Settings")
	TArray<TObjectPtr<const UInputAction>> ChordActions;

protected:
	virtual ETriggerType GetTriggerType_Implementation() const override
	{
		return ETriggerType::Implicit;
	}

	virtual ETriggerState UpdateState_Implementation(
		const UEnhancedPlayerInput* PlayerInput,
		FInputActionValue ModifiedValue,
		float DeltaTime) override;
};
