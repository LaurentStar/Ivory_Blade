#include "VRComboCondition.h"
#include "VRInputComboComponent.h"
#include "InputAction.h"

// ── Base ──

bool UVRComboCondition::IsSatisfied_Implementation(const UVRInputComboComponent* component, FInputActionValue edge_value, float delta_time)
{
	return true;
}

void UVRComboCondition::ResetCondition_Implementation()
{
}

FString UVRComboCondition::DescribeForDebug() const
{
	return GetClass()->GetName();
}

// ── Action held ──

bool UVRComboCondition_ActionHeld::IsSatisfied_Implementation(const UVRInputComboComponent* component, FInputActionValue edge_value, float delta_time)
{
	const bool held = component && component->IsActionHeld(Action, Threshold);

	if (!held)
	{
		HeldTime = 0.0f;
		return false;
	}

	HeldTime += delta_time;

	return HeldTime >= MinHoldSeconds;
}

void UVRComboCondition_ActionHeld::ResetCondition_Implementation()
{
	HeldTime = 0.0f;
}

FString UVRComboCondition_ActionHeld::DescribeForDebug() const
{
	return FString::Printf(TEXT("held(%s)"), Action ? *Action->GetName() : TEXT("<none>"));
}

// ── Action absent ──

bool UVRComboCondition_ActionAbsent::IsSatisfied_Implementation(const UVRInputComboComponent* component, FInputActionValue edge_value, float delta_time)
{
	return component && !component->IsActionHeld(Action, Threshold);
}

FString UVRComboCondition_ActionAbsent::DescribeForDebug() const
{
	return FString::Printf(TEXT("absent(%s)"), Action ? *Action->GetName() : TEXT("<none>"));
}

// ── Direction ──

bool UVRComboCondition_Direction::IsSatisfied_Implementation(const UVRInputComboComponent* component, FInputActionValue edge_value, float delta_time)
{
	const FVector raw = edge_value.Get<FVector>();
	const FVector2D flat(raw.X, raw.Y);

	// A centred stick has no direction to compare against, so nothing matches.
	if (flat.IsNearlyZero())
	{
		return false;
	}

	const float actual = FMath::RadiansToDegrees(FMath::Atan2(flat.Y, flat.X));
	const float delta = FMath::Abs(FMath::FindDeltaAngleDegrees(AngleDegrees, actual));

	return delta <= ToleranceDegrees;
}

FString UVRComboCondition_Direction::DescribeForDebug() const
{
	return FString::Printf(TEXT("dir(%.0f+-%.0f)"), AngleDegrees, ToleranceDegrees);
}
