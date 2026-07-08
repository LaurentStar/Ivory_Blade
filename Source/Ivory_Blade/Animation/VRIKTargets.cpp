#include "VRIKTargets.h"
#include "Components/SkeletalMeshComponent.h"

FTransform UVRIKTargets::ComputeHandIKTarget(
	USkeletalMeshComponent* mesh,
	USceneComponent* motion_controller)
{
	if (!mesh || !motion_controller)
	{
		return FTransform::Identity;
	}

	const FTransform controller_world = motion_controller->GetComponentTransform();
	const FTransform mesh_world = mesh->GetComponentTransform();

	return controller_world.GetRelativeTransform(mesh_world);
}

FVector UVRIKTargets::ComputeJointTarget(
	USkeletalMeshComponent* mesh,
	FName shoulder_bone_name,
	FVector hand_location_cs,
	bool is_left_arm,
	float offset_back,
	float offset_outward)
{
	if (!mesh)
	{
		return FVector::ZeroVector;
	}

	const FVector shoulder_cs = mesh->GetBoneLocation(
		shoulder_bone_name, EBoneSpaces::ComponentSpace);

	const FVector midpoint = (shoulder_cs + hand_location_cs) * 0.5f;

	const float outward_sign = is_left_arm ? -1.0f : 1.0f;

	return FVector(
		midpoint.X - offset_back,
		midpoint.Y + (offset_outward * outward_sign),
		midpoint.Z
	);
}

float UVRIKTargets::InterpIKBlendWeight(
	float current_weight,
	float target_weight,
	float delta_time,
	float interp_speed)
{
	return FMath::Clamp(
		FMath::FInterpTo(current_weight, target_weight, delta_time, interp_speed),
		0.0f,
		1.0f);
}

void UVRIKTargets::ComputeHeadIKTarget(
	USkeletalMeshComponent* mesh,
	USceneComponent* camera,
	FVector& out_location_cs,
	FRotator& out_rotation_cs)
{
	if (!mesh || !camera)
	{
		out_location_cs = FVector::ZeroVector;
		out_rotation_cs = FRotator::ZeroRotator;
		return;
	}

	const FTransform mesh_world = mesh->GetComponentTransform();
	const FTransform camera_world = camera->GetComponentTransform();

	out_location_cs = mesh_world.InverseTransformPosition(
		camera_world.GetLocation());

	const FQuat mesh_quat = mesh_world.GetRotation();
	const FQuat camera_quat = camera_world.GetRotation();
	out_rotation_cs = (mesh_quat.Inverse() * camera_quat).Rotator();
}
