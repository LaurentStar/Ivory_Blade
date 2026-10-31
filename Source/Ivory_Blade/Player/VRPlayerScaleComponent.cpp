// Scales the VR rig up so a short player's reach matches the character's.
//
// The whole feature is one number -- the relative scale written to
// TargetOrigin -- derived from one measurement, the player's arm span. Most of
// the code here is refusing to act on a measurement it does not trust, because
// the failure mode of a bad one is a player stuck at the wrong size with no
// idea why.
//
// Debug slots: 0 = capture opened, 1 = live stability fill, 2 = span accepted,
// 3 = scale resolved and applied, 4 = refusal.

#include "VRPlayerScaleComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Engine.h"
#include "Engine/SkinnedAsset.h"
#include "GameFramework/Character.h"
#include "IXRTrackingSystem.h"
#include "MotionControllerComponent.h"
#include "ReferenceSkeleton.h"

#define LOCTEXT_NAMESPACE "VRPlayerScale"

DEFINE_LOG_CATEGORY(LogVRPlayerScale);

namespace VRPlayerScaleDebug
{
	// One slot per kind of line, so an opened capture is still on screen when
	// its result lands and the pair can be read as a sequence.
	constexpr int32 OpenedSlot   = 0;
	constexpr int32 ProgressSlot = 1;
	constexpr int32 MeasuredSlot = 2;
	constexpr int32 AppliedSlot  = 3;
	constexpr int32 FailedSlot   = 4;

	/** Long enough to take the headset off and still read the line. */
	constexpr float Lifetime = 12.0f;
}

namespace VRPlayerScaleMath
{
	/**
	 * Composes a reference-pose bone up to component space.
	 *
	 * Each entry in the ref bone pose is relative to its parent, so walking to
	 * the root is the only way to get two bones into a common space -- which
	 * the shoulder separation needs and a single bone length does not.
	 */
	FTransform RefBoneComponentTransform(const FReferenceSkeleton& ref_skeleton, int32 bone_index)
	{
		const TArray<FTransform>& pose = ref_skeleton.GetRefBonePose();

		FTransform result = FTransform::Identity;

		for (int32 index = bone_index; index != INDEX_NONE && pose.IsValidIndex(index);
		     index = ref_skeleton.GetParentIndex(index))
		{
			result = result * pose[index];
		}

		return result;
	}
}

UVRPlayerScaleComponent::UVRPlayerScaleComponent()
{
	// Enabled only while a capture or a blend is running. At rest this feature
	// has nothing to do and should cost nothing.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Defaulted to this pawn's hand meshes so the common case needs no setup.
	CounterScaleComponentNames = { TEXT("HandLeft"), TEXT("HandRight") };
}

void UVRPlayerScaleComponent::BeginPlay()
{
	Super::BeginPlay();

	AppliedScale = 1.0f;

	UE_LOG(LogVRPlayerScale, Warning,
		TEXT("VRPlayerScaleComponent live on %s. Rig starts unscaled; nothing happens until CalibratePlayerScale."),
		*GetNameSafe(GetOwner()));
}

void UVRPlayerScaleComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateCapture(DeltaTime);
	UpdateBlend(DeltaTime);

	RefreshTickEnabled();
}

// ── Entry points ──

