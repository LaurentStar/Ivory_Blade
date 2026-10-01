// VRDodgeOffsetComponent.cpp
//
// Kid Icarus: Uprising-style dodge. The player holds jump + flicks the stick
// while inside an alert-box overlap to trigger a permanent horizontal dash.
// An assist nudges the direction perpendicular to the incoming attack (never
// opposing the stick). Dodging INTO an attack triggers a vertical duck instead.
// I-frames cover the dash, and a fatigue counter degrades distance, i-frames,
// and cooldown on consecutive use. Debug arrows: yellow = raw stick input,
// green = assisted direction (redrawn each dash frame).

#include "VRDodgeOffsetComponent.h"
#include "VRThreatSourceComponent.h"
#include "VRAttackTrajectoryComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Components/ShapeComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogVRDodge);

namespace
{
	const TCHAR* PhaseName(EVRDodgePhase phase)
	{
		switch (phase)
		{
		case EVRDodgePhase::Dash:     return TEXT("Dash");
		case EVRDodgePhase::Cooldown: return TEXT("Cooldown");
		default:                      return TEXT("Idle");
		}
	}

	const TCHAR* DuckPhaseName(EVRDuckPhase phase)
	{
		switch (phase)
		{
		case EVRDuckPhase::Down:   return TEXT("DuckDown");
		case EVRDuckPhase::Return: return TEXT("DuckReturn");
		default:                   return TEXT("DuckIdle");
		}
	}
}

UVRDodgeOffsetComponent::UVRDodgeOffsetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

// ── Requests ──

bool UVRDodgeOffsetComponent::RequestOffsetDodgeExact(FVector local_direction, float distance_cm)
{
	return BeginDodge(local_direction, distance_cm);
}

// Gaze-relative stick dodge. Reads per-attack DodgeDistance from the overlapping threat source.
bool UVRDodgeOffsetComponent::RequestOffsetDodgeFromStick(FVector2D stick)
{
	const AActor* owner = GetOwner();

	if (!owner)
	{
		return false;
	}

	if (stick.SizeSquared() < FMath::Square(StickDeadzone))
	{
		return false;
	}

	if (!IsInAlertZone())
	{
		return false;
	}

	const FQuat actor_frame = owner->GetActorQuat();
	const FVector actor_up  = actor_frame.GetAxisZ();

	FRotator view_rotation = owner->GetActorRotation();

	if (const APawn* pawn = Cast<APawn>(owner))
	{
		if (AController* controller = pawn->GetController())
		{
			FVector view_location;
			controller->GetPlayerViewPoint(view_location, view_rotation);
		}
	}

	FVector gaze_forward = FVector::VectorPlaneProject(view_rotation.Vector(), actor_up);

	if (!gaze_forward.Normalize())
	{
		gaze_forward = actor_frame.GetAxisX();
	}

	const FVector gaze_right = FVector::CrossProduct(actor_up, gaze_forward);
	const FVector world_dir = gaze_forward * stick.Y + gaze_right * stick.X;

	float dodge_distance = DefaultDodgeDistance;

	if (const UVRThreatSourceComponent* threat = GetOverlappingThreatSource())
	{
		dodge_distance = threat->DodgeDistance;
	}

	const FVector local_dir = actor_frame.UnrotateVector(world_dir);
	return BeginDodge(local_dir, dodge_distance);
}

// ── Queries ──

bool UVRDodgeOffsetComponent::IsInAlertZone() const
{
	const AActor* owner = GetOwner();

	if (!owner)
	{
		return false;
	}

	TArray<UPrimitiveComponent*> overlapping;
	owner->GetOverlappingComponents(overlapping);

	for (const UPrimitiveComponent* comp : overlapping)
	{
		if (comp && comp->ComponentHasTag(TEXT("AlertBox")))
		{
			return true;
		}
	}

	return false;
}

bool UVRDodgeOffsetComponent::IsDodging() const
{
	return Phase == EVRDodgePhase::Dash;
}

bool UVRDodgeOffsetComponent::IsDucking() const
{
	return DuckPhase == EVRDuckPhase::Down || DuckPhase == EVRDuckPhase::Return;
}

