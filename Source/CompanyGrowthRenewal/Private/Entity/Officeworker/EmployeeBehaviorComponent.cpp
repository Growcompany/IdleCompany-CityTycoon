#include "Entity/Officeworker/EmployeeBehaviorComponent.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/EmployeeManager.h"
#include "Office/WorkstationActorBase.h"
#include "Office/OfficeInterior.h"
#include "GameMode/OfficeGameMode.h"
#include "Core/CGGameInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Table/MoneyVFXTable.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Enum/ResourceType.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Enum/BuildingTraitTarget.h"
#include "Data/FatigueConfig.h"
#include "Data/EmployeeTypes.h"
#include "Data/EmployeeStatsData.h"
#include "Data/EmployeePotentialData.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"

UEmployeeBehaviorComponent::UEmployeeBehaviorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// [Perf] 직원당 풀프레임 틱 불필요 — 수입/피로는 DeltaTime 누적, 상태 전이는 초 단위 임계. 10Hz로 충분(캐치 반응성 유지).
	PrimaryComponentTick.TickInterval = 0.1f;

	// DA_FatigueConfig (피로도 전역 튜닝, 하드참조 → 자동 쿠킹). 에셋 미존재 시 null → 모든 사용처 가드됨.
	static ConstructorHelpers::FObjectFinder<UFatigueConfig> CfgFinder(
		TEXT("/Game/CompanyGrowth/Data/DA_FatigueConfig.DA_FatigueConfig"));
	if (CfgFinder.Succeeded())
	{
		FatigueConfig = CfgFinder.Object;
	}
}

void UEmployeeBehaviorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bSkipAutoInit) return;

	// 초기 모드에 따른 상태 설정 (약간의 딜레이 후 실행하여 다른 컴포넌트 초기화 대기)
	if (UWorld* World = GetWorld())
	{
		FTimerHandle InitTimerHandle;
		World->GetTimerManager().SetTimer(
			InitTimerHandle,
			[this]()
			{
				ApplyMoveSpeed();

				// 현재 BehaviorMode에 맞게 초기 상태 설정
				switch (CurrentBehaviorMode)
				{
				case EEmployeeBehaviorMode::Idle:
					// Idle 모드: 바로 배회 시작
					StartIdleWander();
					break;

				case EEmployeeBehaviorMode::Stage:
					// Stage 모드: Workstation에서 업무. 초기 피로 랜덤 = 직원별 슬랙 타이밍 desync(Operation 과 동일).
					Fatigue = FMath::RandRange(0.0f, 30.0f);
					SetState(EEmployeeState::Typing);
					bIsSeated = true;
					break;

				case EEmployeeBehaviorMode::Operation:
				{
					// 스폰 즉시 착석 업무 — 자리이탈은 피로 슬랙 루프가 전담(상시 배회 없음).
					// 초기 피로 랜덤 → 직원별 슬랙 배회 타이밍 desync(동시 기립 방지).
					Fatigue = FMath::RandRange(0.0f, 30.0f);
					StartOperationSeatedWork();
					break;
				}

				default:
					StartIdleWander();
					break;
				}

				UE_LOG(LogTemp, Log, TEXT("[EmployeeBehaviorComponent] Initialized with mode: %d"), (int32)CurrentBehaviorMode);
			},
			0.1f,
			false
		);
	}
}

void UEmployeeBehaviorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 일시 버프 시간 감소 + 만료 처리
	TickBuffs(DeltaTime);

	// 피로도 갱신 — 기존 모드 로직과 직교(상태 기반 누적/회복). 모드 무관 매프레임.
	TickFatigue(DeltaTime);

	// 폭주 굴림 — 피로 사슬과 독립(체력=조는 것 / 침착성=뛰쳐나감). 슬랙 진입 판정 전에 굴려야 같은 프레임에 반영된다.
	TickBoltRoll(DeltaTime);

	// 피로 슬랙 루프가 워커를 점유 중이면(텔레그래프/배회) 일반 사이클 스킵
	if (TickFatigueSlack(DeltaTime))
	{
		return;
	}

	// BehaviorMode에 따른 처리
	switch (CurrentBehaviorMode)
	{
	case EEmployeeBehaviorMode::Idle:
		// Idle 모드: Wander 상태 또는 전환 상태 처리
		switch (EmployeeState)
		{
		case EEmployeeState::Typing:
			TickWorking(DeltaTime);
			break;
		case EEmployeeState::Sitting:
			TickRest(DeltaTime);
			break;
		case EEmployeeState::Wander:
			TickWander(DeltaTime);
			break;
		case EEmployeeState::TypeToSit:
		case EEmployeeState::SitToStand:
		case EEmployeeState::StandToSit:
			// 전환 상태: 타이머로 처리되므로 틱에서는 아무것도 하지 않음
			break;
		default:
			break;
		}
		break;

	case EEmployeeBehaviorMode::Stage:
		// Stage 모드: Working 상태 유지 (업무만)
		if (EmployeeState == EEmployeeState::Typing)
		{
			TickWorking(DeltaTime);
		}
		break;

	case EEmployeeBehaviorMode::Operation:
		// Operation 모드: 상태에 따른 처리 (배회와 업무 반복)
		switch (EmployeeState)
		{
		case EEmployeeState::Typing:
			TickWorking(DeltaTime);
			break;
		case EEmployeeState::Sitting:
			TickRest(DeltaTime);
			break;
		case EEmployeeState::Wander:
			TickWander(DeltaTime);
			break;
		case EEmployeeState::StandToSit:
		case EEmployeeState::SitToStand:
			// 전환 상태: 타이머로 처리되므로 틱에서는 아무것도 하지 않음
			break;
		default:
			break;
		}

		break;

	default:
		break;
	}
}

//========================================
// 피로도 (Part A — 살아있는 사무실)
//========================================

EFatigueBand UEmployeeBehaviorComponent::GetFatigueBand() const
{
	const float TiredT = FatigueConfig ? FatigueConfig->TiredThreshold : 40.0f;
	const float SlackT = FatigueConfig ? FatigueConfig->SlackingThreshold : 80.0f;
	if (Fatigue >= SlackT) return EFatigueBand::Slacking;
	if (Fatigue >= TiredT) return EFatigueBand::Tired;
	return EFatigueBand::Energetic;
}

void UEmployeeBehaviorComponent::TickFatigue(float DeltaTime)
{
	if (!FatigueConfig) return;

	// 화면 밖 워커는 동결 — 숨은 변동/성능 둘 다 방지(보일 때만 게이지가 움직임).
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->WasRecentlyRendered(0.5f)) return;

	// 결과 모달(LaunchPending/ReportPending) 중엔 동결 — 플레이어가 못 만지는 구간.
	if (bFatigueFrozenByLifecycle) return;

	// 복지 특성 — 회복 속도만 올린다. 누적(GainPerSec)은 건드리지 않는다: 피로 자체가 게임 압력이라
	// 누적을 깎으면 크런치 긴장이 사라진다.
	float WelfareMult = 1.0f;
	if (UCGGameInstance* WelfareGI = UCGGameInstance::GetInstance())
	{
		if (UBuildingTraitManagerSubsystem* WelfareTraitMgr = WelfareGI->GetSubsystem<UBuildingTraitManagerSubsystem>())
		{
			WelfareMult = WelfareTraitMgr->GetTraitFactor(WelfareGI->GetCurrentManagedBuildingIndex(), EBuildingTraitTarget::FatigueRecovery);
		}
	}

	// 피로 = "개발 크런치" 스탯. 개발(Stage) 중에만 쌓이고, 방치/운영에선 회복만.
	// 회복이 빠르면 판 사이에 리셋돼 "무리하게 연타" 자체가 성립 못 하므로 의도적으로 느림.
	if (CurrentBehaviorMode != EEmployeeBehaviorMode::Stage)
	{
		Fatigue = FMath::Clamp(Fatigue - FatigueConfig->SelfRestRecoverPerSec * WelfareMult * DeltaTime, 0.0f, 100.0f);
		return;
	}

	switch (EmployeeState)
	{
	case EEmployeeState::Typing:
	{
		// [Perf] Stamina 조회(O(N) 직원검색)는 실제 쓰는 Typing에서만.
		// 체력이 낮을수록 급격히 쌓임 → 약골은 한 판 안에서도 초반부터 꾸벅(체력 강화가 해답).
		const float Stamina = static_cast<float>(GetOwnerStamina());
		const float GainScale = 1.0f / (1.0f + Stamina * FatigueConfig->StaminaGainScale);
		Fatigue += FatigueConfig->GainPerSec * GainScale * DeltaTime;
		break;
	}
	case EEmployeeState::Sitting:
		// 늘어짐(캐치 실패) 중엔 강제 휴식으로 빠르게 회복. 텔레그래프(캐치 윈도우) 중엔 홀드.
		if (FatigueSlackPhase == EFatigueSlackPhase::Slumping)
		{
			Fatigue -= FatigueConfig->SlumpRecoverPerSec * WelfareMult * DeltaTime;
		}
		break;
	case EEmployeeState::Wander:
		// 번아웃 배회(자리 이탈)도 강제 휴식 — 늘어짐과 동일 회복 → 복귀 시 곧바로 다시 꾸벅하는 것 방지.
		if (FatigueSlackPhase == EFatigueSlackPhase::Bolting)
		{
			Fatigue -= FatigueConfig->SlumpRecoverPerSec * WelfareMult * DeltaTime;
		}
		break;
	default:
		break;  // 전이/연출 상태는 변화 없음
	}
	Fatigue = FMath::Clamp(Fatigue, 0.0f, 100.0f);
}

