#include "VRInputComboComponent.h"
#include "VRComboCondition.h"
#include "VRComboEdge.h"
#include "VRInputComboSet.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "PlayerMappableInputConfig.h"
PRAGMA_ENABLE_DEPRECATION_WARNINGS

DEFINE_LOG_CATEGORY(LogVRInputCombo);

namespace VRInputComboDebug
{
	/** One slot per collision the validator finds, so none overwrite each other. */
	constexpr int32 ValidatorSlot = 100;

	/** Long enough to still be on screen after the headset is on and focused. */
	constexpr float StartupLifetime = 30.0f;
}

UVRInputComboComponent::UVRInputComboComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

// ── Lifetime ──

void UVRInputComboComponent::BeginPlay()
{
	Super::BeginPlay();

	BuildRuntimeCombos();
	TryEstablishTickPrerequisite();

	if (bValidateMappingsOnBeginPlay)
	{
		ValidateMappings();
	}

	UE_LOG(LogVRInputCombo, Log, TEXT("Combo component live on %s with %d combo(s)."),
		*GetNameSafe(GetOwner()), RuntimeCombos.Num());

	ReportDebug(
		FString::Printf(TEXT("Combo BEGINPLAY on %s, %d row(s) built from %s"),
			*GetNameSafe(GetOwner()), RuntimeCombos.Num(), *GetNameSafe(ComboSet)),
		VRInputComboDebug::ValidatorSlot - 1,
		VRInputComboDebug::StartupLifetime,
		FColor::Green);
}

void UVRInputComboComponent::BuildRuntimeCombos()
{
	RuntimeCombos.Reset();

	if (!ComboSet)
	{
		UE_LOG(LogVRInputCombo, Warning, TEXT("No Combo Set assigned on %s. No combos will fire."),
			*GetNameSafe(GetOwner()));

		return;
	}

	for (int32 index = 0; index < ComboSet->Combos.Num(); ++index)
	{
		const FVRInputCombo& combo = ComboSet->Combos[index];

		if (!combo.OutputAction || !combo.EdgeAction || !combo.Edge)
		{
			UE_LOG(LogVRInputCombo, Error,
				TEXT("Combo %d in %s is incomplete and will be skipped. Run asset validation."),
				index, *GetNameSafe(ComboSet));

			continue;
		}

		FVRComboRuntime runtime;
		runtime.ComboIndex = index;

		// Own copies, because the asset is shared between pawns and edges keep
		// per-frame state. Same reason the engine deep copies modifiers in
		// RebuildControlMappings.
		runtime.Edge = DuplicateObject<UVRComboEdge>(combo.Edge, this);
		runtime.Edge->ResetEdge();

		for (const TObjectPtr<UVRComboCondition>& condition : combo.Conditions)
		{
			if (!condition)
			{
				continue;
			}

			UVRComboCondition* copy = DuplicateObject<UVRComboCondition>(condition, this);
			copy->ResetCondition();
			runtime.Conditions.Add(copy);
		}

		RuntimeCombos.Add(MoveTemp(runtime));
	}
}

void UVRInputComboComponent::TryEstablishTickPrerequisite()
{
	if (bTickPrerequisiteSet)
	{
		return;
	}

	APlayerController* controller = ResolvePlayerController();

	if (!controller)
	{
		// Possession can land after BeginPlay. Retried from the tick.
		return;
	}

	AddTickPrerequisiteActor(controller);
	bTickPrerequisiteSet = true;
}

// ── Per frame ──

void UVRInputComboComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TryEstablishTickPrerequisite();

	const UEnhancedPlayerInput* player_input = GetPlayerInput();

	if (!ComboSet || RuntimeCombos.IsEmpty() || !player_input)
	{
		return;
	}

	// Winner per edge action, so a longer combo can outrank the shorter one it
	// contains without either row naming the other.
	TMap<const UInputAction*, int32> winner_by_edge_action;

	for (int32 runtime_index = 0; runtime_index < RuntimeCombos.Num(); ++runtime_index)
	{
		FVRComboRuntime& runtime = RuntimeCombos[runtime_index];
		const FVRInputCombo& combo = ComboSet->Combos[runtime.ComboIndex];

		const FInputActionValue edge_value = GetActionValue(combo.EdgeAction);

		// Everything is evaluated every frame, even when the combo cannot fire.
		// Edges and hold timers are state machines: skipping a frame would let
		// a stick that was already pushed register as a fresh flick the moment
		// combos come back on.
		bool conditions_pass = true;

		for (UVRComboCondition* condition : runtime.Conditions)
		{
			// Deliberately not short circuited, so every condition keeps ticking.
			conditions_pass &= condition->IsSatisfied(this, edge_value, DeltaTime);
		}

		const bool edge_fired = runtime.Edge->CheckEdge(edge_value, DeltaTime);

		if (!bCombosEnabled || !conditions_pass || !edge_fired)
		{
			continue;
		}

		int32* winner = winner_by_edge_action.Find(combo.EdgeAction);

		if (!winner)
		{
			winner_by_edge_action.Add(combo.EdgeAction, runtime_index);
		}
		else if (ComboSet->Combos[RuntimeCombos[*winner].ComboIndex].Priority < combo.Priority)
		{
			*winner = runtime_index;
		}
	}

	if (winner_by_edge_action.IsEmpty())
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* subsystem = GetSubsystem();

	if (!subsystem)
	{
		UE_LOG(LogVRInputCombo, Error, TEXT("A combo matched but there is no Enhanced Input subsystem to inject into."));

		return;
	}

	for (const TPair<const UInputAction*, int32>& entry : winner_by_edge_action)
	{
		const FVRComboRuntime& runtime = RuntimeCombos[entry.Value];
		const FVRInputCombo& combo = ComboSet->Combos[runtime.ComboIndex];
		const FInputActionValue edge_value = GetActionValue(combo.EdgeAction);

		// One injection gives Triggered this frame and Completed the next,
		// because ProcessInputStack releases anything in LastInjectedActions
		// that was not re-injected. That is exactly one fire per gesture.
		subsystem->InjectInputForAction(combo.OutputAction, edge_value, {}, {});

		UE_LOG(LogVRInputCombo, Verbose, TEXT("Injected %s with %s."),
			*combo.OutputAction->GetName(), *edge_value.ToString());
	}
}

// ── Queries ──

const FInputActionInstance* UVRInputComboComponent::FindActionInstance(const UInputAction* action) const
{
	const UEnhancedPlayerInput* player_input = GetPlayerInput();

	return (player_input && action) ? player_input->FindActionInstanceData(action) : nullptr;
}

bool UVRInputComboComponent::IsActionHeld(const UInputAction* action, float threshold) const
{
	const FInputActionInstance* instance = FindActionInstance(action);

	if (!instance)
	{
		return false;
	}

	switch (instance->GetTriggerEvent())
	{
	case ETriggerEvent::Started:
	case ETriggerEvent::Ongoing:
		// Mid evaluation. The value reads as zero here by design, so the event
		// is the only honest answer -- a Hold trigger sits in Ongoing for the
		// entire hold and would otherwise look released.
		return true;

	case ETriggerEvent::Triggered:
		return instance->GetValue().GetMagnitude() >= threshold;

	default:
		return false;
	}
}

FInputActionValue UVRInputComboComponent::GetActionValue(const UInputAction* action) const
{
	const FInputActionInstance* instance = FindActionInstance(action);

	return instance ? instance->GetValue() : FInputActionValue();
}

ETriggerEvent UVRInputComboComponent::GetActionTriggerEvent(const UInputAction* action) const
{
	const FInputActionInstance* instance = FindActionInstance(action);

	return instance ? instance->GetTriggerEvent() : ETriggerEvent::None;
}

APlayerController* UVRInputComboComponent::ResolvePlayerController() const
{
	AActor* owner = GetOwner();

	if (!owner)
	{
		return nullptr;
	}

	if (APlayerController* as_controller = Cast<APlayerController>(owner))
	{
		return as_controller;
	}

	if (const APawn* as_pawn = Cast<APawn>(owner))
	{
		return Cast<APlayerController>(as_pawn->GetController());
	}

	return Cast<APlayerController>(owner->GetInstigatorController());
}

UEnhancedPlayerInput* UVRInputComboComponent::GetPlayerInput() const
{
	const APlayerController* controller = ResolvePlayerController();

	return controller ? Cast<UEnhancedPlayerInput>(controller->PlayerInput) : nullptr;
}

