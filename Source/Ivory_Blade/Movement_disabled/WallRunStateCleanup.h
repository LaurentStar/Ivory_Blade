#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WallRunStateCleanup.generated.h"

class UCapsuleComponent;
class USphereComponent;

UCLASS()
class IVORY_BLADE_API UWallRunStateCleanup : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Movement|WallRun")
	static bool ShouldCleanUpWallRun(
		UCapsuleComponent* wall_run_capsule,
		USphereComponent* base_sphere,
		bool is_ready_wall_running
	);
};
