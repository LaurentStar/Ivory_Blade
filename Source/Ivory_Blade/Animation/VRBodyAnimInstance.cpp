#include "VRBodyAnimInstance.h"
#include "VRIKTargets.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "MotionControllerComponent.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogVRBodyIK);

static TAutoConsoleVariable<float> CVarDebugPlayerHeight(
	TEXT("vr.BodyIK.DebugHeight"),
	-1.0f,
	TEXT("Overrides the measured player height in cm for testing without an HMD. Negative disables."),
	ECVF_Cheat);

namespace
{
	/**
	 * Blueprint "float" variables are double-precision in UE5. Reading one through
	 * a float-typed pointer reinterprets the low half of the mantissa, so both
	 * widths have to be handled explicitly.
	 */
	bool ReadPawnScalar(const APawn* pawn, FName property_name, float& out_value)
	{
		if (!pawn)
		{
			return false;
		}

		FProperty* prop = pawn->GetClass()->FindPropertyByName(property_name);

		if (const FDoubleProperty* double_prop = CastField<FDoubleProperty>(prop))
		{
			out_value = static_cast<float>(double_prop->GetPropertyValue_InContainer(pawn));
			return true;
		}
		if (const FFloatProperty* float_prop = CastField<FFloatProperty>(prop))
		{
			out_value = float_prop->GetPropertyValue_InContainer(pawn);
			return true;
		}
		return false;
	}

	bool ReadPawnVector(const APawn* pawn, FName property_name, FVector& out_value)
	{
		if (!pawn)
		{
			return false;
		}

		const FStructProperty* struct_prop = CastField<FStructProperty>(
			pawn->GetClass()->FindPropertyByName(property_name));

		if (!struct_prop || struct_prop->Struct != TBaseStructure<FVector>::Get())
		{
			return false;
		}

		out_value = *struct_prop->ContainerPtrToValuePtr<FVector>(pawn);
		return true;
	}
}

UVRBodyAnimInstance::UVRBodyAnimInstance()
{
}

void UVRBodyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	APawn* pawn = TryGetPawnOwner();
	if (!pawn)
	{
		return;
	}

	CachedMesh = pawn->FindComponentByClass<USkeletalMeshComponent>();
	CachedCamera = pawn->FindComponentByClass<UCameraComponent>();

	TInlineComponentArray<UMotionControllerComponent*> controllers;
	pawn->GetComponents(controllers);
	for (UMotionControllerComponent* mc : controllers)
	{
		if (mc->MotionSource == FName("Left"))
		{
			CachedLeftController = mc;
		}
		else if (mc->MotionSource == FName("Right"))
		{
			CachedRightController = mc;
		}
	}

	TInlineComponentArray<USceneComponent*> scene_components;
	pawn->GetComponents(scene_components);
	for (USceneComponent* comp : scene_components)
	{
		if (comp->GetName() == TEXT("Base Point"))
		{
			CachedBasePoint = comp;
			break;
		}
	}
}

void UVRBodyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	APawn* pawn = TryGetPawnOwner();
	if (!pawn)
	{
		return;
	}

	// Re-cache if components went stale
	if (!CachedMesh.IsValid())
	{
		CachedMesh = pawn->FindComponentByClass<USkeletalMeshComponent>();
	}
	if (!CachedCamera.IsValid())
	{
		CachedCamera = pawn->FindComponentByClass<UCameraComponent>();
	}
	if (!CachedBasePoint.IsValid())
	{
		TInlineComponentArray<USceneComponent*> scene_components;
		pawn->GetComponents(scene_components);
		for (USceneComponent* comp : scene_components)
		{
			if (comp->GetName() == TEXT("Base Point"))
			{
				CachedBasePoint = comp;
				break;
			}
		}
	}

	USceneComponent* camera = CachedCamera.Get();
	USkeletalMeshComponent* mesh = CachedMesh.Get();

	// ── Keep the mesh origin on the capsule bottom ──
	// The mesh is parented to the capsule, so shrinking the capsule for a crouch
	// raises its bottom and leaves the mesh hanging below it by the half-height
	// change. Relative space rotates with the capsule, so this stays correct
	// under planetoid gravity where world Z is not up.
	float mesh_relative_z = 0.0f;
	if (bKeepMeshOnCapsuleBottom && mesh)
	{
		if (const UCapsuleComponent* capsule = pawn->FindComponentByClass<UCapsuleComponent>())
		{
			const float desired_z = -capsule->GetUnscaledCapsuleHalfHeight();
			FVector mesh_relative = mesh->GetRelativeLocation();

			if (!FMath::IsNearlyEqual(static_cast<float>(mesh_relative.Z), desired_z, 0.01f))
			{
				mesh_relative.Z = desired_z;
				mesh->SetRelativeLocation(mesh_relative);
			}

			mesh_relative_z = desired_z;
		}
	}

	// ── Capture reference-pose foot positions on first valid frame ──
	if (!bFootRefCaptured && mesh)
	{
		RefPoseFootL_CS = mesh->GetBoneLocation(
			FName(TEXT("foot_l")), EBoneSpaces::ComponentSpace);
		RefPoseFootR_CS = mesh->GetBoneLocation(
			FName(TEXT("foot_r")), EBoneSpaces::ComponentSpace);

		if (!RefPoseFootL_CS.IsZero() || !RefPoseFootR_CS.IsZero())
		{
			bFootRefCaptured = true;
		}
	}

	// ── Locomotion speed ──
	if (ACharacter* character = Cast<ACharacter>(pawn))
	{
		if (UCharacterMovementComponent* movement = character->GetCharacterMovement())
		{
			Speed = movement->Velocity.Size2D();
		}
	}

	// ── Read the measured player height and the calibrated posture ceilings ──
	ReadPawnScalar(pawn, FName("PlayerHeight"), CurrentPlayerHeight);

	const float debug_height = CVarDebugPlayerHeight.GetValueOnAnyThread();
	const bool bHeightOverridden = debug_height >= 0.0f;
	if (bHeightOverridden)
	{
		CurrentPlayerHeight = debug_height;
	}

	FVector max_heights = FVector::ZeroVector;
	bCeilingsValid = ReadPawnVector(pawn, FName("MaxPlayerHeights"), max_heights)
	                 && max_heights.X > max_heights.Y
	                 && max_heights.Y > max_heights.Z
	                 && max_heights.Z > 0.0;

	if (bCeilingsValid)
	{
		StandingHMDHeight = static_cast<float>(max_heights.X);
		CrouchingCeiling  = static_cast<float>(max_heights.Y);
		CrawlingCeiling   = static_cast<float>(max_heights.Z);
	}

	// One line per transition in either direction, so neither path can spam the log.
	if (bLogDiagnostics)
	{
		if (bCeilingsValid)
		{
			if (!bLoggedCalibrationOnce)
			{
				bLoggedCalibrationOnce = true;
				bWarnedNoCeilings = false;
				UE_LOG(LogVRBodyIK, Log,
					TEXT("Calibration acquired from %s -- Standing:%.1f Crouching:%.1f Crawling:%.1f"),
					*pawn->GetClass()->GetName(),
					StandingHMDHeight, CrouchingCeiling, CrawlingCeiling);
			}
		}
		else if (!bWarnedNoCeilings)
		{
			bWarnedNoCeilings = true;
			bLoggedCalibrationOnce = false;
			UE_LOG(LogVRBodyIK, Warning,
				TEXT("No usable MaxPlayerHeights on %s (read %.1f/%.1f/%.1f) -- falling back to ratio thresholds"),
				*pawn->GetClass()->GetName(),
				max_heights.X, max_heights.Y, max_heights.Z);
		}
	}

	// Keep the library's internal crawl gate on the same boundary as the state machine.
	const float crawl_gate = bCeilingsValid
		? (1.0f - (CrawlingCeiling / StandingHMDHeight))
		: CrawlThreshold;

	// ── Body IK params (only when both heights are valid) ──
	bool ratio_is_crawling = false;
	const bool bHeightsValid = (StandingHMDHeight > 0.0f && CurrentPlayerHeight > 0.0f);
	if (bHeightsValid)
	{
		UVRIKTargets::ComputeBodyIKParams(
			CurrentPlayerHeight,
			StandingHMDHeight,
			MaxPelvisDrop,
			MaxSpineLeanTotal,
			SpineBoneCount,
			CrouchDeadZone,
			crawl_gate,
			SpineLength,
			PelvisShare,
			CrouchFactor,
			PelvisZOffset,
			LeanPerBone,
			ratio_is_crawling
		);
	}

	// ── Pose state follows the measured body, not the pawn's Posture directive ──
	// Posture is a target the pawn lowers the player toward over time; reading it
	// would swap the animation before the body arrives. Comparing the live height
	// against the ceilings walks Standing -> Crouching -> Crawling during a descent.
	// A zero height means the pawn has not reported one yet, which must not read
	// as "below every ceiling".
	const bool bHeightDriven = bCeilingsValid && CurrentPlayerHeight > 0.0f;

	if (bHeightDriven)
	{
		const float hysteresis  = FMath::Max(PoseStateHysteresis, 0.0f);
		const float crouch_exit = CrouchingCeiling + hysteresis;
		const float crawl_exit  = CrawlingCeiling + hysteresis;

		if (CurrentPlayerHeight <= CrawlingCeiling)
		{
			PoseState = EVRPoseState::Crawling;
		}
		else if (CurrentPlayerHeight > crawl_exit && CurrentPlayerHeight <= CrouchingCeiling)
		{
			PoseState = EVRPoseState::Crouching;
		}
		else if (CurrentPlayerHeight > crouch_exit)
		{
			PoseState = EVRPoseState::Standing;
		}
		// Between a ceiling and its exit clearance the previous state is held.
	}
	else if (ratio_is_crawling)
	{
		PoseState = EVRPoseState::Crawling;
	}
	else if (CrouchFactor >= StandingThreshold)
	{
		PoseState = EVRPoseState::Crouching;
	}
	else
	{
		PoseState = EVRPoseState::Standing;
	}

	bIsCrawling = bHeightDriven
		? (PoseState == EVRPoseState::Crawling)
		: ratio_is_crawling;

	if (PoseState != PreviousPoseState)
	{
		if (bLogDiagnostics)
		{
			UE_LOG(LogVRBodyIK, Log,
				TEXT("PoseState %d -> %d at PlayerH:%.1f (crouch ceil %.1f, crawl ceil %.1f)"),
				(int32)PreviousPoseState, (int32)PoseState,
				CurrentPlayerHeight, CrouchingCeiling, CrawlingCeiling);
		}
		PreviousPoseState = PoseState;
	}

	// ── Head rotation from HMD ──
	if (mesh && camera)
	{
		FVector unused_location;
		UVRIKTargets::ComputeHeadIKTarget(
			mesh, camera, unused_location, HeadRotationCS);
	}

	// ── Foot plant targets (X/Y from reference pose, Z at Base Point ground level) ──
	// Per-animation correction in cm. An absent curve reads 0, which is exactly
	// "no adjustment", so untouched animations behave as they do today.
	AnimGroundOffset = 0.0f;
	GetCurveValue(FName("GroundOffset"), AnimGroundOffset);

	const float total_ground_offset = GroundOffset + AnimGroundOffset;

	float ground_z_cs = total_ground_offset;
	USceneComponent* base_point = CachedBasePoint.Get();
	if (mesh && base_point)
	{
		FVector ground_world(mesh->GetComponentLocation().X,
		                     mesh->GetComponentLocation().Y,
		                     base_point->GetComponentLocation().Z);
		FVector ground_cs = mesh->GetComponentTransform().InverseTransformPosition(ground_world);
		ground_z_cs = ground_cs.Z + total_ground_offset;
	}
	LeftFootTarget = FVector(RefPoseFootL_CS.X, RefPoseFootL_CS.Y, ground_z_cs);
	RightFootTarget = FVector(RefPoseFootR_CS.X, RefPoseFootR_CS.Y, ground_z_cs);

	// ── Knee joint targets (calf bone position offset forward in CS) ──
	if (mesh)
	{
		FVector calf_l = mesh->GetBoneLocation(
			FName(TEXT("calf_l")), EBoneSpaces::ComponentSpace);
		FVector calf_r = mesh->GetBoneLocation(
			FName(TEXT("calf_r")), EBoneSpaces::ComponentSpace);

		LeftKneeTarget = FVector(
			calf_l.X, calf_l.Y + KneeForwardOffset, calf_l.Z);
		RightKneeTarget = FVector(
			calf_r.X, calf_r.Y + KneeForwardOffset, calf_r.Z);
	}

	// ── IK blend weight ──
	float target_weight = 0.0f;
	if (!bIsCrawling && CrouchFactor >= CrouchDeadZone)
	{
		target_weight = 1.0f;
	}

	// The interpolation state stays unscaled; feeding a scaled value back into the
	// interp reapplies the scaling every frame and collapses the result.
	IKBlendBase = UVRIKTargets::InterpIKBlendWeight(
		IKBlendBase, target_weight, DeltaSeconds, BlendInterpSpeed);

	// ── Montage detection: suppress all IK during full-body montages ──
	bMontageActive = IsAnyMontagePlaying();

	if (bMontageActive)
	{
		MontageIKBlendOut = 0.0f;
	}
	else
	{
		MontageIKBlendOut = FMath::FInterpTo(MontageIKBlendOut, 1.0f,
		                                     DeltaSeconds, MontageIKRecoverySpeed);
	}

	// ── Per-animation IK scaling via animation curve ──
	AnimIKScale = FallbackIKScale;
	GetCurveValue(FName("IKScale"), AnimIKScale);
	AnimIKScale = FMath::Clamp(AnimIKScale, 0.0f, 1.0f);

	const float leg_strength = FMath::Lerp(LegIKMin, LegIKMax, CrouchFactor);
	const float common_scale = MontageIKBlendOut * AnimIKScale;

	IKBlendWeight = IKBlendBase * common_scale;
	LegIKAlpha    = IKBlendBase * leg_strength * common_scale;

	// ── Arm IK from VR controllers ──
	if (mesh)
	{
		USceneComponent* left_mc = CachedLeftController.Get();
		USceneComponent* right_mc = CachedRightController.Get();

		if (!left_mc || !right_mc)
		{
			if (APawn* p = TryGetPawnOwner())
			{
				TInlineComponentArray<UMotionControllerComponent*> controllers;
				p->GetComponents(controllers);
				for (UMotionControllerComponent* mc : controllers)
				{
					if (mc->MotionSource == FName("Left"))
					{
						CachedLeftController = mc;
						left_mc = mc;
					}
					else if (mc->MotionSource == FName("Right"))
					{
						CachedRightController = mc;
						right_mc = mc;
					}
				}
			}
		}

		if (left_mc)
		{
			LeftHandTarget = UVRIKTargets::ComputeHandIKTarget(mesh, left_mc);
			LeftElbowTarget = UVRIKTargets::ComputeJointTarget(
				mesh, FName("upperarm_l"), LeftHandTarget.GetLocation(), true);
		}
		if (right_mc)
		{
			RightHandTarget = UVRIKTargets::ComputeHandIKTarget(mesh, right_mc);
			RightElbowTarget = UVRIKTargets::ComputeJointTarget(
				mesh, FName("upperarm_r"), RightHandTarget.GetLocation(), false);
		}
	}

	bool bHMDActive = GEngine && GEngine->XRSystem.IsValid()
	                  && GEngine->XRSystem->IsHeadTrackingAllowed();
	ArmIKAlpha = (bIsCrawling || bMontageActive || !bHMDActive) ? 0.0f : MontageIKBlendOut;

	if (bLogDiagnostics)
	{
		DiagnosticLogAccumulator += DeltaSeconds;
		if (DiagnosticLogAccumulator >= FMath::Max(DiagnosticLogInterval, 0.05f))
		{
			DiagnosticLogAccumulator = 0.0f;
			UE_LOG(LogVRBodyIK, Log,
				TEXT("Ceilings %s%s  Stand:%.1f Crouch:%.1f Crawl:%.1f  PlayerH:%.1f  CrouchFactor:%.3f  Pose:%d  MeshZ:%.1f GndOff:%.1f IKScale:%.3f IKBlend:%.3f LegIK:%.3f ArmIK:%.3f"),
				bCeilingsValid ? TEXT("LIVE") : TEXT("FALLBACK"),
				bHeightOverridden ? TEXT(" [OVERRIDE]") : TEXT(""),
				StandingHMDHeight, CrouchingCeiling, CrawlingCeiling,
				CurrentPlayerHeight, CrouchFactor, (int32)PoseState,
				mesh_relative_z, AnimGroundOffset, AnimIKScale,
				IKBlendWeight, LegIKAlpha, ArmIKAlpha);
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(100, 0.f, FColor::Green,
			FString::Printf(TEXT("Ceilings %s%s  Stand:%.1f Crouch:%.1f Crawl:%.1f  PlayerH:%.1f  CrouchFactor:%.3f  Pose:%d"),
				bCeilingsValid ? TEXT("LIVE") : TEXT("FALLBACK"),
				bHeightOverridden ? TEXT(" [OVERRIDE]") : TEXT(""),
				StandingHMDHeight, CrouchingCeiling, CrawlingCeiling,
				CurrentPlayerHeight, CrouchFactor, (int32)PoseState));
		GEngine->AddOnScreenDebugMessage(101, 0.f, FColor::Yellow,
			FString::Printf(TEXT("IKBlend: %.3f  LegIK: %.3f  ArmIK: %.3f"),
				IKBlendWeight, LegIKAlpha, ArmIKAlpha));
		GEngine->AddOnScreenDebugMessage(102, 0.f, FColor::Cyan,
			FString::Printf(TEXT("BasePoint: %s"),
				CachedBasePoint.IsValid() ? TEXT("FOUND") : TEXT("NULL")));
	}
}
