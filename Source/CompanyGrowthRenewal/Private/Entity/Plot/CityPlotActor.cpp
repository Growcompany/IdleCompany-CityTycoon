// Fill out your copyright notice in the Description page of Project Settings.


#include "Entity/Plot/CityPlotActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h" // LineTraceMulti / ETraceTypeQuery / EDrawDebugTrace (footprint 바닥 트레이스)
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/SpawnManager.h"
#include "Table/CityPlotData.h"
#include "Table/ConstructionCost.h"
#include "Table/BuildingData.h" // FootprintCellSize (격자 셀 한 변 크기) 단일 소스
#include "Enum/ResourceType.h"
#include "Enum/WidgetType.h"
#include "Enum/NotificationType.h"
#include "UI/UIBase.h"
#include "UI/Element/Common/ConfirmCancelWidget.h" // 공통 확인/취소 다이얼로그 재사용(전용 모달 폐기)
#include "UI/Panel/InGameLayerWidget.h"
#include "Core/CGGameInstance.h"
#include "Player/MainMapPlayerController.h"
#include "Manager/CityAcquisitionManager.h"
#include "Manager/MissionManagerSubsystem.h"

ACityPlotActor::ACityPlotActor()
{
	// 부지는 Wobble Timeline 을 쓰지 않으므로 Tick 불필요
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 클릭 트레이스(InteractableInputHandler)가 잡으려면 "Interactable" 태그 필요.
	// BoxComponent(channel1 Block)·IInputHandler 는 부모 생성자에서 이미 셋업됨 — 재생성 금지.
	Tags.Add(FName("Interactable"));

	// 미소유 부지 상시 다크 틴트 평면. 부지 액터 자체 컴포넌트(건물 메시/배치 로직과 무관).
	// 메시/머티리얼/크기 세팅은 Init→SetupPlotTint 에서(런타임 소프트 로드).
	PlotTintSMC = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlotTintMesh"));
	PlotTintSMC->SetupAttachment(RootComponent);
	PlotTintSMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlotTintSMC->SetGenerateOverlapEvents(false);
	PlotTintSMC->SetCanEverAffectNavigation(false);
	PlotTintSMC->SetCastShadow(false);
}

void ACityPlotActor::BeginPlay()
{
	Super::BeginPlay();

	// 에디터에서 PlotId 를 지정한 채 배치된 경우, 시작 시 DT 값으로 Extent/Box 동기화
	if (!PlotId.IsNone())
	{
		Init(PlotId);
	}
}

void ACityPlotActor::Init(FName InPlotId)
{
	PlotId = InPlotId;

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CityPlot] Init: TableManager null (PlotId=%s)"), *PlotId.ToString());
		return;
	}

	bool bOk = false;
	const FCityPlotData PlotData = TableMgr->GetCityPlotData(PlotId, bOk);
	if (!bOk)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CityPlot] Init: no DT row for PlotId=%s"), *PlotId.ToString());
		return;
	}

	// Extent(footprint 반경)는 격자(GridCols/Rows × 셀크기)에서 산출한다.
	// 반경 = 칸수 × 셀크기 / 2. DT 의 Extent 필드는 보조/derive 용도(직접대입하지 않음).
	const int32 GridCols = FMath::Max(1, PlotData.GridCols);
	const int32 GridRows = FMath::Max(1, PlotData.GridRows);
	CachedExtent.X = GridCols * FootprintCellSize * 0.5f;
	CachedExtent.Y = GridRows * FootprintCellSize * 0.5f;

	// 인터랙션 볼륨을 부지 격자 footprint 에 맞춤 (2D extent → Box extent X/Y, Z 는 기존 볼륨 두께 유지)
	if (BoxComponent)
	{
		constexpr float PlotBoxHeight = 500.0f;
		BoxComponent->SetBoxExtent(FVector(CachedExtent.X, CachedExtent.Y, PlotBoxHeight));
	}

	// 소유 상태는 Init 이 건드리지 않는다 — SpawnCityPlots(bOwnedAtStart) + RestorePlotOwnership/구매(Task3) 가 단일 권위.

	// CachedExtent 확정 후 틴트 평면을 부지 격자 크기로 맞추고 현재 소유 상태대로 가시성 적용
	SetupPlotTint();
}

