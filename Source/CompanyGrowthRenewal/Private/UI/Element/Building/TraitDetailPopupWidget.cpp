// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Building/TraitDetailPopupWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Cards/ItemCardWidget.h"
#include "CommonButtonBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "CommonTextBlock.h"
#include "Engine/GameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Table/BuildingTraitTable.h"
#include "Table/BuildingTraitSetBonusTable.h"
#include "Enum/BuildingTraitCategory.h"
#include "Enum/BuildingTraitTarget.h"
#include "Enum/LootBoxRarity.h"

UTraitDetailPopupWidget::UTraitDetailPopupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 등급별 Trait 버튼 스타일 기본 매핑 (CUI_Style_Button_Trait_*). 슬롯(UTraitSlotWidget)과 동일.
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> CommonF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Common.CUI_Style_Button_Trait_Common_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> UnusualF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Unusual.CUI_Style_Button_Trait_Unusual_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> RareF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Rare.CUI_Style_Button_Trait_Rare_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> EpicF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Epic.CUI_Style_Button_Trait_Epic_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> LegendaryF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Legendary.CUI_Style_Button_Trait_Legendary_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> MythicF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Mythic.CUI_Style_Button_Trait_Mythic_C"));
	if (CommonF.Succeeded())    RarityStyleMap.Add(ELootBoxRarity::Common,    CommonF.Class);
	if (UnusualF.Succeeded())   RarityStyleMap.Add(ELootBoxRarity::Unusual,   UnusualF.Class);
	if (RareF.Succeeded())      RarityStyleMap.Add(ELootBoxRarity::Rare,      RareF.Class);
	if (EpicF.Succeeded())      RarityStyleMap.Add(ELootBoxRarity::Epic,      EpicF.Class);
	if (LegendaryF.Succeeded()) RarityStyleMap.Add(ELootBoxRarity::Legendary, LegendaryF.Class);
	if (MythicF.Succeeded())    RarityStyleMap.Add(ELootBoxRarity::Mythic,    MythicF.Class);
}

void UTraitDetailPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (EquipButton)   EquipButton->OnClicked().AddUObject(this, &UTraitDetailPopupWidget::HandleEquipClicked);
	if (UnequipButton) UnequipButton->OnClicked().AddUObject(this, &UTraitDetailPopupWidget::HandleUnequipClicked);
	if (UIE_CloseButton) UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UTraitDetailPopupWidget::HandleCloseClicked);
}

void UTraitDetailPopupWidget::NativeDestruct()
{
	if (EquipButton)   EquipButton->OnClicked().RemoveAll(this);
	if (UnequipButton) UnequipButton->OnClicked().RemoveAll(this);
	if (UIE_CloseButton) UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &UTraitDetailPopupWidget::HandleCloseClicked);

	Super::NativeDestruct();
}

UWidget* UTraitDetailPopupWidget::GetEquipButtonWidget() const
{
	return EquipButton;
}

