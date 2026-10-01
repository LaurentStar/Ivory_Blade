#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VRThreatSourceComponent.generated.h"

class UVRDodgeOffsetComponent;
class UVRAttackTrajectoryComponent;
class USphereComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogVRThreat, Log, All);

/**
 * Placed on every attack actor (projectile or melee hurtbox). Responsibilities:
 *
 * 1. **Alert box**: creates a USphereComponent overlap sphere tagged "AlertBox"
 *    that tells the dodge system the player is in range of this attack.
 *    Radius = AlertRadius (set by the developer per attack).
 *
 * 2. **Trajectory reference**: caches the sibling UVRAttackTrajectoryComponent
 *    so the dodge system can read the attack's travel direction and ETA.
 *
 * 3. **Dodge distance**: DodgeDistance is the displacement the player should
 *    cover when dodging this attack. Set by the developer per attack type.
 *
 * No tick. The alert box is a passive overlap and the trajectory component
 * handles its own time tracking.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class IVORY_BLADE_API UVRThreatSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVRThreatSourceComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ── Queries ──

	/** The trajectory component on this attack actor, if any. */
	UFUNCTION(BlueprintPure, Category = "Threat")
	UVRAttackTrajectoryComponent* GetTrajectory() const { return CachedTrajectory; }

	/** The runtime alert-box sphere. Null before BeginPlay. */
	UFUNCTION(BlueprintPure, Category = "Threat")
	USphereComponent* GetAlertBox() const { return AlertBoxSphere; }

	// ── Configuration ──

	/**
	 * Radius of the alert-box overlap sphere, in cm. The player must be inside
	 * this sphere for a dodge to be accepted. Set per attack type — larger for
	 * fast projectiles, smaller for melee.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threat",
	          meta = (ClampMin = "1.0"))
	float AlertRadius = 300.0f;

	/**
	 * How far the player should dodge when reacting to this attack, in cm.
	 * This is the distance passed to VRDodgeOffsetComponent::BeginDodge.
	 * The developer tunes this per attack so the player clears the hurtbox.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threat",
	          meta = (ClampMin = "0.0"))
	float DodgeDistance = 40.0f;

	/**
	 * Collision-shape radius of the hurtbox itself. Leave at 0 to auto-detect
	 * from the first UShapeComponent on the owning actor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threat",
	          meta = (ClampMin = "0.0"))
	float ThreatRadiusOverride = 0.0f;

private:
	float DetectRadius() const;

	UPROPERTY()
	TObjectPtr<USphereComponent> AlertBoxSphere;

	UPROPERTY()
	TObjectPtr<UVRAttackTrajectoryComponent> CachedTrajectory;

	/** True when this component created the sphere at runtime. False when reusing a manually placed one. */
	bool bAlertBoxOwned = false;
};
