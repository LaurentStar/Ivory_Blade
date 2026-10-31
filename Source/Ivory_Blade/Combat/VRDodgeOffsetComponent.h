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
 * Broadcast once when a dodge converts into an aerial launch.
 *
 * Carries a world-space velocity in cm/s rather than applying it, because the
 * gravity toolkit owns movement mode and velocity. Anything this component did
 * to the character directly would be fighting it.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVRAerialLaunch, FVector, ImpulseWorld);

/**
 * Broadcast every frame of the float with the upward assist to apply, then
 * once more with a zero fraction when the float ends.
 *
 * Split into a direction and a fraction of gravity on purpose. This component
 * cannot know the gravity magnitude -- the toolkit owns it, and on a planetoid
 * it is not world Z -- so handing back a fraction lets the Blueprint scale it
 * by the number it already has. The fraction is clamped below 1, which makes
 * full cancellation unreachable whatever that number turns out to be. The
 * character floats; it cannot be made to fly by mistuning.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVRFloatAssistChanged, FVector, UpDirection, float, GravityFraction);

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
 * Where the threatening attack is, how big it is, and where it is heading.
 *
 * The assist originally worked from the attack's travel direction alone, which
 * is enough to build a perpendicular but not enough to know whether the attack
 * is a threat at all, which side of it the player is standing on, or how far
 * they must travel to clear it. Those three answers all come from the attack's
 * *position*, so it is resolved once at dodge start and reused.
 *
 * Positions are world space. Directions are projected onto the actor's tangent
 * plane, so every field is valid under planetoid gravity.
 */
USTRUCT(BlueprintType)
struct FVRThreatGeometry
{
	GENERATED_BODY()

	/** False when no threat overlapped, or when its geometry was degenerate. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge|Threat")
	bool bValid = false;

	/** Where the damage volume actually is right now. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge|Threat")
	FVector HurtboxLocation = FVector::ZeroVector;

	/** Normalized travel direction, flattened onto the tangent plane. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge|Threat")
	FVector AttackDir = FVector::ZeroVector;

	/**
	 * Tangent-plane speed in cm/s.
	 *
	 * Lets the assist work out where the attack will be partway through the
	 * dash, which is the difference between a ball that is about to arrive and
	 * one still far down the same line.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge|Threat")
	float TangentSpeed = 0.0f;

	/** Collision radius of the damage volume, in cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge|Threat")
	float ThreatRadius = 0.0f;

	/** +1 when driving straight at the player, 0 across them, negative when heading away. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge|Threat")
	float ClosingDot = 0.0f;

	/** How far along its own heading the attack still has to travel to draw level. Negative once past. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge|Threat")
	float AlongDistance = 0.0f;

	/** Perpendicular gap between the player and the attack's line, in cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge|Threat")
	float MissDistance = 0.0f;

	/** Which side of that line the player is on. Zero when dead on it. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge|Threat")
	float SideSign = 0.0f;

	/**
	 * Unit direction out of the attack's path.
	 *
	 * The offset from the hurtbox with the along-travel part removed, which is
	 * one vector covering both of the things a dodge has to do. Stand beside a
	 * passing attack and almost nothing is removed, so it points straight away
	 * from the hurtbox. Stand in front of an oncoming one and almost
	 * everything is, so what is left is the perpendicular out of its line.
	 *
	 * Zero when the attack is dead head-on, where the two sides are equally
	 * good and the caller has to break the tie.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge|Threat")
	FVector EscapeDirection = FVector::ZeroVector;

	/** Threat actor's name, for diagnostics only. */
	UPROPERTY(BlueprintReadOnly, Category = "Dodge|Threat")
	FString ThreatName;
};

