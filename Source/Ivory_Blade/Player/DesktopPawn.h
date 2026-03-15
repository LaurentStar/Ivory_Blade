#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DesktopPawn.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * A minimal first-person pawn for desktop Play-in-Editor testing.
 * Uses keyboard (WASD), mouse look, and spacebar jump so that
 * gameplay systems can be iterated on without a VR headset.
 *
 * Create a Blueprint child class (BP_DesktopPawn) and assign the
 * Input Action / IMC references in its Class Defaults.
 */
UCLASS()
class IVORY_BLADE_API ADesktopPawn : public ACharacter
{
	GENERATED_BODY()

public:
	ADesktopPawn();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// --- Components ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// --- Enhanced Input ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	// --- Movement tuning ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	float LookSensitivity = 1.0f;

private:
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
};
