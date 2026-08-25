// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Enum/WidgetType.h"
#include "Enum/ResourceType.h"
#include "Enum/BubbleType.h"
#include "Data/OperationData.h"
// FGaugeCandidate 를 멤버 TArray 로 들기 때문에 전방선언으로는 부족하다
#include "Enum/GaugeHealth.h"
#include "InGameLayerWidget.generated.h"

class UButton;
class UButtonWidget;
class UResourceWidget;
class UCommonTextBlock;
class UTableManagerSubsystem;
class UCanvasPanel;
class UCanvasPanelSlot;
class UBubbleContainerWidget;
class UCoinFlyoutContainerWidget;
class UImage;
class UTextBlock;
class UMissionTrackerWidget;
class ACityPlotActor;
class UPlacementHandler;

UCLASS()
class COMPANYGROWTHRENEWAL_API UInGameLayerWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

private:
	// 리소스 타입별 위젯 맵
	UPROPERTY()
	TMap<EResourceType, UResourceWidget*> ResourceWidgets;

	UTableManagerSubsystem* TableMgr = nullptr;

private:
	// UIManagerSubsystem 델리게이트 핸들
	FDelegateHandle UIResourceChangedHandle;

	// 회전 및 해상도 변경 감지
	FDelegateHandle OrientationDelegateHandle;
	FDelegateHandle ViewportDelegateHandle;

	// 건물 수 변경 구독 핸들 (EntityManager::OnBuildingCountChanged)
	FDelegateHandle BuildingCountChangedHandle;

	// 현재 건물 수 / 최대치를 UIE_Resource_Building에 반영
	void RefreshBuildingCount();

	// 뷰포트 변경 후 아이콘 위치 재계산 지연 타이머
	FTimerHandle IconRecalcTimerHandle;

	UFUNCTION()
	void UpdateIconScreenPos();

	// 콜백: UIManagerSubsystem을 통해 자원 변경 알림 받음
	UFUNCTION()
	void HandleResourceChanged(EResourceType Type, int64 NewValue);

protected:
	// 블루프린트에 배치된 리소스 위젯 참조
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Money;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Brick;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Diamond;

	// 건물 보유/최대치 카운터 (X/Y 형식 표시)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Building;

	// 미션 트래커 카드 — 디자이너가 InGameCanvas에 배치 (없으면 매니저가 뷰포트 폴백 생성)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<class UMissionTrackerWidget> MissionTracker = nullptr;

	// HUD 실시간 수익 칩 — 디자이너가 ResourceBox 에 배치. 미배치 시 graceful 스킵.
	UPROPERTY(meta = (BindWidgetOptional))
	class URevenueRateChipWidget* RevenueRateChip = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCanvasPanel* InGameCanvas;

	// 벽돌 비행과 자원 델타 숫자 전용 최상단 레이어. 월드 추적 UI는 InGameCanvas에 유지한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCanvasPanel* EffectCanvas;

	// 프로필 영역
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* ProfileImage = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* PlayerNameText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* PlayerLevelText = nullptr;

	// 프로필 이미지 클릭 버튼 (프로필 선택 패널 열기)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* ProfileImageBtn = nullptr;

	// 벽돌 수집 버스트 — 틱당 생산량을 쪼개는 아이콘 수 상한
	UPROPERTY(EditAnywhere, Category = "Collect")
	int32 BrickBurstMaxIcons = 6;

	// 아이콘별 분출 페이즈 가산 시간(초) — 흡수만 순차, 카운터 다다닥 상승
	UPROPERTY(EditAnywhere, Category = "Collect")
	float BrickBurstStagger = 0.06f;

	// 코인 플라이아웃(전체수거 등) 아이콘 수 상한. 대량 수거(RevenueMax 트레일러)에서 코인이 우수수 나오도록 상향.
	UPROPERTY(EditAnywhere, Category = "Collect")
	int32 CoinFlyoutMaxIcons = 100;

	// 코인 수 = log10(수거액) × 이 계수 (하한 3 ~ CoinFlyoutMaxIcons 상한). 클수록 같은 금액에 더 많은 코인.
	// PIE에서 실제 RevenueMax 총액을 보고 상한(100)에 닿게 조정할 것.
	UPROPERTY(EditAnywhere, Category = "Collect")
	float CoinFlyoutScalePerDecade = 12.5f;

	// 코인 분출 반경 — 클수록 화면 넓은 구역에 흩뿌려짐(트레일러 "넓게 한번에" 느낌).
	UPROPERTY(EditAnywhere, Category = "Collect")
	float CoinFlyoutSpawnRadius = 700.0f;

	// 코인 간 출발 간격(초) — 작을수록 한번에 터짐. 0에 가까우면 동시 분출.
	UPROPERTY(EditAnywhere, Category = "Collect")
	float CoinFlyoutStagger = 0.01f;

	// 코인 비행시간 랜덤 편차(초) — 클수록 도착이 퍼져 Money 카운터가 도착마다 다다닥 롤업.
	UPROPERTY(EditAnywhere, Category = "Collect")
	float CoinFlyoutArrivalSpread = 0.4f;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

