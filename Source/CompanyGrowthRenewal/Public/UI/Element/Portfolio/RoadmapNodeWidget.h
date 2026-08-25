#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "RoadmapNodeWidget.generated.h"

class UImage;
class UCommonTextBlock;
class USizeBox;

UENUM(BlueprintType)
enum class ERoadmapNodeState : uint8
{
	Upcoming,   // 미도달 (뮤트)
	Current,    // 현재 티어 (산업색 + 글로우 강)
	Done        // 완료 (그린 + 글로우 약)
};

/**
 * 포트폴리오 로드맵 노드 1개 (UIE_RoadmapNode) — 생김새는 WBP 트리, 개수/상태/색은 CodexPanel C++가 구동.
 * 데이터 없음(순수 표시). 진척 연결선/간격/노드 개수는 소유 패널(CodexPanel)이 담당.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API URoadmapNodeWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 티어 번호 + 상태 + 산업색 주입 → 번호/라벨/색/글로우 visibility/dot 크기 세팅
	void Configure(int32 Tier, ERoadmapNodeState State, const FLinearColor& IndustryColor);

protected:
	// dot 크기 토글 (current 64 / 그 외 54)
	UPROPERTY(meta = (BindWidget))
	USizeBox* DotSizeBox;

	// 원형 필 (SetBrushTintColor 로 상태색 주입 — 외곽 셸 링은 유지)
	UPROPERTY(meta = (BindWidget))
	UImage* DotFill;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* NumberText;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* TierLabel;

	// 상단 광택 (done/current 만 표시)
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* DotGloss;

	// 뒤 글로우 (Glow_Oval, done/current 만 표시 + 산업/그린 틴트)
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* NodeGlow;
};