namespace
{
	// 도시 지면 메시 판정 — 원본 도시 타일과, 여러 타일을 하나로 합친 머지 메시 둘 다 인정한다.
	// 머지 메시는 에디터 Merge Actors 산출물이라 /TheRiverwalkCity/ 가 아닌 _GENERATED 아래로 떨어진다.
	bool IsCityGroundMesh(const UStaticMesh* Mesh)
	{
		if (!Mesh) { return false; }
		const FString Path = Mesh->GetPathName();
		return Path.Contains(TEXT("/TheRiverwalkCity/")) || Path.Contains(TEXT("/_GENERATED/"));
	}

	// [Perf] 부지 틴트용 스트리트 타일 후보 수집을 "프레임당 1회"만(부지 N개가 공유) — 부지마다 전체 액터 스캔/문자열/바운드 하던 걸 제거.
	struct FStreetTileCandidate { FVector BoundsOrigin; AStaticMeshActor* Actor; UStaticMeshComponent* SMC; };
	TWeakObjectPtr<UWorld> GTintCacheWorld;
	uint64 GTintCacheFrame = static_cast<uint64>(-1);
	TArray<FStreetTileCandidate> GTintCandidates;

	const TArray<FStreetTileCandidate>& GetStreetTileCandidates(UWorld* Wld)
	{
		// 같은 월드·같은 프레임이면 재사용. 프레임 단위로만 재스캔 → 스트리밍 중 재시도(다음 프레임)엔 자동 최신화.
		if (GTintCacheWorld.Get() == Wld && GTintCacheFrame == GFrameCounter)
		{
			return GTintCandidates;
		}
		GTintCacheWorld = Wld;
		GTintCacheFrame = GFrameCounter;
		GTintCandidates.Reset();

		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(Wld, AStaticMeshActor::StaticClass(), Found);
		for (AActor* Candidate : Found)
		{
			AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Candidate);
			if (!MeshActor) continue;
			UStaticMeshComponent* CandidateSMC = MeshActor->GetStaticMeshComponent();
			UStaticMesh* CandidateMesh = CandidateSMC ? CandidateSMC->GetStaticMesh() : nullptr;
			if (!IsCityGroundMesh(CandidateMesh)) continue;

			FVector BoundsOrigin, BoundsExtent;
			MeshActor->GetActorBounds(false, BoundsOrigin, BoundsExtent);
			GTintCandidates.Add({ BoundsOrigin, MeshActor, CandidateSMC });
		}
		return GTintCandidates;
	}
}

