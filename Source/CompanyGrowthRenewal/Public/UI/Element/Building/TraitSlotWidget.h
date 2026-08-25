// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Enum/LootBoxRarity.h"
#include "TraitSlotWidget.generated.h"

class UButtonWidget;
class UImage;
class UCommonTextBlock;
class UResourceWidget;
class UItemCardWidget;
class UCommonButtonStyle;

// 슬롯 시각 상태
UENUM(BlueprintType)
enum class ETraitSlotVisualState : uint8
{
	Empty,      // 해금됨 + 비어있음 (＋ 표시)
	Equipped,   // 특성 장착됨 (아이콘 + 등급 프레임)
	Locked      // 미해금 (다이아 해금 비용 표시)
};

// 슬롯 클릭 → 패널이 SlotIndex 로 분기 (타겟 선택 / 다이아 해금 / 장착 특성 상세)
DECLARE_DELEGATE_OneParam(FOnTraitSlotClicked, int32 /*SlotIndex*/);

/**
 * UIE_TraitSlot
 * 빌딩 특성 탭 상단의 장착 슬롯 1칸. 빈칸/장착/잠금/선택 상태를 가짐.
 * 로직은 C++, 비주얼 트리는 WBP(BindWidgetOptional). 클릭은 SlotButton 이 받아 OnSlotClicked 로 forward.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTraitSlotWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UTraitSlotWidget(const FObjectInitializer& ObjectInitializer);

	void SetSlotIndex(int32 InIndex) { SlotIndex = InIndex; }

	UFUNCTION(BlueprintPure, Category = "TraitSlot")
	int32 GetSlotIndex() const { return SlotIndex; }

	// 빈(해금) 슬롯
	UFUNCTION(BlueprintCallable, Category = "TraitSlot")
	void SetEmpty();

	// 특성 장착됨 — DT 조회로 아이콘/등급 세팅
	UFUNCTION(BlueprintCallable, Category = "TraitSlot")
	void SetEquipped(FName InTraitID);

	// 잠김 — 다이아 해금 비용 표시
	UFUNCTION(BlueprintCallable, Category = "TraitSlot")
	void SetLocked(int32 InDiamondCost);

	// 선택(타겟) 하이라이트 토글
	UFUNCTION(BlueprintCallable, Category = "TraitSlot")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "TraitSlot")
	ETraitSlotVisualState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "TraitSlot")
	FName GetEquippedTraitID() const { return EquippedTraitID; }

	// 패널이 바인딩하는 클릭 델리게이트
	FOnTraitSlotClicked OnSlotClicked;

	// 에디터 디자이너에서 상태별 프리뷰용 — 인스턴스마다 빈칸/장착/잠금 다르게 배치 (런타임엔 ClearChildren 후 교체)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TraitSlot|Preview")
	ETraitSlotVisualState PreviewState = ETraitSlotVisualState::Empty;

protected:
	virtual void NativePreConstruct() override;
	virtual void SynchronizeProperties() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 슬롯 전체를 덮는 클릭 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> SlotButton;

	// 장착 비주얼 = UIE_TraitCard(UItemCardWidget=버튼). C++ 가 SetIcon + 등급별 Trait 버튼스타일 SetStyle (배경색)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UItemCardWidget> EquippedCard;

	// 등급 → Trait 버튼 스타일(CUI_Style_Button_Trait_*) 매핑. 생성자에서 6개 기본 채움(디자이너 override 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TraitSlot|Style")
	TMap<ELootBoxRarity, TSubclassOf<UCommonButtonStyle>> EquippedRarityStyleMap;

	// 빈 슬롯 ＋ 표시
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> EmptyPlusText;

	// 잠금 오버레이 (자물쇠)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LockOverlay;

	// 다이아 해금 비용 (잠금 상태에서만) — 프로젝트 공용 ResourceWidget 재사용(아이콘+축약 숫자+afford 색)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UResourceWidget> DiamondCostWidget;

	// 선택 상태 테두리 하이라이트
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> SelectedBorder;

	// 슬롯 하단 캡션 (장착=특성명 / 빈=비어 있음 / 잠금=Collapsed, 가격 칩이 대체)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> SlotCaptionText;

private:
	void HandleClicked();
	void ApplyStateVisibility();
	// 상태별 자식 위젯 가시성 적용 (런타임 State / 디자인타임 PreviewState 공용 경로)
	void ApplyVisualForState(ETraitSlotVisualState InState);
	// 디자이너 프리뷰 적용 (IsDesignTime 가드) — NativePreConstruct/SynchronizeProperties 공용
	void ApplyDesignTimePreview();

	int32 SlotIndex = 0;
	ETraitSlotVisualState State = ETraitSlotVisualState::Empty;
	FName EquippedTraitID = NAME_None;
	bool bSelected = false;
	float PulseTime = 0.f;
};
