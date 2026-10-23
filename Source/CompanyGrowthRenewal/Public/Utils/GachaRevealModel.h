#pragma once

#include "CoreMinimal.h"

// 뽑기 상자 공개 연출 순수 계산 — 월드 의존 0(자동화 테스트 대상). 화면 기준 = docs/05_UI/mockups/GachaRevealJuice_2026-10-17/src.html
struct FGachaRevealEntry
{
	int32 PullIndex = 0;
	int32 Grade = 0; // 0=C … 5=SSS
	bool bNew = false;
	int32 SlotKey = INDEX_NONE; // 장비 부위, 없으면 INDEX_NONE
	bool bEmployeeKnown = false; // false = 직원 없이 연 화면(오피스 도크) → 추천·비교 없음
	int32 EquippedGrade = INDEX_NONE; // 그 부위에 지금 낀 등급, INDEX_NONE = 빈 부위(어떤 등급이든 더 좋다)
	int32 MileageGained = 0;
};

enum class EGachaRevealSort : uint8 { Grade, PullOrder };

struct FGachaRevealRect
{
	float X = 0.f;
	float Y = 0.f;
	float W = 0.f;
	float H = 0.f;
};

// 인덱스를 주고받는 곳은 전부 Entries 배열 인덱스. 「먼저 뽑힌 것」 = PullIndex 가 작은 것.
namespace GachaRevealModel
{
	constexpr int32 MaxGrade = 5;
	constexpr int32 HighGrade = 3; // S 이상 = 뒤집지 않고 도장·단독 장면으로 공개

	// 최고 등급 중 먼저 뽑힌 것. 최고가 HighGrade 미만이면 INDEX_NONE(단독 장면 없음).
	int32 FindTopIndex(const TArray<FGachaRevealEntry>& Entries);

	// (등급, 인덱스들) 등급 오름차순. HighGrade 이상만, TopIndex 는 단독 장면이 맡으므로 뺀다.
	TArray<TPair<int32, TArray<int32>>> BuildStampGroups(const TArray<FGachaRevealEntry>& Entries, int32 TopIndex);

	// 틈 빛 색 단계(첫 값 = 가르기 색, 이후 = 승급 1단계씩). 최고 A 이하는 빈 배열.
	TArray<int32> UpgradeSteps(int32 BestGrade);

	// Grade = 등급 내림 → NEW 먼저 → 추천 먼저 → PullIndex 오름. PullOrder = PullIndex 오름.
	TArray<int32> SortedOrder(const TArray<FGachaRevealEntry>& Entries, EGachaRevealSort SortMode, const TSet<int32>& Recommended);

	// 직원을 아는 결과만, 부위마다 낀 등급을 넘는 것 중 최고 1개(동률 = 먼저 뽑힌 것). 등급만 비교, 오름차순 인덱스.
	TArray<int32> PickRecommended(const TArray<FGachaRevealEntry>& Entries);

	// 시안 slots() 그대로(가상 세로 1440). 배치 등급: 1 = 단독, 2..10 = 10회, 11..30 = 30회. 폭은 반올림해서 넘길 것(floor 경계).
	TArray<FGachaRevealRect> ComputeSlots(int32 Count, float CanvasWidth, bool bSettled);

	// 개수 띠에 낼 등급 = 가중치>0 ∪ 실제로 나온 등급, 높은 순.
	TArray<int32> TallyGrades(const TArray<int32>& Weights, const TArray<FGachaRevealEntry>& Entries);
}

// 속도 1.0 기준 ms(SkipTimeScale 제외). 현행 시간표 = docs/05_UI/GACHA_PANELS.md. 접두 = 박자 이름(EGachaRevealBeat) + Single·Close, 공통 = Skip·Hand·Card·Tally·Hold·Orb.
namespace GachaRevealTiming
{
	constexpr float SkipTimeScale = 4.f; // 화면 터치 가속 = 다음 도장·단독 장면 전까지 시간을 이 배로 당긴다
	constexpr float SkipShowFade = 200.f;
	constexpr float SkipHideFade = 150.f;

	// 1 던지기
	constexpr float ThrowPanelBlur = 250.f;
	constexpr float ThrowDimFade = 260.f;
	constexpr float ThrowLift = 120.f;
	constexpr float ThrowExit = 140.f;
	constexpr float ThrowHeaderFade = 200.f;
	constexpr float ThrowTallyFade = 220.f;
	constexpr float ThrowDrop = 280.f;
	constexpr float ThrowPress = 140.f;

	// 2 테이프 가르기
	constexpr float Cut = 320.f;
	constexpr float CutTail = 80.f;
	constexpr float HandHintLoop = 1100.f;
	constexpr float HandTextShow = 200.f;
	constexpr float HandTextHide = 150.f;