void UEmployeeBehaviorComponent::ApplyTapRelief()
{
	const float Relief = FatigueConfig ? FatigueConfig->TapRelief : 40.0f;
	Fatigue = FMath::Max(0.0f, Fatigue - Relief);

	// 다운된 동안이면 어느 페이즈든 즉시 업무 복귀 — 손해는 "늦게 눈치챈 시간"만큼만 남는다.
	// 늘어짐 중엔 피로가 계속 빠지고 있어 임계값 비교로는 탭 여부를 구분할 수 없으므로 여기서 명시적으로 끝낸다.
	if (FatigueSlackPhase != EFatigueSlackPhase::None)
	{
		ExitSlackToWork();
	}
}

float UEmployeeBehaviorComponent::GetFatigueOutputFactor() const
{
	if (!FatigueConfig) return 1.0f;
	const float Tired = FatigueConfig->TiredThreshold;
	const float T = FMath::Clamp((Fatigue - Tired) / FMath::Max(1.0f, 100.0f - Tired), 0.0f, 1.0f);
	return 1.0f - FatigueConfig->MaxFatiguePenalty * T;
}

//========================================
// 피로 슬랙 루프 (Slacking → 꾸벅(캐치 윈도우) → 자리에서 늘어짐 → 복귀)
//========================================

bool UEmployeeBehaviorComponent::TickFatigueSlack(float DeltaTime)
{
	if (!FatigueConfig) return false;

	// 꾸벅/늘어짐은 개발(Stage) 중에만 — 방치/운영 중엔 관여하지 않음.
	// 모드가 바뀌며 빠져나갈 때 잔여 페이즈가 남으면 워커를 영구 점유하므로 반드시 초기화.
	if (CurrentBehaviorMode != EEmployeeBehaviorMode::Stage)
	{
		if (FatigueSlackPhase != EFatigueSlackPhase::None)
		{
			FatigueSlackPhase = EFatigueSlackPhase::None;
			SlackTelegraphTimer = 0.0f;
			SlackSlumpTimer = 0.0f;
			SlackBoltTimer = 0.0f;
			// 이 경로는 ExitSlackToWork 를 안 타므로 직접 원복 — 빠뜨리면 MaxWalkSpeed 가 Bolt 값에 고착된다
			ApplyMoveSpeed();
			// 같은 이유로 알림도 여기서 걷는다. 다운 중 개발 판이 끝나는 건 예외가 아니라 흔한 종료 경로라,
			// 빠뜨리면 판이 끝났는데 "뛰쳐나갔습니다"가 레일에 남는다.
			NotifyRailRecovered();
		}
		return false;
	}

	AOfficeworker* Owner = Cast<AOfficeworker>(GetOwner());
	// 데스크 배정된(일하는) 워커만 대상 — Idle 배회 워커 제외
	if (!Owner || !Owner->GetAssignedWorkstation()) return false;

	// 화면 밖에선 새 꾸벅을 "시작"만 막는다(안 보이는 곳에서 페널티가 생기면 캐치 기회가 없으므로).
	// 이미 진행 중인 페이즈는 절대 되돌리지 않고 그대로 진행시킨다 — 여기서 ExitSlackToWork 로 되돌리면
	// WasRecentlyRendered 가 한 프레임만 false 여도 텔레그래프 타이머가 0 으로 리셋되고,
	// 피로가 그대로라 다음 틱에 재진입하는 핑퐁이 생긴다. 그 결과 0.15s 스로틀인 캐치 스캔이 링을 거의 못 잡고,
	// Sitting 재진입마다 SeatedPoseIndex 가 재추첨되어 앉은 자세가 툭툭 튄다.
	// 진행만 시켜두면 텔레그래프→늘어짐→복귀가 몇 초 안에 스스로 끝나 기여도 다시 흐른다.
	if (!Owner->WasRecentlyRendered(0.5f) && FatigueSlackPhase == EFatigueSlackPhase::None)
	{
		return false;
	}

	// 결과 모달 중엔 페이즈를 그대로 홀드 — 모달이 닫히면 모드 전환이 페이즈를 초기화하므로 고착되지 않는다.
	if (bFatigueFrozenByLifecycle) return false;

	const float SlackT = FatigueConfig->SlackingThreshold;

	switch (FatigueSlackPhase)
	{
	case EFatigueSlackPhase::None:
		// 자리에서 일/휴식 중 Slacking 도달 → 늘어짐(텔레그래프) 진입
		if (Fatigue >= SlackT &&
			(EmployeeState == EEmployeeState::Typing || EmployeeState == EEmployeeState::Sitting))
		{
			EnterSlackTelegraph();
			return true;
		}
		return false;

	case EFatigueSlackPhase::Telegraph:
		// 클릭("확 깨움")으로 피로가 떨어지면 즉시 복귀
		if (Fatigue < SlackT)
		{
			ExitSlackToWork();
			return true;
		}
		SlackTelegraphTimer += DeltaTime;
		if (SlackTelegraphTimer >= FatigueConfig->SlackTelegraphDuration)
		{
			EnterSlackSlump();  // 못 잡음 → 자리에서 늘어짐(기여 0)
		}
		return true;

	case EFatigueSlackPhase::Slumping:
		// 자리에서 고정 시간 늘어짐 — 그동안 기여 0, 피로는 TickFatigue 가 빠르게 회복.
		// 시간 기준으로 끝내야 손해가 "정확히 N초"로 예측 가능(피로 기준이면 체력 편차로 들쭉날쭉).
		SlackSlumpTimer += DeltaTime;
		if (SlackSlumpTimer >= FatigueConfig->SlackSlumpDuration)
		{
			// 늘어짐은 조용히 복귀로만 끝난다 — 폭주는 피로 사슬에서 분리돼 TickBoltRoll 이 독립 소유
			ExitSlackToWork();
		}
		return true;

	case EFatigueSlackPhase::Bolting:
		// 자리 이탈해 배회(움직이는 캐치 타깃). 고정 시간 뒤 스스로 복귀 — 손해 상한 유지(만회 불가 방지).
		SlackBoltTimer += DeltaTime;
		if (SlackBoltTimer >= FatigueConfig->BoltDuration)
		{
			ExitSlackToWork();
		}
		return true;
	}
	return false;
}

void UEmployeeBehaviorComponent::EnterSlackTelegraph()
{
	FatigueSlackPhase = EFatigueSlackPhase::Telegraph;
	SlackTelegraphTimer = 0.0f;

	StopFloatingTextTimer();

	// 사유는 여기서 한 번만 뽑는다 — 자세와 알림 문구가 갈라지면 "졸고 있습니다"인데 태평하게 앉아 있는 불일치가 난다.
	SlackReason = (FMath::FRand() < 0.5f) ? EWorkerDownReason::Drowsy : EWorkerDownReason::Lazy;

	SetState(EEmployeeState::Sitting);

	// ⚠ 순서 의존 — SetState(Sitting) 이 밴드로 자세를 재추첨하므로 반드시 그 뒤에 덮어쓴다
	SeatedPoseIndex = (SlackReason == EWorkerDownReason::Drowsy) ? 2 : 1;

	// 다운 진입 = 상태 토스트 1회. 진입 시에만이라 스팸 없음.
	NotifyRailDown(SlackReason);
}

void UEmployeeBehaviorComponent::EnterSlackSlump()
{
	FatigueSlackPhase = EFatigueSlackPhase::Slumping;
	SlackSlumpTimer = 0.0f;

	// 자리를 뜨지 않는다 — 개발 판이 짧아 이탈 배회는 판보다 긴 손해가 되어 만회가 불가능해짐.
	// 기여는 EnterSlackTelegraph 의 StopFloatingTextTimer 로 이미 중단된 상태이므로 여기선 자세만 유지.
	SetState(EEmployeeState::Sitting);
}

