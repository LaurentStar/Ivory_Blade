#include "InputTriggerMultiChord.h"
#include "EnhancedPlayerInput.h"

UInputTriggerMultiChord::UInputTriggerMultiChord()
{
	bShouldAlwaysTick = true;
}

ETriggerState UInputTriggerMultiChord::UpdateState_Implementation(
	const UEnhancedPlayerInput* PlayerInput,
	FInputActionValue ModifiedValue,
	float DeltaTime)
{
	if (!PlayerInput || ChordActions.IsEmpty())
	{
		return ETriggerState::None;
	}

	for (const TObjectPtr<const UInputAction>& Action : ChordActions)
	{
		if (!Action)
		{
			return ETriggerState::None;
		}

		const FInputActionInstance* Instance = PlayerInput->FindActionInstanceData(Action);
		if (!Instance)
		{
			return ETriggerState::None;
		}

		const ETriggerEvent Event = Instance->GetTriggerEvent();
		const bool bIsActive = (Event == ETriggerEvent::Ongoing || Event == ETriggerEvent::Triggered);
		if (!bIsActive)
		{
			return ETriggerState::None;
		}
	}

	return ETriggerState::Triggered;
}
