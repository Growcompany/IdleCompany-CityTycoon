#include "Utils/GachaRevealBeatRunner.h"

EGachaRevealLayout FGachaRevealBeatRunner::LayoutFor(int32 Count)
{
	if (Count <= 1) { return EGachaRevealLayout::Single; }
	return Count <= 10 ? EGachaRevealLayout::Ten : EGachaRevealLayout::Thirty;
}

FGachaRevealPlan FGachaRevealBeatRunner::BuildPlan(const TArray<FGachaRevealEntry>& Entries, bool bReduceMotion, bool bHandOpen, bool bUpgrade)
{
	namespace GT = GachaRevealTiming;
	FGachaRevealPlan Out;
	Out.Count = Entries.Num();
	Out.bReduceMotion = bReduceMotion;
	Out.bHandOpen = bHandOpen;
	Out.bUpgrade = bUpgrade;
	if (Out.Count <= 0) { return Out; }

	Out.Layout = LayoutFor(Out.Count);
	for (const FGachaRevealEntry& E : Entries)
	{
		Out.BestGrade = FMath::Max(Out.BestGrade, E.Grade);
		if (E.Grade < GachaRevealModel::HighGrade) { ++Out.LowCount; }
	}
	Out.TopIndex = GachaRevealModel::FindTopIndex(Entries);
	Out.UpgradeSteps = GachaRevealModel::UpgradeSteps(Out.BestGrade);
	Out.StampGroups = GachaRevealModel::BuildStampGroups(Entries, Out.TopIndex);

	auto AddBeat = [&Out](EGachaRevealBeat Beat, float DurationMs) -> FGachaRevealBeatSpec&
	{
		FGachaRevealBeatSpec& Spec = Out.Beats.AddDefaulted_GetRef();
		Spec.Beat = Beat;
		Spec.DurationMs = DurationMs;
		return Spec;
	};

	if (bReduceMotion)
	{
		AddBeat(EGachaRevealBeat::Result, 0.f);
		return Out;
	}

	const bool bSingle = Out.Layout == EGachaRevealLayout::Single;
	const bool bThirty = Out.Layout == EGachaRevealLayout::Thirty;
	const int32 UpgradeCount = bUpgrade ? FMath::Max(0, Out.UpgradeSteps.Num() - 1) : 0;

	AddBeat(EGachaRevealBeat::Throw, GT::ThrowLift + GT::ThrowExit + GT::ThrowDrop + GT::ThrowPress);

	// 승급 단계가 없어도 뚜껑 전 쉼은 있다(시안 :833) ― 그때는 가르기 꼬리에 붙인다
	const float CutRest = GT::CutTail + (UpgradeCount == 0 ? GT::UpgradeTail : 0.f);
	if (bHandOpen)
	{
		FGachaRevealBeatSpec& CutSpec = AddBeat(EGachaRevealBeat::Cut, CutRest);
		CutSpec.bWait = true;
		CutSpec.WaitTimeoutMs = -1.f;
	}
	else
	{
		AddBeat(EGachaRevealBeat::Cut, GT::Cut + CutRest);
	}

	if (UpgradeCount > 0)
	{
		AddBeat(EGachaRevealBeat::Upgrade, static_cast<float>(UpgradeCount) * (GT::UpgradeGap + GT::UpgradePress) + GT::UpgradeTail);
	}
	AddBeat(EGachaRevealBeat::Lid, GT::LidPress);

	const float DealStagger = bSingle ? 0.f : (bThirty ? GT::DealStagger30 : GT::DealStagger10);
	AddBeat(EGachaRevealBeat::Deal, static_cast<float>(Out.Count - 1) * DealStagger + GT::DealFlight + GT::DealTail);

	if (!bSingle)
	{
		const float CascadeStagger = bThirty ? GT::CascadeStagger30 : GT::CascadeStagger10;
		AddBeat(EGachaRevealBeat::Cascade, static_cast<float>(Out.LowCount) * CascadeStagger + GT::CascadeTail);
	}
	else if (Out.TopIndex == INDEX_NONE)
	{
		// 1회·A 이하 = 시안 single()(뒤집기 + 쉼)을 이 박자가 맡는다
		AddBeat(EGachaRevealBeat::Cascade, GT::SingleFlip + GT::SingleTail);
	}

	for (int32 GroupIdx = 0; GroupIdx < Out.StampGroups.Num(); ++GroupIdx)
	{
		const float Members = static_cast<float>(Out.StampGroups[GroupIdx].Value.Num());
		FGachaRevealBeatSpec& StampSpec = AddBeat(EGachaRevealBeat::Stamp, GT::StampRise + GT::StampHitDelay + Members * GT::StampGap + GT::StampTail);
		StampSpec.Group = GroupIdx;
	}

	if (Out.TopIndex != INDEX_NONE)
	{
		constexpr int32 GradeSS = GachaRevealModel::MaxGrade - 1;
		const int32 TopGrade = Entries[Out.TopIndex].Grade;
		const float Rise = TopGrade >= GachaRevealModel::MaxGrade ? GT::SpotlightRiseSSS : (TopGrade >= GradeSS ? GT::SpotlightRiseSS : GT::SpotlightRiseS);
		const float Intro = FMath::Max(0.f, Rise - GT::SpotlightThrow) + GT::SpotlightThrow + GT::SpotlightPress
			+ (TopGrade >= GachaRevealModel::MaxGrade ? GT::SpotlightSSSHitDelay : 0.f) + GT::SpotlightInfoDelay;
		const float Hold = bSingle ? GT::SpotlightHoldSingle : GT::SpotlightHold;
		const float Outro = (bSingle ? 0.f : GT::SpotlightReturn) + GT::SpotlightUndim;
		FGachaRevealBeatSpec& SpotSpec = AddBeat(EGachaRevealBeat::Spotlight, Intro + Hold + Outro);
		SpotSpec.bWait = true;
		SpotSpec.WaitAtMs = Intro;
		SpotSpec.WaitTimeoutMs = Hold;
	}

	// 카드 정렬 이동(SettleSort)은 「이번 최고」와 같이 출발해 아래 띠 동안 끝난다 ― 기다리는 것은 이번 최고 + 아래 띠
	AddBeat(EGachaRevealBeat::Settle, GT::SettleWait + GT::SettleBest + GT::SettleBottom);
	AddBeat(EGachaRevealBeat::Result, 0.f);

	float StartMs = 0.f;
	for (FGachaRevealBeatSpec& Spec : Out.Beats)
	{
		Spec.StartMs = StartMs;
		StartMs += Spec.DurationMs;
	}
	return Out;
}