void UEmployeeBehaviorComponent::EnterSlackBolt()
{
	FatigueSlackPhase = EFatigueSlackPhase::Bolting;
	SlackBoltTimer = 0.0f;

	AOfficeworker* Owner = Cast<AOfficeworker>(GetOwner());
	if (!Owner) return;

	// 자리를 박차고 일어나 배회 — 움직이는 캐치 타깃(링이 매 틱 위치를 추적).
	Owner->StandUpFromWorkstation();
	bIsSeated = false;

	// StandUpFromWorkstation 은 위치를 안 옮겨 워커가 좌석(NavMesh 밖)에 남는다 →
	// StartRandomRoaming 의 도달점 탐색이 실패해 제자리 정지. 가장 가까운 NavMesh 지점으로 텔레포트 후 배회.
	if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
	{
		const FVector CurrentLocation = Owner->GetActorLocation();
		FNavLocation NavLocation;
		if (NavSys->ProjectPointToNavigation(CurrentLocation, NavLocation, FVector(1000.f, 1000.f, 1000.f)))
		{
			FVector TeleportLocation = NavLocation.Location;
			TeleportLocation.Z = CurrentLocation.Z;
			Owner->SetActorLocation(TeleportLocation);
		}
	}

	SetState(EEmployeeState::Wander);

	// 뛰는 상황은 여기뿐 — 이동 시작 전에 올려야 첫 프레임부터 러닝 모션이 나온다
	ApplyMoveSpeed();

	// 배회 반경 = 오피스 바닥 대각선. RoamOrigin 이 방금 박차고 나온 자리라, 대각선이면 구석 자리에서
	// 뛰쳐나가도 바닥 전체가 후보가 된다. 증축 시 UpdateNavMeshBounds 가 갱신하므로 값이 자동으로 따라온다.
	// 반경만 키우고 GetRandomPoint 로 갈아타지 않는 이유 = 도달성 제한이 살아 있어야
	// 캡처 스테이지처럼 멀리 떨어진 별개 NavMesh 섬으로 뛰어가지 않는다.
	float BoltRoamRadius = 800.f;
	if (AOfficeGameMode* OfficeGM = GetWorld() ? GetWorld()->GetAuthGameMode<AOfficeGameMode>() : nullptr)
	{
		if (const AOfficeInterior* Interior = OfficeGM->GetOfficeInterior())
		{
			const FVector BoundsSize = Interior->GetNavMeshBoundsBox().GetSize();
			BoltRoamRadius = FMath::Max(BoltRoamRadius, FVector2D(BoundsSize.X, BoundsSize.Y).Size());
		}
	}

	// 도착 후 대기를 거의 0 으로 — 기본값(2~5초)이면 8초 폭주의 절반 이상을 서서 보낸다(구간당 이동은 1~2초뿐).
	// 0 은 쓰지 않는다: SetTimer 는 Rate<=0 이면 타이머를 거는 대신 지워서 Tick 폴링에 의존하게 된다.
	Owner->StartRandomRoaming(BoltRoamRadius, 0.05f, 0.35f);

	// 움직이는 캐치 타깃이라 화면에서 놓치기 쉽다 — 레일 알림이 "누가 어디로 갔는지"의 유일한 단서
	SlackReason = EWorkerDownReason::Bolted;
	NotifyRailDown(SlackReason);
}

void UEmployeeBehaviorComponent::ExitSlackToWork()
{
	FatigueSlackPhase = EFatigueSlackPhase::None;
	SlackTelegraphTimer = 0.0f;
	SlackSlumpTimer = 0.0f;
	SlackBoltTimer = 0.0f;
	ApplyMoveSpeed();

	// 탭 캐치·텔레그래프 회복·늘어짐/폭주 타임아웃이 전부 여기로 모이므로 해제는 이 한 줄이면 된다.
	// 아래 Owner 널 가드보다 앞에 둬야 워커가 사라진 경우에도 알림이 남지 않는다.
	NotifyRailRecovered();

	AOfficeworker* Owner = Cast<AOfficeworker>(GetOwner());
	if (!Owner) return;

	// 이제 슬랙은 자리를 안 뜨지만, 모드 전환 등으로 서 있는 잔여 상태가 있을 수 있어 착석 보정.
	Owner->StopRandomRoaming();
	if (!bIsSeated)
	{
		Owner->TeleportToWorkstationAndSit();
		bIsSeated = true;
	}
	SetState(EEmployeeState::Typing);
	StartFloatingTextTimer();
}

void UEmployeeBehaviorComponent::NotifyRailDown(EWorkerDownReason Reason)
{
	AOfficeworker* OwnerWorker = Cast<AOfficeworker>(GetOwner());
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!OwnerWorker || !EmpMgr) return;

	// 알림 수명 = 실제로 손댈 수 있는 구간. 해제 신호를 놓쳐도(워커 디스폰/레벨 전환) 스스로 만료되는 폴백이다.
	// 꾸벅/딴짓은 텔레그래프+늘어짐 내내 탭이 먹으므로 두 구간의 합.
	float Window = 3.0f;
	if (FatigueConfig)
	{
		Window = (Reason == EWorkerDownReason::Bolted)
			? FatigueConfig->BoltDuration
			: FatigueConfig->SlackTelegraphDuration + FatigueConfig->SlackSlumpDuration;
	}

	EmpMgr->NotifyEmployeeDown(OwnerWorker->GetEmployeeID(), Reason, Window);
}

void UEmployeeBehaviorComponent::NotifyRailRecovered()
{
	AOfficeworker* OwnerWorker = Cast<AOfficeworker>(GetOwner());
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	if (!OwnerWorker || !EmpMgr) return;

	EmpMgr->NotifyEmployeeRecovered(OwnerWorker->GetEmployeeID());
}

void UEmployeeBehaviorComponent::SetState(EEmployeeState NewState)
{
	if (EmployeeState == NewState)
	{
		return;
	}

	EEmployeeState OldState = EmployeeState;
	EmployeeState = NewState;

	// 피로 밴드 → 앉은 자세 변주(ABP Blend Poses by Int). Sitting 진입 시 1회 결정(매틱 지터 방지).
	if (NewState == EEmployeeState::Sitting)
	{
		switch (GetFatigueBand())
		{
		case EFatigueBand::Slacking: SeatedPoseIndex = (FMath::FRand() < 0.5f) ? 2 : 1; break; // 꾸벅 or 태평
		case EFatigueBand::Tired:    SeatedPoseIndex = (FMath::FRand() < 0.5f) ? 1 : 0; break; // 가끔 태평
		default:                     SeatedPoseIndex = 0; break;                                // 멀쩡
		}
	}

	// 새 상태 진입 = 새 사이클 시작 — 외부 경로(Wander/이벤트)로 이전 상태에 누적된 시간이
	// 새 상태에 그대로 이어지면 즉시 전환되는 버그가 생김. 진입 시점에 항상 reset.
	CurrentStateTime = 0.0f;

	// 디버그: 상태 전환 로그 (항상 출력)
	AOfficeworker* Owner = Cast<AOfficeworker>(GetOwner());
	int32 EmpID = Owner ? Owner->GetEmployeeID() : -1;
	UE_LOG(LogTemp, Warning, TEXT("[Employee %d] State: %d -> %d (Mode: %d)"),
		EmpID, (int32)OldState, (int32)NewState, (int32)CurrentBehaviorMode);

	// 상태에 따른 앉음/서있음 설정
	switch (NewState)
	{
	case EEmployeeState::Typing:
	case EEmployeeState::Sitting:
	case EEmployeeState::CheerSitting:
		bIsSeated = true;
		Walking = false;
		Speed = 0.0f;
		break;

	case EEmployeeState::Wander:
		bIsSeated = false;
		Walking = true;
		Speed = 1.0f;
		break;

	case EEmployeeState::CheerStandUp:
		bIsSeated = false;
		break;

	case EEmployeeState::Greeting:
		bIsSeated = false;
		Walking = false;
		Speed = 0.0f;
		break;

	case EEmployeeState::Dance:
		bIsSeated = false;
		Walking = false;
		Speed = 0.0f;
		break;

	case EEmployeeState::TypeToSit:
		// Typing에서 앉기로 전환: 앉은 상태 유지
		bIsSeated = true;
		Walking = false;
		Speed = 0.0f;
		break;

	case EEmployeeState::SitToStand:
		// 앉은 상태에서 일어나는 중: 전환 중이므로 bIsSeated 상태는 애니메이션에서 처리
		// 일어나는 애니메이션 시작 시 앉음 → 완료 시 서있음
		Walking = false;
		Speed = 0.0f;
		break;

	case EEmployeeState::StandToSit:
		// 서있는 상태에서 앉는 중: 전환 중이므로 bIsSeated 상태는 애니메이션에서 처리
		Walking = false;
		Speed = 0.0f;
		break;

	default:
		break;
	}

	// 인사(Greeting) 클립엔 완료 노티파이가 없어 폴백 타이머가 종료 트리거가 된다.
	// (서기/앉기는 AnimBP 가 EmployeeState 로 직접 전이하므로 중간 홀드/타이머 불필요)
	if (NewState == EEmployeeState::Greeting)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(TransitionFallbackTimerHandle);
			World->GetTimerManager().SetTimer(
				TransitionFallbackTimerHandle,
				this,
				&UEmployeeBehaviorComponent::OnTransitionFallbackTimeout,
				3.0f,
				false
			);
		}
	}

	// 델리게이트 브로드캐스트
	OnStateChanged.Broadcast(OldState, NewState);
}

void UEmployeeBehaviorComponent::SetOperationIncome(float IncomePerSecond)
{
	OperationIncomePerSecond = IncomePerSecond;
}

void UEmployeeBehaviorComponent::StartWander()
{
	if (EmployeeState == EEmployeeState::Wander)
	{
		return;
	}

	// 배회 목적지 랜덤 선택 (확률에 따라)
	float Rand = FMath::FRand();
	if (Rand < 0.5f)
	{
		CurrentWanderDestination = EWanderDestination::RandomPoint;
	}
	else if (Rand < 0.8f)
	{
		CurrentWanderDestination = EWanderDestination::NearCoworker;
	}
	else if (Rand < 0.95f)
	{
		CurrentWanderDestination = EWanderDestination::WindowArea;
	}
	else
	{
		CurrentWanderDestination = EWanderDestination::CenterArea;
	}

	SetState(EEmployeeState::Wander);
	Walking = true;
	Speed = 1.0f;  // 걷기 속도
}

