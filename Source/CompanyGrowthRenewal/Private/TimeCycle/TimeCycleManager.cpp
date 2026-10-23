// TimeCycleSystem 플러그인에서 통합됨
// Original Copyright Grumpy Duck Games 2025 All Rights Reserved.

#include "TimeCycle/TimeCycleManager.h"
#include "Net/UnrealNetwork.h"
#include "TimeCycle/TimeCycleContinuation.h"
#include "Core/CGGameInstance.h"
#include "HAL/PlatformTime.h"

bool FTimeCycleContinuation::Advance(double ElapsedSeconds)
{
	auto IsClockTime = [](const FTimeCycleCode& Code)
	{
		return Code.IsValid() && Code.Hours < 24 && Code.Minutes < 60 && Code.Seconds < 60;
	};
	if (!bValid || !FMath::IsFinite(ElapsedSeconds) || ElapsedSeconds < 0.0
		|| !FMath::IsFinite(CurrentTime) || CurrentTime < 0.f || CurrentTime >= 86400.f || NumCycles < 0
		|| !IsClockTime(InitialTime) || !IsClockTime(MaxTime) || !IsClockTime(CycleDuration)
		|| !IsClockTime(SunRiseTime) || !IsClockTime(SunSetTime) || CycleDuration.ToSeconds() <= 0)
	{
		return false;
	}
	if (bIsCyclePaused || ElapsedSeconds == 0.0) { return true; }
	const int32 InitialSeconds = InitialTime.ToSeconds();
	const int32 MaximumSeconds = MaxTime.ToSeconds();
	const int32 SpanSeconds = bHasMaxTime ? (MaximumSeconds - InitialSeconds + 86400) % 86400 : 86400;
	// 기존 Tick의 정수 속도를 유지해 맵 전환 전후의 시간 배율을 일치시킨다.
	const double AdvanceSeconds = ElapsedSeconds * (SpanSeconds / CycleDuration.ToSeconds());
	if (!FMath::IsFinite(AdvanceSeconds)) { return false; }
	if (bHasMaxTime)
	{
		const double RemainingSeconds = FMath::Fmod(MaximumSeconds - static_cast<double>(CurrentTime) + 86400.0, 86400.0);
		if (AdvanceSeconds >= RemainingSeconds)
		{
			CurrentTime = static_cast<float>(MaximumSeconds);
			bIsCyclePaused = true;
			if (RemainingSeconds > 0.0 && NumCycles < MAX_int32) { ++NumCycles; }
			return true;
		}
	}
	const double UnwrappedTime = CurrentTime + AdvanceSeconds;
	if (!FMath::IsFinite(UnwrappedTime)) { return false; }
	if (!bHasMaxTime)
	{
		const double Crossings = FMath::FloorToDouble((UnwrappedTime - InitialSeconds) / 86400.0)
			- FMath::FloorToDouble((static_cast<double>(CurrentTime) - InitialSeconds) / 86400.0);
		NumCycles += static_cast<int32>(FMath::Min(Crossings, static_cast<double>(MAX_int32 - NumCycles)));
	}
	// float 반올림으로 86400이 되면 다음 프레임에 유효 범위를 벗어나므로 상한을 제한한다.
	CurrentTime = FMath::Min(static_cast<float>(FMath::Fmod(UnwrappedTime, 86400.0)), 86399.9921875f);
	return true;
}

ATimeCycleManager::ATimeCycleManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;

	InitialTime = { 8, 0, 0 };
	CycleDuration = { 0, 10, 0 };
	SunRiseTime = { 7, 0, 0 };
	SunSetTime = { 19, 0, 0 };
}

void ATimeCycleManager::CacheTimeSettings()
{
	InitialTimeSeconds = InitialTime.ToSeconds();
	MaxTimeSeconds = MaxTime.ToSeconds();
	SunRiseSeconds = SunRiseTime.ToSeconds();
	SunSetSeconds = SunSetTime.ToSeconds();
}

FTimeCycleContinuation ATimeCycleManager::MakeContinuation() const
{
	FTimeCycleContinuation State;
	State.bValid = true;
	State.CurrentTime = TimeCycleData.CurrentTime;
	State.NumCycles = TimeCycleData.NumCycles;
	State.bIsCyclePaused = TimeCycleData.bIsCyclePaused;
	State.InitialTime = InitialTime;
	State.bHasMaxTime = bHasMaxTime;
	State.MaxTime = MaxTime;
	State.CycleDuration = CycleDuration;
	State.SunRiseTime = SunRiseTime;
	State.SunSetTime = SunSetTime;
	return State;
}

