#include "VRJumpInputComponent.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY(LogVRJump);

UVRJumpInputComponent::UVRJumpInputComponent()
{
	// Nothing to do per frame. Every entry point is an input event.
	PrimaryComponentTick.bCanEverTick = false;
}

void UVRJumpInputComponent::BeginPlay()
{
	Super::BeginPlay();

	ResetJumpState();

	UE_LOG(LogVRJump, Warning, TEXT("VRJumpInputComponent live on %s. Diagnostics=%s"),
		*GetNameSafe(GetOwner()), bLogDiagnostics ? TEXT("on") : TEXT("OFF"));

	// The OnJumpPerformed binding check cannot live here. A component's BeginPlay
	// runs before the owning actor's Event BeginPlay, so the pawn has not had the
	// chance to bind yet and this would always report unbound. It happens on the
	// first jump instead, which is the first moment the answer matters.

	if (!Cast<ACharacter>(GetOwner()))
	{
		bWarnedNoCharacter = true;

		UE_LOG(LogVRJump, Error,
			TEXT("Owner %s is not a Character. Jump input will be accepted and discarded."),
			*GetNameSafe(GetOwner()));
	}
}

// ── Input entry points ──

void UVRJumpInputComponent::HandleJumpPressed(bool advance_mode_active)
{
	if (bLogDiagnostics)
	{
		UE_LOG(LogVRJump, Warning,
			TEXT("PRESSED. advance_mode_active=%s  (entering state deferred=%s consumed=%s)"),
			advance_mode_active ? TEXT("TRUE") : TEXT("FALSE"),
			bJumpDeferred       ? TEXT("TRUE") : TEXT("FALSE"),
			bDodgeConsumed      ? TEXT("TRUE") : TEXT("FALSE"));
	}

	if (!advance_mode_active)
	{
		// Clearing on the press as well as the release is what self-heals a lost
		// Completed event. A press outside advance mode always starts clean.
		bJumpDeferred  = false;
		bDodgeConsumed = false;

		PerformJump(TEXT("press, outside advance mode"));
		return;
	}

	bJumpDeferred  = true;
	bDodgeConsumed = false;

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRJump, Log, TEXT("Press deferred to release (advance mode)."));
	}
}

void UVRJumpInputComponent::HandleJumpReleased()
{
	if (bLogDiagnostics)
	{
		UE_LOG(LogVRJump, Warning, TEXT("RELEASED. deferred=%s consumed=%s"),
			bJumpDeferred  ? TEXT("TRUE") : TEXT("FALSE"),
			bDodgeConsumed ? TEXT("TRUE") : TEXT("FALSE"));
	}

	if (!bJumpDeferred)
	{
		// The press already jumped. This is the ordinary outside-advance-mode
		// path, and it is why the release must not re-read advance mode: a
		// player who toggled advance on during the hold would jump twice.
		if (bLogDiagnostics)
		{
			UE_LOG(LogVRJump, Warning, TEXT("Release ignored, press was not deferred."));
		}

		return;
	}

	bJumpDeferred = false;

	if (bDodgeConsumed)
	{
		bDodgeConsumed = false;

		if (bLogDiagnostics)
		{
			UE_LOG(LogVRJump, Log, TEXT("Jump suppressed, a dodge claimed the hold."));
		}

		return;
	}

	PerformJump(TEXT("release, advance mode"));
}

void UVRJumpInputComponent::NotifyDodgeConsumed()
{
	if (!bJumpDeferred)
	{
		// A dodge with no jump hold behind it -- the vr.Dodge console commands,
		// or some future input path. There is nothing to suppress, and latching
		// the flag anyway would suppress the *next* hold's jump instead.
		if (bLogDiagnostics)
		{
			UE_LOG(LogVRJump, Warning,
				TEXT("Dodge reported with no jump held. Nothing to suppress."));
		}

		return;
	}

	bDodgeConsumed = true;

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRJump, Log, TEXT("Dodge claimed the hold, release will not jump."));
	}
}

void UVRJumpInputComponent::ResetJumpState()
{
	const bool had_deferred_jump = bJumpDeferred;

	bJumpDeferred  = false;
	bDodgeConsumed = false;

	if (had_deferred_jump && bLogDiagnostics)
	{
		UE_LOG(LogVRJump, Log, TEXT("Deferred jump abandoned."));
	}
}

// ── Execution ──

void UVRJumpInputComponent::PerformJump(const TCHAR* reason)
{
	ACharacter* character = Cast<ACharacter>(GetOwner());

	if (!character)
	{
		if (!bWarnedNoCharacter)
		{
			bWarnedNoCharacter = true;

			UE_LOG(LogVRJump, Error,
				TEXT("Owner %s is not a Character, so it cannot jump."),
				*GetNameSafe(GetOwner()));
		}

		return;
	}

	character->Jump();

	if (!OnJumpPerformed.IsBound() && !bWarnedNoBinding)
	{
		bWarnedNoBinding = true;

		UE_LOG(LogVRJump, Warning,
			TEXT("Jumped with nothing bound to OnJumpPerformed. Anything that used to ")
			TEXT("chain off the Jump node -- Wall Run Jump Off in particular -- must be ")
			TEXT("bound in the pawn's Event BeginPlay or it will never run again."));
	}

	OnJumpPerformed.Broadcast();

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRJump, Log, TEXT("Jump performed (%s)."), reason);
	}
}
