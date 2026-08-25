// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/CountryRouterCardWidget.h"
#include "UI/Panel/CountryDetailWidget.h"
#include "UI/UIBase.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/CountryInfoTable.h"
#include "Table/ResourceInfo.h"
#include "Enum/WidgetType.h"
#include "Entity/Country/CountryActor.h"
#include "CommonTextBlock.h"
#include "CommonButtonBase.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"

void UCountryRouterCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MoveBtn)
	{
		MoveBtn->OnClicked().AddUObject(this, &UCountryRouterCardWidget::HandleMoveClicked);
	}
}

void UCountryRouterCardWidget::NativeDestruct()
{
	if (MoveBtn)
	{
		MoveBtn->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UCountryRouterCardWidget::SetCountry(ECountryType InCountry)
{
	Country = InCountry;
	ApplyCountryBasics();
}

void UCountryRouterCardWidget::SetMode(ECountryRouterMode InMode)
{
	Mode = InMode;
	if (Country != ECountryType::None)
	{
		ApplyCountryBasics();
	}
}

void UCountryRouterCardWidget::SetLockState(bool bInLocked, const FText& InLockReason)
{
	bLocked = bInLocked;

	if (LockBorder)
	{
		LockBorder->SetVisibility(bInLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (LockConditionText && bInLocked && !InLockReason.IsEmpty())
	{
		LockConditionText->SetText(InLockReason);
	}

	if (MoveBtn)
	{
		MoveBtn->SetIsInteractionEnabled(!bInLocked);
	}
}

void UCountryRouterCardWidget::ApplyCountryBasics()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	bool bSuccess = false;
	FCountryInfoTable Info = TableMgr->GetCountryInfo(Country, bSuccess);
	if (!bSuccess) return;

	if (IconImage && !Info.FlagIcon.IsNull())
	{
		if (UTexture2D* Flag = Info.FlagIcon.LoadSynchronous())
		{
			IconImage->SetBrushFromTexture(Flag);
		}
	}
	if (CountryNameText)
	{
		CountryNameText->SetText(Info.DisplayName);
	}
	if (SpecialtyText)
	{
		SpecialtyText->SetText(ResolveSpecialtyForMode(Info, TableMgr));
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(Info.FactoryDescription);
	}
}

FText UCountryRouterCardWidget::ResolveSpecialtyForMode(const FCountryInfoTable& Info, const UTableManagerSubsystem* TableMgr) const
{
	switch (Mode)
	{
	case ECountryRouterMode::Resource:
	{
		if (Info.MinableResources.Num() == 0 || !TableMgr) return FText::GetEmpty();
		TArray<FString> Names;
		Names.Reserve(Info.MinableResources.Num());
		for (EResourceType R : Info.MinableResources)
		{
			bool bOk = false;
			const FResourceInfo Res = TableMgr->GetResourceInfo(R, bOk);
			if (bOk && !Res.DisplayName.IsEmpty())
			{
				Names.Add(Res.DisplayName.ToString());
			}
		}
		return FText::FromString(FString::Join(Names, TEXT(", ")));
	}
	case ECountryRouterMode::Factory:
	default:
		return Info.SpecialtyName;
	}
}

void UCountryRouterCardWidget::HandleMoveClicked()
{
	if (bLocked || Country == ECountryType::None) return;

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !UIMgr) return;

	UUIBase* UIBase = UIMgr->GetUIBase();
	if (!UIBase) return;

	// 카메라 포커스 (국기 클릭과 동일 동작)
	if (ACountryActor* CountryActor = ACountryActor::FindCountryActor(this, Country))
	{
		CountryActor->RequestFocus(true);
	}

	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::CountryDetail);
	if (!Cls) return;

	UCommonActivatableWidget* Pushed = UIBase->PushPromptClass(TSubclassOf<UCommonActivatableWidget>(Cls));
	if (UCountryDetailWidget* Detail = Cast<UCountryDetailWidget>(Pushed))
	{
		Detail->SetCountry(Country);
		Detail->SwitchToTab(TargetTabIndex);
	}
}
