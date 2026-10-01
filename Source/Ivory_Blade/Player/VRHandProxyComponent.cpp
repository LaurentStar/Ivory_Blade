#include "VRHandProxyComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MotionControllerComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY(LogVRHandProxy);

namespace
{
	const TCHAR* kSphereMeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");

	// Stand-in so the spheres are coloured and obviously working on the first run.
	// It is lit, not emissive, so swap in an unlit material for the actual glow.
	const TCHAR* kFallbackMaterialPath =
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
}

UVRHandProxyComponent::UVRHandProxyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// The viewpoint is only final once the camera has been updated for the frame.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> sphere(kSphereMeshPath);
	if (sphere.Succeeded())
	{
		ProxyMesh = sphere.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> fallback_material(
		kFallbackMaterialPath);
	if (fallback_material.Succeeded())
	{
		GlowMaterial = fallback_material.Object;
	}

	TemplateHandComponentNames = { FName("HandLeft"), FName("HandRight") };

	ProxyLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandProxyLeft"));
	ProxyRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandProxyRight"));

	if (ProxyLeft)
	{
		ProxyLeft->SetupAttachment(this);
	}
	if (ProxyRight)
	{
		ProxyRight->SetupAttachment(this);
	}

	ConfigureProxy(ProxyLeft);
	ConfigureProxy(ProxyRight);
}

void UVRHandProxyComponent::ConfigureProxy(UStaticMeshComponent* proxy) const
{
	if (!proxy)
	{
		return;
	}

	if (ProxyMesh)
	{
		proxy->SetStaticMesh(ProxyMesh);
	}

	proxy->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	proxy->SetGenerateOverlapEvents(false);
	proxy->SetCastShadow(false);
	proxy->bReceivesDecals = false;

	// The spheres are positioned in world space every frame. Absolute transforms
	// mean the pawn's capsule rotating to a planetoid surface cannot drag them.
	proxy->SetUsingAbsoluteLocation(true);
	proxy->SetUsingAbsoluteRotation(true);
	proxy->SetUsingAbsoluteScale(true);

	proxy->SetVisibility(false);
}

void UVRHandProxyComponent::EnsureProxy(TObjectPtr<UStaticMeshComponent>& proxy,
                                        const TCHAR* fallback_name)
{
	// Subobjects created in a component constructor normally survive into the
	// instance, but they are not added to the owning actor's component list, so
	// registration has to be confirmed. Recreating from scratch covers the case
	// where the subobject did not survive serialization at all.
	if (!proxy)
	{
		proxy = NewObject<UStaticMeshComponent>(this, FName(fallback_name));
		if (proxy)
		{
			proxy->SetupAttachment(this);
		}
	}

	if (!proxy)
	{
		return;
	}

	ConfigureProxy(proxy);

	if (!proxy->IsRegistered())
	{
		proxy->RegisterComponent();
	}
}

void UVRHandProxyComponent::BeginPlay()
{
	Super::BeginPlay();

	EnsureProxy(ProxyLeft, TEXT("HandProxyLeft"));
	EnsureProxy(ProxyRight, TEXT("HandProxyRight"));

	CreateGlowMaterials();
	ResolveOwnerComponents();

	// A sphere that never registers renders nothing and reports nothing, so say
	// out loud whether both of them made it.
	UE_LOG(LogVRHandProxy, Log,
		TEXT("Ready on %s -- proxies L:%s R:%s, mesh %s, glow material %s"),
		*GetNameSafe(GetOwner()),
		ProxyLeft && ProxyLeft->IsRegistered() ? TEXT("registered") : TEXT("FAILED"),
		ProxyRight && ProxyRight->IsRegistered() ? TEXT("registered") : TEXT("FAILED"),
		*GetNameSafe(ProxyMesh),
		GlowMaterial ? TEXT("set") : TEXT("none, spheres will not glow"));
}

void UVRHandProxyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetTemplateHandsHidden(false);

	Super::EndPlay(EndPlayReason);
}

void UVRHandProxyComponent::CreateGlowMaterials()
{
	if (!GlowMaterial)
	{
		return;
	}

	if (ProxyLeft)
	{
		MaterialLeft = UMaterialInstanceDynamic::Create(GlowMaterial, this);
		ProxyLeft->SetMaterial(0, MaterialLeft);
	}
	if (ProxyRight)
	{
		MaterialRight = UMaterialInstanceDynamic::Create(GlowMaterial, this);
		ProxyRight->SetMaterial(0, MaterialRight);
	}
}

