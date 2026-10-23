// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Element/HQ/HQPanelTypes.h"
#include "HQRewardSlotWidget.generated.h"

class UBorder;
class UCommonTextBlock;
class UImage;
class USizeBox;

/**
 * 본사 레벨업 보상 카드의 보상 칸 한 개 — 아이콘 타일 + 수량 배지 + 이름
 * (WBP = UIE_HQRewardSlot, 런타임 생성 = EWidgetType::HQRewardSlot). 칸 크기는 카드 안 보상 수로 정한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UHQRewardSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// HorizontalGap = 카드 안 칸 사이 가로 간격 — 이름은 칸 + 간격 폭을 넘지 않게 줄을 나눈다
	void SetReward(const FHQRewardSlot& Reward, int32 RewardCountInCard, float HorizontalGap);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USizeBox> TileSize;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> TileGlow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> TileLip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> TileFace;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> TileSheen;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> PhotoImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> CountBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CountText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CaptionText;

	// 칸 한 변 — 보상 1개 / 2~3개 / 4개 이상
	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	float SingleTileSize = 210.f;

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	float FewTileSize = 160.f;

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	float ManyTileSize = 112.f;

	// 칸 아래 립 두께 (칸 높이 = 한 변 + 립)
	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	float LipHeight = 5.f;

	// Slate pt — 보상이 1개일 때만 이름을 키운다
	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	int32 CaptionFontSize = 21;

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	int32 SingleCaptionFontSize = 26;

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	float IconFill = 0.72f;

	// 메뉴 글리프(흰 채움 + 검정 테)는 어두운 칸에서 조금 작게
	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	float MenuIconFill = 0.62f;

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	FLinearColor PlainFace = FLinearColor(FColor(0xFB, 0xF4, 0xE6));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	FLinearColor PlainSheen = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	FLinearColor PlainRim = FLinearColor(FColor(0xE0, 0xC6, 0x8C));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	FLinearColor PlainLip = FLinearColor(FColor(0xCD, 0xAE, 0x6B));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	float PlainRimWidth = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	float IndustryRimWidth = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	FLinearColor FeatureRim = FLinearColor(FColor(0xE0, 0xB8, 0x4F));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	FLinearColor FeatureLip = FLinearColor(FColor(0xC7, 0x9A, 0x2E));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	float FeatureRimWidth = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	FLinearColor MenuFace = FLinearColor(FColor(0x1C, 0x20, 0x26));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	FLinearColor MenuSheen = FLinearColor(FColor(0x2B, 0x30, 0x38));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	FLinearColor MenuRim = FLinearColor(FColor(0x0E, 0x11, 0x16));

	UPROPERTY(EditDefaultsOnly, Category = "HQ|RewardSlot")
	FLinearColor MenuLip = FLinearColor(FColor(0x8C, 0x76, 0x50));

private:
	void ApplyTileBrushes(const FLinearColor& Face, const FLinearColor& Sheen, const FLinearColor& Rim, float RimWidth, const FLinearColor& Lip);
	void ApplyIcon(UObject* Icon, float BoxSize);
};
