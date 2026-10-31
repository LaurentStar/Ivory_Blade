// VRDodgeOffsetComponent.cpp
//
// Kid Icarus: Uprising-style dodge. The player holds jump + flicks the stick
// while inside an alert-box overlap to trigger a permanent horizontal dash.
// The player's aim sets the dash going, the threat turns it away from the
// attack by at most MaxAssistAngleDegrees, and the dash runs straight along
// that heading. Dodging INTO an attack triggers a vertical duck as well.
// I-frames cover the dash, and a fatigue counter degrades distance, i-frames,
// and cooldown on consecutive use.
//
// Releasing jump while the dash runs converts the jump it already suppressed
// into an upward launch, after which an upward assist decays away so gravity
// appears to ease back in. The decay clock never rewinds, so chained midair
// dodges run out of lift on their own.
//
// Debug arrows: white = raw stick input, yellow = the direction the dash
// committed to, green = the live direction (redrawn each dash frame).

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
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogVRDodge);

namespace VRDodgeAssist
{
	/**
	 * Speed in cm/s below which a threat counts as stationary and its velocity
	 * is not trusted as a heading. Covers physics jitter on a resting actor.
	 */
	constexpr float MinThreatSpeed = 1.0f;

	/** Points sampled along the dash when scoring how close a candidate gets to the attack. */
	constexpr int32 ApproachSamples = 12;

	/**
	 * Multiple of the dash duration the scoring looks ahead over.
	 *
	 * Past 1.0 so an attack that arrives just after the player has stopped
	 * still counts against the candidate that parked them in its way.
	 */
	constexpr float ApproachHorizon = 1.5f;

	/** Angular resolution of the cone sweep, in degrees. */
	constexpr float SweepStepDegrees = 10.0f;

	/**
	 * Signed angle from one tangent-plane vector to another, in degrees,
	 * positive about up. Both arguments are assumed normalized.
	 *
	 * Signed because the assist has to know which way round to turn; an
	 * unsigned angle from a dot product cannot tell left from right.
	 */
	float SignedAngleDeg(const FVector& from, const FVector& to, const FVector& up)
	{
		const float sine   = FVector::DotProduct(FVector::CrossProduct(from, to), up);
		const float cosine = FVector::DotProduct(from, to);

		return FMath::RadiansToDegrees(FMath::Atan2(sine, cosine));
	}
}

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

	// Ahead of every early return on purpose. A dodge that never fires and one
	// that fires in the wrong direction are indistinguishable from inside the
	// headset, and both start here.
	//
	// Orange means the stick arrived with no X component, which is what a
	// strafe that never reaches this function looks like -- either the A/D
	// rows in the mapping context swizzle onto Y like the W/S rows do, or the
	// pawn is not passing the action value through.
	ReportDebug(
		FString::Printf(TEXT("STICK raw (%.2f,%.2f)  mag=%.2f  deadzone=%.2f"),
			stick.X, stick.Y, stick.Size(), StickDeadzone),
		5,
		FMath::IsNearlyZero(stick.X, 0.01f) ? FColor::Orange : FColor::Green);

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

	// Separates a bad stick from bad projection maths. If raw carries an X and
	// local does not, the fault is here; if neither does, it is upstream.
	ReportDebug(
		FString::Printf(TEXT("  gazeF (%.2f,%.2f)  gazeR (%.2f,%.2f)  -> local (%.2f,%.2f)"),
			gaze_forward.X, gaze_forward.Y, gaze_right.X, gaze_right.Y,
			local_dir.X, local_dir.Y),
		6,
		FColor::Cyan);

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