void UEmployeeBehaviorComponent::ReturnToWorkstation()
{
	// 복귀 시 빠른 이동
	Walking = true;
	Speed = 6.0f;  // 달리기 속도

	// 실제 이동 완료 후 외부에서 SetState(Working/Rest) 호출
}

void UEmployeeBehaviorComponent::TickWorking(float DeltaTime)
{
	// 수익 발생 (모드 공통)
	GenerateIncome(DeltaTime);

	// Stage 는 연속 타이핑만 유지 — Stage 틱은 Typing 외 상태를 복귀시키지 않아, 여기서 Sitting 으로
	// 넘기면 복귀 경로가 없어 영구 정지한다. Stage 의 자리이탈은 피로 슬랙 루프가 전담.
	if (CurrentBehaviorMode == EEmployeeBehaviorMode::Stage)
	{
		return;
	}

	// Idle/Operation: Stamina 기반 Work 사이클 — WorkDuration 초과 시 Sitting(휴식)으로 전환(복귀는 TickRest)
	CurrentStateTime += DeltaTime;
	if (CurrentStateTime >= GetWorkDuration())
	{
		CurrentStateTime = 0.0f;
		StopFloatingTextTimer();
		SetState(EEmployeeState::Sitting);
		return;
	}
}

void UEmployeeBehaviorComponent::TickRest(float DeltaTime)
{
	// Stamina 기반 Rest 사이클 — 누적 시간이 GetRestDuration() 초과하면 업무 복귀
	CurrentStateTime += DeltaTime;

	if (CurrentStateTime >= GetRestDuration())
	{
		CurrentStateTime = 0.0f;
		SetState(EEmployeeState::Typing);
		StartFloatingTextTimer();
	}
}

void UEmployeeBehaviorComponent::TickWander(float DeltaTime)
{
	// 배회 중 수익 발생 (50%)
	GenerateIncome(DeltaTime);
}

float UEmployeeBehaviorComponent::GetWorkDuration() const
{
	// 체력 높은 직원은 더 오래 일함 — 기본 30s × (1 + Stamina × 계수). ★15 평균(35) → 90s
	const int32 Stamina = GetOwnerStamina();
	return BaseWorkDuration * (1.0f + FMath::Max(0, Stamina) * EmployeeStatTuning::WorkDurationStaminaScale);
}

float UEmployeeBehaviorComponent::GetRestDuration() const
{
	// 체력 높은 직원은 짧게 쉼 — 기본 8s / (1 + Stamina × 계수). ★15 평균(35) → 4s
	const int32 Stamina = GetOwnerStamina();
	return BaseRestDuration / (1.0f + FMath::Max(0, Stamina) * EmployeeStatTuning::RestDurationStaminaScale);
}

int32 UEmployeeBehaviorComponent::GetOwnerStamina() const
{
	AOfficeworker* OwnerWorker = Cast<AOfficeworker>(GetOwner());
	if (!OwnerWorker) return 0;

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GI) return 0;

	UEmployeeManager* EmpMgr = GI->GetSubsystem<UEmployeeManager>();
	if (!EmpMgr) return 0;

	// 강화(★) 이득은 스탯 파생 경로 단일 원칙 — 저장값만 반환하면 강화가 피로/근무·휴식 지속에 반영 안 됨.
	FEmployeeInstance* Employee = EmpMgr->FindEmployee(OwnerWorker->GetEmployeeID());
	return Employee ? Employee->Stats.Stamina + UEmployeeTypeHelper::GetEnhanceStatBonus(Employee->EnhancementLevel) : 0;
}

float UEmployeeBehaviorComponent::GetEffectiveBoltChance() const
{
	if (!FatigueConfig) { return 0.0f; }

	AOfficeworker* OwnerWorker = Cast<AOfficeworker>(GetOwner());
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UEmployeeManager* EmpMgr = GI ? GI->GetSubsystem<UEmployeeManager>() : nullptr;
	FEmployeeInstance* Employee = (OwnerWorker && EmpMgr) ? EmpMgr->FindEmployee(OwnerWorker->GetEmployeeID()) : nullptr;
	// 조회 실패는 데이터 이상 — 감쇠 없는 최대 확률을 반환하면 폴백이 "더 난장판" 방향이 된다
	if (!Employee) { return 0.0f; }

	const int32 Effective = Employee->Stats.Composure + UEmployeeTypeHelper::GetEnhanceStatBonus(Employee->EnhancementLevel);
	// 포화곡선 — 구 Min(V×Scale, Cap) 은 캡 도달 순간 다음 포인트가 0원이 되는 절벽이었다.
	// ★ 밴드로 유효값이 140까지 가면서 캡(30pt)을 ★4 에 넘겨 이후 11별이 침착성에 아무것도 안 주게 됨.
	// V/(V+K) 는 절대 캡에 안 닿지만 계속 오른다. K = 반효과 지점(=구 캡 도달점) 이라 초반 기울기는 동일.
	const float HalfPoint = FMath::Max(1.0f, FatigueConfig->MaxBoltReduction / FMath::Max(KINDA_SMALL_NUMBER, FatigueConfig->ComposureBoltScale));
	const float Reduction = FatigueConfig->MaxBoltReduction * (Effective / (Effective + HalfPoint));
	return FatigueConfig->BoltChancePerSec * (1.0f - Reduction);
}

void UEmployeeBehaviorComponent::TickBoltRoll(float DeltaTime)
{
	if (!FatigueConfig) { return; }

	// 개발(Stage) 전용 — 피로와 같은 이유로, 캐치할 플레이어가 화면에 있을 때만 성립하는 사건이다
	if (CurrentBehaviorMode != EEmployeeBehaviorMode::Stage) { BoltRollTimer = 0.0f; return; }

	// 이미 슬랙 페이즈(꾸벅/늘어짐/배회) 점유 중이면 굴리지 않는다 — 늘어짐 중 폭주 재진입 방지
	if (FatigueSlackPhase != EFatigueSlackPhase::None) { return; }

	if (bFatigueFrozenByLifecycle) { return; }

	AOfficeworker* OwnerWorker = Cast<AOfficeworker>(GetOwner());
	if (!OwnerWorker || !OwnerWorker->WasRecentlyRendered(0.5f)) { return; }

	// 책상 배정된 워커만 — 없으면 EnterSlackBolt 의 StandUpFromWorkstation 이 early-return 하는데
	// 배회는 그대로 진행돼, 복귀 시 bIsSeated=true 로 거짓 표시하며 서 있는 채로 기여를 시작한다.
	if (!OwnerWorker->GetAssignedWorkstation()) { return; }

	// 자리에서 일/휴식 중일 때만 — 전이·연출 상태(MovingToDesk 등)에서 굴리면 상태머신을 가로챈다
	if (EmployeeState != EEmployeeState::Typing && EmployeeState != EEmployeeState::Sitting) { return; }

	// 1초 누산 — 매 프레임 굴리면 발생률이 프레임레이트에 따라 달라진다
	BoltRollTimer += DeltaTime;
	if (BoltRollTimer < 1.0f) { return; }
	BoltRollTimer -= 1.0f;

	if (FMath::FRand() < GetEffectiveBoltChance())
	{
		EnterSlackBolt();
	}
}

float UEmployeeBehaviorComponent::GetIncomeMultiplier() const
{
	switch (EmployeeState)
	{
	case EEmployeeState::Typing:
		return 1.0f;  // 100%

	case EEmployeeState::Wander:
		return 0.5f;  // 50%

	case EEmployeeState::Sitting:
	case EEmployeeState::CheerSitting:
	case EEmployeeState::CheerStandUp:
	default:
		return 0.0f;  // 0%
	}
}

float UEmployeeBehaviorComponent::GetCurrentIncomePerSecond() const
{
	// 돈은 운영 축 전용이다 — 개발(Stage)의 산출은 점수고, 무프로젝트(Idle)는 아무것도 낳지 않는다.
	// Idle 에 요율을 주면 프로젝트를 안 굴려도 지갑이 차서 착수 동기가 사라진다(2026-08-13 제거).
	// 방치 특성(EBuildingTraitTarget::IdleIncome)은 오프라인 정산(SaveLoadManager) 전용으로 남았다.
	if (CurrentBehaviorMode != EEmployeeBehaviorMode::Operation || OperationIncomePerSecond <= 0.0f)
	{
		return 0.0f;
	}

	// 프로젝트 수익 N분의1 × 직접 관리 배율. 직원 개별 능력치는 여기가 아니라
	// ProjectOperationManager::CalculateEmployeeStatBonus 가 운영 요율에 미리 반영한다.
	return OperationIncomePerSecond * OperationDirectManageMultiplier * GetIncomeMultiplier();
}

void UEmployeeBehaviorComponent::GenerateIncome(float DeltaTime)
{
	// 적립만 — 지급은 플로팅텍스트 타이머(코인 연출)가 단독으로 소유한다.
	// 요율 계산도 게터가 단독 소유라 표시(수익 칩)와 지급이 갈릴 수 없다.
	AccumulatedIncome += GetCurrentIncomePerSecond() * DeltaTime;
}

