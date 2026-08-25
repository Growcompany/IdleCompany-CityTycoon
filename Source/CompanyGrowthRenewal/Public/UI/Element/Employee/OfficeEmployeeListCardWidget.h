// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Data/EmployeeTypes.h"
#include "OfficeEmployeeListCardWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UBorder;
class UWidgetAnimation;

// 직원 카드 클릭 델리게이트 (EmployeeID, 카드 포인터)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOfficeEmployeeListCardClicked, int32, EmployeeID, UOfficeEmployeeListCardWidget*, Card);

/**
 * Office 직원 리스트 카드 위젯
 * - UIE_OfficeEmployeeListCard 블루프린트의 C++ 베이스 클래스
 * - ListView Entry로 사용
 * - 직원 이미지, 부서 아이콘, 레벨, 이름/직급 표시
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeEmployeeListCardWidget : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// IUserObjectListEntry 인터페이스 구현 (ListView Entry용)
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	// 블루프린트 바인딩 위젯들
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* EmployeeImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* DepartmentImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* LevelText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* EmployeeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UBorder* SelectionBorder;

	// 레벨업 가능 표시 이미지
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* LevelUpImage;

	// 레벨업 이미지 플로팅 애니메이션 (블루프린트에서 생성)
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	UWidgetAnimation* LevelUpFloatAnim;

	// 초상화 표시 크기 (소프트웨어 다운스케일에 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait")
	FVector2D PortraitDisplaySize = FVector2D(120.f, 160.f);

private:
	// 직원 데이터
	UPROPERTY()
	FEmployeeInstance EmployeeData;

	// 선택 상태
	bool bIsSelected = false;

	// 버튼 클릭 핸들러
	void OnButtonClicked();

	// 부서 아이콘 로드
	void LoadDepartmentIcon(EEmployeeDepartment Department);

	// 초상화 이미지 로드 (Saved/Portraits/{EmployeeID}.png)
	void LoadPortraitImage(int32 EmployeeID);

	// 레벨업 가능 여부에 따라 표시 업데이트
	void UpdateLevelUpIndicator();

public:
	// 직원 정보 설정
	UFUNCTION(BlueprintCallable, Category = "Employee")
	void SetEmployeeInfo(const FEmployeeInstance& InEmployeeData);

	// 직원 ID 반환
	UFUNCTION(BlueprintPure, Category = "Employee")
	int32 GetEmployeeID() const { return EmployeeData.EmployeeID; }

	// 직원 데이터 반환
	UFUNCTION(BlueprintPure, Category = "Employee")
	const FEmployeeInstance& GetEmployeeData() const { return EmployeeData; }

	// 선택 상태 설정
	UFUNCTION(BlueprintCallable, Category = "Selection")
	void SetSelected(bool bInSelected);

	// 선택 상태 확인
	UFUNCTION(BlueprintPure, Category = "Selection")
	bool IsSelected() const { return bIsSelected; }

	// 카드 클릭 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Employee|Event")
	FOnOfficeEmployeeListCardClicked OnOfficeEmployeeListCardClicked;
};