// Where the closest threat is, how big it is, and how the player sits across its path.
FVRThreatGeometry UVRDodgeOffsetComponent::ResolveThreatGeometry() const
{
	FVRThreatGeometry geometry;

	const AActor* owner = GetOwner();
	const UVRThreatSourceComponent* threat = GetOverlappingThreatSource();

	if (!owner || !threat)
	{
		return geometry;
	}

	const AActor* threat_actor = threat->GetOwner();

	if (!threat_actor)
	{
		return geometry;
	}

	geometry.ThreatName   = GetNameSafe(threat_actor);
	geometry.ThreatRadius = threat->GetThreatRadius();

	const UVRAttackTrajectoryComponent* traj = threat->GetTrajectory();

	// Position comes from something real: the tagged collider, or failing that
	// the actor's own transform.
	//
	// Deliberately not the trajectory. GetCurrentPosition walks a straight
	// two-waypoint line built at BeginPlay with no gravity in it, which a
	// ProjectileMovement attack does not fly. The estimate runs ahead of the
	// real thing and can end up beyond the player, and since SideSign and
	// EscapeDirection are both measured from this point, a drifting estimate
	// inverts them and aims the dodge at the attack. The trajectory is still
	// trusted for heading, which stays usable as the position decays.
	geometry.HurtboxLocation = threat_actor->GetActorLocation();

	const USphereComponent* alert_box = threat->GetAlertBox();

	TArray<UShapeComponent*> shapes;
	threat_actor->GetComponents<UShapeComponent>(shapes);

	for (const UShapeComponent* shape : shapes)
	{
		if (!shape || shape == alert_box)
		{
			continue;
		}

		if (shape->ComponentHasTag(ThreatHurtboxTag))
		{
			geometry.HurtboxLocation = shape->GetComponentLocation();
			break;
		}
	}

	const FQuat   actor_frame = owner->GetActorQuat();
	const FVector actor_up    = actor_frame.GetAxisZ();
	const FVector player_pos  = owner->GetActorLocation();

	// Flattened once, here, so every measurement below is already tangent to
	// the surface and nothing downstream has to think about planetoid up.
	const FVector to_player =
		FVector::VectorPlaneProject(player_pos - geometry.HurtboxLocation, actor_up);

	// Three sources for the heading, strongest first. Actual velocity beats the
	// waypoint chain because the chain is authored up front and drifts from
	// anything that arcs, bounces, or is steered by ProjectileMovement, while
	// velocity is what the attack is doing this frame.
	FVector attack_dir =
		FVector::VectorPlaneProject(threat_actor->GetVelocity(), actor_up);

	// Captured before the vector is normalised away. Only a real velocity read
	// gives a speed: the trajectory fallback below supplies a unit direction
	// with no magnitude behind it, and treating that as 1 cm/s would be worse
	// than admitting the speed is unknown.
	const float tangent_speed = attack_dir.Size();

	if (tangent_speed >= VRDodgeAssist::MinThreatSpeed)
	{
		geometry.TangentSpeed = tangent_speed;
	}
	else
	{
		attack_dir = traj
			? FVector::VectorPlaneProject(traj->GetCurrentDirection(), actor_up)
			: FVector::ZeroVector;
	}

	if (!attack_dir.Normalize())
	{
		// A melee volume that is not travelling still has to be escaped.
		// Treating it as closing on the player gives the perpendicular
		// something sane to be perpendicular to.
		attack_dir = (-to_player).GetSafeNormal();
	}

	if (attack_dir.IsNearlyZero())
	{
		return geometry;
	}

	geometry.AttackDir     = attack_dir;
	geometry.AlongDistance = FVector::DotProduct(to_player, attack_dir);
	geometry.ClosingDot    = FVector::DotProduct(attack_dir, to_player.GetSafeNormal());

	const FVector perp_vec  = to_player - attack_dir * geometry.AlongDistance;
	geometry.MissDistance   = perp_vec.Size();

	// Zero when the attack is dead head-on, which is the one case this cannot
	// answer on its own. BeginDodge falls back to the stick there.
	geometry.EscapeDirection = perp_vec.GetSafeNormal();

	const FVector perp_axis = FVector::CrossProduct(attack_dir, actor_up).GetSafeNormal();
	geometry.SideSign       = FMath::Sign(FVector::DotProduct(perp_vec, perp_axis));

	geometry.bValid = true;

	return geometry;
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

	// Measured before the clamp, because bEnsureClearance can raise the request
	// and fatigue scaling plus MaxOffsetDistance must still have the last word.
	LastThreatGeometry = ResolveThreatGeometry();

	// Overlapping an alert box is the whole test. The developer already decided
	// which attacks are worth reacting to when they sized that box, and a
	// closing check layered on top of it only disagreed with them: standing
	// beside a passing attack failed it, which is precisely when the player
	// most needs to be moved off the thing.
	bAssistActive = LastThreatGeometry.bValid;

	if (bEnsureClearance && bAssistActive)
	{
		// How much further than the present gap the player has to travel before
		// the two volumes stop touching.
		const float needed = LastThreatGeometry.ThreatRadius + GetHurtboxRadius()
			+ ClearanceMargin - LastThreatGeometry.MissDistance;

		distance_cm = FMath::Max(distance_cm, needed);
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
	DashCommandedDistance = 0.0f;
	Phase               = EVRDodgePhase::Dash;
	PhaseTime           = 0.0f;

	LastStickWorldDir   = WorldDirection;

	// ── Settle the opening direction before the dash starts ──
	//
	// The first frame already carries whatever correction the assist wants, so
	// the dodge never visibly starts one way and swings another. Zero turn is
	// the common case: an aim that was going to clear the attack is left
	// exactly as the player set it.
	if (bAssistActive)
	{
		const FVector actor_up = actor_frame.GetAxisZ();

		FVector turned = LastStickWorldDir.RotateAngleAxis(
			ComputeAssistTurnDeg(LastThreatGeometry, LastStickWorldDir, actor_up, clamped),
			actor_up);

		if (bProjectToTangentPlane)
		{
			turned = FVector::VectorPlaneProject(turned, actor_up);
		}

		if (turned.Normalize())
		{
			WorldDirection = turned;
		}
	}

	DebugStickLocalDir  = local_direction;
	DebugEscapeLocalDir = actor_frame.UnrotateVector(LastThreatGeometry.EscapeDirection);
	DebugFinalLocalDir  = actor_frame.UnrotateVector(WorldDirection);

	++DodgeCount;
	DashStartActorLocation = owner ? owner->GetActorLocation() : FVector::ZeroVector;
	DashStartViewLocation  = GetViewLocation();

	// ── Detect dodge-into-attack and trigger duck ──
	if (bAssistActive)
	{
		const FVector actor_up = actor_frame.GetAxisZ();

		// The raw stick, deliberately, not the direction the dodge ended up
		// taking. This reads the player's intent to go at the attack, and the
		// threat has just overwritten WorldDirection with the opposite.
		const FVector stick_tangent =
			FVector::VectorPlaneProject(LastStickWorldDir, actor_up).GetSafeNormal();

		// AttackDir is already tangent-projected and normalized by the resolver.
		const float into_dot = FVector::DotProduct(stick_tangent, LastThreatGeometry.AttackDir);

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

		// Yellow is the direction the dash commits to, already turned off the
		// stick by the assist.
		const FVector arrow_end = start + WorldDirection * arrow_len;
		DrawDebugDirectionalArrow(GetWorld(), start, arrow_end,
			head_size, FColor::Yellow, false, life, 0, thickness);

		// White is the raw input, and exists because without it every arrow on
		// screen showed the post-assist direction. A dodge the assist had
		// turned ninety degrees looked identical to one the player had aimed
		// that way, which sent two rounds of debugging at the input pipeline
		// when the input was never wrong.
		//
		// Raised along actor_up, not world Z, so the two stay separable when
		// the assist declines to turn and they point the same way.
		const FVector input_start = start + actor_frame.GetAxisZ() * 5.0f;
		DrawDebugDirectionalArrow(GetWorld(), input_start,
			input_start + LastStickWorldDir * arrow_len,
			head_size, FColor::White, false, life, 0, thickness);
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

	// ── Settle check ──
	//
	// Above the early return on purpose. By the time anything reverts the pawn
	// the phase machine is usually back to Idle, and from there this function
	// returns before reaching the phase machines below.
	if (SettleTimer >= 0.0f)
	{
		SettleTimer -= DeltaTime;

		if (SettleTimer < 0.0f)
		{
			ReportSettleResult();
		}
	}

	// Above the early return for the same reason as the settle check: the
	// float outlives the dodge that launched it, and by the time gravity has
	// eased back in the phase machine has usually been Idle for a second.
	UpdateFloat(DeltaTime);

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

	// Incremental, not absolute. Targeting WorldDirection * desired_distance
	// as a fixed point meant that re-aiming the dash swung that point along an
	// arc, and the delta to reach it dragged the player sideways across ground
	// they had already covered -- the snap back and forth. Stepping along
	// whatever the dodge currently points at bends the path instead.
	const float step = desired_distance - DashCommandedDistance;
	DashCommandedDistance = desired_distance;

	FVector delta = WorldDirection * step;

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

	// Path length, not the straight line back to the start. A curved dash ends
	// closer to its origin than it travelled, and measuring the chord would
	// report that geometry as a blocked move in the short= diagnostic.
	AppliedDistance += achieved.Size();

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

	// Measured before AppliedWorldOffset is cleared, and from the actor rather
	// than from that running total, so a move the sweep refused is not counted.
	const AActor* owner = GetOwner();

	DashEndActorDisplacement = owner
		? FVector::Dist(owner->GetActorLocation(), DashStartActorLocation)
		: 0.0f;

	DashEndViewDisplacement = FVector::Dist(GetViewLocation(), DashStartViewLocation);

	AppliedWorldOffset = FVector::ZeroVector;
	AppliedDistance     = 0.0f;
	DashCommandedDistance = 0.0f;

	ReportDashResult();
	SettleTimer = SettleCheckDelay;

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRDodge, Log,
			TEXT("Dash complete. Displacement is permanent. Actor moved %.1f cm, view moved %.1f cm."),
			DashEndActorDisplacement, DashEndViewDisplacement);
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
	DashCommandedDistance = 0.0f;
	WorldDirection      = FVector::ZeroVector;
	LastStickWorldDir   = FVector::ZeroVector;
	bAssistActive       = false;

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRDodge, Log, TEXT("Dodge complete. Ready."));
	}
}