const TCHAR* FGachaRevealBeatRunner::BeatName(EGachaRevealBeat Beat)
{
	switch (Beat)
	{
	case EGachaRevealBeat::Throw: return TEXT("Throw");
	case EGachaRevealBeat::Cut: return TEXT("Cut");
	case EGachaRevealBeat::Upgrade: return TEXT("Upgrade");
	case EGachaRevealBeat::Lid: return TEXT("Lid");
	case EGachaRevealBeat::Deal: return TEXT("Deal");
	case EGachaRevealBeat::Cascade: return TEXT("Cascade");
	case EGachaRevealBeat::Stamp: return TEXT("Stamp");
	case EGachaRevealBeat::Spotlight: return TEXT("Spotlight");
	case EGachaRevealBeat::Settle: return TEXT("Settle");
	case EGachaRevealBeat::Result: return TEXT("Result");
	}
	return TEXT("?");
}

bool FGachaRevealBeatRunner::Setup(const TArray<FGachaRevealEntry>& Entries, bool bReduceMotion, bool bHandOpen, bool bUpgrade)
{
	if (bRunning || bAdvancing || Entries.Num() == 0) { return false; }
	Plan = BuildPlan(Entries, bReduceMotion, bHandOpen, bUpgrade);
	bRunning = true;
	bWaitReleased = false;
	BeatIndex = INDEX_NONE;
	BeatTimeMs = 0.f;
	ClockMs = 0.f;
	ElapsedRealMs = 0.f;
	Speed = 1.f;
	return true;
}

void FGachaRevealBeatRunner::Reset()
{
	Plan = FGachaRevealPlan();
	bRunning = false;
	bWaitReleased = false;
	BeatIndex = INDEX_NONE;
	BeatTimeMs = 0.f;
	ClockMs = 0.f;
	ElapsedRealMs = 0.f;
	Speed = 1.f;
}

void FGachaRevealBeatRunner::EnterBeat(int32 Index)
{
	if (!Plan.Beats.IsValidIndex(Index)) { return; }
	BeatIndex = Index;
	BeatTimeMs = 0.f;
	bWaitReleased = false;
	const EGachaRevealBeat Beat = Plan.Beats[Index].Beat;
	// 시안 :849 :850 :1044 ― 도장·단독 장면·정리가 시작되면 배속을 푼다
	if (Beat == EGachaRevealBeat::Stamp || Beat == EGachaRevealBeat::Spotlight || Beat == EGachaRevealBeat::Settle || Beat == EGachaRevealBeat::Result)
	{
		Speed = 1.f;
	}
	if (OnBeatStarted)
	{
		// 사본으로 넘긴다 ― 콜백이 Reset 하면 Plan.Beats 가 비워진다
		const FGachaRevealBeatSpec Started = Plan.Beats[Index];
		OnBeatStarted(Index, Started);
	}
}

