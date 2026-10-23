// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "CommonActivatableWidget.h"
#include "Input/UIActionBindingHandle.h"
#include "Enum/ResourceType.h"
#include "Enum/WidgetType.h"
#include "Enum/CountryType.h"
#include "Enum/CountryMapState.h"
#include "Enum/CountryOwnershipRule.h"
#include "Table/CountryInfoTable.h"
#include "Data/WorldMapTypes.h"
#include "Manager/MineManager.h"
#include "Manager/WorldFactoryManager.h"
#include "Entity/OverseasDiorama/OverseasDioramaTypes.h"
#include "UI/Element/WorldMap/MapActionBubbleWidget.h"
#include "WorldMapPanelWidget.generated.h"

class UResourceWidget;
class UCanvasPanel;
class UImage;
class UCountryNameWidget;
class UCountryDetailWidget;
class UTableManagerSubsystem;
class UTradeRouteWidget;
class UCommonActivatableWidgetStack;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UOverseasDioramaSubsystem;
class UButtonWidget;
class UCloseButtonWidget;
class UHorizontalBox;

/**
 * 국가 진입 디오라마 화면의 들어가기·나가기 진행 상태. 패널 없이 시험할 수 있게 위젯 밖에 둔다.
 * K 는 0(지도) → 1(디오라마) 진행도이고, 표시값 셋이 전부 K 하나에서 나온다.
 */
struct FDioramaViewTransition
{
	static constexpr float EnterDelay = 0.1f;
	static constexpr float EnterDuration = 0.45f;
	static constexpr float ExitSpeedMul = 1.5f;
	static constexpr float MapDimOpacity = 0.15f;
	static constexpr float RiseOffsetPx = 40.0f;

	float K = 0.0f;
	bool bEntering = false;
	// 라이브 촬영이 켜져 있다고 보는 구간. 나가기가 0 에 닿는 틱에 한 번만 true 를 돌려 촬영을 끄게 한다.
	bool bLive = false;
	float DelayLeft = 0.0f;

	// 진입 중이거나 나가는 중에 다시 불리면 K 를 이어 간다 ― 나라를 바꿔도 화면이 처음부터 다시 뜨지 않는다.
	void Enter();
	void Exit(bool bImmediate);
	bool Tick(float Dt);
	float ViewOpacity() const;
	float MapOpacity() const;
	float OffsetY() const;
	bool IsIdle() const { return !bLive && K <= 0.0f; }
};

namespace WorldMapDiorama
{
	// 화면 크기(물리 px) → 라이브 RT 크기. 한 축이라도 1px 미만이면 (0,0) = 레이아웃 전이라 촬영 시작을 미룬다.
	COMPANYGROWTHRENEWAL_API FIntPoint ClampLiveSize(const FVector2D& AbsSize);

	// 국가 서랍 주 탭 → 카메라가 볼 시설. 정보 탭과 모르는 탭은 주 시설.
	COMPANYGROWTHRENEWAL_API EDioramaModule ModuleForTab(int32 Tab, EDioramaModule Primary);
}