float UVRDodgeOffsetComponent::GetHurtboxRadius() const
{
	if (CachedHurtboxRadius >= 0.0f)
	{
		return CachedHurtboxRadius;
	}

	if (const AActor* owner = GetOwner())
	{
		TArray<UShapeComponent*> shapes;
		owner->GetComponents<UShapeComponent>(shapes);

		for (const UShapeComponent* shape : shapes)
		{
			if (!shape || !shape->ComponentHasTag(HurtboxTag))
			{
				continue;
			}

			if (const USphereComponent* sphere = Cast<USphereComponent>(shape))
			{
				CachedHurtboxRadius = sphere->GetScaledSphereRadius();
			}
			else if (const UCapsuleComponent* capsule = Cast<UCapsuleComponent>(shape))
			{
				CachedHurtboxRadius = capsule->GetScaledCapsuleRadius();
			}
			else
			{
				CachedHurtboxRadius = shape->Bounds.SphereRadius;
			}

			return CachedHurtboxRadius;
		}
	}

	if (!bWarnedNoHurtbox)
	{
		bWarnedNoHurtbox = true;
		UE_LOG(LogVRDodge, Warning,
			TEXT("No shape component tagged '%s' on %s. Falling back to %.1f cm."),
			*HurtboxTag.ToString(), *GetNameSafe(GetOwner()), FallbackHurtboxRadius);
	}

	return FallbackHurtboxRadius;
}

UVRAttackTrajectoryComponent* UVRDodgeOffsetComponent::GetOverlappingTrajectory() const
{
	const UVRThreatSourceComponent* threat = GetOverlappingThreatSource();
	return threat ? threat->GetTrajectory() : nullptr;
}

// Closest alert-box owner's threat source. Used for per-attack DodgeDistance and trajectory.
UVRThreatSourceComponent* UVRDodgeOffsetComponent::GetOverlappingThreatSource() const
{
	const AActor* owner = GetOwner();

	if (!owner)
	{
		return nullptr;
	}

	TArray<UPrimitiveComponent*> overlapping;
	owner->GetOverlappingComponents(overlapping);

	float closest_dist_sq = TNumericLimits<float>::Max();
	UVRThreatSourceComponent* closest_threat = nullptr;
	const FVector player_pos = owner->GetActorLocation();

	for (const UPrimitiveComponent* comp : overlapping)
	{
		if (!comp || !comp->ComponentHasTag(TEXT("AlertBox")))
		{
			continue;
		}

		AActor* alert_owner = comp->GetOwner();

		if (!alert_owner)
		{
			continue;
		}

		UVRThreatSourceComponent* threat =
			alert_owner->FindComponentByClass<UVRThreatSourceComponent>();

		if (!threat)
		{
			continue;
		}

		const float dist_sq = FVector::DistSquared(player_pos, alert_owner->GetActorLocation());

		if (dist_sq < closest_dist_sq)
		{
			closest_dist_sq = dist_sq;
			closest_threat = threat;
		}
	}

	return closest_threat;
}

// ── Core ──

