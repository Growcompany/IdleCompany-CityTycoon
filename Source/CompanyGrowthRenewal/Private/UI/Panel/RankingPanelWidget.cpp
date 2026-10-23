#include "UI/Panel/RankingPanelWidget.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "Player/MainMapPlayerController.h"
#include "UI/Element/Cards/RankingEntryCardWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Ranking/RankingBuildingPreviewRenderer.h"
#include "Manager/RankingManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/PlayFabManagerSubsystem.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ResourceItemManager.h"
#include "Enum/ResourceType.h"
#include "Data/GameSaveData.h"
#include "Enum/WidgetType.h"
#include "Components/ScrollBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Spacer.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "CommonButtonBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Global/GlobalUtilFunctions.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Manager/SoundManagerSubsystem.h"
#include "TimerManager.h"
#include "UI/UISoundTags.h"

URankingPanelWidget::URankingPanelWidget()
{
	AppearPopTargetName = TEXT("RankingBody");
}

namespace RankingPanelPresentation
{
	// 로딩=가라앉은 슬레이트 #8FA2B8, 실패=재시도 가능 앰버 #C77E08 (둘 다 linear 환산값)
	static const FLinearColor LoadingInk(0.275f, 0.361f, 0.479f, 1.f);
	static const FLinearColor FailedInk(0.571f, 0.209f, 0.002f, 1.f);
	// 칸 수를 3으로 고정해 가운데 정렬 라벨이 좌우로 흔들리지 않게 한다
	static const TCHAR* LoadingDots[3] = { TEXT(".  "), TEXT(".. "), TEXT("...") };

	static UTextBlock* MessageText(UUserWidget* Owner, int32 TabIndex)
	{
		return Owner ? Cast<UTextBlock>(Owner->GetWidgetFromName(FName(*FString::Printf(TEXT("RankingEmpty%d"), TabIndex)))) : nullptr;
	}