void UVRPlayerScaleComponent::CalibratePlayerScale()
{
	// Ignored rather than restarted. An Enhanced Input action that fires more
	// than once per press would otherwise reset the stability timer every
	// frame and the capture could never complete.
	if (bCapturing)
	{
		return;
	}

	if (!ResolveTargetOrigin())
	{
		UE_LOG(LogVRPlayerScale, Warning,
			TEXT("Rig origin '%s' not found. Scene components on %s: %s"),
			*TargetOriginName.ToString(), *GetNameSafe(GetOwner()), *DescribeAvailableComponents());

		FailCalibration(LOCTEXT("NoTargetOrigin",
			"Calibration failed: the rig origin named in Target Origin Name was not found."));
		return;
	}

	FText error;

	if (!ResolveHandSources(error))
	{
		FailCalibration(error);
		return;
	}

	if (!ResolveReferenceWingspan(error))
	{
		FailCalibration(error);
		return;
	}

	if (bRequireHeadTracking
		&& (!GEngine || !GEngine->XRSystem.IsValid() || !GEngine->XRSystem->IsHeadTrackingAllowed()))
	{
		FailCalibration(LOCTEXT("NoTracking", "Calibration failed: the headset is not being tracked."));
		return;
	}

	bCapturing       = true;
	CaptureElapsed   = 0.0f;
	StableElapsed    = 0.0f;
	StableAnchorSpan = -1.0f;

	RefreshTickEnabled();

	OnCalibrationProgress.Broadcast(0.0f);

	ReportDebug(FString::Printf(TEXT("SCALE capture open  reference=%.1fcm  hold a T-pose"),
		CachedReferenceWingspan), VRPlayerScaleDebug::OpenedSlot, FColor::Cyan);

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRPlayerScale, Log, TEXT("Capture opened. Reference wingspan %.1f cm."),
			CachedReferenceWingspan);
	}
}

void UVRPlayerScaleComponent::CancelCalibration()
{
	if (!bCapturing)
	{
		return;
	}

	bCapturing = false;
	RefreshTickEnabled();

	ReportDebug(TEXT("  capture cancelled"), VRPlayerScaleDebug::FailedSlot, FColor::Silver);

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRPlayerScale, Log, TEXT("Capture cancelled. Scale left at %.3f."), AppliedScale);
	}
}

void UVRPlayerScaleComponent::ResetScale()
{
	bCapturing = false;
	bBlending  = false;

	ApplyScaleNow(1.0f);
	RefreshTickEnabled();

	OnScaleApplied.Broadcast(1.0f);

	ReportDebug(TEXT("SCALE reset to 1.000"), VRPlayerScaleDebug::AppliedSlot, FColor::Silver);

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRPlayerScale, Log, TEXT("Scale reset to 1.0."));
	}
}

void UVRPlayerScaleComponent::DebugApplyScale(float Scale)
{
	if (Scale <= KINDA_SMALL_NUMBER)
	{
		FailCalibration(LOCTEXT("BadDebugScale", "Debug scale must be greater than zero."));
		return;
	}

	// An open capture would overwrite this the moment it settled.
	bCapturing = false;

	BlendFrom    = AppliedScale;
	BlendTo      = Scale;
	BlendElapsed = 0.0f;
	bBlending    = true;

	RefreshTickEnabled();

	ReportDebug(FString::Printf(TEXT("SCALE %.3f  (forced, no measurement)"), Scale),
		VRPlayerScaleDebug::AppliedSlot, FColor::Magenta);

	UE_LOG(LogVRPlayerScale, Warning, TEXT("Debug scale %.3f applied without measuring."), Scale);
}

// ── Resolution ──

