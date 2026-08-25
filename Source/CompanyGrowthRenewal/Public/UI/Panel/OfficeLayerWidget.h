// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Enum/ResourceType.h"
#include "Enum/BubbleType.h"
#include "Enum/CompanyType.h"
#include "Enum/ProjectLifecycle.h"
#include "OfficeLayerWidget.generated.h"

class UButtonWidget;
class UCanvasPanel;
class UResourceWidget;
class UImage;
class UHorizontalBox;
class UCommonTextBlock;
class UTableManagerSubsystem;
class URecruitmentManagerSubsystem;
class UOfficeManager;
class UCoinFlyoutContainerWidget;
class UBubbleContainerWidget;
class AWorkstationActorBase;
class UMissionTrackerWidget;
class UCanvasPanelSlot;
class UCatchRingWidget;
class UGestureHintWidget;
enum class EFatigueSlackPhase : uint8;
class AOfficeworker;
class UTexture2D;
class UButton;
class UDiamondProgressWidget;
class URevenueRateChipWidget;

/**
 * OfficeMap 전용 레이어 위젯
 * - 상단 뒤로가기 버튼
 * - 오피스 꾸미기 모드 UI
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeLayerWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCanvasPanel* InGameCanvas;
	
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* BackButton;

	// 블루프린트에 배치된 리소스 위젯 참조 (이름 = WBP UI_OfficeLayer 의 위젯명과 일치해야 바인딩됨)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Money;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Diamond;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Employee;

	// 미션 트래커 카드 — 디자이너가 InGameCanvas에 배치 (없으면 매니저가 뷰포트 폴백 생성).
	// InitTracker 는 MissionManagerSubsystem 이 GetMissionTracker 로 가져와 호출 → 자가 갱신.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UMissionTrackerWidget> MissionTracker = nullptr;

	// 실시간 수익 칩 — 디자이너가 상단 밴드에 배치. 미배치 시 graceful 스킵(InGameLayer 와 같은 롤아웃 패턴).
	UPROPERTY(meta = (BindWidgetOptional))
	URevenueRateChipWidget* RevenueRateChip = nullptr;

	// ===== 상단 밴드 (재화와 같은 상시 레이어에 통합 — OfficeMain 에서 이관) =====
	// 회사 배너: 산업 시그니처색 플레이트 + 글리프 + 회사명/산업 + 빌딩 레벨 배지
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* CompanyPlate;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* CompanyProfileIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* CompanyNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* CompanyIndustryText;

	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* CompanyLevelText;

	// 뱃지(마름모) 테두리 자체가 EXP 게이지 ― 크기 변화 0, 막히면 테두리 색만 앰버라 별도 경고점이 필요 없다
	UPROPERTY(meta = (BindWidgetOptional))
	UDiamondProgressWidget* CompanyLevelProgress;

	// 플레이트 블록 전체를 덮는 투명 버튼 — 탭하면 빌딩 레벨 로드맵
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* CompanyPlateButton;

	UPROPERTY(meta = (BindWidgetOptional))
	UButtonWidget* CodexButton;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void OnBackButtonClicked();

	// 포트폴리오(도감) 패널 열기 — OfficeMain 에서 이관
	void OnCodexBtnClicked();

public:
	// 디자이너가 배치한 미션 트래커 인스턴스 (없으면 nullptr → 매니저가 폴백 생성)
	UMissionTrackerWidget* GetMissionTracker() const { return MissionTracker; }

	// 상단 밴드 데이터 갱신 (회사배너/티어 진행) — OfficeMain 이 라이프사이클 전환마다 호출.
	// 재화는 기존 델리게이트로 자동, 이건 정적 데이터라 명시 갱신.
	void RefreshTopBar();

	// 수익 수집 코인 연출 재생 (오피스 진입/일괄 수집용 — 중앙 팝 원샷)
	void PlayStoredRevenueCollection(int64 CollectedAmount);

	// 튜토리얼 M10b — 사무실 [뒤로] 버튼 (ExitOffice 페이즈 하이라이트 링 타겟)
	UWidget* GetBackButtonWidget() const;

private:
	void BindButtonEvents();
	void UnbindButtonEvents();

	// 리소스 관련
	UPROPERTY()
	TMap<EResourceType, UResourceWidget*> ResourceWidgets;

	UTableManagerSubsystem* TableMgr = nullptr;
	URecruitmentManagerSubsystem* RecruitmentMgr = nullptr;

	int32 CurrentBuildingIndex = INDEX_NONE;

	FDelegateHandle UIResourceChangedHandle;
	FDelegateHandle WorkstationChangedHandle;
	FDelegateHandle RosterChangedHandle;

	UFUNCTION()
	void HandleResourceChanged(EResourceType Type, int64 NewValue);

	void HandleWorkstationCountChanged();

	// 회사 플레이트 블록 탭 → 빌딩 레벨 로드맵 (뱃지가 말하는 대상과 같은 건물)
	UFUNCTION()
	void HandleCompanyPlateClicked();

	// 인원 칩(로스터 / 인원 상한) 갱신의 유일 경로
	void UpdateEmployeeChip();

	// ===== 실시간 수익 칩 =====
	// 오피스 안에선 운영 수익도 금고를 건너뛰고 직원이 지갑에 넣는다(UpdateSingleOperation 의 IsPlayerInOffice 분기).
	// 그래서 상태 분기 없이 직원 지급 요율 합 하나면 "운영중=운영수익 / 방치중=방치수익" 이 저절로 성립한다.
	void RefreshRevenueRateChip();

	float RevenueChipTimer = 0.0f;
	// 매니저 FTSTicker 가 1초라 그보다 잦게 갱신해도 새 정보가 없다
	static constexpr float RevenueChipInterval = 1.0f;

	// OfficeStageProgressManager::OnTierUnlocked 이 dynamic 델리게이트라 UFUNCTION 필수
	UFUNCTION()
	void HandleTierUnlocked(int32 NewTier);

	// ===== 개발중 집중 모드 (뒤로가기 / 포트폴리오 잠금) =====

	UFUNCTION()
	void HandleLifecycleChanged(EProjectLifecycle OldState, EProjectLifecycle NewState);

	// 잠금 상태 진입/해제 — 히트테스트는 즉시 끊고, 페이드는 틱이 이어받는다
	void SetFocusLocked(bool bLocked);

	// 페이드 알파(1=완전히 사라짐)를 두 버튼 opacity 에 반영
	void ApplyFocusFade(float Alpha);

	bool bFocusLocked = false;
	float FocusFadeAlpha = 0.0f;
	static constexpr float FocusFadeSpeed = 12.0f;   // FInterpTo 속도(≈0.2s) — 하단 도크와 동일 체감

	// 코인 연출 중 Money 위젯 자동 업데이트 억제
	bool bSuppressMoneyUpdate = false;

	// 코인 연출 관련 상태
	int64 CoinAnimOldMoney = 0;
	int64 CoinAnimCollectedAmount = 0;
	int32 CoinAnimTotalCoins = 0;
	int32 CoinAnimArrivedCoins = 0;

	// 코인 플라이아웃 위젯 참조
	UPROPERTY()
	UCoinFlyoutContainerWidget* CurrentCoinFlyout = nullptr;

	// 코인 도착 콜백
	void OnCoinArrivedCallback(int32 CoinIndex);
	void OnAllCoinsCompleteCallback();

	// ========== 운영 수익 스트리밍 코인 (직원 머리 위 → 돈 아이콘) ==========

	// 상주 스트리밍 컨테이너 (1회 생성, 위젯 수명 동안 재사용)
	UPROPERTY()
	UCoinFlyoutContainerWidget* StreamCoinFlyout = nullptr;

	// DT Money 아이콘과 UIColor 프레젠테이션 캐시 (1회 로드)
	UPROPERTY()
	UTexture2D* CachedCoinTexture = nullptr;
	FLinearColor CachedCoinColor = FLinearColor::White;
	bool bMoneyPresentationCached = false;

	// ProjectOperationManager::OnIncomeCoinRequested 수신 — 직원 위치에서 코인 발사
	UFUNCTION()
	void OnIncomeCoinRequestedReceived(FVector WorldPos, int64 Amount, int32 EmployeeID);

	// 스트리밍 코인 착지 — 돈 카운터를 실제 보유액으로 갱신 (코인이 돈을 나르는 인과)
	void OnStreamCoinArrivedCallback();

	// ========== 빈 좌석 버블 시스템 ==========

	// 책상 위 빈 좌석 버블 컨테이너 (InGameCanvas에 월드 추적 자식으로 부착)
	UPROPERTY()
	UBubbleContainerWidget* BubbleContainer = nullptr;

	// 배정 변경 전용 델리게이트가 없어 주기적으로 전체 재평가 (책상 변화는 드물어 2초면 충분)
	float BubbleReevalTimer = 0.0f;
	static constexpr float BubbleReevalInterval = 2.0f;

	// 모든 책상의 빈 좌석 버블 상태 재평가
	void EvaluateAllWorkstationBubbles();

	// 단일 책상의 버블 타입 결정 (미배정 좌석 있으면 EmptySeat)
	EBubbleType EvaluateBubbleTypeForWorkstation(AWorkstationActorBase* Workstation) const;

	// 버블 클릭 수신 → 해당 책상의 WorkstationInfo 패널 열기
	void HandleBubbleAction(int32 Key, EBubbleType Type);

	// ========== 농땡이 캐치 펄스 링 (슬랙 텔레그래프 직원 위 어포던스) ==========

	// 링 풀 — InGameCanvas 자식으로 생성한 무인 위젯들. IsAwaitingCatch 직원 수만큼 사용, 나머지는 Collapsed 보관.
	// 텔레그래프가 ~3초로 짧아 즉각 뜨고/사라져야 하므로 매 틱 폴링(NativeTick).
	UPROPERTY()
	TArray<UCatchRingWidget*> CatchRingPool;

	// 링과 같은 인덱스의 라이브 힌트(탭 손 + "탭해서 깨우기/불러오기"). 졸업 전까지만 보인다. HitTestInvisible — 탭은 링이 받는다
	UPROPERTY()
	TArray<UGestureHintWidget*> CatchHintPool;
	UGestureHintWidget* GetOrCreateCatchHint(int32 PoolIndex);
	void PlaceCatchHint(int32 PoolIndex, UCatchRingWidget* InRing, AOfficeworker* InWorker);
	void HideCatchHint(int32 PoolIndex);

	// 풀에서 인덱스 기반 링 1개를 가져오거나 새로 만들어 InGameCanvas 에 붙인다(PoolIndex 는 Num() 와 일치하게 호출).
	UCatchRingWidget* GetOrCreateCatchRing(int32 PoolIndex);

	// 직원 머리 위 앵커(GetCenterLocation + Z 오프셋)를 캔버스 로컬로 투영(BubbleContainer AbsoluteToLocal 패턴 미러). 화면 밖이면 false.
	bool ProjectWorkerAnchorToCanvas(const AOfficeworker* InWorker, FVector2D& OutCanvasLocal) const;

	// 링 1개의 위치 재투영 + 화면 밖 가시성 처리 (대기 배정/파열 유지 두 경로가 공유).
	void PlaceCatchRing(UCatchRingWidget* InRing, AOfficeworker* InWorker);

	// 캐치 성공 수신 — 연타 감쇠 볼륨 + 피치 랜덤으로 사운드 1회 재생 + 페이즈별 힌트 졸업 카운트.
	void HandleCatchRingCaught(EFatigueSlackPhase CaughtPhase);

	// 연타 감쇠 누산기 (링 인스턴스가 아니라 레이어가 소유 — 다중 링 동시 캐치도 한 누산기로 억제)
	double LastCatchSoundTime = -10.0;
	float CatchSoundVolume = 1.0f;

	// [Perf] 캐치 대기 직원 스캔(전체 액터 순회)은 ~6Hz로만, 링 위치 투영은 매 틱(카메라 추적).
	TArray<TWeakObjectPtr<AOfficeworker>> AwaitingCatchWorkers; // 스캔 결과 캐시(보통 0~소수)
	float CatchScanAccum = 0.0f;

	// 전체 액터 순회로 IsAwaitingCatch 직원을 모아 AwaitingCatchWorkers 갱신(스로틀 호출).
	void ScanAwaitingCatchWorkers();

	// 매 틱 — 캐시된 대기 직원만 순회하여 링 풀 배치/회수 + 위치 재투영(카메라 추적).
	void RefreshCatchRings();
};
