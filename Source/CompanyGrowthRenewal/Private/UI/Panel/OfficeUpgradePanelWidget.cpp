// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/OfficeUpgradePanelWidget.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Office/OfficeInterior.h"
#include "UI/Element/Building/UpgradeSlot.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "GameMode/OfficeGameMode.h"

void UOfficeUpgradePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UOfficeUpgradePanelWidget::OnCloseClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UOfficeUpgradePanelWidget::OnBackgroundClicked);
	}
	if (LeftExpansionSlot && LeftExpansionSlot->GetUpgradeButton())
	{
		LeftExpansionSlot->GetUpgradeButton()->OnClicked().AddUObject(this, &UOfficeUpgradePanelWidget::OnExpandLeftClicked);
	}
	if (RightExpansionSlot && RightExpansionSlot->GetUpgradeButton())
	{
		RightExpansionSlot->GetUpgradeButton()->OnClicked().AddUObject(this, &UOfficeUpgradePanelWidget::OnExpandRightClicked);
	}
}

void UOfficeUpgradePanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			ResourceMgr->OnResourceChanged.AddUObject(this, &UOfficeUpgradePanelWidget::OnResourceChanged);
		}
	}
	RefreshExpansionUI();
}

void UOfficeUpgradePanelWidget::NativeOnDeactivated()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			ResourceMgr->OnResourceChanged.RemoveAll(this);
		}
	}
	Super::NativeOnDeactivated();
}

void UOfficeUpgradePanelWidget::NativeDestruct()
{
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &UOfficeUpgradePanelWidget::OnCloseClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UOfficeUpgradePanelWidget::OnBackgroundClicked);
	}
	if (LeftExpansionSlot && LeftExpansionSlot->GetUpgradeButton())
	{
		LeftExpansionSlot->GetUpgradeButton()->OnClicked().RemoveAll(this);
	}
	if (RightExpansionSlot && RightExpansionSlot->GetUpgradeButton())
	{
		RightExpansionSlot->GetUpgradeButton()->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UOfficeUpgradePanelWidget::RefreshExpansionUI()
{
	AOfficeGameMode* GameMode = Cast<AOfficeGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	AOfficeInterior* OfficeInterior = GameMode ? GameMode->GetOfficeInterior() : nullptr;
	if (!OfficeInterior)
	{
		return;
	}

	const FIntPoint TileCount = OfficeInterior->GetTileCount();
	const FIntPoint MaxTileCount = OfficeInterior->GetMaxTileCount();

	if (CurrentSizeText)
	{
		CurrentSizeText->SetText(FText::FromString(FString::Printf(TEXT("%d × %d칸"), TileCount.X, TileCount.Y)));
	}
	if (MaxSizeText)
	{
		MaxSizeText->SetText(FText::FromString(FString::Printf(TEXT("최대 %d × %d칸"), MaxTileCount.X, MaxTileCount.Y)));
	}

	struct FExpansionRow { UUpgradeSlot* TargetSlot; int32 Current; int32 Max; EOfficeExpandDirection Dir; };
	const FExpansionRow Rows[] = {
		{ LeftExpansionSlot,  TileCount.X, MaxTileCount.X, EOfficeExpandDirection::Left },
		{ RightExpansionSlot, TileCount.Y, MaxTileCount.Y, EOfficeExpandDirection::Right },
	};
	for (const FExpansionRow& Row : Rows)
	{
		if (!Row.TargetSlot)
		{
			continue;
		}
		const int64 Cost = OfficeInterior->GetExpandCost(Row.Dir);
		const int32 Next = FMath::Min(Row.Current + 1, Row.Max);

		Row.TargetSlot->SetLevelBadgeOverride(FText::FromString(FString::Printf(TEXT("%d / %d칸"), Row.Current, Row.Max)));
		// 상한 표시는 MaxLevel 축(bIsMaxLevel)이 담당 — 잠금 오버레이(bIsLocked)는 쓰지 않는다
		Row.TargetSlot->UpdateInfo(Row.Current, Row.Current, Next, Cost, /*bIsLocked=*/false, Row.Max);
	}
}

void UOfficeUpgradePanelWidget::TryExpand(EOfficeExpandDirection Direction)
{
	AOfficeGameMode* GameMode = Cast<AOfficeGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	AOfficeInterior* OfficeInterior = GameMode ? GameMode->GetOfficeInterior() : nullptr;
	UGameInstance* GI = GetGameInstance();
	UResourceItemManager* ResourceMgr = GI ? GI->GetSubsystem<UResourceItemManager>() : nullptr;
	if (!OfficeInterior || !ResourceMgr)
	{
		return;
	}

	const int64 Cost = OfficeInterior->GetExpandCost(Direction);
	if (ResourceMgr->GetResourceAmount(EResourceType::Diamond) < Cost)
	{
		if (UUIManagerSubsystem* UIMgr = GI->GetSubsystem<UUIManagerSubsystem>())
		{
			UIMgr->ShowNotification(NSLOCTEXT("Notification", "NotEnoughDiamond", "다이아가 부족합니다"), 3.0f, ENotificationType::Failed);
		}
		return;
	}

	const bool bExpanded = (Direction == EOfficeExpandDirection::Left)
		? OfficeInterior->ExpandLeft()
		: OfficeInterior->ExpandRight();
	if (!bExpanded)
	{
		return;
	}

	ResourceMgr->SpendResource(EResourceType::Diamond, Cost, true);
	RefreshExpansionUI();

	if (UUpgradeSlot* EffectSlot = (Direction == EOfficeExpandDirection::Left) ? LeftExpansionSlot : RightExpansionSlot)
	{
		EffectSlot->PlayUpgradeEffect();
	}
}

void UOfficeUpgradePanelWidget::OnExpandLeftClicked()
{
	TryExpand(EOfficeExpandDirection::Left);
}

void UOfficeUpgradePanelWidget::OnExpandRightClicked()
{
	TryExpand(EOfficeExpandDirection::Right);
}

void UOfficeUpgradePanelWidget::OnResourceChanged(EResourceType Type, int64 NewValue)
{
	if (Type != EResourceType::Diamond)
	{
		return;
	}
	RefreshExpansionUI();
}

void UOfficeUpgradePanelWidget::OnCloseClicked()
{
	CloseWithAnimation();
}

void UOfficeUpgradePanelWidget::OnBackgroundClicked()
{
	CloseWithAnimation();
}
