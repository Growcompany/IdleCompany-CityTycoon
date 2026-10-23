#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/BuildingTraitSaveData.h"
#include "Data/BuildingSkinGachaSaveData.h"
#include "Enum/ItemType.h"
#include "Table/GachaBoxVisualTable.h"
#include "UI/Element/Gacha/GachaRevealCardWidget.h"
#include "Utils/GachaRevealBeatRunner.h"
#include "GachaRevealPresentationWidget.generated.h"

class UAudioComponent;
class UEmployeeEquipmentManagerSubsystem;
class UButtonWidget;
class UCanvasPanel;
class UGachaRevealBoxWidget;
class UGachaRevealFXLayer;
class UGestureHintWidget;
class UHorizontalBox;
class UImage;
class UItemTooltipWidget;
class UOverlay;
class USizeBox;
class UTableManagerSubsystem;
class UTextBlock;
struct FEmployeeEquipmentInstance;
struct FEmployeeEquipmentRequest;
struct FGameplayTag;
enum class EGachaSeam : uint8;

DECLARE_DELEGATE(FOnPresentationClosed);

// 거래가 확정될 때 호출자가 지급 전 상태와 실제 지급 델타를 함께 전달한다.
struct FGachaRevealAwardContext
{
	TArray<bool> bOwnedBefore;
	TArray<int32> GradeWeights;
	int32 MileageGained = INDEX_NONE;
	int32 MileageBalance = INDEX_NONE;
	int32 PityBefore = INDEX_NONE;
	int32 PityAfter = INDEX_NONE;
	int32 PityThreshold = INDEX_NONE;
};

// 시안 KINDS.*.grades[g] 한 칸(linear) ― Ui = 등급 글자·도장 색, Fx = 효과·빛 색, Ink = 도장 외곽선
USTRUCT()
struct FGachaRevealGradeColors
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Ui = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Fx = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Palette")
	FLinearColor Ink = FLinearColor::Black;
};