/**
 * Displaces the owning actor out of an attack's path. The player's aim sets the
 * dash going; the threat turns it away from the attack by at most
 * MaxAssistAngleDegrees, and the dash then runs straight along that heading.
 * The player owns the cone, the threat picks the heading inside it.
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

	/**
	 * Call from IA_Jump's Completed pin, alongside the jump component's own
	 * handler. Converts the jump that a dodge already suppressed into an
	 * aerial launch, when the release lands inside the window.
	 *
	 * Returns true only when a launch actually fired, so Blueprint can branch
	 * on it. Returning false is the ordinary case and means nothing happened:
	 * the jump stays suppressed exactly as it did before this existed.
	 *
	 * Outside advance mode a press jumps for real rather than deferring, so
	 * gate this on UVRJumpInputComponent::IsJumpDeferred() or a launch will
	 * stack on top of that jump and read as a double jump.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dodge|Aerial")
	bool NotifyJumpReleased();

	/**
	 * Call when the character touches down. Ends the float immediately and
	 * rearms it for the next time the player leaves the ground.
	 *
	 * Blueprint has to supply this because the gravity toolkit owns landing;
	 * the character is in a flying movement mode throughout, so none of the
	 * engine's own landing signals fire.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dodge|Aerial")
	void NotifyLanded();

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

	// ── Aerial ──
	//
	// Releasing jump while a dodge runs turns the suppressed jump into an
	// upward launch, after which an upward assist force decays away so that
	// gravity appears to ease back in.
	//
	// The recovery clock is deliberately monotonic. A chained midair dodge
	// adds another impulse but never rewinds it, so each launch in a chain
	// fights more of the returning gravity than the one before and the chain
	// dies out on its own. That is the whole reason there is no air-dodge
	// counter here. Footstooling will be the one thing allowed to rewind the
	// clock, and it is not built yet.

	/**
	 * How long past the end of the dash the release still counts, in seconds.
	 *
	 * The window is the dash itself plus this tail, read straight off the
	 * phase machine, so retuning DashDuration moves the window with it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Aerial",
	          meta = (ClampMin = "0.0"))
	float AerialWindowTailSeconds = 0.10f;

	/** Upward speed in cm/s handed to OnAerialLaunch. Every launch in a chain uses the same value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Aerial",
	          meta = (ClampMin = "0.0"))
	float AerialLaunchSpeed = 400.0f;

	/**
	 * Fraction of gravity countered at the start of the float, decaying to
	 * zero across FloatRecoverDuration.
	 *
	 * Hard-clamped below 1. At 1 the assist cancels gravity outright and the
	 * player hovers or climbs forever, which is flight rather than float, so
	 * the clamp puts that state out of reach instead of merely off by default.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Aerial",
	          meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float FloatAssistFraction = 0.7f;

	/**
	 * Seconds for the assist to decay from FloatAssistFraction to zero.
	 *
	 * The real dial for how long a chain can last, since nothing rewinds the
	 * clock once it starts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Aerial",
	          meta = (ClampMin = "0.0"))
	float FloatRecoverDuration = 1.2f;

	/**
	 * Shapes the decay over 0..1. Linear falloff is used when unset.
	 *
	 * Same convention as DashCurve -- X is normalized recovery time -- but Y
	 * runs the other way, from 1 down to 0, because it is a multiplier on an
	 * assist that is fading rather than a displacement that is accumulating.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Aerial")
	TObjectPtr<UCurveFloat> FloatCurve;

	/** Fires once per launch with the world-space velocity to add. */
	UPROPERTY(BlueprintAssignable, Category = "Dodge|Aerial")
	FVRAerialLaunch OnAerialLaunch;

	/** Fires every frame of the float, and once with zero when it ends. */
	UPROPERTY(BlueprintAssignable, Category = "Dodge|Aerial")
	FVRFloatAssistChanged OnFloatAssistChanged;

	/** True between a launch and the end of the float. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Aerial")
	bool IsFloating() const { return bFloating; }

	/** How far through the recovery the float is, 0 at launch to 1 at normal gravity. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Aerial")
	float GetFloatAlpha() const;

	/** True while a release would convert into a launch. For UI and debug. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Aerial")
	bool IsAerialWindowOpen() const;

	// ── Assist ──

	/**
	 * Dot product threshold between the player's stick direction and the attack's
	 * travel direction. Above this, the player is considered to be dodging INTO
	 * the attack and the duck phase + lateral nudge activate.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Assist",
	          meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float DodgeIntoThreshold = 0.5f;

	/**
	 * Furthest the assist may turn a dodge away from the player's own aim.
	 *
	 * A hard cone around the stick direction, enforced against the direction
	 * they originally pressed rather than against the live one, so tracking
	 * cannot accumulate its way out of it over a long dash.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Assist",
	          meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxAssistAngleDegrees = 70.0f;

	/**
	 * Furthest a single tracking tick may turn the dash, before decay.
	 *
	 * Governs how violently the dodge re-aims when the threat moves under it.
	 * The running total is still held inside MaxAssistAngleDegrees, so this
	 * sets the rate of the correction and not its reach.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Assist",
	          meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxTrackingStepDegrees = 45.0f;

	/**
	 * Component tag marking the damage volume on the *threat*.
	 *
	 * Distinct from HurtboxTag, which is searched on the pawn. The two default
	 * to the same name because the project already tags both that way, but they
	 * answer different questions and conflating them is a trap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Assist")
	FName ThreatHurtboxTag = TEXT("Hurtbox");

	/**
	 * Extend a short dodge until it actually clears the attack.
	 *
	 * Off by default. The authored DodgeDistance stays the floor; when this is
	 * on, a dodge that geometry says would still be inside the hurtbox is
	 * lengthened to escape it. Changes dodge feel, so it is opt-in.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Assist")
	bool bEnsureClearance = false;

	/** Extra cm beyond the touching distance when bEnsureClearance lengthens a dodge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Assist",
	          meta = (ClampMin = "0.0", EditCondition = "bEnsureClearance"))
	float ClearanceMargin = 10.0f;

	/** Threat geometry resolved at the start of the most recent dodge. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset|Assist")
	const FVRThreatGeometry& GetLastThreatGeometry() const { return LastThreatGeometry; }

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

	/**
	 * Print a three-line displacement report after every dodge.
	 *
	 * Built for the case where a dodge reads normally on a desktop build and
	 * small in the headset. It reports how far the capsule moved, how far the
	 * player's viewpoint moved over the same window, and whether the capsule
	 * was still there a moment later. Those three separate a blocked dodge
	 * from a view that is not following the capsule from a pawn that is being
	 * put back, which all look identical from inside the headset.
	 *
	 * On-screen rather than UE_LOG because device testing is an untethered APK.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Diagnostics")
	bool bShowDebugOnScreen = false;

	/** Base key for the debug messages. Offset from the input components' blocks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Diagnostics")
	int32 DebugMessageKey = 7400;

	/**
	 * Seconds after the dash ends before net displacement is measured again.
	 *
	 * Anything that reverts the pawn does so on a later tick, so the check has
	 * to happen after the dash rather than at the end of it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge|Offset|Diagnostics",
	          meta = (ClampMin = "0.0"))
	float SettleCheckDelay = 0.5f;

	/** Displacement the capsule achieved during the last dash, in cm. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset|Diagnostics")
	float GetLastDashActorDisplacement() const { return DashEndActorDisplacement; }

	/** Displacement the player's viewpoint achieved over the same window, in cm. */
	UFUNCTION(BlueprintPure, Category = "Dodge|Offset|Diagnostics")
	float GetLastDashViewDisplacement() const { return DashEndViewDisplacement; }

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

	/**
	 * Advances the float and broadcasts this frame's assist.
	 *
	 * Ticked above the phase machines' early-out, because the float outlives
	 * the dodge that started it. By the time gravity has eased back in the
	 * phase machine has usually been Idle for a second or more.
	 */
	void UpdateFloat(float delta_time);

	/** Ends the float and broadcasts a zero assist so Blueprint stops applying it. */
	void EndFloat();

	void LogHeartbeat() const;

	void LogBlockedMove(const FVector& requested, const FVector& achieved,
	                    const FVector& world_direction, const FVector& actor_up,
	                    const FHitResult& hit) const;

	/**
	 * Signed degrees to turn the player's direction by so the dash finishes
	 * clear of the attack's line, clamped to MaxAssistAngleDegrees.
	 *
	 * Zero whenever the direction they picked was already going to clear, so
	 * an aim that needed no help never gets overruled. Otherwise it is the
	 * angle inside the cone that ends furthest from the line, which keeps the
	 * answer recognisably the direction they asked for.
	 */
	float ComputeAssistTurnDeg(const FVRThreatGeometry& geometry, const FVector& opening,
	                           const FVector& actor_up, float dodge_distance) const;

	/**
	 * Re-aims the dash at the threat's current position. Called each tick
	 * during Dash.
	 *
	 * The turn is always measured from LastStickWorldDir rather than from the
	 * live direction, so corrections cannot accumulate their way out of the
	 * cone however many ticks run.
	 */
	void UpdateDirectionWithAssist();

	/**
	 * Measures the closest overlapping threat: where its damage volume is, how
	 * big it is, which way it is going, and how the player sits relative to its
	 * line of travel.
	 *
	 * Position is taken from the real collider when one is tagged, because for
	 * melee the authored waypoints only approximate an animated swing. The
	 * trajectory remains the source of heading.
	 */
	FVRThreatGeometry ResolveThreatGeometry() const;

	EVRDodgePhase Phase = EVRDodgePhase::Idle;
	float PhaseTime = 0.0f;

	EVRDuckPhase DuckPhase = EVRDuckPhase::Idle;
	float DuckPhaseTime = 0.0f;

	FVector WorldDirection = FVector::ZeroVector;
	float TargetDistance = 0.0f;

	FVector AppliedWorldOffset = FVector::ZeroVector;
	float AppliedDistance = 0.0f;

	/** Distance already commanded along the dash's path, so each tick moves only the increment. */
	float DashCommandedDistance = 0.0f;

	FVector AppliedDuckOffset = FVector::ZeroVector;
	float DuckTargetDepth = 0.0f;

	float LastDesiredDistance = 0.0f;

	mutable float CachedHurtboxRadius = -1.0f;
	mutable bool bWarnedNoHurtbox = false;

	/** Last valid stick direction in world space. Held when stick returns to deadzone mid-dodge. */
	FVector LastStickWorldDir = FVector::ZeroVector;

	/** True when a closing threat was resolved at dodge start and assist should run. */
	bool bAssistActive = false;

	/** Threat geometry measured at the last BeginDodge. Drives the assist and the readout. */
	FVRThreatGeometry LastThreatGeometry;

	// ── Aerial state ──

	/** True between a launch and the end of the recovery. */
	bool bFloating = false;

	/**
	 * Seconds since the float began.
	 *
	 * Only ever reset by landing. A chained launch finds this already running
	 * and leaves it alone, which is what makes the chain decay.
	 */
	float FloatTime = 0.0f;

	/** Launches so far in the current float. Diagnostics only; nothing is gated on it. */
	int32 FloatLaunchCount = 0;

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

	// ── Displacement diagnostics ──

	/** One on-screen line, owning its slot so the three stay readable as a group. */
	void ReportDebug(const FString& message, int32 slot, FColor color) const;

	/**
	 * The player's eye position: the HMD in VR, the active camera on desktop.
	 *
	 * Taken from the controller rather than any named component, which is the
	 * same source RequestOffsetDodgeFromStick already uses for gaze.
	 */
	FVector GetViewLocation() const;

	/** Draws the request, the capsule displacement and the view displacement. */
	void ReportDashResult() const;

	/** Draws the net displacement once SettleCheckDelay has elapsed. */
	void ReportSettleResult() const;

	FVector DashStartActorLocation = FVector::ZeroVector;
	FVector DashStartViewLocation  = FVector::ZeroVector;

	float DashEndActorDisplacement = 0.0f;
	float DashEndViewDisplacement  = 0.0f;

	/** Counts down after a dash. Negative when no check is pending. */
	float SettleTimer = -1.0f;

	// Directions at the last dodge start, in actor space, purely for the
	// readout. Sampled there because the assist rewrites WorldDirection as the
	// dash runs, so by the time the report prints the original is gone.
	FVector DebugStickLocalDir  = FVector::ZeroVector;
	FVector DebugEscapeLocalDir = FVector::ZeroVector;
	FVector DebugFinalLocalDir  = FVector::ZeroVector;

	int32 DodgeCount = 0;
};
