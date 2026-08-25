// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BuildingSlotHorizonWidget.generated.h"

class UButtonWidget;
class UCommonTextBlock;
class UImage;
class UProgressBar;
class ABuildingBaseActor;

UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildingSlotHorizonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 건물 데이터 설정
	void SetBuildingData(ABuildingBaseActor* InBuilding);

	// 이동 버튼 클릭 델리게이트
	DECLARE_DELEGATE_OneParam(FOnMoveClicked, int32 /* BuildingIndex */);
	FOnMoveClicked OnMoveClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* BuildingNameButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* LevelText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* EmployeeNumText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* IconImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* MoveBtn;

	// 업종 시그니처색을 아이콘 테두리로 흡수 — 별도 업종 컬럼 없이 색만으로 식별
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* IconPlate = nullptr;

	// 실수익 "1.2만/초" — 배율(×N.NN)은 추상적이라 폐기, 운영 실측치 표시
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* IncomeText = nullptr;

	// 직원 정원 대비 채움 게이지 (현재/정원)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProgressBar* CapacityBar = nullptr;

private:
	void HandleMoveClicked();

	// 수익 변동 라이브 갱신 — OnExpectedRevenueChanged 구독 (스냅샷 금지 규칙)
	void HandleExpectedRevenueChanged(int32 InBuildingID, float NewRate);
	void SetIncomePerSec(float RevPerSec);

	// 운영 매니저의 BuildingID 파라미터 = 이 인덱스 (FName GetBuildingID 아님 — 호출부 전수 확인)
	UPROPERTY()
	int32 CachedBuildingIndex = INDEX_NONE;
};
