#include "VRInputComboSet.h"
#include "VRComboCondition.h"
#include "VRComboEdge.h"
#include "InputAction.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "InputMappingContext.h"
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "VRInputComboSet"

#if WITH_EDITOR

namespace VRInputComboSetValidation
{
	/**
	 * Names every mapping context that gives OutputAction a physical key.
	 *
	 * Any such context is a bug: the combo would fire both from the key and
	 * from the injection, and on Quest the key would also put the action into
	 * OpenXR's priority arbitration, which is the failure this whole system
	 * exists to avoid.
	 */
	static TArray<FString> FindContextsMappingAction(const UInputAction* action)
	{
		TArray<FString> offenders;

		if (!action)
		{
			return offenders;
		}

		const IAssetRegistry* registry = IAssetRegistry::Get();

		if (!registry)
		{
			return offenders;
		}

		TArray<FAssetData> context_assets;
		registry->GetAssetsByClass(UInputMappingContext::StaticClass()->GetClassPathName(), context_assets);

		for (const FAssetData& asset : context_assets)
		{
			const UInputMappingContext* context = Cast<UInputMappingContext>(asset.GetAsset());

			if (!context)
			{
				continue;
			}

			for (const FEnhancedActionKeyMapping& mapping : context->GetMappings())
			{
				if (mapping.Action == action)
				{
					offenders.AddUnique(context->GetName());
					break;
				}
			}
		}

		return offenders;
	}
}

EDataValidationResult UVRInputComboSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult result = Super::IsDataValid(Context);

	for (int32 index = 0; index < Combos.Num(); ++index)
	{
		const FVRInputCombo& combo = Combos[index];

		if (!combo.OutputAction)
		{
			Context.AddError(FText::Format(LOCTEXT("NoOutput", "Combo {0} has no Output Action."), index));
			result = EDataValidationResult::Invalid;
		}

		if (!combo.EdgeAction)
		{
			Context.AddError(FText::Format(LOCTEXT("NoEdgeAction", "Combo {0} has no Edge Action."), index));
			result = EDataValidationResult::Invalid;
		}

		if (!combo.Edge)
		{
			Context.AddError(FText::Format(LOCTEXT("NoEdge", "Combo {0} has no Edge. Pick a gesture, for example Flick."), index));
			result = EDataValidationResult::Invalid;
		}

		if (combo.OutputAction && combo.OutputAction == combo.EdgeAction)
		{
			Context.AddError(FText::Format(
				LOCTEXT("SelfFeeding", "Combo {0} outputs to its own Edge Action, which would make it retrigger itself."),
				index));
			result = EDataValidationResult::Invalid;
		}

		for (int32 condition_index = 0; condition_index < combo.Conditions.Num(); ++condition_index)
		{
			if (!combo.Conditions[condition_index])
			{
				Context.AddError(FText::Format(
					LOCTEXT("NullCondition", "Combo {0} has an empty entry at Conditions[{1}]."),
					index,
					condition_index));
				result = EDataValidationResult::Invalid;
			}
		}

		// Hysteresis only works if the release threshold sits below the actuate
		// threshold. Equal values let a stick hovering on the line retrigger
		// every frame.
		if (const UVRComboEdge_Threshold* threshold_edge = Cast<UVRComboEdge_Threshold>(combo.Edge))
		{
			if (threshold_edge->ReleaseThreshold >= threshold_edge->ActuateThreshold)
			{
				Context.AddError(FText::Format(
					LOCTEXT("BadHysteresis", "Combo {0} has Release Threshold {1} at or above Actuate Threshold {2}. Release must be lower or the combo will retrigger every frame."),
					index,
					threshold_edge->ReleaseThreshold,
					threshold_edge->ActuateThreshold));
				result = EDataValidationResult::Invalid;
			}
		}

		const TArray<FString> offenders = VRInputComboSetValidation::FindContextsMappingAction(combo.OutputAction);

		if (offenders.Num() > 0)
		{
			Context.AddError(FText::Format(
				LOCTEXT("OutputStillMapped", "Combo {0} outputs to {1}, which still has key mappings in: {2}. A combo output must own no physical source."),
				index,
				FText::FromString(combo.OutputAction->GetName()),
				FText::FromString(FString::Join(offenders, TEXT(", ")))));
			result = EDataValidationResult::Invalid;
		}
	}

	return result;
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
