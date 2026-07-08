#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VRBodyRotation.generated.h"

UCLASS()
class IVORY_BLADE_API UVRBodyRotation : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Animation|VRBody")
	static void UpdateBodyYaw(
		USkeletalMeshComponent* mesh,
		FVector velocity,
		FVector camera_forward,
		FVector local_up,
		float delta_time,
		bool is_first_person    = true,
		float mesh_yaw_offset   = -90.0f,
		float interp_speed      = 5.0f,
		float speed_threshold   = 50.0f
	);
};
