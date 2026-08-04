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

}

void UThirdPersonCameraFollow::UpdateCameraSpringLength(
	USpringArmComponent* camera_boom,
	float target_length,
	float delta_time,
	float interp_speed,
	float ease_exponent)
{
	if (!camera_boom)
	{
		return;
	}

	const float current = camera_boom->TargetArmLength;
	const float total_range = FMath::Max(FMath::Abs(target_length), 1.0f);
	const float distance_to_target = FMath::Abs(target_length - current);

	// 0 = just started (far from target), 1 = arrived (at target)
	const float progress = 1.0f - FMath::Clamp(distance_to_target / total_range, 0.0f, 1.0f);

	// ease_exponent controls ease-in; FInterpTo naturally provides ease-out
	const float ease_factor = FMath::Max(FMath::Pow(progress, 1.0f / ease_exponent), 0.05f);

	camera_boom->TargetArmLength = FMath::FInterpTo(
		current, target_length, delta_time, interp_speed * ease_factor);
}