// ── Aerial ──

bool UVRDodgeOffsetComponent::IsAerialWindowOpen() const
{
	// Read off the phase machine rather than a timestamp of its own, so the
	// window tracks DashDuration automatically when that is retuned.
	if (Phase == EVRDodgePhase::Dash)
	{
		return true;
	}

	return Phase == EVRDodgePhase::Cooldown && PhaseTime <= AerialWindowTailSeconds;
}

float UVRDodgeOffsetComponent::GetFloatAlpha() const
{
	if (!bFloating || FloatRecoverDuration <= 0.0f)
	{
		return bFloating ? 0.0f : 1.0f;
	}

	return FMath::Clamp(FloatTime / FloatRecoverDuration, 0.0f, 1.0f);
}

// Turns a jump the dodge already suppressed into an upward launch.
bool UVRDodgeOffsetComponent::NotifyJumpReleased()
{
	const AActor* owner = GetOwner();

	if (!owner)
	{
		return false;
	}

	if (!IsAerialWindowOpen())
	{
		ReportDebug(
			FString::Printf(TEXT("AERIAL miss  phase=%s  t=%.2f  tail=%.2f"),
				Phase == EVRDodgePhase::Dash     ? TEXT("Dash")
				: Phase == EVRDodgePhase::Cooldown ? TEXT("Cooldown")
				: TEXT("Idle"),
				PhaseTime, AerialWindowTailSeconds),
			7,
			FColor::Orange);

		return false;
	}

	const FVector up = owner->GetActorQuat().GetAxisZ();

	// Started only when the player was not already floating. A chained launch
	// deliberately leaves the clock where it is, so it fights whatever gravity
	// has already returned -- that decay is the only thing ending the chain.
	if (!bFloating)
	{
		bFloating        = true;
		FloatTime        = 0.0f;
		FloatLaunchCount = 0;
	}

	++FloatLaunchCount;

	OnAerialLaunch.Broadcast(up * AerialLaunchSpeed);

	ReportDebug(
		FString::Printf(TEXT("AERIAL launch #%d  speed=%.0f  floatAlpha=%.2f"),
			FloatLaunchCount, AerialLaunchSpeed, GetFloatAlpha()),
		7,
		FColor::Green);

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRDodge, Log,
			TEXT("Aerial launch #%d at %.0f cm/s. Float alpha %.2f, %.2fs of recovery left."),
			FloatLaunchCount, AerialLaunchSpeed, GetFloatAlpha(),
			FMath::Max(FloatRecoverDuration - FloatTime, 0.0f));
	}

	return true;
}

