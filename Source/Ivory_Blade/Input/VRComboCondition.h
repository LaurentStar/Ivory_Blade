#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "VRComboCondition.generated.h"

class UInputAction;
class UVRInputComboComponent;

/**
 * A precondition that must hold on the frame a combo's edge fires.
 *
 * Subclass to add a new kind. Every subclass appears automatically in the
 * Conditions list on a combo row, the same way a new UInputModifier appears on
 * an Input Action.
 *
 * Takes the component rather than just the player input so a Blueprint
 * subclass can reach the owning pawn and gate on gameplay state as well as on
 * input.
 *
 * Called every frame regardless of whether the edge fired, so a condition may
 * carry state. That is also why the combo component duplicates conditions out
 * of the shared combo asset at BeginPlay.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, CollapseCategories)
class IVORY_BLADE_API UVRComboCondition : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * @param component   The combo component, for reading other actions and the owning pawn.
	 * @param edge_value  Current value of the row's edge action, so direction-style
	 *                    conditions do not have to look it up again.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Combo")
	bool IsSatisfied(const UVRInputComboComponent* component, FInputActionValue edge_value, float delta_time);
	virtual bool IsSatisfied_Implementation(const UVRInputComboComponent* component, FInputActionValue edge_value, float delta_time);

	/** Drops any accumulated state. Called on BeginPlay and whenever combos are re-enabled. */
	UFUNCTION(BlueprintNativeEvent, Category = "Combo")
	void ResetCondition();
	virtual void ResetCondition_Implementation();

	/** One short token for the on-screen diagnostics. */
	virtual FString DescribeForDebug() const;
};

/**
 * Satisfied while another Input Action is being held.
 *
 * Deliberately consults the trigger event and not just the value.
 * FInputActionInstance::GetValue returns zero unless the event is exactly
 * Triggered, so an action parked in Ongoing -- which is what a Hold trigger
 * does for the entire hold -- would otherwise read as released.
 */
UCLASS(meta = (DisplayName = "Action Held"))
class IVORY_BLADE_API UVRComboCondition_ActionHeld : public UVRComboCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
	TObjectPtr<const UInputAction> Action;

	/**
	 * Magnitude an analog source must reach to count as held. Only applied when
	 * the action's value is actually readable; see the class comment.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Threshold = 0.2f;

	/** Require the action to have been held this long before the condition passes. Zero disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo",
	          meta = (ClampMin = "0.0"))
	float MinHoldSeconds = 0.0f;

	virtual bool IsSatisfied_Implementation(const UVRInputComboComponent* component, FInputActionValue edge_value, float delta_time) override;
	virtual void ResetCondition_Implementation() override;
	virtual FString DescribeForDebug() const override;

private:
	float HeldTime = 0.0f;
};

/**
 * Satisfied while another Input Action is *not* held. Lets two rows on the
 * same edge be told apart explicitly rather than by priority alone.
 */
UCLASS(meta = (DisplayName = "Action Absent"))
class IVORY_BLADE_API UVRComboCondition_ActionAbsent : public UVRComboCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo")
	TObjectPtr<const UInputAction> Action;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Threshold = 0.2f;

	virtual bool IsSatisfied_Implementation(const UVRInputComboComponent* component, FInputActionValue edge_value, float delta_time) override;
	virtual FString DescribeForDebug() const override;
};

/**
 * Satisfied when the edge action's direction lies within Tolerance of Angle.
 *
 * Angle is measured in the action's own 2D space, counter-clockwise from +X,
 * so 0 is right, 90 is forward, 180 is left and 270 is back. Use it to give a
 * flick left and a flick right two different output actions.
 */
UCLASS(meta = (DisplayName = "Direction"))
class IVORY_BLADE_API UVRComboCondition_Direction : public UVRComboCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo",
	          meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float AngleDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo",
	          meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ToleranceDegrees = 45.0f;

	virtual bool IsSatisfied_Implementation(const UVRInputComboComponent* component, FInputActionValue edge_value, float delta_time) override;
	virtual FString DescribeForDebug() const override;
};
