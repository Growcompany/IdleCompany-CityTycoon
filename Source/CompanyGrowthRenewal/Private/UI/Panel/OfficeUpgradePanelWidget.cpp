// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/OfficeUpgradePanelWidget.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "Manager/CloudEconomySubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Office/OfficeInterior.h"
#include "Office/OfficeManager.h"
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

	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.AddDynamic(this, &UOfficeUpgradePanelWidget::OnCloseClicked);
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

	// 잠금이 풀리는 순간 서버값으로 정정된 잔액으로 비용 행을 다시 계산한다
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCloudEconomySubsystem* Cloud = GI->GetSubsystem<UCloudEconomySubsystem>())
		{
			Cloud->OnEconomyReady.AddUObject(this, &UOfficeUpgradePanelWidget::HandleEconomyReady);
		}
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
	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.RemoveDynamic(this, &UOfficeUpgradePanelWidget::OnCloseClicked);
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
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCloudEconomySubsystem* Cloud = GI->GetSubsystem<UCloudEconomySubsystem>())
		{
			Cloud->OnEconomyReady.RemoveAll(this);
		}
	}
	Super::NativeDestruct();
}

void UOfficeUpgradePanelWidget::HandleEconomyReady(bool bReady)
{
	// 잠긴 동안에도 버튼은 살려 둔다 — 조용히 죽은 버튼은 "다이아가 사라졌다" 로 읽힌다.
	// 탭하면 NotifyCloudOpFailure 가 3상태 중 하나를 말해준다(§7.2)
	RefreshExpansionUI();
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
	// 차감·확장·저장은 전부 매니저가 서버 응답 뒤에 한다 — 위젯이 직접 지출하지 않는다(설계서 §6.3).
	// 로컬 잔액 판정도 매니저가 하고, 부족은 응답 코드(INSUFFICIENT_FUNDS)로 돌아온다.
	UOfficeManager* OfficeMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeManager>() : nullptr;
	if (!OfficeMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeUpgradePanel] 확장 실패 - OfficeManager 없음"));
		return;
	}

	// 실패 안내에 부족액이 필요하다 — 응답이 오기 전 값으로 고정한다(응답 뒤엔 서버 지갑이 덮인다)
	const int64 Cost = OfficeMgr->GetExpandCost(Direction);

	OfficeMgr->RequestExpand(Direction, FOnCloudEconomyOpDone::CreateWeakLambda(this,
		[this, Direction, Cost](bool bOk, FName Code)
	{
		if (!bOk)
		{
			if (UUIManagerSubsystem* UIMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UUIManagerSubsystem>() : nullptr)
			{
				UIMgr->NotifyCloudOpFailure(Code, EResourceType::Diamond, Cost);
			}
			return;
		}

		RefreshExpansionUI();

		if (UUpgradeSlot* EffectSlot = (Direction == EOfficeExpandDirection::Left) ? LeftExpansionSlot : RightExpansionSlot)
		{
			EffectSlot->PlayUpgradeEffect();
		}
	}));
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
