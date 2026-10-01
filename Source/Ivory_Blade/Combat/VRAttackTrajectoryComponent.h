#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VRAttackTrajectoryComponent.generated.h"

class UProjectileMovementComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogVRTrajectory, Log, All);

/**
 * A single waypoint along an attack's travel path.
 *
 * Position is world-space. Time is seconds after the attack started — when the
 * hurtbox is expected to reach this position.
 */
USTRUCT(BlueprintType)
struct FVRAttackWaypoint
{
	GENERATED_BODY()

	/** World-space position of this waypoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	FVector Position = FVector::ZeroVector;

	/**
	 * Seconds after attack start when the hurtbox reaches this position.
	 * Waypoint 0 is typically Time = 0 (the launch point).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory",
	          meta = (ClampMin = "0.0"))
	float Time = 0.0f;
};

/**
 * Describes the path a hurtbox travels as an ordered chain of waypoints.
 *
 * Projectiles: call InitFromProjectile() in BeginPlay (or let the component
 * auto-detect a sibling ProjectileMovementComponent). This builds a 2-waypoint
 * chain from current position + velocity.
 *
 * Melee: author the waypoints in the Blueprint details panel or populate them
 * from animation notify data at runtime.
 *
 * Each frame the component advances ElapsedTime and updates CurrentIndex so
 * consumers can read which segment the hurtbox is currently on and which
 * direction it is heading.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class IVORY_BLADE_API UVRAttackTrajectoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVRAttackTrajectoryComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// ── Initialisation ──

	/**
	 * Builds a 2-waypoint chain from a ProjectileMovementComponent's current
	 * position and velocity. The second waypoint is placed at
	 * position + velocity * ProjectileLifetime.
	 *
	 * Replaces any existing waypoints.
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory")
	void InitFromProjectile(UProjectileMovementComponent* projectile);

	/**
	 * Replaces the waypoint chain at runtime. Resets elapsed time and current
	 * index. Use for melee attacks whose path is determined by animation data.
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory")
	void SetWaypoints(const TArray<FVRAttackWaypoint>& new_waypoints);

	// ── Queries ──

	/** Normalized direction the hurtbox is currently travelling (current → next waypoint). */
	UFUNCTION(BlueprintPure, Category = "Trajectory")
	FVector GetCurrentDirection() const;

	/**
	 * Estimated seconds until the hurtbox reaches the given world position,
	 * based on the current segment's speed. Returns -1 if the chain is empty
	 * or the attack has passed all waypoints.
	 */
	UFUNCTION(BlueprintPure, Category = "Trajectory")
	float GetTimeToArrival(FVector world_position) const;

	/** World-space position the hurtbox is estimated to be at right now, interpolated along the current segment. */
	UFUNCTION(BlueprintPure, Category = "Trajectory")
	FVector GetCurrentPosition() const;

	/** Index of the segment the hurtbox is currently on (0 = first segment). */
	UFUNCTION(BlueprintPure, Category = "Trajectory")
	int32 GetCurrentIndex() const { return CurrentIndex; }

	/** True once ElapsedTime has passed the last waypoint's time. */
	UFUNCTION(BlueprintPure, Category = "Trajectory")
	bool HasFinished() const;

	/** Number of waypoints in the chain. */
	UFUNCTION(BlueprintPure, Category = "Trajectory")
	int32 GetWaypointCount() const { return Waypoints.Num(); }

	/** Read-only access to the waypoint array. */
	const TArray<FVRAttackWaypoint>& GetWaypoints() const { return Waypoints; }

	// ── Configuration ──

	/**
	 * The ordered waypoint chain. For projectiles this is auto-generated in
	 * BeginPlay. For melee attacks, author it in the Blueprint details panel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	TArray<FVRAttackWaypoint> Waypoints;

	/**
	 * Assumed lifetime for projectile auto-generation. The second waypoint is
	 * placed at position + velocity * this value.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory",
	          meta = (ClampMin = "0.01"))
	float ProjectileLifetime = 5.0f;

	/**
	 * If true, BeginPlay will look for a sibling ProjectileMovementComponent
	 * and call InitFromProjectile automatically. Set to false for melee attacks
	 * where waypoints are authored or set from animation notifies.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	bool bAutoInitFromProjectile = true;

	/** Logs trajectory state each tick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory|Diagnostics")
	bool bLogDiagnostics = false;

private:
	/** Which segment (Waypoints[i] → Waypoints[i+1]) the hurtbox is currently on. */
	int32 CurrentIndex = 0;

	/** Seconds since the attack started. */
	float ElapsedTime = 0.0f;

	/** Advances CurrentIndex when ElapsedTime passes the next waypoint's time. */
	void AdvanceIndex();
};