/**
 * 뽑기 상자 공개 화면 ― 장비·특성·스킨 공용(WBP UI_GachaPresentation).
 * 1~8단계(던지기·가르기·승급·뚜껑·뿌리기·뒤집기·도장·단독 장면)와 정리·결과 진입을 박자 실행기 하나로 몬다.
 * 정답 = docs/05_UI/mockups/GachaRevealJuice_2026-10-17/src.html startPull(:789~), 설계 = docs/superpowers/specs/2026-10-17-gacha-box-reveal-design.md §4
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UGachaRevealPresentationWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

	friend struct FGachaRevealWidgetTestAccessor;
	friend struct FEquipmentThirtyRevealValidation;
	friend struct FGachaRevealResultTestAccessor;

public:
	UGachaRevealPresentationWidget(const FObjectInitializer& ObjectInitializer);

	// 이미 지급된 결과만 넘길 것(서버가 일부만 받았으면 받은 것만). 열려 있는 동안(결과 화면 포함) 다시 부르면 무시하고 Warning
	void SetupPullTrait(const TArray<FBuildingTraitGachaResult>& Results, bool bInAdvanced, const FGachaRevealAwardContext& Award = {});
	void SetupPullSkin(const TArray<FBuildingSkinGachaResult>& Results, bool bInAdvanced, const FGachaRevealAwardContext& Award = {});
	void SetupPullEquipment(const TArray<FEmployeeEquipmentInstance>& Results,
		EItemType SourceTicket = EItemType::EmployeeEquipmentTicketNormal, int32 EmployeeId = INDEX_NONE,
		const FGachaRevealAwardContext& Award = {});
	FText BuildSummaryLine() const;
	int32 GetRecommendedCount() const { return Recommended.Num(); }
	void EquipRecommended();

	// 패널 주인공 상자의 무대 좌표·배율(1단계 출발점). Setup 전에 부를 것. 안 부르면 시안 값 (W/2, 600, 0.6)
	void SetLaunchFrom(FVector2D CanvasPos, float Scale);

	// 검증 하네스가 퇴장 페이드와 경주하지 않게 끈다
	void SetDisableExitFade(bool bDisable) { bDisableExitFade = bDisable; }

	// 떠 있거나 닫히는 중 ― 패널은 이 동안 뽑기 단추를 막아야 한다(이미 지급된 결과가 거절되면 화면에 안 나온다)
	bool IsPresentationOpen() const { return Runner.IsRunning() || bClosing; }

	// 튜토리얼 M11/M12 ― [확인] 하이라이트 대상
	UWidget* GetConfirmButtonWidget() const;

	FOnPresentationClosed OnPresentationClosed;

	// 시험용 ― 박자 열거형은 반사되지 않아 UFUNCTION 으로 내지 않는다
	bool IsOnBeat(EGachaRevealBeat Beat) const { return Runner.IsOnBeat(Beat); }
	void DebugAdvanceTime(float Seconds);
	void DebugTap() { HandleScreenTap(); }
	void DebugSkipToResult();
	const FGachaRevealBeatRunner& GetRunner() const { return Runner; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// ===== WBP 계약(Task 10c §3.1) =====
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButtonWidget> ConfirmButton;
	// 화면 누르기를 받는 판 ― Image 라 누르기가 위젯 NativeOnMouseButtonDown 으로 올라온다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> DimBG;
	// 흔들 대상 1 ― RenderOpacity 1 고정(효과 캔버스 조상)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> ContentRoot;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UGachaRevealBoxWidget> BoxWidget;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> UnderCanvas;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CardLayer;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> FXLayer;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> PillarCanvas;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> HeaderTitle;
	// 트레이 클래스(Border/CommonBorder)는 저작 때 원본을 따라 정해진다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> SortToggle;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> TallyRow;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> BestPanel;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> BottomBar;
	// 흔들 대상 2 ― RenderOpacity 1 고정, 흐리기는 SpotlightShade·SpotlightContent 에
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> SpotlightRoot;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UGachaRevealCardWidget> SpotlightCard;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> SpotlightInfo;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> StampLayer;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButtonWidget> SkipButton;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HintText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UGestureHintWidget> HandGuide;

	// 설계서 §4-3 승급 노브 ― 실행기 Setup 넷째 인자
	UPROPERTY(EditDefaultsOnly, Category = "Reveal")
	bool bUpgrade = true;

	// 시안 S.shakeMul·S.sparkMul (연출 감소면 흔들림 0)
	UPROPERTY(EditDefaultsOnly, Category = "Reveal", meta = (ClampMin = "0.0"))
	float ShakeScale = 1.f;
	UPROPERTY(EditDefaultsOnly, Category = "Reveal", meta = (ClampMin = "0.0"))
	float SparkScale = 1.f;

	// 시안 KINDS 팔레트(등급 0..5). 생성자 값 = 시안, WBP Class Defaults 가 덮을 수 있다. 스킨은 특성 색(시안 KINDS.skin.grades = trait)
	UPROPERTY(EditDefaultsOnly, Category = "Reveal|Palette")
	TArray<FGachaRevealGradeColors> EquipmentPalette;
	UPROPERTY(EditDefaultsOnly, Category = "Reveal|Palette")
	TArray<FGachaRevealGradeColors> TraitPalette;

	UFUNCTION()
	void OnConfirmClicked();

private:
	// 카드 한 장의 연출 상태(박자 시계 ms 기준점)
	struct FCardMotion
	{
		FGachaRevealRect Cell;
		float DealRotation = 0.f;
		float FlipStartMs = -1.f;
		float FlipMs = 0.f;
		bool bFaceUp = false;
		bool bHidden = false;
		FGachaRevealRect SettleFrom;
	};

	struct FBoxPose
	{
		FVector2D Pos = FVector2D::ZeroVector;
		float Scale = 1.f;
		float Angle = 0.f;
	};

	struct FStampMotion
	{
		TWeakObjectPtr<UTextBlock> Text;
		float StartMs = 0.f;
		bool bKeep = false;
		bool bLeftAnchor = false;
		float FadeStartMs = -1.f;
	};

	// ===== 진입 =====
	bool RejectIfOpen(const TCHAR* Source) const;
	void BeginReveal(EGachaRevealKind InKind, EGachaBoxType InBoxType, const FText& InTitle,
		const TArray<FGachaRevealEntry>& InEntries, const TArray<FGachaRevealCardData>& InCards, const TArray<FText>& InMeta);
	void ResolveExtraWidgets();
	void ResetStage();
	void CreateDealtCards();
	void ClearDealtCards();
	bool ResolveReduceMotion() const;
	const UTableManagerSubsystem* ResolveTables() const;
	static bool IsHandOpenBoxSetting(const UObject* WorldContext);

	// ===== 박자 =====
	void HandleBeatStarted(int32 Index, const FGachaRevealBeatSpec& Beat);
	void FinishBeat(int32 Index);
	void FireBeatEvents(int32 Index, float FromMs, float ToMs);
	void UpdateStage();
	void StepTick(float DeltaSeconds);

	void UpdateThrow(float T);
	void UpdateCut(float T);
	void UpdateUpgrade(float T);
	void UpdateLid(float T);
	void UpdateDeal(float T);
	void UpdateCascade(float T);
	void UpdateStamp(float T);
	void UpdateSpotlight(float T);
	void UpdateSettle(float T);
	void UpdateResult(float T);
	void UpdateCards();
	void UpdateStamps();
	void UpdateFades();
	void UpdateTallyBumps();
	void UpdateHandGuide();
	void UpdateBoxLid();
	void FillSpotlightInfo(int32 Index);
	void TickClose(float DeltaSeconds);

	void EnterResultState();
	void ApplySettledLayout(bool bAnimate);
	void ShowResultChrome();
	void ConfigureResultDetails();
	void UpdateResultGauge();
	int32 PityHitCount() const;
	float PityFillValueAt(float Progress) const;
	int32 BestResultIndex() const;
	void ShowCardTooltip(int32 Index, FVector2D AnchorAbs);
	void OnSortGradeClicked();
	void OnSortOrderClicked();
	void OnEquipClicked();
	FEmployeeEquipmentRequest MakeRecommendedRequest(int32 Index, UEmployeeEquipmentManagerSubsystem& Equipment) const;
	TArray<int32> PendingRecommendedIndices(const UEmployeeEquipmentManagerSubsystem& Equipment) const;

	// ===== 입력·닫기 =====
	void HandleScreenTap();
	void HandleSkipClicked();
	bool HandleBackRequest();
	void CloseSelf();
	void FinishClose();
	void ResetPresentation();

	// ===== 그리기 도우미 =====
	float StageWidth() const;
	float ClockMs() const { return Runner.GetClockMs(); }
	float AnchorAt(float BeatEventMs) const;
	float ScaledSec(float Ms) const;
	const FGachaRevealGradeColors& GradeColors(int32 Grade) const;
	FLinearColor FxColor(int32 Grade) const { return GradeColors(Grade).Fx; }
	FText GradeLabel(int32 Grade) const;
	bool IsKoreanGrades() const { return Kind != EGachaRevealKind::Equipment; }
	EGachaCardSize DealtCardSize() const;
	FVector2D CardCenter(int32 Index) const;
	FVector2D BoxPoint(EGachaSeam Which) const;
	void ApplyBoxPose(const FBoxPose& Pose, float Opacity);
	FBoxPose BoxPress(const FBoxPose& Base, float Depth, float Push, float Alpha) const;
	FLinearColor LeakColor(int32 Grade) const;
	void Sparks(FVector2D Pos, int32 Count, FLinearColor Color, float A0, float A1, float S0, float S1,
		float L0 = .18f, float L1 = .42f, float W0 = 3.f, float W1 = 7.f, float Gravity = 1400.f, float Jitter = 0.f);
	void Shake(float Px);
	void FlipCard(int32 Index, float DurationMs, float StartClockMs);
	// 시작 시계 = 사건 시각(AnchorAt) ― 한 틱에 여러 사건을 따라잡아도 늦게 시작하지 않게
	void BumpTally(int32 Grade, float StartClockMs);
	void SpawnStamp(FVector2D Pos, int32 Grade, float SizePx, float StartClockMs, bool bKeep, bool bLeftAnchor);
	void ClearStamps();
	void ShowFade(UWidget* Widget, bool bShow, float DurationMs, ESlateVisibility ShownVisibility, float StartClockMs);
	void ShowThrowDim(float StartClockMs);
	void JumpToResult();
	void PlayTag(const FGameplayTag& Tag, float VolumeScale = 1.f);
	void PlayTok(float VolumeScale);
	void StartZipLoop();
	void SetZipLoop(float Progress, float DragSpeed);
	void StopZipLoop();
	void UpdateHandCut(const FVector2D& StagePos, float DeltaRealMs);
	void CompleteHandCutEvent();

	// ===== 상태 =====
	FGachaRevealBeatRunner Runner;
	EGachaRevealKind Kind = EGachaRevealKind::Equipment;
	EGachaBoxType BoxType = EGachaBoxType::EquipmentNormal;
	TArray<FGachaRevealEntry> Entries;
	TArray<FGachaRevealCardData> CardData;
	TArray<FText> EntryMeta;
	TArray<FCardMotion> CardMotion;
	TArray<int32> TallyCounts;
	TArray<FStampMotion> StampMotions;

	bool bReduceMotionActive = false;
	bool bEquipmentReveal = false;
	bool bDisableExitFade = false;
	bool bClosing = false;
	bool bCloseNotified = false;
	float CloseElapsedMs = 0.f;
	float LastGeometryWidth = 0.f;

	FVector2D LaunchPos = FVector2D(-1.f, 600.f);
	float LaunchScale = .6f;
	FBoxPose CurrentBox;
	FVector2D DealOrigin = FVector2D::ZeroVector;
	FLinearColor LidGlowColor = FLinearColor::White;
	TSet<int32> Recommended;
	FGachaRevealAwardContext AwardContext;
	TArray<FGuid> EquipmentInstanceIds;
	TArray<bool> bSkinDuplicates;
	int32 RevealEmployeeId = INDEX_NONE;
	bool bRecommendedEquipped = false;
	EGachaRevealSort SortMode = EGachaRevealSort::Grade;
	TArray<TWeakObjectPtr<UWidget>> CloseFadeTargets;
	TArray<float> CloseStartOpacity;
	float EventNowMs = 0.f;
	float InfoClockMs = -1.f;

	int32 ActiveBeat = INDEX_NONE;
	// 뒤로가기·건너뛰기로 Result 에 뛰는 중 ― 앞 박자 남은 사건(정리 착지음·흔들림)을 쏘지 않는다
	bool bJumpingToResult = false;
	float LastEventMs = -1.f;
	float DealClockStartMs = -1.f;
	float LidClockStartMs = -1.f;
	float SpotlightReleaseClockMs = -1.f;
	float SettleClockStartMs = -1.f;
	float ResultClockStartMs = -1.f;
	float SortMoveStartMs = -1.f;

	// 틈 빛(시안 slit)
	bool bSlitOn = false;
	bool bBlade = false;
	float SlitProgress = 0.f;
	float SlitIntensity = .55f;
	FLinearColor SlitColor = FLinearColor::White;

	// 손 긋기
	bool bHandDragging = false;
	bool bHandCutDone = false;
	float HandRealMs = 0.f;
	float HandLastX = 0.f;

	// 페이드 중인 위젯(대상 → 시작 시계·길이·켜짐)
	struct FFade
	{
		TWeakObjectPtr<UWidget> Widget;
		float From = 0.f;
		float To = 1.f;
		float StartMs = 0.f;
		float DurationMs = 0.f;
		ESlateVisibility EndVisibility = ESlateVisibility::SelfHitTestInvisible;
		bool bThrowDim = false;
	};
	TArray<FFade> Fades;

	UPROPERTY(Transient)
	TObjectPtr<UGachaRevealFXLayer> FX;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGachaRevealCardWidget>> DealtCards;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> StampTexts;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ZipLoopAudio;

	UPROPERTY(Transient)
	TObjectPtr<UItemTooltipWidget> ActiveTooltip;

	// 계약 밖 이름 붙은 자식(10c §3.2) ― 이름은 트리와 다르게(트리 이름은 전부 블루프린트 변수가 된다)
	UPROPERTY(Transient)
	TObjectPtr<UImage> ShadeImage;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> SpotContentPanel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> SinglePanel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StampLatinTemplate;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StampKoreanTemplate;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> HandTextBlock;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> TallyCountTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> TallyLatinTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> TallyKoreanTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> TallyCellWidgets;
	TArray<float> TallyBumpClockMs;
	TWeakObjectPtr<UButtonWidget> SortGradeControl;
	TWeakObjectPtr<UButtonWidget> SortOrderControl;
	TWeakObjectPtr<UButtonWidget> EquipControl;
	TWeakObjectPtr<UButtonWidget> SingleEquipControl;

	// 시험만 넣는다 ― 게임에서는 늘 GetGameInstance 의 하위 시스템
	TWeakObjectPtr<const UTableManagerSubsystem> TablesOverride;
	TOptional<bool> ReduceMotionOverride;

#if WITH_DEV_AUTOMATION_TESTS
	// 시험만 읽는다 ― 1회성 사건이 몇 번 났는지(소리 = 태그 이름, 그 밖 = Tok·Shake·Stamp·Flip). 시험 월드엔 소리 관리자가 없어 소리 대신 센다
	TMap<FName, int32> TestEventCounts;
	void CountTestEvent(FName Event) { ++TestEventCounts.FindOrAdd(Event); }
#endif
};