	// 3 승급
	constexpr float UpgradeGap = 100.f;
	constexpr float UpgradePress = 180.f;
	constexpr float UpgradeTail = 90.f; // 승급 단계가 없어도 뚜껑 전에 쉰다

	// 4 뚜껑 ― 박자 길이는 LidPress, 날개·빛 기둥·막은 다음 박자와 겹쳐 끝난다
	constexpr float LidInnerGlowFade = 160.f;
	constexpr float LidFront = 300.f;
	constexpr float LidBack = 320.f;
	constexpr float LidBeamHigh = 900.f;
	constexpr float LidBeamLow = 600.f;
	constexpr float LidCurtain = 520.f;
	constexpr float LidPress = 240.f;

	// 5 카드 뿌리기 ― 상자 떨어지기는 기다리지 않는다
	constexpr float DealFlight = 400.f;
	constexpr float DealStagger10 = 22.f;
	constexpr float DealStagger30 = 11.f;
	constexpr float DealBoxDropDelay = 120.f;
	constexpr float DealBoxDrop = 420.f;
	constexpr float DealTail = 140.f;

	// 6 한 번에 뒤집기 ― 1회 뽑기는 이 박자 전체(꼬리 포함)를 건너뛴다
	constexpr float CascadeStagger10 = 45.f;
	constexpr float CascadeStagger30 = 19.f;
	constexpr float CascadeFlip = 210.f;
	constexpr float CascadeBumpDelay = 100.f;
	constexpr float CascadeTail = 220.f;

	// 카드·개수 띠 공통
	constexpr float CardFlash = 380.f;
	constexpr float TallyBump = 260.f;
	constexpr float HoldBreathePeriod = 900.f;
	constexpr float OrbPeriod = 2600.f;
	constexpr float OrbPeriodSSS = 1700.f;

	// 7 도장 묶음 ― 묶음 길이 = StampRise + StampHitDelay + 장수 × StampGap + StampTail
	constexpr float StampHot = 140.f;
	constexpr float StampRise = 180.f;
	constexpr float StampGap = 55.f;
	constexpr float StampFlip = 170.f;
	constexpr float StampHitDelay = 160.f;
	constexpr float StampDrop = 300.f;
	constexpr float StampHold = 380.f;
	constexpr float StampFade = 220.f;
	constexpr float StampTail = 270.f;

	// 8 최고 1장 단독 장면 ― 던지기 출발 = max(0, 상승음 − SpotlightThrow), 착지가 상승음 끝과 맞는다
	constexpr float SpotlightDim = 200.f;
	constexpr float SpotlightRiseS = 240.f;
	constexpr float SpotlightRiseSS = 340.f;
	constexpr float SpotlightRiseSSS = 460.f;
	constexpr float SpotlightThrow = 300.f;
	constexpr float SpotlightCurtain = 520.f;
	constexpr float SpotlightPillarRise = 380.f;
	constexpr float SpotlightPillarBreathe = 1800.f;
	constexpr float SpotlightPillarBreatheDelay = 400.f;
	constexpr float SpotlightPress = 180.f;
	constexpr float SpotlightSSSHitDelay = 90.f;
	constexpr float SpotlightStampDelay = 80.f;
	constexpr float SpotlightStampSoundDelay = 200.f;
	constexpr float SpotlightInfoDelay = 200.f;
	constexpr float SpotlightInfoSlide = 300.f;
	constexpr float SpotlightHold = 800.f; // 정보가 밀려 들어오기 시작할 때부터 센다
	constexpr float SpotlightHoldSingle = 900.f;
	constexpr float SpotlightOutFade = 160.f;
	constexpr float SpotlightPillarOut = 200.f;
	constexpr float SpotlightReturn = 260.f;
	constexpr float SpotlightUndim = 180.f;

	// 1회 뽑기에서 최고가 A 이하일 때(단독 장면 대신)
	constexpr float SingleFlip = 240.f;
	constexpr float SingleTail = 300.f;

	// 9 결과 정리 ― 카드 이동과 「이번 최고」는 같이 출발, [확인] 지연은 아래 띠 착지부터(게이지와 겹침)
	constexpr float SettleWait = 80.f;
	constexpr float SettleSort = 450.f;
	constexpr float SettleSortToggleFade = 200.f;
	constexpr float SettleBest = 380.f;
	constexpr float SettleBottom = 260.f;
	constexpr float SettleCount = 600.f;
	constexpr float SettleCountHit = 700.f;
	constexpr float SettleConfirmDelay = 500.f;
	constexpr float SettleConfirmFade = 200.f;

	// 닫기 ― 패널 주인공 상자가 위에서 다시 떨어져 앉는다
	constexpr float CloseFade = 200.f;
	constexpr float CloseBoxDrop = 320.f;
	constexpr float CloseBoxPress = 180.f;
}