// Sets up phases, caches assist state, detects dodge-into-attack for duck, activates i-frames.
bool UVRDodgeOffsetComponent::BeginDodge(FVector local_direction, float distance_cm)
{
	if (Phase != EVRDodgePhase::Idle || DuckPhase != EVRDuckPhase::Idle)
	{
		return false;
	}

	if (bExhausted)
	{
		if (bLogDiagnostics)
		{
			UE_LOG(LogVRDodge, Warning, TEXT("Dodge rejected: exhausted (%.1fs remaining)."), ExhaustedTimer);
		}
		return false;
	}

	if (bProjectToTangentPlane)
	{
		local_direction.Z = 0.0f;
	}

	if (!local_direction.Normalize())
	{
		UE_LOG(LogVRDodge, Warning, TEXT("Dodge rejected: direction has no usable length after tangent projection."));
		return false;
	}

	const float clamped = FMath::Clamp(GetEffectiveDodgeDistance(distance_cm), 0.0f, MaxOffsetDistance);

	if (clamped <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogVRDodge, Warning, TEXT("Dodge rejected: requested distance %.1f cm resolves to zero."), distance_cm);
		return false;
	}

	const AActor* owner     = GetOwner();
	const FQuat actor_frame = owner ? owner->GetActorQuat() : FQuat::Identity;

	WorldDirection      = actor_frame.RotateVector(local_direction);
	TargetDistance      = clamped;
	LastDesiredDistance  = 0.0f;
	AppliedWorldOffset  = FVector::ZeroVector;
	AppliedDistance     = 0.0f;
	Phase               = EVRDodgePhase::Dash;
	PhaseTime           = 0.0f;

	LastStickWorldDir   = WorldDirection;
	bAssistActive       = (GetOverlappingTrajectory() != nullptr);

	// ── Detect dodge-into-attack and trigger duck ──
	if (bAssistActive)
	{
		if (const UVRAttackTrajectoryComponent* traj = GetOverlappingTrajectory())
		{
			const FVector attack_dir = traj->GetCurrentDirection();
			const FVector actor_up = actor_frame.GetAxisZ();
			const FVector stick_tangent = FVector::VectorPlaneProject(WorldDirection, actor_up).GetSafeNormal();
			const FVector attack_tangent = FVector::VectorPlaneProject(attack_dir, actor_up).GetSafeNormal();

			const float into_dot = FVector::DotProduct(stick_tangent, attack_tangent);

			if (into_dot > DodgeIntoThreshold)
			{
				DuckPhase = EVRDuckPhase::Down;
				DuckPhaseTime = 0.0f;
				DuckTargetDepth = GetHurtboxRadius() * 1.5f;

				if (bLogDiagnostics)
				{
					UE_LOG(LogVRDodge, Log,
						TEXT("Dodging INTO attack (dot:%.2f > %.2f). Duck engaged, depth:%.1f cm."),
						into_dot, DodgeIntoThreshold, DuckTargetDepth);
				}
			}
		}
	}

	if (DuckPhase == EVRDuckPhase::Idle)
	{
		DuckPhase         = EVRDuckPhase::Idle;
		DuckPhaseTime     = 0.0f;
		DuckTargetDepth   = 0.0f;
		AppliedDuckOffset = FVector::ZeroVector;
	}

	// ── Activate i-frames (fatigue-scaled) ──
	bInvulnerable = true;
	IFrameTimer = GetEffectiveIFrameDuration();

	// ── Fatigue bookkeeping ──
	Fatigue = FMath::Min(Fatigue + 1.0f, static_cast<float>(MaxFatigue));
	TimeSinceLastDodge = 0.0f;

	if (Fatigue >= static_cast<float>(MaxFatigue))
	{
		bExhausted = true;
		ExhaustedTimer = ExhaustedDuration;

		if (bLogDiagnostics)
		{
			UE_LOG(LogVRDodge, Warning, TEXT("Max fatigue reached (%d). Exhausted for %.1fs."),
				MaxFatigue, ExhaustedDuration);
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bLogDiagnostics && owner)
	{
		const FVector start = owner->GetActorLocation();
		const float arrow_len = TargetDistance;
		const float head_size = 12.0f;
		const float thickness = 2.5f;
		const float life = 20.0f;

		const FVector arrow_end = start + WorldDirection * arrow_len;
		DrawDebugDirectionalArrow(GetWorld(), start, arrow_end,
			head_size, FColor::Yellow, false, life, 0, thickness);
	}
#endif

	if (bLogDiagnostics)
	{
		const FVector actor_up = actor_frame.GetAxisZ();

		UE_LOG(LogVRDodge, Log,
			TEXT("Dodge start Local:(%.2f,%.2f,%.2f) Dist:%.1f (max %.1f) Hurtbox:%.1f ")
			TEXT("AlertZone:%s IFrames:%.2fs Assist:%s ActorUp:(%.2f,%.2f,%.2f) WorldDir:(%.2f,%.2f,%.2f)"),
			local_direction.X, local_direction.Y, local_direction.Z,
			TargetDistance, MaxOffsetDistance, GetHurtboxRadius(),
			IsInAlertZone() ? TEXT("yes") : TEXT("no"),
			BaseIFrameDuration,
			bAssistActive ? TEXT("yes") : TEXT("no"),
			actor_up.X, actor_up.Y, actor_up.Z,
			WorldDirection.X, WorldDirection.Y, WorldDirection.Z);
	}

	return true;
}

void UVRDodgeOffsetComponent::CancelDodge()
{
	if (Phase == EVRDodgePhase::Idle && DuckPhase == EVRDuckPhase::Idle)
	{
		return;
	}

	if (DuckPhase != EVRDuckPhase::Idle)
	{
		ApplyVerticalOffset(0.0f);
		FinishDuck();
	}

	bInvulnerable = false;
	IFrameTimer = 0.0f;

	ReturnToIdle();
}

