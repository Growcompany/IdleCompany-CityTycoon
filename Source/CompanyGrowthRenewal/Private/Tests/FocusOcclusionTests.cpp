#include "Misc/AutomationTest.h"
#include "Player/Components/FocusOcclusionMath.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFocusOcclusionSamplePointsTest,
	"CGR.FocusOcclusion.SamplePoints",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFocusOcclusionSamplePointsTest::RunTest(const FString& Parameters)
{
	// 원점 중심, 하프익스텐트 (100, 200, 1000) — X 와 Y 가 달라야 "짧은 쪽을 쓴다"를 판별할 수 있다
	const FBox Bounds(FVector(-100.f, -200.f, -1000.f), FVector(100.f, 200.f, 1000.f));
	const FVector Right(0.f, 1.f, 0.f);

	{
		TArray<FVector> Points;
		FocusOcclusionMath::ComputeSamplePoints(Bounds, Right, 0.6f, Points);

		TestEqual(TEXT("샘플은 5개"), Points.Num(), 5);
		TestEqual(TEXT("첫 샘플은 바운드 중심"), Points[0], FVector::ZeroVector);

		// 상/하 = Z 하프익스텐트(1000) x Inset(0.6) = 600
		// 기대값/허용오차는 double 리터럴 — FVector 성분이 double 이라 float 리터럴을 쓰면 TestEqual 이 모호해진다(C2666)
		TestEqual(TEXT("상단 샘플 Z = +600"), Points[1].Z, 600.0, 0.01);
		TestEqual(TEXT("하단 샘플 Z = -600"), Points[2].Z, -600.0, 0.01);

		// 좌/우 = min(X,Y) 하프익스텐트(100) x 0.6 = 60. Max 를 쓰면 120 이 나와 실패한다.
		TestEqual(TEXT("우측 샘플 Y = +60 (짧은 축 기준)"), Points[3].Y, 60.0, 0.01);
		TestEqual(TEXT("좌측 샘플 Y = -60 (짧은 축 기준)"), Points[4].Y, -60.0, 0.01);
	}

	{
		// Inset 0 = 전부 중심으로 수렴. 오프셋 축이 뒤바뀌어도 이건 통과하므로 위 단언과 함께 봐야 한다.
		TArray<FVector> Points;
		FocusOcclusionMath::ComputeSamplePoints(Bounds, Right, 0.f, Points);
		TestEqual(TEXT("Inset 0 은 5개 모두 중심"), Points.Num(), 5);
		for (const FVector& P : Points)
		{
			TestTrue(TEXT("Inset 0 샘플은 중심"), P.Equals(FVector::ZeroVector, 0.01f));
		}
	}

	{
		// 중심이 원점이 아닐 때도 상대 오프셋이 유지되는지 — 중심을 더하는 걸 빠뜨리면 여기서 터진다
		const FBox Offset(FVector(900.f, 1800.f, 0.f), FVector(1100.f, 2200.f, 2000.f));
		TArray<FVector> Points;
		FocusOcclusionMath::ComputeSamplePoints(Offset, Right, 0.5f, Points);
		TestEqual(TEXT("이동된 바운드의 중심"), Points[0], FVector(1000.f, 2000.f, 1000.f));
		TestEqual(TEXT("이동된 바운드의 상단 Z"), Points[1].Z, 1500.0, 0.01);
	}

	{
		// 정규화 안 된 Right 벡터를 넣어도 결과가 같아야 한다 (길이 3배)
		TArray<FVector> A, B;
		FocusOcclusionMath::ComputeSamplePoints(Bounds, Right, 0.6f, A);
		FocusOcclusionMath::ComputeSamplePoints(Bounds, Right * 3.f, 0.6f, B);
		TestTrue(TEXT("Right 길이는 결과에 영향 없음"), A[3].Equals(B[3], 0.01f));
	}

	{
		// 무효 바운드는 빈 배열 — 이게 없으면 호출부가 쓰레기 좌표로 트레이스한다
		TArray<FVector> Points;
		Points.Add(FVector::OneVector); // 호출 전 내용이 반드시 지워지는지도 함께 확인
		FocusOcclusionMath::ComputeSamplePoints(FBox(ForceInit), Right, 0.6f, Points);
		TestEqual(TEXT("무효 바운드는 빈 배열"), Points.Num(), 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFocusOcclusionGhostAlphaTest,
	"CGR.FocusOcclusion.GhostAlpha",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFocusOcclusionGhostAlphaTest::RunTest(const FString& Parameters)
{
	constexpr float T = 0.15f;

	// 페이드 인: 정확히 전환시간만큼 지나면 목표 도달
	TestEqual(TEXT("dt = 전환시간 이면 목표 도달"),
		FocusOcclusionMath::StepGhostAlpha(0.f, 1.f, T, T), 1.f, 0.001f);
	TestEqual(TEXT("dt = 전환시간 절반이면 0.5"),
		FocusOcclusionMath::StepGhostAlpha(0.f, 1.f, T * 0.5f, T), 0.5f, 0.001f);

	// 페이드 아웃
	TestEqual(TEXT("역방향도 전환시간만큼이면 0"),
		FocusOcclusionMath::StepGhostAlpha(1.f, 0.f, T, T), 0.f, 0.001f);
	TestEqual(TEXT("역방향 절반은 0.5"),
		FocusOcclusionMath::StepGhostAlpha(1.f, 0.f, T * 0.5f, T), 0.5f, 0.001f);

	// 오버슈트 금지 — Lerp 로 짜면 여기가 아니라 아래 반복 누적에서 터진다
	TestEqual(TEXT("큰 dt 도 목표에서 멈춘다"),
		FocusOcclusionMath::StepGhostAlpha(0.f, 1.f, T * 10.f, T), 1.f, 0.001f);
	TestEqual(TEXT("역방향 큰 dt 도 0 에서 멈춘다"),
		FocusOcclusionMath::StepGhostAlpha(1.f, 0.f, T * 10.f, T), 0.f, 0.001f);

	// 0 나눗셈 방지 — 전환시간 0 은 즉시 목표
	TestEqual(TEXT("전환시간 0 은 즉시 목표"),
		FocusOcclusionMath::StepGhostAlpha(0.f, 1.f, 0.016f, 0.f), 1.f, 0.001f);
	TestEqual(TEXT("전환시간 음수도 즉시 목표"),
		FocusOcclusionMath::StepGhostAlpha(0.f, 1.f, 0.016f, -1.f), 1.f, 0.001f);

	// 프레임을 쪼개 누적해도 전환시간 뒤엔 정확히 도달 — 실제 Tick 사용 형태
	{
		float Alpha = 0.f;
		constexpr float Dt = 0.01f;
		for (int32 i = 0; i < 15; ++i)
		{
			Alpha = FocusOcclusionMath::StepGhostAlpha(Alpha, 1.f, Dt, T);
		}
		TestEqual(TEXT("0.01초 x 15 = 0.15초 뒤 알파 1.0"), Alpha, 1.f, 0.001f);
	}

	// dt 0 이나 음수는 현재값 유지 (일시정지 프레임에서 상태가 튀지 않게)
	TestEqual(TEXT("dt 0 은 현재값 유지"),
		FocusOcclusionMath::StepGhostAlpha(0.3f, 1.f, 0.f, T), 0.3f, 0.001f);
	TestEqual(TEXT("dt 음수는 현재값 유지"),
		FocusOcclusionMath::StepGhostAlpha(0.3f, 1.f, -0.05f, T), 0.3f, 0.001f);

	return true;
}

#endif
