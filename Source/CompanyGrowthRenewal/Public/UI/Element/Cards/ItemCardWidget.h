// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "ItemCardWidget.generated.h"

class UImage;
class UBorder;
class USizeBox;
class UTexture2D;

/**
 * UIE_ItemCard
 * 144x144 정사각 카드 본체. 시각(EntityImage/Outline/Lock) + 클릭 받음.
 * 단독으로도 NoQty 카드로 사용 가능. 수량 라벨이 필요하면 UItemCardSlotWidget 변형(Compact/Full) 으로 감싼다.
 */
DECLARE_DELEGATE_OneParam(FOnItemCardClicked, FName /*ItemID*/);

UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UItemCardWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetIcon(UTexture2D* InIcon);

	// 텍스처/머티리얼(MI) 공용 — 상점 아이템 아이콘은 MI 일 수 있음
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetIconResource(UObject* InResource);

	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetItemID(FName InItemID) { ItemID = InItemID; }

	UFUNCTION(BlueprintPure, Category = "ItemCard")
	FName GetItemID() const { return ItemID; }

	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetLocked(bool bInLocked);

	UFUNCTION(BlueprintPure, Category = "ItemCard")
	bool IsLocked() const { return bLocked; }

	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetInsufficient(bool bInInsufficient);

	UFUNCTION(BlueprintPure, Category = "ItemCard")
	bool IsInsufficient() const { return bInsufficient; }

	// 선택 상태 (Outline ON + 선택 색). bInsufficient 가 우선순위 높음 (빨강).
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "ItemCard")
	bool IsSelected() const { return bSelected; }

	// EntityImage 슬롯 크기를 동적으로 변경. Product 카드(4:3) 같이 비정사각형 비율 적용용.
	// ⚠ 카드 몸체(루트 CardSizeBox 144)는 그대로다 — 카드 자체를 줄이려면 SetCardSize().
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetIconSize(FVector2D NewSize);

	// 슬롯 변형이 "IconSize 0 = 상속"을 지킬 때 되읽는다 — SetCardSize 가 두 크기를 함께 덮으므로
	// 저작값을 그대로 되돌려주지 않으면 아이콘이 카드 변으로 부푼다
	UFUNCTION(BlueprintPure, Category = "ItemCard")
	FVector2D GetIconSize() const { return IconSize; }

	// 카드 몸체(루트 SizeBox)와 아이콘 박스를 각각 명시해 리사이즈 — 미리보기 스트립 등 축소 배치용 opt-in.
	// 아이콘 슬롯의 저작 패딩(144 카드 기준 10px)은 0 으로 리셋 — 두 크기를 호출자가 전부 소유하고 아이콘은 중앙 정렬 유지.
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetCardSize(FVector2D InCardSize, FVector2D InIconSize);

	// 아이콘 그림 곱색 — 부르지 않으면 저작값 그대로(받은 보상 어둡게 등 배치처 opt-in)
	UFUNCTION(BlueprintCallable, Category = "ItemCard")
	void SetIconTint(FLinearColor InTint);

	UFUNCTION(BlueprintPure, Category = "ItemCard")
	FLinearColor GetIconTint() const;

	FOnItemCardClicked OnItemCardClicked;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnClicked() override;

	// 디자이너가 Class Defaults 에서 변형 WBP 별로 override (UIE_ItemCard_Product 등).
	// 런타임 변경은 SetIconSize() 호출.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemCard|Layout",
		meta = (ClampMin = 16.0))
	FVector2D IconSize = FVector2D(128.0f, 128.0f);

	// 켜면 아이콘 투명 여백(UUIIconContentBounds)을 잘라 IconSize 안에 그림 비율대로 맞춘다 — 출석 본체 등 opt-in
	UPROPERTY(EditAnywhere, Category = "ItemCard|Layout")
	bool bFitIconToContent = false;

	// 부모 WBP/Details/CreateWidget 에서 아이콘을 바꿀 수 있게 노출. 비우면 EntityImage 브러시 원본 유지.
	// 런타임 변경은 SetIcon() 호출.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemCard|Visual",
		meta = (ExposeOnSpawn = true))
	TObjectPtr<UTexture2D> DefaultIcon;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> EntityImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> IconSizeBox;

	// 카드 몸체 크기 권위(WBP 루트 SizeBox). 미배선 변형에서는 SetCardSize 가 아이콘만 줄인다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> CardSizeBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> LockBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LockImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Outline;

	// 선택 테두리 색 — 공용 기본은 흰 글로우, 배치 인스턴스가 문맥 색(예: 특성 격자 기준링 #4A83DA)을 굽는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemCard|Style")
	FLinearColor SelectedOutlineColor = FLinearColor::White;

private:
	FName ItemID = NAME_None;
	bool bLocked = false;
	bool bInsufficient = false;
	bool bSelected = false;
	bool bWarnedIconFitSlot = false;

	// Outline 우선순위: bInsufficient(빨강) > bSelected(흰/선택색) > Collapsed
	void RefreshOutline();

	void ApplyIconSize();
	void ApplyCardSize();
	void ApplyIconFit();

	// 0 = WBP 저작값 유지 (SetCardSize 호출 전에는 카드 몸체를 건드리지 않는다)
	FVector2D CardSize = FVector2D::ZeroVector;
};
