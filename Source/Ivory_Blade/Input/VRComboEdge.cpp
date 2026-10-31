#include "VRComboEdge.h"

// ── Base ──

bool UVRComboEdge::CheckEdge_Implementation(FInputActionValue value, float delta_time)
{
	return false;
}

void UVRComboEdge::ResetEdge_Implementation()
{
}

FString UVRComboEdge::DescribeForDebug() const
{
	return GetClass()->GetName();
}

// ── Threshold latch ──

UVRComboEdge_Threshold::EActuation UVRComboEdge_Threshold::UpdateActuation(const FInputActionValue& value)
{
	// Type aware on purpose. GetMagnitude uses Size2D for an Axis2D action, so a
	// thumbstick is not inflated by a stale Z component.
	const float magnitude = value.GetMagnitude();

	if (bActuated)
	{
		if (magnitude <= ReleaseThreshold)
		{
			bActuated = false;
			return EActuation::Fell;
		}

		return EActuation::None;
	}

	if (magnitude >= ActuateThreshold)
	{
		bActuated = true;
		return EActuation::Rose;
	}

	return EActuation::None;
}

void UVRComboEdge_Threshold::ResetEdge_Implementation()
{
	bActuated = false;
}

FString UVRComboEdge_Threshold::DescribeForDebug() const
{
	return FString::Printf(TEXT("%s%s"),
		*GetClass()->GetName(),
		bActuated ? TEXT("[on]") : TEXT("[off]"));
}

// ── Flick ──

bool UVRComboEdge_Flick::CheckEdge_Implementation(FInputActionValue value, float delta_time)
{
	return UpdateActuation(value) == EActuation::Rose;
}

// ── Released ──

bool UVRComboEdge_Released::CheckEdge_Implementation(FInputActionValue value, float delta_time)
{
	return UpdateActuation(value) == EActuation::Fell;
}

// ── Double tap ──

bool UVRComboEdge_DoubleTap::CheckEdge_Implementation(FInputActionValue value, float delta_time)
{
	if (WindowRemaining > 0.0f)
	{
		WindowRemaining = FMath::Max(0.0f, WindowRemaining - delta_time);
	}

	if (UpdateActuation(value) != EActuation::Rose)
	{
		return false;
	}

	// A tap was already pending and this one landed in time.
	if (WindowRemaining > 0.0f)
	{
		WindowRemaining = 0.0f;
		return true;
	}

	WindowRemaining = WindowSeconds;
	return false;
}

void UVRComboEdge_DoubleTap::ResetEdge_Implementation()
{
	Super::ResetEdge_Implementation();
	WindowRemaining = 0.0f;
}

FString UVRComboEdge_DoubleTap::DescribeForDebug() const
{
	return FString::Printf(TEXT("DoubleTap[%s window=%.2f]"),
		IsActuated() ? TEXT("on") : TEXT("off"), WindowRemaining);
}

// ── Held for ──

bool UVRComboEdge_HeldFor::CheckEdge_Implementation(FInputActionValue value, float delta_time)
{
	UpdateActuation(value);

	if (!IsActuated())
	{
		HeldTime = 0.0f;
		bFiredThisHold = false;
		return false;
	}

	HeldTime += delta_time;

	if (bFiredThisHold || HeldTime < DurationSeconds)
	{
		return false;
	}

	// Latched for the rest of the hold. Without this the gesture would complete
	// again on every frame past the duration.
	bFiredThisHold = true;
	return true;
}

void UVRComboEdge_HeldFor::ResetEdge_Implementation()
{
	Super::ResetEdge_Implementation();
	HeldTime = 0.0f;
	bFiredThisHold = false;
}

FString UVRComboEdge_HeldFor::DescribeForDebug() const
{
	return FString::Printf(TEXT("HeldFor[%.2f/%.2f]"), HeldTime, DurationSeconds);
}