// ── Scoreboard ──

void UVRDodgeOffsetComponent::ReportHit(AActor* threat)
{
	++HitCount;

	UE_LOG(LogVRDodge, Warning,
		TEXT("HIT by %s. Phase:%s Duck:%s Applied:%.1f cm. Tally %d hit / %d evaded (%.0f%% evaded)."),
		*GetNameSafe(threat), PhaseName(Phase), DuckPhaseName(DuckPhase),
		AppliedDistance,
		HitCount, EvadeCount,
		(HitCount + EvadeCount) > 0 ? 100.0f * EvadeCount / (HitCount + EvadeCount) : 0.0f);
}

void UVRDodgeOffsetComponent::ReportEvaded(AActor* threat)
{
	++EvadeCount;

	UE_LOG(LogVRDodge, Log,
		TEXT("Evaded %s. Phase:%s Duck:%s Applied:%.1f cm. Tally %d hit / %d evaded (%.0f%% evaded)."),
		*GetNameSafe(threat), PhaseName(Phase), DuckPhaseName(DuckPhase),
		AppliedDistance,
		HitCount, EvadeCount,
		(HitCount + EvadeCount) > 0 ? 100.0f * EvadeCount / (HitCount + EvadeCount) : 0.0f);
}

void UVRDodgeOffsetComponent::LogScoreboard() const
{
	const int32 total = HitCount + EvadeCount;

	UE_LOG(LogVRDodge, Log,
		TEXT("Scoreboard: %d threats, %d hit, %d evaded (%.0f%%)."),
		total, HitCount, EvadeCount,
		total > 0 ? 100.0f * EvadeCount / total : 0.0f);
}

void UVRDodgeOffsetComponent::ResetScoreboard()
{
	HitCount  = 0;
	EvadeCount = 0;

	UE_LOG(LogVRDodge, Log, TEXT("Scoreboard reset."));
}

// ── Timeline ──

float UVRDodgeOffsetComponent::GetPhaseDuration(EVRDodgePhase phase) const
{
	switch (phase)
	{
	case EVRDodgePhase::Dash:     return DashDuration;
	case EVRDodgePhase::Cooldown: return GetEffectiveCooldownDuration();
	default:                      return 0.0f;
	}
}

float UVRDodgeOffsetComponent::GetDuckPhaseDuration(EVRDuckPhase phase) const
{
	switch (phase)
	{
	case EVRDuckPhase::Down:   return DuckDownDuration;
	case EVRDuckPhase::Return: return DuckReturnDuration;
	default:                   return 0.0f;
	}
}

float UVRDodgeOffsetComponent::EvaluateCurve(const UCurveFloat* curve, float alpha) const
{
	if (curve)
	{
		return curve->GetFloatValue(alpha);
	}

	return FMath::InterpEaseInOut(0.0f, 1.0f, alpha, 2.0f);
}

float UVRDodgeOffsetComponent::ComputeDashAlpha() const
{
	if (Phase != EVRDodgePhase::Dash)
	{
		return (Phase == EVRDodgePhase::Idle) ? 0.0f : 1.0f;
	}

	const float raw = DashDuration > 0.0f ? FMath::Clamp(PhaseTime / DashDuration, 0.0f, 1.0f) : 1.0f;

	if (DashCurve)
	{
		return DashCurve->GetFloatValue(raw);
	}

	return FMath::InterpEaseOut(0.0f, 1.0f, raw, 2.0f);
}

float UVRDodgeOffsetComponent::ComputeDuckAlpha() const
{
	switch (DuckPhase)
	{
	case EVRDuckPhase::Down:
	{
		const float raw = DuckDownDuration > 0.0f
			? FMath::Clamp(DuckPhaseTime / DuckDownDuration, 0.0f, 1.0f) : 1.0f;
		return FMath::InterpEaseOut(0.0f, 1.0f, raw, 2.0f);
	}

	case EVRDuckPhase::Return:
	{
		const float raw = DuckReturnDuration > 0.0f
			? FMath::Clamp(DuckPhaseTime / DuckReturnDuration, 0.0f, 1.0f) : 1.0f;
		return 1.0f - EvaluateCurve(DuckReturnCurve, raw);
	}

	default:
		return 0.0f;
	}
}

void UVRDodgeOffsetComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ── I-frame countdown (runs independently of dodge phase) ──
	if (bInvulnerable)
	{
		IFrameTimer -= DeltaTime;

		if (IFrameTimer <= 0.0f)
		{
			bInvulnerable = false;
			IFrameTimer = 0.0f;

			if (bLogDiagnostics)
			{
				UE_LOG(LogVRDodge, Log, TEXT("I-frames expired."));
			}
		}
	}

	// ── Fatigue decay ──
	TimeSinceLastDodge += DeltaTime;

	if (bExhausted)
	{
		ExhaustedTimer -= DeltaTime;

		if (ExhaustedTimer <= 0.0f)
		{
			bExhausted = false;
			ExhaustedTimer = 0.0f;
			Fatigue = FMath::Max(Fatigue - 1.0f, 0.0f);

			if (bLogDiagnostics)
			{
				UE_LOG(LogVRDodge, Log, TEXT("Exhausted state ended. Fatigue:%.1f"), Fatigue);
			}
		}
	}
	else if (Fatigue > 0.0f && TimeSinceLastDodge >= FatigueDecayDelay)
	{
		Fatigue = FMath::Max(Fatigue - FatigueDecayRate * DeltaTime, 0.0f);
	}

	const bool phase_active = Phase != EVRDodgePhase::Idle;
	const bool duck_active  = DuckPhase != EVRDuckPhase::Idle;

	if (!phase_active && !duck_active)
	{
		return;
	}

	// ── Primary phase machine (Dash → Cooldown → Idle) ──
	if (phase_active)
	{
		PhaseTime += DeltaTime;

		const float duration = FMath::Max(GetPhaseDuration(Phase), 0.0f);

		if (PhaseTime >= duration)
		{
			PhaseTime -= duration;

			if (Phase == EVRDodgePhase::Dash)
			{
				FinishDash();
				Phase = EVRDodgePhase::Cooldown;
			}
			else
			{
				Phase = EVRDodgePhase::Idle;

				if (DuckPhase == EVRDuckPhase::Idle)
				{
					ReturnToIdle();
					return;
				}
			}
		}

		if (Phase == EVRDodgePhase::Dash)
		{
			UpdateDirectionWithAssist();

#if ENABLE_DRAW_DEBUG
			if (bLogDiagnostics)
			{
				if (const AActor* dbg_owner = GetOwner())
				{
					const FVector dbg_start = dbg_owner->GetActorLocation();
					const FVector dbg_end = dbg_start + WorldDirection * TargetDistance;
					DrawDebugDirectionalArrow(GetWorld(), dbg_start, dbg_end,
						12.0f, FColor::Green, false, 20.0f, 0, 2.5f);
				}
			}
#endif

			ApplyHorizontalOffset(TargetDistance * ComputeDashAlpha());
		}
	}

	// ── Duck phase machine (Down → Return → Idle), runs in parallel ──
	if (duck_active)
	{
		DuckPhaseTime += DeltaTime;

		const float duck_dur = FMath::Max(GetDuckPhaseDuration(DuckPhase), 0.0f);

		if (DuckPhaseTime >= duck_dur)
		{
			DuckPhaseTime -= duck_dur;

			if (DuckPhase == EVRDuckPhase::Down)
			{
				DuckPhase = EVRDuckPhase::Return;
			}
			else if (DuckPhase == EVRDuckPhase::Return)
			{
				FinishDuck();

				if (Phase == EVRDodgePhase::Idle)
				{
					ReturnToIdle();
					return;
				}
			}
		}

		if (DuckPhase != EVRDuckPhase::Idle)
		{
			ApplyVerticalOffset(DuckTargetDepth * ComputeDuckAlpha());
		}
	}

	LogHeartbeat();
}

// ── Motion ──

