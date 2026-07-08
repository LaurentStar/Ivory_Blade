#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PrefabChildFinder.generated.h"

UCLASS()
class IVORY_BLADE_API UPrefabChildFinder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UI|Prefab")
	static bool FindAdvanceModeUIElements(
		AActor* spawned_ui,
		AActor*& out_frame,
		AActor*& out_spikes,
		AActor*& out_bar,
		AActor*& out_reticle
	);
};
