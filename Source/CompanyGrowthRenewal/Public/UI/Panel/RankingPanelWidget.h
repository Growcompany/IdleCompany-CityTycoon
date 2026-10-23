#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Data/RankingData.h"
#include "RankingPanelWidget.generated.h"

class UScrollBox;
class UWidgetSwitcher;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UButton;
class URankingManagerSubsystem;
class UTableManagerSubsystem;
class UPanelHeaderWidget;
class URankingEntryCardWidget;
class URankingBuildingPreviewRenderer;
class UMaterialInstanceDynamic;
class UMaterialInterface;

UCLASS()
class COMPANYGROWTHRENEWAL_API URankingPanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	URankingPanelWidget();
	// 실데이터와 UI 검증이 같은 표시 경로를 사용하며 네트워크 조회를 일으키지 않는다.
	void ApplyLeaderboard(int32 TabIndex, const TArray<FRankingEntry>& Entries, const FRankingEntry& OwnEntry, bool bOwnScoreAvailable = true);
	void ApplyPodiumSnapshot(const FString& PlayerId, const FCitySnapshot& Snapshot);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<URankingEntryCardWidget> MyRankingEntryCard = nullptr;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UScrollBox> RevenueLeaderboardScrollBox = nullptr;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<URankingEntryCardWidget> MyLevelRankingEntryCard = nullptr;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UScrollBox> LevelLeaderboardScrollBox = nullptr;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> ContentSwitcher = nullptr;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> MarketCapTab = nullptr;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> WeeklyRevenueTab = nullptr;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelHeaderWidget> PanelHeader;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> BackgroundBtn = nullptr;

private:
	UPROPERTY() TObjectPtr<URankingManagerSubsystem> RankingMgr = nullptr;
	UPROPERTY() TObjectPtr<UTableManagerSubsystem> TableMgr = nullptr;
	UPROPERTY() TObjectPtr<UCommonButtonGroupBase> TabButtonGroup = nullptr;
	UPROPERTY() TObjectPtr<URankingBuildingPreviewRenderer> BuildingPreview = nullptr;
	UPROPERTY() TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> PreviewMaterials;
	UPROPERTY() TObjectPtr<UMaterialInterface> PreviewMaterialBase = nullptr;

	TArray<FRankingEntry> PodiumEntries[2];
	TArray<TPair<int32, FString>> ListEntryIdentities[2];
	FString OwnPlayerIds[2];
	TSet<FString> RequestedSnapshotIds;
	TSet<FString> CompletedSnapshotIds;
	TMap<FString, FText> SnapshotStatuses;
	TMap<FString, FTimerHandle> SnapshotTimeouts;
	FTimerHandle LeaderboardTimeouts[2];
	void FailLeaderboardRequest(int32 TabIndex);
	FTimerHandle ListLoadingPulse;
	int32 ListLoadingPhase = 0;
	int32 ListLoadingTab = INDEX_NONE;
	FLinearColor ListMessageInkDefault = FLinearColor::White;
	void StartListLoadingPulse(int32 TabIndex);
	void AdvanceListLoadingPulse();
	void StopListLoadingPulse(int32 TabIndex);
	bool bTabLoaded[2] = {false, false};
	bool bTabRequested[2] = {false, false};
	bool bSessionActive = false;
	int32 CurrentTabIndex = 0;

	UFUNCTION() void OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);
	UFUNCTION() void OnCloseButtonClicked();
	UFUNCTION() void OnBackgroundClicked();
	void HandleLeaderboardLoaded(int32 TabIndex, const TArray<FRankingEntry>& Entries);
	void HandleRankingError(const FString& ErrorMessage);
	void PopulateScrollBox(UScrollBox* ScrollBox, const TArray<FRankingEntry>& Entries, const FString& OwnPlayerId);
	void SwitchToTab(int32 TabIndex);
	void RefreshPodium(int32 TabIndex);
	void RequestPodiumSnapshots(int32 TabIndex);
	void VisitEntry(const FRankingEntry& Entry, const FString& OwnPlayerId);
	void ReleasePresentation();
};
