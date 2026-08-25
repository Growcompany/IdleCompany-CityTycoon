// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Building/TraitSlotWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "UI/Element/Cards/ItemCardWidget.h"
#include "CommonButtonBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Table/BuildingTraitTable.h"

UTraitSlotWidget::UTraitSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 등급별 Trait 버튼 스타일 기본 매핑 (CUI_Style_Button_Trait_*). 디자이너가 EquippedRarityStyleMap 으로 override 가능.
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> CommonF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Common.CUI_Style_Button_Trait_Common_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> UnusualF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Unusual.CUI_Style_Button_Trait_Unusual_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> RareF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Rare.CUI_Style_Button_Trait_Rare_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> EpicF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Epic.CUI_Style_Button_Trait_Epic_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> LegendaryF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Legendary.CUI_Style_Button_Trait_Legendary_C"));
	static ConstructorHelpers::FClassFinder<UCommonButtonStyle> MythicF(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/Trait/CUI_Style_Button_Trait_Mythic.CUI_Style_Button_Trait_Mythic_C"));
	if (CommonF.Succeeded())    EquippedRarityStyleMap.Add(ELootBoxRarity::Common,    CommonF.Class);
	if (UnusualF.Succeeded())   EquippedRarityStyleMap.Add(ELootBoxRarity::Unusual,   UnusualF.Class);
	if (RareF.Succeeded())      EquippedRarityStyleMap.Add(ELootBoxRarity::Rare,      RareF.Class);
	if (EpicF.Succeeded())      EquippedRarityStyleMap.Add(ELootBoxRarity::Epic,      EpicF.Class);
	if (LegendaryF.Succeeded()) EquippedRarityStyleMap.Add(ELootBoxRarity::Legendary, LegendaryF.Class);
	if (MythicF.Succeeded())    EquippedRarityStyleMap.Add(ELootBoxRarity::Mythic,    MythicF.Class);
}

void UTraitSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SlotButton)
	{
		SlotButton->OnClicked().AddUObject(this, &UTraitSlotWidget::HandleClicked);
	}
}

void UTraitSlotWidget::NativeDestruct()
{
	if (SlotButton)
	{
		SlotButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UTraitSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyDesignTimePreview();
}

void UTraitSlotWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyDesignTimePreview();
}

void UTraitSlotWidget::ApplyDesignTimePreview()
{
	// 디자이너 프리뷰 전용. 런타임은 SetEmpty/SetEquipped/SetLocked 가 처리하므로 건드리지 않음.
	if (!IsDesignTime()) return;

	ApplyVisualForState(PreviewState);
	// 잠금 비용 샘플은 DiamondCostWidget(UIE_Resource) 인스턴스 기본값(ResourceType=Diamond/Amount=30)이 표시 — 별도 세팅 불필요
	if (SelectedBorder)
	{
		SelectedBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTraitSlotWidget::HandleClicked()
{
	OnSlotClicked.ExecuteIfBound(SlotIndex);
}

void UTraitSlotWidget::SetEmpty()
{
	State = ETraitSlotVisualState::Empty;
	EquippedTraitID = NAME_None;
	if (SlotCaptionText)
	{
		SlotCaptionText->SetText(FText::FromString(TEXT("비어 있음")));
	}
	ApplyStateVisibility();
}

void UTraitSlotWidget::SetEquipped(FName InTraitID)
{
	State = ETraitSlotVisualState::Equipped;
	EquippedTraitID = InTraitID;

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FBuildingTraitTableRow Row;
	const bool bRowFound = TableMgr && TableMgr->GetBuildingTraitData(InTraitID, Row);
	if (bRowFound)
	{
		if (EquippedCard)
		{
			if (UTexture2D* Tex = Row.Icon.IsNull() ? nullptr : Row.Icon.LoadSynchronous())
			{
				EquippedCard->SetIcon(Tex);
			}
			EquippedCard->SetItemID(InTraitID);
			// 등급별 Trait 버튼 스타일 = 카드 배경색
			if (TSubclassOf<UCommonButtonStyle>* StylePtr = EquippedRarityStyleMap.Find(Row.Rarity))
			{
				if (*StylePtr) EquippedCard->SetStyle(*StylePtr);
			}
		}
	}

	ApplyStateVisibility();
}

void UTraitSlotWidget::SetLocked(int32 InDiamondCost)
{
	State = ETraitSlotVisualState::Locked;
	EquippedTraitID = NAME_None;

	if (DiamondCostWidget)
	{
		// 비용 표기는 프로젝트 공용 ResourceWidget 경유 (아이콘+축약 숫자+afford 색). 비용은 올림(UCostActionButtonWidget::SetCost 동일 규칙).
		DiamondCostWidget->AmountRoundMode = ENumberRoundMode::Ceil;
		DiamondCostWidget->SetResourceType(EResourceType::Diamond, /*bUseTypeColor=*/true);
		DiamondCostWidget->SetValue(InDiamondCost);

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UResourceItemManager* RM = GI->GetSubsystem<UResourceItemManager>())
			{
				DiamondCostWidget->SetCanAfford(RM->GetResourceAmount(EResourceType::Diamond) >= InDiamondCost);
			}
		}
	}

	ApplyStateVisibility();
}

void UTraitSlotWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	PulseTime = 0.f;
	if (SelectedBorder)
	{
		SelectedBorder->SetRenderOpacity(1.f);
		SelectedBorder->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UTraitSlotWidget::ApplyStateVisibility()
{
	ApplyVisualForState(State);
}

void UTraitSlotWidget::ApplyVisualForState(ETraitSlotVisualState InState)
{
	const bool bEquipped = (InState == ETraitSlotVisualState::Equipped);
	const bool bLocked = (InState == ETraitSlotVisualState::Locked);
	const bool bEmpty = (InState == ETraitSlotVisualState::Empty);

	auto Show = [](UWidget* W, bool bVisible)
	{
		if (W) W->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	};

	Show(EquippedCard, bEquipped);
	Show(EmptyPlusText, bEmpty);
	Show(LockOverlay, bLocked);
	Show(DiamondCostWidget, bLocked);
	// 캡션 = 빈 슬롯 전용("비어 있음") — 장착 이름은 상세 모달 담당 + 긴 이름 겹침 (2026-07-21 사용자 픽)
	Show(SlotCaptionText, bEmpty);
}

void UTraitSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 타겟 슬롯 블루 링 펄스 (0.1~1.0, 주기 약 1.4초). 비선택 시 연산 0.
	if (bSelected && SelectedBorder)
	{
		PulseTime += InDeltaTime;
		SelectedBorder->SetRenderOpacity(0.55f + 0.45f * FMath::Sin(PulseTime * 4.5f));
	}
}
