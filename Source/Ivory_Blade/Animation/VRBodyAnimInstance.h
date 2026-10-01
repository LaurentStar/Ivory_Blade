#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "VRBodyAnimInstance.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVRBodyIK, Log, All);

UENUM(BlueprintType)
enum class EVRPoseState : uint8
{
	Standing,
	Crouching,
	Crawling
};

/**
 * AnimInstance that reads the VR HMD height each frame and computes
 * all body-IK variables: pelvis drop, spine lean, head rotation,
 * foot plant targets, IK blend weight, and crawl flag.
 *
 * Set your Animation Blueprint's parent class to this in
 * Class Settings > Parent Class.  All UPROPERTY variables below
 * then appear in the AnimGraph for wiring into Transform Bone
 * and Two Bone IK nodes.
 */
UCLASS()
class IVORY_BLADE_API UVRBodyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UVRBodyAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// ── Tuning (set defaults here or override in the ABP Details panel) ──

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning")
	float MaxPelvisDrop = 100.0f;

	/** Approximate vertical height of the spine chain (pelvis to head) in standing pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning")
	float SpineLength = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning")
	float MaxSpineLeanTotal = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning")
	int32 SpineBoneCount = 5;

	/** CrouchFactor below this is ignored (filters HMD jitter). IK starts here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning")
	float CrouchDeadZone = 0.05f;

	/**
	 * Fallback only. CrouchFactor at which PoseState switches from Standing to
	 * Crouching, used when the pawn exposes no MaxPlayerHeights to read ceilings from.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning")
	float StandingThreshold = 0.30f;

	/** Fallback only. CrouchFactor at which PoseState switches to Crawling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning")
	float CrawlThreshold = 0.80f;

	/**
	 * Clearance required to leave a pose state, in cm. The pawn clamps the player
	 * to the ceiling of the current posture, parking the height on the boundary
	 * where HMD jitter would otherwise flip the state every frame. Applied only
	 * on the way up, so a descending body still switches exactly at the ceiling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning")
	float PoseStateHysteresis = 2.0f;

	/** Small upward padding added to the computed ground level. Prevents feet clipping into the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning")
	float GroundOffset = 2.0f;

	/**
	 * Pins the mesh origin to the capsule bottom as the capsule resizes for crouch.
	 * Turn off if the pawn already drives the mesh's relative Z, so the two do not fight.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning")
	bool bKeepMeshOnCapsuleBottom = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning")
	float BlendInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning")
	float KneeForwardOffset = 40.0f;

	/** Fraction of height gap absorbed by pelvis drop (rest comes from leg IK). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PelvisShare = 0.4f;

	/** Leg IK strength at shallowest crouch (animation dominates). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LegIKMin = 0.2f;

	/** Leg IK strength at deepest crouch (IK dominates to prevent clipping). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LegIKMax = 1.0f;

	/** Speed at which IK ramps back on after a montage ends. Higher = faster recovery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning")
	float MontageIKRecoverySpeed = 5.0f;

	/** IK scale used when the playing animation has no IKScale curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Tuning",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FallbackIKScale = 1.0f;

	// ── Diagnostics ──

	/** Master switch for LogVRBodyIK output. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Diagnostics")
	bool bLogDiagnostics = true;

	/** Seconds between periodic diagnostic lines. State changes always log immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Diagnostics")
	float DiagnosticLogInterval = 0.5f;

	// ── Calibration (read from the VRPawn each tick, not hand-entered) ──

	/**
	 * Ceiling of the standing band. Overwritten every tick from the pawn's
	 * MaxPlayerHeights.X, so any value typed into Class Defaults is ignored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Calibration")
	float StandingHMDHeight = 200.0f;

	/** Ceiling of the crouching band. Overwritten every tick from MaxPlayerHeights.Y. */
	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Calibration")
	float CrouchingCeiling = 0.0f;

	/** Ceiling of the crawling band. Overwritten every tick from MaxPlayerHeights.Z. */
	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Calibration")
	float CrawlingCeiling = 0.0f;

	/**
	 * True when the pawn supplied a well-ordered MaxPlayerHeights this tick.
	 * When false, PoseState falls back to the CrouchFactor ratio thresholds.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Calibration")
	bool bCeilingsValid = false;

	/** Current player height. Read from the VRPawn's PlayerHeight variable each tick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR Body IK|Calibration")
	float CurrentPlayerHeight = 0.0f;

	// ── Computed outputs (read these in the AnimGraph) ──

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	float CrouchFactor = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	float PelvisZOffset = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	float LeanPerBone = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	FRotator HeadRotationCS = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	float IKBlendWeight = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	bool bIsCrawling = false;

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	EVRPoseState PoseState = EVRPoseState::Standing;

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	FVector LeftFootTarget = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	FVector RightFootTarget = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	FVector LeftKneeTarget = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	FVector RightKneeTarget = FVector::ZeroVector;

	/** IKBlendWeight scaled by LegIKStrength. Wire this to Two Bone IK Alpha pins. */
	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	float LegIKAlpha = 0.0f;

	/** IKScale resolved from the playing animation this tick, or FallbackIKScale. */
	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	float AnimIKScale = 1.0f;

	/** GroundOffset correction from the playing animation's curve, in cm. */
	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	float AnimGroundOffset = 0.0f;

	// ── Arm IK outputs (VR controller tracking) ──

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Arm IK")
	FTransform LeftHandTarget = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Arm IK")
	FTransform RightHandTarget = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Arm IK")
	FVector LeftElbowTarget = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Arm IK")
	FVector RightElbowTarget = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Arm IK")
	float ArmIKAlpha = 1.0f;

	// ── Montage output ──

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Output")
	bool bMontageActive = false;

	// ── Locomotion (used by state machine transitions) ──

	UPROPERTY(BlueprintReadOnly, Category = "VR Body IK|Locomotion")
	float Speed = 0.0f;

private:
	bool bFootRefCaptured = false;
	FVector RefPoseFootL_CS = FVector::ZeroVector;
	FVector RefPoseFootR_CS = FVector::ZeroVector;

	float MontageIKBlendOut = 1.0f;

	/** Unscaled interpolation state behind IKBlendWeight. Never has scaling applied to it. */
	float IKBlendBase = 0.0f;

	EVRPoseState PreviousPoseState = EVRPoseState::Standing;
	float DiagnosticLogAccumulator = 0.0f;
	bool bLoggedCalibrationOnce = false;
	bool bWarnedNoCeilings = false;

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> CachedCamera;

	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshComponent> CachedMesh;

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> CachedLeftController;

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> CachedRightController;

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> CachedBasePoint;
};