public:
	UResourceWidget* GetResourceWidget(EResourceType Type);

	// 디자이너가 InGameCanvas에 배치한 미션 트래커 인스턴스 (없으면 nullptr → 매니저가 폴백 생성)
	UMissionTrackerWidget* GetMissionTracker() const { return MissionTracker; }

	// M10b 가이드 — 지정 빌딩의 수익 버블(링 타겟). 인덱스 미지정 시 오조준하므로 앵커를 명시받는다.
	UWidget* GetTutorialRevenueBubbleWidget(int32 BuildingIndex) const;

	// M10b 설명 페이즈 스포트라이트 타겟 3종. 미배치/비표시면 nullptr — 매니저는 이 레이어 하나에만 묻는다.
	UWidget* GetRevenueRateChipWidget() const;
	UWidget* GetVaultGaugeWidgetForBuilding(int32 BuildingIndex) const;
	UWidget* GetBubbleWidgetForBuilding(int32 BuildingIndex) const;

	// M7 설명 동안 지정 건물의 기존 게이지 풀 슬롯을 LOD/거리 캡보다 우선해 준비한다.
	void ReserveTutorialVaultGauge(int32 BuildingIndex);
	void ReleaseTutorialVaultGauge();

	// ========== 부지 가격 배지 (인접-미소유 위 상시 표시, 클릭 가능) ==========

	// 인접-미소유 부지 위 잠금+가격 배지(WBP UIE_PlotPriceBadge = UPlotPriceBadgeWidget)를 재평가/재배치.
	// 부지 소유 변경(구매/복원) 시 즉시 호출 + 매 틱 폴링. 배지는 클릭 버튼(자기 부지 인수) — 부지 3D 탭과 동일 경로.
	void RefreshPlotPriceBadges();

	// 프로필 이미지 갱신 (ProfileImagePanel에서 호출)
	void UpdateProfileImage(int32 ImageID);

	UFUNCTION(BlueprintCallable)
	FVector2D GetBrickIconScreenPosition() const;

	// 리소스 타입에 해당하는 아이콘 위치에 브릭 생성 (CollectAmount 개수만큼 증가)
	UFUNCTION(BlueprintCallable)
	void SpawnBrickCollectAt(FVector2D startpos, EResourceType TargetResourceType, int CollectAmount = 1);

	// 해당 자원 HUD 카운터 아이콘 위에 획득 "+N" / 지출 "-N" 플로팅 숫자 팝업
	UFUNCTION(BlueprintCallable)
	void SpawnGainPopup(EResourceType Type, int64 DeltaAmount);

	UFUNCTION(BlueprintCallable)
	void SpawnSpendPopup(EResourceType Type, int64 DeltaAmount);

	// 전체 빌딩 StoredRevenue 일괄 수거 + 코인 플라이아웃 애니메이션 + 토스트
	// BuildOpenWidget 의 "전체 수거" 버튼에서 호출
	UFUNCTION(BlueprintCallable)
	void ExecuteAllVaultCollection();

	// ===== Money 드립 포커스 (CityCompanyManage 패널 소유) =====
	// 열린 동안 Money 즉시 갱신 억제, 코인 착지 순간에만 롤업 — 코인이 돈을 나르는 인과 연출

	void BeginMoneyDripFocus();
	void EndMoneyDripFocus();

	// 드립 코인 착지 — Money 카운터 실보유액 카운트업 롤 + 펀치 + "+N" 플로팅
	void PlayMoneyDripLanding(int64 Delta);

	// ===== 스크린 링 FX (NativePaint 직접 드로잉 — 텍스처/WBP 불필요) =====

	// 현재 포인터 위치에 탭 링 리플 1회 (위치는 내부에서 GetMousePositionOnViewport 로 취득 —
	// ProjectTouchToGroundPlane 류 물리픽셀 좌표를 넘기면 DPI 불일치)
	void SpawnTapRing();

	// 홀드 동안 포인터 아래 라디얼 진행 게이지. CycleSeconds 마다 한 바퀴 = 생산 주기,
	// 한 바퀴 완료(=생산 틱)마다 게이지 팝 플래시
	void BeginHoldPulse(float CycleSeconds);
	void EndHoldPulse();

	// 선택 건물 위 셰브론 마커 (nullptr = 해제). 좌표는 버블 앵커 체인 재사용, 매 틱 재투영
	void SetSelectedBuildingMarker(class ABuildingBaseActor* Building);

	// 배치 프리뷰 셰브론. 표시 우선순위는 배치 > 선택이며, 유효성은 PlacementHandler가 단일 판정한다.
	void SetPlacementBuildingMarker(class ABuildingBaseActor* Building, UPlacementHandler* PlacementHandler);

