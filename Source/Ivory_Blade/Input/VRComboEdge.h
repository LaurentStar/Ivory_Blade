#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "VRComboEdge.generated.h"

/**
 * Decides the moment a combo fires, from the running value of a single Input
 * Action.
 *
 * Subclass to add a new shape of gesture. Every subclass appears automatically
 * in the Edge dropdown on a combo row, the same way a new UInputTrigger appears
 * on an Input Action.
 *
 * Instances are stateful, so the combo component duplicates them out of the
 * shared combo asset at BeginPlay rather than driving the asset's copies.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, CollapseCategories)
class IVORY_BLADE_API UVRComboEdge : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Called every frame with the edge action's current value, whether or not
	 * the row's conditions pass. Return true only on the frame the gesture
	 * completes.
	 *
	 * Ticking unconditionally is deliberate. A latch that only advanced while
	 * the conditions held would arm itself partway through a hold and then fire
	 * on a stick the player never actually flicked.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Combo")
	bool CheckEdge(FInputActionValue value, float delta_time);
	virtual bool CheckEdge_Implementation(FInputActionValue value, float delta_time);

	/** Drops any latched progress. Called on BeginPlay and whenever combos are re-enabled. */
	UFUNCTION(BlueprintNativeEvent, Category = "Combo")
	void ResetEdge();
	virtual void ResetEdge_Implementation();

	/** One short line for the on-screen diagnostics. */
	virtual FString DescribeForDebug() const;
};

/**
 * Shared machinery for every edge defined by a magnitude crossing a threshold.
 *
 * Holds the actuation latch and its hysteresis, which is what stops one
 * physical gesture from chattering into several as the value jitters around the
 * threshold.
 */
UCLASS(Abstract)
class IVORY_BLADE_API UVRComboEdge_Threshold : public UVRComboEdge
{
	GENERATED_BODY()

public:
	/** Magnitude at or above which the input counts as actuated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ActuateThreshold = 0.5f;

	/**
	 * The value must fall to or below this before the gesture can fire again.
	 * Keep it under ActuateThreshold -- equal values give no hysteresis at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReleaseThreshold = 0.3f;

	virtual void ResetEdge_Implementation() override;
	virtual FString DescribeForDebug() const override;

protected:
	/** What the latch did this frame. */
	enum class EActuation : uint8
	{
		None,
		Rose,
		Fell
	};

	/** Feeds the latch and reports the transition, if any. Call exactly once per frame. */
	EActuation UpdateActuation(const FInputActionValue& value);

	bool IsActuated() const { return bActuated; }

private:
	bool bActuated = false;
};

/**
 * Fires on the frame the value crosses up through ActuateThreshold. The flick
 * of a thumbstick, and the default for analog sources.
 */
UCLASS(meta = (DisplayName = "Flick (rising edge)"))
class IVORY_BLADE_API UVRComboEdge_Flick : public UVRComboEdge_Threshold
{
	GENERATED_BODY()

public:
	virtual bool CheckEdge_Implementation(FInputActionValue value, float delta_time) override;
};

/**
 * The same rising-edge test as Flick, under a name that reads correctly for a
 * button. Kept separate so a combo row built around a face button says what it
 * means in the dropdown.
 */
UCLASS(meta = (DisplayName = "Pressed"))
class IVORY_BLADE_API UVRComboEdge_Pressed : public UVRComboEdge_Flick
{
	GENERATED_BODY()
};

/** Fires on the frame the value falls back through ReleaseThreshold. */
UCLASS(meta = (DisplayName = "Released"))
class IVORY_BLADE_API UVRComboEdge_Released : public UVRComboEdge_Threshold
{
	GENERATED_BODY()

public:
	virtual bool CheckEdge_Implementation(FInputActionValue value, float delta_time) override;
};

/** Fires on the second rising edge, when it lands within WindowSeconds of the first. */
UCLASS(meta = (DisplayName = "Double Tap"))
class IVORY_BLADE_API UVRComboEdge_DoubleTap : public UVRComboEdge_Threshold
{
	GENERATED_BODY()

public:
	/** How long the second tap has to arrive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo",
	          meta = (ClampMin = "0.0"))
	float WindowSeconds = 0.3f;

	virtual bool CheckEdge_Implementation(FInputActionValue value, float delta_time) override;
	virtual void ResetEdge_Implementation() override;
	virtual FString DescribeForDebug() const override;

private:
	/** Counts down from WindowSeconds after a first tap. Zero means no tap is pending. */
	float WindowRemaining = 0.0f;
};

/**
 * Fires once after the value has stayed actuated for DurationSeconds. For
 * charge-ups: the gesture completes while the player is still holding, not on
 * release.
 */
UCLASS(meta = (DisplayName = "Held For"))
class IVORY_BLADE_API UVRComboEdge_HeldFor : public UVRComboEdge_Threshold
{
	GENERATED_BODY()

public:
	/** How long the input must stay actuated before the gesture completes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo",
	          meta = (ClampMin = "0.0"))
	float DurationSeconds = 0.4f;

	virtual bool CheckEdge_Implementation(FInputActionValue value, float delta_time) override;
	virtual void ResetEdge_Implementation() override;
	virtual FString DescribeForDebug() const override;

private:
	float HeldTime = 0.0f;

	/** Stops a continuing hold from re-firing every frame past the duration. */
	bool bFiredThisHold = false;
};
