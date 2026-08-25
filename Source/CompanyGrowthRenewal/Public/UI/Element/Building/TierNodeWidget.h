#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TierNodeWidget.generated.h"

class UCommonTextBlock;
class UImage;
class USizeBox;

UENUM(BlueprintType)
enum class ETierNodeState : uint8
{
	Upcoming,   // 미도달
	Current,    // 현재 티어 (강조 + 글로우)
	Done        // 지나온 티어
};

/**
 * 티어 로드맵 노드 1개 (UIE_TierNode) — 순수 표시.
 * 데이터 조회 없음: 티어/상태/해금명을 소유 팝오버가 주입한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTierNodeWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void Configure(int32 Tier, ETierNodeState State, const TArray<FText>& UnlockNames);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* LevelText;

	UPROPERTY(meta = (BindWidgetOptional))
	USizeBox* DotSizeBox;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* DotFill;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* NodeGlow;

	// 구 좌석 수 표시 — 인원 상한이 층수 축으로 이관돼 항상 Collapsed
	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* SeatText;

	// 이 티어에서 열리는 강화 이름들. 여러 개면 줄바꿈으로 이어붙인다
	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* UnlockText;
};
