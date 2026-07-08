#include "ThirdPersonCameraFollow.h"
#include "GameFramework/SpringArmComponent.h"

void UThirdPersonCameraFollow::UpdateCameraFollowRotation(
	USpringArmComponent* camera_boom,
	FVector velocity,
	float delta_time,
	float interp_speed,
	float speed_threshold)
{
	if (!camera_boom)
	{
		return;
	}

	velocity.Z = 0.0f;

	if (velocity.Size2D() <= speed_threshold)
	{
		return;
	}

	const FRotator target_rotation = FRotationMatrix::MakeFromX(velocity).Rotator();
	const FRotator current_rotation = camera_boom->GetComponentRotation();
	const FRotator new_rotation = FMath::RInterpTo(current_rotation, target_rotation,
	                                                delta_time, interp_speed);

	camera_boom->SetWorldRotation(new_rotation);
}

void UThirdPersonCameraFollow::UpdateVRCameraFollow(
	USpringArmComponent* camera_boom,
	FVector vr_camera_forward,
	FVector character_velocity,
	FVector local_up,
	float delta_time,
	float follow_distance,
	float interp_speed,
	float speed_threshold)
{
	if (!camera_boom)
	{
		return;
	}

	if (!local_up.Normalize())
	{
		return;
	}

	const FVector horizontal_velocity = character_velocity
		- (FVector::DotProduct(character_velocity, local_up) * local_up);

	if (horizontal_velocity.Size() <= speed_threshold)
	{
		return;
	}

	FVector horizontal_forward = vr_camera_forward
		- (FVector::DotProduct(vr_camera_forward, local_up) * local_up);

	if (!horizontal_forward.Normalize())
	{
		return;
	}

	const FRotator target_rotation = FRotationMatrix::MakeFromXZ(
		horizontal_forward, local_up).Rotator();
	const FRotator current_rotation = camera_boom->GetComponentRotation();

	camera_boom->SetWorldRotation(
		FMath::RInterpTo(current_rotation, target_rotation, delta_time, interp_speed));

	camera_boom->TargetArmLength = follow_distance;
}
