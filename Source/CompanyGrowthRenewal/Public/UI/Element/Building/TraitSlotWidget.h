// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Enum/LootBoxRarity.h"
#include "TraitSlotWidget.generated.h"

class UButtonWidget;
class UBorder;
class UImage;
class UCommonTextBlock;
class UResourceWidget;

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
 * 빌딩 특성 탭 장착 밴드의 슬롯 1칸 = 판에 파인 소켓. 빈칸/장착/잠금/선택 상태를 가짐.
 * 로직은 C++, 비주얼 트리는 WBP. 등급색(면·빛·보석)과 효과 문구만 C++ 가 런타임 주입한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTraitSlotWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetSlotIndex(int32 InIndex) { SlotIndex = InIndex; }

	UFUNCTION(BlueprintPure, Category = "TraitSlot")
	int32 GetSlotIndex() const { return SlotIndex; }

	// 빈(해금) 슬롯
	UFUNCTION(BlueprintCallable, Category = "TraitSlot")
	void SetEmpty();

	// 특성 장착됨 — DT 조회로 아이콘/등급/효과 세팅
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

	// 에디터 디자이너에서 상태별 프리뷰용 — 인스턴스마다 빈칸/장착/잠금 다르게 배치
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TraitSlot|Preview")
	ETraitSlotVisualState PreviewState = ETraitSlotVisualState::Empty;

	// 디자이너 장착 프리뷰의 등급색
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TraitSlot|Preview")
	ELootBoxRarity PreviewRarity = ELootBoxRarity::Epic;

protected:
	virtual void NativePreConstruct() override;
	virtual void SynchronizeProperties() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 슬롯 전체를 덮는 클릭 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> SlotButton;

	// 소켓 밖으로 번지는 등급색 빛 (장착)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> RarityGlow;

	// 소켓 안의 장착 면 — SDF 판 머티리얼, 등급색을 MID 로 주입 (장착)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> EquippedFace;

	// 특성 아이콘 (장착)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> EquippedIcon;

	// 오른쪽 위 등급 보석 — 45도 회전 SDF 판, 등급색 MID 주입 (장착)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> RarityGem;

	// 빈 슬롯 링 + ＋
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> EmptyRing;

	// ＋ = 둥근 막대 두 개를 감싼 테두리. 막대 색은 이 테두리의 내용 색이 곱해진다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UBorder> EmptyPlus;

	// 잠긴 소켓 덮개 (빗금) + 자물쇠와 그 아래 그림자
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> LockStripes;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> LockOverlay;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> LockShadow;

	// 다이아 해금 비용 (잠금 상태에서만) — 프로젝트 공용 ResourceWidget 재사용(아이콘+축약 숫자+afford 색)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UResourceWidget> DiamondCostWidget;

	// 선택(장착 대상) 링
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> SelectedBorder;

	// 빈 슬롯 캡션 "장착 가능"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> SlotCaptionText;

	// 장착 캡션 2줄 — 효과 대상 이름 / 수치
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> EffectNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> EffectValueText;

	// 등급색을 면 위/아래 어두운 바탕과 섞는 비율 (sRGB 공간, 1 = 등급색 그대로)
	UPROPERTY(EditAnywhere, Category = "TraitSlot|Rarity", meta = (ClampMin = "0", ClampMax = "1"))
	float FaceTopRarityMix = 0.70f;

	UPROPERTY(EditAnywhere, Category = "TraitSlot|Rarity", meta = (ClampMin = "0", ClampMax = "1"))
	float FaceBottomRarityMix = 0.30f;

	UPROPERTY(EditAnywhere, Category = "TraitSlot|Rarity")
	FColor FaceTopBase = FColor(0x1A, 0x1F, 0x29);

	UPROPERTY(EditAnywhere, Category = "TraitSlot|Rarity")
	FColor FaceBottomBase = FColor(0x11, 0x15, 0x1C);

	// 소켓 밖 빛의 알파 (Glow_Oval 중심 알파 0.54 에 곱해진다)
	UPROPERTY(EditAnywhere, Category = "TraitSlot|Rarity", meta = (ClampMin = "0", ClampMax = "1"))
	float GlowAlpha = 0.9f;

	// 빈 슬롯 링·＋·캡션 색 — 평소 / 장착 대상 (sRGB, 알파 포함). 링의 알파는 재질 BaseA 로 들어간다
	UPROPERTY(EditAnywhere, Category = "TraitSlot|Empty")
	FColor RingColor = FColor(0xFF, 0xFF, 0xFF, 41);

	UPROPERTY(EditAnywhere, Category = "TraitSlot|Empty")
	FColor RingTargetColor = FColor(0x4A, 0x83, 0xDA, 217);

	UPROPERTY(EditAnywhere, Category = "TraitSlot|Empty")
	FColor PlusColor = FColor(0xFF, 0xFF, 0xFF, 107);

	UPROPERTY(EditAnywhere, Category = "TraitSlot|Empty")
	FColor PlusTargetColor = FColor(0x8D, 0xB4, 0xF0);

	UPROPERTY(EditAnywhere, Category = "TraitSlot|Empty")
	FColor CaptionColor = FColor(0x97, 0xA3, 0xB6);

	UPROPERTY(EditAnywhere, Category = "TraitSlot|Empty")
	FColor CaptionTargetColor = FColor(0x8D, 0xB4, 0xF0);

private:
	void HandleClicked();
	void ApplyStateVisibility();
	// 상태별 자식 위젯 가시성 적용 (런타임 State / 디자인타임 PreviewState 공용 경로)
	void ApplyVisualForState(ETraitSlotVisualState InState);
	// 등급색을 면·보석 MID 와 빛 틴트에 주입
	void ApplyRarityColors(ELootBoxRarity Rarity);
	// 디자이너 프리뷰 적용 (IsDesignTime 가드) — NativePreConstruct/SynchronizeProperties 공용
	void ApplyDesignTimePreview();
	// 선택 링 농도를 PulseTime 에 맞춰 재질에 주입
	void ApplySelectedRingPulse();

	float SelectedRingPeakAlpha = -1.f;
	int32 SlotIndex = 0;
	ETraitSlotVisualState State = ETraitSlotVisualState::Empty;
	FName EquippedTraitID = NAME_None;
	bool bSelected = false;
	float PulseTime = 0.f;
};
