#include "PrefabChildFinder.h"

bool UPrefabChildFinder::FindAdvanceModeUIElements(
	AActor* spawned_ui,
	AActor*& out_frame,
	AActor*& out_spikes,
	AActor*& out_bar,
	AActor*& out_reticle)
{
	out_frame = nullptr;
	out_spikes = nullptr;
	out_bar = nullptr;
	out_reticle = nullptr;

	if (!spawned_ui)
	{
		UE_LOG(LogTemp, Warning, TEXT("PrefabChildFinder: spawned_ui is null"));
		return false;
	}

	TArray<AActor*> root_children;
	spawned_ui->GetAttachedActors(root_children);

	UE_LOG(LogTemp, Warning, TEXT("PrefabChildFinder: root has %d attached actors"), root_children.Num());

	if (root_children.IsEmpty())
	{
		return false;
	}

	for (int32 i = 0; i < root_children.Num(); ++i)
	{
		UE_LOG(LogTemp, Warning, TEXT("  root child [%d]: %s"), i,
			*root_children[i]->GetName());
	}

	out_frame = root_children[0];

	TArray<AActor*> frame_children;
	out_frame->GetAttachedActors(frame_children);

	UE_LOG(LogTemp, Warning, TEXT("PrefabChildFinder: Frame has %d attached actors"), frame_children.Num());

	for (int32 i = 0; i < frame_children.Num(); ++i)
	{
		AActor* child = frame_children[i];
		if (!child) { continue; }

		TArray<FName> tags = child->Tags;
		FString tag_str = TEXT("none");
		if (!tags.IsEmpty())
		{
			tag_str = FString::JoinBy(tags, TEXT(", "), [](const FName& tag) { return tag.ToString(); });
		}

		UE_LOG(LogTemp, Warning, TEXT("  frame child [%d]: %s | tags: %s"), i,
			*child->GetName(), *tag_str);

		if (child->ActorHasTag(FName(TEXT("Spikes"))))
		{
			out_spikes = child;
		}
		else if (child->ActorHasTag(FName(TEXT("Bar"))))
		{
			out_bar = child;
		}
		else if (child->ActorHasTag(FName(TEXT("Reticle"))))
		{
			out_reticle = child;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("PrefabChildFinder: Frame=%s Spikes=%s Bar=%s Reticle=%s"),
		out_frame ? TEXT("found") : TEXT("NULL"),
		out_spikes ? TEXT("found") : TEXT("NULL"),
		out_bar ? TEXT("found") : TEXT("NULL"),
		out_reticle ? TEXT("found") : TEXT("NULL"));

	return out_frame && out_spikes && out_bar && out_reticle;
}
