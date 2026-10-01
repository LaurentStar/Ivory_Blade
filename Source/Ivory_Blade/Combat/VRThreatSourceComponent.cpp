// VRThreatSourceComponent.cpp
//
// Placed on every attack actor. Creates (or reuses) an alert-box overlap sphere
// so the dodge system knows the player is in range. Caches the sibling trajectory
// component and exposes a per-attack DodgeDistance for the dodge to use.

#include "VRThreatSourceComponent.h"
#include "VRAttackTrajectoryComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ShapeComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogVRThreat);

UVRThreatSourceComponent::UVRThreatSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVRThreatSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* owner = GetOwner();

	if (!owner)
	{
		return;
	}

	// ── Cache the trajectory component ──
	CachedTrajectory = owner->FindComponentByClass<UVRAttackTrajectoryComponent>();

	if (!CachedTrajectory)
	{
		UE_LOG(LogVRThreat, Warning,
			TEXT("%s: No UVRAttackTrajectoryComponent found. Dodge assist will have no "
			     "direction data for this attack."),
			*GetNameSafe(owner));
	}

	// ── Find or create the alert-box overlap sphere ──
	// Look for a manually placed sphere tagged "AlertBox" first (viewport-editable).
	TArray<USphereComponent*> spheres;
	owner->GetComponents<USphereComponent>(spheres);

	for (USphereComponent* sphere : spheres)
	{
		if (sphere && sphere->ComponentHasTag(TEXT("AlertBox")))
		{
			AlertBoxSphere = sphere;
			bAlertBoxOwned = false;

			UE_LOG(LogVRThreat, Log,
				TEXT("%s: Using existing AlertBox sphere (radius:%.0f). DodgeDist:%.0f HasTrajectory:%s"),
				*GetNameSafe(owner), sphere->GetScaledSphereRadius(), DodgeDistance,
				CachedTrajectory ? TEXT("yes") : TEXT("no"));
			break;
		}
	}

	// If none was placed manually, create one at runtime from AlertRadius.
	if (!AlertBoxSphere)
	{
		AlertBoxSphere = NewObject<USphereComponent>(owner, TEXT("AlertBox"));

		if (AlertBoxSphere)
		{
			AlertBoxSphere->SetSphereRadius(AlertRadius);
			AlertBoxSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			AlertBoxSphere->SetCollisionObjectType(ECC_WorldDynamic);
			AlertBoxSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
			AlertBoxSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
			AlertBoxSphere->SetGenerateOverlapEvents(true);
			AlertBoxSphere->SetCanEverAffectNavigation(false);
			AlertBoxSphere->ComponentTags.Add(TEXT("AlertBox"));
			AlertBoxSphere->SetHiddenInGame(true);

			AlertBoxSphere->SetupAttachment(owner->GetRootComponent());
			AlertBoxSphere->RegisterComponent();
			bAlertBoxOwned = true;

			UE_LOG(LogVRThreat, Log,
				TEXT("%s: Alert box auto-created. Radius:%.0f DodgeDist:%.0f HasTrajectory:%s"),
				*GetNameSafe(owner), AlertRadius, DodgeDistance,
				CachedTrajectory ? TEXT("yes") : TEXT("no"));
		}
		else
		{
			UE_LOG(LogVRThreat, Error,
				TEXT("%s: Failed to create alert-box sphere component."),
				*GetNameSafe(owner));
		}
	}
}

void UVRThreatSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AlertBoxSphere && bAlertBoxOwned)
	{
		AlertBoxSphere->DestroyComponent();
	}

	AlertBoxSphere = nullptr;
	CachedTrajectory = nullptr;

	Super::EndPlay(EndPlayReason);
}

float UVRThreatSourceComponent::DetectRadius() const
{
	const AActor* owner = GetOwner();

	if (!owner)
	{
		return 25.0f;
	}

	TArray<UShapeComponent*> shapes;
	owner->GetComponents<UShapeComponent>(shapes);

	for (const UShapeComponent* shape : shapes)
	{
		if (!shape || shape == AlertBoxSphere)
		{
			continue;
		}

		if (const USphereComponent* sphere = Cast<USphereComponent>(shape))
		{
			return sphere->GetScaledSphereRadius();
		}

		if (const UCapsuleComponent* capsule = Cast<UCapsuleComponent>(shape))
		{
			return capsule->GetScaledCapsuleRadius();
		}

		return shape->Bounds.SphereRadius;
	}

	UE_LOG(LogVRThreat, Warning,
		TEXT("%s has no UShapeComponent (other than alert box). Using fallback radius of 25 cm."),
		*GetNameSafe(owner));

	return 25.0f;
}
