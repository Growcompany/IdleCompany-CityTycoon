#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "TierRoadmapWidget.generated.h"

class UHorizontalBox;
class UScrollBox;
class UProgressBar;
class UImage;
class UCommonTextBlock;
class UCloseButtonWidget;
class UButton;

// 티어 상태색 — 로드맵 노드와 오피스 밴드 게이지가 같은 상태를 같은 색으로 말해야 해서 공유한다.
// 이 헤더에 두는 이유: 두 소비처 모두 이미 ComputeTierProgress 때문에 이 헤더를 include 한다.
// 값은 전부 linear (sRGB 분수를 그대로 넣으면 물빠진 색이 된다).
namespace CGTierColors
{
	// #43C95E — 도달(Done/Current)·진행 채움
	inline const FLinearColor Reached(0.056128f, 0.584078f, 0.111932f, 1.f);
}

/**
 * 티어 해금 로드맵 (UI_TierRoadmap) — PushPromptClass 로 열리는 온디맨드 팝오버.
 * MainMap(관리 패널 칩)과 OfficeMap(밴드 뱃지) 양쪽에서 같은 위젯을 재사용한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTierRoadmapWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// push 직후 호출 — 이 빌딩의 티어/클리어 진행도로 레일 전체를 다시 그린다
	void ConfigureForBuilding(int32 BuildingIndex);

	// 현재 티어의 클리어 진행률(0~1) = 클리어 수 / CLEAR_TO_UNLOCK. 최종 티어면 1 + bOutAtMax=true.
	// static+public 인 이유 — 로드맵/관리 패널 칩/오피스 밴드 뱃지 셋이 같은 식을 써야 하는데,
	// 복제하면 세 화면이 서로 다른 진행률을 말하게 된다.
	static float ComputeTierProgress(int32 BuildingIndex, int32 Tier, bool& bOutAtMax);

	// 연 쪽이 UI 모드를 잡았을 때만 true — 맥락(오피스=Normal 모드)에 따라 복원 책임이 갈린다.
	// 무조건 복원하면 관리 패널 위에 얹혀 열린 MainMap 경로에서 월드 입력이 되살아난다.
	void SetOwnsInputMode(bool bInOwns);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnDeactivated() override;

	UPROPERTY(meta = (BindWidgetOptional))
	UHorizontalBox* NodeRail;

	// 레일을 담는 가로 스크롤 컨테이너 — 10개 티어 전량을 그리고 현재 노드로 스크롤한다
	UPROPERTY(meta = (BindWidgetOptional))
	UScrollBox* NodeScrollBox;

	UPROPERTY(meta = (BindWidgetOptional))
	UProgressBar* ExpBar;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* ExpBarHead;

	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* ExpText;

	UPROPERTY(meta = (BindWidgetOptional))
	UCloseButtonWidget* UIE_CloseButton;

	// 배경 클릭 시 닫기용 투명 버튼
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* BackgroundBtn;

private:
	bool bOwnsInputMode = false;

	UFUNCTION()
	void HandleCloseClicked();
};
