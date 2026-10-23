// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Enum/LootBoxRarity.h"
#include "Enum/BuildingTraitRequirement.h"
#include "Enum/BuildingTraitCategory.h"
#include "TraitDetailPopupWidget.generated.h"

class UButtonWidget;
class UCloseButtonWidget;
class UItemCardWidget;
class UCommonButtonStyle;
class UImage;
class UCommonTextBlock;
class UBorder;
struct FBuildingTraitTableRow;

// 장착/해제 요청 — 슬롯 결정 로직은 패널이 소유하므로 델리게이트로만 통지
DECLARE_DELEGATE_OneParam(FOnTraitDetailEquip, FName /*TraitID*/);
DECLARE_DELEGATE_OneParam(FOnTraitDetailUnequip, int32 /*SlotIndex*/);

/**
 * UIE_TraitDetailPopup
 * 특성 카드/슬롯 클릭 시 패널 왼쪽 가장자리에 붙어 뜨는 상세 카드.
 * 정보(아이콘/이름/등급/분야/효과/설명/세트보너스/장착불가사유) + 장착/해제 버튼.
 * 배치·열고 닫기는 패널이 담당. 팝업은 내용 표시 + 델리게이트 발화만.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UTraitDetailPopupWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UTraitDetailPopupWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * 상세 내용 채우기 + 버튼 모드 분기.
	 * @param InEquippedSlotIndex  INDEX_NONE(-1) = 인벤토리(미장착) 특성 → [장착] 모드.
	 *                             0 이상       = 해당 슬롯에 장착된 특성 → [해제] 모드.
	 */
	UFUNCTION(BlueprintCallable, Category = "TraitDetail")
	void ConfigureForTrait(FName InTraitID, int32 InBuildingIndex, int32 InEquippedSlotIndex = -1);

	// [장착] 이 향할 슬롯을 버튼 글자로 알린다. INDEX_NONE = 빈 해금 슬롯 없음 → 버튼 비활성 + 안내 문구.
	// 인벤 모드에서 ConfigureForTrait 직후 패널이 호출한다(업종 제한 특성이면 무시)
	UFUNCTION(BlueprintCallable, Category = "TraitDetail")
	void SetEquipTarget(int32 TargetSlotIndex, bool bSwap, bool bNoUnlockedSlot);

	FOnTraitDetailEquip OnEquipClicked;
	FOnTraitDetailUnequip OnUnequipClicked;
	FSimpleDelegate OnCloseClicked;

	// 미션 가이드(M11)가 [장착] 버튼을 하이라이트 링 타겟으로 쓰기 위한 노출
	UWidget* GetEquipButtonWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 헤더 아이콘 = 기존 UIE_TraitCard(UItemCardWidget) 재사용. C++ 가 SetIcon + 등급별 Trait 버튼스타일 SetStyle(배경색)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UItemCardWidget> HeaderCard;

	// 등급 → Trait 버튼 스타일(CUI_Style_Button_Trait_*) 매핑. 생성자가 6개 기본 채움(슬롯과 동일). HeaderCard 배경색.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TraitDetail|Style")
	TMap<ELootBoxRarity, TSubclassOf<UCommonButtonStyle>> RarityStyleMap;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> NameText;

	// 등급 이름 — 등급색 잉크 (칩 없이 글자만)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> RarityText;

	// 등급 옆 제약 분류 (공통/제조 전용/프로젝트 전용). 분야는 세트 이름이 말하므로 싣지 않는다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> RequirementText;

	// 효과 줄 양끝 — 대상 이름 / 수치
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> EffectTargetText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> EffectValueText;

	// 효과 상세 설명 문장 (GetTraitTargetDetailText 로 생성. 조건/상한을 문장이 싣는다)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> EffectDetailText;

	// 세트 보너스 미리보기 (분야/현재 장착 수/2·3세트 효과)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> SetBonusText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SetBonusSection;

	// 헤더 카드 뒤 등급색 글로우 (Glow_Oval 틴트)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> HeaderGlow;

	// 세트 섹션 구조화 (세트명/진행/2·3세트 행/도트) — 미배치 시 구 SetBonusText 단일 문자열 폴백
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> SetBonusNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> SetBonusProgressText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> SetBonus2Text;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> SetBonus3Text;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> SetDot0;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> SetDot1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> SetDot2;

	// 장착 불가 경고 플레이트 (레드 틴트 섹션째 토글 — 미배치 시 RestrictionText 단독 토글 폴백)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RestrictionSection;

	// 장착 불가 사유 (제조/프로젝트 전용 특성, 비면 숨김)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> RestrictionText;

	// 빈 슬롯이 없을 때만 뜨는 안내 (CTA 바로 위, 왼쪽 정렬 줄바꿈)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> TargetSlotHintText;

	// [장착] (인벤토리 모드)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> EquipButton;

	// [해제] (장착 모드)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> UnequipButton;

	// 닫기 (X) — 우상단 표준 닫기 위젯 (UCloseButtonWidget, OnCloseClicked)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCloseButtonWidget> UIE_CloseButton;

private:
	void HandleEquipClicked();
	void HandleUnequipClicked();
	UFUNCTION()
	void HandleCloseClicked();

	void BuildSetBonusPreview(const FBuildingTraitTableRow& Row, int32 BuildingIndex);

	void ApplyMetaLine(EBuildingTraitRequirement InRequirement);

	FName CurrentTraitID = NAME_None;
	int32 CurrentBuildingIndex = INDEX_NONE;
	int32 CurrentEquippedSlotIndex = INDEX_NONE;
	bool bCurrentCanEquip = true;
};