TArray<FName> UVRPlayerScaleComponent::GetSelectableComponentNames() const
{
	TArray<FName> names;

	// With a real owner -- a placed instance, or play -- the components exist
	// and can simply be listed.
	if (const AActor* owner = GetOwner())
	{
		TInlineComponentArray<USceneComponent*> components;
		owner->GetComponents(components);

		for (const USceneComponent* component : components)
		{
			if (component)
			{
				names.AddUnique(component->GetFName());
			}
		}

		names.Sort(FNameLexicalLess());
		return names;
	}

	// Otherwise this is a component template being edited inside a Blueprint,
	// where no actor exists yet -- which is exactly the situation the engine's
	// own component picker gives up on. The components the pawn *will* build
	// are described by the construction script on the generated class, which
	// is this template's outer, so the list is read from there instead.
	const UClass* generated_class = nullptr;

	for (const UObject* outer = GetOuter(); outer; outer = outer->GetOuter())
	{
		if (const UBlueprintGeneratedClass* as_generated = Cast<UBlueprintGeneratedClass>(outer))
		{
			generated_class = as_generated;
			break;
		}
	}

	if (!generated_class)
	{
		return names;
	}

	// Components declared in C++ are subobjects of the default object and
	// never appear in a construction script, so they are collected separately.
	if (const AActor* default_actor = Cast<AActor>(generated_class->GetDefaultObject()))
	{
		TInlineComponentArray<USceneComponent*> components;
		default_actor->GetComponents(components);

		for (const USceneComponent* component : components)
		{
			if (component)
			{
				names.AddUnique(component->GetFName());
			}
		}
	}

	// Then everything added through the Components panel, including whatever
	// the pawn inherits from a parent Blueprint.
	TArray<const UBlueprintGeneratedClass*> class_hierarchy;
	UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(generated_class, class_hierarchy);

	for (const UBlueprintGeneratedClass* bp_class : class_hierarchy)
	{
		const USimpleConstructionScript* construction_script =
			bp_class ? bp_class->SimpleConstructionScript : nullptr;

		if (!construction_script)
		{
			continue;
		}

		for (const USCS_Node* node : construction_script->GetAllNodes())
		{
			if (node && node->ComponentClass
				&& node->ComponentClass->IsChildOf(USceneComponent::StaticClass()))
			{
				names.AddUnique(node->GetVariableName());
			}
		}
	}

	names.Sort(FNameLexicalLess());
	return names;
}

USceneComponent* UVRPlayerScaleComponent::FindSceneComponentByName(FName component_name) const
{
	const AActor* owner = GetOwner();

	if (!owner || component_name.IsNone())
	{
		return nullptr;
	}

	TInlineComponentArray<USceneComponent*> components;
	owner->GetComponents(components);

	for (USceneComponent* component : components)
	{
		if (component && component->GetFName() == component_name)
		{
			return component;
		}
	}

	return nullptr;
}

FString UVRPlayerScaleComponent::DescribeAvailableComponents() const
{
	const AActor* owner = GetOwner();

	if (!owner)
	{
		return TEXT("<no owner>");
	}

	TInlineComponentArray<USceneComponent*> components;
	owner->GetComponents(components);

	TArray<FString> names;
	names.Reserve(components.Num());

	for (const USceneComponent* component : components)
	{
		if (component)
		{
			names.Add(component->GetName());
		}
	}

	return FString::Join(names, TEXT(", "));
}

USceneComponent* UVRPlayerScaleComponent::ResolveTargetOrigin() const
{
	if (!ResolvedTargetOrigin.IsValid())
	{
		ResolvedTargetOrigin = FindSceneComponentByName(TargetOriginName);
	}

	return ResolvedTargetOrigin.Get();
}

bool UVRPlayerScaleComponent::ResolveHandSources(FText& out_error)
{
	USceneComponent* left  = FindSceneComponentByName(LeftHandComponentName);
	USceneComponent* right = FindSceneComponentByName(RightHandComponentName);

	// Falling back to the same MotionSource search VRBodyAnimInstance uses,
	// per hand rather than all-or-nothing, so one name resolving and the other
	// not still leaves a usable pair. This picks whichever of the aim/grip
	// pair carries the plain Left and Right sources, which is a guess -- hence
	// the names being the preferred route.
	if (!left || !right)
	{
		const AActor* owner = GetOwner();

		if (!owner)
		{
			out_error = LOCTEXT("NoOwner", "Calibration failed: the component has no owning actor.");
			return false;
		}

		TInlineComponentArray<UMotionControllerComponent*> controllers;
		owner->GetComponents(controllers);

		for (UMotionControllerComponent* controller : controllers)
		{
			if (!left && controller->MotionSource == FName("Left"))
			{
				left = controller;
			}
			else if (!right && controller->MotionSource == FName("Right"))
			{
				right = controller;
			}
		}
	}

	if (!left || !right)
	{
		UE_LOG(LogVRPlayerScale, Warning,
			TEXT("Hand lookup failed for '%s' / '%s'. Scene components on %s: %s"),
			*LeftHandComponentName.ToString(), *RightHandComponentName.ToString(),
			*GetNameSafe(GetOwner()), *DescribeAvailableComponents());

		out_error = LOCTEXT("NoHands",
			"Calibration failed: could not find both hands. Check Left and Right Hand Component Name.");
		return false;
	}

	ResolvedLeftHand  = left;
	ResolvedRightHand = right;

	return true;
}

