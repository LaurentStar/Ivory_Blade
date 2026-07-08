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
};