// Moves the capsule along WorldDirection. Position-based, not velocity-based.
void UVRDodgeOffsetComponent::ApplyHorizontalOffset(float desired_distance)
{
	AActor* owner = GetOwner();

	if (!owner)
	{
		return;
	}

	desired_distance    = FMath::Clamp(desired_distance, 0.0f, MaxOffsetDistance);
	LastDesiredDistance = desired_distance;

	const FQuat actor_frame = owner->GetActorQuat();
	const FVector actor_up  = actor_frame.GetAxisZ();

	if (bProjectToTangentPlane)
	{
		FVector projected = FVector::VectorPlaneProject(WorldDirection, actor_up);

		if (projected.Normalize())
		{
			WorldDirection = projected;
		}
	}

	const FVector desired_world = WorldDirection * desired_distance;
	FVector delta = desired_world - AppliedWorldOffset;

	const float max_step = MaxOffsetDistance * 2.0f;

	if (delta.SizeSquared() > FMath::Square(max_step))
	{
		delta = delta.GetSafeNormal() * max_step;
	}

	if (delta.IsNearlyZero())
	{
		return;
	}

	const FVector before = owner->GetActorLocation();
	FHitResult hit;

	UCharacterMovementComponent* movement = nullptr;

	if (const ACharacter* character = Cast<ACharacter>(owner))
	{
		movement = character->GetCharacterMovement();
	}

	if (movement && movement->UpdatedComponent)
	{
		movement->SafeMoveUpdatedComponent(
			delta, movement->UpdatedComponent->GetComponentQuat(), bSweep, hit);
	}
	else
	{
		owner->AddActorWorldOffset(delta, bSweep, &hit);
	}

	const FVector achieved = owner->GetActorLocation() - before;

	AppliedWorldOffset += achieved;
	AppliedDistance     = AppliedWorldOffset.Size();

	LogBlockedMove(delta, achieved, WorldDirection, actor_up, hit);
}

void UVRDodgeOffsetComponent::ApplyVerticalOffset(float desired_depth)
{
	AActor* owner = GetOwner();

	if (!owner)
	{
		return;
	}

	const FVector actor_up = owner->GetActorQuat().GetAxisZ();
	const FVector desired_duck = -actor_up * desired_depth;
	FVector delta = desired_duck - AppliedDuckOffset;

	if (delta.IsNearlyZero())
	{
		return;
	}

	const FVector before = owner->GetActorLocation();
	FHitResult hit;

	UCharacterMovementComponent* movement = nullptr;

	if (const ACharacter* character = Cast<ACharacter>(owner))
	{
		movement = character->GetCharacterMovement();
	}

	if (movement && movement->UpdatedComponent)
	{
		movement->SafeMoveUpdatedComponent(
			delta, movement->UpdatedComponent->GetComponentQuat(), bSweep, hit);
	}
	else
	{
		owner->AddActorWorldOffset(delta, bSweep, &hit);
	}

	AppliedDuckOffset += owner->GetActorLocation() - before;
}

void UVRDodgeOffsetComponent::LogBlockedMove(const FVector& requested, const FVector& achieved,
                                             const FVector& world_direction, const FVector& actor_up,
                                             const FHitResult& hit) const
{
	if (!bLogDiagnostics)
	{
		return;
	}

	const float shortfall = requested.Size() - achieved.Size();

	if (shortfall < 0.1f)
	{
		return;
	}

	UE_LOG(LogVRDodge, Warning,
		TEXT("Move short %.2f cm (asked %.2f, got %.2f) Blocker:%s.%s StartPenetrating:%d ")
		TEXT("Normal:(%.2f,%.2f,%.2f) ActorUp:(%.2f,%.2f,%.2f) WorldDir:(%.2f,%.2f,%.2f)"),
		shortfall, requested.Size(), achieved.Size(),
		*GetNameSafe(hit.GetActor()), *GetNameSafe(hit.GetComponent()),
		hit.bStartPenetrating ? 1 : 0,
		hit.ImpactNormal.X, hit.ImpactNormal.Y, hit.ImpactNormal.Z,
		actor_up.X, actor_up.Y, actor_up.Z,
		world_direction.X, world_direction.Y, world_direction.Z);
}

void UVRDodgeOffsetComponent::FinishDash()
{
	ApplyHorizontalOffset(TargetDistance);
	AppliedWorldOffset = FVector::ZeroVector;
	AppliedDistance     = 0.0f;

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRDodge, Log, TEXT("Dash complete. Displacement is permanent."));
	}
}

void UVRDodgeOffsetComponent::FinishDuck()
{
	ApplyVerticalOffset(0.0f);

	if (!AppliedDuckOffset.IsNearlyZero(0.5f))
	{
		UE_LOG(LogVRDodge, Warning, TEXT("Duck return obstructed. Accepting residual offset of %.2f cm."),
			AppliedDuckOffset.Size());
	}

	AppliedDuckOffset = FVector::ZeroVector;
	DuckTargetDepth   = 0.0f;
	DuckPhase         = EVRDuckPhase::Idle;
	DuckPhaseTime     = 0.0f;
}

