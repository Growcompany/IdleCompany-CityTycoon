// TimeCycleSystem 플러그인에서 통합됨
// Original Copyright Grumpy Duck Games 2025 All Rights Reserved.

#include "TimeCycle/TimeCycleManager.h"
#include "Net/UnrealNetwork.h"

ATimeCycleManager::ATimeCycleManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;

	InitialTime = { 8, 0, 0 };
	CycleDuration = { 0, 10, 0 };
	SunRiseTime = { 7, 0, 0 };
	SunSetTime = { 17, 0, 0 };
}

void ATimeCycleManager::BeginPlay()
{
	InitialTimeSeconds = InitialTime.ToSeconds();
	MaxTimeSeconds = MaxTime.ToSeconds();
	CycleDurationSeconds = CycleDuration.ToSeconds();
	CycleSpanSeconds = bHasMaxTime ? (MaxTimeSeconds - InitialTimeSeconds + 86400) % 86400 : 86400;
	SunRiseSeconds = SunRiseTime.ToSeconds();
	SunSetSeconds = SunSetTime.ToSeconds();

	TimeCycleData.CurrentTime = InitialTimeSeconds;

	Super::BeginPlay();
}

void ATimeCycleManager::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, TimeCycleData);
}

void ATimeCycleManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (TimeCycleData.bIsCyclePaused)
		return;

	FTimeCycleData PreviousTimeCycle = TimeCycleData;
	TimeCycleData.CurrentTime = TimeCycleData.CurrentTime + (CycleSpanSeconds / CycleDurationSeconds) * DeltaSeconds;
	TimeCycleData.CurrentTime = FMath::Fmod(TimeCycleData.CurrentTime, 86400.f);

	UpdateNumCycles(PreviousTimeCycle);

	OnRep_TimeCycleData(PreviousTimeCycle);
}

void ATimeCycleManager::StartCycle()
{
	if (!HasAuthority())
		return;

	if (!TimeCycleData.bIsCyclePaused)
		return;

	if (bHasMaxTime && TimeCycleData.GetCurrentSeconds() == MaxTimeSeconds)
		return;

	TimeCycleData.bIsCyclePaused = false;
	OnCycleStarted.Broadcast();
}

void ATimeCycleManager::PauseCycle()
{
	if (!HasAuthority())
		return;

	if (TimeCycleData.bIsCyclePaused)
		return;

	TimeCycleData.bIsCyclePaused = true;
	OnCycleStopped.Broadcast();
}

void ATimeCycleManager::SetCurrentTime(FTimeCycleCode NewTime)
{
	FTimeCycleData PreviousTimeCycle = TimeCycleData;
	TimeCycleData.CurrentTime = NewTime.ToSeconds();
	UpdateNumCycles(PreviousTimeCycle);

	// 시간 텔레포트는 Tick의 OnRep 경로를 안 타 일출/일몰 엣지 이벤트가 안 뜬다 →
	// 이벤트 구독형 리스너(가로등/차량/항공등/빌딩)가 낮/밤을 못 따라옴. 절대 상태가 바뀌면 해당 엣지 재방송.
	auto IsNight = [this](int32 Sec) { return (Sec >= SunSetSeconds) || (Sec < SunRiseSeconds); };
	const bool bWasNight = IsNight(PreviousTimeCycle.GetCurrentSeconds());
	const bool bNowNight = IsNight(TimeCycleData.GetCurrentSeconds());
	if (bNowNight != bWasNight)
	{
		if (bNowNight) { OnSunSet.Broadcast(); } else { OnSunRise.Broadcast(); }
	}
}

void ATimeCycleManager::ResetToInitialTime()
{
	SetCurrentTime(InitialTime);
}

FTimeCycleCode ATimeCycleManager::GetCurrentTime() const
{
	return FTimeCycleCode::FromSeconds(TimeCycleData.GetCurrentSeconds());
}

int32 ATimeCycleManager::GetNumCycles() const
{
	return TimeCycleData.NumCycles;
}

