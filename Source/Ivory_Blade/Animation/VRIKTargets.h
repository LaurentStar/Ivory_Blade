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
	 * head rotation override.
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|VRIK")
	static void ComputeHeadIKTarget(
		USkeletalMeshComponent* mesh,
		USceneComponent* camera,
		FVector& out_location_cs,
		FRotator& out_rotation_cs
	);

	/**
	 * Derives pelvis drop, spine lean, and crawl flag from the HMD's
	 * current height relative to a calibrated standing height.
	 *
	 * Three zones (controlled by crouch_dead_zone and crawl_threshold):
	 *   Standing  — outputs zero, pure locomotion animation
	 *   Crouching — pelvis drops, spine leans forward, legs IK-bend
	 *   Crawling  — outputs zero, signals crawl animation takeover
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|VRIK")
	static void ComputeBodyIKParams(
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
		bool&  out_is_crawling
	);

	/**
	 * Returns the foot bone's animated X/Y position with Z clamped to
	 * ground level in component space. Keeps feet planted when the
	 * pelvis drops.
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|VRIK")
	static FVector ComputeFootPlantTarget(
		USkeletalMeshComponent* mesh,
		FName foot_bone_name,
		float ground_offset = 2.0f
	);
};
