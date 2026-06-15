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
};