void UVRHandProxyComponent::ResolveOwnerComponents()
{
	AActor* owner = GetOwner();
	if (!owner)
	{
		return;
	}

	if (!CachedHeadCamera.IsValid())
	{
		TInlineComponentArray<UCameraComponent*> cameras;
		owner->GetComponents(cameras);

		UCameraComponent* named = nullptr;
		UCameraComponent* off_boom = nullptr;

		for (UCameraComponent* camera : cameras)
		{
			if (camera->GetFName() == HMDCameraComponentName)
			{
				named = camera;
			}
			// Both third person cameras hang off spring arms; the head camera is
			// parented straight to VROrigin. That structure identifies it without
			// depending on a name.
			if (!off_boom && !Cast<USpringArmComponent>(camera->GetAttachParent()))
			{
				off_boom = camera;
			}
		}

		CachedHeadCamera = named ? named : off_boom;
	}

	if (!CachedLeftController.IsValid() || !CachedRightController.IsValid())
	{
		TInlineComponentArray<UMotionControllerComponent*> controllers;
		owner->GetComponents(controllers);

		// Exact matches land on the Grip components; the aim ones use LeftAim
		// and RightAim. Same lookup the body IK uses for its arm targets.
		for (UMotionControllerComponent* controller : controllers)
		{
			if (controller->MotionSource == FName("Left"))
			{
				CachedLeftController = controller;
			}
			else if (controller->MotionSource == FName("Right"))
			{
				CachedRightController = controller;
			}
		}
	}

	// Resolved once. A name that matches nothing must not cause a component scan
	// every frame for the rest of the session.
	if (!bTemplateHandsResolved)
	{
		bTemplateHandsResolved = true;
		CachedTemplateHands.Reset();

		TInlineComponentArray<USceneComponent*> scene_components;
		owner->GetComponents(scene_components);

		for (const FName& hand_name : TemplateHandComponentNames)
		{
			for (USceneComponent* component : scene_components)
			{
				if (component->GetFName() == hand_name)
				{
					FTemplateHandRef entry;
					entry.Component = component;
					entry.bOriginalVisibility = component->GetVisibleFlag();
					CachedTemplateHands.Add(entry);
					break;
				}
			}
		}
	}
}

bool UVRHandProxyComponent::ResolveViewPoint(FTransform& out_view_point) const
{
	const AActor* owner = GetOwner();
	if (!owner)
	{
		return false;
	}

	// The camera manager's POV is the final resolved view, whatever mechanism
	// switched to it, so nothing here depends on how the pawn swaps cameras.
	if (const APawn* pawn = Cast<APawn>(owner))
	{
		if (const APlayerController* controller = Cast<APlayerController>(pawn->GetController()))
		{
			if (const APlayerCameraManager* camera_manager = controller->PlayerCameraManager)
			{
				out_view_point = FTransform(camera_manager->GetCameraRotation().Quaternion(),
				                            camera_manager->GetCameraLocation());
				return true;
			}
		}
	}

	// No player controller, as in a preview world. Fall back to whichever camera
	// component is active.
	TInlineComponentArray<UCameraComponent*> cameras;
	owner->GetComponents(cameras);
	for (const UCameraComponent* camera : cameras)
	{
		if (camera->IsActive())
		{
			out_view_point = FTransform(camera->GetComponentQuat(),
			                            camera->GetComponentLocation());
			return true;
		}
	}

	return false;
}

void UVRHandProxyComponent::UpdateProxy(UStaticMeshComponent* proxy,
                                        UMaterialInstanceDynamic* material,
                                        const FLinearColor& color,
                                        const FTransform& view_point,
                                        const FTransform& head,
                                        const USceneComponent* controller,
                                        float delta_time,
                                        bool snap)
{
	if (!proxy || !controller)
	{
		return;
	}

	// The controller's offset from the head, in the head's own axes. Re-applying
	// that offset to the viewpoint preserves hand motion one-to-one while moving
	// its origin from the player's head to the camera they are looking through.
	const FVector offset_from_head =
		head.InverseTransformPosition(controller->GetComponentLocation());
	const FVector target_local = offset_from_head * ProxyOffsetScale + ProxyLocalOffset;
	const FVector target_world = view_point.TransformPosition(target_local);

	const FQuat controller_relative_to_head =
		head.GetRotation().Inverse() * controller->GetComponentQuat();
	const FQuat target_rotation = view_point.GetRotation() * controller_relative_to_head;

	const FVector location = (snap || PositionInterpSpeed <= 0.0f)
		? target_world
		: FMath::VInterpTo(proxy->GetComponentLocation(), target_world,
		                   delta_time, PositionInterpSpeed);

	proxy->SetWorldTransform(
		FTransform(target_rotation, location, FVector(FMath::Max(ProxyScale, 0.0f))));

	if (material)
	{
		material->SetVectorParameterValue(ColorParameterName, color);
		material->SetScalarParameterValue(IntensityParameterName, GlowIntensity);
	}
}

