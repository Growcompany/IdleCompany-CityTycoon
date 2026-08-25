// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/LootBoxRarity.h"
#include "SkinInfoWidget.generated.h"

class UCommonTextBlock;
class UImage;

/**
 * 건물 스킨 정보를 표시하는 위젯
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API USkinInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 희귀도 기반 배경색으로 스킨 정보 설정
	UFUNCTION(BlueprintCallable, Category = "Skin")
	void SetSkinInfo(const FText& InSkinName, ELootBoxRarity InRarity);

	// 배경 색상 직접 설정
	UFUNCTION(BlueprintCallable, Category = "Skin")
	void SetBackgroundColor(FLinearColor InColor);

	// 스킨 이름 직접 설정
	UFUNCTION(BlueprintCallable, Category = "Skin")
	void SetSkinName(const FText& InName);

protected:
	virtual void NativePreConstruct() override;

protected:
	// 에디터에서 설정 가능한 기본 배경 색상
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	FLinearColor DefaultBackgroundColor = FLinearColor::White;

	// 에디터에서 설정 가능한 기본 스킨 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	FText DefaultSkinName;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* SkinName;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* BackgroundColor;
};
