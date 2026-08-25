// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Cards/CardWidgetBase.h"
#include "Data/EmployeeTypes.h"
#include "OfficeRecruitmentCardWidget.generated.h"

class UWidgetSwitcher;
class UVerticalBox;
class UCommonTextBlock;
class UCommonButtonBase;
class UStatRowWidget;
class UBorder;

/**
 * 채용 카드 위젯 - 주스탯/부스탯 표시 및 상세 뷰 전환
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeRecruitmentCardWidget : public UCardWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void OnButtonClicked() override;

	// ========== 선택 상태 처리 ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBorder* SelectionBorder;

	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;

	// ========== UI 바인딩 - 기본 정보 ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* DepartmentImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* EmployeeNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* RankText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* DepartmentNameText;

	// ========== UI 바인딩 - WidgetSwitcher ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidgetSwitcher* StatViewSwitcher;

	// ========== UI 바인딩 - 스탯 컨테이너 ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UVerticalBox* MainStatInfoBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UVerticalBox* DetailStatInfoBox;

	// ========== UI 바인딩 - 버튼 ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonButtonBase* DetailBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonButtonBase* SummaryBtn;

	// ========== 동적 생성 StatRow 관리 ==========

	UPROPERTY()
	TArray<UStatRowWidget*> MainStatRows;

	UPROPERTY()
	TArray<UStatRowWidget*> DetailStatRows;

	// ========== 데이터 ==========

	UPROPERTY(BlueprintReadOnly, Category = "Recruitment")
	FEmployeeInstance EmployeeData;

	UPROPERTY(BlueprintReadOnly, Category = "Recruitment")
	EEmployeeDepartment CurrentDepartment;

	// ========== 내부 함수 ==========

	UFUNCTION()
	void OnDetailBtnClicked();

	UFUNCTION()
	void OnSummaryBtnClicked();

	void CreateStatRows();
	void ClearStatRows();
	void LoadPortraitImage(int32 EmployeeID);
	void LoadDepartmentImage(EEmployeeDepartment Department);

public:
	/** 직원 데이터 설정 */
	UFUNCTION(BlueprintCallable, Category = "Recruitment")
	void SetEmployeeData(const FEmployeeInstance& InEmployee);

	/** 스탯 표시 업데이트 */
	UFUNCTION(BlueprintCallable, Category = "Recruitment")
	void UpdateStatDisplay();

	/** 뷰 전환 (0: 요약, 1: 상세) */
	UFUNCTION(BlueprintCallable, Category = "Recruitment")
	void SwitchView(int32 ViewIndex);
};
