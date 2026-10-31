#include "InputTriggerMultiChord.h"
#include "EnhancedPlayerInput.h"
#include "Engine/Engine.h"

namespace
{
	const TCHAR* TriggerEventName(ETriggerEvent Event)
	{
		switch (Event)
		{
		case ETriggerEvent::Triggered: return TEXT("Triggered");
		case ETriggerEvent::Started:   return TEXT("Started");
		case ETriggerEvent::Ongoing:   return TEXT("Ongoing");
		case ETriggerEvent::Canceled:  return TEXT("Canceled");
		case ETriggerEvent::Completed: return TEXT("Completed");
		case ETriggerEvent::None:      return TEXT("None");
		default:                       return TEXT("Unknown");
		}
	}

	// A thumbstick and a face button need different reads, so branch on the
	// shape of the key rather than assuming either one.
	bool ChordKeyActive(const UEnhancedPlayerInput* PlayerInput, const FKey& Key, float Threshold)
	{
		if (Key.IsAxis2D() || Key.IsAxis3D())
		{
			return PlayerInput->GetRawVectorKeyValue(Key).Size() >= Threshold;
		}

		if (Key.IsAxis1D())
		{
			return FMath::Abs(PlayerInput->GetKeyValue(Key)) >= Threshold;
		}

		return PlayerInput->IsPressed(Key);
	}
}

UInputTriggerMultiChord::UInputTriggerMultiChord()
{
	bShouldAlwaysTick = true;
}

ETriggerState UInputTriggerMultiChord::UpdateState_Implementation(
	const UEnhancedPlayerInput* PlayerInput,
	FInputActionValue ModifiedValue,
	float DeltaTime)
{
	const bool bDraw = bShowDebugOnScreen && GEngine != nullptr;

	// Ahead of every early return on purpose. A missing heartbeat means this
	// trigger is never ticked at all, which is a mapping context problem and
	// not something the gate below could account for.
	if (bDraw)
	{
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(DebugMessageKey), 0.0f, FColor::Cyan,
			FString::Printf(TEXT("MultiChord ALIVE (%d keys, %d actions)"),
				ChordKeys.Num(), ChordActions.Num()));
	}

	if (!PlayerInput || (ChordKeys.IsEmpty() && ChordActions.IsEmpty()))
	{
		if (bDraw)
		{
			GEngine->AddOnScreenDebugMessage(
				static_cast<uint64>(DebugMessageKey) + 1, 0.0f, FColor::Orange,
				PlayerInput ? TEXT("MultiChord NOTHING_CONFIGURED -> None")
				            : TEXT("MultiChord NO_PLAYER_INPUT -> None"));
		}

		return ETriggerState::None;
	}

	bool bGatePassed = true;

	// Raw key clause. Read from the key state map, which is filled before any
	// action mapping is evaluated, so consumption cannot starve it.
	FString KeyDetail;

	if (!ChordKeys.IsEmpty())
	{
		bool bAnyKeyHeld = false;
		bool bAllKeysHeld = true;

		for (int32 Index = 0; Index < ChordKeys.Num(); ++Index)
		{
			const FKey& Key = ChordKeys[Index];
			const bool bValid = Key.IsValid();
			const bool bHeld = bValid && ChordKeyActive(PlayerInput, Key, KeyAxisThreshold);

			bAnyKeyHeld = bAnyKeyHeld || bHeld;
			bAllKeysHeld = bAllKeysHeld && bHeld;

			if (bDraw)
			{
				KeyDetail += FString::Printf(TEXT("%s%s=%s"),
					Index > 0 ? TEXT(" ") : TEXT(""),
					bValid ? *Key.ToString() : TEXT("<blank>"),
					bValid ? (bHeld ? TEXT("DOWN") : TEXT("up")) : TEXT("INVALID_KEY"));
			}
		}

		if (!(bRequireAllChordKeys ? bAllKeysHeld : bAnyKeyHeld))
		{
			bGatePassed = false;
		}

		if (bDraw)
		{
			KeyDetail = FString::Printf(TEXT(" keys[%s]"), *KeyDetail);
		}
	}

	// Action clause. Platform dependent by nature, since a consumed action
	// reports None and cannot be told apart from an unpressed one.
	FString ActionDetail;

	for (const TObjectPtr<const UInputAction>& Action : ChordActions)
	{
		const TCHAR* StateName = TEXT("Unset");

		if (!Action)
		{
			StateName = TEXT("NULL_ACTION");
			bGatePassed = false;
		}
		else if (const FInputActionInstance* Instance = PlayerInput->FindActionInstanceData(Action))
		{
			const ETriggerEvent Event = Instance->GetTriggerEvent();
			StateName = TriggerEventName(Event);

			if (Event != ETriggerEvent::Ongoing && Event != ETriggerEvent::Triggered)
			{
				bGatePassed = false;
			}
		}
		else
		{
			// The asset exists but nothing in any applied mapping context feeds
			// it, so this action has no binding on the current platform.
			StateName = TEXT("NO_INSTANCE");
			bGatePassed = false;
		}

		if (bDraw)
		{
			ActionDetail += FString::Printf(TEXT(" %s=%s"),
				Action ? *Action->GetName() : TEXT("<null>"), StateName);
		}
	}

	if (bDraw)
	{
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(DebugMessageKey) + 1, 0.0f,
			bGatePassed ? FColor::Green : FColor::Orange,
			FString::Printf(TEXT("MultiChord val=[%s]%s%s -> %s"),
				*ModifiedValue.ToString(), *KeyDetail, *ActionDetail,
				bGatePassed ? TEXT("Triggered") : TEXT("None")));
	}

	return bGatePassed ? ETriggerState::Triggered : ETriggerState::None;
}
