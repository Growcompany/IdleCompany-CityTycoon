// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HQLevelStepWidget.generated.h"

class UCommonTextBlock;
class UBorder;

/**
 * 최고 레벨에서 성장 탭 본문을 채우는 레벨별 해금 기록 한 칸
 * (WBP = UIE_HQLevelStep, 런타임 생성 = EWidgetType::HQLevelStep). 문구는 DT_HQLevel.UnlockDescription.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UHQLevelStepWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetStep(int32 Level, const FText& Unlock, bool bCurrent);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> CardLip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> CardFace;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> LevelTile;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> LevelPrefixText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> LevelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> UnlockText;

	UPROPERTY(EditDefaultsOnly, Category = "HQ|LevelStep")
	FLinearColor RimColor = FLinearColor(FColor(0xC5, 0xD0, 0xDD));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|LevelStep")
	FLinearColor LipColor = FLinearColor(FColor(0xB1, 0xBD, 0xCC));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|LevelStep")
	FLinearColor TileColor = FLinearColor(FColor(0x24, 0x3B, 0x56));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|LevelStep")
	FLinearColor TileInk = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "HQ|LevelStep")
	FLinearColor CurrentRimColor = FLinearColor(FColor(0xE0, 0xB8, 0x4F));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|LevelStep")
	FLinearColor CurrentLipColor = FLinearColor(FColor(0xC7, 0x9A, 0x2E));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|LevelStep")
	FLinearColor CurrentTileColor = FLinearColor(FColor(0xE6, 0xB4, 0x47));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|LevelStep")
	FLinearColor CurrentTileInk = FLinearColor(FColor(0x3A, 0x27, 0x00));
};
