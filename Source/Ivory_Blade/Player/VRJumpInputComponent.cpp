#include "VRJumpInputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogVRJump);

namespace VRJumpDebug
{
	// One slot per kind of line, so a press is still on screen when its
	// release lands and the pair can be read as a sequence.
	constexpr int32 BeginPlaySlot = 0;
	constexpr int32 PressedSlot   = 1;
	constexpr int32 ReleasedSlot  = 2;
	constexpr int32 DodgeSlot     = 3;
	constexpr int32 PerformedSlot = 4;

	/** Long enough to take the headset off and still read the line. */
	constexpr float Lifetime = 12.0f;

	constexpr float StartupLifetime = 30.0f;

	/** "Walking", "Falling" and so on, without the EMovementMode:: prefix. */
	FString MovementModeName(const UCharacterMovementComponent* movement)
	{
		if (!movement)
		{
			return TEXT("no-movement-component");
		}

		return UEnum::GetDisplayValueAsText(movement->MovementMode).ToString();
	}
}

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

	ReportDebug(FString::Printf(TEXT("Jump BEGINPLAY on %s"), *GetNameSafe(GetOwner())),
		VRJumpDebug::BeginPlaySlot, VRJumpDebug::StartupLifetime, FColor::Green);

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

		ReportDebug(
			FString::Printf(TEXT("Jump BROKEN: owner %s is not a Character."), *GetNameSafe(GetOwner())),
			VRJumpDebug::BeginPlaySlot, VRJumpDebug::StartupLifetime, FColor::Red);
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

	const UWorld* world = GetWorld();
	JumpPressTimeSeconds = world ? world->GetTimeSeconds() : -1.0;

	if (!advance_mode_active)
	{
		// Clearing on the press as well as the release is what self-heals a lost
		// Completed event. A press outside advance mode always starts clean.
		ClearHoldTimeout();
		bJumpDeferred  = false;
		bDodgeConsumed = false;

		ReportDebug(TEXT("Jump PRESSED advance=N -> jumped now"),
			VRJumpDebug::PressedSlot, VRJumpDebug::Lifetime, FColor::Cyan);

		PerformJump(TEXT("press, outside advance mode"));
		return;
	}

	bJumpDeferred  = true;
	bDodgeConsumed = false;

	// Arm the net before the hold begins, not after. If the release is the
	// event that goes missing, nothing later in the gesture will arm it.
	if (JumpHoldTimeout > 0.0f && world)
	{
		world->GetTimerManager().SetTimer(JumpTimeoutHandle, this,
			&UVRJumpInputComponent::HandleJumpHoldTimeout, JumpHoldTimeout, false);
	}

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRJump, Log, TEXT("Press deferred to release (advance mode)."));
	}

	ReportDebug(TEXT("Jump PRESSED advance=Y -> deferred"),
		VRJumpDebug::PressedSlot, VRJumpDebug::Lifetime, FColor::Cyan);
}

void UVRJumpInputComponent::HandleJumpReleased()
{
	// The release arrived, so the hold resolved on its own. Cancel the net
	// before anything else can return early and leave it armed.
	ClearHoldTimeout();

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRJump, Warning, TEXT("RELEASED after %.2fs. deferred=%s consumed=%s"),
			GetCurrentHoldDuration(),
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

		ReportDebug(TEXT("Jump RELEASED deferred=N -> ignored"),
			VRJumpDebug::ReleasedSlot, VRJumpDebug::Lifetime, FColor::Silver);

		JumpPressTimeSeconds = -1.0;
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

		ReportDebug(TEXT("Jump RELEASED deferred=Y consumed=Y -> suppressed by dodge"),
			VRJumpDebug::ReleasedSlot, VRJumpDebug::Lifetime, FColor::Orange);

		JumpPressTimeSeconds = -1.0;
		return;
	}

	ReportDebug(FString::Printf(TEXT("Jump RELEASED deferred=Y consumed=N after %.2fs -> JUMP"),
			GetCurrentHoldDuration()),
		VRJumpDebug::ReleasedSlot, VRJumpDebug::Lifetime, FColor::Cyan);

	PerformJump(TEXT("release, advance mode"));

	JumpPressTimeSeconds = -1.0;
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

		ReportDebug(TEXT("Jump DODGE reported with no jump held -> nothing to suppress"),
			VRJumpDebug::DodgeSlot, VRJumpDebug::Lifetime, FColor::Orange);

		return;
	}

	bDodgeConsumed = true;

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRJump, Log, TEXT("Dodge claimed the hold, release will not jump."));
	}

	ReportDebug(TEXT("Jump DODGE claimed the hold -> release will not jump"),
		VRJumpDebug::DodgeSlot, VRJumpDebug::Lifetime, FColor::Yellow);
}