void ATimeCycleManager::BeginPlay()
{
	CacheTimeSettings();
	TimeCycleData.CurrentTime = InitialTimeSeconds;

	// BP BeginPlay의 StartCycle/기본 시간 설정 이후에 이전 월드 상태를 복원한다.
	Super::BeginPlay();
	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!HasAuthority() || !GameInstance) { return; }
	FTimeCycleContinuation State = GameInstance->GetTimeCycleContinuation();
	if (!State.bValid || !FMath::IsFinite(State.CapturedRealtimeSeconds) || State.CapturedRealtimeSeconds <= 0.0
		|| !State.Advance(FPlatformTime::Seconds() - State.CapturedRealtimeSeconds)) { return; }
	const bool bWasPaused = TimeCycleData.bIsCyclePaused;
	InitialTime = State.InitialTime;
	bHasMaxTime = State.bHasMaxTime;
	MaxTime = State.MaxTime;
	CycleDuration = State.CycleDuration;
	SunRiseTime = State.SunRiseTime;
	SunSetTime = State.SunSetTime;
	CacheTimeSettings();
	TimeCycleData.CurrentTime = State.CurrentTime;
	TimeCycleData.NumCycles = State.NumCycles;
	TimeCycleData.bIsCyclePaused = State.bIsCyclePaused;
	if (bWasPaused != TimeCycleData.bIsCyclePaused)
	{
		if (TimeCycleData.bIsCyclePaused) { OnCycleStopped.Broadcast(); }
		else { OnCycleStarted.Broadcast(); }
	}
	// 로딩 중 지나간 모든 이벤트를 재생하지 않고 복원된 낮/밤 상태를 알린다.
	const int32 CurrentSeconds = TimeCycleData.GetCurrentSeconds();
	if (CurrentSeconds >= SunSetSeconds || CurrentSeconds < SunRiseSeconds) { OnSunSet.Broadcast(); }
	else { OnSunRise.Broadcast(); }
	if (!bHasVisualTimeOverride) { OnVisualTimeChanged.Broadcast(); }
}

void ATimeCycleManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && EndPlayReason == EEndPlayReason::LevelTransition)
	{
		if (UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance()))
		{
			FTimeCycleContinuation State = MakeContinuation();
			State.CapturedRealtimeSeconds = FPlatformTime::Seconds();
			if (State.Advance(0.0)) { GameInstance->SetTimeCycleContinuation(State); }
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ATimeCycleManager::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, TimeCycleData);
}

void ATimeCycleManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || TimeCycleData.bIsCyclePaused)
		return;

	FTimeCycleContinuation State = MakeContinuation();
	if (!State.Advance(DeltaSeconds)) { return; }
	const FTimeCycleData PreviousTimeCycle = TimeCycleData;
	TimeCycleData.CurrentTime = State.CurrentTime;
	TimeCycleData.NumCycles = State.NumCycles;
	TimeCycleData.bIsCyclePaused = State.bIsCyclePaused;

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
		if (!bHasVisualTimeOverride) { OnVisualTimeChanged.Broadcast(); }
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

FTimeCycleCode ATimeCycleManager::GetVisualTime() const
{
	return bHasVisualTimeOverride ? VisualTimeOverride : GetCurrentTime();
}

bool ATimeCycleManager::SetVisualTimeOverride(FTimeCycleCode NewTime)
{
	UWorld* ManagerWorld = GetWorld();
	if (!HasAuthority() || !ManagerWorld || !ManagerWorld->IsGameWorld()
		|| !NewTime.IsValid() || NewTime.Hours > 23 || NewTime.Minutes > 59 || NewTime.Seconds > 59)
	{
		return false;
	}
	if (bHasVisualTimeOverride && VisualTimeOverride == NewTime) { return true; }
	VisualTimeOverride = NewTime;
	bHasVisualTimeOverride = true;
	OnVisualTimeChanged.Broadcast();
	return true;
}

void ATimeCycleManager::ClearVisualTimeOverride()
{
	if (!HasAuthority() || !bHasVisualTimeOverride) { return; }
	bHasVisualTimeOverride = false;
	OnVisualTimeChanged.Broadcast();
}

bool ATimeCycleManager::IsVisualNight() const
{
	const int32 VisualSeconds = GetVisualTime().ToSeconds();
	return VisualSeconds >= SunSetSeconds || VisualSeconds < SunRiseSeconds;
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
		if (!bHasVisualTimeOverride) { OnVisualTimeChanged.Broadcast(); }
	}
	else if (OldTimeCycleData.GetCurrentSeconds() < NoonSeconds && NoonSeconds <= TimeCycleData.GetCurrentSeconds())
	{
		OnNoon.Broadcast();
	}
	else if (OldTimeCycleData.GetCurrentSeconds() < SunSetSeconds && SunSetSeconds <= TimeCycleData.GetCurrentSeconds())
	{
		OnSunSet.Broadcast();
		if (!bHasVisualTimeOverride) { OnVisualTimeChanged.Broadcast(); }
	}
	else if (MidnightSeconds == TimeCycleData.GetCurrentSeconds() ||
		(OldTimeCycleData.GetCurrentSeconds() > TimeCycleData.GetCurrentSeconds() && (MidnightSeconds - (OldTimeCycleData.GetCurrentSeconds() - TimeCycleData.GetCurrentSeconds()) <= MidnightDifferenceTolerance)))
	{
		OnMidnight.Broadcast();
	}
}
