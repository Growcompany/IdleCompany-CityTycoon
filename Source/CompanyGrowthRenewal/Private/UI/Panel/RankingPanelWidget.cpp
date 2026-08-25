#include "UI/Panel/RankingPanelWidget.h"
#include "UI/Element/Cards/RankingEntryCardWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Manager/RankingManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Data/GameSaveData.h"
#include "Enum/WidgetType.h"
#include "Components/ScrollBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Spacer.h"
#include "Components/Button.h"
#include "CommonButtonBase.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/TabButtonWidget.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"

void URankingPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		RankingMgr = GI->GetSubsystem<URankingManagerSubsystem>();
		TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	}

	// 탭 버튼 그룹 초기화 (배타적 선택)
	TabButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	TabButtonGroup->SetSelectionRequired(true);

	// 래퍼 → 내부 UCommonButtonBase 핸들 꺼내 그룹 등록 + OnClicked 바인딩
	if (UCommonButtonBase* Btn = MarketCapTab ? MarketCapTab->GetButton() : nullptr)
	{
		TabButtonGroup->AddWidget(Btn);
		Btn->SetIsSelectable(true);
		Btn->OnClicked().AddUObject(this, &URankingPanelWidget::OnMarketCapTabClicked);
	}
	if (UCommonButtonBase* Btn = WeeklyRevenueTab ? WeeklyRevenueTab->GetButton() : nullptr)
	{
		TabButtonGroup->AddWidget(Btn);
		Btn->SetIsSelectable(true);
		Btn->OnClicked().AddUObject(this, &URankingPanelWidget::OnWeeklyRevenueTabClicked);
	}

	TabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &URankingPanelWidget::OnTabSelectionChanged);

	UE_LOG(LogTemp, Warning, TEXT("[RankingPanel] NativeConstruct: ButtonGroup created, Revenue=%s, Level=%s"),
		MarketCapTab ? TEXT("OK") : TEXT("NULL"), WeeklyRevenueTab ? TEXT("OK") : TEXT("NULL"));

	// 초기 탭 설정 (매출 탭) — NativeConstruct에서 1회만
	if (TabButtonGroup && MarketCapTab)
	{
		TabButtonGroup->SelectButtonAtIndex(0);
		UE_LOG(LogTemp, Warning, TEXT("[RankingPanel] SelectButtonAtIndex(0) called"));
	}
	// 델리게이트 미발동 대비 명시적 초기화
	CurrentTabIndex = 0;
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(0);
		ContentSwitcher->SetClipping(EWidgetClipping::ClipToBounds);
	}

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &URankingPanelWidget::OnCloseButtonClicked);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &URankingPanelWidget::OnBackgroundClicked);
	}
}

void URankingPanelWidget::NativeDestruct()
{
	if (TabButtonGroup)
	{
		TabButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}

	if (UCommonButtonBase* Btn = MarketCapTab ? MarketCapTab->GetButton() : nullptr)
	{
		Btn->OnClicked().RemoveAll(this);
	}
	if (UCommonButtonBase* Btn = WeeklyRevenueTab ? WeeklyRevenueTab->GetButton() : nullptr)
	{
		Btn->OnClicked().RemoveAll(this);
	}

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &URankingPanelWidget::OnCloseButtonClicked);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &URankingPanelWidget::OnBackgroundClicked);
	}

	Super::NativeDestruct();
}

void URankingPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (RevenueLeaderboardScrollBox)
	{
		RevenueLeaderboardScrollBox->ClearChildren();
	}
	if (LevelLeaderboardScrollBox)
	{
		LevelLeaderboardScrollBox->ClearChildren();
	}

	// 리더보드 콜백 바인딩
	if (RankingMgr)
	{
		RankingMgr->OnLeaderboardLoaded.AddUObject(this, &URankingPanelWidget::HandleLeaderboardLoaded);
	}

	// 내 점수를 먼저 업로드 후 매출 탭 리더보드 조회
	if (RankingMgr)
	{
		RankingMgr->ForceUploadRevenueScore();
		RankingMgr->FetchLeaderboard(0);
	}
}

void URankingPanelWidget::NativeOnDeactivated()
{
	if (RankingMgr)
	{
		RankingMgr->OnLeaderboardLoaded.RemoveAll(this);
	}

	if (RevenueLeaderboardScrollBox)
	{
		RevenueLeaderboardScrollBox->ClearChildren();
	}
	if (LevelLeaderboardScrollBox)
	{
		LevelLeaderboardScrollBox->ClearChildren();
	}

	Super::NativeOnDeactivated();
}

