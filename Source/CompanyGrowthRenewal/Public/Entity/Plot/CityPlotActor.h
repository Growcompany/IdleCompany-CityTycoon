// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Entity/InteractableBaseActor.h"
#include "Interfaces/BubbleAnchorProvider.h"
#include "CityPlotActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * 도시 블록(부지) 1개 = 인스턴스 1개. 레벨에 블록마다 배치하고 PlotId로 DT_CityPlot 행과 연결한다.
 * 클릭 인터랙션은 부모 AInteractableBaseActor 의 BoxComponent(channel1 Block, QueryOnly) +
 * "Interactable" 태그 + IInputHandler 를 그대로 재사용한다.
 *   - 미소유: 탭 → 구매 확인 모달 (Task 3)
 *   - 소유:   탭 입력 없음
 * 미소유 부지만 TryPurchase 경로로 연결하고, 소유 부지 탭은 처리하지 않는다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ACityPlotActor : public AInteractableBaseActor, public IBubbleAnchorProvider
{
	GENERATED_BODY()

public:
	ACityPlotActor();

	// PlotId 로 DT 조회 → Extent 캐시 + BoxComponent 크기를 Extent 에 맞춤
	void Init(FName InPlotId);

	FName GetPlotId() const { return PlotId; }
	bool IsOwned() const { return bOwned; }
	void SetOwnedState(bool bInOwned);

	// 부지 footprint 반경(X,Y) — 건물 배치 제약 bounds (Task 4)
	FVector2D GetExtent() const { return CachedExtent; }
	FVector GetCenter() const { return GetActorLocation(); }

	// 이 부지 블록 메시 위에 footprint 사각형이 통째로 얹혀 있는지(깎인 모서리/블록 밖이면 false).
	// 사각형 4코너 + 중심에서 아래로 StreetSurface(ECC_GameTraceChannel3) 트레이스(bTraceComplex)해 전부
	// 이 부지 블록(또는 /TheRiverwalkCity/ 메시)에 맞아야 true.
	// 블록 미해결이어도 트레이스는 한다 — 그때는 "도시 지면 메시에 맞았는가"만으로 판정한다.
	// "전부 가능" 폴백은 채널이 5점 전부 무응답일 때(= 콜리전/채널 미설정)로 한정된다.
	bool IsFootprintOnGround(const FVector& FootprintCenter, float HalfX, float HalfY) const;

	// footprint 가 블록 밖(인도/깎인 모서리)으로 삐져나갔을 때, MaxPush 안에서 땅 안쪽으로 최소 이동시킨다.
	// 밀 방향은 "실패한 코너들의 부호 합"의 반대로 역산하고, 그 방향으로 이분 탐색해 접지되는 최근접 위치를 찾는다.
	// (블록 메시가 깎인 모서리를 가진 임의 형상이라 공식으로 경계를 못 구한다 — 트레이스가 유일한 권위.)
	// 반환 = 최종 위치가 접지인지. OutAdjustedXY = 보정된 XY(보정 불필요/불가면 입력 XY 그대로).
	// 이미 접지면 평가 1회로 즉시 반환(드래그 상시 경로의 트레이스 비용을 현행 유지).
	bool ResolveFootprintOntoGround(const FVector& FootprintCenter, float HalfX, float HalfY,
		float MaxPush, FVector2D& OutAdjustedXY) const;

	// IBubbleAnchorProvider — 가격 배지가 추적할 부지 상단 월드 위치(부지 중심 위). 가격 배지 앵커로 재사용한다.
	virtual FVector GetBubbleAnchorPosition() const override;

	// 인수 시도(게이트 단일 구현) — 인접 아니면 토스트, 자금 부족이면 토스트, 둘 다 통과면 ConfirmCancel 모달.
	// 진입점 2개: 미소유 부지 3D 탭(OnEndInteract) + 가격 배지(UPlotPriceBadgeWidget) 클릭 — 둘 다 이 함수 호출.
	void TryPurchase();

	// 부수효과 없이 "지금 인수하면 성공하는가"만 판정(토스트/모달 없음) — 가이드 타겟팅(가격 배지) 전용.
	// TryPurchase() 의 게이트 0/1/2 를 미러링한다 — 그쪽 게이트가 바뀌면 이 함수도 같이 고칠 것(안 그러면 소프트락 재발).
	bool CanBePurchasedNow() const;

	// IInputHandler — 미소유=TryPurchase(), 소유=no-op.
	virtual void OnEndInteract_Implementation(APlayerController* InstigatingPC) override;

protected:
	virtual void BeginPlay() override;

	// 인수 게이트 통과 후 — 공통 ConfirmCancel 다이얼로그(부지 인수/취소)를 push + OnConfirm 바인딩
	void OpenPurchaseModal();

	// 모달 [인수] 콜백 — Money 결제 → 소유 전환 → 세이브
	UFUNCTION()
	void HandlePurchaseConfirmed();

	// ConfirmCancel 닫힘 시(확인/취소 공통) UI 입력 모드를 Normal 로 복원 — 네이티브 멀티캐스트 바인딩(UFUNCTION 불필요).
	void RestoreNormalInputMode();

	// AInteractableBaseActor 의 순수 가상함수 — 부지는 EntityManager 건물 목록에 등록하지 않으므로 no-op
	virtual void RegisterWithEntityManager() override {}
	virtual void UnregisterWithEntityManager() override {}

	// 미소유 부지 상시 다크 틴트 메시. 평면이 아니라 그 부지 위치의 실제 도시 블록 타일(Street/Park)
	// 메시를 같은 트랜스폼으로 복사해(깎인 모서리/회전 정확 일치) 반투명 다크 머티리얼(M_PlotHighlight)을
	// 입힌다. 소유 시 숨겨 부지가 밝아진다.
	UPROPERTY(VisibleAnywhere, Category = "Plot")
	UStaticMeshComponent* PlotTintSMC = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* PlotTintMID = nullptr;

	// 부지 위치의 블록 타일 메시를 찾아 복사 + 다크 머티리얼 적용 + 현재 소유 상태에 맞춰 가시성 적용.
	// 블록을 못 찾으면(레벨 인스턴스 미로드 등) 다음 프레임 재시도한다(상한까지).
	void SetupPlotTint();

	// footprint 5점(중심 + 4코너)의 개별 접지 결과. IsFootprintOnGround(전부 통과 판정)와
	// ResolveFootprintOntoGround(실패 코너로 밀 방향 역산)가 이 단일 트레이스 구현을 공유한다.
	// 인덱스: 0=중심, 1=(-X,-Y), 2=(+X,-Y), 3=(-X,+Y), 4=(+X,+Y).
	// 반환 = 트레이스 채널이 응답했는지. false 면 채널/콜리전 미설정 → 호출자는 "전부 가능" 폴백.
	bool EvaluateFootprintPoints(const FVector& FootprintCenter, float HalfX, float HalfY,
		bool (&OutPointsOnBlock)[5]) const;

	// 블록 탐색 재시도 횟수(무한 루프 방지)
	int32 PlotTintRetryCount = 0;

	// 블록 탐색 재시도 타이머(간격 재시도 — 핸들을 들고 있어야 중복 예약이 안 쌓인다)
	FTimerHandle PlotTintRetryTimer;

	// "접지 판정 권위 없음" 경고를 부지당 1회만 내기 위한 플래그. 판정은 const 경로라 mutable.
	mutable bool bLoggedNoTraceAuthority = false;

	// SetupPlotTint 가 찾은 이 부지의 블록 액터(IsFootprintOnGround 트레이스의 권위 기준). 약참조.
	TWeakObjectPtr<AActor> CachedBlockActor;

	// 블록을 한 번이라도 찾았는지. false 면 접지 판정이 "이 블록인가"를 못 보고 "도시 지면인가"까지만 본다
	// (판정 자체는 계속 동작 — 트레이스를 건너뛰지 않는다). 진단 로그 컨텍스트로도 쓴다.
	bool bBlockResolved = false;

	// DT_CityPlot 의 RowName 과 일치시킬 것 (레벨 배치 시 에디터에서 지정)
	UPROPERTY(EditAnywhere, Category = "Plot")
	FName PlotId = NAME_None;

	// 소유 상태 (런타임). 세이브 복원/구매 확정은 Task 3 에서 SetOwnedState 호출.
	bool bOwned = false;

	// Init 에서 DT 로부터 캐시한 Extent
	FVector2D CachedExtent = FVector2D::ZeroVector;
};
