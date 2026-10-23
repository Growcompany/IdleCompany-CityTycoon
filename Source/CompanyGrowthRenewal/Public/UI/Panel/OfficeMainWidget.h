#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/OperationData.h"
#include "Data/ProjectEventData.h"
#include "Enum/ProjectMode.h"
#include "Enum/ProjectLifecycle.h"
#include "Enum/CompanyType.h"
#include "Enum/OfficeExpansionType.h"
#include "Enum/ResourceType.h"
#include "OfficeMainWidget.generated.h"

class UButtonWidget;
class AOfficeInterior;
class UResourceWidget;
class UIconWithButtonWidget;
class UOfficeFeatureButtonWidget;
class UButton;
class UTexture2D;
class UEmployeeEquipmentDialogWidget;
class UEmployeeEquipmentItemObject;
class AWorkstationActorBase;
class UPlacementHandler;
class USelectionChevronWidget;
class UCanvasPanelSlot;
class UCanvasPanel;
class AOfficeCameraPawn;
class UBuildPlacementPanelWidget;
class UConfirmCancelWidget;
class UOfficeExpansionContentWidget;

class UCommonTextBlock;
class UBorder;
class UVerticalBox;
class UIconCardWidget;
class UProjectInfoBarWidget;
class UImage;
class UProgressBar;
class URadialProgressWidget;
class UMaterialInstanceDynamic;
class UOfficeStageProgressManager;
class UProjectOperationManager;
class UOfficeProjectCardWidget;
class UDevelopmentBannerWidget;
class UWidgetSwitcher;
class UStatRowWidget;
class UMissionTrackerWidget;
class ULaunchConfirmWidget;
class UProjectReportWidget;
class UScoreOrbContainerWidget;
class UOfficeEventRailWidget;
struct FProjectData;
struct FProjectReportData;
struct FProjectEventData;

