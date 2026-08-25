// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/CountryRouterPanelWidget.h"
#include "UI/Panel/CountryDetailWidget.h"
#include "UI/Element/Cards/CountryRouterCardWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Table/CountryInfoTable.h"
#include "Enum/WidgetType.h"
#include "Enum/ResourceType.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"

void UCountryRouterPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnCloseClicked.AddDynamic(this, &UCountryRouterPanelWidget::HandleCloseClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UCountryRouterPanelWidget::HandleBackgroundClicked);
	}

	// InitializeForMode가 Push 직후 호출되므로 여기서는 아직 모드 미확정 가능 —
	// 하지만 Push 순서상 NativeConstruct 전에 InitializeForMode 불가능하므로,
	// 기본 Mode(Resource)로 초기 렌더. 이후 InitializeForMode 호출 시 재빌드.
	RebuildCardList();
}

void UCountryRouterPanelWidget::NativeDestruct()
{
	if (CloseButton)
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &UCountryRouterPanelWidget::HandleCloseClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UCountryRouterPanelWidget::HandleBackgroundClicked);
	}

	SpawnedCards.Reset();
	Super::NativeDestruct();
}

void UCountryRouterPanelWidget::InitializeForMode(ECountryRouterMode InMode)
{
	Mode = InMode;

	const FModeSpec Spec = ResolveModeSpec();
	if (TitleText)
	{
		TitleText->SetText(Spec.Title);
	}

	RebuildCardList();
}

UCountryRouterPanelWidget::FModeSpec UCountryRouterPanelWidget::ResolveModeSpec() const
{
	// Mode → CountryDetail 탭 인덱스 매핑은 UCountryDetailWidget 의 static constexpr 직접 참조.
	// 매직넘버 사용 시 CountryDetail 의 탭 추가/순서 변경에 컴파일 에러 없이 런타임 어긋남.
	switch (Mode)
	{
	case ECountryRouterMode::Resource:
		return { NSLOCTEXT("CountryRouter", "TitleResource", "자원 채광국"), UCountryDetailWidget::TAB_INDEX_MINE };
	case ECountryRouterMode::Factory:
		return { NSLOCTEXT("CountryRouter", "TitleFactory", "생산 공장국"), UCountryDetailWidget::TAB_INDEX_FACTORY };
	default:
		return { FText::GetEmpty(), UCountryDetailWidget::TAB_INDEX_INFO };
	}
}

bool UCountryRouterPanelWidget::PassesFilter(const FCountryInfoTable& Info) const
{
	switch (Mode)
	{
	case ECountryRouterMode::Resource: return Info.bSupportsMine;
	case ECountryRouterMode::Factory:  return Info.bSupportsFactory;
	default: return false;
	}
}

void UCountryRouterPanelWidget::RebuildCardList()
{
	if (!CardScrollBox) return;

	CardScrollBox->ClearChildren();
	SpawnedCards.Reset();

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return;

	const TArray<FCountryInfoTable> All = TableMgr->GetAllCountryInfos();

	// 시가총액 기준 오름차순 정렬 → 자연스러운 해금 순서로 표시
	TArray<FCountryInfoTable> Filtered;
	Filtered.Reserve(All.Num());
	for (const FCountryInfoTable& Info : All)
	{
		if (PassesFilter(Info))
		{
			Filtered.Add(Info);
		}
	}
	Filtered.Sort([](const FCountryInfoTable& A, const FCountryInfoTable& B)
	{
		return A.RequiredMarketCap < B.RequiredMarketCap;
	});

	for (const FCountryInfoTable& Info : Filtered)
	{
		SpawnCard(Info);
	}
}

UCountryRouterCardWidget* UCountryRouterPanelWidget::SpawnCard(const FCountryInfoTable& Info)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return nullptr;

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	if (!TableMgr) return nullptr;

	TSubclassOf<UUserWidget> CardClass = TableMgr->GetWidgetClass(EWidgetType::CountryRouterCard);
	if (!CardClass) return nullptr;

	UCountryRouterCardWidget* Card = CreateWidget<UCountryRouterCardWidget>(this, CardClass);
	if (!Card) return nullptr;

	const FModeSpec Spec = ResolveModeSpec();
	// 모드를 먼저 — SetCountry가 ApplyCountryBasics 호출 시 모드별 SpecialtyText 분기됨
	Card->SetMode(Mode);
	Card->SetTargetTabIndex(Spec.TargetTabIndex);
	Card->SetCountry(Info.CountryType);

	// 시가총액 잠금 판정
	UResourceItemManager* RMgr = GI->GetSubsystem<UResourceItemManager>();
	const int64 PlayerRep = RMgr ? RMgr->GetResourceAmount(EResourceType::MarketCap) : 0;
	const bool bLocked = PlayerRep < Info.RequiredMarketCap;
	const FText Reason = FText::Format(
		NSLOCTEXT("CountryRouter", "NeedMarketCap", "시가총액 {0}"),
		FText::AsNumber(Info.RequiredMarketCap));
	Card->SetLockState(bLocked, Reason);

	if (CardScrollBox)
	{
		CardScrollBox->AddChild(Card);
	}
	SpawnedCards.Add(Card);
	return Card;
}

void UCountryRouterPanelWidget::HandleCloseClicked()
{
	DeactivateWidget();
}

void UCountryRouterPanelWidget::HandleBackgroundClicked()
{
	DeactivateWidget();
}