void ACityPlotActor::SetupPlotTint()
{
	if (!PlotTintSMC)
	{
		return;
	}

	UWorld* Wld = GetWorld();
	if (!Wld)
	{
		return;
	}

	// 이 부지 위치의 실제 도시 블록 타일(Street/Park) 메시를 찾는다.
	// 블록 타일은 LevelInstance(TheRiverwalkInstance) 소속이지만 런타임에는 AStaticMeshActor 로 잡힌다.
	// 식별: StaticMesh 경로에 "/TheRiverwalkCity/Street/" 포함 + 부지 중심에 가장 가까운 것.
	// 매칭 기준 = 블록의 "바운드 중심"(피벗 아님). 공원/큰 타일은 피벗이 바운드 중심에서 멀어
	// 피벗 거리로는 못 찾으므로(틴트 누락), GetActorBounds 의 origin 으로 매칭한다.
	// 부지 액터 위치(=DT Center=블록 바운드중심)와의 바운드중심 거리로 가장 가까운 + 충분히 가까운 것.
	const FVector PlotLocation = GetActorLocation();
	constexpr float SearchRadiusCm = 2000.f;     // 후보 반경(부지 중심 ↔ 블록 바운드 중심)
	const float SearchRadiusSq = SearchRadiusCm * SearchRadiusCm;

	AStaticMeshActor* BlockActor = nullptr;
	UStaticMeshComponent* BlockSMC = nullptr;
	float ClosestDistSq = SearchRadiusSq;

	// [Perf] 후보(스트리트 타일)는 프레임당 1회만 수집해 모든 부지가 공유 — 여기선 최근접만 고른다.
	const TArray<FStreetTileCandidate>& Candidates = GetStreetTileCandidates(Wld);
	for (const FStreetTileCandidate& C : Candidates)
	{
		const float DistSq = FVector::DistSquared(C.BoundsOrigin, PlotLocation);
		if (DistSq < ClosestDistSq)
		{
			ClosestDistSq = DistSq;
			BlockActor = C.Actor;
			BlockSMC = C.SMC;
		}
	}

	// 블록을 못 찾음 — LevelInstance 가 아직 로드 안 됐을 수 있다.
	// 재시도 예산은 프레임이 아니라 시간으로 잡는다 — 다음틱 30회(=30프레임)는 로딩이 느린 기기에서
	// 도시가 뜨기 전에 소진돼 블록이 영구 미해결로 남는다(틴트 누락 + 접지 판정 권위 상실).
	if (!BlockActor || !BlockSMC)
	{
		constexpr int32 MaxPlotTintRetries = 40;
		constexpr float PlotTintRetryInterval = 0.5f; // 40 x 0.5s = 20초 예산
		if (PlotTintRetryCount < MaxPlotTintRetries)
		{
			++PlotTintRetryCount;
			Wld->GetTimerManager().SetTimer(PlotTintRetryTimer, this, &ACityPlotActor::SetupPlotTint,
				PlotTintRetryInterval, false);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[CityPlot] SetupPlotTint: no city block tile found near plot (PlotId=%s)"), *PlotId.ToString());
		}
		// 블록을 못 찾는 동안엔 틴트 숨김(크래시 회피·그레이스풀).
		PlotTintSMC->SetVisibility(false);
		return;
	}

	UStaticMesh* BlockMesh = BlockSMC->GetStaticMesh();

	// 블록 메시 복사(평면 아님) — 깎인 모서리/모양 그대로.
	PlotTintSMC->SetStaticMesh(BlockMesh);

	// 다크 틴트 머티리얼은 4b 하이라이트와 동일한 M_PlotHighlight 재사용("Color" 벡터 파라미터).
	// 로드 실패면 PlotTintMID=null → 틴트 없이 진행(graceful).
	if (!PlotTintMID)
	{
		TSoftObjectPtr<UMaterialInterface> TintMatPath(
			FSoftObjectPath(TEXT("/Game/CompanyGrowth/Resources/Materials/M_PlotHighlight.M_PlotHighlight")));
		if (UMaterialInterface* TintMat = TintMatPath.LoadSynchronous())
		{
			PlotTintMID = UMaterialInstanceDynamic::Create(TintMat, this);
		}
	}
	if (PlotTintMID)
	{
		// 은은한 다크(약간 푸른빛) + 0.5 알파.
		PlotTintMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.02f, 0.04f, 0.5f));

		// 블록 메시는 멀티 슬롯일 수 있으므로 모든 머티리얼 슬롯에 동일 DMI 적용.
		const int32 NumMaterials = PlotTintSMC->GetNumMaterials();
		for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
		{
			PlotTintSMC->SetMaterial(SlotIndex, PlotTintMID);
		}
	}

	// 월드 트랜스폼을 블록과 정확히 일치(크기·깎인모양·회전). Z 만 +3cm 띄워 z-fight 회피.
	PlotTintSMC->SetWorldLocation(BlockActor->GetActorLocation() + FVector(0.f, 0.f, 3.f));
	PlotTintSMC->SetWorldRotation(BlockActor->GetActorRotation());
	PlotTintSMC->SetWorldScale3D(BlockSMC->GetComponentScale());

	// 미소유일 때만 표시(소유=틴트 제거 → 밝아짐).
	PlotTintSMC->SetVisibility(!bOwned);

	// 블록을 확정한 직후 권위 기준으로 캐시 — IsFootprintOnGround 트레이스가 이 블록(또는 도시 폴더 메시)에
	// 맞는지로 깎인 모서리/블록 밖 footprint 를 걸러낸다. (매 프레임 X, 트레이스는 배치 중에만.)
	CachedBlockActor = BlockActor;
	bBlockResolved = true;
}

