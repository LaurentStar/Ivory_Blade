#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VRDodgeOffsetComponent.generated.h"

class UCurveFloat;
class UVRThreatSourceComponent;
class UVRAttackTrajectoryComponent;
struct FHitResult;

DECLARE_LOG_CATEGORY_EXTERN(LogVRDodge, Log, All);

/**
 * Primary phases of a dodge. The Dash phase permanently displaces the capsule
 * horizontally, then Cooldown prevents re-dodging too quickly. Vertical duck
 * runs on a separate phase machine in parallel.
 */
UENUM(BlueprintType)
enum class EVRDodgePhase : uint8
{
	Idle,
	Dash,
	Cooldown
};

/** Vertical duck phase. Runs in parallel with the primary phase when active. */
UENUM(BlueprintType)
enum class EVRDuckPhase : uint8
{
	Idle,
	Down,
	Return
};

/**
 * Displaces the owning actor along the player's stick direction with a light
 * assist that nudges perpendicular to the incoming attack. Never opposes the
 * player's input.
 *
 * Horizontal displacement (tangent plane) is **permanent** -- a dash to a new
 * position. Vertical displacement (duck along actor Z) is temporary and returns
 * to standing height.
 *
 * Dodge only fires when the player is inside an alert box overlap AND holds
 * jump + flicks the stick. Without an alert box, the input is ignored.
 *
 * Directions are given in **actor space**, where the actor's own +Z is its up
 * vector. Projecting a dodge onto the surface tangent plane is therefore just
 * zeroing local Z, so the planetoid case is correct by construction and no
 * world Z is referenced anywhere.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class IVORY_BLADE_API UVRDodgeOffsetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVRDodgeOffsetComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// ── Requests ──

	/** Starts a dodge with an explicit distance. */
	UFUNCTION(BlueprintCallable, Category = "Dodge|Offset")
	bool RequestOffsetDodgeExact(FVector local_direction, float distance_cm);

	/**
	 * Starts a dodge from a raw left-stick vector, X right and Y forward.
	 *
	 * The stick is interpreted relative to **gaze yaw**, not capsule facing. In VR
	 * those differ constantly, since the body faces one way while the head looks
	 * another, and a player who flicks left means left of what she is looking at.
	 * Gaze is flattened onto the actor's tangent plane first, so this stays correct
	 * on a planetoid.
	 *
	 * Deflections shorter than StickDeadzone are rejected.
	 * Dodge is rejected if the player is not inside an alert zone.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dodge|Offset")
	bool RequestOffsetDodgeFromStick(FVector2D stick);

	/** Cancels any dodge in progress, snapping the duck back to standing immediately. */
	UFUNCTION(BlueprintCallable, Category = "Dodge|Offset")
	void CancelDodge();

	// ── Queries ──

	UFUNCTION(BlueprintPure, Category = "Dodge|Offset")
	EVRDodgePhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Dodge|Offset")
	EVRDuckPhase GetDuckPhase() const { return DuckPhase; }

	/** True during Dash. False during Cooldown and Idle. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset")
	bool IsDodging() const;

	/** True when the duck is in Down or Return. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset")
	bool IsDucking() const;

	/** True whenever a new request would be rejected: during Dash, Cooldown, or while still ducking. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset")
	bool IsBusy() const { return Phase != EVRDodgePhase::Idle || DuckPhase != EVRDuckPhase::Idle; }

	/**
	 * Displacement actually achieved, in cm. Falls short of the request when a
	 * swept move was blocked by geometry.
	 */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset")
	float GetAppliedDistance() const { return AppliedDistance; }

	/** Displacement the current dodge is aiming for, in cm. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset")
	float GetTargetDistance() const { return TargetDistance; }

	/** True if the player's capsule currently overlaps any alert-box tagged component. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset")
	bool IsInAlertZone() const;

	/** Radius of the component tagged HurtboxTag, or FallbackHurtboxRadius if none was found. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset")
	float GetHurtboxRadius() const;

	/**
	 * Returns the UVRAttackTrajectoryComponent from the closest overlapping
	 * alert box's owner. Null when no alert box overlaps the player.
	 */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset")
	UVRAttackTrajectoryComponent* GetOverlappingTrajectory() const;

	/**
	 * Returns the UVRThreatSourceComponent from the closest overlapping
	 * alert box's owner. Null when no alert box overlaps the player.
	 * Used by the dodge system to read per-attack DodgeDistance.
	 */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset")
	UVRThreatSourceComponent* GetOverlappingThreatSource() const;

	// ── Distance ──

	/** Hard cap on displacement, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Distance",
	          meta = (ClampMin = "0.0"))
	float MaxOffsetDistance = 60.0f;

	/**
	 * Default dodge distance when no attack-specific distance is provided.
	 * The alert-box system will supply per-attack distances in the future;
	 * this is the fallback.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Distance",
	          meta = (ClampMin = "0.0"))
	float DefaultDodgeDistance = 40.0f;

	/** Component tag identifying the pawn's hurtbox. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Distance")
	FName HurtboxTag = TEXT("Hurtbox");

	/** Used when no component carries HurtboxTag. Warns once when it has to be used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Distance",
	          meta = (ClampMin = "0.0"))
	float FallbackHurtboxRadius = 20.0f;

	// ── Timeline ──

	/** Time for the horizontal dash to reach full displacement. An ease-out curve shapes this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Timeline",
	          meta = (ClampMin = "0.0"))
	float DashDuration = 0.15f;

	/** Time for the vertical duck to reach full depth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Timeline",
	          meta = (ClampMin = "0.0"))
	float DuckDownDuration = 0.08f;

	/** Time for the duck to return to standing height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Timeline",
	          meta = (ClampMin = "0.0"))
	float DuckReturnDuration = 0.15f;

	/** Dead time after the dash before another dodge is accepted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Timeline",
	          meta = (ClampMin = "0.0"))
	float CooldownDuration = 0.35f;

	/** Shapes the horizontal dash over 0..1. Ease-out is used when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Timeline")
	TObjectPtr<UCurveFloat> DashCurve;

	/** Shapes the duck return over 0..1. Ease in-out is used when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Timeline")
	TObjectPtr<UCurveFloat> DuckReturnCurve;

	// ── Behaviour ──

	/**
	 * Sweeps the move against geometry so a dodge into a wall is reduced rather
	 * than penetrating. Leave on; the shortfall is reported by GetAppliedDistance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Behaviour")
	bool bSweep = true;

	/**
	 * Removes any component of the requested direction along the actor's own up
	 * axis, keeping the dodge tangent to the surface. Gravity-agnostic because it
	 * works in actor space rather than world space.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Behaviour")
	bool bProjectToTangentPlane = true;

	/** Minimum stick deflection RequestOffsetDodgeFromStick will act on. A flick, not a nudge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Behaviour",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StickDeadzone = 0.5f;

	// ── Assist ──

	/**
	 * Base assist strength at dodge start (0 = no assist, 1 = full assist).
	 * Decays to zero over the dash duration: effective = AssistWeight * (1 - DashAlpha).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Assist",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AssistWeight = 0.3f;

	/**
	 * Dot product threshold between the player's stick direction and the attack's
	 * travel direction. Above this, the player is considered to be dodging INTO
	 * the attack and the duck phase + lateral nudge activate.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Assist",
	          meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float DodgeIntoThreshold = 0.5f;

	// ── Scoreboard (test rig instrumentation) ──

	/** Call from the hurtbox overlap. Logs the phase and applied distance at the moment of the hit. */
	UFUNCTION(BlueprintCallable, Category = "Dodge|Scoreboard")
	void ReportHit(AActor* threat);

	/** Call when a threat expires without connecting. */
	UFUNCTION(BlueprintCallable, Category = "Dodge|Scoreboard")
	void ReportEvaded(AActor* threat);

	/** Prints the running tally to the log. */
	UFUNCTION(BlueprintCallable, Category = "Dodge|Scoreboard")
	void LogScoreboard() const;

	UFUNCTION(BlueprintCallable, Category = "Dodge|Scoreboard")
	void ResetScoreboard();

	// ── I-Frames ──

	/**
	 * Base invulnerability duration in seconds. Matches KIU's 18 frames at 60fps.
	 * Fatigue will scale this down when implemented.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|IFrames",
	          meta = (ClampMin = "0.0"))
	float BaseIFrameDuration = 0.3f;

	/**
	 * True while the player is invulnerable from a dodge. Blueprint should check
	 * this in the hurtbox overlap handler and skip damage when true.
	 */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset|IFrames")
	bool IsInvulnerable() const { return bInvulnerable; }

	/** Remaining i-frame time. Exposed for UI/debug. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset|IFrames")
	float GetIFrameTimeRemaining() const { return IFrameTimer; }

	// ── Fatigue ──

	/**
	 * Maximum fatigue before the player enters the exhausted state and cannot
	 * dodge. Each dodge increments fatigue by 1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Fatigue",
	          meta = (ClampMin = "1"))
	int32 MaxFatigue = 5;

	/** Seconds after the last dodge before fatigue starts decaying. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Fatigue",
	          meta = (ClampMin = "0.0"))
	float FatigueDecayDelay = 1.5f;

	/** Fatigue units removed per second once decay begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Fatigue",
	          meta = (ClampMin = "0.0"))
	float FatigueDecayRate = 2.0f;

	/** Seconds the player is locked out of dodging at max fatigue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Fatigue",
	          meta = (ClampMin = "0.0"))
	float ExhaustedDuration = 1.0f;

	/**
	 * At max fatigue i-frame duration shrinks to this fraction of BaseIFrameDuration.
	 * 0.3 = 30% of base at full fatigue.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Fatigue",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinIFrameScale = 0.3f;

	/**
	 * At max fatigue dodge distance shrinks to this fraction of the requested distance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Fatigue",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinDistanceScale = 0.5f;

	/**
	 * At max fatigue cooldown grows by this multiplier.
	 * 2.0 = cooldown doubles at full fatigue.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Fatigue",
	          meta = (ClampMin = "1.0"))
	float MaxCooldownScale = 2.0f;

	/** Current fatigue ratio (0 = fresh, 1 = exhausted). For UI display. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset|Fatigue")
	float GetFatigueRatio() const;

	/** True when the player is in the exhausted lockout. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset|Fatigue")
	bool IsExhausted() const { return bExhausted; }

	// ── Diagnostics ──

	/** Master switch for LogVRDodge output. Only prints while a dodge is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Diagnostics")
	bool bLogDiagnostics = true;

private:
	float GetPhaseDuration(EVRDodgePhase phase) const;
	float GetDuckPhaseDuration(EVRDuckPhase phase) const;

	bool BeginDodge(FVector local_direction, float distance_cm);
	void ApplyHorizontalOffset(float desired_distance);
	void ApplyVerticalOffset(float desired_depth);
	void FinishDash();
	void FinishDuck();
	void ReturnToIdle();

	float ComputeDashAlpha() const;
	float ComputeDuckAlpha() const;
	float EvaluateCurve(const UCurveFloat* curve, float alpha) const;

	void LogHeartbeat() const;

	void LogBlockedMove(const FVector& requested, const FVector& achieved,
	                    const FVector& world_direction, const FVector& actor_up,
	                    const FHitResult& hit) const;

	/**
	 * Recomputes WorldDirection from the current stick input and attack trajectory.
	 * Called each tick during Dash. The assist influence decays with DashAlpha.
	 */
	void UpdateDirectionWithAssist();

	/**
	 * Returns the perpendicular nudge vector that clears the player from the
	 * attack path without opposing the stick direction. The returned vector
	 * lies in the tangent plane.
	 */
	FVector ComputeClearVector(const FVector& stick_world, const FVector& attack_dir,
	                           const FVector& actor_up) const;

	EVRDodgePhase Phase = EVRDodgePhase::Idle;
	float PhaseTime = 0.0f;

	EVRDuckPhase DuckPhase = EVRDuckPhase::Idle;
	float DuckPhaseTime = 0.0f;

	FVector WorldDirection = FVector::ZeroVector;
	float TargetDistance = 0.0f;

	FVector AppliedWorldOffset = FVector::ZeroVector;
	float AppliedDistance = 0.0f;

	FVector AppliedDuckOffset = FVector::ZeroVector;
	float DuckTargetDepth = 0.0f;

	float LastDesiredDistance = 0.0f;

	mutable float CachedHurtboxRadius = -1.0f;
	mutable bool bWarnedNoHurtbox = false;

	/** Last valid stick direction in world space. Held when stick returns to deadzone mid-dodge. */
	FVector LastStickWorldDir = FVector::ZeroVector;

	/** True when a trajectory was active at dodge start and assist should run. */
	bool bAssistActive = false;

	// ── I-Frame state ──
	bool bInvulnerable = false;
	float IFrameTimer = 0.0f;

	// ── Fatigue state ──
	float Fatigue = 0.0f;
	float TimeSinceLastDodge = 0.0f;
	bool bExhausted = false;
	float ExhaustedTimer = 0.0f;

	/** Returns fatigue-scaled i-frame duration. */
	float GetEffectiveIFrameDuration() const;

	/** Returns fatigue-scaled dodge distance. */
	float GetEffectiveDodgeDistance(float base_distance) const;

	/** Returns fatigue-scaled cooldown duration. */
	float GetEffectiveCooldownDuration() const;

	// ── Scoreboard ──
	int32 HitCount = 0;
	int32 EvadeCount = 0;
};