bool UVRPlayerScaleComponent::ResolveReferenceWingspan(FText& out_error)
{
	if (ReferenceWingspanOverride > 0.0f)
	{
		CachedReferenceWingspan = ReferenceWingspanOverride;
		return true;
	}

	if (CachedReferenceWingspan > 0.0f)
	{
		return true;
	}

	const ACharacter* character = Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent* mesh = character ? character->GetMesh() : nullptr;
	const USkinnedAsset* asset = mesh ? mesh->GetSkinnedAsset() : nullptr;

	if (!asset)
	{
		out_error = LOCTEXT("NoMesh",
			"Calibration failed: no character mesh to measure. Set Reference Wingspan Override instead.");
		return false;
	}

	const FReferenceSkeleton& ref_skeleton = asset->GetRefSkeleton();
	const TArray<FTransform>& pose         = ref_skeleton.GetRefBonePose();

	const int32 lower_arm_l = ref_skeleton.FindBoneIndex(LeftLowerArmBone);
	const int32 hand_l      = ref_skeleton.FindBoneIndex(LeftHandBone);
	const int32 upper_arm_l = ref_skeleton.FindBoneIndex(LeftUpperArmBone);
	const int32 upper_arm_r = ref_skeleton.FindBoneIndex(RightUpperArmBone);

	if (!pose.IsValidIndex(lower_arm_l) || !pose.IsValidIndex(hand_l)
	    || !pose.IsValidIndex(upper_arm_l) || !pose.IsValidIndex(upper_arm_r))
	{
		out_error = LOCTEXT("NoArmBones",
			"Calibration failed: the arm bones were not found on this skeleton. Set Reference Wingspan Override instead.");
		return false;
	}

	// A bone's reference translation is its offset from its parent, so the
	// length of a segment is read off the bone at its far end.
	const float upper_arm_length = pose[lower_arm_l].GetTranslation().Size();
	const float forearm_length   = pose[hand_l].GetTranslation().Size();

	const float shoulder_separation = FVector::Dist(
		VRPlayerScaleMath::RefBoneComponentTransform(ref_skeleton, upper_arm_l).GetTranslation(),
		VRPlayerScaleMath::RefBoneComponentTransform(ref_skeleton, upper_arm_r).GetTranslation());

	// Reference-pose units are mesh-local, so the mesh's own scale has to come
	// back in before this can be compared against a measurement taken in the
	// actor's space. Uniform scale is assumed; a non-uniform character mesh
	// has no single wingspan to speak of.
	const float mesh_scale = mesh->GetRelativeScale3D().X;

	CachedReferenceWingspan =
		(shoulder_separation + 2.0f * (upper_arm_length + forearm_length + GripOffset))
		* FMath::Abs(mesh_scale);

	if (CachedReferenceWingspan <= KINDA_SMALL_NUMBER)
	{
		CachedReferenceWingspan = -1.0f;
		out_error = LOCTEXT("DegenerateReference",
			"Calibration failed: the character's arm measured as zero length.");
		return false;
	}

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRPlayerScale, Log,
			TEXT("Reference wingspan %.1f cm (shoulders %.1f, upper %.1f, fore %.1f, grip %.1f, mesh scale %.2f)."),
			CachedReferenceWingspan, shoulder_separation, upper_arm_length, forearm_length, GripOffset, mesh_scale);
	}

	return true;
}

float UVRPlayerScaleComponent::GetReferenceWingspan()
{
	FText discarded;

	return ResolveReferenceWingspan(discarded) ? CachedReferenceWingspan : 0.0f;
}

// ── Measurement ──

