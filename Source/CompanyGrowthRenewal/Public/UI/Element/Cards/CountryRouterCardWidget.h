// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Entity/Country/CountryActor.h"
#include "Enum/ResourceType.h"
#include "UI/Panel/CountryRouterPanelWidget.h"  // ECountryRouterMode
#include "CountryRouterCardWidget.generated.h"

struct FCountryInfoTable;

class UImage;
class UCommonTextBlock;
class UCommonButtonBase;
class UWidget;

/**
 * CountryRouterCardWidget
 * 월드맵 자원/공장 패널의 리스트 항목.
 *
 * 카드 자체는 표시 전용 컨테이너(UUserWidget). 우측 "관리(MoveBtn)" 버튼만 클릭 인터랙션.
 * MoveBtn 클릭 시 해당 국가의 CountryDetail을 열고 지정된 탭으로 자동 전환.
 *
 * 사용 예:
 *   Card->SetCountry(ECountryType::Korea);
 *   Card->SetTargetTabIndex(1);  // 1=Factory
 *   Card->SetLockState(false);
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCountryRouterCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CountryRouter")
	void SetCountry(ECountryType InCountry);

	// 모드별 SpecialtyText 표시 분기 — SetCountry 전에 호출 권장
	UFUNCTION(BlueprintCallable, Category = "CountryRouter")
	void SetMode(ECountryRouterMode InMode);

	UFUNCTION(BlueprintCallable, Category = "CountryRouter")
	void SetTargetTabIndex(int32 InTabIndex) { TargetTabIndex = InTabIndex; }

	UFUNCTION(BlueprintCallable, Category = "CountryRouter")
	void SetLockState(bool bInLocked, const FText& InLockReason = FText::GetEmpty());

	UFUNCTION(BlueprintPure, Category = "CountryRouter")
	ECountryType GetCountry() const { return Country; }

	UFUNCTION(BlueprintPure, Category = "CountryRouter")
	bool IsLocked() const { return bLocked; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CountryNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> SpecialtyText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> DescriptionText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LockBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LockConditionText;

	// 우측 "관리" 버튼 — 실제 클릭 인터랙션 담당
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> MoveBtn;

private:
	ECountryType Country = ECountryType::None;
	int32 TargetTabIndex = 0;
	bool bLocked = false;
	ECountryRouterMode Mode = ECountryRouterMode::Resource;

	void ApplyCountryBasics();
	FText ResolveSpecialtyForMode(const FCountryInfoTable& Info, const class UTableManagerSubsystem* TableMgr) const;

	void HandleMoveClicked();
};
