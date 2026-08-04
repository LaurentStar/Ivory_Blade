#include "VRBodyAnimInstance.h"
#include "VRIKTargets.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "MotionControllerComponent.h"

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

	USceneComponent* camera = CachedCamera.Get();
	USkeletalMeshComponent* mesh = CachedMesh.Get();

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

	// ── Calibrate standing height on first valid frame ──
	if (!bCalibrated && camera)
	{
		const float z = camera->GetComponentLocation().Z;
		if (z > 1.0f)
		{
			StandingHMDHeight = z;
			bCalibrated = true;
		}
	}

	if (!bCalibrated)
	{
		return;
	}

	// ── Body IK params (pelvis drop, spine lean, crawl flag) ──
	UVRIKTargets::ComputeBodyIKParams(
		camera,
		StandingHMDHeight,
		MaxPelvisDrop,
		MaxSpineLeanTotal,
		SpineBoneCount,
		CrouchDeadZone,
		CrawlThreshold,
		SpineLength,
		PelvisShare,
		CrouchFactor,
		PelvisZOffset,
		LeanPerBone,
		bIsCrawling
	);

	// ── Pose state for animation state machine ──
	if (bIsCrawling)
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

	// ── Head rotation from HMD ──
	if (mesh && camera)
	{
		FVector unused_location;
		UVRIKTargets::ComputeHeadIKTarget(
			mesh, camera, unused_location, HeadRotationCS);
	}

	// ── Foot plant targets (fixed X/Y from reference pose, Z at ground level) ──
	LeftFootTarget = FVector(RefPoseFootL_CS.X, RefPoseFootL_CS.Y, GroundOffset);
	RightFootTarget = FVector(RefPoseFootR_CS.X, RefPoseFootR_CS.Y, GroundOffset);

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

	IKBlendWeight = UVRIKTargets::InterpIKBlendWeight(
		IKBlendWeight, target_weight, DeltaSeconds, BlendInterpSpeed);

	const float leg_strength = FMath::Lerp(LegIKMin, LegIKMax, CrouchFactor);
	LegIKAlpha = IKBlendWeight * leg_strength;

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

	IKBlendWeight *= MontageIKBlendOut;
	LegIKAlpha   *= MontageIKBlendOut;

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

	ArmIKAlpha = (bIsCrawling || bMontageActive) ? 0.0f : MontageIKBlendOut;
}