bool ACityPlotActor::IsFootprintOnGround(const FVector& FootprintCenter, float HalfX, float HalfY) const
{
	bool PointsOnBlock[5];
	if (!EvaluateFootprintPoints(FootprintCenter, HalfX, HalfY, PointsOnBlock))
	{
		return true; // 트레이스 권위 없음(블록 미해결/채널 미설정) → 막지 않음
	}

	for (const bool bPointOnBlock : PointsOnBlock)
	{
		if (!bPointOnBlock)
		{
			return false; // 한 점이라도 블록 밖 = 깎인 모서리/인도
		}
	}
	return true;
}

// 논리 footprint와 메시 AABB 답이 갈림 → 실메시 트레이스가 최종 판정
bool ACityPlotActor::EvaluateFootprintPoints(const FVector& FootprintCenter, float HalfX, float HalfY,
	bool (&OutPointsOnBlock)[5]) const
{
	// 권위가 없으면 호출자가 "전부 가능"으로 해석하도록 전부 true 로 두고 false 반환.
	for (bool& bPoint : OutPointsOnBlock)
	{
		bPoint = true;
	}

	// 블록 미해결이어도 트레이스 수행. 1차 판정 = StreetSurface 채널 응답(도시 지면 여부), 블록 동일성은 정밀 판정
	// 조기 반환 시 블록 탐색 실패 기기에서 트레이스 생략 → 전부 가능 폴백 → 인도 배치·되밀기 미동작
	UWorld* Wld = GetWorld();
	if (!Wld)
	{
		return false;
	}

	// 트레이스 채널 = StreetSurface(ECC_GameTraceChannel3). IsPlacementGrounded와 동일 출처
	const ETraceTypeQuery StreetTraceType = UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel3);

	// 깎인 모서리는 실형상 바깥 → bTraceComplex=true로 진짜 모양을 따라 판정
	constexpr bool bTraceComplex = true;

	AActor* BlockActor = CachedBlockActor.Get();

	// footprint 5점 = 중심 + 4코너(살짝 안쪽으로 0.98 곱해 경계 노이즈 회피).
	constexpr float CornerInset = 0.98f;
	const float InsetX = HalfX * CornerInset;
	const float InsetY = HalfY * CornerInset;
	const FVector2D Points[5] = {
		FVector2D(FootprintCenter.X, FootprintCenter.Y),
		FVector2D(FootprintCenter.X - InsetX, FootprintCenter.Y - InsetY),
		FVector2D(FootprintCenter.X + InsetX, FootprintCenter.Y - InsetY),
		FVector2D(FootprintCenter.X - InsetX, FootprintCenter.Y + InsetY),
		FVector2D(FootprintCenter.X + InsetX, FootprintCenter.Y + InsetY)
	};

	// 부지 자신/틴트 컴포넌트는 무시(틴트 메시가 블록 위에 떠 있어 오탐 방지). 루프 밖 1회 구성.
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(const_cast<ACityPlotActor*>(this));

	// 채널 미설정 안전장치: 5점 전부 무응답이면 마스크 불신 → 막지 않음 (콜리전 오설정으로 전면 배치 불가 회피)
	// 일부라도 응답하면 채널 정상 → 한 점이라도 블록 밖이면 무효 (깎인 모서리 걸러냄)
	bool bAnyTraceResponded = false;

	for (int32 PointIndex = 0; PointIndex < 5; ++PointIndex)
	{
		const FVector2D& Pt = Points[PointIndex];
		const FVector TraceStart(Pt.X, Pt.Y, FootprintCenter.Z + 1000.f);
		const FVector TraceEnd(Pt.X, Pt.Y, FootprintCenter.Z - 2000.f);

		TArray<FHitResult> Hits;
		const bool bAnyHit = UKismetSystemLibrary::LineTraceMulti(
			Wld, TraceStart, TraceEnd, StreetTraceType, bTraceComplex,
			ActorsToIgnore, EDrawDebugTrace::None, Hits, /*bIgnoreSelf=*/true);

		if (!bAnyHit)
		{
			// 이 점 빗나감 (깎인 모서리/바닥 없음). 채널 응답 여부는 다른 점이 결정
			OutPointsOnBlock[PointIndex] = false;
			continue;
		}

		bAnyTraceResponded = true; // 채널이 작동함(최소 한 점이 무언가에 맞음).

		// 부지 블록(BlockActor) 또는 /TheRiverwalkCity/ 메시에 맞으면 유효
		bool bPointValid = false;
		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor && BlockActor && HitActor == BlockActor)
			{
				bPointValid = true;
				break;
			}

			// 폴백 식별 — 히트 컴포넌트가 도시 지면 메시면 허용(다른 블록 타일/머지 메시도 포함).
			if (const UStaticMeshComponent* HitSMC = Cast<UStaticMeshComponent>(Hit.GetComponent()))
			{
				if (IsCityGroundMesh(HitSMC->GetStaticMesh()))
				{
					bPointValid = true;
					break;
				}
			}
		}

		if (!bPointValid)
		{
			OutPointsOnBlock[PointIndex] = false; // 블록/도시 메시 밖에 맞음 → 이 점 무효.
		}
	}

	// 채널 무응답 = 미설정 → 판정 권위 없음 (호출자가 전부 가능 폴백)
	// 폴백은 배치 제약을 통째로 없애므로 반드시 로그 (모바일 제약 소실을 눈으로만 발견한 전례). 부지당 1회
	if (!bAnyTraceResponded)
	{
		if (!bLoggedNoTraceAuthority)
		{
			bLoggedNoTraceAuthority = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[CityPlot] footprint 접지 판정 권위 없음 — StreetSurface(GameTraceChannel3) 무응답. 배치 제약이 전부 해제된다. (PlotId=%s, bBlockResolved=%d)"),
				*PlotId.ToString(), bBlockResolved ? 1 : 0);
		}

		for (bool& bPoint : OutPointsOnBlock)
		{
			bPoint = true;
		}
		return false;
	}

	return true;
}

