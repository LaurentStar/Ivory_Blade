#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VRIKTargets.generated.h"

UCLASS()
class IVORY_BLADE_API UVRIKTargets : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Converts a motion controller's world transform into the skeletal mesh's
	 * component space, suitable for feeding into a Two Bone IK effector.
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|VRIK")
	static FTransform ComputeHandIKTarget(
		USkeletalMeshComponent* mesh,
		USceneComponent* motion_controller
	);

	/**
	 * Computes an elbow joint-target position in component space so the
	 * Two Bone IK solver bends elbows naturally (backward and outward).
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|VRIK")
	static FVector ComputeJointTarget(
		USkeletalMeshComponent* mesh,
		FName shoulder_bone_name,
		FVector hand_location_cs,
		bool is_left_arm,
		float offset_back    = 30.0f,
		float offset_outward = 20.0f
	);

	UFUNCTION(BlueprintCallable, Category = "Animation|VRIK")
	static float InterpIKBlendWeight(
		float current_weight,
		float target_weight,
		float delta_time,
		float interp_speed = 8.0f
	);

	/**
	 * Converts the VR camera's world transform into the skeletal mesh's
	 * component space, providing position and rotation targets for
	 * spine FABRIK and head rotation override.
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|VRIK")
	static void ComputeHeadIKTarget(
		USkeletalMeshComponent* mesh,
		USceneComponent* camera,
		FVector& out_location_cs,
		FRotator& out_rotation_cs
	);
};