void URankingPanelWidget::OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	// 사운드는 이 핸들러에만 — 탭 버튼은 OnClicked 경로로도 SwitchToTab 을 부르므로 거기 넣으면 더블플레이
	if (USoundManagerSubsystem* SM = GetGameInstance()->GetSubsystem<USoundManagerSubsystem>())
	{
		SM->PlayUISound(CGUISoundTags::TabSwitch);
	}

	UE_LOG(LogTemp, Warning, TEXT("[RankingPanel] OnTabSelectionChanged: ButtonIndex=%d, Button=%s"),
		ButtonIndex, SelectedButton ? *SelectedButton->GetName() : TEXT("null"));
	SwitchToTab(ButtonIndex);
}

void URankingPanelWidget::OnMarketCapTabClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[RankingPanel] OnMarketCapTabClicked"));
	SwitchToTab(0);
}

void URankingPanelWidget::OnWeeklyRevenueTabClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[RankingPanel] OnWeeklyRevenueTabClicked"));
	SwitchToTab(1);
}

void URankingPanelWidget::OnCloseButtonClicked()
{
	DeactivateWidget();
}

void URankingPanelWidget::OnBackgroundClicked()
{
	DeactivateWidget();
}

void URankingPanelWidget::SwitchToTab(int32 TabIndex)
{
	CurrentTabIndex = TabIndex;

	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(TabIndex);
	}

	// 해당 탭의 스크롤이 비어있으면 fetch (lazy loading)
	UScrollBox* TargetScroll = (TabIndex == 0) ? RevenueLeaderboardScrollBox : LevelLeaderboardScrollBox;
	if (RankingMgr && TargetScroll && TargetScroll->GetChildrenCount() == 0)
	{
		RankingMgr->FetchLeaderboard(TabIndex);
	}
}

void URankingPanelWidget::HandleLeaderboardLoaded(int32 TabIndex, const TArray<FRankingEntry>& Entries)
{
	// 응답 탭의 스크롤에 데이터 채우기
	UScrollBox* TargetScrollBox = (TabIndex == 0) ? RevenueLeaderboardScrollBox : LevelLeaderboardScrollBox;
	URankingEntryCardWidget* MyCard = (TabIndex == 0) ? MyRankingEntryCard : MyLevelRankingEntryCard;

	// 내 고정 카드에 데이터 설정
	if (MyCard)
	{
		FString MyEntityId;
		if (UPlayFabManagerSubsystem* PlayFabMgr = GetGameInstance()->GetSubsystem<UPlayFabManagerSubsystem>())
		{
			MyEntityId = PlayFabMgr->GetEntityId();
		}

		// 리더보드에서 내 엔트리 찾기
		FRankingEntry MyEntry;
		bool bFound = false;
		for (const FRankingEntry& Entry : Entries)
		{
			if (Entry.PlayFabId == MyEntityId)
			{
				MyEntry = Entry;
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			MyEntry.Rank = 0;
			if (UPlayFabManagerSubsystem* PFMgr = GetGameInstance()->GetSubsystem<UPlayFabManagerSubsystem>())
			{
				MyEntry.DisplayName = PFMgr->GetUserInfo().DisplayName;
			}
			if (MyEntry.DisplayName.IsEmpty())
			{
				MyEntry.DisplayName = TEXT("나");
			}
		}

		// HQLevel과 TotalRevenue는 항상 로컬 SaveData에서 (가장 정확)
		if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
		{
			if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
			{
				MyEntry.HQLevel = SaveData->GameData.HQLevel;
				MyEntry.TotalRevenue = SaveData->GameData.TotalRevenueEarned;
				MyEntry.ProfileImageID = SaveData->GameData.ProfileImageID;
			}
		}
		MyCard->SetEntryData(MyEntry);
	}

	if (TargetScrollBox)
	{
		PopulateScrollBox(TargetScrollBox, Entries);
	}
}

void URankingPanelWidget::PopulateScrollBox(UScrollBox* ScrollBox, const TArray<FRankingEntry>& Entries)
{
	if (!ScrollBox || !TableMgr) return;

	ScrollBox->ClearChildren();

	TSubclassOf<UUserWidget> CardClass = TableMgr->GetWidgetClass(EWidgetType::RankingEntryCard);
	if (!CardClass) return;

	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		URankingEntryCardWidget* Card = CreateWidget<URankingEntryCardWidget>(this, CardClass);
		if (!Card) continue;

		Card->SetEntryData(Entries[i]);

		// MoveBtn 클릭 → 방문 모드 진입
		Card->OnEntryCardClicked.AddLambda([this](const FRankingEntry& Entry)
		{
			if (RankingMgr)
			{
				RankingMgr->EnterVisitMode(Entry.PlayFabId, Entry.DisplayName);
			}
		});

		ScrollBox->AddChild(Card);

		// stagger 는 ScrollBox 자식 인덱스가 아닌 루프 i 기준 (Spacer 가 자식 인덱스를 2배로 밀음)
		Card->PlayIntro(i);

		if (i < Entries.Num() - 1)
		{
			USpacer* Spacer = NewObject<USpacer>(this);
			Spacer->SetSize(FVector2D(1.0f, 8.0f));
			ScrollBox->AddChild(Spacer);
		}
	}
}
