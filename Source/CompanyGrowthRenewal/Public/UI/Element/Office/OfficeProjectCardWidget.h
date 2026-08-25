#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OfficeProjectCardWidget.generated.h"

class UButtonWidget;
class UIconCardWidget;
class UStatRowWidget;
class UWidgetSwitcher;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStartButtonClicked, int32, ProjectIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStopOperationClicked);

/**
 * 프로젝트 카드 위젯
 * - 프로젝트 이미지
 * - 4개 단계별 StatRow (기획/개발/QA/출시)
 * - 4개 단계 선택 버튼
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeProjectCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 도전하기(StartButton) 클릭 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Project")
	FOnStartButtonClicked OnStartButtonClicked;

	// 운영종료 버튼 클릭 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Project")
	FOnStopOperationClicked OnStopOperationClicked;

	// 프로젝트 인덱스로 데이터 설정 (1부터 시작)
	UFUNCTION(BlueprintCallable, Category = "Project")
	void SetProjectData(int32 ProjectIndex);

	// 현재 진행 상태 업데이트 (각 단계별 현재값/목표값)
	UFUNCTION(BlueprintCallable, Category = "Project")
	void UpdateStepProgress(int32 Step1Current, int32 Step1Max, int32 Step2Current, int32 Step2Max,
	                       int32 Step3Current, int32 Step3Max, int32 Step4Current, int32 Step4Max);

	// 현재값을 즉시 스냅 (애니메이션 없음, 이벤트 선택 직후 즉각 반영용)
	UFUNCTION(BlueprintCallable, Category = "Project")
	void UpdateStepProgressInstant(int32 Step1Current, int32 Step1Max, int32 Step2Current, int32 Step2Max,
	                              int32 Step3Current, int32 Step3Max, int32 Step4Current, int32 Step4Max);

	UFUNCTION(BlueprintPure, Category = "Project")
	int32 GetCurrentProjectIndex() const { return CurrentProjectIndex; }

	// Step별 StatRow 위젯 반환 (ScoreOrb 타겟 좌표용)
	UWidget* GetStepWidget(int32 StepNumber) const;

	// 운영 모드 전환 (CardSwitcher index 1)
	UFUNCTION(BlueprintCallable, Category = "Project")
	void SwitchToOperationMode(float RevenuePerSec, int64 TotalRevenue, const FString& QualityGradeStr);

	// 스테이지 모드 전환 (CardSwitcher index 0)
	UFUNCTION(BlueprintCallable, Category = "Project")
	void SwitchToStageMode();

	// 운영 정보 갱신 (수익/초, 총수익, 등급)
	UFUNCTION(BlueprintCallable, Category = "Project")
	void UpdateOperationInfo(float RevenuePerSec, int64 TotalRevenue, const FString& QualityGradeStr);

	// 타이틀 텍스트 설정 (수주: 클라이언트명, 자체개발: "현재 프로젝트" 등)
	UFUNCTION(BlueprintCallable, Category = "Project")
	void SetTitle(const FText& TitleText);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Stage/Operation 패널 전환 (index 0=Stage, index 1=Operation)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidgetSwitcher* CardSwitcher;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UIconCardWidget* UI_ProjectImageCard;

	// 상단 타이틀 (수주 클라이언트명/자체개발 라벨)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* TitleButton;

	// === 운영 모드 위젯 ===

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* UIE_QualityGrade;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* UIE_RevenuePerSec;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* UIE_TotalRevenue;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* StopOperationButton;

	// 단계별 StatRow (기획, 개발, QA, 출시)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* UIE_StatRow_1;    // 기획

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* UIE_StatRow_2;    // 개발

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* UIE_StatRow_3;    // QA

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* UIE_StatRow_4;    // 출시

	// 도전 시작 버튼 (동시 진행 시작)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* StartButton;

private:
	int32 CurrentProjectIndex = 0;

	UFUNCTION()
	void OnStartBtnClicked();

	UFUNCTION()
	void OnStopOperationBtnClicked();
};
