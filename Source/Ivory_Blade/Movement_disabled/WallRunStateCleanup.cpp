#include "WallRunStateCleanup.h"
#include "Components/CapsuleComponent.h"

bool UWallRunStateCleanup::ShouldCleanUpWallRun(
	UCapsuleComponent* wall_run_capsule,
	bool is_ready_wall_running)
{
	if (!is_ready_wall_running)
	{
		return false;
	}

	if (!wall_run_capsule)
	{
		return false;
	}

	TArray<AActor*> overlapping_actors;
	wall_run_capsule->GetOverlappingActors(overlapping_actors);

	return overlapping_actors.IsEmpty();
}
