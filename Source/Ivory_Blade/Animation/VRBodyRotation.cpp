#include "VRBodyRotation.h"
#include "Components/SkeletalMeshComponent.h"

void UVRBodyRotation::UpdateBodyYaw(
	USkeletalMeshComponent* mesh,
	FVector velocity,
	FVector camera_forward,
	FVector local_up,
	float delta_time,
	bool is_first_person,
	float mesh_yaw_offset,
	float interp_speed,
	float speed_threshold)
{
	if (!mesh || !mesh->GetAttachParent())
	{
		return;
	}

	if (!local_up.Normalize())
	{
		return;
	}

	FVector world_facing;

	if (is_first_person)
	{
		world_facing = camera_forward
			- (FVector::DotProduct(camera_forward, local_up) * local_up);
	}
	else
	{
		world_facing = velocity
			- (FVector::DotProduct(velocity, local_up) * local_up);

		if (world_facing.Size() <= speed_threshold)
		{
			return;
		}
	}

	if (!world_facing.Normalize())
	{
		return;
	}

	const FTransform parent_transform =
		mesh->GetAttachParent()->GetComponentTransform();
	FVector local_facing =
		parent_transform.InverseTransformVectorNoScale(world_facing);
	local_facing.Z = 0.0f;

	if (local_facing.IsNearlyZero())
	{
		return;
	}

	const float target_yaw = FMath::Atan2(local_facing.Y, local_facing.X)
		* (180.0f / PI) + mesh_yaw_offset;

	const FRotator target_relative(0.0f, target_yaw, 0.0f);
	const FRotator current_relative = mesh->GetRelativeRotation();
	const FRotator new_relative = FMath::RInterpTo(
		current_relative, target_relative, delta_time, interp_speed);

	mesh->SetRelativeRotation(new_relative);
}
