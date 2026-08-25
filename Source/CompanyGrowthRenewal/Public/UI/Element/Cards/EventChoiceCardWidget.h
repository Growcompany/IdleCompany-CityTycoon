#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Data/ProjectEventData.h"
#include "Enum/ProjectMode.h"
#include "EventChoiceCardWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UHorizontalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEventCardSelected, int32, ChoiceIndex);

/**
 * 단일 이벤트 선택 카드 (UIE_EventChoiceCard WBP에 부착)
 * - 카드 전체가 CommonButton (클릭 시 카드 선택)
 * - WBP 위젯: IconImage / TitleText(제목) / TitleDescriptionText(이벤트 설명) / EffectDescription(효과)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEventChoiceCardWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "EventCard")
	FOnEventCardSelected OnCardSelected;

	/** 선택지 데이터 + 인덱스(0/1/2) 설정 */
	UFUNCTION(BlueprintCallable, Category = "EventCard")
	void SetChoiceData(const FProjectEventChoice& Choice, int32 InChoiceIndex);

	/**
	 * 선택지 효과 한 줄 요약 (카드 행과 동일 포맷, 줄바꿈 결합).
	 * 결과 팝업(OfficeMainWidget)이 재사용 — 버프/정지/보상까지 모두 포함하므로
	 * 점수/시간 델타만 보던 구 팝업의 가짜 "변화 없음" 오판을 제거.
	 * @return 비어 있으면 진짜 무효과(호출부에서 "변화 없음" 처리)
	 */
	static FString BuildEffectSummaryText(const FProjectEventChoice& Choice, int32 WorkerCount, EProjectMode Mode);

	/** World 내 AOfficeworker 수 (Buff "전 직원" vs "N명" 표시 판정용) */
	static int32 CountWorkersInWorld(const UWorld* World);

	/** 현재 진행 중인 프로젝트 모드 (보상 텍스트를 수주="계약금"/자체개발="운영수익"으로 구분) */
	static EProjectMode GetActiveProjectMode(const UWorld* World);

protected:
	virtual void NativeOnClicked() override;

	// === WBP UIE_EventChoiceCard BindWidget (이름 일치) ===

	// 선택지 제목 (예: "야근 수락")
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* TitleText;

	// 효과 폴백/"변화 없음" (카테고리 행 미바인딩 구 WBP 폴백 + 무효과 메시지)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* EffectDescription;

	// 보조 설명 (옵션, 이벤트 공용 설명 — 패널이 외부에서 set)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* TitleDescriptionText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* IconImage;

	// 선택지 성격(Tone) 글로우 — 코드가 SetColorAndOpacity 로 틴트(긍정/위험/버프/중립)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* MedallionGlow;

	// === 효과 카테고리 행 (행=HBox 표시토글 / 텍스트=라벨없는 효과값. 아이콘·틴트는 WBP 베이크) ===
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional)) UHorizontalBox* ScoreRow;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional)) UCommonTextBlock* ScoreText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional)) UHorizontalBox* TimeRow;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional)) UCommonTextBlock* TimeText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional)) UHorizontalBox* PauseRow;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional)) UCommonTextBlock* PauseText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional)) UHorizontalBox* BuffRow;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional)) UCommonTextBlock* BuffText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional)) UHorizontalBox* RandomRow;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional)) UCommonTextBlock* RandomText;

private:
	int32 ChoiceIndex = -1;

	/** 카테고리별 라벨없는 효과값 (빈 문자열 = 해당 카테고리 없음). 아이콘이 카테고리를 표시 */
	static void BuildCategoryTexts(const FProjectEventChoice& Choice, int32 WorkerCount,
		FString& OutScore, FString& OutTime, FString& OutPause, FString& OutBuff, FString& OutRandom);

	/** 보상 배율 → 표시 문자열 (수주="계약금 -20%" / 자체개발="운영수익 -20%"). 변화 없으면 빈 문자열 */
	static FString BuildRewardText(const FProjectEventChoice& Choice, EProjectMode Mode);

	/** 구 WBP 폴백용 (라벨 포함 multi-line 전체 문자열) */
	FString BuildEffectText(const FProjectEventChoice& Choice) const;

	/** 선택지 성격 색 (긍정 그린 / 위험 레드 / 버프 틸 / 중립 골드) */
	FLinearColor GetToneColor(const FProjectEventChoice& Choice) const;

	/** 현재 World의 직원 수 (Buff 표시 "전 직원" vs "N명" 판정용) */
	int32 GetCurrentWorkerCount() const;

	/** 행 표시토글 + 텍스트 (행/텍스트 nullptr 안전, 빈 값이면 Collapsed) */
	static void ApplyRow(UHorizontalBox* Row, UCommonTextBlock* Text, const FString& Value);
};