/**
 * 월드맵 = 전략 지도 테이블 (풀스크린 패널, PromptStack)
 *
 * 구 ULegacyWorldMapLayerWidget 을 대체한다. 레벨이 없어졌으므로 매 프레임 3D→스크린
 * 투영이 사라지고, 메달리온은 DT_CountryInfo.MapAnchor 로 Canvas 에 한 번만 배치된다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UWorldMapPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UWorldMapPanelWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * 백 액션(안드로이드 뒤로가기 · ESC)으로 이 패널을 닫는다.
	 * 닫기 경로는 이것과 도크 홈 버튼 둘뿐이다(헤더 X 는 홈과 역할이 겹쳐 2026-09-26 제거) ―
	 * 둘 다 죽으면 열고 못 닫는다. 검증이 직접 부를 수 있게 public 으로 올린다(베이스는 protected).
	 */
	virtual bool NativeOnHandleBackAction() override;

	/**
	 * 이 패널이 활성인 동안 액션 라우터에 요구하는 입력 설정.
	 * ⚠ 이걸 선언하지 않으면 라우터가 게임 입력 모드에 머물러, ESC 바인딩이 **만들어져 있어도**
	 *   `ProcessInput` 이 Unhandled 로 떨어진다(2026-09-26 실측). 풀스크린 UI 패널이므로 Menu 다.
	 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	/** 닫기 경로 검증 입구 ― 이게 false 면 뒤로가기 키가 이 패널에 도달하지 않는다. */
	bool IsBackActionHandler() const { return bIsBackHandler; }

	void FocusOnCountry(ECountryType Country, bool bAnimate = true);

	/**
	 * 정규화 앵커로 애니메이션 없이 포커스한다. 레이아웃이 있으면 반환 직후 목표 변형이다.
	 * 실제 지도/드로어의 첫 레이아웃 전이면 유효한 Geometry가 생기는 다음 틱까지 적용을 미룬다.
	 */
	void FocusOnAnchorImmediate(const FVector2D& Anchor);

	/** 판을 원위치(1배·중앙)로. 포커스를 걸어 둔 채 돌아오면 줌이 영구 고정되므로 되돌릴 입구가 필요하다. */
	void ResetFocus(bool bAnimate = true);

	/** 트윈이 진행 중인가. 즉시 경로는 반환 직후 false 여야 한다. */
	bool IsFocusAnimating() const { return FocusDuration > 0.0f; }

	/** 하위 패널을 드로어로 올린다. 드로어가 없는 트리면 nullptr ― 호출부가 PromptStack 으로 폴백한다. */
	UCommonActivatableWidget* PushDrawer(TSubclassOf<UCommonActivatableWidget> WidgetClass);

	bool IsDrawerOpen() const;
	UCommonActivatableWidget* GetActiveDrawerWidget() const;

	ECountryType GetSelectedCountry() const { return SelectedCountry; }

	// 판에 실제로 적용된 변형. 값 단언이 "그려졌는가" 를 대신할 수 없으므로 렌더 검증도 이 둘을 읽는다.
	FVector2D GetAppliedFocusTranslation() const { return AppliedFocusTranslation; }
	float GetAppliedFocusScale() const { return AppliedFocusScale; }

	/** 지오메트리가 아직 0 인 프레임에 쓰는 판 실측 폴백(2560 캔버스 기준). */
	static FVector2D DesignBoardSize();

	/**
	 * 판 로컬 좌표에서 선택 원반/라벨을 가시 영역 안에 두고 지도 가장자리도 덮는 변형.
	 * BoardOrigin = 변형 전 판 왼쪽 위의 가시 영역 좌표, CoverScale = 판 중심 기준으로 화면을 덮는 그림(바다)의 배율.
	 */
	static void ComputeFocusTransform(const FVector2D& Anchor, const FVector2D& BoardSize,
		const FSlateRect& VisibleRect, const FSlateRect& SelectionBounds,
		FVector2D& OutTranslation, float& OutScale,
		const FVector2D& BoardOrigin = FVector2D::ZeroVector, const FVector2D& CoverScale = FVector2D(1.0, 1.0));

	/** 화면 가득 지도의 상단 바 높이. 대륙 사각형은 이 절반만큼 아래로 내려 앉고, 포커스 가시 영역은 이 아래부터다. */
	static constexpr float MapTopBarHeight = 150.0f;

	/** 정규화 앵커 → 판 로컬 픽셀. 판 크기는 비율 고정이라 호출부가 실측해 넘긴다. */
	static FVector2D AnchorToCanvasPosition(const FVector2D& Anchor, const FVector2D& BoardSize);

	static bool IsCountryUnlocked(int64 CurrentMarketCap, int64 RequiredMarketCap);

	/**
	 * 진출 판정 규칙의 유일한 출처. 공장·채광을 하나도 못 짓는 국가는 시설 수로 판정하면
	 * 영원히 0 이라 진출에 도달하지 못한다(싱가포르). 그 국가만 해금 규칙으로 간다.
	 */
	static ECountryOwnershipRule ResolveOwnershipRule(const FCountryInfoTable& Row);

	/**
	 * 국가 상태의 유일한 출처. 링 색·영역 덮개 알파·자물쇠가 전부 이 한 값에서 파생돼야 한다 ―
	 * 두 군데서 따로 계산하면 타입만 같고 결과가 갈리는데 각 함수의 단위 테스트는 전부 초록이다.
	 */
	static ECountryMapState ResolveCountryState(ECountryOwnershipRule Rule, int32 FacilityCount,
		int64 CurrentMarketCap, int64 RequiredMarketCap);

	/**
	 * 링 색에 쓸 산업. DT 의 RepresentativeIndustry 가 우선이고, 비어 있으면 SupportedIndustries 의
	 * 첫 유효 원소로 폴백한다(그 경우 호출부가 경고를 낸다). 둘 다 없으면 None.
	 */
	static ECompanyType ResolveSpecialtyIndustry(const FCountryInfoTable& Row);

	/** 읽어올 산업이 없는 국가의 링 색. 시설을 못 짓는 허브는 청록, 채광 전담국은 브라스. */
	static FLinearColor ResolveNoIndustryRingColor(const FCountryInfoTable& Row);

	/**
	 * DT 에서 읽은 산업 강조색을 링에 쓸 수 있는지. 흰색은 FCompanyInfoTable::AccentColor 의
	 * "미지정" 기본값이고 투명은 링이 사라지는 값이라 둘 다 조달 실패로 본다.
	 */
	static bool IsUsableAccent(const FLinearColor& Accent);

	/**
	 * 배치 대상 행만 추린다. 배치 루프가 이 함수의 결과를 그대로 순회하므로
	 * "거르는 규칙은 맞는데 루프가 안 부른다" 가 성립하지 않는다.
	 */
	static TArray<FCountryInfoTable> FilterPlaceableRows(const TArray<FCountryInfoTable>& Rows);

	/** 렌더 검증(Task 9)이 배치 결과를 읽는 입구. */
	const TMap<ECountryType, TObjectPtr<UCountryNameWidget>>& GetMedallions() const { return Medallions; }

	/** 서랍 왼쪽 가시 영역에 그 나라 라이브 디오라마를 띄운다. 레이아웃 전이면 다음 틱으로 미룬다. */
	void EnterDiorama(ECountryType Country);

	/** 즉시가 아니면 역재생 후 라이브 촬영을 끈다. 즉시면 그 자리에서 끄고 지도 불투명도를 1 로 되돌린다. */
	void ExitDiorama(bool bImmediate);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;


	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> MedallionCanvas;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> MapBaseImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> RegionShadeCanvas;

	// 포커스 애니의 변형 대상. 판·덮개·메달리온이 한 덩어리로 움직여야 해서 그 공통 부모를 받는다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> MapTable;

	// ⚠ MapBaseImage·MedallionCanvas 와 **같은 사각형**에 풀스트레치로 놓을 것 ― 이 위젯의 로컬 크기가
	// 곧 항로 앵커 공간이라, 크기가 다르면 선이 메달리온에서 비례해 어긋나되 에러도 로그도 남지 않는다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTradeRouteWidget> RouteLayer;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UResourceWidget> UIE_Resource_Money;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UResourceWidget> UIE_Resource_Diamond;

	// 시총은 메달리온 해금의 유일한 통화다 ― 판 위에서 잠긴 이유를 읽으려면 상단에 있어야 한다. 그래서 required.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UResourceWidget> UIE_Resource_MarketCap;

	// 하위 패널이 들어가는 자리. UIBase.PromptStack 에 올리면 그 스택이 애니메이티드 스위처라
	// 월드맵이 통째로 사라진다 ― 그래서 패널이 자기 스택을 갖는다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonActivatableWidgetStack> DrawerStack;

	// 비었을 때 접어 둘 껍데기. 스택만 접으면 드로어 배경 판이 클릭을 계속 먹는다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> DrawerHost;

	// 변형되지 않는 지도 창. 드로어에 가리지 않는 실제 가시 영역을 이 좌표계로 측정한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> MapViewport;

	// 국가 진입 디오라마 층(MapViewport 위·DrawerHost 아래, 화면 전체). DioramaView 를 이 층 로컬 좌표로 놓는다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> DioramaLayer;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> DioramaView;

	// 화면 가득 바다(대륙 사각형 중심 기준 렌더 스케일로 화면 끝까지 덮는다). 있으면 포커스 덮개 판정도 이 배율을 쓴다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> SeaImage;

	// MapTable 안 MedallionCanvas 위 ― 말풍선이 판과 같이 확대·이동한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> BubbleCanvas;

	// 받을 것이 있을 때만 보이는 「모두 받기 N」(왼쪽 아래 바다).
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> ClaimAllButton;

	// 도크 홈이 빠진 뒤 화면에 남는 유일한 닫기 수단(오른쪽 위).
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCloseButtonWidget> CloseButton;

	// 미진출·잠김 영역을 덮는 색. 알파만 상태에서 나오고 색은 WBP(Class Defaults)가 소유한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldMap|Map")
	FLinearColor RegionShadeTint = FLinearColor(FColor(0x2B, 0x34, 0x44, 0xFF));