void UVRDodgeOffsetComponent::NotifyLanded()
{
	if (!bFloating)
	{
		return;
	}

	EndFloat();

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRDodge, Log, TEXT("Landed. Float ended and rearmed."));
	}
}

void UVRDodgeOffsetComponent::EndFloat()
{
	bFloating = false;
	FloatTime = 0.0f;

	const AActor* owner = GetOwner();
	const FVector up    = owner ? owner->GetActorQuat().GetAxisZ() : FVector::UpVector;

	// A final zero so Blueprint stops applying the assist. Without it the last
	// non-zero fraction would be the one left standing.
	OnFloatAssistChanged.Broadcast(up, 0.0f);

	ReportDebug(TEXT("  float ended"), 8, FColor::Silver);
}

// Per-tick: decays the upward assist so gravity appears to ease back in.
void UVRDodgeOffsetComponent::UpdateFloat(float delta_time)
{
	if (!bFloating)
	{
		return;
	}

	FloatTime += delta_time;

	if (FloatTime >= FloatRecoverDuration)
	{
		EndFloat();
		return;
	}

	const float alpha = GetFloatAlpha();

	// Runs 1 down to 0, the opposite of DashCurve, because this multiplies an
	// assist that is fading rather than a displacement that is accumulating.
	const float falloff = FloatCurve
		? FloatCurve->GetFloatValue(alpha)
		: 1.0f - alpha;

	// Clamped below 1 as well as at the property. A curve is free to return
	// anything, and a curve that overshot past 1 here would cancel gravity
	// outright and leave the player climbing indefinitely.
	const float fraction = FMath::Clamp(FloatAssistFraction * falloff, 0.0f, 0.95f);

	const AActor* owner = GetOwner();
	const FVector up    = owner ? owner->GetActorQuat().GetAxisZ() : FVector::UpVector;

	OnFloatAssistChanged.Broadcast(up, fraction);

	ReportDebug(
		FString::Printf(TEXT("  float %.2f of gravity  %.2fs left  launches=%d"),
			fraction, FMath::Max(FloatRecoverDuration - FloatTime, 0.0f), FloatLaunchCount),
		8,
		FColor::Cyan);
}