void UVRDodgeOffsetComponent::ReturnToIdle()
{
	Phase               = EVRDodgePhase::Idle;
	PhaseTime           = 0.0f;
	TargetDistance      = 0.0f;
	LastDesiredDistance = 0.0f;
	WorldDirection      = FVector::ZeroVector;
	LastStickWorldDir   = FVector::ZeroVector;
	bAssistActive       = false;

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRDodge, Log, TEXT("Dodge complete. Ready."));
	}
}

// ── Assist ──

// Per-tick assist: blends WorldDirection toward the perpendicular clear vector with decaying weight.
void UVRDodgeOffsetComponent::UpdateDirectionWithAssist()
{
	if (!bAssistActive || Phase != EVRDodgePhase::Dash)
	{
		return;
	}

	const UVRAttackTrajectoryComponent* traj = GetOverlappingTrajectory();

	if (!traj)
	{
		return;
	}

	const AActor* owner = GetOwner();

	if (!owner)
	{
		return;
	}

	const FQuat actor_frame = owner->GetActorQuat();
	const FVector actor_up = actor_frame.GetAxisZ();
	const FVector attack_dir = traj->GetCurrentDirection();

	const float dash_alpha = ComputeDashAlpha();
	const float effective_weight = AssistWeight * (1.0f - dash_alpha);

	if (effective_weight < KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector clear = ComputeClearVector(LastStickWorldDir, attack_dir, actor_up);

	if (clear.IsNearlyZero())
	{
		return;
	}

	FVector blended = FMath::Lerp(WorldDirection, clear, effective_weight);

	if (bProjectToTangentPlane)
	{
		blended = FVector::VectorPlaneProject(blended, actor_up);
	}

	if (blended.Normalize())
	{
		WorldDirection = blended;
	}
}

// Perpendicular to attack direction, on the side closest to the stick. Never opposes the stick.
FVector UVRDodgeOffsetComponent::ComputeClearVector(const FVector& stick_world,
                                                     const FVector& attack_dir,
                                                     const FVector& actor_up) const
{
	const FVector attack_tangent = FVector::VectorPlaneProject(attack_dir, actor_up).GetSafeNormal();
	const FVector stick_tangent = FVector::VectorPlaneProject(stick_world, actor_up).GetSafeNormal();

	if (attack_tangent.IsNearlyZero() || stick_tangent.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const FVector perp = FVector::CrossProduct(attack_tangent, actor_up).GetSafeNormal();

	if (perp.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const float side = FVector::DotProduct(stick_tangent, perp);
	const FVector clear_dir = (side >= 0.0f) ? perp : -perp;

	return clear_dir;
}

// ── Fatigue scaling ──

float UVRDodgeOffsetComponent::GetFatigueRatio() const
{
	if (MaxFatigue <= 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(Fatigue / static_cast<float>(MaxFatigue), 0.0f, 1.0f);
}

float UVRDodgeOffsetComponent::GetEffectiveIFrameDuration() const
{
	const float ratio = GetFatigueRatio();
	const float scale = FMath::Lerp(1.0f, MinIFrameScale, ratio);
	return BaseIFrameDuration * scale;
}

float UVRDodgeOffsetComponent::GetEffectiveDodgeDistance(float base_distance) const
{
	const float ratio = GetFatigueRatio();
	const float scale = FMath::Lerp(1.0f, MinDistanceScale, ratio);
	return base_distance * scale;
}

float UVRDodgeOffsetComponent::GetEffectiveCooldownDuration() const
{
	const float ratio = GetFatigueRatio();
	const float scale = FMath::Lerp(1.0f, MaxCooldownScale, ratio);
	return CooldownDuration * scale;
}

void UVRDodgeOffsetComponent::LogHeartbeat() const
{
	if (!bLogDiagnostics)
	{
		return;
	}

	UE_LOG(LogVRDodge, Log,
		TEXT("Phase:%s t:%.3f Duck:%s dt:%.3f Target:%.1f Want:%.1f Applied:%.1f Short:%.1f WorldDir:(%.2f,%.2f,%.2f)"),
		PhaseName(Phase), PhaseTime, DuckPhaseName(DuckPhase), DuckPhaseTime,
		TargetDistance, LastDesiredDistance, AppliedDistance,
		FMath::Max(LastDesiredDistance - AppliedDistance, 0.0f),
		WorldDirection.X, WorldDirection.Y, WorldDirection.Z);
}

// ── Console ──

namespace
{
	UVRDodgeOffsetComponent* FindLocalDodgeComponent(UWorld* world)
	{
		if (!world)
		{
			return nullptr;
		}

		for (FConstPlayerControllerIterator it = world->GetPlayerControllerIterator(); it; ++it)
		{
			const APlayerController* controller = it->Get();

			if (!controller)
			{
				continue;
			}

			if (APawn* pawn = controller->GetPawn())
			{
				if (UVRDodgeOffsetComponent* component = pawn->FindComponentByClass<UVRDodgeOffsetComponent>())
				{
					return component;
				}
			}
		}

		return nullptr;
	}

	void HandleTestOffsetCommand(const TArray<FString>& args, UWorld* world)
	{
		UVRDodgeOffsetComponent* component = FindLocalDodgeComponent(world);

		if (!component)
		{
			UE_LOG(LogVRDodge, Warning, TEXT("vr.Dodge.TestOffset: no VRDodgeOffsetComponent on the local pawn."));
			return;
		}

		if (args.Num() < 2)
		{
			UE_LOG(LogVRDodge, Warning,
				TEXT("Usage: vr.Dodge.TestOffset <local_x> <local_y> [distance_cm]"));
			return;
		}

		const FVector direction(FCString::Atof(*args[0]), FCString::Atof(*args[1]), 0.0f);
		const float dist = args.Num() >= 3 ? FCString::Atof(*args[2]) : component->DefaultDodgeDistance;

		const bool accepted = component->RequestOffsetDodgeExact(direction, dist);

		UE_LOG(LogVRDodge, Log, TEXT("vr.Dodge.TestOffset (%.2f,%.2f) dist:%.1f -> %s"),
			direction.X, direction.Y, dist, accepted ? TEXT("accepted") : TEXT("rejected"));
	}

	void HandleTestGazeCommand(const TArray<FString>& args, UWorld* world)
	{
		UVRDodgeOffsetComponent* component = FindLocalDodgeComponent(world);

		if (!component)
		{
			UE_LOG(LogVRDodge, Warning, TEXT("vr.Dodge.TestGaze: no VRDodgeOffsetComponent on the local pawn."));
			return;
		}

		if (args.Num() < 2)
		{
			UE_LOG(LogVRDodge, Warning, TEXT("Usage: vr.Dodge.TestGaze <stick_x> <stick_y>"));
			return;
		}

		const FVector2D stick(FCString::Atof(*args[0]), FCString::Atof(*args[1]));
		const bool accepted = component->RequestOffsetDodgeFromStick(stick);

		UE_LOG(LogVRDodge, Log, TEXT("vr.Dodge.TestGaze (%.2f,%.2f) -> %s"),
			stick.X, stick.Y, accepted ? TEXT("accepted") : TEXT("rejected"));
	}

	void HandleScoreCommand(const TArray<FString>& args, UWorld* world)
	{
		UVRDodgeOffsetComponent* component = FindLocalDodgeComponent(world);

		if (!component)
		{
			UE_LOG(LogVRDodge, Warning, TEXT("vr.Dodge.Score: no VRDodgeOffsetComponent on the local pawn."));
			return;
		}

		if (args.Num() > 0 && args[0].Equals(TEXT("reset"), ESearchCase::IgnoreCase))
		{
			component->ResetScoreboard();
			return;
		}

		component->LogScoreboard();
	}
}

static FAutoConsoleCommandWithWorldAndArgs GVRDodgeTestOffsetCommand(
	TEXT("vr.Dodge.TestOffset"),
	TEXT("vr.Dodge.TestOffset <local_x> <local_y> [distance_cm] - fires a dodge in actor space. Omit distance for DefaultDodgeDistance."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleTestOffsetCommand));

static FAutoConsoleCommandWithWorldAndArgs GVRDodgeTestGazeCommand(
	TEXT("vr.Dodge.TestGaze"),
	TEXT("vr.Dodge.TestGaze <stick_x> <stick_y> - fires a dodge relative to gaze. Requires alert zone overlap."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleTestGazeCommand));

static FAutoConsoleCommandWithWorldAndArgs GVRDodgeScoreCommand(
	TEXT("vr.Dodge.Score"),
	TEXT("vr.Dodge.Score [reset] - prints the hit/evade tally."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleScoreCommand));
