#include "DashImpulse.h"
#include "GameFramework/Character.h"

void UDashImpulse::ComputeAndApplyDash(
	ACharacter* character,
	FVector camera_forward,
	float camera_blend_weight,
	float impulse_strength)
{
	if (!character)
	{
		return;
	}

	const FVector capsule_forward = character->GetActorForwardVector();

	FVector blend_dir = (camera_forward * camera_blend_weight)
	                  + (capsule_forward * (1.0f - camera_blend_weight));

	if (!blend_dir.Normalize())
	{
		return;
	}

	character->LaunchCharacter(blend_dir * impulse_strength, false, false);
}