// ── Assist ──

// Per-tick: re-aims the dash at where the threat is now, inside the player's cone.
void UVRDodgeOffsetComponent::UpdateDirectionWithAssist()
{
	if (!bAssistActive || Phase != EVRDodgePhase::Dash)
	{
		return;
	}

	if (MaxAssistAngleDegrees < KINDA_SMALL_NUMBER || MaxTrackingStepDegrees < KINDA_SMALL_NUMBER)
	{
		return;
	}

	const AActor* owner = GetOwner();

	if (!owner)
	{
		return;
	}

	// Re-measured rather than reused. A fast attack has moved far enough
	// within a few frames that the geometry taken at dodge start no longer
	// describes where it is.
	const FVRThreatGeometry live = ResolveThreatGeometry();

	if (!live.bValid)
	{
		return;
	}

	const FQuat   actor_frame = owner->GetActorQuat();
	const FVector actor_up    = actor_frame.GetAxisZ();

	const FVector opening = LastStickWorldDir.GetSafeNormal();

	if (opening.IsNearlyZero())
	{
		return;
	}

	// Both angles are measured from the direction the player pressed, never
	// from the live one. Accumulating tick to tick would let a long dash creep
	// out of the cone one small correction at a time.
	const float current = VRDodgeAssist::SignedAngleDeg(opening,
		WorldDirection.GetSafeNormal(), actor_up);

	const float wanted = ComputeAssistTurnDeg(live, opening, actor_up,
		FMath::Max(TargetDistance - DashCommandedDistance, 0.0f));

	// Decays across the dash, so the dodge is most willing to re-aim in its
	// opening frames and has effectively committed by the end.
	const float allowance = MaxTrackingStepDegrees * (1.0f - ComputeDashAlpha());

	const float step = FMath::Clamp(wanted - current, -allowance, allowance);

	const float total = FMath::Clamp(current + step,
		-MaxAssistAngleDegrees, MaxAssistAngleDegrees);

	FVector turned = opening.RotateAngleAxis(total, actor_up);

	if (bProjectToTangentPlane)
	{
		turned = FVector::VectorPlaneProject(turned, actor_up);
	}

	if (turned.Normalize())
	{
		WorldDirection = turned;
	}
}

