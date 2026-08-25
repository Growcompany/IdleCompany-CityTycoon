#include "Manager/WindowLightManagerSubsystem.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialParameterCollection.h"
#include "TimeCycle/TimeCycleManager.h"

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

		if (!CachedTimeCycleManager)
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
	
	WindowLightMPC = Cast<UMaterialParameterCollection>(
		StaticLoadObject(UMaterialParameterCollection::StaticClass(), nullptr, *MPCPath)
	);

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
	bIsInitialized = false;
	
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
	// Tick 간격 제어
	TickAccumulator += DeltaTime;
	if (TickAccumulator < TickInterval)
	{
		return;
	}
	TickAccumulator = 0.0f;

	// TimeCycleManager 캐싱 안 되어 있으면 다시 찾기
	if (!CachedTimeCycleManager)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			CachedTimeCycleManager = Cast<ATimeCycleManager>(
				UGameplayStatics::GetActorOfClass(World, ATimeCycleManager::StaticClass())
			);
		}

		if (!CachedTimeCycleManager)
		{
			return;
		}
		
		UE_LOG(LogTemp, Log, TEXT("[WindowLightManager] TimeCycleManager found in Tick"));
	}

	// 캐싱된 TimeCycleManager에서 시간 가져오기
	FTimeCycleCode CurrentTime = CachedTimeCycleManager->GetCurrentTime();
	float CurrentHour = CurrentTime.Hours + (CurrentTime.Minutes / 60.0f);

	// Night Intensity 계산
	float NewIntensity = CalculateNightIntensity(CurrentHour);

	// 값이 변경되었을 때만 MPC 업데이트
	if (!FMath::IsNearlyEqual(CurrentNightIntensity, NewIntensity, 0.01f))
	{
		CurrentNightIntensity = NewIntensity;
		UpdateMPCParameter();
	}
}

float UWindowLightManagerSubsystem::CalculateNightIntensity(float CurrentHour) const
{
	// 밤 발광 밝기 단일 노브(기본 완전점등=1.0, 모든 발광카드가 이 게이트 구독 → 도시 전체 야간 발광). 살짝 밝게.
	constexpr float MaxIntensity = 1.2f;
	constexpr float PreDuskIntensity = 0.1f;
	// 블렌드 폭은 의도적으로 이 조명 매니저의 로컬 상수다(하늘 ATimeCycleSky의 EditAnywhere 블렌드와 별개).
	// 단일 소스는 '경계'(SunRise/SunSet)이며, 하늘 어둠 ⟺ 조명 점등 일치는 '끝점'에서 보장된다:
	// 램프 끝(DuskEnd/DawnEnd)은 항상 SunSet/SunRise라, 블렌드가 서로 달라도 완전점등/완전소등 시각은 일치
	// (블렌드는 램프 '시작'만 옮긴다). 조명 블렌드까지 런타임 동기가 필요해지면 그때 공유 노브로 승격할 것.
	constexpr float DawnBlendHours = 2.0f; // 여명: 불 꺼지는 데 걸리는 시간
	constexpr float DuskBlendHours = 2.0f; // 황혼: 불 켜지는 데 걸리는 시간

	// 밤 경계는 시계(단일 소스)에서 파생. 매니저 없으면 안전 폴백.
	float SunRiseHour = 7.0f;
	float SunSetHour = 20.0f;
	if (CachedTimeCycleManager)
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

	// 여명 (DawnStart~DawnEnd): 1 → 0 (하늘 밝아짐과 동기, SunRise에 완전 소등)
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
	// 황혼 본점등 (DuskMid~DuskEnd): 0.1 → 1 (하늘 어두워짐과 동기, SunSet에 완전 점등)
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

void UWindowLightManagerSubsystem::UpdateMPCParameter()
{
	UWorld* World = GetWorld();
	if (!World || !WindowLightMPC)
	{
		return;
	}

	// MPC에 Night_Intensity 설정 (한 번 호출로 모든 머티리얼에 적용)
	UKismetMaterialLibrary::SetScalarParameterValue(
		World,
		WindowLightMPC,
		TEXT("Night_Intensity"),
		CurrentNightIntensity
	);
}
