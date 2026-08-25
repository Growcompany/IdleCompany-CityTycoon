#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/RankingData.h"
#include "RankingEntryCardWidget.generated.h"

class UTextBlock;
class UCommonButtonBase;
class UImage;

/**
 * 랭킹 리스트의 개별 항목 카드
 * - 순위, 이름, HQ레벨, 매출 표시
 * - 방문 버튼 클릭 시 상세 팝업/방문 진입
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API URankingEntryCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 방문 버튼 클릭 시 브로드캐스트 (RankingEntry 전달)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEntryCardClicked, const FRankingEntry&);
	FOnEntryCardClicked OnEntryCardClicked;

	// 데이터 설정
	void SetEntryData(const FRankingEntry& InEntry);

	// 리스트 채움 시 순차 슬라이드인 시작. OrderIndex = 카드 순번 (Spacer 제외한 루프 인덱스)
	void PlayIntro(int32 OrderIndex);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 순위 텍스트
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> RankText = nullptr;

	// 플레이어 이름 버튼 (UI_Element_Button)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> PlayerNameButton = nullptr;

	// HQ 레벨 텍스트
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> HQLevelText = nullptr;

	// 매출 텍스트
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> RevenueText = nullptr;

	// 프로필 아이콘 이미지
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage = nullptr;

	// 방문 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> MoveBtn = nullptr;

private:
	FRankingEntry CachedEntry;

	UFUNCTION()
	void OnMoveBtnClicked();

	// 슬라이드인 상태 (FloatingNumberWidget 의 NativeTick 수동 보간 패턴)
	float IntroElapsed = 0.f;
	float IntroDelay = 0.f;
	bool bIntroPlaying = false;

	static constexpr float IntroDuration = 0.28f;
	static constexpr float IntroSlideX = 60.f;
	static constexpr float IntroStagger = 0.06f;
	static constexpr float IntroMaxDelay = 0.48f;   // 화면 밖 카드까지 끝없이 늦어지지 않게 캡

	static float EaseOutCubic(float A);
};