// 무효 처리 대신 유효해지는 최소 거리만 밀어 배치 유지
bool ACityPlotActor::ResolveFootprintOntoGround(const FVector& FootprintCenter, float HalfX, float HalfY,
	float MaxPush, FVector2D& OutAdjustedXY) const
{
	OutAdjustedXY = FVector2D(FootprintCenter.X, FootprintCenter.Y);

	bool PointsOnBlock[5];
	if (!EvaluateFootprintPoints(FootprintCenter, HalfX, HalfY, PointsOnBlock))
	{
		return true; // 권위 없음 → 접지로 간주(전부 가능 폴백)
	}

	bool bAllOnBlock = true;
	for (const bool bPointOnBlock : PointsOnBlock)
	{
		bAllOnBlock &= bPointOnBlock;
	}
	if (bAllOnBlock)
	{
		return true; // 이미 접지 — 상시 드래그 경로는 여기서 끝(평가 1회, 현행과 동일 비용)
	}

	if (MaxPush <= 0.f)
	{
		return false;
	}

	// 실패 코너 부호 합의 반대 = 밀 방향. 인덱스 1..4 = (-,-)(+,-)(-,+)(+,+)
	// (오른쪽 두 코너 실패 → 합 (+2,0) → 왼쪽. 한 코너면 대각선)
	static const FVector2D CornerSigns[4] = {
		FVector2D(-1.f, -1.f), FVector2D(1.f, -1.f), FVector2D(-1.f, 1.f), FVector2D(1.f, 1.f)
	};
	FVector2D FailSum = FVector2D::ZeroVector;
	for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
	{
		if (!PointsOnBlock[CornerIndex + 1])
		{
			FailSum += CornerSigns[CornerIndex];
		}
	}

	// 합 0 = 마주보는 코너 동시 실패 (footprint > 부지 등) → 방향 불명, 보정 포기
	if (FailSum.IsNearlyZero())
	{
		return false;
	}
	const FVector2D PushDir = FailSum.GetSafeNormal() * -1.f;

	// 최대 밀기에서도 안 되면 포기(경계에서 멀리 벗어난 의도적 이동으로 본다).
	auto IsGroundedAt = [&](float Dist) -> bool
	{
		const FVector2D Candidate = OutAdjustedXY + PushDir * Dist;
		return IsFootprintOnGround(FVector(Candidate.X, Candidate.Y, FootprintCenter.Z), HalfX, HalfY);
	};
	if (!IsGroundedAt(MaxPush))
	{
		return false;
	}

	// 이분 탐색: Lo(불가)~Hi(가능) 사이 최소 이동. 6회 = MaxPush/64 ≈ 34cm (2150 기준)
	// MaxPush를 키우면 반복수도 같이 올려야 정밀도 유지 (전엔 1075/5회로 동일 34cm)
	float Lo = 0.f;
	float Hi = MaxPush;
	for (int32 Step = 0; Step < 6; ++Step)
	{
		const float Mid = (Lo + Hi) * 0.5f;
		if (IsGroundedAt(Mid))
		{
			Hi = Mid;
		}
		else
		{
			Lo = Mid;
		}
	}

	OutAdjustedXY += PushDir * Hi;
	return true;
}