	// 빈 상태 워터마크 글리프. 로딩/실패 때는 숨긴다 — 아이콘이 "비었다"는 뜻이라 오해를 만든다.
	static void SetEmptyGlyphVisible(UUserWidget* Owner, int32 TabIndex, bool bVisible)
	{
		if (!Owner) return;
		if (UWidget* Glyph = Owner->GetWidgetFromName(FName(*FString::Printf(TEXT("RankingEmptyIcon%d"), TabIndex))))
		{
			Glyph->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	static FName WidgetName(int32 TabIndex, int32 Rank, const TCHAR* Suffix)
	{
		return FName(*FString::Printf(TEXT("Podium%d%d%s"), TabIndex, Rank, Suffix));
	}
}

void URankingPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UGameInstance* GI = GetGameInstance())
	{
		RankingMgr = GI->GetSubsystem<URankingManagerSubsystem>();
		TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	}
	TabButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	TabButtonGroup->SetSelectionRequired(true);
	for (UCommonButtonBase* Tab : {MarketCapTab.Get(), WeeklyRevenueTab.Get()})
	{
		if (!Tab) continue;
		Tab->SetIsSelectable(true);
		TabButtonGroup->AddWidget(Tab);
		// 전환음은 이 패널이 낸다 — 버튼 자체 클릭음을 끊어 두 발을 막는다
		if (UButtonWidget* TabBtn = Cast<UButtonWidget>(Tab)) TabBtn->SetPlayClickSound(false);
	}
	TabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &URankingPanelWidget::OnTabSelectionChanged);
	TabButtonGroup->SelectButtonAtIndex(0);
	// 빈 목록 잉크는 텍스트 스타일이 구운 값이라, C++ 이 로딩/실패 색으로 덮기 전에 한 번 받아 둔다
	if (UTextBlock* Message = RankingPanelPresentation::MessageText(this, 0)) ListMessageInkDefault = Message->GetColorAndOpacity().GetSpecifiedColor();
	CurrentTabIndex = 0;
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(0);
		ContentSwitcher->SetClipping(EWidgetClipping::ClipToBounds);
	}
	for (int32 TabIndex = 0; TabIndex < 2; ++TabIndex)
	{
		for (int32 Rank = 1; Rank <= 3; ++Rank)
		{
			if (UCommonButtonBase* Visit = Cast<UCommonButtonBase>(GetWidgetFromName(RankingPanelPresentation::WidgetName(TabIndex, Rank, TEXT("Visit")))))
			{
				Visit->OnClicked().AddWeakLambda(this, [this, TabIndex, Rank]()
				{
					if (PodiumEntries[TabIndex].IsValidIndex(Rank - 1)) VisitEntry(PodiumEntries[TabIndex][Rank - 1], OwnPlayerIds[TabIndex]);
				});
			}
			if (!PreviewMaterialBase)
			{
				if (UImage* PreviewImage = Cast<UImage>(GetWidgetFromName(RankingPanelPresentation::WidgetName(TabIndex, Rank, TEXT("Building")))))
					PreviewMaterialBase = Cast<UMaterialInterface>(PreviewImage->GetBrush().GetResourceObject());
			}
		}
		RefreshPodium(TabIndex);
	}
	for (URankingEntryCardWidget* OwnCard : {MyRankingEntryCard.Get(), MyLevelRankingEntryCard.Get()})
	{
		if (OwnCard)
		{
			OwnCard->SetVisitEnabled(false);
			if (UImage* RowSurface = Cast<UImage>(OwnCard->GetWidgetFromName(TEXT("RankingRowSurface")))) RowSurface->SetColorAndOpacity(FLinearColor::Transparent);
		}
	}
	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr) PanelCloseButton->OnCloseClicked.AddDynamic(this, &URankingPanelWidget::OnCloseButtonClicked);
	for (int32 TabIndex = 0; TabIndex < 2; ++TabIndex)
	{
		UCommonButtonBase* Tab = TabIndex == 0 ? MarketCapTab.Get() : WeeklyRevenueTab.Get();
		if (!Tab) continue;
		// 이미 선택된 탭은 그룹 선택이 안 바뀌어 재시도가 걸리지 않는다 — 클릭을 직접 받는다
		Tab->OnClicked().AddWeakLambda(this, [this, TabIndex]()
		{
			if (!bSessionActive || TabIndex != CurrentTabIndex || bTabLoaded[TabIndex] || bTabRequested[TabIndex]) return;
			if (UGameInstance* GI = GetGameInstance())
			{
				if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>()) SM->PlayUISound(CGUISoundTags::TabSwitch);
			}
			SwitchToTab(TabIndex);
		});
	}
	if (BackgroundBtn) BackgroundBtn->OnClicked.AddDynamic(this, &URankingPanelWidget::OnBackgroundClicked);
}

void URankingPanelWidget::NativeDestruct()
{
	if (RankingMgr) RankingMgr->CancelPendingVisit();
	ReleasePresentation();
	if (TabButtonGroup) TabButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	for (int32 TabIndex = 0; TabIndex < 2; ++TabIndex)
	{
		for (int32 Rank = 1; Rank <= 3; ++Rank)
		{
			if (UCommonButtonBase* Visit = Cast<UCommonButtonBase>(GetWidgetFromName(RankingPanelPresentation::WidgetName(TabIndex, Rank, TEXT("Visit"))))) Visit->OnClicked().RemoveAll(this);
		}
	}
	if (UCloseButtonWidget* PanelCloseButton = PanelHeader ? PanelHeader->GetCloseButton() : nullptr) PanelCloseButton->OnCloseClicked.RemoveAll(this);
	for (UCommonButtonBase* Tab : {MarketCapTab.Get(), WeeklyRevenueTab.Get()})
	{
		if (Tab) Tab->OnClicked().RemoveAll(this);
	}
	if (BackgroundBtn) BackgroundBtn->OnClicked.RemoveAll(this);
	Super::NativeDestruct();
}

void URankingPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	ReleasePresentation();
	bSessionActive = true;
	if (RankingMgr)
	{
		RankingMgr->OnLeaderboardLoaded.AddUObject(this, &URankingPanelWidget::HandleLeaderboardLoaded);
		RankingMgr->OnCitySnapshotLoaded.AddUObject(this, &URankingPanelWidget::ApplyPodiumSnapshot);
		RankingMgr->OnRankingError.AddUObject(this, &URankingPanelWidget::HandleRankingError);
		RankingMgr->ForceUploadRevenueScore();
	}
	SwitchToTab(CurrentTabIndex);
}

void URankingPanelWidget::NativeOnDeactivated()
{
	if (RankingMgr) RankingMgr->CancelPendingVisit();
	ReleasePresentation();
	if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
	{
		if (PC->GetCurrentInputMode() == EInputMode::UI) PC->GoToNormalMode();
	}
	Super::NativeOnDeactivated();
}

void URankingPanelWidget::ReleasePresentation()
{
	bSessionActive = false;
	if (RankingMgr)
	{
		RankingMgr->OnLeaderboardLoaded.RemoveAll(this);
		RankingMgr->OnCitySnapshotLoaded.RemoveAll(this);
		RankingMgr->OnRankingError.RemoveAll(this);
	}
	if (GetWorld()) for (auto& Pair : SnapshotTimeouts) GetWorld()->GetTimerManager().ClearTimer(Pair.Value);
	SnapshotTimeouts.Reset(); RequestedSnapshotIds.Reset(); CompletedSnapshotIds.Reset(); SnapshotStatuses.Reset(); PreviewMaterials.Reset();
	StopListLoadingPulse(ListLoadingTab);
	if (RevenueLeaderboardScrollBox) RevenueLeaderboardScrollBox->ClearChildren();
	if (LevelLeaderboardScrollBox) LevelLeaderboardScrollBox->ClearChildren();
	for (int32 TabIndex = 0; TabIndex < 2; ++TabIndex)
	{
		if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LeaderboardTimeouts[TabIndex]);
		PodiumEntries[TabIndex].Reset(); OwnPlayerIds[TabIndex].Reset();
		ListEntryIdentities[TabIndex].Reset();
		if (URankingEntryCardWidget* OwnCard = TabIndex == 0 ? MyRankingEntryCard.Get() : MyLevelRankingEntryCard.Get())
		{
			FRankingEntry EmptyEntry;
			EmptyEntry.HQLevel = 0;
			OwnCard->SetEntryData(EmptyEntry);
			OwnCard->SetScoreAvailable(false);
			OwnCard->SetVisitEnabled(false);
		}
		bTabLoaded[TabIndex] = false; bTabRequested[TabIndex] = false;
		RefreshPodium(TabIndex);
	}
	if (BuildingPreview) BuildingPreview->Release();
	BuildingPreview = nullptr;
}

void URankingPanelWidget::OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>()) SM->PlayUISound(CGUISoundTags::TabSwitch);
	}
	SwitchToTab(ButtonIndex);
}
void URankingPanelWidget::OnCloseButtonClicked() { DeactivateWidget(); }
void URankingPanelWidget::OnBackgroundClicked() { DeactivateWidget(); }

void URankingPanelWidget::StartListLoadingPulse(int32 TabIndex)
{
	ListLoadingTab = TabIndex;
	ListLoadingPhase = 0;
	AdvanceListLoadingPulse();
	// 응답이 15초까지 걸려 정지 화면으로 보인다 — NativeTick 없이 타이머로만 점을 돌린다
	if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(ListLoadingPulse, this, &URankingPanelWidget::AdvanceListLoadingPulse, 0.35f, true);
}

void URankingPanelWidget::AdvanceListLoadingPulse()
{
	UTextBlock* Message = RankingPanelPresentation::MessageText(this, ListLoadingTab);
	if (!Message) return;
	Message->SetText(FText::FromString(FString(TEXT("랭킹 불러오는 중 ")) + RankingPanelPresentation::LoadingDots[ListLoadingPhase % 3]));
	++ListLoadingPhase;
}

