#include "WallRunStateCleanup.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMeshActor.h"

bool UWallRunStateCleanup::ShouldCleanUpWallRun(
	UCapsuleComponent* wall_run_capsule,
	USphereComponent* base_sphere,
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

	AActor* owner = wall_run_capsule->GetOwner();

	TArray<AActor*> overlapping_actors;
	wall_run_capsule->GetOverlappingActors(overlapping_actors);

	overlapping_actors.RemoveAll([owner](AActor* actor)
	{
		return actor == owner;
	});

	if (overlapping_actors.IsEmpty())
	{
		return true;
	}

	if (base_sphere)
	{
		TArray<AActor*> base_overlapping_actors;
		base_sphere->GetOverlappingActors(base_overlapping_actors, AStaticMeshActor::StaticClass());

		base_overlapping_actors.RemoveAll([owner](AActor* actor)
		{
			return actor == owner;
		});

		if (!base_overlapping_actors.IsEmpty())
		{
			return true;
		}
	}

	return false;
}
