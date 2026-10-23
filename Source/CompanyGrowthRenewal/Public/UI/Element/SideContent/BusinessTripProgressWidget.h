#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/Element/Common/CountdownText.h"
#include "Table/BusinessTripCardTable.h"
#include "BusinessTripProgressWidget.generated.h"

class UImage;
class UProgressBar;
class UHorizontalBox;
class UVerticalBox;
class UCommonTextBlock;

/** 진행 카드에 그릴 직원 1명. 초상은 패널이 로드해 넘긴다. */
USTRUCT()
struct FBusinessTripCrewView
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	bool bBuildingTeam = false;

	UPROPERTY()
	TObjectPtr<UTexture2D> Portrait = nullptr;
};

/**
 * 출장 진행 카드 — 목적지(크림 카드) + 남은 시간·진행 바·보낸 직원(다크 플레이트).
 * 남은 시간은 FCountdownTicker 가 1초마다 그린다(값은 시계가 소유, 여기는 렌더만).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBusinessTripProgressWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** InRemainingProvider = 남은 초 게터. 패널이 서브시스템을 물려준다. */
	void SetTrip(const FBusinessTripCardTable& InRow, const FText& InDisciplineName,
		const TArray<FBusinessTripCrewView>& InCrew, TFunction<int64()> InRemainingProvider);

	/** 진행 바 갱신 — 0~1. 타이머와 별개로 패널이 밀어 넣는다. */
	void SetProgress(float InRatio);

	/** 화면이 카드 선택으로 돌아갈 때 — 숨은 위젯이 1초 타이머를 계속 물고 있지 않게 한다. */
	void StopCountdown() { RemainTicker.Stop(); }

	/** 진행 중에도 무엇이 떨어질지 계속 보인다 — 카드와 같은 줄 문법을 쓴다. */
	void SetLootPreview(const TArray<struct FSideDungeonLootRow>& InRows, int32 InStage);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* DestinationText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* ConditionText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* DurationText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* ExperienceText = nullptr;

	/** 남은 시간 — 큰 숫자. 스타일 슬롯 없이 Pretendard SemiBold 를 WBP 가 직접 쥔다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* RemainText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UProgressBar* ProgressBar = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* CrewHeadText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UHorizontalBox* CrewBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UVerticalBox* RewardList = nullptr;

	/** 초상·이름·출처를 담는 칸 — 디자인타임 샘플에서 폰트/크기를 복사한다(CGRewardChip 문법과 같은 뿌리). */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* CrewSample = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* RewardRowSample = nullptr;

private:
	FCountdownTicker RemainTicker;
};