/**
 * OfficeMap 하단 메인 위젯
 * - 동시 진행 모드: 3개 프로그레스 바 (기획/개발/QA) + 자동 출시 팝업
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeMainWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 패널 닫힘 후 요청한다. 하단 스택 복귀가 진행 중이면 재활성화까지 기다린다.
	void RequestPlacementEdit(AActor* Target = nullptr);
	void SetPlacementWorkstationMarker(AWorkstationActorBase* Workstation, UPlacementHandler* Handler);

	// M4 미션 가이드 — [채용] 버튼 하이라이트 타겟
	UWidget* GetRecruitmentButtonWidget() const;

	// M10 미션 가이드 — [새 프로젝트] 버튼 하이라이트 타겟 (내부명 OpenBoardButton 유지)
	UWidget* GetOpenBoardButtonWidget() const;

	// M7 미션 가이드 — [책상 배치] 버튼 하이라이트 타겟
	UWidget* GetWorkstationOpenButtonWidget() const;

	// 채용 창 [책상 배치] 바로가기 — 하단 [책상] 버튼과 같은 경로(배치 패널 push + 미션 알림).
	void OpenWorkstationPanel() { OnWorkstationButtonClicked(); }

	// [꾸미기] 버튼 링 타겟 (현재 미사용)
	UWidget* GetDecorationOpenButtonWidget() const;

	// M9 미션 가이드 — 도크 [직원] 버튼 하이라이트 타겟
	UWidget* GetEmployeeButtonWidget() const;
	// 튜토리얼 M18 — 직원 패널 안에 뽑기 진입이 없을 때 쓰는 도크 [장비] 버튼
	UWidget* GetEquipmentGachaButtonWidget() const;

	// 첫 개발 관찰 단계에서 강조할 배속 버튼.
	UWidget* GetStripSpeedButtonWidget() const;
	// 개발 요구점수 6칸을 감싸는 행.
	UWidget* GetStripRequirementRowWidget() const;
	// 운영 중 「남은 시간」 숫자 ― 다음 기획안을 기다리는 동안 안내가 붙는다.
	UWidget* GetStripOperationTimeWidget() const;

	// 좌레일 상단에 배치된 미션 트래커 (없으면 nullptr → MissionManager 가 폴백 생성)
	UMissionTrackerWidget* GetMissionTracker() const { return MissionTracker; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void TryPlayManufacturingIntro();
	FTimerHandle ManufacturingIntroTimer;
	TWeakObjectPtr<class UPanelIntroOverlayWidget> ManufacturingIntroOverlay;

	// 대기 상태에서 [기획안 보기] CTA 를 은은히 펄스 → 다음 행동 유도 (RefreshOfficeUI 가 bIdlePulse 세팅)
	bool bIdlePulse = false;
	float IdlePulseElapsed = 0.0f;

	// 대기 CTA 위를 주기 횡단하는 광택 띠 — 버튼 Overlay 안, ClipToBounds 전제.
	// 버튼 자체를 움직이면 지글거림/계단을 피할 수 없어 장식 이미지만 움직인다(HQ claim 샤인과 같은 방식).
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* CtaShineImage = nullptr;

	static constexpr float CtaShinePeriod = 3.0f;     // 스윕 주기(이동 + 휴지)
	static constexpr float CtaShineTravel = 0.65f;    // 횡단 시간
	static constexpr float CtaShineSweepHalf = 250.f; // 횡단 반폭 — 버튼 폭 480 기준, 캡처로 튜닝

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UIconWithButtonWidget* WorkstationOpenButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* DecorationOpenButton;

	// 기술 연구소 진입. Optional 인 이유는 OfficeDockTray 와 같다 — WBP 주입과 C++ 리빌드 타이밍이
	// 어긋나도 오피스 전체가 깨지지 않게. 널이면 버튼이 없을 뿐 나머지는 그대로 돈다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UOfficeFeatureButtonWidget> TrainingCenterOpenButton = nullptr;

	// 출장 진입. 위와 같은 이유로 Optional 이다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UOfficeFeatureButtonWidget> BusinessTripOpenButton = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* EmployeeButton;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> EquipmentManualButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* RecruitmentButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButtonWidget> EquipmentGachaButton;

	// 도감 버튼 — 클리어한 프로젝트 목록 열람
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* CollectionBtn;

	// 피버타임 풀스크린 따뜻한 화염 틴트 (WBP에 풀스크린 Image로 배치 — 없으면 틴트 연출만 생략, 나머지 피버 효과는 작동)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* FeverScreenTint;

	// 남은 시간은 상단 한 자리에서 라벨/단위를 함께 읽는다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* ProcessTimeText;
	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* ProcessTimeLabel = nullptr;
	void UpdateDevelopmentEffectArea();

	// 카운트다운 링 — 남은시간/전체 비율을 형태로. 스트립의 가로 바 6개와 형태를 갈라 "점수 아님"을 즉시 구분.
	// required: 데이터 주입 대상이라 조용히 null 이 되면 안 된다 (구 ProgressBar 가 Optional 이라 트리에 없는 채 방치됐던 선례)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	URadialProgressWidget* TimerRing;

	// 프로젝트 정보 표시
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* ProjectSubName;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UIconCardWidget* UI_ProjectImageCard;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProjectInfoBarWidget* UIE_ProjectNameBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UOfficeProjectCardWidget* UI_OfficeProjectCardCurrent;

	// ========== 진행 줄 페이지 (Index 0: 대기 안내, Index 1: 개발 · 제조) ==========
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidgetSwitcher* ProjectWidgetSwitcher;

	// 운영 카드(오른쪽 열) ― 운영 중에만 보인다. 안의 표지·이름·수익·그래프·남은 시간·종료 버튼은 아래 Op*·RevPerSecText·
	// TotalRevText·SalesCurve·StripStopButton 바인딩 그대로라 갱신 경로가 바뀌지 않는다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> OpCard = nullptr;

	// 카드 머리 「운영 중」 ― 결과 대기(ReportPending)에는 접는다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> OpCardStatusRow = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* OpenBoardButton;

	// [기획안 보기] 뒤 은은한 글로우 — 유휴 시 opacity 브리딩(스케일 대신). NativeTick 구동.
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* CTAGlow;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UStatRowWidget* IdleTierProgress;

	// 미션 트래커 — 좌레일 상단(구 OfficeLayer 배치에서 이사).
	// InitTracker 는 MissionManagerSubsystem 이 GetMissionTracker 로 가져와 호출 → 자가 갱신.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMissionTrackerWidget> MissionTracker = nullptr;

	// 하단 도크 트레이(허브 버튼 Overlay) — 개발중 집중 모드에서 아래로 슬라이드아웃.
	// C++ 렌더 트랜스폼 구동(트리 전체교체 워크플로에서 WBP 애니가 끊기던 문제 회피).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> OfficeDockTray = nullptr;

	// 개발 이벤트 레일 — 개발 스트립(StripRoot) 바로 아래 같은 세로 스택(StripRailColumn) 에 레이아웃 배치.
	// DevEvent 카드 + 상태 토스트가 여기 쌓인다(자체 크롬 0 + SelfHitTestInvisible 은 WBP 에 baked — 비면 사라지고 빈 영역이 입력 안 막음).
	// Optional 인 이유: WBP 주입/C++ 리빌드 타이밍이 어긋나도 오피스가 안 깨지게(널이면 OnBoostGambleRequestedReceived 가 안전 정산).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UOfficeEventRailWidget> EventRail = nullptr;

	// ===== 상단 프로젝트 스트립 (신규 — 은퇴한 UI_OfficeProjectCardCurrent/UIE_ProjectNameBar 역할 흡수) =====
	// 개발 페이지: 이름 + 장르 배지(빈 장르면 Box Collapsed)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* DevProjectName;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* DevGenreBadge;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* DevGenreBadgeBox;
	// 운영 카드: 이름 + 등급 배지 + 수익 메트릭 + 운영종료
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OpProjectName;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OpGradeBadge;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* OpGradeBadgeBox;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* RevPerSecText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* TotalRevText;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* StripStopButton;

	// ===== 개발 배속(디펜스 게임식 ×2) =====
	// 월드 TimeDilation 하나로 개발 타이머·직원 이동·기여 오브가 함께 빨라진다.
	// 타이머와 기여 케이던스를 따로 곱하면 히트 수가 반토막 나 출시 게이트가 막힌다(케이던스 0.75 주석 참조).
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* StripSpeedButton;
	// 삼각형 1개(▶) ↔ 2개(▶▶) — 텍스처만 갈아끼운다
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* StripSpeedIcon = nullptr;

	// 아이콘 2종은 WBP Class Defaults 소유(하드 참조 → 자동 쿠킹). CasualPack Actions 의 Forward_Arrow / Fast_Forward.
	UPROPERTY(EditDefaultsOnly, Category = "Office|Speed")
	TObjectPtr<UTexture2D> SpeedIcon_x1 = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "Office|Speed")
	TObjectPtr<UTexture2D> SpeedIcon_x2 = nullptr;

	float DevSpeedMultiplier = 1.0f;

	// OnClicked 가 dynamic 델리게이트라 UFUNCTION 필수
	UFUNCTION()
	void OnSpeedToggleClickedHandler();
	// 배속 적용의 단일 경로 — 사무실을 벗어날 때 1.0 복원도 여기로 들어온다
	void ApplyDevSpeed(float NewSpeed);
	// 아이콘만 현재 배속에 맞춰 되그린다 (재활성화 시 배속을 건드리지 않고 표시만 동기화)
	void RefreshSpeedIcon();
	// 운영 썸네일 — OpThumbInner 배경에 프로젝트 아이콘 세팅, 아이콘 없으면 OpThumbQ("?") 표시
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* OpThumbInner;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OpThumbQ;
	// 개발 썸네일 — 커버를 DevThumbInner 배경으로 (Op와 동일 패턴, "?" 다크판 플레이스홀더 은퇴)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* DevThumbInner;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* DevThumbQ;
	// 개발 썸네일 키라인 — 장르색 주입 대상 (SDF Line MI)
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* DevThumbLine = nullptr;

	// 스트립 셸 SDF (Chip 대칭 마스터) — 가변 폭이라 Wpx/Hpx 지오메트리 주입 필요 (UResourceWidget 칩 패턴)
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* StripBG = nullptr;
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* StripLine = nullptr;
	FVector2D LastStripMatSize = FVector2D::ZeroVector;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> StageTray = nullptr;
	FVector2D LastStageTrayMatSize = FVector2D::ZeroVector;
	void UpdateStripShellMaterialSize();

	// 개발·운영 중 왼쪽 위 미션판이 표시줄과 겹치지 않게 표시줄 아래로 내린다(대기 중엔 제자리).
	UPROPERTY(Transient)
	TObjectPtr<UWidget> StripRootWidget = nullptr;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> MissionAnchor = nullptr;
	float MissionAnchorShiftY = 0.0f;
	static constexpr float MissionAnchorGap = 18.0f;
	void UpdateMissionAnchorBelowStrip(float DeltaTime);

	// 진행 줄 칸(StripRailColumn) 위 여백 ― WBP 값 = 회사 블록 아래. 개발 구간에는 회사 블록을 숨기고 그 자리로 올린다
	// (진행 줄 판은 칸보다 20 위로 붙어 있어 칸 위 70 = 판 위 50 = 회사 블록 위)
	void ApplyStripRailPlacement(bool bStripOnTop);
	float StripRailTopBelowBlock = -1.0f;
	static constexpr float StripRailTopFocus = 70.0f;

	// 운영 카드 = 운영 중(제조 제외), 배치 편집 중에는 숨김. 카드는 대기 전용 오른쪽 열(OfficeContentRail) 밖의 형제라 편집 숨김도 따로 건다
	void RefreshOpCardVisibility(EProjectLifecycle State);

	// 개발 페이지 스테이지 셀과 직능 아이콘/목표 테두리는 NativeConstruct에서 트리 이름으로 캐시한다.
	UPROPERTY(Transient)
	TArray<UWidget*> StripStageCells;
	UPROPERTY(Transient)
	TArray<UCommonTextBlock*> StripStageNames;
	UPROPERTY(Transient)
	TArray<UCommonTextBlock*> StripStageVals;
	UPROPERTY(Transient)
	TArray<UProgressBar*> StripStageBars;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> StripRequirementRow = nullptr;
	UPROPERTY(Transient)
	TArray<UImage*> StripStageDisciplineIcons;
	UPROPERTY(Transient)
	TArray<UImage*> StripStageGoalRings;
	FVector2D StripStageRingMatSize[6];
	// 표시 점수의 문턱을 통과할 때만 셀 팝과 금색 테두리를 재생한다.
	uint8 StripStageVisualState[6] = {};
	float StripStageGoalElapsed[6] = { 99.f, 99.f, 99.f, 99.f, 99.f, 99.f };
	static constexpr float StripGoalDuration = 0.72f;

	// 스테이지 바 부드러운 채움 — raw ProgressBar SetPercent은 즉시라 툭툭 점프. NativeTick에서 Shown→Target 을 0.3초 count-up으로 lerp (옛 UStatRowWidget 방식 이식).
	float StripStageShownPct[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	float StripStageTargetPct[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	// 스테이지별 목표 점수(수치) — val 을 "획득 / 목표" 원본 숫자로 표시(길면 축약). Pct×Target = 현재 획득. 0=미설정.
	float StripStageTargetScore[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };

	// 착지 정산 — 권위 점수는 pending에만 저장, orb 착지(OnScoreOrbArrivedReceived)가 TargetPct로 플러시.
	// 폴백: 착지가 PendingFlushTimeout 안에 안 오면 NativeTick이 연출 없이 플러시 (orb 유실 대비).
	float StripStagePendingPct[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	bool bStripStagePendingDirty[6] = { false, false, false, false, false, false };
	double StripStagePendingSince[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	static constexpr float PendingFlushTimeout = 1.5f;   // orb 비행 1.0s + 여유

	// 착지 연출 상태 — Val 펀치(RenderScale)와 바 fill 화이트 플래시. NativeTick 구동.
	float StripStagePunchElapsed[6] = { 99.f, 99.f, 99.f, 99.f, 99.f, 99.f };   // >= Duration = 유휴
	float StripStagePunchPeak[6] = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
	float StripStageFlashAlpha[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	FLinearColor StripStageFlashColor[6] = { FLinearColor::White, FLinearColor::White, FLinearColor::White, FLinearColor::White, FLinearColor::White, FLinearColor::White };
	double StripStageLastJuiceTime[6] = { -10.0, -10.0, -10.0, -10.0, -10.0, -10.0 };
	static constexpr float StripPunchDuration = 0.2f;
	static constexpr float StripPunchThrottle = 0.12f;   // ~1s 케이던스 × 직원수 스팸 가드
	static constexpr float StripFlashDecayTime = 0.15f;

private:
	void BuildPlacementEditControls();
	void BeginPendingPlacementEdit();
	bool bPendingPlacementEdit = false;
	TWeakObjectPtr<AActor> PendingPlacementEditTarget;
	void BindPlacementEditControls();
	void UnbindPlacementEditControls();
	UFUNCTION() void RefreshPlacementEditControls();
	UFUNCTION() void HandlePlacementEditToggle();
	void HandlePlacementEditFeedback(const FText& Message);
	TWeakObjectPtr<AOfficeCameraPawn> PlacementEditPawn;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> PlacementEditCanvas;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> PlacementEditDoneButton;
	UPROPERTY(Transient) TObjectPtr<UBuildPlacementPanelWidget> PlacementEditPanel;
	UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> PlacementEditHint;
	bool bPlacementEditSuppressedHud = false;
	ESlateVisibility PlacementEditPreviousDockVisibility = ESlateVisibility::SelfHitTestInvisible;
	bool bPlacementEditPreviousRightEnabled = true;
	bool bPlacementEditPreviousEquipmentEnabled = true;

	void UpdatePlacementWorkstationMarker(float DeltaTime);
	TWeakObjectPtr<AWorkstationActorBase> PlacementMarkerWorkstation;
	TWeakObjectPtr<UPlacementHandler> PlacementMarkerHandler;
	UPROPERTY(Transient) TObjectPtr<USelectionChevronWidget> PlacementChevron;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanelSlot> PlacementChevronSlot;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> PlacementMarkerCanvas;
	float PlacementMarkerElapsed = 0.f;

	void RefreshEquipmentManualButton();
	void ShowEquipmentManualEmployees();
	void HandleEquipmentManualEmployee(UObject* Item);
	void HandleEquipmentManualDiscipline(UObject* Item);
	void CloseEquipmentManualDialog();
	UPROPERTY() TObjectPtr<UEmployeeEquipmentDialogWidget> EquipmentManualDialog;
	UPROPERTY() TArray<TObjectPtr<UEmployeeEquipmentItemObject>> EquipmentManualEntries;
	int32 EquipmentManualEmployeeId = INDEX_NONE;

	UFUNCTION()
	void OnWorkstationButtonClicked();

	UFUNCTION()
	void OnTrainingCenterButtonClicked();
	void OnBusinessTripButtonClicked();
	void RefreshCompanyFeatureButtons();
	bool CanOpenCompanyFeature(FName FeatureKey);
	UFUNCTION()
	void OnCompanyTierUnlocked(int32 NewTier);

	// 업무공간 뱃지 갱신
	void UpdateWorkstationBadge();

	UFUNCTION()
	void OnDecorationButtonClicked();

	UFUNCTION()
	void OnEmployeeButtonClicked();

	UFUNCTION()
	void OnRecruitmentButtonClicked();
	void OnEquipmentGachaButtonClicked();

	UFUNCTION()
	void OnCollectionBtnClicked();

	// UI_OfficeProjectCardCurrent의 StartButton 클릭 핸들러
	UFUNCTION()
	void OnStartButtonClickedHandler(int32 ProjectIndex);

	// 프로젝트 선택 완료 핸들러 (OfficeStagePanelWidget에서 선택 시)
	UFUNCTION()
	void OnProjectSelectedReceived();

	// ========== Stage Progress UI (동시 진행) ==========

	void BindStageProgressDelegates();
	void UnbindStageProgressDelegates();

	// 타이머 카운트다운 콜백
	UFUNCTION()
	void OnTimerUpdated(float RemainingTime);

	// 직능 점수 업데이트 콜백 — 활성 직능 스텝별 배열 (스트립 N칸 라이브 채움)
	UFUNCTION()
	void OnDisciplineScoresUpdatedReceived(
		const TArray<float>& Scores, const TArray<float>& Targets);

	// 결과 이펙트 콜백 (타이머 종료 시 "완료!" 이펙트)
	UFUNCTION()
	void OnStageResultEffectReceived();

	// 이펙트+환호 후 자동 팝업 콜백
	UFUNCTION()
	void OnSimultaneousTimerEndReceived();

	// 진행 데이터 복원 콜백 (로드 시 이펙트 없이 UI 갱신)
	UFUNCTION()
	void OnProgressDataRestoredReceived();

	// 개발 시작 배너 콜백
	UFUNCTION()
	void OnStepStartEffectReceived();

	// Score Orb 비행 연출
	UFUNCTION()
	void OnScoreOrbRequestedReceived(FVector WorldPos, int32 StepNumber, float Score, bool bIsCritical, int32 OrbCount, bool bPlaySpectacle);

	// ===== 피버타임 연출 (dev-spectacle Phase 2) =====
	// 매니저 OnFeverChanged 수신 → 진입 시 레전드 번개 버스트 + 화염 틴트 페이드인 + 음, 종료 시 페이드아웃.
	UFUNCTION()
	void OnFeverChangedReceived(bool bActive);

	// 직원들에게 DevFever 레전드 VFX 버스트 (전원 폭주 한 방). MaxWorkers 로 동시 스폰 상한(모바일 헤드룸).
	void SpawnFeverBurst(int32 MaxWorkers = 5);

	// 화염 틴트 알파를 목표값으로 부드럽게 보간 (timer 구동)
	void TickFeverOverlay();
	FTimerHandle FeverOverlayTimerHandle;
	float FeverOverlayAlpha = 0.0f;
	float FeverOverlayTargetAlpha = 0.0f;

	// ===== 마일스톤 / escalate / 클라이맥스 (dev-spectacle Phase 3) =====
	// 직능 칸 목표 첫 달성 시 1회 팝. 스테이지 시작 시 리셋. 활성 직능이 최대 6개라 6칸 전부 대상.
	bool bCategoryMilestoneReached[6] = { false, false, false, false, false, false };
	void PlayCategoryMilestone(int32 CategoryIndex);

	// 후반(잔여<40%) 진입 시 1회 라스트 스퍼트 큐 (중복 방지)
	bool bLateStageEscalated = false;

	// 라스트 스퍼트 시각 펄스 — 타이머 숫자 골드 진동 (NativeTick). 종료/전환 시 Reset.
	bool bLastSpurtVisual = false;
	float LastSpurtElapsed = 0.0f;
	void ResetLastSpurtVisual();

	// 타이머 종료 "완료!" 시점의 출시 클라이맥스 한 방 (품질 등급 비례)
	void PlayLaunchClimax(bool bManufacturingSuccess = false);

	UPROPERTY()
	UScoreOrbContainerWidget* ScoreOrbContainer = nullptr;

	// 프로젝트 카드 스테이지 점수 업데이트
	void UpdateProjectCardProgress();


	// ========== Operation UI ==========

	UPROPERTY()
	bool bIsInOperationMode = false;

	// 하단 도크 슬라이드 상태 (C++ 렌더 트랜스폼 구동 — NativeTick)
	bool bDockHidden = false;       // true=개발중 아래로 슬라이드아웃
	float DockSlideAlpha = 0.f;     // 0=제자리 / 1=완전 슬라이드다운(투명)
	static constexpr float DockSlideDistance = 260.f;  // 바텀 도크 높이+여유
	static constexpr float DockSlideSpeed = 12.f;      // FInterpTo 속도(≈0.2s)

	// 도크 표시/숨김 요청 — 이름은 레거시(구 WBP 애니), 내부는 C++ 슬라이드 플래그 세팅.
	void PlayShowButtonsAnim();
	void PlayHideButtonsAnim();

	// 도크 트랜스폼/투명도를 알파 그대로 반영 — 보간이든 스냅이든 유일한 적용 경로
	void ApplyDockSlide(float Alpha);

	// ===== Lifecycle FSM 단일 진입점 =====
	// 모든 상태 변화는 OnLifecycleChanged 한 곳으로 수신 → RefreshOfficeUI가 모든 UI 갱신
	UFUNCTION()
	void OnLifecycleChangedHandler(EProjectLifecycle OldState, EProjectLifecycle NewState);

	// 라이프사이클 상태 기반으로 모든 UI 일괄 갱신 (Switcher / NameBar / 버튼 / 카드)
	void RefreshOfficeUI(EProjectLifecycle State);

	void BindOperationDelegates();
	void UnbindOperationDelegates();

	UFUNCTION()
	void OnOperationStartedReceived(int32 BuildingID);

	UFUNCTION()
	void OnOperationUpdatedReceived(int32 BuildingID, const FOperationData& Data);

	UFUNCTION()
	void OnOperationCompletedReceived(int32 BuildingID, const FOperationData& Data);

	// 운영종료 버튼 클릭 핸들러
	UFUNCTION()
	void OnStopOperationClickedHandler();

	void UpdateUIForOperationMode(bool bOperating);
	void UpdateOperationUI(const FOperationData& Data);
	FOperationData* GetCurrentBuildingOperation();
	int32 GetCurrentBuildingIDAsInt() const;

	// ========== Step 시작 이펙트 ==========

	void ShowStepStartEffect();
	void RemoveStepStartEffect();

	UPROPERTY()
	TObjectPtr<UDevelopmentBannerWidget> StepStartBannerWidget;

	FTimerHandle StepEffectTimerHandle;

	// ========== 출시 확인 모달 ==========

	void ShowLaunchConfirmModal();

	UFUNCTION()
	void OnLaunchConfirmedHandler();

	UFUNCTION()
	void OnLaunchCancelledHandler();

	// 실패 화면 [추가 개발] — 다이아 결제 후 개발 재개
	UFUNCTION()
	void OnLaunchRetryRequestedHandler();

	// ========== 프로젝트 결산서 모달 ==========

	void ShowProjectReportModal(const FOperationData& CompletedData);
	void ShowPendingReportModal();

	UFUNCTION()
	void OnReportClosedHandler();

	// 현재 화면에 떠 있는 프로젝트 결산서 (중복 푸시 방지)
	UPROPERTY()
	class UProjectReportWidget* ActiveReportWidget = nullptr;

	// PendingReport는 위젯 생애에 한 번만 체크 — 다른 패널 여닫을 때마다 재트리거 방지
	bool bHasCheckedPendingReport = false;

	// ========== 프로젝트 보드 이벤트 시스템 ==========

	void BindBoardEventDelegates();
	void UnbindBoardEventDelegates();

	// 중간 이벤트 팝업 표시
	UFUNCTION()
	void OnProjectEventTriggeredReceived(const FProjectEventData& EventData);

	// 부스트 도박 팝업 표시 (타임드 바이너리 모달, 자체 해결)
	UFUNCTION()
	void OnBoostGambleRequestedReceived();

	// 직원 상태 토스트 수신 (EmployeeManager::OnEmployeeStatusToast) → 이벤트 레일에 자동만료 토스트.
	// EmployeeID 를 키로 넘겨 같은 직원의 후속 상태가 쌓이지 않고 교체되게 한다.
	UFUNCTION()
	void OnEmployeeStatusToastReceived(int32 EmployeeID, const FText& Message, float Duration);

	// 업무 복귀 수신 → 그 직원의 알림 조기 해제 (잡았는데 안내가 남아 있는 어긋남 방지)
	UFUNCTION()
	void OnEmployeeStatusToastClearedReceived(int32 EmployeeID);

	// 우레일 대기존 묶음 (힌트박스 + 착수 CTA) — 대기에만 표시
	UPROPERTY(meta = (BindWidgetOptional))
	UVerticalBox* RightIdleZone;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> ManufacturingExperienceSummary;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> IdleHintText;
	void RefreshManufacturingPointsStatus();
	void OnManufacturingPointsChanged(int32 BuildingID);
	void TryShowManufacturingResearchResult();
	void TryOpenManufacturingQuantity();
	UFUNCTION() void OnManufacturingResearchResultClosed(bool bOpenQuantity);
	FTimerHandle ManufacturingResultTimer;
	FTimerHandle ManufacturingQuantityTimer;
	bool bManufacturingResearchObserved = false;
	bool bManufacturingResearchWasActive = false;
	bool bHandlingManufacturingResult = false;
	int32 ManufacturingResultBuildingID = INDEX_NONE;
	int32 ManufacturingResultProjectIndex = INDEX_NONE;
	TWeakObjectPtr<ULaunchConfirmWidget> ActiveManufacturingResult;
	int32 ManufacturingQuantityProjectIndex = INDEX_NONE;
	int32 ManufacturingQuantityBuildingID = INDEX_NONE;
	void HandleEmployeeExperienceChanged(int32 EmployeeID);

	// 상단 밴드(회사배너/트렌드칩/티어칩/포트폴리오)는 OfficeLayerWidget 로 이관 — 재화와 같은 상시 레이어.

	// 판매 감쇠 곡선 라이브 그래프 (우레일 운영 상태 — 미배치 시 무동작)
	UPROPERTY(meta = (BindWidgetOptional))
	class USalesCurveWidget* SalesCurve;

	// 이벤트 선택지 처리
	UFUNCTION()
	void OnEventChoiceMadeHandler(int32 ChoiceIndex);

	// 이벤트 효과 가시화 — 선택된 Choice의 효과(점수/시간/버프/정지/보상)를 카드와 동일 포맷으로 NiagaraTextEffect 표시
	void ShowEventEffectPopup(const FProjectEventChoice& Choice);

	// 이벤트 점수 보너스 → 화면 중앙에서 해당 Step ProgressBar로 Orb burst 비행
	void SpawnEventScoreOrbs(int32 StepNumber, int32 BonusAmount);

	// 현재 트리거된 이벤트 데이터 — 선택 결과 팝업이 선택된 Choice 효과를 카드와 동일하게 표시하려고 보관
	UPROPERTY()
	FProjectEventData PendingEventData;

	// 모드 변경 수신 → 좌측 슬롯 전환
	UFUNCTION()
	void OnProjectModeChangedReceived(EProjectMode NewMode);

	// 보드 열기 버튼 클릭
	UFUNCTION()
	void OnOpenBoardButtonClicked();

	// 현재 게임 상태에서 Switcher 인덱스 재동기화 (단일 진실 공급원)
	void RefreshLeftSlotState();

	// 경제 잠금/해제 — 해제 시 서버값으로 정정된 다이아 잔액으로 컨티뉴 슬롯 재계산
	void HandleEconomyReady(bool bReady);

	// 현재 게임 상태에 맞게 ProjectNameBar 텍스트 재동기화
	void RefreshProjectNameBar();

	// 대기 CTA 티어 텍스트 갱신
	void UpdateIdleTierText();

	// ===== 상단 프로젝트 스트립 populate (신규) =====
	// 스테이지 셀 4개를 이름으로 캐시 (NativeConstruct 1회)
	void CacheStripStageCells();
	// 이름 + 장르/등급 배지 (개발/운영 페이지 공통 — 안 보이는 페이지 것도 채워도 무해)
	void PopulateStripIdentity();
	// 스테이지 바/값/이름 갱신 (Progress.Steps 기반, 빈 스텝 셀 Collapsed, 100%면 초록)
	// — 라벨/가시성/커밋값 스냅용. RefreshOfficeUI 등 상태 전환 시에만 호출 (라운드 중 매틱 호출 금지: 커밋값=이미 목표치라 100으로 튐)
	void UpdateStripStageCells();
	// 라운드 진행 중 실시간 바 채움 (OnSimultaneousScoresUpdated 라이브 파라미터 기반, 옛 Plan/Dev/QA 바와 동일 소스). Index 0=기획,1=개발,2=QA
	// — 목표 퍼센트만 설정. 실제 바/값/색은 NativeTick 이 Shown 을 lerp 하며 ApplyStripStageVisual 로 그림.
	void UpdateStripStageLive(int32 Index, float Score, float Target);
	// orb 착지 수신 → 해당 스테이지 pending 플러시 (성공 시 착지 연출)
	void OnScoreOrbArrivedReceived(int32 StepNumber, bool bIsCritical);
	// pending을 시각 목표로 커밋 + 마일스톤 체크. dirty 였으면 true.
	bool FlushStripStagePending(int32 Index);
	// 착지 순간 펀치+플래시 시작 (스로틀 포함)
	void StartStripStageJuice(int32 Index, bool bIsCritical);
	// 특정 스테이지의 바 퍼센트/값 텍스트/색을 즉시 적용 (틱 lerp + 스냅 공용)
	void ApplyStripStageVisual(int32 Index, float Pct, bool bAnimateGoalTransition = false);
	// 운영 수익 메트릭 (수익/초, 누적)
	void UpdateStripOperationMetrics(const FOperationData& Data);
	// 운영 줄 등급 = 출시 결과와 같은 공용 엠블럼. Max 는 접는다.
	void ApplyOpGradeEmblem(EQualityGrade Grade);
	// 공용 Apply 는 재질을 비웠다가 다시 넣어 광택을 처음부터 재생하므로, 칠한 값이 같으면 건너뛴다.
	EQualityGrade AppliedOpGrade = EQualityGrade::Max;
	ECompanyType AppliedOpGradeCompany = ECompanyType::None;
	// 운영 카드 제목 크기 단계의 기준 = WBP 크기. 줄인 크기가 다음 이름의 기준이 되지 않게 처음 값을 기억한다
	float OpTitleBaseFontSize = 0.0f;
	// orb 목표 = 스텝 번호(1-based) → Stage{n}Bar 위젯
	UWidget* GetStripStepBar(int32 StepNumber) const;
	// 정적 저작 요소가 없는 빌드에서도 나머지 Office HUD는 유지한다. 누락은 바인딩 시 보고한다.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCanvasPanel> ExpansionLayer;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> ExpandAnchorX;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> ExpandAnchorY;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UIconWithButtonWidget> ExpandHotspotX;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UIconWithButtonWidget> ExpandHotspotY;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UResourceWidget> ExpandCostX;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UResourceWidget> ExpandCostY;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UOfficeExpansionContentWidget> ExpansionConfirmContent;

	void BindExpansionHotspots();
	void UnbindExpansionHotspots();
	void UpdateExpansionHotspots(bool bFromTick = false);
	void RefreshExpansionValues();
	void OnExpansionResourceChanged(EResourceType Type, int64 NewValue);
	void OnExpansionFootprintChanged(FIntPoint TileCount);
	void OpenExpansionConfirm(EOfficeExpandDirection Direction);
	void CloseExpansionConfirm();
	void ConfirmExpansion(EOfficeExpandDirection Direction, TWeakObjectPtr<AOfficeInterior> QuotedInterior,
		FIntPoint QuotedFootprint, int64 QuotedCost);
	TWeakObjectPtr<AOfficeInterior> ExpansionInterior;
	TWeakObjectPtr<AOfficeInterior> LastPreviewInterior;
	TWeakObjectPtr<UWidget> ExpansionChevrons[2];
	FDelegateHandle ExpansionFootprintHandle;
	TWeakObjectPtr<UConfirmCancelWidget> ExpansionDialog;
	FDelegateHandle ExpansionConfirmHandle;
	FDelegateHandle ExpansionCancelHandle;
	EOfficeExpandDirection ExpansionDirection = EOfficeExpandDirection::Left;
	bool bExpansionConfirmOpen = false;
	bool bPushingExpansionConfirm = false;
	bool bExpansionConfirmWasActivated = false;
	bool bExpansionRequestPending = false;
	bool bExpansionPreviewShown = false;
	bool bExpansionAffordInitialized = false;
	bool bExpansionCanAfford[2] = {false, false};
};
