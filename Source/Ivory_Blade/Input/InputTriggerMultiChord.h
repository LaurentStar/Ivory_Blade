#pragma once

#include "CoreMinimal.h"
#include "InputTriggers.h"
#include "InputAction.h"
#include "InputTriggerMultiChord.generated.h"

/**
 * Gates an action behind other inputs being held. Drop-in replacement for
 * stacking multiple UInputTriggerChordAction triggers, which is bugged in
 * UE 5.3.
 *
 * Two independent ways to express the chord, and a populated array of either
 * kind must pass for the trigger to fire:
 *
 * - ChordKeys reads raw device state. Prefer this. UPlayerInput fills its key
 *   state map before action mappings are evaluated, so input consumption, IMC
 *   priority, and mapping rebuilds cannot starve it, and it therefore behaves
 *   the same on desktop and on device.
 * - ChordActions reads other actions' trigger state. Kept for existing setups,
 *   but an action whose input has been consumed by a higher-priority mapping
 *   reports None and is indistinguishable from an unpressed one, which makes
 *   this path platform dependent.
 */
UCLASS(meta = (DisplayName = "Multi Chord Action"))
class IVORY_BLADE_API UInputTriggerMultiChord : public UInputTrigger
{
	GENERATED_BODY()

public:
	UInputTriggerMultiChord();

	/**
	 * Raw keys that must be held, read straight from the key state map and so
	 * unaffected by input consumption or mapping context priority.
	 *
	 * List the same logical input once per platform here, such as Space Bar
	 * alongside Oculus Touch (R) A, and leave bRequireAllChordKeys off so
	 * either satisfies it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Settings")
	TArray<FKey> ChordKeys;

	/**
	 * Off means any one key in ChordKeys satisfies the clause, which is what a
	 * cross-platform list of the same button needs since only one of them can
	 * ever be physically pressed. Tick it for a genuine multi-key chord where
	 * every listed key must be held at once.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Settings")
	bool bRequireAllChordKeys = false;

	/** Magnitude above which an axis key in ChordKeys counts as held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Settings",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float KeyAxisThreshold = 0.5f;

	/** Every action in this array must be active for the trigger to fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger Settings")
	TArray<TObjectPtr<const UInputAction>> ChordActions;

	/**
	 * Draws the gate decision to the screen each frame. On-screen rather than
	 * UE_LOG because device testing runs untethered from a packaged APK, where
	 * logcat needs a cable and Shipping strips Log and Warning verbosity.
	 */
	UPROPERTY(EditAnywhere, Category = "Trigger Settings")
	bool bShowDebugOnScreen = false;

	/**
	 * Screen line to write to. The heartbeat takes this key and the detail line
	 * takes the next one up, so keep separate trigger instances at least two
	 * apart or they will overwrite each other.
	 */
	UPROPERTY(EditAnywhere, Category = "Trigger Settings",
	          meta = (EditCondition = "bShowDebugOnScreen"))
	int32 DebugMessageKey = 200;

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
