#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WallRunStateCleanup.generated.h"

class UCapsuleComponent;

UCLASS()
class IVORY_BLADE_API UWallRunStateCleanup : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Movement|WallRun")
	static bool ShouldCleanUpWallRun(
		UCapsuleComponent* wall_run_capsule,
		bool is_ready_wall_running
	);
};
