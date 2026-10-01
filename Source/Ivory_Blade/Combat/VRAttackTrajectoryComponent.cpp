// VRAttackTrajectoryComponent.cpp
//
// Ordered waypoint chain describing an attack's travel path. Projectiles
// auto-generate a 2-point chain from velocity in BeginPlay. Melee attacks
// author waypoints manually or from animation notifies. Ticks each frame to
// advance the current segment index so the dodge system knows which direction
// the hurtbox is heading right now.

#include "VRAttackTrajectoryComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogVRTrajectory);

UVRAttackTrajectoryComponent::UVRAttackTrajectoryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// ── Lifecycle ──

void UVRAttackTrajectoryComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoInitFromProjectile && Waypoints.Num() == 0)
	{
		if (UProjectileMovementComponent* projectile =
				GetOwner() ? GetOwner()->FindComponentByClass<UProjectileMovementComponent>() : nullptr)
		{
			InitFromProjectile(projectile);
		}
		else
		{
			UE_LOG(LogVRTrajectory, Warning,
				TEXT("%s: bAutoInitFromProjectile is true but no ProjectileMovementComponent found. "
				     "Waypoints must be set manually or via SetWaypoints()."),
				*GetNameSafe(GetOwner()));
		}
	}

	if (Waypoints.Num() < 2)
	{
		UE_LOG(LogVRTrajectory, Warning,
			TEXT("%s: Trajectory has %d waypoint(s). At least 2 are needed for a valid segment."),
			*GetNameSafe(GetOwner()), Waypoints.Num());
	}
}

void UVRAttackTrajectoryComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Waypoints.Num() < 2)
	{
		return;
	}

	ElapsedTime += DeltaTime;
	AdvanceIndex();

#if ENABLE_DRAW_DEBUG
	if (bLogDiagnostics)
	{
		const FVector current_dir = GetCurrentDirection();
		const FVector current_pos = GetCurrentPosition();

		UE_LOG(LogVRTrajectory, Log,
			TEXT("%s Seg:%d/%d t:%.3f Pos:(%.0f,%.0f,%.0f) Dir:(%.2f,%.2f,%.2f) Finished:%s"),
			*GetNameSafe(GetOwner()),
			CurrentIndex, Waypoints.Num() - 1,
			ElapsedTime,
			current_pos.X, current_pos.Y, current_pos.Z,
			current_dir.X, current_dir.Y, current_dir.Z,
			HasFinished() ? TEXT("yes") : TEXT("no"));

		if (GetWorld())
		{
			for (int32 i = 0; i < Waypoints.Num() - 1; ++i)
			{
				const FColor color = (i == CurrentIndex) ? FColor::Orange : FColor::Cyan;
				DrawDebugLine(GetWorld(),
					Waypoints[i].Position, Waypoints[i + 1].Position,
					color, false, 0.0f, 0, 2.0f);
			}

			DrawDebugSphere(GetWorld(), current_pos, 8.0f, 6, FColor::Yellow, false, 0.0f, 0, 1.5f);
		}
	}
#endif
}

// ── Initialisation ──

// Builds a 2-waypoint chain from current position + velocity * lifetime.
void UVRAttackTrajectoryComponent::InitFromProjectile(UProjectileMovementComponent* projectile)
{
	if (!projectile)
	{
		UE_LOG(LogVRTrajectory, Warning,
			TEXT("%s: InitFromProjectile called with null projectile."),
			*GetNameSafe(GetOwner()));
		return;
	}

	const AActor* owner = GetOwner();
	const FVector start = owner ? owner->GetActorLocation() : FVector::ZeroVector;
	const FVector velocity = projectile->Velocity;
	const float speed = velocity.Size();

	if (speed < KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogVRTrajectory, Warning,
			TEXT("%s: Projectile velocity is near zero. Cannot build trajectory."),
			*GetNameSafe(GetOwner()));
		return;
	}

	const FVector end = start + velocity * ProjectileLifetime;

	Waypoints.Empty(2);

	FVRAttackWaypoint wp0;
	wp0.Position = start;
	wp0.Time = 0.0f;
	Waypoints.Add(wp0);

	FVRAttackWaypoint wp1;
	wp1.Position = end;
	wp1.Time = ProjectileLifetime;
	Waypoints.Add(wp1);

	CurrentIndex = 0;
	ElapsedTime = 0.0f;

	UE_LOG(LogVRTrajectory, Log,
		TEXT("%s: Trajectory from projectile. Start:(%.0f,%.0f,%.0f) End:(%.0f,%.0f,%.0f) "
		     "Speed:%.0f Lifetime:%.1fs"),
		*GetNameSafe(GetOwner()),
		start.X, start.Y, start.Z,
		end.X, end.Y, end.Z,
		speed, ProjectileLifetime);
}

