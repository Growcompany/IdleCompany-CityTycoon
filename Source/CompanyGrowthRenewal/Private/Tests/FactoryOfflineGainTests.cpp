#include "Misc/AutomationTest.h"
#include "Data/FactoryOfflineGainPolicy.h"
#include "Data/FactorySaveData.h"
#include "Data/GameSaveData.h"
#include "Manager/SaveLoadManager.h"
#include "Kismet/GameplayStatics.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// 실제 상수를 쓴다 — 테스트가 60/43200 을 따로 들고 있으면 상수를 바꿔도 초록이라 의미가 없다.
	// 함수로 감싼 이유: 파일 스코프 const 로 두면 static 초기화 순서에 의존하게 된다.
	float MinSettle() { return USaveLoadManager::MinOfflineSettleSeconds; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFactoryOfflineSettleTest,
	"CGR.Factory.OfflineSettle",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryOfflineSettleTest::RunTest(const FString& Parameters)
{
	// 초당 1개 · 상한 100000 — 경계만 보도록 상한을 멀리 둔다
	constexpr float Rate = 1.0f;
	constexpr int32 FarCapacity = 100000;

	// ===== 60초 바닥 (돈 경로와 동률) =====
	TestEqual(TEXT("59초는 0개 — 돈 정산도 건너뛰는 구간"),
		FactoryOfflineGainPolicy::SettledBricks(Rate, 59.f, FarCapacity, MinSettle()), 0LL);
	TestEqual(TEXT("60초는 지급된다(경계 포함)"),
		FactoryOfflineGainPolicy::SettledBricks(Rate, 60.f, FarCapacity, MinSettle()), 60LL);

	// ===== 상한 클램프 =====
	TestEqual(TEXT("상한 미만은 그대로"),
		FactoryOfflineGainPolicy::SettledBricks(Rate, 300.f, 1000, MinSettle()), 300LL);
	TestEqual(TEXT("상한에서 정확히 멈춘다"),
		FactoryOfflineGainPolicy::SettledBricks(Rate, 5000.f, 1000, MinSettle()), 1000LL);
	TestEqual(TEXT("상한 경계값은 상한"),
		FactoryOfflineGainPolicy::SettledBricks(Rate, 1000.f, 1000, MinSettle()), 1000LL);

	// 12시간 벽 — 경과시간은 호출 전에 백엔드가 자르므로 이 함수는 12h 를 넘는 입력을 안 본다.
	// 여기서 잠그는 것은 "12h 를 꽉 비웠을 때 받는 양 = 생산율 x 12h" 이고,
	// 그 값이 상한보다 작으면 상한에 영원히 못 닿는다는 사실이다.
	const float Capped = USaveLoadManager::GetOfflineCapSeconds();
	const int64 FullWindowGain = FactoryOfflineGainPolicy::SettledBricks(Rate, Capped, FarCapacity, MinSettle());
	TestEqual(TEXT("12시간 꽉 비우면 생산율 x 12h"), FullWindowGain, static_cast<int64>(Rate * Capped));
	TestTrue(TEXT("그 값이 상한보다 작으면 상한에 못 닿는다"), FullWindowGain < FarCapacity);

	// ===== 생산 불능 / 상한 0 =====
	TestEqual(TEXT("생산율 0 은 0개"),
		FactoryOfflineGainPolicy::SettledBricks(0.f, 3600.f, FarCapacity, MinSettle()), 0LL);
	TestEqual(TEXT("생산율 음수는 0개"),
		FactoryOfflineGainPolicy::SettledBricks(-1.f, 3600.f, FarCapacity, MinSettle()), 0LL);
	TestEqual(TEXT("상한 0 은 0개"),
		FactoryOfflineGainPolicy::SettledBricks(Rate, 3600.f, 0, MinSettle()), 0LL);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFactoryAwayOnlineProductionTest,
	"CGR.Factory.AwayOnlineProduction",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryAwayOnlineProductionTest::RunTest(const FString& Parameters)
{
	FFactoryAwayProductionClock Clock;
	int64 Granted = -1;
	TestTrue(TEXT("도시를 떠날 때 기존 생산율과 소수를 이어받는다"), Clock.Start(0.75, 0.25, 100.0));
	TestTrue(TEXT("온라인 1초 정산"), Clock.Accrue(101.0, MAX_int64, Granted));
	TestEqual(TEXT("60초 바닥이나 오프라인 상한 없이 1개 지급"), Granted, 1LL);
	TestEqual(TEXT("정수 지급 후 소수만 보관"), Clock.Fraction, 0.0);
	TestTrue(TEXT("같은 시각의 재저장은 성공"), Clock.Accrue(101.0, MAX_int64, Granted));
	TestEqual(TEXT("중복 저장은 중복 지급하지 않음"), Granted, 0LL);

	TestTrue(TEXT("다음 도시 밖 맵으로 이동 전 정산"), Clock.Accrue(102.0, MAX_int64, Granted));
	TestEqual(TEXT("외출 중 새 생산량은 아직 정수가 아님"), Granted, 0LL);
	const double SavedFraction = Clock.Fraction;
	Clock.Pause();
	TestFalse(TEXT("맵 로딩 구간은 생산하지 않음"), Clock.Accrue(150.0, MAX_int64, Granted));
	TestTrue(TEXT("도착 맵의 저장 로드 후 재개"), Clock.Resume(150.0));
	TestTrue(TEXT("외출에서 외출로 이어진 소수 생산"), Clock.Accrue(151.0, MAX_int64, Granted));
	TestEqual(TEXT("로딩 48초를 포함하지 않고 1초분만 지급"), Granted, 1LL);
	TestEqual(TEXT("두 번째 외출에도 소수 보존"), SavedFraction, 0.75);
	TestEqual(TEXT("지급 후 소수 보존"), Clock.Fraction, 0.5);

	Clock.Pause();
	TestTrue(TEXT("도시 재진입 때 남은 소수만 저장"), Clock.Fraction >= 0.0 && Clock.Fraction < 1.0);
	FFactorySaveData FactoryRecord(TEXT("MainFactory"));
	FactoryRecord.AwayProductionFraction = Clock.Fraction;
	USaveGame_GameData* Save = NewObject<USaveGame_GameData>();
	Save->GameData.FactoryData.Add(FactoryRecord.FactoryID, FactoryRecord);
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("실제 SaveGame 포맷으로 소수 저장"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return false;
	const USaveGame_GameData* Reloaded = Cast<USaveGame_GameData>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("세이브 메모리 재로드"), Reloaded)) return false;
	const FFactorySaveData* ReloadedFactory = Reloaded->GameData.FactoryData.Find(FactoryRecord.FactoryID);
	if (!TestNotNull(TEXT("공장 저장 행 재로드"), ReloadedFactory)) return false;
	TestEqual(TEXT("재로드 후 소수 생산 보존"), ReloadedFactory->AwayProductionFraction, Clock.Fraction);
	FFactoryAwayProductionClock NextSession;
	TestTrue(TEXT("다음 도시 이탈 때 소수 이어받기"), NextSession.Start(0.75, ReloadedFactory->AwayProductionFraction, 200.0));
	TestTrue(TEXT("이어받은 소수가 다음 수확에 반영"), NextSession.Accrue(201.0, MAX_int64, Granted));
	TestEqual(TEXT("한 번만 지급"), Granted, 1LL);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFactoryAwayOnlineProductionInvalidTest,
	"CGR.Factory.AwayOnlineProductionInvalid",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFactoryAwayOnlineProductionInvalidTest::RunTest(const FString& Parameters)
{
	FFactoryAwayProductionClock Clock;
	int64 Granted = -1;
	TestTrue(TEXT("인원 0의 생산율은 시계 상태 유지"), Clock.Start(0.0, 0.4, 10.0));
	TestTrue(TEXT("인원 0 정산"), Clock.Accrue(100.0, MAX_int64, Granted));
	TestEqual(TEXT("인원 0은 벽돌 지급 없음"), Granted, 0LL);
	TestEqual(TEXT("인원 0은 기존 소수 보존"), Clock.Fraction, 0.4);
	TestTrue(TEXT("역행한 시각은 기준만 재설정"), Clock.Accrue(90.0, MAX_int64, Granted));
	TestEqual(TEXT("역행 시각에서 지급 없음"), Granted, 0LL);
	TestEqual(TEXT("역행 시각에서 소수 보존"), Clock.Fraction, 0.4);
	TestFalse(TEXT("NaN 시각은 거절"), Clock.Accrue(std::numeric_limits<double>::quiet_NaN(), MAX_int64, Granted));
	TestEqual(TEXT("NaN 시각에서 지급 없음"), Granted, 0LL);
	Clock.Pause();
	TestFalse(TEXT("잘못된 재개 시각 거절"), Clock.Resume(-1.0));
	TestFalse(TEXT("거절된 재개는 정지 상태 유지"), Clock.bRunning);
	TestFalse(TEXT("무한 생산율은 시작 실패"), FFactoryAwayProductionClock().Start(std::numeric_limits<double>::infinity(), 0.0, 0.0));
	FFactoryAwayProductionClock FullBankClock;
	TestTrue(TEXT("잔고 여유 검증을 위한 생산 시작"), FullBankClock.Start(2.0, 0.0, 0.0));
	TestFalse(TEXT("자원 은행을 넘길 지급은 거절"), FullBankClock.Accrue(1.0, 1, Granted));
	TestEqual(TEXT("거절된 정산은 지급 없음"), Granted, 0LL);
	TestTrue(TEXT("잔고 여유가 생기면 같은 생산량 정산"), FullBankClock.Accrue(1.0, 2, Granted));
	TestEqual(TEXT("거절 당시 진행분 보존"), Granted, 2LL);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
