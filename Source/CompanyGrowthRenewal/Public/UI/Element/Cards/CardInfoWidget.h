// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CardInfoWidget.generated.h"

class UImage;
class UHorizontalBox;
class UCommonTextBlock;
struct FBuildableCardTable;
struct FEmployeeCardTable;
struct FWorkstationCardTable;
struct FDecorationCardTable;
enum class ELootBoxRarity : uint8;
enum class EResourceType : uint8;

UCLASS()
class COMPANYGROWTHRENEWAL_API UCardInfoWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* BackgroundImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* BuildEntityName;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UUserWidget* UIE_Resource;

	virtual void NativeConstruct() override;

public:
	// virtual — 빌딩 전용 파생(UBuildCardInfoWidget)이 등급 틴트 무력화 + 멀티코스트 처리를 덧붙임.
	virtual void SetBuildableInfo(const FBuildableCardTable& buildableInfo);
	void SetEmployeeInfo(const FEmployeeCardTable& employeeInfo);
	void SetWorkstationInfo(const FWorkstationCardTable& workstationInfo);
	void SetDecorationInfo(const FDecorationCardTable& decorationInfo);

	// 건설/고용 비용 부족 여부 업데이트 (virtual — 파생이 2차 비용칩까지 토글)
	virtual void UpdateCostAffordability(EResourceType ResourceType, bool bCanAfford);
};
