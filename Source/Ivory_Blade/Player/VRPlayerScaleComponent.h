#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "VRPlayerScaleComponent.generated.h"

class USceneComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogVRPlayerScale, Log, All);

/** Fires every frame of an open capture with how full the stability gate is, 0 to 1. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVRScaleCalibrationProgress, float, Alpha);

/** Fires once with the accepted arm span in centimetres, before any scaling happens. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVRScaleArmSpanMeasured, float, ArmSpanCm);

/** Fires once with the scale the measurement resolved to, before the blend starts. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVRScaleComputed, float, Scale);

/** Fires once when the blend finishes and the rig is actually at the new scale. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVRScaleApplied, float, Scale);

/**
 * Fires on any refusal, carrying a reason fit to show the player.
 *
 * Every path that broadcasts this leaves the rig exactly as it was, so a
 * failed calibration can never strand someone at the wrong size.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVRScaleCalibrationFailed, FText, Reason);

/**
 * Scales the VR rig up so a short player's reach matches the playable
 * character's.
 *
 * Driven by arm span rather than height, because reach is what a sword game
 * actually needs to line up -- and scaling until the arms match makes them
 * match by construction, with no per-limb correction afterwards. The cost is
 * that children have proportionally shorter arms than adults, so a matched
 * player ends up slightly taller than the character rather than the same
 * height. ChildCorrectionStrength mutes that.
 *
 * Nothing runs at startup and nothing is written to disk. The rig stays at
 * scale 1 until CalibratePlayerScale is called, which is deliberate: the real
 * entry point will be a dedicated calibration menu, and this component should
 * not have an opinion about when that happens.
 *
 * Only TargetOrigin is scaled. Scale propagates down a hierarchy and never up,
 * so the capsule the level collides against, the wall-run capsule and the
 * character mesh are all untouched. Stereo separation is driven by
 * WorldSettings->WorldToMeters rather than rig scale, so the player becomes a
 * taller person with longer arms rather than a giant, and the world keeps its
 * apparent size.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class IVORY_BLADE_API UVRPlayerScaleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVRPlayerScaleComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// ── Entry points ──

	/**
	 * Opens a capture window. The player holds a T-pose, and the measurement
	 * commits once their span has been steady for SampleStableSeconds.
	 *
	 * Deliberately does not sample on the call itself. Whatever triggers this
	 * is a button, and pressing a button is not a T-pose -- the player needs a
	 * moment to get into position afterwards.
	 *
	 * Calling this while a window is already open does nothing rather than
	 * restarting it, so an input action that fires more than once per press
	 * cannot keep resetting the stability timer out from under the player.
	 */
	UFUNCTION(BlueprintCallable, Category = "Player|Scale")
	void CalibratePlayerScale();

	/** Abandons an open capture window. The current scale is left alone. */
	UFUNCTION(BlueprintCallable, Category = "Player|Scale")
	void CancelCalibration();

	/** Cancels any running blend and returns the rig to scale 1. */
	UFUNCTION(BlueprintCallable, Category = "Player|Scale")
	void ResetScale();

	/**
	 * Blends to a scale directly, skipping the measurement entirely.
	 *
	 * For bring-up only. A calibration that does nothing is ambiguous -- the
	 * capture may have been refused, or the scale may have been applied and
	 * had no visible effect -- and this tells the two apart by exercising the
	 * apply path on its own. ResetScale puts it back.
	 */
	UFUNCTION(BlueprintCallable, Category = "Player|Scale|Diagnostics")
	void DebugApplyScale(float Scale);

	// ── Wiring ──

	// Everything below is referenced by component name rather than by pointer
	// or by FComponentReference, and both alternatives were tried first.
	//
	// A TObjectPtr<USceneComponent> shows an asset browser that never lists
	// the pawn's own components. An FComponentReference with UseComponentPicker
	// is the documented answer, but its customization resolves the owning actor
	// by walking the property's outer chain -- and inside the Blueprint editor
	// the object being edited is a component template whose outer is the
	// generated class and whose GetOwner() is null, so the picker finds no
	// actor and offers an empty list. It only populates on an Actor's own
	// properties or on an instance already placed in a level.
	//
	// GetOptions restores the dropdown by supplying the list ourselves, from
	// the construction script the engine's picker declined to read. Names then
	// resolve against the owner at runtime, where the components do exist.

	/**
	 * Fills the dropdowns on the wiring properties below.
	 *
	 * Lists the owner's components when there is an owner, and the components
	 * the Blueprint will construct when there is not -- which is the case
	 * whenever these properties are being edited.
	 */
	UFUNCTION()
	TArray<FName> GetSelectableComponentNames() const;

	/**
	 * The rig root to scale, by component name.
	 *
	 * Everything parented under it scales: the camera, the motion controllers,
	 * and so the player's eye height and reach. Anything that should keep its
	 * authored size despite that goes in CounterScaleComponentNames.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Wiring",
		meta = (GetOptions = "GetSelectableComponentNames"))
	FName TargetOriginName = TEXT("VROrigin");

	/**
	 * Components that should stay their authored size while the rig grows.
	 *
	 * Their authored relative scale is cached the first time it is needed and
	 * divided by the rig scale, rather than being overwritten with a bare
	 * reciprocal, so a mesh deliberately authored at something other than 1 is
	 * not silently flattened.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Wiring",
		meta = (GetOptions = "GetSelectableComponentNames"))
	TArray<FName> CounterScaleComponentNames;

	/**
	 * Where the player's hands are, for the span measurement.
	 *
	 * The grip controllers, not the aim ones. The pawn carries four motion
	 * controllers -- an aim and a grip per hand -- so a search by class alone
	 * would be picking one of two at random. If a name does not resolve, that
	 * hand falls back to matching MotionSource against Left or Right, the same
	 * search VRBodyAnimInstance uses.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Wiring",
		meta = (GetOptions = "GetSelectableComponentNames"))
	FName LeftHandComponentName = TEXT("MotionControllerLeftGrip");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Wiring",
		meta = (GetOptions = "GetSelectableComponentNames"))
	FName RightHandComponentName = TEXT("MotionControllerRightGrip");

	// ── Scale ──

	/**
	 * The smallest player who gets the full correction, as a wingspan in cm.
	 *
	 * Authored as a player size rather than a bare multiplier so the limit
	 * keeps its meaning if the character is ever retargeted -- the ceiling is
	 * derived as ReferenceWingspan / this. 135 is roughly a ten-year-old.
	 * Anyone smaller is still scaled, just not all the way.
	 *
	 * The thing this protects against is not reach, it is roomscale
	 * amplification: a scaled rig multiplies physical walking, and that
	 * mismatch between what the legs do and what the eyes see is what makes
	 * people queasy.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale",
		meta = (ClampMin = "1.0"))
	float MinSupportedWingspanCm = 135.0f;

	/**
	 * How much of the gap to the character to close. 1 means an exact match.
	 *
	 * Holds most players below the derived ceiling on its own, so the clamp
	 * only ever binds for the very smallest.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChildCorrectionStrength = 0.75f;

	/** Seconds to ease into a new scale. Snapping it moves the player's viewpoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale",
		meta = (ClampMin = "0.0"))
	float ApplyBlendSeconds = 0.3f;

	// ── Reference ──

	/**
	 * Skip the skeleton measurement and use this wingspan in cm instead.
	 *
	 * Zero computes it from the character's own reference skeleton, which is
	 * the point -- a retarget or a mesh swap then cannot silently invalidate
	 * the calibration. This is the escape hatch for when the derived number
	 * does not match how the character reads in the headset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Reference",
		meta = (ClampMin = "0.0"))
	float ReferenceWingspanOverride = 0.0f;

	/**
	 * Wrist to controller grip, in cm, doubled into the reference span.
	 *
	 * The player's measurement runs controller to controller while the
	 * skeleton's runs wrist to wrist, and without this the reference reads
	 * short and every player is scaled slightly too far.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Reference",
		meta = (ClampMin = "0.0"))
	float GripOffset = 8.0f;

	/** Arm chain bone names. Exposed because retargeting is the reason this is derived at all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Reference")
	FName LeftUpperArmBone = TEXT("upperarm_l");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Reference")
	FName LeftLowerArmBone = TEXT("lowerarm_l");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Reference")
	FName LeftHandBone = TEXT("hand_l");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Reference")
	FName RightUpperArmBone = TEXT("upperarm_r");

	// ── Capture ──

	/** How long the span must hold still before the measurement is accepted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Capture",
		meta = (ClampMin = "0.0"))
	float SampleStableSeconds = 0.75f;

	/** How far the span may drift, in cm, and still count as held still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Capture",
		meta = (ClampMin = "0.0"))
	float SampleToleranceCm = 3.0f;

	/** Give up on an unsteady capture after this long. Zero waits forever. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Capture",
		meta = (ClampMin = "0.0"))
	float CaptureTimeoutSeconds = 10.0f;

	/** Spans outside this range are refused rather than acted on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Capture",
		meta = (ClampMin = "0.0"))
	float MinValidWingspan = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Capture",
		meta = (ClampMin = "0.0"))
	float MaxValidWingspan = 230.0f;

	/**
	 * Refuse to calibrate unless a headset is actually being tracked.
	 *
	 * Off, because the span range check above is the real guard against a
	 * nonsense measurement and it does not care where the positions came
	 * from. On a Quest build head tracking is always allowed, so this only
	 * ever fires in the editor -- where blocking it costs the ability to test
	 * by moving the grip components in the viewport and gains nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Capture")
	bool bRequireHeadTracking = false;

	// ── Output ──

	UPROPERTY(BlueprintAssignable, Category = "Player|Scale")
	FVRScaleCalibrationProgress OnCalibrationProgress;

	UPROPERTY(BlueprintAssignable, Category = "Player|Scale")
	FVRScaleArmSpanMeasured OnArmSpanMeasured;

	UPROPERTY(BlueprintAssignable, Category = "Player|Scale")
	FVRScaleComputed OnScaleComputed;

	UPROPERTY(BlueprintAssignable, Category = "Player|Scale")
	FVRScaleApplied OnScaleApplied;

	UPROPERTY(BlueprintAssignable, Category = "Player|Scale")
	FVRScaleCalibrationFailed OnCalibrationFailed;

	// ── Queries ──

	/** The rig scale in effect right now, mid-blend included. */
	UFUNCTION(BlueprintPure, Category = "Player|Scale")
	float GetAppliedScale() const { return AppliedScale; }

	/** The last accepted arm span in cm, or zero before the first calibration. */
	UFUNCTION(BlueprintPure, Category = "Player|Scale")
	float GetMeasuredWingspan() const { return MeasuredWingspan; }

	/** The character's wingspan in cm. Resolves and caches on first call, zero on failure. */
	UFUNCTION(BlueprintPure, Category = "Player|Scale")
	float GetReferenceWingspan();

	/** True between CalibratePlayerScale and the measurement committing. */
	UFUNCTION(BlueprintPure, Category = "Player|Scale")
	bool IsCalibrating() const { return bCapturing; }

	// ── Diagnostics ──

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Diagnostics")
	bool bLogDiagnostics = true;

	/**
	 * Mirror the calibration to the screen as well as the log.
	 *
	 * Device testing is an untethered APK, where the log is unreadable. Each
	 * kind of line owns its own slot and outlives the frame, so a whole
	 * calibration can be read after taking the headset off.
	 *
	 * On by default. With it off a refusal is indistinguishable from success,
	 * since both leave the rig exactly as it was.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Diagnostics")
	bool bShowDebugOnScreen = true;

	/** Base key for the debug block. Clear of combo 7100, jump 7300 and dodge 7400. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player|Scale|Diagnostics")
	int32 DebugMessageKey = 7500;

private:
	/** Finds a scene component on the owner by name, or null. */
	USceneComponent* FindSceneComponentByName(FName component_name) const;

	/** Every scene component name on the owner, comma separated, for error messages. */
	FString DescribeAvailableComponents() const;

	/** The rig root, resolved from TargetOriginName and cached. */
	USceneComponent* ResolveTargetOrigin() const;

	/** Fills the hand source weak pointers, by picker first and MotionSource second. */
	bool ResolveHandSources(FText& out_error);

	/** Computes and caches the character's wingspan from its reference skeleton. */
	bool ResolveReferenceWingspan(FText& out_error);

	/**
	 * Controller-to-controller distance, expressed in TargetOrigin's local
	 * space so the reading does not depend on the scale already applied.
	 *
	 * Measuring in world space and dividing by a scale this component tracks
	 * itself would compound on every recalibration the moment anything else
	 * touched the rig. Local space is scale-free by construction instead.
	 */
	bool SampleSpan(float& out_span) const;

	/** The piecewise scale for a given span. Child branch live, tall branch inert. */
	float ComputeScaleForSpan(float span) const;

	void UpdateCapture(float delta_time);
	void UpdateBlend(float delta_time);

	/** Writes a scale to the rig and the matching counter-scale to the listed components. */
	void ApplyScaleNow(float scale);

	/** Resolves the counter-scale picks and remembers their authored relative scales. */
	void CacheCounterScaleTargets();

	/** Ends the capture, broadcasts the reason, and leaves the rig untouched. */
	void FailCalibration(const FText& reason);

	/** Ticking costs nothing at rest, so it is only on while a capture or blend runs. */
	void RefreshTickEnabled();

	/** One on-screen line, owning its slot so each kind of event stays readable. */
	void ReportDebug(const FString& message, int32 slot, FColor color) const;

	// ── State ──

	/** Scale currently written to the rig. Moves during a blend. */
	float AppliedScale = 1.0f;

	/** Last accepted span in cm. Zero until the first successful calibration. */
	float MeasuredWingspan = 0.0f;

	/** Negative until resolved. The reference skeleton does not change at runtime. */
	float CachedReferenceWingspan = -1.0f;

	bool  bCapturing      = false;
	float CaptureElapsed  = 0.0f;
	float StableElapsed   = 0.0f;

	/** Span the stability window is currently anchored on. Negative before the first sample. */
	float StableAnchorSpan = -1.0f;

	bool  bBlending    = false;
	float BlendElapsed = 0.0f;
	float BlendFrom    = 1.0f;
	float BlendTo      = 1.0f;

	/** Counter-scale names resolved to live components, with their authored scales alongside. */
	TArray<TWeakObjectPtr<USceneComponent>> ResolvedCounterScaleComponents;
	TArray<FVector> AuthoredCounterScales;
	bool bCounterScaleTargetsCached = false;

	TWeakObjectPtr<USceneComponent> ResolvedLeftHand;
	TWeakObjectPtr<USceneComponent> ResolvedRightHand;

	/** Mutable so the lazy resolve can happen from the const query paths. */
	mutable TWeakObjectPtr<USceneComponent> ResolvedTargetOrigin;
};
