#include "Manager/WindowLightManagerSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "TimeCycle/TimeCycleManager.h"
#include "UObject/SoftObjectPtr.h"

UWindowLightManagerSubsystem::UWindowLightManagerSubsystem()
{
	// 생성자에서는 아무것도 하지 않음 (모바일 호환성)
}

void UWindowLightManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("[WindowLightManager] Initialize called"));

	// MPC 런타임 로드
	LoadMPC();

	// TimeCycleManager 캐싱
	UWorld* World = GetWorld();
	if (World)
	{
		CachedTimeCycleManager = Cast<ATimeCycleManager>(
			UGameplayStatics::GetActorOfClass(World, ATimeCycleManager::StaticClass())
		);
		BindVisualTimeManager();

		if (!CachedTimeCycleManager.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[WindowLightManager] TimeCycleManager not found at Initialize!"));
		}
	}

	// Tick 활성화
	bIsInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("[WindowLightManager] Initialized successfully, MPC: %s"), 
		WindowLightMPC ? TEXT("Found") : TEXT("NULL"));
}

void UWindowLightManagerSubsystem::LoadMPC()
{
	// 런타임에 MPC 로드 (ConstructorHelpers 대신)
	const FString MPCPath = TEXT("/Game/CompanyGrowth/Resources/Materials/MainMap/MPC_WindowLight.MPC_WindowLight");
	
	WindowLightMPC = TSoftObjectPtr<UMaterialParameterCollection>(FSoftObjectPath(MPCPath)).LoadSynchronous();

	if (!WindowLightMPC)
	{
		UE_LOG(LogTemp, Error, TEXT("[WindowLightManager] Failed to load MPC from path: %s"), *MPCPath);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[WindowLightManager] MPC loaded successfully"));
	}
}

void UWindowLightManagerSubsystem::Deinitialize()
{
	if (ATimeCycleManager* BoundManager = BoundVisualTimeManager.Get())
	{
		BoundManager->OnVisualTimeChanged.RemoveDynamic(this, &UWindowLightManagerSubsystem::HandleVisualTimeChanged);
	}
	BoundVisualTimeManager.Reset();
	bIsInitialized = false;
	bHasAppliedMPCParameter = false;
	CachedTimeCycleManager.Reset();
	WindowLightMPC = nullptr;
	
	Super::Deinitialize();
	UE_LOG(LogTemp, Log, TEXT("[WindowLightManager] Deinitialized"));
}

bool UWindowLightManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	UWorld* World = Cast<UWorld>(Outer);
	bool bShouldCreate = World && World->IsGameWorld();
	
	UE_LOG(LogTemp, Log, TEXT("[WindowLightManager] ShouldCreateSubsystem: %s"), 
		bShouldCreate ? TEXT("TRUE") : TEXT("FALSE"));
	
	return bShouldCreate;
}

TStatId UWindowLightManagerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWindowLightManagerSubsystem, STATGROUP_Tickables);
}

void UWindowLightManagerSubsystem::Tick(float DeltaTime)
{
	// 첫 시계 동기화는 즉시 처리하고, 시계 재탐색과 이후 변경 감지는 10Hz로 제한한다.
	TickAccumulator += DeltaTime;
	if (TickAccumulator < TickInterval && (bHasAppliedMPCParameter || !CachedTimeCycleManager.IsValid()))
	{
		return;
	}
	TickAccumulator = 0.0f;

	// TimeCycleManager 캐싱 안 되어 있으면 다시 찾기
	if (!CachedTimeCycleManager.IsValid())
	{
		UWorld* World = GetWorld();
		if (World)
		{
			CachedTimeCycleManager = Cast<ATimeCycleManager>(
				UGameplayStatics::GetActorOfClass(World, ATimeCycleManager::StaticClass())
			);
		}

		if (!CachedTimeCycleManager.IsValid())
		{
			return;
		}
		
		bHasAppliedMPCParameter = false;
		UE_LOG(LogTemp, Log, TEXT("[WindowLightManager] TimeCycleManager found in Tick"));
	}
	BindVisualTimeManager();
	RefreshNightIntensity();
}

void UWindowLightManagerSubsystem::BindVisualTimeManager()
{
	ATimeCycleManager* Manager = CachedTimeCycleManager.Get();
	if (BoundVisualTimeManager.Get() == Manager) { return; }
	if (ATimeCycleManager* PreviousManager = BoundVisualTimeManager.Get())
	{
		PreviousManager->OnVisualTimeChanged.RemoveDynamic(this, &UWindowLightManagerSubsystem::HandleVisualTimeChanged);
	}
	BoundVisualTimeManager = Manager;
	if (Manager)
	{
		Manager->OnVisualTimeChanged.AddUniqueDynamic(this, &UWindowLightManagerSubsystem::HandleVisualTimeChanged);
	}
}

void UWindowLightManagerSubsystem::HandleVisualTimeChanged()
{
	RefreshNightIntensity();
}

