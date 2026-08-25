// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/WorldProductsPanelWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Manager/WorldMapManager.h"
#include "Manager/TradePort.h"
#include "Manager/TableManagerSubsystem.h"
#include "Global/GlobalUtilFunctions.h"
#include "Table/ProjectDataTable.h"
#include "Enum/CompanyType.h"
#include "Enum/QualityGrade.h"
#include "Entity/Country/CountryActor.h"
#include "Player/MainMapPlayerController.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Font.h"
#include "Engine/World.h"

void UWorldProductsPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 모바일 쿠킹 안전 규칙 = TSoftObjectPtr + LoadSynchronous (LoadObject 문자열 경로 금지)
	RowFont = TSoftObjectPtr<UFont>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicRegular_Font.NEXONLv1GothicRegular_Font"))).LoadSynchronous();

	if (UGameInstance* GI = GetGameInstance())
	{
		WorldMapMgr = GI->GetSubsystem<UWorldMapManager>();
		TradePortMgr = GI->GetSubsystem<UTradePort>();
		TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	}

	if (SellAllSingaporeBtn)
	{
		SellAllSingaporeBtn->OnClicked().AddUObject(this, &UWorldProductsPanelWidget::OnSellAllSingaporeClicked);
	}
	if (SellAllUSABtn)
	{
		SellAllUSABtn->OnClicked().AddUObject(this, &UWorldProductsPanelWidget::OnSellAllUSAClicked);
	}
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UWorldProductsPanelWidget::OnCloseButtonClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UWorldProductsPanelWidget::OnBackgroundClicked);
	}
}

void UWorldProductsPanelWidget::NativeDestruct()
{
	if (SellAllSingaporeBtn) SellAllSingaporeBtn->OnClicked().RemoveAll(this);
	if (SellAllUSABtn)       SellAllUSABtn->OnClicked().RemoveAll(this);

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &UWorldProductsPanelWidget::OnCloseButtonClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UWorldProductsPanelWidget::OnBackgroundClicked);
	}
	Super::NativeDestruct();
}

void UWorldProductsPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (WorldMapMgr)
	{
		WorldMapMgr->OnProductAdded.AddDynamic(this, &UWorldProductsPanelWidget::HandleProductChanged);
		WorldMapMgr->OnProductConsumed.AddDynamic(this, &UWorldProductsPanelWidget::HandleProductChanged);
	}

	Refresh();
}

void UWorldProductsPanelWidget::NativeOnDeactivated()
{
	if (WorldMapMgr)
	{
		WorldMapMgr->OnProductAdded.RemoveDynamic(this, &UWorldProductsPanelWidget::HandleProductChanged);
		WorldMapMgr->OnProductConsumed.RemoveDynamic(this, &UWorldProductsPanelWidget::HandleProductChanged);
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

void UWorldProductsPanelWidget::HandleProductChanged(FIntPoint ProductKey, int64 NewAmount)
{
	Refresh();
}

UTextBlock* UWorldProductsPanelWidget::MakeRowText(const FText& InText, int32 FontSize) const
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	if (!Text) return nullptr;

	Text->SetText(InText);
	if (RowFont)
	{
		Text->SetFont(FSlateFontInfo(RowFont.Get(), FontSize));
	}
	return Text;
}

void UWorldProductsPanelWidget::Refresh()
{
	if (!WorldMapMgr || !ProductsContainer) return;

	ProductsContainer->ClearChildren();

	TArray<FIntPoint> Keys = WorldMapMgr->GetAllProductKeys();
	if (Keys.Num() == 0)
	{
		// Body_S(24) — 빈 상태 안내
		if (UTextBlock* Empty = MakeRowText(FText::FromString(TEXT("보유한 완성품이 없습니다")), 24))
		{
			ProductsContainer->AddChildToVerticalBox(Empty);
		}
		return;
	}

	for (const FIntPoint& Key : Keys)
	{
		const int64 Qty = WorldMapMgr->GetProductAmount(Key);
		if (Qty <= 0) continue;

		const EQualityGrade Grade = WorldMapMgr->GetProductGrade(Key);
		const ECompanyType Company = static_cast<ECompanyType>(Key.X);
		const int32 ProjectIndex = Key.Y;
		const FText ProductName = GetProductDisplayName(Company, ProjectIndex);

		// Body_M(26) — 목록 본문
		UTextBlock* Row = MakeRowText(FText::FromString(FString::Printf(
			TEXT("%s (%s) x%s"),
			*ProductName.ToString(),
			*QualityGradeToAlphabetString(Grade),
			*UGlobalUtilFunctions::AbbreviateNumber(Qty, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString())), 26);
		if (!Row) continue;

		UVerticalBoxSlot* BoxSlot = ProductsContainer->AddChildToVerticalBox(Row);
		if (BoxSlot) BoxSlot->SetPadding(FMargin(0.f, 2.f));
	}
}

void UWorldProductsPanelWidget::OnSellAllSingaporeClicked()
{
	if (!WorldMapMgr || !TradePortMgr) return;

	TArray<FIntPoint> Keys = WorldMapMgr->GetAllProductKeys();
	for (const FIntPoint& Key : Keys)
	{
		const int64 Qty = WorldMapMgr->GetProductAmount(Key);
		if (Qty <= 0) continue;
		TradePortMgr->SellProduct(ECountryType::Singapore,
			static_cast<ECompanyType>(Key.X), Key.Y, Qty);
	}
}

void UWorldProductsPanelWidget::OnSellAllUSAClicked()
{
	if (!WorldMapMgr || !TradePortMgr) return;

	TArray<FIntPoint> Keys = WorldMapMgr->GetAllProductKeys();
	for (const FIntPoint& Key : Keys)
	{
		const int64 Qty = WorldMapMgr->GetProductAmount(Key);
		if (Qty <= 0) continue;
		TradePortMgr->SellProduct(ECountryType::USA,
			static_cast<ECompanyType>(Key.X), Key.Y, Qty);
	}
}

void UWorldProductsPanelWidget::OnCloseButtonClicked()
{
	DeactivateWidget();
}

void UWorldProductsPanelWidget::OnBackgroundClicked()
{
	DeactivateWidget();
}

FText UWorldProductsPanelWidget::GetProductDisplayName(ECompanyType Company, int32 ProjectIndex) const
{
	if (TableMgr)
	{
		bool bOk = false;
		FProjectData Data = TableMgr->GetProjectData(Company, ProjectIndex, bOk);
		if (bOk && !Data.ProjectName.IsEmpty())
		{
			return Data.ProjectName;
		}
	}
	// DT 미등록은 조용히 감추지 않고 눈에 띄게 (loud failure) — 개발용 Latin 식별자는 UI 문자열로 부적합
	return FText::FromString(FString::Printf(TEXT("미등록 물품 #%d"), ProjectIndex));
}