void ACityPlotActor::SetOwnedState(bool bInOwned)
{
	bOwned = bInOwned;

	// 소유 시 다크 틴트 제거(밝아짐), 미소유 시 표시
	if (PlotTintSMC)
	{
		PlotTintSMC->SetVisibility(!bOwned);
	}
}

FVector ACityPlotActor::GetBubbleAnchorPosition() const
{
	// 부지 중심 위로 살짝 띄워 인수 마커가 부지 위에 뜨도록 한다(WorkstationActorBase 미러).
	constexpr float PlotMarkerZOffset = 300.0f;
	return GetActorLocation() + FVector(0.0f, 0.0f, PlotMarkerZOffset);
}

void ACityPlotActor::OnEndInteract_Implementation(APlayerController* InstigatingPC)
{
	if (!bOwned)
	{
		TryPurchase();
	}
}

void ACityPlotActor::TryPurchase()
{
	// 인수 시도 — 인접+자금 두 게이트를 통과해야 ConfirmCancel 모달, 아니면 사유 토스트.
	// 진입점 2개(미소유 부지 3D 탭 / 가격 배지 클릭)가 이 한 구현을 공유.
	if (bOwned)
	{
		return;
	}

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	UUIManagerSubsystem* UIManager = GI->GetSubsystem<UUIManagerSubsystem>();
	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>();
	USpawnManager* SpawnMgr = GetWorld() ? GetWorld()->GetSubsystem<USpawnManager>() : nullptr;
	if (!TableMgr || !ResMgr || !SpawnMgr)
	{
		return;
	}

	// 게이트 0 — 입주 회사가 있으면 전원 인수(Cleared)해야 부지 구매 가능.
	if (UCityAcquisitionManager* Acq = GetWorld()->GetSubsystem<UCityAcquisitionManager>())
	{
		if (!Acq->ArePlotOccupantsCleared(PlotId))
		{
			if (UIManager)
			{
				UIManager->ShowNotification(FText::FromString(TEXT("먼저 입주 회사를 인수해 정리하세요")), 3.0f, ENotificationType::Warning);
			}
			return;
		}
	}

	// 게이트 1 — 인접(소유 부지에 붙은 미소유)만 인수 가능.
	if (!SpawnMgr->IsPlotAdjacentBuyable(this))
	{
		if (UIManager)
		{
			UIManager->ShowNotification(
				FText::FromString(TEXT("인접한 부지만 인수할 수 있어요")), 3.0f, ENotificationType::Warning);
		}
		return;
	}

	// 게이트 2 — 자금 충분(배지가 빨강으로 미리 알려준 그 상태). MoneyPrice==0 은 항상 통과.
	bool bOk = false;
	const FCityPlotData PlotData = TableMgr->GetCityPlotData(PlotId, bOk);
	if (!bOk)
	{
		return;
	}
	if (PlotData.MoneyPrice > 0 && ResMgr->GetResourceAmount(EResourceType::Money) < PlotData.MoneyPrice)
	{
		if (UIManager)
		{
			UIManager->ShowNotification(
				NSLOCTEXT("Notification", "NotEnoughMoney", "자금이 부족합니다"), 3.0f, ENotificationType::Failed);
		}
		return;
	}

	// 두 게이트 통과 → 인수 확인 모달(확인/취소).
	OpenPurchaseModal();
}