void UVRJumpInputComponent::ResetJumpState()
{
	const bool had_deferred_jump = bJumpDeferred;

	ClearHoldTimeout();

	bJumpDeferred        = false;
	bDodgeConsumed       = false;
	JumpPressTimeSeconds = -1.0;

	if (had_deferred_jump)
	{
		if (bLogDiagnostics)
		{
			UE_LOG(LogVRJump, Log, TEXT("Deferred jump abandoned."));
		}

		ReportDebug(TEXT("Jump RESET -> deferred jump abandoned"),
			VRJumpDebug::ReleasedSlot, VRJumpDebug::Lifetime, FColor::Orange);
	}
}

// ── Hold timing ──

float UVRJumpInputComponent::GetCurrentHoldDuration() const
{
	const UWorld* world = GetWorld();

	if (JumpPressTimeSeconds < 0.0 || !world)
	{
		return 0.0f;
	}

	return static_cast<float>(world->GetTimeSeconds() - JumpPressTimeSeconds);
}

void UVRJumpInputComponent::ClearHoldTimeout()
{
	if (const UWorld* world = GetWorld())
	{
		world->GetTimerManager().ClearTimer(JumpTimeoutHandle);
	}
}

void UVRJumpInputComponent::HandleJumpHoldTimeout()
{
	if (!bJumpDeferred)
	{
		return;
	}

	// The release never arrived. Performing the jump beats dropping it: a
	// stranded bJumpDeferred swallows every jump that comes after it, which is
	// a far worse failure than one jump landing slightly late.
	bJumpDeferred = false;

	UE_LOG(LogVRJump, Warning,
		TEXT("Release never arrived within %.2fs. Resolving the deferred jump. ")
		TEXT("If this is frequent, IA_Jump's Completed pin is not firing reliably."),
		JumpHoldTimeout);

	if (bDodgeConsumed)
	{
		bDodgeConsumed       = false;
		JumpPressTimeSeconds = -1.0;

		ReportDebug(TEXT("Jump TIMEOUT consumed=Y -> suppressed by dodge"),
			VRJumpDebug::ReleasedSlot, VRJumpDebug::Lifetime, FColor::Orange);

		return;
	}

	ReportDebug(FString::Printf(TEXT("Jump TIMEOUT after %.2fs, no release -> JUMP"),
			GetCurrentHoldDuration()),
		VRJumpDebug::ReleasedSlot, VRJumpDebug::Lifetime, FColor::Yellow);

	PerformJump(TEXT("hold timeout, release never arrived"));

	JumpPressTimeSeconds = -1.0;
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

		ReportDebug(
			FString::Printf(TEXT("Jump DISCARDED: owner %s is not a Character"), *GetNameSafe(GetOwner())),
			VRJumpDebug::PerformedSlot, VRJumpDebug::Lifetime, FColor::Red);

		return;
	}

	// Sampled before Jump(), which mutates bPressedJump. Jump() returns nothing
	// and the movement component silently discards the request when CanJump is
	// false, so without this the report says PERFORMED either way.
	const UCharacterMovementComponent* movement = character->GetCharacterMovement();
	const bool can_jump = character->CanJump();

	character->Jump();

	if (!can_jump)
	{
		UE_LOG(LogVRJump, Warning,
			TEXT("Jump (%s) requested while CanJump is false. MovementMode=%s JumpZVelocity=%.0f ")
			TEXT("JumpCurrentCount=%d. The movement component will discard it."),
			reason, *VRJumpDebug::MovementModeName(movement),
			movement ? movement->JumpZVelocity : 0.0f, character->JumpCurrentCount);
	}

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

	// Three distinct failures share one symptom of "nothing happened", so all
	// three are on the line: the jump was never requested, the movement
	// component threw it away, or it jumped with nothing listening.
	ReportDebug(
		FString::Printf(TEXT("Jump %s (%s) bound=%s mode=%s vel=%.0f count=%d"),
			can_jump ? TEXT("PERFORMED") : TEXT("REFUSED"),
			reason,
			OnJumpPerformed.IsBound() ? TEXT("Y") : TEXT("N"),
			*VRJumpDebug::MovementModeName(movement),
			movement ? movement->JumpZVelocity : 0.0f,
			character->JumpCurrentCount),
		VRJumpDebug::PerformedSlot, VRJumpDebug::Lifetime,
		can_jump ? FColor::Green : FColor::Red);
}

void UVRJumpInputComponent::ReportDebug(const FString& message, int32 slot, float lifetime, FColor color) const
{
	if (!bShowDebugOnScreen || !GEngine)
	{
		return;
	}

	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(DebugMessageKey + slot), lifetime, color, message);
}