void UEmployeeBehaviorComponent::SetBehaviorMode(EEmployeeBehaviorMode NewMode)
{
	// 같은 모드로 다시 설정할 때
	if (CurrentBehaviorMode == NewMode)
	{
		// Stage 모드인데 Typing 상태가 아니면 처리 필요
		if (NewMode == EEmployeeBehaviorMode::Stage && EmployeeState != EEmployeeState::Typing)
		{
			// 슬랙 점유 상태(텔레그래프/배회)에서 무대 강제 착석 — 스테일 페이즈 리셋(영구 정지 방지)
			FatigueSlackPhase = EFatigueSlackPhase::None;
			SlackTelegraphTimer = 0.0f;
			SlackBoltTimer = 0.0f;
			ApplyMoveSpeed();  // 배회(Bolting) 중 강제 착석이면 Bolt 속도가 남으므로 원복

			AOfficeworker* Owner = Cast<AOfficeworker>(GetOwner());
			if (Owner)
			{
				Owner->StopRandomRoaming();
			}
			// 이미 앉아있으면 바로 Typing, 아니면 텔레포트 후 StandToSit
			if (EmployeeState == EEmployeeState::Sitting || EmployeeState == EEmployeeState::CheerSitting)
			{
				SetState(EEmployeeState::Typing);
				UE_LOG(LogTemp, Log, TEXT("[EmployeeBehaviorComponent] Re-entering Stage: Already sitting, start Typing"));
			}
			else
			{
				bool bTeleportSuccess = false;
				if (Owner)
				{
					bTeleportSuccess = Owner->TeleportToWorkstationAndSit();
				}
				if (bTeleportSuccess)
				{
					// 서있다가 무대 재진입 — 텔레포트 후 바로 업무(착석 포즈는 AnimBP 가 처리)
					SetState(EEmployeeState::Typing);
					UE_LOG(LogTemp, Log, TEXT("[EmployeeBehaviorComponent] Re-entering Stage: Teleport success, Typing"));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[EmployeeBehaviorComponent] Re-entering Stage: Teleport FAILED! No workstation."));
				}
			}
		}
		return;
	}

	EEmployeeBehaviorMode OldMode = CurrentBehaviorMode;
	CurrentBehaviorMode = NewMode;

	// 모드 전환은 워커 상태를 새로 셋업(강제 착석/배회)하므로 슬랙 페이즈도 초기화 —
	// 스테일 페이즈가 남으면 TickFatigueSlack 이 영구 점유해 워커가 정지(피로 100 고정)된다
	FatigueSlackPhase = EFatigueSlackPhase::None;
	SlackTelegraphTimer = 0.0f;
	SlackBoltTimer = 0.0f;
	ApplyMoveSpeed();  // 페이즈를 지웠으니 걷기 속도로 원복 (Bolting 중 모드 전환 시 450 고착 방지)

	// Stage/Operation 모드에서 벗어나면 FloatingText 타이머 중지
	if ((OldMode == EEmployeeBehaviorMode::Stage || OldMode == EEmployeeBehaviorMode::Operation) && OldMode != NewMode)
	{
		StopFloatingTextTimer();
	}

	AOfficeworker* Owner = Cast<AOfficeworker>(GetOwner());

	switch (NewMode)
	{
	case EEmployeeBehaviorMode::Idle:
		// Idle 모드: 현재 상태에 따른 전환 시퀀스 실행
		if (Owner)
		{
			Owner->StopRandomRoaming();
		}
		StartIdleTransitionSequence();
		break;

	case EEmployeeBehaviorMode::Stage:
		// Stage 모드: 현재 상태에 따라 분기
		if (Owner)
		{
			Owner->StopRandomRoaming();
		}
		// 이미 앉아있는 상태(Sitting, CheerSitting)면 바로 Typing으로
		if (EmployeeState == EEmployeeState::Sitting || EmployeeState == EEmployeeState::CheerSitting)
		{
			SetState(EEmployeeState::Typing);
			UE_LOG(LogTemp, Log, TEXT("[EmployeeBehaviorComponent] Stage mode: Already sitting, start Typing"));
		}
		else
		{
			// 서있는 상태면 텔레포트 후 StandToSit
			bool bTeleportSuccess = false;
			if (Owner)
			{
				bTeleportSuccess = Owner->TeleportToWorkstationAndSit();
			}

			if (bTeleportSuccess)
			{
				// 서있다가 무대 진입 — 텔레포트 후 바로 업무(착석 포즈는 AnimBP 가 처리)
				SetState(EEmployeeState::Typing);
				UE_LOG(LogTemp, Log, TEXT("[EmployeeBehaviorComponent] Stage mode: Teleport success, Typing"));
			}
			else
			{
				// 텔레포트 실패: Workstation이 없는 직원 - Idle 모드로 유지
				CurrentBehaviorMode = EEmployeeBehaviorMode::Idle;
				UE_LOG(LogTemp, Warning, TEXT("[EmployeeBehaviorComponent] Stage mode: Teleport FAILED! No workstation assigned. Keeping Idle mode."));
			}
		}
		// Stage 모드 진입 시 첫 타이머 플래그 설정 + FloatingText 타이머 시작
		bIsFirstStageTimer = true;
		StartFloatingTextTimer();
		break;

	case EEmployeeBehaviorMode::Operation:
		// 환호 중이면 환호 완료 후 착석 (OnCheerAnimationComplete에서 처리)
		if (EmployeeState != EEmployeeState::CheerSitting && EmployeeState != EEmployeeState::CheerStandUp)
		{
			StartOperationSeatedWork();
		}
		break;

	default:
		break;
	}

	UE_LOG(LogTemp, Log, TEXT("[EmployeeBehaviorComponent] BehaviorMode changed: %d -> %d"), (int32)OldMode, (int32)NewMode);
}

void UEmployeeBehaviorComponent::StartIdleTransitionSequence()
{
	// 앉음/서있음 사이 중간 홀드 없이 곧장 배회 진입(스냅).
	// 일어서는 포즈는 AnimBP 가 EmployeeState==Wander 로, 걷기는 bWalking 으로 직접 전이.
	StartIdleWander();
}

void UEmployeeBehaviorComponent::OnTypeToSitComplete()
{
	// 폴백 타이머 취소
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TransitionFallbackTimerHandle);
	}

	// Idle 모드가 아니면 무시
	if (CurrentBehaviorMode != EEmployeeBehaviorMode::Idle)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[EmployeeBehaviorComponent] TypeToSit complete -> SitToStand"));

	// TypeToSit 완료 → SitToStand 시작 (AnimNotify에서 OnSitToStandComplete 호출)
	SetState(EEmployeeState::SitToStand);
}

void UEmployeeBehaviorComponent::OnSitToStandComplete()
{
	// 폴백 타이머 취소
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TransitionFallbackTimerHandle);
	}

	UE_LOG(LogTemp, Warning, TEXT("[EmployeeBehaviorComponent] OnSitToStandComplete called! Mode: %d"), (int32)CurrentBehaviorMode);

	// SitToStand 완료 → bIsSeated를 false로 설정
	bIsSeated = false;

	// 모드에 따라 배회 시작
	switch (CurrentBehaviorMode)                                                  
	{
	case EEmployeeBehaviorMode::Idle:
		UE_LOG(LogTemp, Warning, TEXT("[EmployeeBehaviorComponent] Starting Idle Wander"));
		StartIdleWander();
		break;

	case EEmployeeBehaviorMode::Operation:
		StartOperationSeatedWork();
		break;

	default:
		UE_LOG(LogTemp, Warning, TEXT("[EmployeeBehaviorComponent] OnSitToStandComplete - Unknown mode, not wandering"));
		break;
	}
}

void UEmployeeBehaviorComponent::OnStandToSitComplete()
{
	// 폴백 타이머 취소
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TransitionFallbackTimerHandle);
	}

	// StandToSit 완료 → bIsSeated를 true로 설정
	bIsSeated = true;

	// 현재 모드에 따라 다음 상태 결정
	switch (CurrentBehaviorMode)
	{
	case EEmployeeBehaviorMode::Stage:
		// Stage 모드 → Working 상태로
		SetState(EEmployeeState::Typing);
		break;

	case EEmployeeBehaviorMode::Operation:
		// 자리에 앉음 → 착석 업무 재개 (상시 배회 타이머 무장 없음)
		SetState(EEmployeeState::Typing);
		StartFloatingTextTimer();
		break;

	default:
		// 기타 → Rest 상태로
		SetState(EEmployeeState::Sitting);
		break;
	}
}

void UEmployeeBehaviorComponent::OnCheerAnimationComplete()
{
	UE_LOG(LogTemp, Log, TEXT("[EmployeeBehaviorComponent] CheerAnimation complete (Mode: %d, State: %d)"),
		(int32)CurrentBehaviorMode, (int32)EmployeeState);

	if (EmployeeState == EEmployeeState::CheerSitting)
	{
		if (CurrentBehaviorMode == EEmployeeBehaviorMode::Stage)
		{
			SetState(EEmployeeState::Sitting);
		}
		else if (CurrentBehaviorMode == EEmployeeBehaviorMode::Operation)
		{
			// 앉아서 환호 완료 → 자리 착석 업무 복귀
			StartOperationSeatedWork();
		}
		else if (CurrentBehaviorMode == EEmployeeBehaviorMode::Idle)
		{
			StartIdleWander();
		}
	}
	else if (EmployeeState == EEmployeeState::CheerStandUp)
	{
		// 방어 코드: CheerStandUp이 호출된 경우에도 처리
		if (CurrentBehaviorMode == EEmployeeBehaviorMode::Operation)
		{
			StartOperationSeatedWork();
		}
		else if (CurrentBehaviorMode == EEmployeeBehaviorMode::Idle)
		{
			StartIdleWander();
		}
	}
}