private:
	void BuildMedallions();
	void RefreshCountry(ECountryType Country);
	void RefreshAll();

	// 항로는 국가별이 아니라 주문 단위라 RefreshCountry 와 따로 돈다.
	void RefreshRoutes();

	// 국가 하나의 상태를 한 번 계산해 메달리온과 덮개에 한 번에 주입한다.
	void ApplyCountryState(const FCountryInfoTable& Row);

	// 링에 넘길 산업 강조색. 못 구하면 채광 브라스로 떨어지면서 경고를 남긴다(국가당 1회).
	FLinearColor ResolveSpecialtyAccent(const FCountryInfoTable& Row);

	FText BuildStatusLine(const FCountryInfoTable& Row, ECountryMapState MapState) const;

	// 입력 모드 쌍(GoToUIMode ↔ GoToNormalMode)이 같은 컨트롤러를 보게 한 곳에서만 찾는다.
	class AMainMapPlayerController* GetMainMapPlayerController() const;

	int32 CountFacilities(ECountryType Country) const;
	bool HasPendingAttention(ECountryType Country) const;
	int64 GetCurrentMarketCap() const;
	FVector2D GetBoardSize() const;
	void ApplyFocusTransform(const FVector2D& Translation, float Scale);
	void StartFocusTween(const FVector2D& InTargetTranslation, float InTargetScale, bool bAnimate);

	void HandleCountryFlagClicked(ECountryType CountryType);
	void HandleCountryFlagRejected(ECountryType CountryType);
	void HandleResourceChanged(EResourceType Type, int64 NewValue);

	// 드로어가 비면 여기서 판을 되돌린다 ― 패널이 더는 deactivate 되지 않아 NativeOnActivated 가 안 돈다.
	void HandleDrawerChanged(UCommonActivatableWidget* NewWidget);

	UFUNCTION()
	void HandleCloseClicked();

	void SetSelectedCountry(ECountryType Country);
	bool MeasureFocusLayout(FSlateRect& OutVisibleRect, FSlateRect& OutSelectionBounds) const;
	void RefreshFocusLayout(bool bAnimate);

	UFUNCTION()
	void HandleOrderGenerated(FTradeOrder Order);

	UFUNCTION()
	void HandleOrderAccepted(int32 OrderId);

	UFUNCTION()
	void HandleOrderDismissed(int32 OrderId, bool bWasAccepted);

	UFUNCTION()
	void HandleOrderCompleted(int32 OrderId);

	UFUNCTION()
	void HandleOrderExpired(int32 OrderId);

	UFUNCTION()
	void HandleMineLineCreated(ECountryType Country, FMineLineState LineState);

	UFUNCTION()
	void HandleMineLineRemoved(ECountryType Country, EResourceType Resource);

	UFUNCTION()
	void HandleMineLineSaturated(ECountryType Country, EResourceType Resource, bool bIsSaturated);

	UFUNCTION()
	void HandleFactoryLineStarted(ECountryType Country, FWorldFactoryLineState LineState);

	UFUNCTION()
	void HandleFactoryLineCompleted(ECountryType Country, int32 LineId);

	UFUNCTION()
	void HandleFactoryLineClaimed(ECountryType Country, int32 LineId, int64 FinalQty);

	UFUNCTION()
	void HandleFactoryLineRemoved(ECountryType Country, int32 LineId);

	UFUNCTION()
	void HandleFactoryUpgradeChanged(ECountryType Country, EWorldFactoryUpgradeType UpgradeType, int32 NewLevel);

	UFUNCTION()
	void HandleMineResourceClaimed(ECountryType Country, EResourceType Resource, int64 ClaimedQty);

	UFUNCTION()
	void HandleMineUpgradeChanged(ECountryType Country, EMineUpgradeType UpgradeType, int32 NewLevel);

	UFUNCTION()
	void HandleProductAdded(FIntPoint ProductKey, int64 NewAmount);

	UFUNCTION()
	void HandleProductConsumed(FIntPoint ProductKey, int64 NewAmount);

	// ── 지도 말풍선 · 모두 받기 ──
	// 받기·팔기 판정은 전부 OverseasActions 가 한다. 여기서는 보이기·자리·누름만 맡는다.
	void RefreshActionBubbles();
	void PlaceBubbleRow(ECountryType Country);
	UMapActionBubbleWidget* EnsureBubble(ECountryType Country, EMapActionBubbleKind Kind);
	void HandleBubbleClicked(EMapActionBubbleKind Kind, ECountryType Country);
	void HandleClaimAllClicked();
	// 수령 중 매니저 방송을 받아 도크와 같은 「[나라] 이름 N개 수집완료」 요약을 모은다.
	void RunClaimWithSummary(TFunctionRef<int64()> Claim);
	// 다음 라인 성숙까지 깨우는 타이머. 성숙 자체를 알리는 방송이 없어 도크와 같은 ETA 방식을 쓴다.
	void ScheduleBubbleEtaRefresh();
	// 여러 방송을 한 번에 일으키는 행동(모두 받기·판매) 동안은 갱신을 미뤘다가 끝에 한 번만 한다.
	void RequestBubbleRefresh();
	FVector2D MeasureBoardOrigin() const;
	FVector2D GetSeaCoverScale() const;

	UPROPERTY()
	TMap<ECountryType, TObjectPtr<UCountryNameWidget>> Medallions;

	UPROPERTY()
	TMap<ECountryType, TObjectPtr<UImage>> RegionShades;

	UPROPERTY()
	TMap<ECountryType, TObjectPtr<UHorizontalBox>> BubbleRows;

	UPROPERTY()
	TMap<ECountryType, TObjectPtr<UMapActionBubbleWidget>> ClaimBubbles;

	UPROPERTY()
	TMap<ECountryType, TObjectPtr<UMapActionBubbleWidget>> SellBubbles;

	FTimerHandle BubbleEtaHandle;
	bool bBatchAction = false;
	bool bBubbleRefreshPending = false;
	TArray<FString> ClaimSummaryLines;
	bool bBubbleWidgetsWarned = false;

	UPROPERTY()
	TMap<EResourceType, TObjectPtr<UResourceWidget>> ResourceWidgets;

	UPROPERTY()
	TObjectPtr<UTableManagerSubsystem> TableMgr;

	FDelegateHandle UIResourceChangedHandle;

	// 경고를 국가당 1회로 묶는다 ― 시총이 오를 때마다 갱신되므로 그냥 두면 로그가 못 쓰게 된다.
	// 두 실패는 원인이 달라 따로 센다(하나로 묶으면 먼저 뜬 쪽이 나머지를 삼킨다).
	TSet<ECountryType> AccentWarnedCountries;
	TSet<ECountryType> RepresentativeWarnedCountries;

	// 판에 마지막으로 밀어 넣은 값. MapTable 이 없어도 상태가 성립하도록 위젯 밖에 따로 들고 있다.
	FVector2D AppliedFocusTranslation = FVector2D::ZeroVector;
	float AppliedFocusScale = 1.0f;

	ECountryType SelectedCountry = ECountryType::None;
	// 닫히는 상세에는 새 내용을 넣지 않고 제거가 끝난 뒤 마지막으로 누른 국가 하나만 연다.
	ECountryType PendingCountryAfterDrawerClose = ECountryType::None;
	FVector2D FocusAnchor = FVector2D::ZeroVector;
	bool bHasFocusAnchor = false;
	bool bFocusLayoutPending = false;
	FVector2D LastFocusBoardSize = FVector2D::ZeroVector;
	FSlateRect LastFocusVisibleRect;
	FSlateRect LastFocusSelectionBounds;

	// 드로어가 없는 트리에서 push 할 때마다 짖지 않게 한 번으로 묶는다.
	bool bDrawerMissingWarned = false;

	// 포커스 이동 애니 상태. 선택 링의 반복 강조와 별도로 진행된다.
	FVector2D FocusStartTranslation = FVector2D::ZeroVector;
	FVector2D FocusTargetTranslation = FVector2D::ZeroVector;
	float FocusStartScale = 1.0f;
	float FocusTargetScale = 1.0f;
	float FocusElapsed = 0.0f;
	float FocusDuration = 0.0f;

	// UI 모션 예산 0.3s 이내 (UI_VFX_SOURCEBOOK §2.5)
	static constexpr float FocusAnimDuration = 0.25f;
	// 판 텍스처가 뭉개지기 시작하는 1.6 아래로 잡은 값.
	static constexpr float FocusZoom = 1.35f;

	// ── 국가 디오라마(라이브 화면 + 지도 미니어처) ──
	UOverseasDioramaSubsystem* GetDioramaSubsystem() const;
	UMaterialInterface* GetDioramaRTMaterial();
	bool TryBeginDioramaLive();
	// 가시 영역을 DioramaLayer 로컬로 옮겨 DioramaView 를 놓고, 라이브 RT 크기(0 = 미룸)를 돌려준다.
	FIntPoint LayoutDioramaView();
	void TickDiorama(float DeltaTime);
	void ApplyDioramaVisuals();
	void BindDioramaDetail(UCountryDetailWidget& Detail);
	void UnbindDioramaDetail();
	void RequestUnlockedMiniatures();
	void ApplyMiniature(UCountryNameWidget& Pin, ECountryType Country, bool bUnlocked);
	void HandleMiniatureUpdated(ECountryType Country);

	UFUNCTION()
	void HandleDetailMainTabChanged(int32 TabIndex);

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> DioramaViewMID;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> DioramaRTMaterial;

	TWeakObjectPtr<UCountryDetailWidget> DioramaDetail;
	FDioramaViewTransition DioramaTransition;
	ECountryType DioramaCountry = ECountryType::None;
	EDioramaModule DioramaPrimaryModule = EDioramaModule::Factory;
	bool bDioramaStartPending = false;
	bool bDioramaMaterialWarned = false;
	FDelegateHandle MiniatureUpdatedHandle;
};