private:
	UFUNCTION()
	void HandleHQLevelUp(int32 NewLevel);

	UFUNCTION()
	void OnProfileImageBtnClicked();

	// ========== 건물 버블 시스템 ==========

	UPROPERTY()
	UBubbleContainerWidget* BubbleContainer = nullptr;

	// 폴링 fallback 타이머 — CompanyType/직원 배치 변경은 OnBubbleRefreshRequested 델리게이트로 즉시 반영되고
	// 이 폴링은 외부 시스템 변화 누락 대비 안전망 (5초로 충분)
	float BubbleReevalTimer = 0.0f;
	static constexpr float BubbleReevalInterval = 5.0f;

	// 전역 동시 표시 상한 = 동시에 살아 있는 버블 위젯 수의 상한(건물당 1개 추적 위젯).
	// 3 이었을 때는 만금고 20건에서 17건이 아무 표시도 못 받았다 — 게이지가 만차를 버블에 인계하던 당시
	// 캡 밖 만금고는 게이지도 버블도 없는 완전 무신호가 됐고, 그게 하필 손실 중인 상태(LossByVault)였다.
	// 24 = 스펙 §6 이 게이지 캡에 잡아둔 밴드 상단(16~24)과 같은 값 — 월드 추적 위젯 풀 둘이 근거를 공유한다.
	// 받아들이는 바운드: 버블 24 + 게이지 16 = 월드 추적 위젯 최대 40개. 축 전환(2026-08-12) 이후
	// 두 집합은 더 이상 배타가 아니라 겹칠 수 있어 40 이 실제 동시 상한이다(겹침은 버블 리프트로 해소).
	static constexpr int32 MaxConcurrentBubbles = 24;

	// 단일 건물 이벤트는 즉시 갱신 대신 이 플래그로 다음 틱 전역 재평가에 합류 (상한 3개 정합 + 이벤트 버스트 합산)
	bool bBubbleReevalPending = false;

	// 현재 금고가 만액인 건물 집합 — HandleWarehouseUpdated 가 "상태가 뒤집혔는지" 를 판정하는 근거.
	// ⚠ 건물 목록이 바뀌면 반드시 비울 것: 파괴된 인덱스가 남으면 재사용된 인덱스의 첫 전이를 삼킨다.
	TSet<int32> FullVaultBuildings;

	// 전체 건물 버블 상태 일괄 재평가
	void EvaluateAllBuildingBubbles();

	// 창문 발광 단일 판정처 — 프로젝트형은 운영 중일 때만, 제조업은 상시(운영 페이즈가 없어 영원히 꺼지는 걸 방지)
	static bool ShouldWindowLightBeOn(class ABuildingBaseActor* Building, class UProjectOperationManager* OpMgr);

	// ========== 부지 가격 배지 (인접-미소유 위 상시, WBP UIE_PlotPriceBadge 풀) ==========

	// 배지 풀 — InGameCanvas 자식으로 생성한 WBP 위젯들. 인접-미소유 부지 수만큼 사용, 나머지는 Collapsed 로 보관.
	UPROPERTY()
	TArray<class UPlotPriceBadgeWidget*> PlotPriceBadgePool;

	// 풀에서 활성(현재 사용 중) 배지 1개를 가져오거나 새로 만들어 InGameCanvas 에 붙인다(인덱스 기반).
	class UPlotPriceBadgeWidget* GetOrCreatePlotBadge(int32 PoolIndex);

	// 인접-미소유 부지 앵커를 캔버스 로컬로 투영(BubbleContainer AbsoluteToLocal 패턴 미러). 화면 밖이면 false.
	bool ProjectPlotAnchorToCanvas(const class ACityPlotActor* Plot, FVector2D& OutCanvasLocal) const;

	// 매 틱 활성 배지 위치만 재투영(카메라 추적). 멤버십/가격/자금색은 RefreshPlotPriceBadges 가 주기/이벤트로 갱신.
	void UpdatePlotBadgePositions();

	// RefreshPlotPriceBadges 가 배정한 활성(인접-미소유) 배지 수 — UpdatePlotBadgePositions 의 추적 범위.
	int32 ActivePlotBadgeCount = 0;

	// [Perf] 부지 배지는 고정 위치라 카메라 시점이 안 변하면 재투영 불필요 — 시점 캐시로 매 틱 재투영 스킵.
	FVector LastBadgeViewLoc = FVector::ZeroVector;
	FRotator LastBadgeViewRot = FRotator::ZeroRotator;
	bool bForcePlotBadgeReproject = true; // 배지 재배정/소유변경 후 1회 강제 재투영

	// ===== 금고 게이지 풀 =====
	// PlotPriceBadge 풀 패턴 미러. InGameCanvas 자식으로 생성, 미사용분은 Collapsed 로 보관.
	// 공용 풀로 추출하지 않은 이유 = 동시 세션 충돌 위험(스펙 §3.2). 추출은 후속 작업.
	UPROPERTY()
	TArray<class UVaultGaugeWidget*> VaultGaugePool;

	// 풀에서 게이지 1개를 가져오거나 새로 만들어 InGameCanvas 에 붙인다(인덱스 기반).
	class UVaultGaugeWidget* GetOrCreateVaultGauge(int32 PoolIndex);

	// 후보 산출 -> LOD -> 중앙 근접 정렬 -> 캡 절단 -> 값·색 주입. 주기 호출(매 프레임 금지).
	void RefreshVaultGauges();

	// 매 틱 활성 게이지 위치·폭만 재투영(카메라 추적).
	void UpdateVaultGaugePositions();

	// 현재 카메라 줌에 히스테리시스를 적용해 표현 LOD를 갱신한다.
	void UpdateVaultGaugeLOD();
	float GetVaultGaugeZoomValue() const;

	// 건물의 게이지 앵커(옥상)와 화면상 건물 폭을 함께 투영. 화면 밖이면 false.
	bool ProjectBuildingGaugeGeometry(const class ABuildingBaseActor* Building,
		FVector2D& OutCanvasLocal, float& OutScreenWidth) const;

	// RefreshVaultGauges 가 확정한 활성 게이지 목록 — UpdateVaultGaugePositions 의 추적 범위.
	TArray<FGaugeCandidate> ActiveGaugeCandidates;

	// [Perf] 건물은 고정 → 화면 위 게이지 위치는 오직 카메라 시점에 의존. 형제 UpdatePlotBadgePositions 와 동일한
	// 시점 캐시(idle 게임이라 카메라가 대부분 정지). 후보 재배정 후에는 bForce 로 1회 통과시킨다.
	FVector LastGaugeViewLoc = FVector::ZeroVector;
	FRotator LastGaugeViewRot = FRotator::ZeroRotator;
	bool bForceVaultGaugeReproject = true;

	// 창고 만액 감지 전용 ― Bar 길이는 시간 축이라 창고 값을 더는 읽지 않는다.
	UFUNCTION()
	void HandleWarehouseUpdated(int32 BuildingID, float Amount, float Capacity);

	// 건강도와 진행률을 한 번에 뽑는다 ― 둘 다 같은 FOperationData 조회에서 나오므로 조회를 쪼개지 않는다
	void EvaluateGauge(class UProjectOperationManager* OpMgr, int32 BuildingID,
		EGaugeHealth& OutHealth, float& OutProgress) const;

	// 활성 게이지의 진행률 재주입. 진행률은 연속값이라 이벤트 소스가 없어 매 틱 조회한다
	void RefreshVaultGaugeValues();

	// 후보 멤버십 변화만 다음 틱 전체 재조정으로 합류시킨다.
	void RequestVaultGaugeReconcile(EVaultGaugeReconcileTrigger Trigger);

	// 위치·폭 적용 단일 판정처. 배정 시점(RefreshVaultGauges)과 매 틱 추적이 같은 산식을 쓰게 한다.
	void ApplyGaugeTransform(class UVaultGaugeWidget* Gauge, const FVector2D& CanvasPos, float ScreenWidth) const;

	// 콜드 진입 초기 갱신 대기 — NativeConstruct 시점엔 아직 페인트 전이라 InGameCanvas 의 캐시 지오메트리가
	// 기본 크기다. 그때 RefreshVaultGauges 를 부르면 투영이 CanvasSize<=0 가드에서 전부 실패하고,
	// 투영 실패 후보는 버려지므로 결과가 빈 배열 = 호출이 무효다.
	// ⚠ 조건은 틱 인덱스가 아니라 "캔버스가 실제 크기를 가졌는가" — 첫 틱에 유효하다고 가정하지 말 것.
	bool bVaultGaugeInitialRefreshPending = true;

	// 후보 재조정 요청 — 다음 틱에 RefreshVaultGauges 를 1회 돌려 멤버십/표현/캡을 다시 확정한다.
	// 멤버십은 운영 유무에만 의존하므로 창고 값 갱신은 이 플래그를 올리지 않는다(값은 RefreshVaultGaugeValues 가 매 틱 담당).
	bool bVaultGaugeReevalPending = false;
	EVaultGaugeLOD CurrentVaultGaugeLOD = EVaultGaugeLOD::Uninitialized;
	int32 TutorialReservedGaugeBuildingIndex = INDEX_NONE;

	// ===== HUD 수익 칩 =====

	// 유량 주입 주기. ⚠ BubbleReevalInterval(5초)에 얹지 말 것 — 칩의 펄스가 1초 박자라
	// 값이 5초마다 계단으로 튀면 박자와 어긋나 고장으로 읽힌다. 매니저 FTSTicker 가 1.0초라
	// 1초가 유효 해상도의 상한이다. (5초 리듬은 버블·부지배지 경로와 공유하므로 줄일 수 없다.)
	float RevenueChipTimer = 0.0f;
	static constexpr float RevenueChipInterval = 1.0f;

	// 회사 전체 유량을 칩에 주입. 미배치(BindWidgetOptional null)면 아무 일도 하지 않는다.
	void RefreshRevenueRateChip();

	// ===== 게이지 노브 (PIE 실측 대상 — 스펙 §6) =====

	// 동시 표시 상한. ⚠ 커버리지 100% 를 목표로 올리지 말 것 — 이건 GPU 안전판이고
	// 정밀 판독의 경로는 줌인이다(스펙 §3.2 "커버리지 목표").
	UPROPERTY(EditAnywhere, Category = "VaultGauge|LOD")
	int32 MaxConcurrentGauges = 16;

	// 게이지 폭 = 건물 화면폭 × 이 비율, 아래 상하한으로 클램프.
	UPROPERTY(EditAnywhere, Category = "VaultGauge|Size")
	float GaugeWidthRatio = 0.70f;

	UPROPERTY(EditAnywhere, Category = "VaultGauge|Size")
	float MinGaugeWidth = 128.0f;

	UPROPERTY(EditAnywhere, Category = "VaultGauge|Size")
	float MaxGaugeWidth = 220.0f;

	// 게이지를 옥상에서 위로 띄우는 여유(px). 0 = 옥상에 접함(확정값).
	UPROPERTY(EditAnywhere, Category = "VaultGauge|Size")
	float GaugeLiftPx = 0.0f;

	// 게이지가 뜬 건물의 버블만 이만큼 위로. Bar 와 버블은 앵커·정렬(0.5,1.0)·ZOrder(-1)가 전부 같아
	// 안 올리면 정확히 겹친다. 42 = Bar 높이(30) + 여백(12).
	UPROPERTY(EditAnywhere, Category = "VaultGauge|Size")
	float BubbleLiftForBarPx = 42.0f;

	// Pearl 은 28x28 점으로 강등되므로 Bar 값을 그대로 쓰면 버블이 필요 이상으로 뜬다.
	UPROPERTY(EditAnywhere, Category = "VaultGauge|Size")
	float BubbleLiftForPearlPx = 40.0f;

	// 게이지가 실제로 뜬 건물에만 버블 리프트를 세우고 나머지는 0 으로 되돌린다.
	void ApplyBubbleLiftForActiveGauges();

	// 단일 빌딩 버블 재평가 (BuildingBaseActor::OnBubbleRefreshRequested 핸들러)
	UFUNCTION()
	void HandleBuildingBubbleRefreshRequested(int32 BuildingIndex);

	// EntityManager 빌딩 목록 변경 시 모든 빌딩의 OnBubbleRefreshRequested 구독 재배선
	void RewireBuildingBubbleSubscriptions();

	// 개별 건물의 표시 우선순위에 따른 버블 타입 결정
	EBubbleType EvaluateBubbleTypeForBuilding(int32 BuildingIndex) const;

	// ProjectOperationManager 델리게이트 핸들러
	// (창고 갱신은 HandleWarehouseUpdated 단독 — 게이트 없는 형제를 다시 만들지 말 것)
	UFUNCTION()
	void OnOperationCompletedForBubble(int32 BuildingID, const FOperationData& Data);

	UFUNCTION()
	void OnOperationStartedForBubble(int32 BuildingID);

	UFUNCTION()
	void OnRevenueCollectedForBubble(int64 Amount);

	// 개별 건물 창문 조명 토글 (운영 시작/완료 시)
	void UpdateBuildingWindowLight(int32 BuildingID, bool bActive);

	// ========== 버블 클릭 액션 ==========

	// BubbleContainer에서 클릭 이벤트 수신
	void HandleBubbleAction(int32 BuildingIndex, EBubbleType Type);
	void HandleActiveBubbleMembershipChanged();

	// VaultFull 버블 클릭: 수금 → 코인 플라이아웃 → 토스트 → 버블 제거
	void ExecuteVaultCollection(int32 BuildingIndex);

	// 코인 플라이아웃 애니메이션 + Money 위젯 동기화 공용 경로 (개별/전체 수거 공통)
	// CollectedAmount 은 이미 Money 에 반영된 상태여야 함
	void PlayCoinFlyoutAnimation(int64 CollectedAmount);

	// 건물 인터랙션 시뮬레이션 (OpenBuildingUI 호출)
	void ExecuteBuildingInteraction(int32 BuildingIndex);

	// 코인 플라이아웃 콜백
	void OnCoinArrivedCallback(int32 CoinIndex);
	void OnAllCoinsCompleteCallback();

	// 코인 플라이아웃 관련 멤버
	UPROPERTY()
	UCoinFlyoutContainerWidget* CurrentCoinFlyout = nullptr;

	bool bSuppressMoneyUpdate = false;
	int64 CoinAnimOldMoney = 0;
	int64 CoinAnimCollectedAmount = 0;
	int32 CoinAnimTotalCoins = 0;
	int32 CoinAnimArrivedCoins = 0;

	// 플로팅 숫자 연타 분산용 인덱스 (증가만, % N 으로만 사용)
	int32 GainPopupStackIndex = 0;

	// 획득/지출 공용 팝업 — 같은 (Type, bSpend) 는 배치 윈도우로 합산 후 1회 스폰 (가챠 10연/강화 연타 스팸 게이트)
	void SpawnAmountPopup(EResourceType Type, int64 Amount, bool bSpend);

	// 배치 윈도우 만료 시 실제 스폰 (구 SpawnAmountPopup 본문)
	void SpawnAmountPopupNow(EResourceType Type, int64 Amount, bool bSpend);

	struct FPendingAmountPopup
	{
		EResourceType Type = EResourceType::Money;
		bool bSpend = false;
		int64 Accum = 0;
		float Remaining = 0.f;
	};

	// 키 = (Type << 1) | bSpend
	TMap<uint32, FPendingAmountPopup> PendingAmountPopups;
	static constexpr float AmountPopupBatchWindow = 0.2f;

	// NativeTick 에서 윈도우 만료 배치를 스폰
	void FlushPendingAmountPopups(float DeltaTime);

	// 벽돌 착지음 배치 간 감쇠 공유 상태 — BrickCollectWidget 은 벽돌마다 새 인스턴스라 상태를 못 들므로 레이어가 든다
	double LastBrickLandSoundTime = -1.0;
	float BrickLandSoundVolume = 1.0f;

	// ========== 스크린 링 FX 상태 (Tick 에서 갱신, NativePaint 는 읽기만) ==========

	// 링 글로우 브러시 — DT_UIVFXTexture("RingGlow")에서 NativeConstruct 시 주입 (DT 단일 진실).
	// UPROPERTY는 브러시가 든 텍스처 GC 보호용. 행 없으면 빈 브러시 = 기존 라인 전용 동작
	UPROPERTY(Transient)
	FSlateBrush RingGlowBrush;

	struct FScreenRingFX
	{
		FVector2D AbsPos = FVector2D::ZeroVector;   // 절대(스크린) 좌표 — 페인트 시 AbsoluteToLocal
		float Elapsed = 0.f;
	};
	TArray<FScreenRingFX> ActiveRings;

	// 홀드 라디얼 게이지
	bool bHoldPulseActive = false;
	FVector2D HoldPulseAbsPos = FVector2D::ZeroVector;
	float HoldPulseElapsed = 0.f;
	float HoldPulseCycle = 0.5f;
	float GaugePopElapsed = -1.f;   // <0 = 비활성. 한 바퀴 완료 시 0 부터 재생

	// 선택/배치 건물 셰브론 마커 — 캔버스 슬롯 배치 (버블과 동일 좌표계 = 투영 정합 보장.
	// 루트 NativePaint 직접 드로잉은 페인트 공간 불일치로 줌 의존 어긋남 발생해 폐기 —
	// 모양은 자가 페인트 위젯(USelectionChevronWidget) 내부 로컬 드로잉이라 무관)
	TWeakObjectPtr<class ABuildingBaseActor> MarkerBuilding;
	TWeakObjectPtr<class ABuildingBaseActor> PlacementMarkerBuilding;
	TWeakObjectPtr<UPlacementHandler> PlacementMarkerHandler;
	TWeakObjectPtr<class ABuildingBaseActor> ActiveMarkerBuilding;

	enum class EMarkerVisualState : uint8
	{
		None,
		Selected,
		PlacementValid,
		PlacementInvalid
	};

	EMarkerVisualState MarkerVisualState = EMarkerVisualState::None;
	float MarkerElapsed = 0.f;

	UPROPERTY()
	class USelectionChevronWidget* MarkerChevron = nullptr;

	UPROPERTY()
	UCanvasPanelSlot* MarkerSlot = nullptr;

	void UpdateSelectionMarker(float DeltaTime);

	// 크기/시간 튜닝 상수는 cpp 익명 namespace (수치 조정 = Live Coding 가능)

	void UpdateScreenRings(float DeltaTime);
};