void UWindowLightManagerSubsystem::RefreshNightIntensity()
{
	// Subsystems initialize before actors; wait until the clock has applied its start time.
	if (!CachedTimeCycleManager.IsValid() || !CachedTimeCycleManager->HasActorBegunPlay())
	{
		return;
	}

	// 캐싱된 TimeCycleManager에서 시간 가져오기
	FTimeCycleCode CurrentTime = CachedTimeCycleManager->GetVisualTime();
	float CurrentHour = CurrentTime.Hours + (CurrentTime.Minutes / 60.0f);

	// Night Intensity 계산
	float NewIntensity = CalculateNightIntensity(CurrentHour);

	// 값이 변경되었을 때만 MPC 업데이트
	if (!bHasAppliedMPCParameter || !FMath::IsNearlyEqual(CurrentNightIntensity, NewIntensity, 0.01f))
	{
		CurrentNightIntensity = NewIntensity;
		bHasAppliedMPCParameter = UpdateMPCParameter();
	}
}

// 시각 → 도시 전체 발광 강도 단일 계산기. 출력은 MPC 스칼라 1개
float UWindowLightManagerSubsystem::CalculateNightIntensity(float CurrentHour) const
{
	// 밤 발광 밝기 단일 노브 (모든 발광 카드 구독 → 도시 전체 점등). 1.0보다 살짝 밝게
	constexpr float MaxIntensity = 1.2f;
	constexpr float PreDuskIntensity = 0.1f;
	// 블렌드 폭은 이 매니저 로컬 상수 (하늘 블렌드와 별개). 단일 소스는 경계(SunRise/SunSet)
	// 램프 끝 = SunSet/SunRise라 완전 점등·소등 시각은 하늘과 일치 (블렌드는 시작만 이동)
	constexpr float DawnBlendHours = 2.0f; // 여명: 불 꺼지는 데 걸리는 시간
	constexpr float DuskBlendHours = 2.0f; // 황혼: 불 켜지는 데 걸리는 시간

	// 밤 경계는 시계(단일 소스)에서 파생. 매니저 없으면 안전 폴백.
	float SunRiseHour = 7.0f;
	float SunSetHour = 20.0f;
	if (CachedTimeCycleManager.IsValid())
	{
		const FTimeCycleCode RiseCode = CachedTimeCycleManager->GetSunRiseTime();
		const FTimeCycleCode SetCode = CachedTimeCycleManager->GetSunSetTime();
		SunRiseHour = RiseCode.Hours + RiseCode.Minutes / 60.0f;
		SunSetHour = SetCode.Hours + SetCode.Minutes / 60.0f;
	}

	const float DawnStart = SunRiseHour - DawnBlendHours;       // 불 꺼지기 시작(여명 시작)
	const float DawnEnd   = SunRiseHour;                        // 불 완전 소등(= 하늘 완전 밝음)
	const float DuskStart = SunSetHour - DuskBlendHours;        // 불 켜지기 시작(황혼 시작)
	const float DuskMid   = SunSetHour - DuskBlendHours * 0.5f; // 약한 예열 → 본점등 전환
	const float DuskEnd   = SunSetHour;                         // 불 완전 점등(= 하늘 완전 어둠)

	// 여명 DawnStart~DawnEnd: 1 → 0 (SunRise에 완전 소등)
	if (CurrentHour >= DawnStart && CurrentHour < DawnEnd)
	{
		return MaxIntensity * (1.0f - (CurrentHour - DawnStart) / FMath::Max(0.01f, DawnEnd - DawnStart));
	}
	// 낮 (DawnEnd~DuskStart): 0
	else if (CurrentHour >= DawnEnd && CurrentHour < DuskStart)
	{
		return 0.0f;
	}
	// 황혼 예열 (DuskStart~DuskMid): 0 → 0.1 (천천히)
	else if (CurrentHour >= DuskStart && CurrentHour < DuskMid)
	{
		return PreDuskIntensity * ((CurrentHour - DuskStart) / FMath::Max(0.01f, DuskMid - DuskStart));
	}
	// 황혼 본점등 DuskMid~DuskEnd: 0.1 → 1 (SunSet에 완전 점등)
	else if (CurrentHour >= DuskMid && CurrentHour < DuskEnd)
	{
		const float Progress = (CurrentHour - DuskMid) / FMath::Max(0.01f, DuskEnd - DuskMid);
		return PreDuskIntensity + (MaxIntensity - PreDuskIntensity) * Progress;
	}
	// 밤: 1
	else
	{
		return MaxIntensity;
	}
}

bool UWindowLightManagerSubsystem::UpdateMPCParameter()
{
	UWorld* World = GetWorld();
	if (!World || !WindowLightMPC)
	{
		return false;
	}

	// MPC에 Night_Intensity 설정 (한 번 호출로 모든 머티리얼에 적용)
	UMaterialParameterCollectionInstance* CollectionInstance = World->GetParameterCollectionInstance(WindowLightMPC);
	return CollectionInstance && CollectionInstance->SetScalarParameterValue(TEXT("Night_Intensity"), CurrentNightIntensity);
}