void URankingPanelWidget::StopListLoadingPulse(int32 TabIndex)
{
	if (ListLoadingTab != TabIndex) return;
	ListLoadingTab = INDEX_NONE;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ListLoadingPulse);
}


void URankingPanelWidget::SwitchToTab(int32 TabIndex)
{
	if (TabIndex < 0 || TabIndex > 1) return;
	CurrentTabIndex = TabIndex;
	if (ContentSwitcher) ContentSwitcher->SetActiveWidgetIndex(TabIndex);
	if (bSessionActive && RankingMgr && !bTabLoaded[TabIndex] && !bTabRequested[TabIndex])
	{
		bTabRequested[TabIndex] = true;
		if (UTextBlock* EmptyText = Cast<UTextBlock>(GetWidgetFromName(FName(*FString::Printf(TEXT("RankingEmpty%d"), TabIndex)))))
		{
			EmptyText->SetColorAndOpacity(RankingPanelPresentation::LoadingInk);
			RankingPanelPresentation::SetEmptyGlyphVisible(this, TabIndex, false);
			StartListLoadingPulse(TabIndex);
			EmptyText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(LeaderboardTimeouts[TabIndex], FTimerDelegate::CreateWeakLambda(this, [this, TabIndex]() { FailLeaderboardRequest(TabIndex); }), 15.f, false);
		RankingMgr->FetchLeaderboard(TabIndex);
	}
}

void URankingPanelWidget::HandleLeaderboardLoaded(int32 TabIndex, const TArray<FRankingEntry>& Entries)
{
	if (!bSessionActive || TabIndex < 0 || TabIndex > 1) return;
	FRankingEntry OwnEntry;
	if (UPlayFabManagerSubsystem* PlayFab = GetGameInstance()->GetSubsystem<UPlayFabManagerSubsystem>())
	{
		OwnEntry.PlayFabId = PlayFab->GetUserInfo().PlayFabId;
		OwnEntry.DisplayName = PlayFab->GetUserInfo().DisplayName;
	}
	bool bOwnScoreAvailable = false;
	for (const FRankingEntry& Entry : Entries)
	{
		if (!OwnEntry.PlayFabId.IsEmpty() && Entry.PlayFabId == OwnEntry.PlayFabId) { OwnEntry = Entry; bOwnScoreAvailable = true; break; }
	}
	if (OwnEntry.DisplayName.IsEmpty()) OwnEntry.DisplayName = TEXT("나");
	if (USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>())
	{
		if (USaveGame_GameData* SaveData = SaveMgr->GetCurrentSaveData())
		{
			OwnEntry.HQLevel = SaveData->GameData.HQLevel;
			OwnEntry.ProfileImageID = SaveData->GameData.ProfileImageID;
		}
	}
	if (!bOwnScoreAvailable && TabIndex == 0)
	{
		if (UResourceItemManager* Resources = GetGameInstance()->GetSubsystem<UResourceItemManager>())
		{
			OwnEntry.TotalRevenue = Resources->GetResourceAmount(EResourceType::MarketCap);
			bOwnScoreAvailable = true;
		}
	}
	ApplyLeaderboard(TabIndex, Entries, OwnEntry, bOwnScoreAvailable);
	RequestPodiumSnapshots(TabIndex);
}

void URankingPanelWidget::ApplyLeaderboard(int32 TabIndex, const TArray<FRankingEntry>& Entries, const FRankingEntry& OwnEntry, bool bOwnScoreAvailable)
{
	if (TabIndex < 0 || TabIndex > 1) return;
	if (!TableMgr && GetGameInstance()) TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LeaderboardTimeouts[TabIndex]);
	bTabRequested[TabIndex] = false;
	bTabLoaded[TabIndex] = true; OwnPlayerIds[TabIndex] = OwnEntry.PlayFabId;
	PodiumEntries[TabIndex].SetNum(3);
	for (FRankingEntry& Entry : PodiumEntries[TabIndex]) Entry = FRankingEntry();
	for (const FRankingEntry& Entry : Entries)
	{
		if (Entry.Rank >= 1 && Entry.Rank <= 3) PodiumEntries[TabIndex][Entry.Rank - 1] = Entry;
	}
	RefreshPodium(TabIndex);
	URankingEntryCardWidget* OwnCard = TabIndex == 0 ? MyRankingEntryCard.Get() : MyLevelRankingEntryCard.Get();
	if (OwnCard) { OwnCard->SetEntryData(OwnEntry); OwnCard->SetScoreAvailable(bOwnScoreAvailable); OwnCard->SetVisitEnabled(false); }
	PopulateScrollBox(TabIndex == 0 ? RevenueLeaderboardScrollBox.Get() : LevelLeaderboardScrollBox.Get(), Entries, OwnEntry.PlayFabId);
	if (UTextBlock* EmptyText = Cast<UTextBlock>(GetWidgetFromName(FName(*FString::Printf(TEXT("RankingEmpty%d"), TabIndex)))))
	{
		StopListLoadingPulse(TabIndex);
		EmptyText->SetColorAndOpacity(ListMessageInkDefault);
		EmptyText->SetText(FText::FromString(TEXT("아직 등록된 기업이 없습니다")));
		const bool bHasRows = Entries.ContainsByPredicate([](const FRankingEntry& Entry) { return Entry.Rank > 3; });
		EmptyText->SetVisibility(bHasRows ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		RankingPanelPresentation::SetEmptyGlyphVisible(this, TabIndex, !bHasRows);
	}
}

void URankingPanelWidget::RefreshPodium(int32 TabIndex)
{
	for (int32 Rank = 1; Rank <= 3; ++Rank)
	{
		const FRankingEntry* Entry = PodiumEntries[TabIndex].IsValidIndex(Rank - 1) ? &PodiumEntries[TabIndex][Rank - 1] : nullptr;
		const bool bHasEntry = Entry && Entry->Rank == Rank && !Entry->PlayFabId.IsEmpty();
		auto SetText = [this, TabIndex, Rank](const TCHAR* Suffix, const FText& Value)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(RankingPanelPresentation::WidgetName(TabIndex, Rank, Suffix)))) Label->SetText(Value);
		};
		SetText(TEXT("Name"), bHasEntry ? FText::FromString(Entry->DisplayName) : FText::FromString(TEXT("순위 대기")));
		SetText(TEXT("Level"), bHasEntry ? FText::FromString(FString::Printf(TEXT("Lv.%d"), Entry->HQLevel)) : FText::GetEmpty());
		SetText(TEXT("Score"), bHasEntry ? UGlobalUtilFunctions::AbbreviateNumber(Entry->TotalRevenue, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor) : FText::GetEmpty());
		UMaterialInstanceDynamic* Preview = bHasEntry ? PreviewMaterials.FindRef(Entry->PlayFabId) : nullptr;
		FText Status;
		if (bHasEntry && !Preview)
		{
			const FText* CachedStatus = SnapshotStatuses.Find(Entry->PlayFabId);
			Status = CachedStatus ? *CachedStatus : FText::FromString(TEXT("건물 불러오는 중"));
		}
		SetText(TEXT("Status"), Status);
		if (UImage* BuildingImage = Cast<UImage>(GetWidgetFromName(RankingPanelPresentation::WidgetName(TabIndex, Rank, TEXT("Building")))))
		{
			if (!PreviewMaterialBase) PreviewMaterialBase = Cast<UMaterialInterface>(BuildingImage->GetBrush().GetResourceObject());
			if (Preview) BuildingImage->SetBrushFromMaterial(Preview);
			else if (PreviewMaterialBase) BuildingImage->SetBrushFromMaterial(PreviewMaterialBase);
			BuildingImage->SetVisibility(Preview ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
		if (UCommonButtonBase* Visit = Cast<UCommonButtonBase>(GetWidgetFromName(RankingPanelPresentation::WidgetName(TabIndex, Rank, TEXT("Visit")))))
		{
			Visit->SetIsEnabled(bHasEntry && Entry->PlayFabId != OwnPlayerIds[TabIndex]);
			Visit->SetVisibility(bHasEntry ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		}
	}
}

void URankingPanelWidget::RequestPodiumSnapshots(int32 TabIndex)
{
	if (!bSessionActive || !RankingMgr || !GetWorld()) return;
	for (const FRankingEntry& Entry : PodiumEntries[TabIndex])
	{
		if (Entry.PlayFabId.IsEmpty() || RequestedSnapshotIds.Contains(Entry.PlayFabId)) continue;
		if (RequestedSnapshotIds.Num() >= 6) { SnapshotStatuses.Add(Entry.PlayFabId, FText::FromString(TEXT("다시 열면 표시합니다"))); continue; }
		const FString PlayerId = Entry.PlayFabId;
		RequestedSnapshotIds.Add(PlayerId);
		FTimerHandle Timeout;
		GetWorld()->GetTimerManager().SetTimer(Timeout, FTimerDelegate::CreateWeakLambda(this, [this, PlayerId]()
		{
			SnapshotTimeouts.Remove(PlayerId);
			if (!PreviewMaterials.Contains(PlayerId)) SnapshotStatuses.Add(PlayerId, FText::FromString(TEXT("건물을 불러오지 못했습니다")));
			RefreshPodium(0); RefreshPodium(1);
		}), 15.f, false);
		SnapshotTimeouts.Add(PlayerId, Timeout);
		RankingMgr->FetchCitySnapshot(PlayerId);
	}
	RefreshPodium(TabIndex);
}

void URankingPanelWidget::ApplyPodiumSnapshot(const FString& PlayerId, const FCitySnapshot& Snapshot)
{
	const bool bCurrent = PodiumEntries[0].ContainsByPredicate([&PlayerId](const FRankingEntry& Entry) { return Entry.PlayFabId == PlayerId; })
		|| PodiumEntries[1].ContainsByPredicate([&PlayerId](const FRankingEntry& Entry) { return Entry.PlayFabId == PlayerId; });
	if (PlayerId.IsEmpty() || !bCurrent || CompletedSnapshotIds.Contains(PlayerId) || CompletedSnapshotIds.Num() >= 6) return;
	CompletedSnapshotIds.Add(PlayerId);
	if (FTimerHandle* Timeout = SnapshotTimeouts.Find(PlayerId))
	{
		if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(*Timeout);
		SnapshotTimeouts.Remove(PlayerId);
	}
	if (!BuildingPreview) BuildingPreview = NewObject<URankingBuildingPreviewRenderer>(this);
	if (Snapshot.Buildings.IsEmpty()) SnapshotStatuses.Add(PlayerId, FText::FromString(TEXT("등록된 건물 없음")));
	else if (UTextureRenderTarget2D* Target = BuildingPreview->CaptureRepresentative(GetWorld(), TableMgr, Snapshot))
	{
		if (PreviewMaterialBase)
		{
			UMaterialInstanceDynamic* Preview = UMaterialInstanceDynamic::Create(PreviewMaterialBase, this);
			Preview->SetTextureParameterValue(TEXT("RT"), Target);
			Preview->SetVectorParameterValue(TEXT("UVWindow"), FLinearColor(0, 0, 1, 1));
			Preview->SetScalarParameterValue(TEXT("OutlineThickness"), 0.f);
			PreviewMaterials.Add(PlayerId, Preview); SnapshotStatuses.Remove(PlayerId);
		}
		else SnapshotStatuses.Add(PlayerId, FText::FromString(TEXT("건물 표시 준비 실패")));
	}
	else SnapshotStatuses.Add(PlayerId, FText::FromString(TEXT("건물을 표시할 수 없습니다")));
	RefreshPodium(0); RefreshPodium(1);
}

void URankingPanelWidget::FailLeaderboardRequest(int32 TabIndex)
{
	if (TabIndex < 0 || TabIndex > 1 || bTabLoaded[TabIndex] || !bTabRequested[TabIndex]) return;
	bTabRequested[TabIndex] = false;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(LeaderboardTimeouts[TabIndex]);
	if (UTextBlock* EmptyText = Cast<UTextBlock>(GetWidgetFromName(FName(*FString::Printf(TEXT("RankingEmpty%d"), TabIndex)))))
	{
		StopListLoadingPulse(TabIndex);
		EmptyText->SetColorAndOpacity(RankingPanelPresentation::FailedInk);
		RankingPanelPresentation::SetEmptyGlyphVisible(this, TabIndex, false);
		EmptyText->SetText(FText::FromString(TEXT("랭킹을 불러오지 못했습니다. 탭을 다시 누르면 재시도합니다")));
		EmptyText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void URankingPanelWidget::HandleRankingError(const FString& ErrorMessage)
{
	FailLeaderboardRequest(0); FailLeaderboardRequest(1);
	for (auto& Pair : SnapshotTimeouts)
	{
		if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(Pair.Value);
		SnapshotStatuses.Add(Pair.Key, FText::FromString(TEXT("건물을 불러오지 못했습니다")));
	}
	SnapshotTimeouts.Reset(); RefreshPodium(0); RefreshPodium(1);
}

void URankingPanelWidget::VisitEntry(const FRankingEntry& Entry, const FString& OwnPlayerId)
{
	if (bSessionActive && RankingMgr && !Entry.PlayFabId.IsEmpty() && Entry.PlayFabId != OwnPlayerId) RankingMgr->EnterVisitMode(Entry.PlayFabId, Entry.DisplayName);
}

void URankingPanelWidget::PopulateScrollBox(UScrollBox* ScrollBox, const TArray<FRankingEntry>& Entries, const FString& OwnPlayerId)
{
	if (!ScrollBox || !TableMgr) return;
	const int32 TabIndex = ScrollBox == RevenueLeaderboardScrollBox.Get() ? 0 : 1;
	TArray<const FRankingEntry*> ListEntries;
	TArray<TPair<int32, FString>> Identities;
	for (const FRankingEntry& Entry : Entries)
	{
		if (Entry.Rank <= 3) continue;
		ListEntries.Add(&Entry);
		Identities.Emplace(Entry.Rank, Entry.PlayFabId);
	}
	TArray<URankingEntryCardWidget*> ExistingCards;
	for (UWidget* Child : ScrollBox->GetAllChildren())
	{
		if (URankingEntryCardWidget* Card = Cast<URankingEntryCardWidget>(Child)) ExistingCards.Add(Card);
	}
	if (Identities == ListEntryIdentities[TabIndex] && ExistingCards.Num() == ListEntries.Num())
	{
		for (int32 Index = 0; Index < ListEntries.Num(); ++Index)
		{
			const FRankingEntry& Entry = *ListEntries[Index];
			ExistingCards[Index]->SetEntryData(Entry);
			ExistingCards[Index]->SetVisitEnabled(!Entry.PlayFabId.IsEmpty() && Entry.PlayFabId != OwnPlayerId);
		}
		return;
	}
	ScrollBox->ClearChildren();
	ListEntryIdentities[TabIndex] = MoveTemp(Identities);
	TSubclassOf<UUserWidget> CardClass = TableMgr->GetWidgetClass(EWidgetType::RankingEntryCard);
	if (!CardClass) return;
	for (const FRankingEntry* ListEntry : ListEntries)
	{
		const FRankingEntry& Entry = *ListEntry;
		URankingEntryCardWidget* Card = CreateWidget<URankingEntryCardWidget>(this, CardClass);
		if (!Card) continue;
		Card->SetEntryData(Entry); Card->SetVisitEnabled(!Entry.PlayFabId.IsEmpty() && Entry.PlayFabId != OwnPlayerId);
		Card->OnEntryCardClicked.AddWeakLambda(this, [this, TabIndex](const FRankingEntry& ClickedEntry) { VisitEntry(ClickedEntry, OwnPlayerIds[TabIndex]); });
		ScrollBox->AddChild(Card);
		USpacer* Gap = NewObject<USpacer>(this); Gap->SetSize(FVector2D(1.f, 8.f)); ScrollBox->AddChild(Gap);
	}
}
