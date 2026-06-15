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
