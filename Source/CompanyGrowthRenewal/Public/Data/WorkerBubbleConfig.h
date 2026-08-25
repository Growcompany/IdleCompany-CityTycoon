#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Enum/BubbleType.h"
#include "WorkerBubbleConfig.generated.h"

class UTexture2D;

/**
 * 단일 상태별 이모트 풀 — 같은 상태에서도 매번 다른 이모트가 뜨도록 변형 여러 개
 */
USTRUCT(BlueprintType)
struct FWorkerBubbleVisual
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bubble")
	TArray<TSoftObjectPtr<UTexture2D>> EmoteVariants;

	/** 풀에서 랜덤 1장 선택 후 LoadSynchronous */
	UTexture2D* PickRandomTexture() const
	{
		if (EmoteVariants.Num() == 0) return nullptr;
		int32 Idx = FMath::RandRange(0, EmoteVariants.Num() - 1);
		const TSoftObjectPtr<UTexture2D>& Soft = EmoteVariants[Idx];
		return Soft.IsNull() ? nullptr : Soft.LoadSynchronous();
	}
};

/**
 * 직원 머리 위 버블 이모트 매핑 DataAsset
 * 에디터에서 EWorkerBubbleType별로 Kenney 이모트 PNG 풀 지정
 *
 * 사용 위치:
 * - AOfficeworker::SetWorkerBubbleType — 상태 변경 시 풀에서 랜덤 1장 선택
 *
 * 권장 매핑 (30종 활용):
 * - Working      : emote_dots1, dots2, dots3, music
 * - Tired        : emote_sleep, sleeps, drop, drops, faceSad
 * - Boosted      : emote_star, stars, faceHappy
 * - Happy        : emote_idea, heart, hearts, laugh
 * - Angry        : emote_anger, faceAngry, exclamation, cross
 * - EventEffect  : emote_alert, swirl, cloud, question, bars, circle
 * - Critical     : emote_exclamations, stars
 */
UCLASS(BlueprintType)
class COMPANYGROWTHRENEWAL_API UWorkerBubbleConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bubble")
	TMap<EWorkerBubbleType, FWorkerBubbleVisual> StateToEmotes;

	/** 상태에 해당하는 풀에서 랜덤 텍스처 1장 (없으면 nullptr) */
	UTexture2D* PickEmoteForState(EWorkerBubbleType State) const
	{
		if (const FWorkerBubbleVisual* Visual = StateToEmotes.Find(State))
		{
			return Visual->PickRandomTexture();
		}
		return nullptr;
	}
};