void UTraitDetailPopupWidget::ConfigureForTrait(FName InTraitID, int32 InBuildingIndex, int32 InEquippedSlotIndex)
{
	CurrentTraitID = InTraitID;
	CurrentBuildingIndex = InBuildingIndex;
	CurrentEquippedSlotIndex = InEquippedSlotIndex;

	// 동적 생성(Collapsed) 팝업이라 NativeConstruct 바인딩 타이밍이 불확실 → 매 오픈마다 버튼 바인딩 보장(RemoveAll 로 중복 방지)
	if (EquipButton)   { EquipButton->OnClicked().RemoveAll(this);   EquipButton->OnClicked().AddUObject(this, &UTraitDetailPopupWidget::HandleEquipClicked); }
	if (UnequipButton) { UnequipButton->OnClicked().RemoveAll(this); UnequipButton->OnClicked().AddUObject(this, &UTraitDetailPopupWidget::HandleUnequipClicked); }
	if (UIE_CloseButton) { UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &UTraitDetailPopupWidget::HandleCloseClicked); UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UTraitDetailPopupWidget::HandleCloseClicked); }

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UBuildingTraitManagerSubsystem* TraitMgr = GI ? GI->GetSubsystem<UBuildingTraitManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	FBuildingTraitTableRow Row;
	if (!TableMgr->GetBuildingTraitData(InTraitID, Row)) return;

	// 아이콘 = 기존 UIE_TraitCard(UItemCardWidget) 재사용 — 카드가 프레임/그림자 내장, 여기선 SetIcon 만
	if (HeaderCard && !Row.Icon.IsNull())
	{
		if (UTexture2D* Tex = Row.Icon.LoadSynchronous())
		{
			HeaderCard->SetIcon(Tex);
		}
		HeaderCard->SetItemID(InTraitID);
	}
	// 헤더 카드 배경 = 등급별 Trait 버튼 스타일 (슬롯 EquippedCard 와 동일)
	if (HeaderCard)
	{
		if (TSubclassOf<UCommonButtonStyle>* StylePtr = RarityStyleMap.Find(Row.Rarity))
		{
			if (*StylePtr) HeaderCard->SetStyle(*StylePtr);
		}
	}

	// 이름 / 등급 / 분야
	if (NameText) NameText->SetText(Row.DisplayName);
	if (RarityText)
	{
		RarityText->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(Row.Rarity)));
		// 등급 이름을 등급색으로 (런타임 SetColorAndOpacity 는 스타일 UpdateFromStyle 이후라 유지됨)
		RarityText->SetColorAndOpacity(FSlateColor(FLootBoxRarityUtility::GetRarityColor(Row.Rarity)));
	}
	if (CategoryText)
	{
		// 이 두 분야의 표시명(제조전용/프로젝트전용)은 제약 그 자체라 전용 칩과 같은 말을 반복한다 → 칩에 맡기고 숨김
		const bool bCategoryIsRequirement =
			(Row.Category == EBuildingTraitCategory::Manufacturing || Row.Category == EBuildingTraitCategory::Project);
		if (bCategoryIsRequirement)
		{
			CategoryText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			// 표시명 SOT = BuildingTraitCategory.h (⚠ UMETA 는 에디터 전용 — 패키징 빌드에서 "Revenue"/"Efficiency" 로 떨어진다)
			CategoryText->SetText(GetBuildingTraitCategoryDisplayName(Row.Category));
			CategoryText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
	ApplyRequirementChip(Row.RequiredType);

	// 등급색 주입 — 칩 필 A0.16 / 링 A0.45 / 글로우 A0.5 (런타임 데이터 주입)
	const FLinearColor RarityColor = FLootBoxRarityUtility::GetRarityColor(Row.Rarity);
	if (RarityChipBorder)
	{
		FSlateBrush ChipBrush = RarityChipBorder->Background;
		ChipBrush.TintColor = FSlateColor(FLinearColor(RarityColor.R, RarityColor.G, RarityColor.B, 0.16f));
		ChipBrush.OutlineSettings.Color = FSlateColor(FLinearColor(RarityColor.R, RarityColor.G, RarityColor.B, 0.45f));
		RarityChipBorder->SetBrush(ChipBrush);
	}
	if (HeaderGlow)
	{
		HeaderGlow->SetColorAndOpacity(FLinearColor(RarityColor.R, RarityColor.G, RarityColor.B, 0.5f));
	}

	// 효과 한 줄(카드와 동일 문구) + 상세 설명 문장 (섹션 배경 플레이트째 토글 — 비면 통째로 숨김)
	const EBuildingTraitTarget EffectTarget = GetTargetForCategory(Row.Category);
	if (DescText) DescText->SetText(FormatTraitEffectText(EffectTarget, Row.BaseEffect));
	{
		const FText DetailText = GetTraitTargetDetailText(EffectTarget, Row.BaseEffect);
		const bool bHasDetail = !DetailText.IsEmpty();
		if (EffectDetailText && bHasDetail) EffectDetailText->SetText(DetailText);
		const ESlateVisibility DetailVis = bHasDetail ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		if (EffectDetailSection)   EffectDetailSection->SetVisibility(DetailVis);
		else if (EffectDetailText) EffectDetailText->SetVisibility(DetailVis);
	}

	// 모드 분기 (인벤토리 = 장착 / 슬롯 = 해제)
	const bool bEquippedMode = (InEquippedSlotIndex >= 0);

	// 장착 불가 사유 (인벤토리 모드에서만 평가)
	bool bCanEquip = true;
	if (!bEquippedMode && TraitMgr)
	{
		bCanEquip = TraitMgr->CanEquipTraitToBuilding(InBuildingIndex, InTraitID);
	}
	const bool bShowRestriction = (!bEquippedMode && !bCanEquip);
	if (RestrictionText)
	{
		RestrictionText->SetText(FText::FromString(TEXT("이 건물 업종에는 장착할 수 없는 특성입니다")));
	}
	if (RestrictionSection)
	{
		RestrictionSection->SetVisibility(bShowRestriction ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	else if (RestrictionText)
	{
		RestrictionText->SetVisibility(bShowRestriction ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (EquipButton)
	{
		EquipButton->SetVisibility(bEquippedMode ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		EquipButton->SetIsEnabled(bCanEquip);
		EquipButton->SetButtonText(bCanEquip
			? FText::FromString(TEXT("장착"))
			: FText::FromString(TEXT("장착 불가")));
	}
	if (UnequipButton)
	{
		UnequipButton->SetVisibility(bEquippedMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		UnequipButton->SetButtonText(FText::FromString(TEXT("해제")));
	}

	// 세트 보너스 미리보기
	BuildSetBonusPreview(Row, InBuildingIndex);
}

void UTraitDetailPopupWidget::SetTargetSlotHint(const FText& InHint)
{
	if (!TargetSlotHintText) return;
	TargetSlotHintText->SetText(InHint);
	TargetSlotHintText->SetVisibility(InHint.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UTraitDetailPopupWidget::ApplyRequirementChip(EBuildingTraitRequirement InRequirement)
{
	FText ChipText;
	switch (InRequirement)
	{
	case EBuildingTraitRequirement::Manufacturing: ChipText = FText::FromString(TEXT("제조 전용"));   break;
	case EBuildingTraitRequirement::Project:       ChipText = FText::FromString(TEXT("프로젝트 전용")); break;
	default:                                       ChipText = FText::FromString(TEXT("공통"));         break;
	}

	if (RequirementText) { RequirementText->SetText(ChipText); }

	// 3상태 상시 표시 — 침묵하면 "제약 없음"과 "칩을 못 봤음"이 구분되지 않는다.
	// 저작본이 Collapsed 를 들고 있어도(구 paste) 런타임에 켜지도록 매번 명시한다.
	if (RequirementChipBorder)  { RequirementChipBorder->SetVisibility(ESlateVisibility::HitTestInvisible); }
	else if (RequirementText)   { RequirementText->SetVisibility(ESlateVisibility::HitTestInvisible); }
}

void UTraitDetailPopupWidget::BuildSetBonusPreview(const FBuildingTraitTableRow& Row, int32 BuildingIndex)
{
	auto ShowSetSection = [this](bool bShow)
	{
		const ESlateVisibility V = bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		if (SetBonusSection)   SetBonusSection->SetVisibility(V);
		else if (SetBonusText) SetBonusText->SetVisibility(V);
	};

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UBuildingTraitManagerSubsystem* TraitMgr = GI ? GI->GetSubsystem<UBuildingTraitManagerSubsystem>() : nullptr;
	if (!TableMgr || !TraitMgr) { ShowSetSection(false); return; }

	// 이 건물에 현재 장착된 같은 분야 개수
	int32 SameCategoryCount = 0;
	for (const FName& Equipped : TraitMgr->GetEquippedTraits(BuildingIndex))
	{
		if (Equipped.IsNone()) continue;
		FBuildingTraitTableRow ER;
		if (TableMgr->GetBuildingTraitData(Equipped, ER) && ER.Category == Row.Category)
		{
			++SameCategoryCount;
		}
	}

	FBuildingTraitSetBonus Bonus2, Bonus3;
	const bool bHas2 = TableMgr->GetBuildingTraitSetBonus(Row.Category, 2, Bonus2);
	const bool bHas3 = TableMgr->GetBuildingTraitSetBonus(Row.Category, 3, Bonus3);
	if (!bHas2 && !bHas3) { ShowSetSection(false); return; }

	// 표시명 SOT = BuildingTraitCategory.h (⚠ UMETA 는 에디터 전용 — 패키징 빌드에서 "Revenue"/"Efficiency" 로 떨어진다)
	const FText CatName = GetBuildingTraitCategoryDisplayName(Row.Category);

	if (SetBonusNameText)
	{
		// 구조화 트리: 세트명 + 진행 도트/카운트 + 2·3세트 행(달성=활성색)
		SetBonusNameText->SetText(CatName);
		if (SetBonusProgressText)
		{
			SetBonusProgressText->SetText(FText::FromString(
				FString::Printf(TEXT("%d/3"), FMath::Clamp(SameCategoryCount, 0, 3))));
		}

		static const FLinearColor ActiveCol(0.597f, 0.791f, 0.650f);  // #CBE6D3
		static const FLinearColor MutedCol(0.337f, 0.451f, 0.381f);   // #9DB3A6
		auto FillLine = [&](UCommonTextBlock* LineText, bool bHas, int32 Req, const FBuildingTraitSetBonus& Bonus)
		{
			if (!LineText) return;
			if (!bHas) { LineText->SetVisibility(ESlateVisibility::Collapsed); return; }
			LineText->SetVisibility(ESlateVisibility::HitTestInvisible);
			LineText->SetText(FText::FromString(
				FString::Printf(TEXT("%d세트 · %s"), Req,
					*FormatTraitEffectText(GetTargetForCategory(Bonus.Category), Bonus.BonusValue).ToString())));
			LineText->SetColorAndOpacity(FSlateColor(SameCategoryCount >= Req ? ActiveCol : MutedCol));
		};
		FillLine(SetBonus2Text, bHas2, 2, Bonus2);
		FillLine(SetBonus3Text, bHas3, 3, Bonus3);

		UImage* Dots[3] = { SetDot0.Get(), SetDot1.Get(), SetDot2.Get() };
		for (int32 i = 0; i < 3; ++i)
		{
			if (!Dots[i]) continue;
			Dots[i]->SetColorAndOpacity(SameCategoryCount > i
				? FLinearColor(0.275f, 0.521f, 0.347f, 1.0f)   // #8FBF9F 채움
				: FLinearColor(1.0f, 1.0f, 1.0f, 0.16f));      // 미달 = 흰 16%
		}
	}
	else if (SetBonusText)
	{
		// 구 트리 폴백 — 단일 문자열
		FString Out = FString::Printf(TEXT("[세트] %s (현재 %d개 장착)\n"), *CatName.ToString(), SameCategoryCount);
		if (bHas2) Out += FString::Printf(TEXT("2세트: %s\n"), *FormatTraitEffectText(GetTargetForCategory(Bonus2.Category), Bonus2.BonusValue).ToString());
		if (bHas3) Out += FString::Printf(TEXT("3세트: %s\n"), *FormatTraitEffectText(GetTargetForCategory(Bonus3.Category), Bonus3.BonusValue).ToString());
		SetBonusText->SetText(FText::FromString(Out.TrimEnd()));
	}
	ShowSetSection(true);
}

void UTraitDetailPopupWidget::HandleEquipClicked()
{
	OnEquipClicked.ExecuteIfBound(CurrentTraitID);
}

void UTraitDetailPopupWidget::HandleUnequipClicked()
{
	OnUnequipClicked.ExecuteIfBound(CurrentEquippedSlotIndex);
}

void UTraitDetailPopupWidget::HandleCloseClicked()
{
	OnCloseClicked.ExecuteIfBound();
}
