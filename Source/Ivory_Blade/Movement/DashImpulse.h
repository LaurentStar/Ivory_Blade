#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DashImpulse.generated.h"

UCLASS()
class IVORY_BLADE_API UDashImpulse : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Movement|Dash")
	static void ComputeAndApplyDash(
		ACharacter* character,
		FVector camera_forward,
		float camera_blend_weight = 0.5f,
		float impulse_strength    = 2000.0f
	);
};