void UVRAttackTrajectoryComponent::SetWaypoints(const TArray<FVRAttackWaypoint>& new_waypoints)
{
	Waypoints = new_waypoints;
	CurrentIndex = 0;
	ElapsedTime = 0.0f;

	UE_LOG(LogVRTrajectory, Log,
		TEXT("%s: Waypoints set (%d points)."),
		*GetNameSafe(GetOwner()), Waypoints.Num());
}

// ── Queries ──

FVector UVRAttackTrajectoryComponent::GetCurrentDirection() const
{
	if (Waypoints.Num() < 2)
	{
		return FVector::ForwardVector;
	}

	const int32 safe_index = FMath::Clamp(CurrentIndex, 0, Waypoints.Num() - 2);
	const FVector from = Waypoints[safe_index].Position;
	const FVector to = Waypoints[safe_index + 1].Position;

	FVector dir = to - from;

	if (!dir.Normalize())
	{
		return FVector::ForwardVector;
	}

	return dir;
}

// Interpolated position along the current segment based on elapsed time.
FVector UVRAttackTrajectoryComponent::GetCurrentPosition() const
{
	if (Waypoints.Num() == 0)
	{
		return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	}

	if (Waypoints.Num() == 1 || HasFinished())
	{
		return Waypoints.Last().Position;
	}

	const int32 safe_index = FMath::Clamp(CurrentIndex, 0, Waypoints.Num() - 2);
	const float seg_start = Waypoints[safe_index].Time;
	const float seg_end = Waypoints[safe_index + 1].Time;
	const float seg_duration = seg_end - seg_start;

	float alpha = 0.0f;

	if (seg_duration > KINDA_SMALL_NUMBER)
	{
		alpha = FMath::Clamp((ElapsedTime - seg_start) / seg_duration, 0.0f, 1.0f);
	}

	return FMath::Lerp(Waypoints[safe_index].Position, Waypoints[safe_index + 1].Position, alpha);
}

// Distance to target divided by current segment speed.
float UVRAttackTrajectoryComponent::GetTimeToArrival(FVector world_position) const
{
	if (Waypoints.Num() < 2 || HasFinished())
	{
		return -1.0f;
	}

	const int32 safe_index = FMath::Clamp(CurrentIndex, 0, Waypoints.Num() - 2);
	const FVector from = Waypoints[safe_index].Position;
	const FVector to = Waypoints[safe_index + 1].Position;

	const FVector segment = to - from;
	const float seg_length = segment.Size();

	if (seg_length < KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float seg_start_time = Waypoints[safe_index].Time;
	const float seg_end_time = Waypoints[safe_index + 1].Time;
	const float seg_duration = seg_end_time - seg_start_time;

	if (seg_duration < KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float speed = seg_length / seg_duration;
	const float distance_to_target = FVector::Dist(GetCurrentPosition(), world_position);

	return distance_to_target / speed;
}

bool UVRAttackTrajectoryComponent::HasFinished() const
{
	if (Waypoints.Num() < 2)
	{
		return true;
	}

	return ElapsedTime >= Waypoints.Last().Time;
}

// ── Internal ──

void UVRAttackTrajectoryComponent::AdvanceIndex()
{
	while (CurrentIndex < Waypoints.Num() - 2 &&
	       ElapsedTime >= Waypoints[CurrentIndex + 1].Time)
	{
		++CurrentIndex;
	}
}
