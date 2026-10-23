#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/SideSkylineTypes.h"
#include "Data/SkylineBeatTiming.h"
#include "Engine/TimerHandle.h"
#include "SkylineScreenWidget.generated.h"

class UCommonButtonBase;
class UCommonTextBlock;
class UHorizontalBox;
class UImage;
class UPanelWidget;
class UProgressBar;
class USideContentSubsystem;
class USkylineCompanyRowWidget;
class UVerticalBox;
class UWidgetSwitcher;
class AOfficeGameMode;

/** 사무실 씬의 재생 비트. 판정이 아니라 연출 상태다 ― 화면에 쓰는 값은 전부 FSkylineRunResult 에서만 읽는다. */
enum class ESkylineBeat : uint8
{
	Idle,
	Work,
	Clear
};

/**
 * 스카이라인 전용 풀스크린 화면 (WBP: UI_SkylineScreen). PushMenuClass 로 HUD 를 덮는다.
 *
 * MainMap에서는 로비, 선택한 건물의 OfficeMap에서는 투명 진행 HUD로 생성된다.
 * SceneSwitcher는 각 맵에 맞는 표시만 고르고, 등반 결과는 GI 서브시스템이 보관한다.
 *
 * 판정은 USideContentSubsystem::Climb 한 번이 전부다. 사무실 재생은 그 결과를 그릴 뿐 다시 판정하지 않는다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API USkylineScreenWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	USkylineScreenWidget();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;

	// ── 재생 타이밍 노브 ──
	// WorkMs·SlamMs 가 둘 다 0 이면 비트가 타이머 없이 흘러 결과 카드로 직행한다(모션 최소화 경로).

	UPROPERTY(EditAnywhere, Category = "Skyline|Timing", meta = (ClampMin = "0"))
	int32 WorkMs = 380;

	UPROPERTY(EditAnywhere, Category = "Skyline|Timing", meta = (ClampMin = "0"))
	int32 SlamMs = 300;

	/** 층마다 이 비율로 빨라진다. 1.0 이면 가속 없음. */
	UPROPERTY(EditAnywhere, Category = "Skyline|Timing", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float AccelPerFloor = 0.93f;

	/** 가속이 걸려도 한 층 전체는 이 아래로 안 내려간다. */
	UPROPERTY(EditAnywhere, Category = "Skyline|Timing", meta = (ClampMin = "0"))
	int32 MinFloorBeatMs = 420;

	/** 보스 층만 무겁게 되돌린다 ― 관문이 일반 층과 같은 속도로 지나가면 게이트가 안 읽힌다. */
	UPROPERTY(EditAnywhere, Category = "Skyline|Timing", meta = (ClampMin = "1.0"))
	float BossBeatMul = 1.6f;

	// ── 공용 골격 ──

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> SceneSwitcher = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> NightBG = nullptr;

	/** 로비의 [나가기]와 사무실의 [층 목록]을 같은 자리에서 전환한다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> BackButton = nullptr;

	/** 주 CTA. 로비에서는 [개발하기 n/5], 사무실 수동 모드에서는 [다음 층 N층] 이 된다.
	 *  라벨을 바꾸려면 프로젝트 표준 텍스트 버튼(UButtonWidget) 파생이어야 한다 ― UCommonButtonBase 에는 세터가 없다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> ActionButton = nullptr;

	// ── 판정 밴드(두 씬 공용) ──

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> RequirementText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TeamPowerText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UProgressBar> VerdictBar = nullptr;

	/** 막힌 사유. ESkylineBlockReason 하나로만 고른다 ― 화면마다 다른 문장을 쓰지 않는다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> BlockReasonText = nullptr;

	// ── 로비: 상단 바 ──

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> BestFloorText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> EntryText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> BonusText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> NextGateText = nullptr;

	// ── 로비: 출전 회사 칩 ──

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> CompanyChipButton = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CompanyNameText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> CompanyGlyph = nullptr;

	// ── 로비: 층 카드 열 + 크루 레일 ──

	/** UVerticalBox·UScrollBox 둘 다 받으려고 공통 베이스로 잡는다. 슬롯 패딩은 양쪽을 캐스팅해 적용한다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelWidget> FloorListBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UVerticalBox> CrewRailBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CrewCountText = nullptr;

	// ── 사무실 씬 ──

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> FloorSpineBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> EndRunButton = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> OfficeFloorText = nullptr;

	/** 층 밴드의 입주 기업 이름 ― DT_SkylineFloor 가 단일 진실이라 서브시스템에서 받아온다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> OfficeTenantText = nullptr;

	// ── 결과 카드 ──

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ResultRoot = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ResultGainText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> ResultStopText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ResultRewardsBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> ResultConfirmButton = nullptr;

	// ── 출전 회사 선택 ──

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CompanyPickRoot = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> CompanyPickBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonButtonBase> CompanyPickCloseButton = nullptr;

private:
	USideContentSubsystem* GetSide() const;

	void RefreshAll();
	void RefreshCompanyChip();
	void RefreshVerdictBand();
	void RefreshActionButton();
	void RefreshNavigationControls();
	void RebuildFloorList();
	void RebuildCrewRail();
	void RebuildCompanyPick();

	void HandleAction();
	void HandleBack();
	void HandleCompanyChip();
	void HandleCompanyPickClose();
	void HandleCompanyRowClicked(int32 BuildingIndex);
	void HandleEndRun();
	void HandleResultConfirm();
	void HandleBeatTimer();
	void HandleOfficeEmployeesReady();
	void UnbindOfficeEmployees();

	void StartPlayback();
	void RunPlayback();
	bool StepBeat();
	void ApplyBeat();

	/** 지금 비트의 길이(ms). 가속·보스 배수는 FSkylineBeatTimeline 이 계산한다. */
	int32 GetCurrentBeatMs() const;
	void EnsureSpineThrough(int32 Count, bool bAnimate = false);
	void ClearBeatTimer();
	void StopPlayback();
	void FinishPlayback();
	void ShowResultCard();
	void ReturnToLobby();

	void SetActionLabel(const FText& Label);
	void SetMainMapHUDSuppressed(bool bSuppress);

	bool IsPlaybackActive() const { return Beat != ESkylineBeat::Idle; }

	/** 출전 회사. 서브시스템이 저장값이 죽었을 때 파워 최대 건물을 대신 주므로 갱신마다 다시 읽는다. */
	int32 SelectedBuildingIndex = INDEX_NONE;

	/** 등반 1회의 전부. 재생은 이 값만 읽는다. */
	UPROPERTY()
	FSkylineRunResult RunResult;

	/** 선택 행은 매 오픈 새로 만든다 ― 해제 대상을 들고 있어야 중복 구독이 안 쌓인다. */
	UPROPERTY()
	TArray<TObjectPtr<USkylineCompanyRowWidget>> CompanyRows;

	/** MainStack과 별개인 도시 HUD를 화면 활성화 동안만 숨기고 원래 표시 상태로 복원한다. */
	TWeakObjectPtr<UCommonActivatableWidget> SuppressedMainMapHUD;
	ESlateVisibility MainMapHUDVisibility = ESlateVisibility::SelfHitTestInvisible;

	FTimerHandle BeatTimer;

	ESkylineBeat Beat = ESkylineBeat::Idle;
	int32 PlayIndex = 0;
	int32 SpineShown = 0;
	bool bShowingResult = false;
	bool bDeferringRecordRefresh = false;
	bool bOfficePresentation = false;
	bool bTravelRequested = false;
	bool bOfficePlaybackStarted = false;
	TWeakObjectPtr<AOfficeGameMode> SkylineOffice;
};