bool UVRPlayerScaleComponent::SampleSpan(float& out_span) const
{
	const USceneComponent* left   = ResolvedLeftHand.Get();
	const USceneComponent* right  = ResolvedRightHand.Get();
	const USceneComponent* origin = ResolveTargetOrigin();

	if (!left || !right || !origin)
	{
		return false;
	}

	// Local space, not world. The rig may already be scaled from a previous
	// calibration, and a world-space reading would fold that scale into the
	// measurement and compound it on every recalibration.
	const FTransform origin_inverse = origin->GetComponentTransform().Inverse();

	out_span = FVector::Dist(
		origin_inverse.TransformPosition(left->GetComponentLocation()),
		origin_inverse.TransformPosition(right->GetComponentLocation()));

	return true;
}

void UVRPlayerScaleComponent::UpdateCapture(float delta_time)
{
	if (!bCapturing)
	{
		return;
	}

	CaptureElapsed += delta_time;

	float span = 0.0f;

	if (!SampleSpan(span))
	{
		FailCalibration(LOCTEXT("LostHands", "Calibration failed: lost track of the controllers."));
		return;
	}

	// Any drift past the tolerance re-anchors the window, so the span has to
	// be genuinely still rather than merely passing through a plausible value.
	if (StableAnchorSpan < 0.0f || FMath::Abs(span - StableAnchorSpan) > SampleToleranceCm)
	{
		StableAnchorSpan = span;
		StableElapsed    = 0.0f;
	}
	else
	{
		StableElapsed += delta_time;
	}

	const float alpha = SampleStableSeconds > 0.0f
		? FMath::Clamp(StableElapsed / SampleStableSeconds, 0.0f, 1.0f)
		: 1.0f;

	OnCalibrationProgress.Broadcast(alpha);

	ReportDebug(FString::Printf(TEXT("  span %.1fcm  steady %.0f%%"), span, alpha * 100.0f),
		VRPlayerScaleDebug::ProgressSlot, FColor::Yellow);

	if (StableElapsed < SampleStableSeconds)
	{
		if (CaptureTimeoutSeconds > 0.0f && CaptureElapsed >= CaptureTimeoutSeconds)
		{
			FailCalibration(LOCTEXT("Timeout",
				"Calibration timed out. Hold both arms straight out and stay still."));
		}

		return;
	}

	// Range-checked only once the pose settled. Checking every frame would
	// reject the player mid-raise, before they ever reached the pose.
	if (span < MinValidWingspan || span > MaxValidWingspan)
	{
		FailCalibration(FText::Format(
			LOCTEXT("SpanOutOfRange", "Calibration failed: measured {0} cm, which is outside the supported range."),
			FText::AsNumber(FMath::RoundToInt(span))));
		return;
	}

	bCapturing       = false;
	MeasuredWingspan = span;

	OnArmSpanMeasured.Broadcast(span);

	ReportDebug(FString::Printf(TEXT("SCALE span accepted %.1fcm"), span),
		VRPlayerScaleDebug::MeasuredSlot, FColor::Green);

	const float scale = ComputeScaleForSpan(span);

	OnScaleComputed.Broadcast(scale);

	BlendFrom    = AppliedScale;
	BlendTo      = scale;
	BlendElapsed = 0.0f;
	bBlending    = true;

	ReportDebug(FString::Printf(TEXT("SCALE %.3f  (ref %.0f / span %.0f = %.3f raw)"),
		scale, CachedReferenceWingspan, span, CachedReferenceWingspan / span),
		VRPlayerScaleDebug::AppliedSlot, FColor::Green);

	if (bLogDiagnostics)
	{
		UE_LOG(LogVRPlayerScale, Log,
			TEXT("Span %.1f cm accepted. Reference %.1f, raw ratio %.3f, applying %.3f."),
			span, CachedReferenceWingspan, CachedReferenceWingspan / span, scale);
	}

	RefreshTickEnabled();
}

// ── Scale ──