UEnhancedInputLocalPlayerSubsystem* UVRInputComboComponent::GetSubsystem() const
{
	const APlayerController* controller = ResolvePlayerController();
	const ULocalPlayer* local_player = controller ? controller->GetLocalPlayer() : nullptr;

	return local_player ? local_player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
}

// ── Validation ──

void UVRInputComboComponent::ValidateMappings() const
{
	if (ValidationConfig.IsNull())
	{
		UE_LOG(LogVRInputCombo, Warning, TEXT("Validation is on but no Validation Config is set."));

		ReportDebug(TEXT("Combo validator SKIPPED: no Validation Config set."),
			VRInputComboDebug::ValidatorSlot, VRInputComboDebug::StartupLifetime, FColor::Orange);

		return;
	}

	UObject* loaded = ValidationConfig.TryLoad();

	// UPlayerMappableInputConfig is deprecated in 5.3, but it is still the only
	// readable description of the contexts OpenXR turns into action sets, which
	// is precisely what needs auditing.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const UPlayerMappableInputConfig* config = Cast<UPlayerMappableInputConfig>(loaded);

	if (!config)
	{
		UE_LOG(LogVRInputCombo, Warning, TEXT("Could not load %s for mapping validation."),
			*ValidationConfig.ToString());

		ReportDebug(
			FString::Printf(TEXT("Combo validator FAILED: could not load %s"), *ValidationConfig.ToString()),
			VRInputComboDebug::ValidatorSlot, VRInputComboDebug::StartupLifetime, FColor::Red);

		return;
	}

	const TMap<TObjectPtr<UInputMappingContext>, int32>& contexts = config->GetMappingContexts();
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Every input source, and who claims it.
	TMap<FKey, TMap<const UInputAction*, TArray<FString>>> owners_by_key;

	for (const TPair<TObjectPtr<UInputMappingContext>, int32>& context_entry : contexts)
	{
		const UInputMappingContext* context = context_entry.Key;

		if (!context)
		{
			continue;
		}

		for (const FEnhancedActionKeyMapping& mapping : context->GetMappings())
		{
			if (!mapping.Action || !mapping.Key.IsValid())
			{
				continue;
			}

			owners_by_key.FindOrAdd(mapping.Key).FindOrAdd(mapping.Action).AddUnique(context->GetName());
		}
	}

	int32 collision_count = 0;

	for (const TPair<FKey, TMap<const UInputAction*, TArray<FString>>>& key_entry : owners_by_key)
	{
		if (key_entry.Value.Num() < 2)
		{
			continue;
		}

		++collision_count;

		FString claimants;

		for (const TPair<const UInputAction*, TArray<FString>>& owner : key_entry.Value)
		{
			claimants += FString::Printf(TEXT("%s%s (%s)"),
				claimants.IsEmpty() ? TEXT("") : TEXT(", "),
				*owner.Key->GetName(),
				*FString::Join(owner.Value, TEXT("+")));
		}

		const FString message = FString::Printf(TEXT("Input collision: %s claimed by %s"),
			*key_entry.Key.ToString(), *claimants);

		// An error rather than a warning: on Quest this is not a style problem,
		// it is the one that silently zeroes whichever action loses the
		// xrSyncActions priority contest.
		UE_LOG(LogVRInputCombo, Error, TEXT("%s"), *message);

		ReportDebug(message, VRInputComboDebug::ValidatorSlot + collision_count,
			VRInputComboDebug::StartupLifetime, FColor::Orange);
	}

	const FString summary = FString::Printf(TEXT("Combo validator: %d source(s) owned by more than one action."),
		collision_count);

	UE_LOG(LogVRInputCombo, Log, TEXT("%s"), *summary);

	ReportDebug(summary, VRInputComboDebug::ValidatorSlot, VRInputComboDebug::StartupLifetime,
		collision_count > 0 ? FColor::Orange : FColor::Green);
}

void UVRInputComboComponent::ReportDebug(const FString& message, int32 slot, float lifetime, FColor color) const
{
	if (!bShowDebugOnScreen || !GEngine)
	{
		return;
	}

	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(DebugMessageKey + slot), lifetime, color, message);
}