void FGachaRevealBeatRunner::Advance(float DeltaSec)
{
	if (!bRunning || bAdvancing) { return; }
	TGuardValue<bool> AdvanceGuard(bAdvancing, true);

	if (BeatIndex == INDEX_NONE)
	{
		EnterBeat(0);
		if (!bRunning) { return; }
	}

	float RealMs = FMath::Clamp(DeltaSec, 0.f, MaxStepSec) * 1000.f;
	while (RealMs > 0.f)
	{
		const FGachaRevealBeatSpec& Spec = Plan.Beats[BeatIndex];
		if (Spec.Beat == EGachaRevealBeat::Result)
		{
			BeatTimeMs += RealMs * Speed;
			ClockMs += RealMs * Speed;
			ElapsedRealMs += RealMs;
			return;
		}

		const bool bGated = Spec.bWait && !bWaitReleased;
		const bool bBeforeWait = bGated && BeatTimeMs < Spec.WaitAtMs;
		if (bGated && !bBeforeWait && Spec.WaitTimeoutMs < 0.f)
		{
			// 손 긋기: 실시간만 흐르고 박자 시계는 멈춘다
			ElapsedRealMs += RealMs;
			return;
		}
		const float Boundary = !bGated ? Spec.DurationMs : (bBeforeWait ? Spec.WaitAtMs : Spec.WaitAtMs + Spec.WaitTimeoutMs);
		const float NeedRealMs = (Boundary - BeatTimeMs) / Speed;
		if (RealMs < NeedRealMs)
		{
			BeatTimeMs += RealMs * Speed;
			ClockMs += RealMs * Speed;
			ElapsedRealMs += RealMs;
			return;
		}

		RealMs -= NeedRealMs;
		ElapsedRealMs += NeedRealMs;
		ClockMs += Boundary - BeatTimeMs;
		BeatTimeMs = Boundary;
		if (bGated)
		{
			bWaitReleased = !bBeforeWait;
			continue;
		}
		EnterBeat(BeatIndex + 1);
		if (!bRunning) { return; }
	}
}

EGachaRevealTapResult FGachaRevealBeatRunner::Tap()
{
	const FGachaRevealBeatSpec* Spec = GetCurrentBeat();
	// 시안 :1044 :1128 ― 정리부터는 공개 단계가 아니라 화면 탭을 받지 않는다
	if (!Spec || Spec->Beat == EGachaRevealBeat::Settle || Spec->Beat == EGachaRevealBeat::Result)
	{
		return EGachaRevealTapResult::Ignored;
	}
	if (IsWaiting())
	{
		if (Spec->WaitTimeoutMs >= 0.f)
		{
			// 한 탭 = 이 대기 하나만(시안 :1129). 연타가 다음 박자를 건너뛰지 않게 배속도 켜지 않는다
			BeatTimeMs = Spec->WaitAtMs + Spec->WaitTimeoutMs;
			bWaitReleased = true;
			return EGachaRevealTapResult::Advanced;
		}
		return EGachaRevealTapResult::Ignored;
	}
	// 시안 :1130 ― 손 긋기 동안 화면 터치는 긋기 몸짓(손 뗄 때 클릭이 배속을 켜지 않게)
	if (Plan.bHandOpen && (Spec->Beat == EGachaRevealBeat::Cut || Spec->Beat == EGachaRevealBeat::Upgrade))
	{
		return EGachaRevealTapResult::Ignored;
	}
	Speed = GachaRevealTiming::SkipTimeScale;
	return EGachaRevealTapResult::SpedUp;
}

bool FGachaRevealBeatRunner::CompleteHandCut()
{
	const FGachaRevealBeatSpec* Spec = GetCurrentBeat();
	if (!Spec || Spec->Beat != EGachaRevealBeat::Cut || Spec->WaitTimeoutMs >= 0.f || !IsWaiting()) { return false; }
	bWaitReleased = true;
	return true;
}

bool FGachaRevealBeatRunner::SkipToResult()
{
	if (!bRunning || bAdvancing || Plan.Beats.Num() == 0 || IsOnBeat(EGachaRevealBeat::Result)) { return false; }
	TGuardValue<bool> SkipGuard(bAdvancing, true);
	EnterBeat(Plan.Beats.Num() - 1);
	return true;
}

bool FGachaRevealBeatRunner::IsOnBeat(EGachaRevealBeat Beat) const
{
	const FGachaRevealBeatSpec* Spec = GetCurrentBeat();
	return Spec && Spec->Beat == Beat;
}

bool FGachaRevealBeatRunner::IsWaiting() const
{
	const FGachaRevealBeatSpec* Spec = GetCurrentBeat();
	return Spec && Spec->bWait && !bWaitReleased && BeatTimeMs >= Spec->WaitAtMs;
}

bool FGachaRevealBeatRunner::IsConfirmEnabled() const
{
	// 확인 지연은 아래 띠 착지(= Result 시작)부터 센다(시안 :1059). Result 는 늘 배속 1
	return IsOnBeat(EGachaRevealBeat::Result) && BeatTimeMs >= GachaRevealTiming::SettleConfirmDelay;
}

const FGachaRevealBeatSpec* FGachaRevealBeatRunner::GetCurrentBeat() const
{
	return Plan.Beats.IsValidIndex(BeatIndex) ? &Plan.Beats[BeatIndex] : nullptr;
}