void UVRHandProxyComponent::SetTemplateHandsHidden(bool hidden)
{
	if (hidden == bTemplateHandsHidden)
	{
		return;
	}

	bTemplateHandsHidden = hidden;

	for (const FTemplateHandRef& hand : CachedTemplateHands)
	{
		if (USceneComponent* component = hand.Component.Get())
		{
			// Restoring the captured value rather than a hard true, so a hand the
			// pawn had already hidden for its own reasons stays hidden.
			component->SetVisibility(hidden ? false : hand.bOriginalVisibility, true);
		}
	}
}

void UVRHandProxyComponent::SetThirdPersonActive(bool third_person_active)
{
	bThirdPersonActive = third_person_active;
}

UStaticMeshComponent* UVRHandProxyComponent::GetProxyMesh(bool is_left) const
{
	return is_left ? ProxyLeft.Get() : ProxyRight.Get();
}

void UVRHandProxyComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ResolveOwnerComponents();

	// Picks up a material assigned mid-session, so tuning it in PIE takes effect
	// without a restart.
	if (GlowMaterial && !MaterialLeft)
	{
		CreateGlowMaterials();
	}

	UCameraComponent* head_camera = CachedHeadCamera.Get();
	USceneComponent* left_controller = CachedLeftController.Get();
	USceneComponent* right_controller = CachedRightController.Get();

	FTransform view_point;
	const bool view_point_valid = ResolveViewPoint(view_point);

	if (!head_camera || !view_point_valid)
	{
		// Without a head reference there is no offset to re-base, so stand down
		// rather than leave the spheres frozen mid-air or the hands hidden.
		bProxiesVisible = false;
		if (ProxyLeft)
		{
			ProxyLeft->SetVisibility(false);
		}
		if (ProxyRight)
		{
			ProxyRight->SetVisibility(false);
		}
		SetTemplateHandsHidden(false);

		if (!bWarnedMissingReferences)
		{
			bWarnedMissingReferences = true;
			UE_LOG(LogVRHandProxy, Warning,
				TEXT("Standing down on %s -- head camera %s, viewpoint %s. Check HMDCameraComponentName."),
				*GetNameSafe(GetOwner()),
				head_camera ? TEXT("found") : TEXT("MISSING"),
				view_point_valid ? TEXT("found") : TEXT("MISSING"));
		}
		return;
	}

	bWarnedMissingReferences = false;

	const FTransform head(head_camera->GetComponentQuat(),
	                      head_camera->GetComponentLocation());

	if (bAutoDetectThirdPerson)
	{
		// If the viewpoint sits at the head we are in VR. Anywhere else means a
		// camera took over and the player needs hands they can actually see.
		const float tolerance = FMath::Max(FirstPersonViewTolerance, 0.0f);
		bThirdPersonActive = FVector::DistSquared(view_point.GetLocation(), head.GetLocation())
		                     > FMath::Square(tolerance);
	}

	const bool was_visible = bProxiesVisible;
	bProxiesVisible = bThirdPersonActive && (left_controller || right_controller);

	if (ProxyLeft)
	{
		ProxyLeft->SetVisibility(bProxiesVisible && left_controller != nullptr);
	}
	if (ProxyRight)
	{
		ProxyRight->SetVisibility(bProxiesVisible && right_controller != nullptr);
	}

	SetTemplateHandsHidden(bProxiesVisible && bHideTemplateHandsInThirdPerson);

	if (bProxiesVisible)
	{
		// Snap on the frame they appear so they do not fly in from wherever they
		// were last left.
		const bool snap = !was_visible;

		UpdateProxy(ProxyLeft, MaterialLeft, GlowColorLeft,
		            view_point, head, left_controller, DeltaTime, snap);
		UpdateProxy(ProxyRight, MaterialRight, GlowColorRight,
		            view_point, head, right_controller, DeltaTime, snap);
	}

	if (bLogDiagnostics)
	{
		DiagnosticLogAccumulator += DeltaTime;
		if (DiagnosticLogAccumulator >= FMath::Max(DiagnosticLogInterval, 0.05f))
		{
			DiagnosticLogAccumulator = 0.0f;
			UE_LOG(LogVRHandProxy, Log,
				TEXT("ThirdPerson:%d Visible:%d  ViewToHead:%.1fcm  Controllers L:%d R:%d  Hands:%d hidden:%d  Mat:%d"),
				bThirdPersonActive ? 1 : 0,
				bProxiesVisible ? 1 : 0,
				FVector::Dist(view_point.GetLocation(), head.GetLocation()),
				left_controller ? 1 : 0,
				right_controller ? 1 : 0,
				CachedTemplateHands.Num(),
				bTemplateHandsHidden ? 1 : 0,
				MaterialLeft ? 1 : 0);
		}
	}
}
