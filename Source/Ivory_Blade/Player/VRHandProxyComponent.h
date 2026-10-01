#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VRHandProxyComponent.generated.h"

class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogVRHandProxy, Log, All);

/**
 * Shows two glowing spheres as stand-in hands whenever the player's viewpoint
 * is not at their head.
 *
 * The template hands live under the motion controllers, so they sit where the
 * player's real hands are. That reads correctly in VR because the view is also
 * at the head, but the moment a third person camera takes over the viewpoint
 * moves hundreds of centimetres away and the hands are left behind at the body.
 *
 * Each sphere's offset from the active viewpoint is set to the controller's
 * offset from the HMD, so relative hand motion is preserved one-to-one while
 * the whole rig rides in front of whatever camera is being looked through.
 *
 * Add this to the pawn anywhere in the component hierarchy. The spheres use
 * absolute transforms, so the attachment point does not affect them.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class IVORY_BLADE_API UVRHandProxyComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UVRHandProxyComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// ── Activation ──

	/**
	 * Derives third person from the distance between the viewpoint and the HMD,
	 * which works regardless of how the camera switch is performed. Turn off to
	 * drive bThirdPersonActive yourself.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Activation")
	bool bAutoDetectThirdPerson = true;

	/** Used only when bAutoDetectThirdPerson is off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Activation")
	bool bThirdPersonActive = false;

	/** How close the viewpoint must be to the HMD to count as first person, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Activation")
	float FirstPersonViewTolerance = 25.0f;

	// ── Placement ──

	/**
	 * Where the hand rig sits relative to the viewpoint, in the viewpoint's own
	 * axes: X forward, Y right, Z up. Dropping Z keeps the spheres from covering
	 * the character.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Placement")
	FVector ProxyLocalOffset = FVector(60.0f, 0.0f, -20.0f);

	/** Multiplies the head-relative controller offset, to exaggerate or damp hand travel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Placement")
	float ProxyOffsetScale = 1.0f;

	/** The engine sphere is 100 cm across, so 0.15 gives a ball roughly 15 cm wide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Placement")
	float ProxyScale = 0.15f;

	/** 0 snaps instantly. Above 0 smooths the spheres toward their target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Placement")
	float PositionInterpSpeed = 0.0f;

	// ── Appearance ──

	/** Defaults to the engine sphere. Swap in a hand mesh later without touching code. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Appearance")
	TObjectPtr<UStaticMesh> ProxyMesh;

	/**
	 * Unlit material with a vector parameter for colour and a scalar for
	 * intensity. Defaults to the engine's basic shape material, which honours the
	 * colour but is lit rather than emissive, so the spheres show up on the first
	 * run without glowing. Replace it with M_HandProxyGlow for the real look.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Appearance")
	TObjectPtr<UMaterialInterface> GlowMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Appearance")
	FLinearColor GlowColorLeft = FLinearColor(0.15f, 0.6f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Appearance")
	FLinearColor GlowColorRight = FLinearColor(1.0f, 0.45f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Appearance")
	float GlowIntensity = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Appearance")
	FName ColorParameterName = FName("Color");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Appearance")
	FName IntensityParameterName = FName("Glow");

	// ── Template hands ──

	/**
	 * Hides the template hands while the spheres are up. Floating template hands
	 * next to the character's own IK-driven arms read badly from behind.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Template Hands")
	bool bHideTemplateHandsInThirdPerson = true;

	/** Components hidden alongside the hands. Add the XR device visualizations here if they should go too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Template Hands")
	TArray<FName> TemplateHandComponentNames;

	// ── Component lookup ──

	/** Name of the camera parented under VROrigin. Only used if the structural search fails. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Lookup")
	FName HMDCameraComponentName = FName("Camera");

	// ── Diagnostics ──

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Diagnostics")
	bool bLogDiagnostics = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand Proxy|Diagnostics")
	float DiagnosticLogInterval = 1.0f;

	// ── Output ──

	UPROPERTY(BlueprintReadOnly, Category = "Hand Proxy|Output")
	bool bProxiesVisible = false;

	/** Sets the mode when bAutoDetectThirdPerson is off. */
	UFUNCTION(BlueprintCallable, Category = "Hand Proxy")
	void SetThirdPersonActive(bool third_person_active);

	UFUNCTION(BlueprintPure, Category = "Hand Proxy")
	UStaticMeshComponent* GetProxyMesh(bool is_left) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Weak reference plus the visibility the component had before we touched it. */
	struct FTemplateHandRef
	{
		TWeakObjectPtr<USceneComponent> Component;
		bool bOriginalVisibility = true;
	};

	void ResolveOwnerComponents();
	bool ResolveViewPoint(FTransform& out_view_point) const;
	void UpdateProxy(UStaticMeshComponent* proxy,
	                 UMaterialInstanceDynamic* material,
	                 const FLinearColor& color,
	                 const FTransform& view_point,
	                 const FTransform& head,
	                 const USceneComponent* controller,
	                 float delta_time,
	                 bool snap);
	void SetTemplateHandsHidden(bool hidden);
	void ConfigureProxy(UStaticMeshComponent* proxy) const;
	void EnsureProxy(TObjectPtr<UStaticMeshComponent>& proxy, const TCHAR* fallback_name);
	void CreateGlowMaterials();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ProxyLeft;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ProxyRight;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialLeft;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialRight;

	TWeakObjectPtr<UCameraComponent> CachedHeadCamera;
	TWeakObjectPtr<USceneComponent> CachedLeftController;
	TWeakObjectPtr<USceneComponent> CachedRightController;
	TArray<FTemplateHandRef> CachedTemplateHands;

	bool bTemplateHandsResolved = false;
	bool bTemplateHandsHidden = false;
	bool bWarnedMissingReferences = false;
	float DiagnosticLogAccumulator = 0.0f;
};