// Signed degrees to turn the player's direction by, never leaving the cone around it.
float UVRDodgeOffsetComponent::ComputeAssistTurnDeg(const FVRThreatGeometry& geometry,
                                                     const FVector& opening,
                                                     const FVector& actor_up,
                                                     float dodge_distance) const
{
	const AActor* owner = GetOwner();

	if (!owner || !geometry.bValid || MaxAssistAngleDegrees < KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const FVector from = FVector::VectorPlaneProject(opening, actor_up).GetSafeNormal();

	if (from.IsNearlyZero())
	{
		return 0.0f;
	}

	const FVector player_now = owner->GetActorLocation();

	// Separation the two volumes need before they stop touching.
	const float needed = geometry.ThreatRadius + GetHurtboxRadius() + ClearanceMargin;

	// Closest the player ever gets to the attack if the dash runs at turn_deg.
	//
	// Both of them move. Scoring only the endpoint said a dash that goes
	// clean through a ball and stops just past it was a perfect dodge, which
	// is exactly what happens when you strafe into one passing beside you.
	// The endpoint was clear; the path went through the attack.
	auto closest_approach = [&](float turn_deg)
	{
		const FVector dir = from.RotateAngleAxis(turn_deg, actor_up);

		float nearest = TNumericLimits<float>::Max();

		for (int32 step = 0; step <= VRDodgeAssist::ApproachSamples; ++step)
		{
			const float fraction = static_cast<float>(step) / VRDodgeAssist::ApproachSamples;

			// Run past the end of the dash, so an attack that arrives just
			// after the player stops still counts against the score.
			const float time = fraction * DashDuration * VRDodgeAssist::ApproachHorizon;

			const float raw = DashDuration > 0.0f
				? FMath::Clamp(time / DashDuration, 0.0f, 1.0f)
				: 1.0f;

			const float alpha = DashCurve
				? DashCurve->GetFloatValue(raw)
				: FMath::InterpEaseOut(0.0f, 1.0f, raw, 2.0f);

			const FVector player_at = player_now + dir * dodge_distance * alpha;
			const FVector threat_at = geometry.HurtboxLocation
				+ geometry.AttackDir * geometry.TangentSpeed * time;

			nearest = FMath::Min(nearest,
				FVector::VectorPlaneProject(player_at - threat_at, actor_up).Size());
		}

		return nearest;
	};

	// The player's own aim gets first refusal. Turning someone who was never
	// going to be hit is the assist overruling a decision that was not wrong,
	// and that is what sent a rightward press forward when standing a little
	// left of an oncoming ball.
	if (closest_approach(0.0f) >= needed)
	{
		return 0.0f;
	}

	// Closest approach is not a clean cosine the way the old endpoint measure
	// was, so there is no two-candidate shortcut. Sweeping the cone is a few
	// hundred distance checks and is robust to whatever shape the score takes.
	float best_turn  = 0.0f;
	float best_score = closest_approach(0.0f);

	for (float turn = -MaxAssistAngleDegrees; turn <= MaxAssistAngleDegrees;
	     turn += VRDodgeAssist::SweepStepDegrees)
	{
		const float score = closest_approach(turn);

		if (score > best_score)
		{
			best_score = score;
			best_turn  = turn;
		}
	}

	return best_turn;
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

// ── Displacement diagnostics ──

void UVRDodgeOffsetComponent::ReportDebug(const FString& message, int32 slot, FColor color) const
{
	if (!bShowDebugOnScreen || !GEngine)
	{
		return;
	}

	// Long enough to pull the headset off and still read all three lines.
	constexpr float Lifetime = 20.0f;

	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(DebugMessageKey + slot), Lifetime, color, message);
}

FVector UVRDodgeOffsetComponent::GetViewLocation() const
{
	const AActor* owner = GetOwner();

	if (const APawn* pawn = Cast<APawn>(owner))
	{
		if (AController* controller = pawn->GetController())
		{
			FVector location;
			FRotator rotation;
			controller->GetPlayerViewPoint(location, rotation);

			return location;
		}
	}

	// No controller means no viewpoint to compare against. Returning the actor
	// location makes the view displacement match the actor's rather than
	// reading as a spurious failure.
	return owner ? owner->GetActorLocation() : FVector::ZeroVector;
}

void UVRDodgeOffsetComponent::ReportDashResult() const
{
	if (!bShowDebugOnScreen)
	{
		return;
	}

	ReportDebug(
		FString::Printf(TEXT("DODGE #%d  req=%.1f  applied=%.1f  short=%.1f"),
			DodgeCount, TargetDistance, DashEndActorDisplacement,
			FMath::Max(TargetDistance - DashEndActorDisplacement, 0.0f)),
		0,
		DashEndActorDisplacement >= TargetDistance * 0.9f ? FColor::Green : FColor::Orange);

	// The line that separates "the dodge did not happen" from "the dodge
	// happened and the headset did not see it". Head motion also moves the
	// view, but a few centimetres of that against a multi-metre dodge is noise.
	const bool view_followed = DashEndViewDisplacement >= DashEndActorDisplacement * 0.5f;

	ReportDebug(
		FString::Printf(TEXT("  actor %.1f   view %.1f%s"),
			DashEndActorDisplacement, DashEndViewDisplacement,
			view_followed ? TEXT("") : TEXT("   <- VIEW NOT FOLLOWING")),
		1,
		view_followed ? FColor::Green : FColor::Red);

	// Separates "the assist stayed out because nothing was closing" from "the
	// assist ran and chose the wrong side" from "no threat resolved at all".
	// All three look identical from inside the headset.
	if (!LastThreatGeometry.bValid)
	{
		ReportDebug(TEXT("  threat NONE RESOLVED"), 3, FColor::Yellow);
		return;
	}

	// perp is cross(attack, up), which points left of the attack's heading, so
	// a positive side means the player stands to the attack's left.
	const TCHAR* side_label =
		FMath::IsNearlyZero(LastThreatGeometry.SideSign) ? TEXT("on-line")
		: (LastThreatGeometry.SideSign > 0.0f ? TEXT("L") : TEXT("R"));

	ReportDebug(
		FString::Printf(TEXT("  threat %s  r=%.0f  miss=%.0f  closing=%.2f  side=%s  assist=%s"),
			*LastThreatGeometry.ThreatName,
			LastThreatGeometry.ThreatRadius,
			LastThreatGeometry.MissDistance,
			LastThreatGeometry.ClosingDot,
			side_label,
			bAssistActive ? TEXT("ON") : TEXT("off")),
		3,
		bAssistActive ? FColor::Green : FColor::Yellow);

	// On a keyboard the stick is a constant forward press, so the useful
	// reading is not what it says but whether final broke away from it.
	const bool took_escape =
		FVector::DotProduct(DebugFinalLocalDir, DebugStickLocalDir) < 0.99f;

	ReportDebug(
		FString::Printf(TEXT("  stick (%.2f,%.2f)  escape (%.2f,%.2f)  final (%.2f,%.2f)"),
			DebugStickLocalDir.X,  DebugStickLocalDir.Y,
			DebugEscapeLocalDir.X, DebugEscapeLocalDir.Y,
			DebugFinalLocalDir.X,  DebugFinalLocalDir.Y),
		4,
		took_escape ? FColor::Green : FColor::Orange);
}

void UVRDodgeOffsetComponent::ReportSettleResult() const
{
	if (!bShowDebugOnScreen)
	{
		return;
	}

	const AActor* owner = GetOwner();

	const float net = owner
		? FVector::Dist(owner->GetActorLocation(), DashStartActorLocation)
		: 0.0f;

	const float reverted = FMath::Max(DashEndActorDisplacement - net, 0.0f);

	// A large revert means the dodge worked and something moved the pawn back
	// afterwards, which is a different bug from the dodge falling short.
	const bool held = reverted <= DashEndActorDisplacement * 0.25f;

	ReportDebug(
		FString::Printf(TEXT("  settled +%.2fs: net %.1f  (reverted %.1f)"),
			SettleCheckDelay, net, reverted),
		2,
		held ? FColor::Green : FColor::Red);
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