void UEmployeeBehaviorComponent::ApplyMoveSpeed()
{
	AOfficeworker* Owner = Cast<AOfficeworker>(GetOwner());
	if (!Owner) return;

	UCharacterMovementComponent* Move = Owner->GetCharacterMovement();
	if (!Move) return;

	// 행동 모드와 무관 — 뛰는 상황은 번아웃 배회 하나뿐이다.
	// Officeworker 생성자가 125 로 고정해두므로 런타임에 덮어써야 ABP Speed 축이 러닝 구간에 도달한다.
	Move->MaxWalkSpeed = (FatigueSlackPhase == EFatigueSlackPhase::Bolting) ? MoveSpeedBolt : MoveSpeedWalk;
}

void UEmployeeBehaviorComponent::StartOperationSeatedWork()
{
	AOfficeworker* Owner = Cast<AOfficeworker>(GetOwner());
	if (!Owner)
	{
		return;
	}

	Owner->StopRandomRoaming();

	if (Owner->TeleportToWorkstationAndSit())
	{
		bIsSeated = true;
		SetState(EEmployeeState::Typing);
		StartFloatingTextTimer();
	}
	else
	{
		// 운영중 직원은 항상 데스크 배정(해고=액터 소멸)이라 도달 불가 — 방어적 loud fail.
		UE_LOG(LogTemp, Warning, TEXT("[EmployeeBehaviorComponent] Operation: no workstation, fallback to idle wander"));
		StartIdleWander();
	}
}

void UEmployeeBehaviorComponent::StartIdleWander()
{
	AOfficeworker* Owner = Cast<AOfficeworker>(GetOwner());
	if (!Owner)
	{
		return;
	}

	// 현재 위치에서 가장 가까운 NavMesh 위치로 텔레포트
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (NavSys)
	{
		FVector CurrentLocation = Owner->GetActorLocation();
		FNavLocation NavLocation;
		if (NavSys->ProjectPointToNavigation(CurrentLocation, NavLocation, FVector(1000.f, 1000.f, 1000.f)))
		{
			// 캐릭터 높이 보정 (캡슐 절반 높이만큼 위로)
			FVector TeleportLocation = NavLocation.Location;
			TeleportLocation.Z = CurrentLocation.Z;  // 현재 Z 높이 유지

			Owner->SetActorLocation(TeleportLocation);
			UE_LOG(LogTemp, Log, TEXT("[EmployeeBehaviorComponent] Teleported to nearest NavMesh: (%.1f, %.1f, %.1f)"),
				TeleportLocation.X, TeleportLocation.Y, TeleportLocation.Z);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[EmployeeBehaviorComponent] No NavMesh found nearby"));
		}
	}

	// Wander 상태로 전환
	SetState(EEmployeeState::Wander);
	Walking = true;
	Speed = 1.0f;

	// 랜덤 배회 시작
	Owner->StartRandomRoaming(800.f, 2.f, 5.f);

	UE_LOG(LogTemp, Log, TEXT("[EmployeeBehaviorComponent] Idle wander started"));
}

void UEmployeeBehaviorComponent::StartCheerSitting()
{
	// 앉아서 환호 상태로 전환 (ABP에서 애니메이션 끝나면 자동으로 Sitting으로 전환)
	SetState(EEmployeeState::CheerSitting);
	UE_LOG(LogTemp, Log, TEXT("[EmployeeBehaviorComponent] CheerSitting started"));
}

void UEmployeeBehaviorComponent::StartCheerStandUp()
{
	// 일어서며 환호 상태로 전환 (ABP에서 애니메이션 끝나면 자동으로 StandingIdle로 전환)
	SetState(EEmployeeState::CheerStandUp);
	bIsSeated = false;  // 일어서는 환호이므로
	UE_LOG(LogTemp, Log, TEXT("[EmployeeBehaviorComponent] CheerStandUp started"));
}

void UEmployeeBehaviorComponent::StartGreeting()
{
	SetState(EEmployeeState::Greeting);
	UE_LOG(LogTemp, Log, TEXT("[EmployeeBehaviorComponent] Greeting started"));
}

void UEmployeeBehaviorComponent::OnGreetingComplete()
{
	// 노티파이 + 폴백 타이머 이중 발화 방지 — 이미 인사 상태를 벗어났으면 무시(중복 채용 차단).
	if (EmployeeState != EEmployeeState::Greeting)
	{
		return;
	}

	// 한쪽(노티파이/타이머)이 먼저 완료시키면 남은 폴백 타이머는 정리.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TransitionFallbackTimerHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("[EmployeeBehaviorComponent] Greeting complete"));
	SetState(EEmployeeState::Wander);
	OnGreetingFinished.Broadcast();
}

void UEmployeeBehaviorComponent::StartDance(int32 DanceIndex, float Duration)
{
	// 클립 선택: 음수면 랜덤. 풀이 비면(설정 0) 무시.
	if (NumDanceAnimations <= 0)
	{
		return;
	}
	CurrentDanceIndex = (DanceIndex >= 0)
		? FMath::Clamp(DanceIndex, 0, NumDanceAnimations - 1)
		: FMath::RandRange(0, NumDanceAnimations - 1);

	UE_LOG(LogTemp, Warning, TEXT("[Dance] StartDance → idx=%d (NumDanceAnimations=%d, 요청=%d) — idx 바뀌는데 춤 같으면 ABP Blend 문제"),
		CurrentDanceIndex, NumDanceAnimations, DanceIndex);

	// 제자리에서 춤추도록 진행 중인 이동 즉시 정지 (배회 MoveTo 슬라이딩 방지).
	if (AOfficeworker* OwnerWorker = Cast<AOfficeworker>(GetOwner()))
	{
		if (UCharacterMovementComponent* Move = OwnerWorker->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
		}
	}

	SetState(EEmployeeState::Dance);

	// 루프 클립을 PlayTime 동안 계속 재생 후 Wander 복귀. 시간은 호출 상황이 지정(미지정 시 기본값).
	// 더 길게 추거나 상황 종료 시 끊으려면 StopDance() 사용.
	if (UWorld* World = GetWorld())
	{
		const float PlayTime = (Duration > 0.0f) ? Duration : DanceDefaultDuration;
		World->GetTimerManager().ClearTimer(DanceTimerHandle);
		World->GetTimerManager().SetTimer(DanceTimerHandle, this,
			&UEmployeeBehaviorComponent::OnDanceComplete, PlayTime, false);
	}
}

void UEmployeeBehaviorComponent::StopDance()
{
	if (EmployeeState != EEmployeeState::Dance)
	{
		return;
	}
	OnDanceComplete();
}

void UEmployeeBehaviorComponent::OnDanceComplete()
{
	// 중복/오발화 방지 — 이미 춤 상태가 아니면 무시.
	if (EmployeeState != EEmployeeState::Dance)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DanceTimerHandle);
	}
	SetState(EEmployeeState::Wander);
}

//========================================
// FloatingText 관련 함수들
//========================================

void UEmployeeBehaviorComponent::StartFloatingTextTimer()
{
	// Stage/Operation 모드가 아니면 타이머 시작하지 않음
	if (CurrentBehaviorMode != EEmployeeBehaviorMode::Stage && CurrentBehaviorMode != EEmployeeBehaviorMode::Operation)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		float Interval = GetRandomTextInterval();
		World->GetTimerManager().SetTimer(
			FloatingTextTimerHandle,
			this,
			&UEmployeeBehaviorComponent::SpawnScoreFloatingText,
			Interval,
			false
		);
	}
}

void UEmployeeBehaviorComponent::StopFloatingTextTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FloatingTextTimerHandle);
	}
}

