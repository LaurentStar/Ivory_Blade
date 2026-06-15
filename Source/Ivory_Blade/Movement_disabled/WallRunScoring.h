#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WallRunScoring.generated.h"

UCLASS()
class IVORY_BLADE_API UWallRunScoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Movement|WallRun")
	static void ComputeWallRunScores(
		float input_direction_dot_product,
		float camera_forward_dot_product,
		float character_direction_dot_product,
		float current_speed,
		float fall_speed,
		float& horizontal_score,
		float& vertical_score,
		float input_direction_weight      = 8.0f,
		float camera_forward_weight       = 6.0f,
		float character_direction_weight   = 9.0f,
		float speed_weight                = 9.0f,
		float fall_speed_weight_true      = 2.0f,
		float fall_speed_weight_false     = 15.0f
	);
};