bool ACityPlotActor::CanBePurchasedNow() const
{
	if (bOwned)
	{
		return false;
	}

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI)
	{
		return false;
	}

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>();
	USpawnManager* SpawnMgr = GetWorld() ? GetWorld()->GetSubsystem<USpawnManager>() : nullptr;
	if (!TableMgr || !ResMgr || !SpawnMgr)
	{
		return false;
	}

	// 게이트 0 — 입주 회사가 전원 Cleared 여야 통과 (TryPurchase 게이트 0 과 동일 API 재사용).
	if (UCityAcquisitionManager* Acq = GetWorld()->GetSubsystem<UCityAcquisitionManager>())
	{
		if (!Acq->ArePlotOccupantsCleared(PlotId))
		{
			return false;
		}
	}

	// 게이트 1 — 인접(소유 부지에 붙은 미소유)만 인수 가능 (TryPurchase 게이트 1과 동일 API 재사용).
	if (!SpawnMgr->IsPlotAdjacentBuyable(this))
	{
		return false;
	}

	// 게이트 2 — 자금 충분(MoneyPrice==0 은 항상 통과) (TryPurchase 게이트 2 미러링).
	bool bOk = false;
	const FCityPlotData PlotData = TableMgr->GetCityPlotData(PlotId, bOk);
	if (!bOk)
	{
		return false;
	}

	return PlotData.MoneyPrice <= 0 || ResMgr->GetResourceAmount(EResourceType::Money) >= PlotData.MoneyPrice;
}

void ACityPlotActor::OpenPurchaseModal()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UUIManagerSubsystem* UIManager = GI->GetSubsystem<UUIManagerSubsystem>();
	UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
	if (!TableMgr || !UIBase)
	{
		return;
	}

	// 중복 push 방지 — 다른 프롬프트 모달과 일관
	if (UIBase->GetPromptStackCount() > 0)
	{
		return;
	}

	bool bOk = false;
	const FCityPlotData PlotData = TableMgr->GetCityPlotData(PlotId, bOk);
	if (!bOk)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CityPlot] OpenPurchaseModal: no DT row for %s"), *PlotId.ToString());
		return;
	}

	// 공통 ConfirmCancel 다이얼로그 재사용 — 전용 모달 대신(BuildingManagePanel 슬롯 개방 확인과 동일 패턴).
	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(EWidgetType::ConfirmCancel);
	if (!Cls)
	{
		// null push 시 모달 없이 GoToUIMode 가 실행돼 입력이 잠기므로 반드시 여기서 중단 (DT_WidgetClass 행 확인)
		UE_LOG(LogTemp, Error, TEXT("[CityPlot] ConfirmCancel widget class not found! (DT_WidgetClass 행 확인)"));
		return;
	}

	UConfirmCancelWidget* Confirm = Cast<UConfirmCancelWidget>(UIBase->PushPromptClass(Cls.Get()));
	if (!Confirm)
	{
		return;
	}

	// 프롬프트 스택은 위젯 풀 재사용 — 이전 소비자(운영 종료/슬롯 개방 등)의 잔류 바인딩 해제 후 소유
	Confirm->OnConfirm.Clear();
	Confirm->OnCancel.Clear();

	Confirm->SetTitle(FText::FromString(TEXT("부지 인수")));
	Confirm->SetMessage(FText::Format(
		NSLOCTEXT("CityPlot", "PurchaseMsg", "이 부지를 {0} Money에 인수할까요?"),
		FText::AsNumber(PlotData.MoneyPrice)));
	Confirm->SetConfirmButtonText(FText::FromString(TEXT("인수")));
	Confirm->SetCancelButtonText(FText::FromString(TEXT("취소")));

	// 확인 → 결제+소유 전환. 확인/취소 둘 다 입력 모드 복원(다이얼로그는 bAutoRemove 로 자동 닫힘).
	Confirm->OnConfirm.AddUObject(this, &ACityPlotActor::HandlePurchaseConfirmed);
	Confirm->OnConfirm.AddUObject(this, &ACityPlotActor::RestoreNormalInputMode);
	Confirm->OnCancel.AddUObject(this, &ACityPlotActor::RestoreNormalInputMode);

	// UI 모드 진입 — 배경 탭이 3D 부지로 새지 않도록. 복원은 위 OnConfirm/OnCancel 델리게이트가 담당.
	if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GI->GetCurrentPlayerController()))
	{
		PC->GoToUIMode();
	}
}