void UEmployeeBehaviorComponent::SpawnScoreFloatingText()
{
	// Stage/Operation 모드가 아니면 중지
	if (CurrentBehaviorMode != EEmployeeBehaviorMode::Stage && CurrentBehaviorMode != EEmployeeBehaviorMode::Operation)
	{
		return;
	}

	// Typing 상태가 아니면 VFX 스폰 스킵, 타이머만 재예약
	if (EmployeeState != EEmployeeState::Typing)
	{
		StartFloatingTextTimer();
		return;
	}

	AOfficeworker* Owner = Cast<AOfficeworker>(GetOwner());
	if (!Owner)
	{
		StartFloatingTextTimer();
		return;
	}

	AWorkstationActorBase* Workstation = Owner->GetAssignedWorkstation();
	if (!Workstation)
	{
		StartFloatingTextTimer();
		return;
	}

	// Operation 모드: 누적 수익 확인
	if (CurrentBehaviorMode == EEmployeeBehaviorMode::Operation && AccumulatedIncome < 1.f)
	{
		StartFloatingTextTimer();
		return;
	}

	if (CurrentBehaviorMode == EEmployeeBehaviorMode::Stage)
	{
		// Stage 모드: 이산 기여 — 지금 시점에 3카테고리 점수 계산 후 반영
		UOfficeStageProgressManager* StageMgr = GetWorld()->GetSubsystem<UOfficeStageProgressManager>();
		if (!StageMgr || !StageMgr->IsTimerRunning())
		{
			StartFloatingTextTimer();
			return;
		}

		// 이벤트 팝업 중에는 점수/이펙트 모두 중단 (타이머와 동기화)
		if (StageMgr->IsEventPaused())
		{
			StartFloatingTextTimer();
			return;
		}

		UCGGameInstance* GameInst = UCGGameInstance::GetInstance();
		if (!GameInst)
		{
			StartFloatingTextTimer();
			return;
		}

		UEmployeeManager* EmpMgr = GameInst->GetSubsystem<UEmployeeManager>();
		if (!EmpMgr)
		{
			StartFloatingTextTimer();
			return;
		}

		FEmployeeInstance* Employee = EmpMgr->FindEmployee(Owner->GetEmployeeID());
		if (!Employee)
		{
			StartFloatingTextTimer();
			return;
		}

		// 기여 간격 (이 직원의 WorkInterval)
		float WorkInterval = CalculateWorkInterval();

		// 활성 직능 스텝 목록 (프로젝트 weight>0 직능) — 직원 직능 어피니티로 가중 랜덤 선택 (특화 = 부서효율 대체)
		const TArray<FStepRoundData>& ActiveSteps = StageMgr->GetProgressData().Steps;
		TArray<int32> CandStepNo;   // 1-based Steps 인덱스 (orb/strip 라우팅용)
		TArray<int32> CandSlot;     // EProductionDiscipline 슬롯
		TArray<float> CandWeight;   // DisciplineAffinity(직원 포인트)
		float TotalW = 0.f;
		for (int32 i = 0; i < ActiveSteps.Num(); ++i)
		{
			if (ActiveSteps[i].DisciplineSlot == INDEX_NONE) { continue; }
			const int32 Slot = ActiveSteps[i].DisciplineSlot;
			const int32 Pts = Employee->DisciplinePoints.IsValidIndex(Slot) ? Employee->DisciplinePoints[Slot] : 0;
			const float W = UOfficeStageProgressManager::DisciplineAffinity(Pts);
			CandStepNo.Add(i + 1);
			CandSlot.Add(Slot);
			CandWeight.Add(W);
			TotalW += W;
		}
		if (CandStepNo.Num() == 0) { return; }  // 활성 직능 없음 (착수 전 / 직능 미설정)

		// 업무집중도 — 이 확률로 주 직능(부서칩과 같은 argmax)을 강제 지정한다. 굴림 실패거나
		// 주 직능이 이번 프로젝트의 활성 스텝에 없으면 조용히 아래 가중 랜덤으로 떨어진다.
		// 상한(MaxFocusChance)이 100% 를 막는 이유 = 직원 1명 사무실에서 다른 스텝이 영구 0점이 되면
		// 출시 게이트를 영영 못 넘는다.
		int32 Pick = INDEX_NONE;
		{
			const int32 EffectiveFocus = Employee->Stats.Focus + UEmployeeTypeHelper::GetEnhanceStatBonus(Employee->EnhancementLevel);
			const float FocusChance = FMath::Min(EffectiveFocus * EmployeeStatTuning::FocusPerPoint, EmployeeStatTuning::MaxFocusChance);
			if (FMath::FRand() < FocusChance)
			{
				const int32 PrimarySlot = GetPrimaryDisciplineSlot(*Employee);
				if (PrimarySlot != INDEX_NONE)
				{
					Pick = CandSlot.IndexOfByKey(PrimarySlot);
				}
			}
		}

		// 가중 랜덤 — 직능 포인트 높은 직능일수록 자주 선택
		if (Pick == INDEX_NONE)
		{
			Pick = CandWeight.Num() - 1;
			float Roll = FMath::FRandRange(0.f, TotalW);
			float Acc = 0.f;
			for (int32 k = 0; k < CandWeight.Num(); ++k)
			{
				Acc += CandWeight[k];
				if (Roll < Acc) { Pick = k; break; }
			}
		}
		const int32 ChosenStep = CandStepNo[Pick];   // orb/strip 라우팅 1-based 인덱스
		const int32 ChosenSlot = CandSlot[Pick];
		const float ChosenAffinity = CandWeight[Pick];

		// 정규화 기준을 고정 상수로 두어 자기상쇄를 끊는다 — 간격이 짧아진 만큼 초당 산출이 오른다.
		// 시각 바닥(MinWorkInterval)을 넘는 초과분은 Overflow 로 흡수해 개당 점수/배출 개수로 환산.
		const float SpeedFactor = GetSpeedFactor();
		const float Overflow = SpeedFactor * WorkInterval;
		const int32 OrbCount = FMath::Clamp(FMath::RoundToInt(Overflow), 1, MaxOrbPerEmission);

		float BaseScore = UEmployeeTypeHelper::CalculateBaseOutput(Employee->Level);
		float RandomFactor = FMath::RandRange(0.8f, 1.2f);
		// 정규화는 ScoreNormInterval(케이던스와 분리된 고정 상수) 로 — BaseWorkInterval 을 쓰면
		// 점수가 케이던스의 제곱에 비례해 히트 수를 늘리는 순간 DPS 가 같이 깎인다.
		float ContributionScore = BaseScore * ChosenAffinity * RandomFactor * ScoreNormInterval * Overflow;

		// 피버타임 — 전역 산출 폭증 (비활성 시 1.0, dev-spectacle)
		ContributionScore *= StageMgr->GetFeverScoreMultiplier();

		// 일시 버프 — ScoreMultiplier 적용
		ContributionScore *= GetTotalScoreMultiplier();

		// 건물 특성 (효율 → 개발 점수 배율). WorkSpeed 버프는 GetSpeedFactor() 로 이동(간격/Overflow 단일 반영).
		{
			if (UCGGameInstance* CGI = UCGGameInstance::GetInstance())
			{
				const int32 CurBldg = CGI->GetCurrentManagedBuildingIndex();
				if (UBuildingTraitManagerSubsystem* TraitMgr = CGI->GetSubsystem<UBuildingTraitManagerSubsystem>())
				{
					ContributionScore *= TraitMgr->GetTraitFactor(CurBldg, EBuildingTraitTarget::DevScore);
				}
			}
		}

		// 피로 커플링 — Tired 초과 시 선형 감산 (피곤한 직원 = 실제로 덜 버는 직원)
		ContributionScore *= GetFatigueOutputFactor();

		// 잠재큐브 (메이플식 % 축) — 줄 최대 3개 순회라 캐시 불요
		const FPotentialModifiers PotMods = UEmployeePotentialHelper::AggregateModifiers(Employee->PotentialAbility);
		ContributionScore *= PotMods.ScoreMult;

		// 크리티컬 판정 (스테이지 기본/특성/피버 + 버프 + 큐브 잭팟 줄 + 크리티컬 확률 스탯)
		// 상한을 두는 이유: 크리에만 sweep 사운드+orb 버스트가 붙어 punctuation 역할을 하므로 상시 크리는 연출을 죽인다
		const int32 EffectiveCrit = Employee->Stats.CritChance + UEmployeeTypeHelper::GetEnhanceStatBonus(Employee->EnhancementLevel);
		float CritChance = FMath::Min(
			StageMgr->GetCriticalChance() + GetTotalCritChanceBonus() + PotMods.CritChanceAdd
				+ EffectiveCrit * EmployeeStatTuning::CritChancePerPoint,
			EmployeeStatTuning::MaxCritChance);
		float CritMultiplier = 1.0f;
		if (FMath::FRand() < CritChance)
		{
			CritMultiplier = 2.0f + PotMods.CritDamageAdd;
		}
		ContributionScore *= CritMultiplier;

		// 선택된 직능 스텝에만 점수 반영
		StageMgr->AddDisciplineContribution(ChosenSlot, ContributionScore);

		// 점수 숫자 "+N" 연출은 orb 요청 경로(OfficeMainWidget::OnScoreOrbRequestedReceived)에서
		// orb와 함께 스크린스페이스로 스폰 (ScoreOrbContainer::SpawnScoreNumber)
		// 히트당 점수가 ~1점 근처라 버림 시 orb 가 침묵 — 반올림 + 최소 1 표시
		int32 DisplayInt = FMath::Max(1, FMath::RoundToInt(ContributionScore));

		// 구슬 비행 요청 (직원의 실제 현재 위치 사용 - 터치 이동 대응)
		if (DisplayInt > 0)
		{
			FVector EmployeePos = Owner->GetActorLocation();
			EmployeePos.Z += 80.f;
			StageMgr->RequestScoreOrb(EmployeePos, ChosenStep, ContributionScore, CritMultiplier > 1.0f, OrbCount);
		}

		StageMgr->OnEmployeeFloatingTextSpawned.Broadcast();

		// 크리 때만 sweep — 1초 드립에선 매 틱 재생 = 청각 피로 (정상 틱은 무음, 크리가 punctuation)
		if (CritMultiplier > 1.0f)
		{
			if (UCGGameInstance* GI = UCGGameInstance::GetInstance())
			{
				if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
				{
					SoundMgr->PlaySound(FName("Event_ScoreSweep"));
				}
			}
		}

		// 첫 기여 완료 → 이후부터는 정규 간격
		bIsFirstStageTimer = false;
	}
	else if (CurrentBehaviorMode == EEmployeeBehaviorMode::Operation)
	{
		// Operation 모드: 직원별 "+N" 자금 플로팅(귀속 표시) + 조용한 적립 + 코인 연출용 pending.
		// MoneyVFX(나이아가라 파티클)만 제거 — 수집 시각화는 오피스 코인 플라이아웃이 담당(난잡 방지).
		int32 DisplayInt = static_cast<int32>(AccumulatedIncome);

		UCGGameInstance* GameInst = UCGGameInstance::GetInstance();
		if (GameInst)
		{
			// "+N" 자금 텍스트는 코인 경로(OfficeLayer::OnIncomeCoinRequestedReceived → SpawnIncomeText)에서
			// C++ 직접 렌더 — BP_DamageText 미사용

			if (UResourceItemManager* ResourceMgr = GameInst->GetSubsystem<UResourceItemManager>())
			{
				ResourceMgr->StoreResource(EResourceType::Money, static_cast<int64>(DisplayInt), false);
			}

			if (UProjectOperationManager* OpMgr = GameInst->GetSubsystem<UProjectOperationManager>())
			{
				// 결산서용 직원 수익 누적
				const int32 BuildingIndex = GameInst->GetCurrentManagedBuildingIndex();
				if (BuildingIndex != INDEX_NONE)
				{
					OpMgr->AddEmployeeRevenue(BuildingIndex, static_cast<float>(DisplayInt));
				}
				// 코인 연출: 이 직원 머리 위에서 돈 아이콘으로 코인 1개 (OfficeLayer 구독)
				FVector CoinPos = Owner->GetActorLocation();
				CoinPos.Z += 80.f;
				OpMgr->RequestIncomeCoin(CoinPos, static_cast<int64>(DisplayInt), Owner->GetEmployeeID());
			}
		}

		// 정수 부분만 차감, 소수점 잔여분은 유지
		AccumulatedIncome -= static_cast<float>(DisplayInt);
	}

	// 다음 타이머 예약
	StartFloatingTextTimer();
}

