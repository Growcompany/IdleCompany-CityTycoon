// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Cards/EntityCardWidgetBase.h"
#include "Table/WorkstationCardTable.h"
#include "OfficeWorkstationCardWidget.generated.h"

class UBorder;
class UImage;
class UCommonTextBlock;

/**
 * 업무공간 카드 위젯
 * - 업무공간(책상) 배치용 카드 UI
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeWorkstationCardWidget : public UEntityCardWidgetBase
{
	GENERATED_BODY()

protected:
	// 카드 클릭 핸들러 (부모의 OnButtonClicked에서 호출됨)
	virtual void HandleCardClicked() override;

	// IUserObjectListEntry 인터페이스 구현 (ListView Entry용)
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	// 잠금 조건 체크 (현재는 모두 해금)
	void CheckUnlockCondition();

private:
	// 업무공간 데이터
	UPROPERTY(BlueprintReadOnly, Category = "Workstation", meta = (AllowPrivateAccess = "true"))
	FWorkstationCardTable WorkstationInfo;

public:
	// 업무공간 정보 설정
	UFUNCTION(BlueprintCallable, Category = "Workstation")
	void SetWorkstationInfo(const FWorkstationCardTable& InWorkstationInfo);

	// 업무공간 정보 반환
	UFUNCTION(BlueprintCallable, Category = "Workstation")
	const FWorkstationCardTable& GetWorkstationInfo() const { return WorkstationInfo; }
};