void ACityPlotActor::RestoreNormalInputMode()
{
	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GI->GetCurrentPlayerController()))
	{
		// 다른 모드(BuildPlace 등)를 덮어쓰지 않도록 UI 모드일 때만 복원.
		if (PC->GetCurrentInputMode() == EInputMode::UI)
		{
			PC->GoToNormalMode();
		}
	}
}

void ACityPlotActor::HandlePurchaseConfirmed()
{
	// 모달이 이미 닫히는 중 다시 클릭 등으로 중복 호출돼도 안전하도록 소유 가드
	if (bOwned)
	{
		return;
	}

	UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance());
	if (!GI)
	{
		return;
	}

	UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
	UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>();
	UUIManagerSubsystem* UIManager = GI->GetSubsystem<UUIManagerSubsystem>();
	if (!TableMgr || !ResMgr)
	{
		return;
	}

	bool bOk = false;
	const FCityPlotData PlotData = TableMgr->GetCityPlotData(PlotId, bOk);
	if (!bOk)
	{
		return;
	}

	// MoneyPrice == 0 (시작 부지 등) 가드 — 0원 결제 토스트/소비 생략하고 바로 소유 전환
	if (PlotData.MoneyPrice > 0)
	{
		FConstructionCost Cost;
		Cost.ResourceType = EResourceType::Money;
		Cost.Cost = PlotData.MoneyPrice;

		TArray<FConstructionCost> Costs;
		Costs.Add(Cost);

		EResourceType MissingType = EResourceType::None;
		if (!ResMgr->CanAffordCosts(Costs, MissingType))
		{
			if (UIManager)
			{
				UIManager->ShowNotification(
					NSLOCTEXT("Notification", "NotEnoughMoney", "자금이 부족합니다"), 3.0f, ENotificationType::Failed);
			}
			return;
		}

		// bShouldSave=false: 소유 전환(SetOwnedState) 전에 세이브되면 새 부지가 OwnedPlotIds 에서 누락된다.
		// 차감→소유전환→단일 세이브 순서로 원자성 확보(중간 크래시 시 돈만 빠지는 창 제거).
		ResMgr->SpendResource(EResourceType::Money, PlotData.MoneyPrice, /*bShouldSave=*/false);
	}

	// 소유 전환 — 세이브 직렬화(OwnedPlotIds)는 SpawnedPlots.IsOwned() 순회를 단일 소스로 함
	SetOwnedState(true);

	if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
	{
		SaveMgr->SaveGameData();
	}

	// 미션판 G11 — 결제가 실제로 끝난 이 지점에서만 신호(모달 확인 시점은 취소 여지가 남는다)
	if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
	{
		MissionMgr->NotifyPlotAcquired();
	}

	// 인수 성공 → 가격 배지 즉시 재평가(방금 산 부지 배지 제거 + 새로 인접해진 미소유 부지에 배지 — 바깥으로 번짐).
	if (UIManager)
	{
		if (UInGameLayerWidget* InGameLayer = UIManager->GetInGameLayer())
		{
			InGameLayer->RefreshPlotPriceBadges();
		}
	}

	if (UIManager)
	{
		UIManager->ShowNotification(
			FText::FromString(TEXT("부지를 인수했습니다")), 3.0f, ENotificationType::Success);
	}
}