float UEmployeeBehaviorComponent::GetRandomTextInterval() const
{
	if (CurrentBehaviorMode == EEmployeeBehaviorMode::Operation)
	{
		// 운영 수익도 ~1초 연속 드립 (누적분 표시라 총 수익 불변, <1 게이트가 저수익 자동 억제)
		return FMath::FRandRange(0.9f, 1.1f);
	}

	if (CurrentBehaviorMode == EEmployeeBehaviorMode::Stage)
	{
		float Interval = CalculateWorkInterval();

		// 첫 기여: 랜덤 스태거 오프셋 (직원들이 동시에 기여하지 않도록)
		if (bIsFirstStageTimer)
		{
			return FMath::FRandRange(0.1f, Interval);
		}
		return Interval * FMath::FRandRange(0.9f, 1.1f);  // ±10% desync (동시 발화 방지)
	}

	return FMath::RandRange(1.f, 2.f);
}

float UEmployeeBehaviorComponent::CalculateWorkInterval() const
{
	// 조회 실패 시 GetSpeedFactor()가 1.0f를 반환 → Interval=BaseWorkInterval로 자연 폴백(별도 가드 불요)
	const float Interval = BaseWorkInterval / GetSpeedFactor();
	return FMath::Clamp(Interval, MinWorkInterval, BaseWorkInterval);
}

float UEmployeeBehaviorComponent::ComposeWorkstationSpeedFactor(
	float EmployeeSpeedFactor,
	float WorkstationRate)
{
	return EmployeeSpeedFactor * (1.0f + FMath::Max(0.0f, WorkstationRate));
}

float UEmployeeBehaviorComponent::GetSpeedFactor() const
{
	AOfficeworker* OwnerWorker = Cast<AOfficeworker>(GetOwner());
	UCGGameInstance* GameInst = UCGGameInstance::GetInstance();
	if (!OwnerWorker || !GameInst) { return 1.0f; }

	UEmployeeManager* EmpMgr = GameInst->GetSubsystem<UEmployeeManager>();
	if (!EmpMgr) { return 1.0f; }

	FEmployeeInstance* Employee = EmpMgr->FindEmployee(OwnerWorker->GetEmployeeID());
	if (!Employee) { return 1.0f; }

	const int32 Effective = Employee->Stats.WorkSpeed + UEmployeeTypeHelper::GetEnhanceStatBonus(Employee->EnhancementLevel);
	const float StatFactor = 1.0f + Effective * EmployeeStatTuning::WorkSpeedFactorPerPoint;

	// 버프 Value<1.0 = 빠름 → 나눠서 가속
	const float EmployeeSpeedFactor = StatFactor / FMath::Max(KINDA_SMALL_NUMBER, GetWorkSpeedMultiplier());
	const AWorkstationActorBase* AssignedWorkstation = OwnerWorker->GetAssignedWorkstation();
	const float WorkstationRate = AssignedWorkstation
		? AssignedWorkstation->GetWorkSpeedBonusRate()
		: 0.0f;
	return ComposeWorkstationSpeedFactor(EmployeeSpeedFactor, WorkstationRate);
}

void UEmployeeBehaviorComponent::OnTransitionFallbackTimeout()
{
	AOfficeworker* Owner = Cast<AOfficeworker>(GetOwner());
	int32 EmpID = Owner ? Owner->GetEmployeeID() : -1;

	UE_LOG(LogTemp, Warning, TEXT("[Employee %d] Transition fallback timeout! State: %d, Mode: %d"),
		EmpID, (int32)EmployeeState, (int32)CurrentBehaviorMode);

	// 현재 전환 상태에 따라 적절한 Complete 함수 호출
	switch (EmployeeState)
	{
	case EEmployeeState::StandToSit:
		OnStandToSitComplete();
		break;

	case EEmployeeState::SitToStand:
		OnSitToStandComplete();
		break;

	case EEmployeeState::TypeToSit:
		OnTypeToSitComplete();
		break;

	case EEmployeeState::Greeting:
		OnGreetingComplete();
		break;

	default:
		UE_LOG(LogTemp, Warning, TEXT("[Employee %d] Fallback timeout in unexpected state: %d"),
			EmpID, (int32)EmployeeState);
		break;
	}
}

// ===== 일시 버프 (이벤트 효과) =====

void UEmployeeBehaviorComponent::AddBuff(EBuffType Type, float Value, float Duration)
{
	if (Type == EBuffType::None || Duration <= 0.0f) return;

	const bool bWasActive = HasActiveBuff();

	// 같은 Type 기존 버프가 있으면 RemainingTime을 더 긴 쪽으로, Value도 더 강한 쪽으로
	for (FEmployeeBuff& Existing : ActiveBuffs)
	{
		if (Existing.Type == Type)
		{
			Existing.RemainingTime = FMath::Max(Existing.RemainingTime, Duration);
			// ScoreMultiplier/CritChance는 큰 값이 강함, WorkSpeed는 작은 값이 강함(빠름)
			if (Type == EBuffType::WorkSpeed)
			{
				Existing.Value = FMath::Min(Existing.Value, Value);
			}
			else
			{
				Existing.Value = FMath::Max(Existing.Value, Value);
			}
			OnBuffChanged.Broadcast(true);
			return;
		}
	}

	ActiveBuffs.Add(FEmployeeBuff(Type, Value, Duration));

	if (!bWasActive)
	{
		OnBuffChanged.Broadcast(true);
	}
}

void UEmployeeBehaviorComponent::TickBuffs(float DeltaTime)
{
	if (ActiveBuffs.Num() == 0) return;

	bool bAnyExpired = false;
	for (int32 i = ActiveBuffs.Num() - 1; i >= 0; --i)
	{
		ActiveBuffs[i].RemainingTime -= DeltaTime;
		if (ActiveBuffs[i].RemainingTime <= 0.0f)
		{
			ActiveBuffs.RemoveAt(i);
			bAnyExpired = true;
		}
	}

	if (bAnyExpired)
	{
		OnBuffChanged.Broadcast(HasActiveBuff());
	}
}

float UEmployeeBehaviorComponent::GetTotalScoreMultiplier() const
{
	float Mult = 1.0f;
	for (const FEmployeeBuff& Buff : ActiveBuffs)
	{
		if (Buff.Type == EBuffType::ScoreMultiplier && Buff.IsActive())
		{
			Mult = FMath::Max(Mult, Buff.Value);
		}
	}
	return Mult;
}

float UEmployeeBehaviorComponent::GetTotalCritChanceBonus() const
{
	float Bonus = 0.0f;
	for (const FEmployeeBuff& Buff : ActiveBuffs)
	{
		if (Buff.Type == EBuffType::CritChance && Buff.IsActive())
		{
			Bonus += Buff.Value;
		}
	}
	return Bonus;
}

float UEmployeeBehaviorComponent::GetWorkSpeedMultiplier() const
{
	float Mult = 1.0f;
	for (const FEmployeeBuff& Buff : ActiveBuffs)
	{
		if (Buff.Type == EBuffType::WorkSpeed && Buff.IsActive())
		{
			Mult = FMath::Min(Mult, Buff.Value);
		}
	}
	return Mult;
}

bool UEmployeeBehaviorComponent::HasActiveBuff() const
{
	for (const FEmployeeBuff& Buff : ActiveBuffs)
	{
		if (Buff.IsActive()) return true;
	}
	return false;
}
