// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/MineResourceCardWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/StatRowWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/MineManager.h"
#include "Global/GlobalUtilFunctions.h"
#include "Table/ResourceInfo.h"
#include "Components/Image.h"
#include "Components/EditableTextBox.h"
#include "CommonTextBlock.h"
#include "Engine/Texture2D.h"

void UMineResourceCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (AcceptButton)
	{
		AcceptButton->OnClicked().AddUObject(this, &UMineResourceCardWidget::HandleSelectClicked);
	}
	if (MinusButton)
	{
		MinusButton->OnClicked().AddUObject(this, &UMineResourceCardWidget::HandleMinusClicked);
	}
	if (PlusButton)
	{
		PlusButton->OnClicked().AddUObject(this, &UMineResourceCardWidget::HandlePlusClicked);
	}
	if (NumberEditableTextBox)
	{
		NumberEditableTextBox->OnTextCommitted.AddDynamic(this, &UMineResourceCardWidget::HandleQuantityCommitted);
	}

	CurrentQuantity = FMath::Max<int64>(MinQuantity, 1);
	RefreshQuantityUI();
}

void UMineResourceCardWidget::NativeDestruct()
{
	if (AcceptButton)
	{
		AcceptButton->OnClicked().RemoveAll(this);
	}
	if (MinusButton)
	{
		MinusButton->OnClicked().RemoveAll(this);
	}
	if (PlusButton)
	{
		PlusButton->OnClicked().RemoveAll(this);
	}
	if (NumberEditableTextBox)
	{
		NumberEditableTextBox->OnTextCommitted.RemoveDynamic(this, &UMineResourceCardWidget::HandleQuantityCommitted);
	}
	Super::NativeDestruct();
}

void UMineResourceCardWidget::SetResource(ECountryType Country, EResourceType Resource)
{
	CachedCountry = Country;
	CachedResource = Resource;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (TableMgr)
	{
		bool bSucc = false;
		const FResourceInfo Info = TableMgr->GetResourceInfo(Resource, bSucc);
		if (bSucc)
		{
			if (ProductionNameText && !Info.DisplayName.IsEmpty())
			{
				ProductionNameText->SetText(Info.DisplayName);
			}
			if (EntityImage && !Info.Icon.IsNull())
			{
				if (UTexture2D* IconTex = Info.Icon.LoadSynchronous())
				{
					EntityImage->SetBrushFromTexture(IconTex);
				}
			}
		}
	}

	CurrentQuantity = FMath::Max<int64>(MinQuantity, 1);
	ClampQuantity();
	RefreshQuantityUI();
}

void UMineResourceCardWidget::ClampQuantity()
{
	CurrentQuantity = FMath::Max<int64>(CurrentQuantity, MinQuantity);

	const UGameInstance* GI = GetGameInstance();
	const UMineManager* MineMgr = GI ? GI->GetSubsystem<UMineManager>() : nullptr;
	if (!MineMgr) return;

	const int64 Cap = MineMgr->GetMaxStorage(CachedCountry);
	if (Cap > 0 && CurrentQuantity > Cap)
	{
		CurrentQuantity = Cap;
	}
}

void UMineResourceCardWidget::RefreshQuantityUI()
{
	if (NumberEditableTextBox)
	{
		NumberEditableTextBox->SetText(FText::AsNumber(CurrentQuantity));
	}
	RefreshCostText();
	RefreshTimeText();
}

int64 UMineResourceCardWidget::ComputeCost(int64 Quantity) const
{
	const UGameInstance* GI = GetGameInstance();
	const UMineManager* MineMgr = GI ? GI->GetSubsystem<UMineManager>() : nullptr;
	if (!MineMgr) return 0;
	return MineMgr->GetCreateLineCost(CachedCountry, CachedResource, Quantity);
}

double UMineResourceCardWidget::ComputeProductionTimeSec(int64 Quantity) const
{
	if (Quantity <= 0) return 0.0;
	const UGameInstance* GI = GetGameInstance();
	const UMineManager* MineMgr = GI ? GI->GetSubsystem<UMineManager>() : nullptr;
	if (!MineMgr) return 0.0;

	const float RatePerMin = MineMgr->GetEffectiveRatePerMinute(CachedCountry, CachedResource);
	if (RatePerMin <= 0.0f) return 0.0;

	// 분당 N개 → 1개당 60/N초 → Quantity 개 = Quantity * 60 / RatePerMin
	return static_cast<double>(Quantity) * 60.0 / static_cast<double>(RatePerMin);
}

void UMineResourceCardWidget::RefreshCostText()
{
	if (!Text_Cost) return;
	const int64 Cost = ComputeCost(CurrentQuantity);
	// 사용자 WBP DefaultStatValue 형식 ("200원") 과 일치
	Text_Cost->SetStatValue(FString::Printf(TEXT("%s원"),
		*UGlobalUtilFunctions::AbbreviateNumber(Cost, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil).ToString()));
}

void UMineResourceCardWidget::RefreshTimeText()
{
	if (!Text_Time) return;
	const double Sec = ComputeProductionTimeSec(CurrentQuantity);
	const int32 Total = FMath::Max(static_cast<int32>(Sec), 0);

	FString Formatted;
	if (Total < 60)
	{
		Formatted = FString::Printf(TEXT("%ds"), Total);
	}
	else if (Total < 3600)
	{
		Formatted = FString::Printf(TEXT("%dm %ds"), Total / 60, Total % 60);
	}
	else
	{
		const int32 Hrs = Total / 3600;
		const int32 RemMins = (Total % 3600) / 60;
		Formatted = FString::Printf(TEXT("%dh %dm"), Hrs, RemMins);
	}
	Text_Time->SetStatValue(Formatted);
}

void UMineResourceCardWidget::HandleSelectClicked()
{
	ClampQuantity();
	OnSelected.Broadcast(CachedResource, CurrentQuantity);
}

void UMineResourceCardWidget::HandleMinusClicked()
{
	--CurrentQuantity;
	ClampQuantity();
	RefreshQuantityUI();
}

void UMineResourceCardWidget::HandlePlusClicked()
{
	++CurrentQuantity;
	ClampQuantity();
	RefreshQuantityUI();
}

void UMineResourceCardWidget::HandleQuantityCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter && CommitType != ETextCommit::OnUserMovedFocus) return;
	CurrentQuantity = FCString::Atoi64(*Text.ToString());
	ClampQuantity();
	RefreshQuantityUI();
}
