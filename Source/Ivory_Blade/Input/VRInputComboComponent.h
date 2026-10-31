#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "InputTriggers.h"
#include "VRInputComboComponent.generated.h"

class APlayerController;
class UEnhancedInputLocalPlayerSubsystem;
class UEnhancedPlayerInput;
class UInputAction;
class UVRComboCondition;
class UVRComboEdge;
class UVRInputComboSet;
struct FInputActionInstance;

DECLARE_LOG_CATEGORY_EXTERN(LogVRInputCombo, Log, All);

/**
 * One row of the combo set, with its own copies of the stateful parts.
 *
 * The asset is shared between every pawn that references it, but edges and
 * some conditions carry per-frame state, so the component duplicates them the
 * same way the engine deep copies modifiers in RebuildControlMappings.
 */
USTRUCT()
struct FVRComboRuntime
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ComboIndex = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<UVRComboEdge> Edge;

	UPROPERTY()
	TArray<TObjectPtr<UVRComboCondition>> Conditions;
};

/**
 * Turns combinations of Input Actions into a single Input Action.
 *
 * Exists because an input source can only belong to one Input Action on Quest.
 * The OpenXR enhanced path builds one action set per mapping context, and when
 * two sets bind the same source xrSyncActions picks a winner and reports the
 * loser inactive, which arrives as a zero value. So no Input Action is allowed
 * to encode "locomotion AND jump" by binding the thumbstick a second time.
 *
 * Instead the combination is resolved here, from action values, and the result
 * is pushed into an output Input Action that has no key mappings of its own
 * via InjectInputForAction -- the same call the OpenXR plugin uses to deliver
 * every input on device. Gameplay still binds the output with an ordinary
 * EnhancedInputAction node and never learns it came from a combo.
 */
UCLASS(ClassGroup = (Input), meta = (BlueprintSpawnableComponent))
class IVORY_BLADE_API UVRInputComboComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVRInputComboComponent();

	// ── Configuration ──

	/** Every combination this pawn can perform. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Combos")
	TObjectPtr<UVRInputComboSet> ComboSet;

	/**
	 * Master gate. When false the edges still tick, so their latches stay
	 * honest and a stick already pushed when combos come back on does not
	 * count as a fresh flick, but nothing is injected.
	 *
	 * This replaces applying and removing a mapping context to gate a combo.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Combos")
	bool bCombosEnabled = true;

	// ── Validation ──

	/** Report input sources owned by more than one Input Action, once, on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Validation")
	bool bValidateMappingsOnBeginPlay = true;

	/**
	 * The mappable input config to audit, normally PMI_VRTemplate.
	 *
	 * Held as a path rather than a typed pointer because
	 * UPlayerMappableInputConfig is deprecated in 5.3 and naming the type in a
	 * UPROPERTY would push the deprecation into generated code. It is still
	 * the only readable description of what OpenXR builds its action sets
	 * from, which is exactly what needs auditing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Validation",
		meta = (AllowedClasses = "/Script/EnhancedInput.PlayerMappableInputConfig"))
	FSoftObjectPath ValidationConfig = FSoftObjectPath(TEXT("/Game/VRTemplate/Input/PMI_VRTemplate.PMI_VRTemplate"));

	// ── Debug ──

	/**
	 * Report the startup mapping-collision audit to the screen.
	 *
	 * On-screen rather than UE_LOG on purpose: device testing is an untethered
	 * APK, so there is no log to read. Only the validator draws now; the
	 * per-frame combo trace was removed once the combo system was working,
	 * because twelve lines redrawn at 72 Hz are unreadable in a headset and
	 * crowd out everything else.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Debug")
	bool bShowDebugOnScreen = false;

	/** Base key for the debug messages, so each line owns its own slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Debug")
	int32 DebugMessageKey = 7100;

	// ── Queries, for conditions and for gameplay ──

	/**
	 * Whether an action is currently being pressed or held.
	 *
	 * Deliberately reads the trigger event and not the value.
	 * FInputActionInstance::GetValue returns zero unless the event is exactly
	 * Triggered, so an action parked in Ongoing -- which is what a Hold
	 * trigger does for the whole hold -- would otherwise read as released.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|Combos")
	bool IsActionHeld(const UInputAction* action, float threshold = 0.2f) const;

	/** Current value of an action. Zero unless the action is in Triggered this frame. */
	UFUNCTION(BlueprintPure, Category = "Input|Combos")
	FInputActionValue GetActionValue(const UInputAction* action) const;

	/** Current trigger event of an action, or None if it has no instance data. */
	UFUNCTION(BlueprintPure, Category = "Input|Combos")
	ETriggerEvent GetActionTriggerEvent(const UInputAction* action) const;

	const FInputActionInstance* FindActionInstance(const UInputAction* action) const;

	UEnhancedPlayerInput* GetPlayerInput() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Owner may be the pawn or the controller itself. */
	APlayerController* ResolvePlayerController() const;

	UEnhancedInputLocalPlayerSubsystem* GetSubsystem() const;

	void BuildRuntimeCombos();

	/**
	 * Tick after the controller so values are from this frame.
	 *
	 * Component and controller tick order is otherwise unspecified. Nothing is
	 * dropped without this -- injections survive until the next
	 * ProcessInputStack -- but the reaction can lag a frame, unpredictably.
	 */
	void TryEstablishTickPrerequisite();

	void ValidateMappings() const;

	/** One on-screen line, owning its slot. */
	void ReportDebug(const FString& message, int32 slot, float lifetime, FColor color) const;

	UPROPERTY(Transient)
	TArray<FVRComboRuntime> RuntimeCombos;

	bool bTickPrerequisiteSet = false;
};
