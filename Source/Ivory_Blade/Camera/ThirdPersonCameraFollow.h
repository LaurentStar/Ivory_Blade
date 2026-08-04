#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ThirdPersonCameraFollow.generated.h"

class USpringArmComponent;

UCLASS()
class IVORY_BLADE_API UThirdPersonCameraFollow : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Rotates the spring arm to face the character's movement direction.
	 * Call on tick. Ignores velocities below speed_threshold to prevent jitter.
	 *
	 * @param camera_boom     Spring arm to rotate.
	 * @param velocity        Character movement velocity (Z is zeroed internally).
	 * @param delta_time      Frame delta time.
	 * @param interp_speed    Rotation smoothing rate (higher = faster catch-up).
	 * @param speed_threshold Minimum horizontal speed before rotation updates.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera|ThirdPerson")
	static void UpdateCameraFollowRotation(
		USpringArmComponent* camera_boom,
		FVector velocity,
		float delta_time,
		float interp_speed    = 3.0f,
		float speed_threshold = 50.0f
	);

	UFUNCTION(BlueprintCallable, Category = "Camera|ThirdPerson")
	static void UpdateVRCameraFollow(
		USpringArmComponent* camera_boom,
		FVector vr_camera_forward,
		FVector character_velocity,
		FVector local_up,
		float delta_time,
		float follow_distance    = 100.0f,
		float interp_speed       = 2.0f,
		float speed_threshold    = 50.0f
	);

	UFUNCTION(BlueprintCallable, Category = "Camera|ThirdPerson")
	static void UpdateCameraSpringLength(
		USpringArmComponent* camera_boom,
		float target_length,
		float delta_time,
		float interp_speed   = 3.0f,
		float ease_exponent  = 2.0f
	);
};
