#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VRJumpInputComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVRJump, Log, All);

/** Broadcast the instant a jump actually happens, and never on a suppressed one. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVRJumpPerformed);

/**
 * Owns the decision of whether an IA_Jump press becomes a jump, and when.
 *
 * Outside advance mode a press jumps immediately, exactly as it always has.
 * Inside advance mode the press is deferred and the release resolves it, so the
 * hold in between is free for a dodge to claim. If it does, the release jumps
 * nothing.
 *
 * The release deliberately consults a flag latched at press time rather than
 * re-reading advance mode. Advance mode is a toggle the player can hit
 * mid-hold, and re-reading it double-jumps in one direction and swallows the
 * jump in the other.
 *
 * This component never ticks. Every entry point is an input event, and the
 * Blueprint side supplies the advance-mode state rather than this class
 * reaching into the pawn to find it.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class IVORY_BLADE_API UVRJumpInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVRJumpInputComponent();

	virtual void BeginPlay() override;

	// ── Input entry points ──

	/**
	 * Call from IA_Jump's Started pin, never Triggered. Triggered fires every
	 * frame the button is held, which would re-defer continuously and clear the
	 * dodge's suppression a frame after it was set.
	 *
	 * @param advance_mode_active  Is_Advance_Moves_On, read from the pawn's
	 *                             Control Flag Switches map by the caller.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|Jump")
	void HandleJumpPressed(bool advance_mode_active);

	/**
	 * Call from IA_Jump's Completed pin. Jumps only if this press was deferred
	 * and no dodge was spent during the hold; otherwise does nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|Jump")
	void HandleJumpReleased();

	/**
	 * Call from the dodge handler when RequestOffsetDodgeFromStick returned true.
	 * Suppresses the jump that this hold's release would otherwise produce.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|Jump")
	void NotifyDodgeConsumed();

	/**
	 * Abandons any deferred jump without performing it. For possession changes,
	 * cutscenes, or anywhere the release event will not arrive.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|Jump")
	void ResetJumpState();

	// ── Output ──

	/**
	 * Bind Wall Run Jump Off, and anything else that used to chain off the Jump
	 * node, to this. It fires once per actual jump rather than every frame the
	 * button is held.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Input|Jump")
	FVRJumpPerformed OnJumpPerformed;

	// ── Queries ──

	/** True between a deferred press and its release. */
	UFUNCTION(BlueprintPure, Category = "Input|Jump")
	bool IsJumpDeferred() const { return bJumpDeferred; }

	/** True once a dodge has claimed the current hold. */
	UFUNCTION(BlueprintPure, Category = "Input|Jump")
	bool IsDodgeConsumed() const { return bDodgeConsumed; }

	// ── Diagnostics ──

	/** Logs one LogVRJump line per decision, naming the branch taken. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Jump|Diagnostics")
	bool bLogDiagnostics = true;

private:
	/** Calls Jump on the owning Character and broadcasts OnJumpPerformed. */
	void PerformJump(const TCHAR* reason);

	/** This press took the advance-mode branch, so its release owes a jump. */
	bool bJumpDeferred = false;

	/** A dodge fired during the current hold. */
	bool bDodgeConsumed = false;

	/** Keeps a non-Character owner from warning on every single press. */
	bool bWarnedNoCharacter = false;

	/** Keeps the unbound-delegate warning to one line per session. */
	bool bWarnedNoBinding = false;
};
