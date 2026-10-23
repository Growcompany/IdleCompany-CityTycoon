#pragma once

#include "CoreMinimal.h"
#include "Utils/GachaRevealModel.h"

// 뽑기 상자 공개 연출의 박자 실행기 ― 월드·UObject 의존 0, Advance(Dt) 로만 움직인다(자동화 테스트 대상). 정답 = 시안 startPull(src.html :789~:855)
enum class EGachaRevealBeat : uint8 { Throw, Cut, Upgrade, Lid, Deal, Cascade, Stamp, Spotlight, Settle, Result };

// 부분 지급 장수는 1 / 2..10 / 11..30 등급을 따른다(ComputeSlots 와 같은 경계)
enum class EGachaRevealLayout : uint8 { Single, Ten, Thirty };

enum class EGachaRevealTapResult : uint8 { Ignored, SpedUp, Advanced };

struct FGachaRevealBeatSpec
{
	EGachaRevealBeat Beat = EGachaRevealBeat::Result;
	float StartMs = 0.f; // 명목 시작(속도 1, 대기는 자동 넘김 길이로 친다)
	float DurationMs = 0.f; // 명목 길이(대기 포함). Result = 0 ― 끝 박자라 머문다
	bool bWait = false;
	float WaitAtMs = 0.f; // 박자 안에서 대기가 시작되는 시각
	float WaitTimeoutMs = 0.f; // 탭이 없을 때 자동 넘김까지. 음수 = CompleteHandCut 까지 멈춤
	int32 Group = INDEX_NONE; // Stamp = Plan.StampGroups 인덱스
};

struct FGachaRevealPlan
{
	int32 Count = 0;
	EGachaRevealLayout Layout = EGachaRevealLayout::Single;
	bool bReduceMotion = false;
	bool bHandOpen = false;
	bool bUpgrade = true; // false = 승급 박자 없음, 틈 빛 색은 Cut 마지막 UpgradeTail 시작에 BestGrade 로(시안 :823 :832)
	int32 BestGrade = INDEX_NONE;
	int32 TopIndex = INDEX_NONE; // Entries 인덱스, INDEX_NONE = 단독 장면 없음
	int32 LowCount = 0; // S 미만 = 한 번에 뒤집기 대상
	TArray<int32> UpgradeSteps; // GachaRevealModel::UpgradeSteps ― 승급 단계 수 = Num() - 1
	TArray<TPair<int32, TArray<int32>>> StampGroups; // GachaRevealModel::BuildStampGroups
	TArray<FGachaRevealBeatSpec> Beats;
};

class FGachaRevealBeatRunner
{
public:
	// 앱 복귀 같은 큰 Dt 가 단독 장면 대기·[확인] 지연을 한 번에 건너뛰지 않게 한 번에 이만큼만 쓴다
	static constexpr float MaxStepSec = 0.1f;

	static EGachaRevealLayout LayoutFor(int32 Count);
	static FGachaRevealPlan BuildPlan(const TArray<FGachaRevealEntry>& Entries, bool bReduceMotion, bool bHandOpen, bool bUpgrade = true);
	static const TCHAR* BeatName(EGachaRevealBeat Beat);

	// 실행 중(Setup 뒤 Reset 전)·Advance 안·빈 결과면 false 이고 상태를 건드리지 않는다. 첫 박자는 첫 Advance 에서 시작한다.
	bool Setup(const TArray<FGachaRevealEntry>& Entries, bool bReduceMotion, bool bHandOpen, bool bUpgrade = true);
	void Reset();
	void Advance(float DeltaSec);
	// 단독 장면 대기 = 그 대기만 끝냄 / 정리·결과 = 무시 / 손 긋기 중 화면 터치 = 무시(긋기 몸짓) / 그 밖 = 다음 Stamp·Spotlight·Settle 까지 배속
	EGachaRevealTapResult Tap();
	bool CompleteHandCut();
	// 건너뛰기·뒤로가기 = 남은 박자를 건너뛰고 Result 로. 이미 Result·실행 아님·Advance 안이면 false
	bool SkipToResult();

	bool IsRunning() const { return bRunning; }
	bool IsOnBeat(EGachaRevealBeat Beat) const;
	bool IsWaiting() const;
	bool IsConfirmEnabled() const;
	int32 GetBeatIndex() const { return BeatIndex; }
	const FGachaRevealBeatSpec* GetCurrentBeat() const;
	// 박자 안 위치 ― 대기를 탭으로 끝내면 대기 끝으로 뛴다
	float GetBeatTimeMs() const { return BeatTimeMs; }
	// 배속이 걸린 연속 시계 ― 박자를 넘어 이어지는 애니용, 뛰지 않고 손 긋기 대기 동안만 멈춘다
	float GetClockMs() const { return ClockMs; }
	float GetElapsedRealMs() const { return ElapsedRealMs; }
	float GetSpeed() const { return Speed; }
	const FGachaRevealPlan& GetPlan() const { return Plan; }

	// 박자가 시작될 때마다(Advance 한 번에 여러 개일 수 있음, SkipToResult 도). 이 안에서 Setup·Advance·SkipToResult 는 무시된다. Reset 은 지우지 않는다.
	TFunction<void(int32 StartedIndex, const FGachaRevealBeatSpec& StartedBeat)> OnBeatStarted;

private:
	void EnterBeat(int32 Index);

	FGachaRevealPlan Plan;
	bool bRunning = false;
	bool bAdvancing = false;
	bool bWaitReleased = false;
	int32 BeatIndex = INDEX_NONE;
	float BeatTimeMs = 0.f;
	float ClockMs = 0.f;
	float ElapsedRealMs = 0.f;
	float Speed = 1.f;
};
