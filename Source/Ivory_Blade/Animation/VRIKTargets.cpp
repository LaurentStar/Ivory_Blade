#include "VRIKTargets.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"

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

void UVRIKTargets::ComputeBodyIKParams(
	USceneComponent* camera,
	float standing_hmd_height,
	float max_pelvis_drop,
	float max_spine_lean_total,
	int32 spine_bone_count,
	float crouch_dead_zone,
	float crawl_threshold,
	float spine_length,
	float pelvis_share,
	float& out_crouch_factor,
	float& out_pelvis_z_offset,
	float& out_lean_per_bone,
	bool&  out_is_crawling)
{
	out_crouch_factor   = 0.0f;
	out_pelvis_z_offset = 0.0f;
	out_lean_per_bone   = 0.0f;
	out_is_crawling     = false;

	if (!camera || standing_hmd_height <= 0.0f)
	{
		return;
	}

	const float current_z = camera->GetComponentLocation().Z;
	const float raw_factor = FMath::Clamp(
		1.0f - (current_z / standing_hmd_height), 0.0f, 1.0f);

	out_crouch_factor = raw_factor;

	if (raw_factor < crouch_dead_zone)
	{
		return;
	}

	if (raw_factor >= crawl_threshold)
	{
		out_is_crawling = true;
		return;
	}

	const float ik_range = crawl_threshold - crouch_dead_zone;
	if (ik_range <= 0.0f)
	{
		return;
	}

	const float effective = (raw_factor - crouch_dead_zone) / ik_range;

	const int32 bone_count = FMath::Max(spine_bone_count, 1);
	out_lean_per_bone = (effective * max_spine_lean_total) / static_cast<float>(bone_count);

	const float actual_drop = standing_hmd_height - current_z;
	const float lean_radians = FMath::DegreesToRadians(
		effective * max_spine_lean_total);
	const float spine_height_reduction =
		spine_length * (1.0f - FMath::Cos(lean_radians));
	const float needed_drop = actual_drop - spine_height_reduction;

	out_pelvis_z_offset = -FMath::Clamp(needed_drop * pelvis_share, 0.0f, max_pelvis_drop);
}

FVector UVRIKTargets::ComputeFootPlantTarget(
	USkeletalMeshComponent* mesh,
	FName foot_bone_name,
	float ground_offset)
{
	if (!mesh)
	{
		return FVector::ZeroVector;
	}

	FVector foot_cs = mesh->GetBoneLocation(
		foot_bone_name, EBoneSpaces::ComponentSpace);

	const FVector root_cs = mesh->GetBoneLocation(
		NAME_None, EBoneSpaces::ComponentSpace);

	foot_cs.Z = root_cs.Z + ground_offset;

	return foot_cs;
}