FTimeCycleCode ATimeCycleManager::GetMaxTime() const
{
	return MaxTime;
}

FTimeCycleCode ATimeCycleManager::GetCycleDuration() const
{
	return CycleDuration;
}

bool ATimeCycleManager::HasReachedMaxTime() const
{
	return TimeCycleData.GetCurrentSeconds() == MaxTimeSeconds;
}

FTimeCycleCode ATimeCycleManager::GetSunRiseTime() const
{
	return SunRiseTime;
}

FTimeCycleCode ATimeCycleManager::GetSunSetTime() const
{
	return SunSetTime;
}

bool ATimeCycleManager::ApplyTransientSunSetTimeForPreview(FTimeCycleCode NewTime)
{
#if WITH_EDITOR
	UWorld* ManagerWorld = GetWorld();
	const bool bIsValidTime = NewTime.IsValid()
		&& NewTime.Hours <= 23
		&& NewTime.Minutes <= 59
		&& NewTime.Seconds <= 59;
	if (!HasAuthority() || !ManagerWorld || !ManagerWorld->IsGameWorld() || !bIsValidTime)
	{
		return false;
	}

	SunSetTime = NewTime;
	SunSetSeconds = SunSetTime.ToSeconds();
	return true;
#else
	(void)NewTime;
	return false;
#endif
}

void ATimeCycleManager::UpdateNumCycles(const FTimeCycleData& PreviousTimeCycle)
{
	if (!HasAuthority())
		return;

	if (bHasMaxTime && PreviousTimeCycle.GetCurrentSeconds() < MaxTimeSeconds && TimeCycleData.GetCurrentSeconds() >= MaxTimeSeconds)
	{
		TimeCycleData.CurrentTime = MaxTimeSeconds;
		TimeCycleData.bIsCyclePaused = true;
		TimeCycleData.NumCycles++;
	}

	if (TimeCycleData.GetCurrentSeconds() >= InitialTimeSeconds)
	{
		if (PreviousTimeCycle.GetCurrentSeconds() < InitialTimeSeconds || PreviousTimeCycle.GetCurrentSeconds() > TimeCycleData.GetCurrentSeconds())
		{
			TimeCycleData.NumCycles++;
		}
	}
}

void ATimeCycleManager::OnRep_TimeCycleData(const FTimeCycleData& OldTimeCycleData)
{
	// 사이클 델리게이트
	if (OldTimeCycleData.bIsCyclePaused && !TimeCycleData.bIsCyclePaused)
	{
		OnCycleStarted.Broadcast();
	}
	else if (!OldTimeCycleData.bIsCyclePaused && TimeCycleData.bIsCyclePaused)
	{
		OnCycleStopped.Broadcast();
	}

	// 일출/정오/일몰/자정 델리게이트
	constexpr int32 MidnightDifferenceTolerance = 1000;
	if (OldTimeCycleData.GetCurrentSeconds() < SunRiseSeconds && SunRiseSeconds <= TimeCycleData.GetCurrentSeconds())
	{
		OnSunRise.Broadcast();
	}
	else if (OldTimeCycleData.GetCurrentSeconds() < NoonSeconds && NoonSeconds <= TimeCycleData.GetCurrentSeconds())
	{
		OnNoon.Broadcast();
	}
	else if (OldTimeCycleData.GetCurrentSeconds() < SunSetSeconds && SunSetSeconds <= TimeCycleData.GetCurrentSeconds())
	{
		OnSunSet.Broadcast();
	}
	else if (MidnightSeconds == TimeCycleData.GetCurrentSeconds() ||
		(OldTimeCycleData.GetCurrentSeconds() > TimeCycleData.GetCurrentSeconds() && (MidnightSeconds - (OldTimeCycleData.GetCurrentSeconds() - TimeCycleData.GetCurrentSeconds()) <= MidnightDifferenceTolerance)))
	{
		OnMidnight.Broadcast();
	}
}