float UVRPlayerScaleComponent::ComputeScaleForSpan(float span) const
{
	if (span <= KINDA_SMALL_NUMBER || CachedReferenceWingspan <= 0.0f)
	{
		return 1.0f;
	}

	const float raw = CachedReferenceWingspan / span;

	// Taller than the character. Left inert on purpose: bringing tall players
	// down is a separate decision that has not been made, and the evidence we
	// have says it is not needed.
	if (raw <= 1.0f)
	{
		return 1.0f;
	}

	// The ceiling is derived rather than authored so that it keeps meaning the
	// same thing if the character is retargeted -- it is always "the smallest
	// player we fully support", never a bare multiplier that silently drifts
	// away from that intent.
	const float max_scale = MinSupportedWingspanCm > 0.0f
		? FMath::Max(1.0f, CachedReferenceWingspan / MinSupportedWingspanCm)
		: raw;

	return FMath::Clamp(FMath::Lerp(1.0f, raw, ChildCorrectionStrength), 1.0f, max_scale);
}

void UVRPlayerScaleComponent::UpdateBlend(float delta_time)
{
	if (!bBlending)
	{
		return;
	}

	BlendElapsed += delta_time;

	const float alpha = ApplyBlendSeconds > 0.0f
		? FMath::Clamp(BlendElapsed / ApplyBlendSeconds, 0.0f, 1.0f)
		: 1.0f;

	ApplyScaleNow(FMath::Lerp(BlendFrom, BlendTo, alpha));

	if (alpha >= 1.0f)
	{
		bBlending = false;
		OnScaleApplied.Broadcast(AppliedScale);
	}
}

void UVRPlayerScaleComponent::CacheCounterScaleTargets()
{
	if (bCounterScaleTargetsCached)
	{
		return;
	}

	ResolvedCounterScaleComponents.Reset(CounterScaleComponentNames.Num());
	AuthoredCounterScales.Reset(CounterScaleComponentNames.Num());

	for (const FName& component_name : CounterScaleComponentNames)
	{
		USceneComponent* component = FindSceneComponentByName(component_name);

		if (!component && bLogDiagnostics)
		{
			UE_LOG(LogVRPlayerScale, Warning,
				TEXT("Counter-scale target '%s' not found; it will grow with the rig."),
				*component_name.ToString());
		}

		ResolvedCounterScaleComponents.Add(component);
		AuthoredCounterScales.Add(component ? component->GetRelativeScale3D() : FVector::OneVector);
	}

	bCounterScaleTargetsCached = true;
}

void UVRPlayerScaleComponent::ApplyScaleNow(float scale)
{
	AppliedScale = scale;

	USceneComponent* origin = ResolveTargetOrigin();

	if (!origin)
	{
		return;
	}

	origin->SetRelativeScale3D(FVector(scale));

	if (scale <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Divided out of the authored scale rather than overwritten with a bare
	// reciprocal, so a hand mesh deliberately authored at something other than
	// 1 keeps its intended size instead of being flattened to the rig's.
	CacheCounterScaleTargets();

	for (int32 index = 0; index < ResolvedCounterScaleComponents.Num(); ++index)
	{
		USceneComponent* component = ResolvedCounterScaleComponents[index].Get();

		if (component && AuthoredCounterScales.IsValidIndex(index))
		{
			component->SetRelativeScale3D(AuthoredCounterScales[index] / scale);
		}
	}
}

// ── Plumbing ──

void UVRPlayerScaleComponent::FailCalibration(const FText& reason)
{
	bCapturing = false;
	RefreshTickEnabled();

	OnCalibrationFailed.Broadcast(reason);

	ReportDebug(FString::Printf(TEXT("SCALE FAILED  %s"), *reason.ToString()),
		VRPlayerScaleDebug::FailedSlot, FColor::Orange);

	UE_LOG(LogVRPlayerScale, Warning, TEXT("%s Scale left at %.3f."), *reason.ToString(), AppliedScale);
}

void UVRPlayerScaleComponent::RefreshTickEnabled()
{
	SetComponentTickEnabled(bCapturing || bBlending);
}

void UVRPlayerScaleComponent::ReportDebug(const FString& message, int32 slot, FColor color) const
{
	if (!bShowDebugOnScreen || !GEngine)
	{
		return;
	}

	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(DebugMessageKey + slot),
		VRPlayerScaleDebug::Lifetime, color, message);
}

#undef LOCTEXT_NAMESPACE
