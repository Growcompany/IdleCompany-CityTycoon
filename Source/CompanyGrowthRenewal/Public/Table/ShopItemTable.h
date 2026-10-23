#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/ShopTypes.h"
#include "Enum/ItemType.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "ShopItemTable.generated.h"

// 번들 구성품 1칸. CSV 표기 = "((Item=RecruitTicketAdvanced,Quantity=5),(Item=BusinessCardPaper,Quantity=3))"
USTRUCT(BlueprintType)
struct FShopBundleEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemType Item = EItemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Quantity = 1;
};

// 상점 상품 정의 (DT_ShopItem — 단일 진실. 품목/가격/한도는 CSV 리임포트만으로 수정)
USTRUCT(BlueprintType)
struct FShopItemTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EShopTab Tab = EShopTab::Daily;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EItemType Item = EItemType::None;

	// 1회 구매 시 지급 수량
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;

	// 머티리얼 아이콘(MI, 티어색 파라미터). 설정되면 Icon 보다 우선 — 읽기는 ResolveIconResource() 로
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UMaterialInterface> IconMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EShopCurrency Currency = EShopCurrency::Money;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int64 Price = 0;

	// 리셋 주기당 구매 한도 (0 = 무제한)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 LimitCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SortOrder = 0;

	// 신규 배지. 기획이 켜고 끄는 값이라 날짜 계산으로 추론하지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bNew = false;

	// 카드 아이콘 칸(무대) 바탕색 = 상품 등급색의 진한 쪽. 위쪽 밝기는 WBP 의 빛 층이 더한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor BandTint = FLinearColor::White;

	// 귀한 상품(최고급·럭키·패키지). 무대 뒤에 빛줄기를 켠다 — 모든 카드에 켜면 구분이 사라진다
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bFeatured = false;

	// 패키지 구성품. 비어 있으면 단일 상품(Item/Quantity)으로 동작한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FShopBundleEntry> BundleItems;

	bool IsBundle() const { return BundleItems.Num() > 0; }

	// 지급 목록 — 번들이면 구성품 전부, 아니면 Item/Quantity 한 칸
	TArray<FShopBundleEntry> GetGrantEntries() const
	{
		if (IsBundle())
		{
			return BundleItems;
		}
		FShopBundleEntry Single;
		Single.Item = Item;
		Single.Quantity = Quantity;
		return TArray<FShopBundleEntry>{ Single };
	}

	UObject* ResolveIconResource() const
	{
		if (!IconMaterial.IsNull())
		{
			if (UMaterialInterface* Mat = IconMaterial.LoadSynchronous()) { return Mat; }
		}
		return Icon.IsNull() ? nullptr : Icon.LoadSynchronous();
	}
};
