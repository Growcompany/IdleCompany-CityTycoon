// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/WorldResourcePanelWidget.h"
#include "UI/Panel/CountryDetailWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/UIBase.h"
#include "Manager/WorldMapManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Global/GlobalUtilFunctions.h"
#include "Enum/WidgetType.h"
#include "Player/MainMapPlayerController.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Engine/World.h"

void UWorldResourcePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		WorldMapMgr = GI->GetSubsystem<UWorldMapManager>();
		TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	}

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UWorldResourcePanelWidget::OnCloseButtonClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UWorldResourcePanelWidget::OnBackgroundClicked);
	}

	BuildRows();
}

void UWorldResourcePanelWidget::NativeDestruct()
{
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &UWorldResourcePanelWidget::OnCloseButtonClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UWorldResourcePanelWidget::OnBackgroundClicked);
	}
	Super::NativeDestruct();
}

void UWorldResourcePanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (WorldMapMgr)
	{
		WorldMapMgr->OnMaterialChanged.AddDynamic(this, &UWorldResourcePanelWidget::HandleMaterialChanged);
		WorldMapMgr->OnEnergyChanged.AddDynamic(this, &UWorldResourcePanelWidget::HandleEnergyChanged);
		WorldMapMgr->OnRefinedOilChanged.AddDynamic(this, &UWorldResourcePanelWidget::HandleRefinedOilChanged);
	}

	RefreshAllValues();
}

void UWorldResourcePanelWidget::NativeOnDeactivated()
{
	if (WorldMapMgr)
	{
		WorldMapMgr->OnMaterialChanged.RemoveDynamic(this, &UWorldResourcePanelWidget::HandleMaterialChanged);
		WorldMapMgr->OnEnergyChanged.RemoveDynamic(this, &UWorldResourcePanelWidget::HandleEnergyChanged);
		WorldMapMgr->OnRefinedOilChanged.RemoveDynamic(this, &UWorldResourcePanelWidget::HandleRefinedOilChanged);
	}

	if (UWorld* World = GetWorld())
	{
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(World->GetFirstPlayerController()))
		{
			if (PC->GetCurrentInputMode() == EInputMode::UI)
			{
				PC->GoToNormalMode();
			}
		}
	}
	Super::NativeOnDeactivated();
}

void UWorldResourcePanelWidget::BuildRows()
{
	if (!ResourceContainer) return;

	ResourceContainer->ClearChildren();
	MaterialRowMap.Reset();

	// None=0 스킵, Max=sentinel 전까지. 루프 순서가 UI 노출 순서를 결정.
	for (int32 i = 1; i < static_cast<int32>(ERawMaterialType::Max); ++i)
	{
		const ERawMaterialType Mat = static_cast<ERawMaterialType>(i);
		UTextBlock* Row = NewObject<UTextBlock>(this);
		if (!Row) continue;

		UVerticalBoxSlot* BoxSlot = ResourceContainer->AddChildToVerticalBox(Row);
		if (BoxSlot) BoxSlot->SetPadding(FMargin(0.f, 2.f));

		MaterialRowMap.Add(Mat, Row);
	}
}

void UWorldResourcePanelWidget::RefreshAllValues()
{
	if (!WorldMapMgr) return;

	for (const TPair<ERawMaterialType, TObjectPtr<UTextBlock>>& Pair : MaterialRowMap)
	{
		SetMaterialRowText(Pair.Key, WorldMapMgr->GetMaterialAmount(Pair.Key));
	}

	if (EnergyText)     EnergyText->SetText(UGlobalUtilFunctions::AbbreviateNumber(WorldMapMgr->GetEnergy(), ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
	if (RefinedOilText) RefinedOilText->SetText(UGlobalUtilFunctions::AbbreviateNumber(WorldMapMgr->GetRefinedOil(), ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
}

void UWorldResourcePanelWidget::SetMaterialRowText(ERawMaterialType Mat, int64 Amount)
{
	TObjectPtr<UTextBlock>* Found = MaterialRowMap.Find(Mat);
	if (!Found || !*Found) return;

	(*Found)->SetText(FText::FromString(FString::Printf(TEXT("%s: %s"),
		*RawMaterialTypeToString(Mat),
		*UGlobalUtilFunctions::AbbreviateNumber(Amount, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString())));
}

void UWorldResourcePanelWidget::HandleMaterialChanged(ERawMaterialType Mat, int64 NewAmount)
{
	SetMaterialRowText(Mat, NewAmount);
}

void UWorldResourcePanelWidget::HandleEnergyChanged(int64 NewEnergy)
{
	if (EnergyText) EnergyText->SetText(UGlobalUtilFunctions::AbbreviateNumber(NewEnergy, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
}

void UWorldResourcePanelWidget::HandleRefinedOilChanged(int64 NewOil)
{
	if (RefinedOilText) RefinedOilText->SetText(UGlobalUtilFunctions::AbbreviateNumber(NewOil, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
}

void UWorldResourcePanelWidget::JumpToCountry(ERawMaterialType Mat)
{
	if (!WorldMapMgr || !TableMgr) return;

	const ECountryType TargetCountry = WorldMapMgr->GetPrimaryCountryForMaterial(Mat);
	if (TargetCountry == ECountryType::None) return;

	// 카메라 포커스
	if (ACountryActor* CountryActor = ACountryActor::FindCountryActor(this, TargetCountry))
	{
		CountryActor->RequestFocus(true);
	}

	// CountryDetail 푸시
	UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	if (!UIMgr) return;
	UUIBase* UIBase = UIMgr->GetUIBase();
	if (!UIBase) return;

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::CountryDetail);
	if (!Cls) return;

	UCommonActivatableWidget* Pushed = UIBase->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(Cls));
	if (UCountryDetailWidget* Detail = Cast<UCountryDetailWidget>(Pushed))
	{
		Detail->SetCountry(TargetCountry);
	}
}

void UWorldResourcePanelWidget::OnCloseButtonClicked()
{
	DeactivateWidget();
}

void UWorldResourcePanelWidget::OnBackgroundClicked()
{
	DeactivateWidget();
}
