#include "Misc/AutomationTest.h"
#include "Entity/Officeworker/FunnyPartPicker.h"
#include "Manager/TableManagerSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCGRFunnyPartPickerTest,
	"CGR.Employee.Cosmetics.FunnyPartPicker",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCGRFunnyPartPickerTest::RunTest(const FString& Parameters)
{
	const TArray<int32> Even = { 1, 1, 1, 1, 1 };

	// 결정성 — 한 프로세스에서 두 번 불러 비교하는 것은 전역 RNG 구현도 20% 확률로 통과시킨다.
	// 골든값이라야 "세션/빌드가 바뀌어도 같다"를 실제로 관측한다(세이브에 외형을 안 넣는 근거).
	TestEqual(TEXT("골든: PickWeightedIndex(42, Hair, Even)"),
		FFunnyPartPicker::PickWeightedIndex(42, EFunnyPartSlot::Hair, Even), 4);

	// Hash 비트 동일성 — 구 AFunnyOfficeworker::AppearanceHash 와 같은 상수열이라야 기존 직원 외형이 유지된다.
	TestEqual(TEXT("골든: Hash(1234, 0x9E3779B1)"),
		static_cast<int64>(FFunnyPartPicker::Hash(1234, 0x9E3779B1u)), static_cast<int64>(1768308367));

	// 솔트 쌍별 불일치 — 충돌하면 두 외형 축이 조용히 상관되고 진단 비용이 크다.
	// 레거시 4종은 스틱 외형축(얼굴/스킨/헤어/옷)이 쓰던 값 — 재사용하면 축끼리 상관이 생긴다.
	TArray<uint32> Salts;
	for (int32 SlotIdx = 0; SlotIdx < static_cast<int32>(EFunnyPartSlot::Max); ++SlotIdx)
	{
		Salts.Add(FFunnyPartPicker::SaltFor(static_cast<EFunnyPartSlot>(SlotIdx)));
	}
	TestEqual(TEXT("슬롯 솔트 수 = Max"), Salts.Num(), static_cast<int32>(EFunnyPartSlot::Max));
	Salts.Append({ 0x9E3779B1u, 0x85EBCA6Bu, 0xC2B2AE35u, 0x27D4EB2Fu, 0x3C6EF372u });   // FunnyOfficeworker 쪽 해시 솔트(피부/헤어/천/수염확률)

	for (int32 i = 0; i < Salts.Num(); ++i)
	{
		for (int32 j = i + 1; j < Salts.Num(); ++j)
		{
			TestTrue(FString::Printf(TEXT("솔트 %d 와 %d 가 다르다 (0x%08X)"), i, j, Salts[i]), Salts[i] != Salts[j]);
		}
	}

	// 축 독립 — 슬롯마다 솔트가 달라야 헤어와 신발이 같이 움직이지 않는다.
	int32 SameCount = 0;
	for (int32 Id = 0; Id < 200; ++Id)
	{
		if (FFunnyPartPicker::PickWeightedIndex(Id, EFunnyPartSlot::Hair, Even)
			== FFunnyPartPicker::PickWeightedIndex(Id, EFunnyPartSlot::Shoe, Even))
		{
			++SameCount;
		}
	}
	// 결정적 값이라 플레이크가 없다. 기대 40(200/5), σ≈5.66, 실측 39 → [25,60] 밴드로 양측 구속.
	// 구 임계값 100 은 완전 상관만 잡고 25% 초과 상관(≈90)은 통과시켰다.
	TestTrue(FString::Printf(TEXT("헤어와 신발이 상관되지 않는다 (200명 중 일치 %d, 기대 밴드 [25,60])"), SameCount),
		SameCount >= 25 && SameCount <= 60);

	// 범위 — 항상 유효 인덱스
	for (int32 Id = -50; Id < 50; ++Id)
	{
		const int32 Idx = FFunnyPartPicker::PickWeightedIndex(Id, EFunnyPartSlot::Body, Even);
		TestTrue(TEXT("인덱스가 풀 범위 안"), Idx >= 0 && Idx < Even.Num());
	}

	// 빈 풀 / 전부 0가중치 → INDEX_NONE (호출자가 폴백하도록)
	TestEqual(TEXT("빈 풀은 INDEX_NONE"),
		FFunnyPartPicker::PickWeightedIndex(1, EFunnyPartSlot::Body, TArray<int32>()), (int32)INDEX_NONE);
	TestEqual(TEXT("전부 0가중치면 INDEX_NONE"),
		FFunnyPartPicker::PickWeightedIndex(1, EFunnyPartSlot::Body, TArray<int32>{ 0, 0, 0 }), (int32)INDEX_NONE);

	// 가중치 반영 — 0 가중치 항목은 절대 안 뽑힌다
	const TArray<int32> Skewed = { 0, 5, 0 };
	for (int32 Id = 0; Id < 100; ++Id)
	{
		TestEqual(TEXT("0가중치는 뽑히지 않는다"),
			FFunnyPartPicker::PickWeightedIndex(Id, EFunnyPartSlot::Outerwear, Skewed), 1);
	}

	// 성별 매칭 — Any 는 양쪽 다 통과
	TestTrue(TEXT("Any 는 남성 직원에 매치"),
		FFunnyPartPicker::IsGenderMatch(EFunnyPartGender::Any, EEmployeeGender::Male));
	TestTrue(TEXT("Any 는 여성 직원에 매치"),
		FFunnyPartPicker::IsGenderMatch(EFunnyPartGender::Any, EEmployeeGender::Female));
	TestTrue(TEXT("Female 행은 여성 직원에 매치"),
		FFunnyPartPicker::IsGenderMatch(EFunnyPartGender::Female, EEmployeeGender::Female));
	TestFalse(TEXT("Female 행은 남성 직원에 매치되지 않는다"),
		FFunnyPartPicker::IsGenderMatch(EFunnyPartGender::Female, EEmployeeGender::Male));

	// 얼굴 판 개인 차이 — 골든값이라야 세션이 바뀌어도 같은 얼굴임을 관측한다(세이브에 얼굴을 안 넣는 근거).
	const FFunnyFaceVariation Face42 = FFunnyPartPicker::PickFaceVariation(42);
	TestNearlyEqual(TEXT("골든: PickFaceVariation(42).EyeScale"), Face42.EyeScale, 0.954187f, 1e-4f);
	TestNearlyEqual(TEXT("골든: PickFaceVariation(42).EyeOffsetY"), Face42.EyeOffsetY, 0.000886f, 1e-4f);
	TestNearlyEqual(TEXT("골든: PickFaceVariation(42).MouthOffsetY"), Face42.MouthOffsetY, -0.009433f, 1e-4f);

	bool bFaceInRange = true;
	float MinEyeScale = 2.f;
	float MaxEyeScale = 0.f;
	for (int32 Id = -50; Id < 150; ++Id)
	{
		const FFunnyFaceVariation Face = FFunnyPartPicker::PickFaceVariation(Id);
		bFaceInRange = bFaceInRange
			&& Face.EyeScale >= 0.92f && Face.EyeScale <= 1.08f
			&& Face.EyeOffsetY >= -0.02f && Face.EyeOffsetY <= 0.02f
			&& Face.MouthOffsetY >= -0.015f && Face.MouthOffsetY <= 0.02f;
		MinEyeScale = FMath::Min(MinEyeScale, Face.EyeScale);
		MaxEyeScale = FMath::Max(MaxEyeScale, Face.EyeScale);
	}
	TestTrue(TEXT("얼굴 개인 차이가 설계 범위 안"), bFaceInRange);
	// 실측 0.920~1.078 — 범위의 대부분을 쓰지 않으면 솔트/단위 변환이 망가진 것이다.
	TestTrue(FString::Printf(TEXT("눈 크기가 사람마다 퍼진다 (%.3f~%.3f)"), MinEyeScale, MaxEyeScale),
		MinEyeScale < 0.94f && MaxEyeScale > 1.06f);

	// 블록마다 각 스타일이 정확히 한 번 나온다. 카탈로그 크기 변경에도 픽커는 같은 계약을 유지한다.
	for (const int32 Count : { 1, 3, 5, 7 })
	{
		for (int32 Block = 0; Block < 4; ++Block)
		{
			TSet<int32> Seen;
			for (int32 Offset = 0; Offset < Count; ++Offset)
			{
				const int32 Id = 1 + Block * Count + Offset;
				const int32 Style = FFunnyPartPicker::PickFaceStyle(Id, Count);
				TestTrue(TEXT("스타일 번호가 카탈로그 범위 안"), Style >= 0 && Style < Count);
				Seen.Add(Style);
				TestEqual(TEXT("동일 ID는 같은 스타일"), Style, FFunnyPartPicker::PickFaceStyle(Id, Count));
			}
			TestEqual(TEXT("연속 ID 블록은 모든 스타일을 한 번씩 사용"), Seen.Num(), Count);
		}
	}
	TestEqual(TEXT("빈 카탈로그"), FFunnyPartPicker::PickFaceStyle(1, 0), INDEX_NONE);
	TestEqual(TEXT("음수 크기"), FFunnyPartPicker::PickFaceStyle(1, -1), INDEX_NONE);
	TestEqual(TEXT("얼굴 배정 골든 ID 1"), FFunnyPartPicker::PickFaceStyle(1, 5), 1);
	TestEqual(TEXT("얼굴 배정 골든 ID 5"), FFunnyPartPicker::PickFaceStyle(5, 5), 0);
	TestEqual(TEXT("얼굴 배정 골든 ID 6"), FFunnyPartPicker::PickFaceStyle(6, 5), 4);
	TestEqual(TEXT("얼굴 배정 골든 ID 42"), FFunnyPartPicker::PickFaceStyle(42, 5), 2);
	for (const int32 Id : { MIN_int32, -1, 0, MAX_int32 })
	{
		const int32 Style = FFunnyPartPicker::PickFaceStyle(Id, 5);
		TestTrue(TEXT("ID 경계에서도 유효한 스타일"), Style >= 0 && Style < 5);
		TestEqual(TEXT("ID 경계의 결정성"), Style, FFunnyPartPicker::PickFaceStyle(Id, 5));
	}

	UDataTable* FaceTable = NewObject<UDataTable>();
	FaceTable->RowStruct = FFunnyFaceStyleTable::StaticStruct();
	FFunnyFaceStyleTable FaceRow;
	FaceRow.EyesAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Test/Eyes.Eyes")));
	FaceRow.MouthAtlas = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Test/Mouth.Mouth")));
	FaceRow.StyleIndex = 1;
	FaceTable->AddRow(TEXT("Second"), FaceRow);
	FaceRow.StyleIndex = 0;
	FaceTable->AddRow(TEXT("First"), FaceRow);
	TArray<FFunnyFaceStyleTable> OrderedFaces;
	const bool bOrdered = UTableManagerSubsystem::BuildFunnyFaceStyles(FaceTable, OrderedFaces);
	TestTrue(TEXT("행 삽입 순서와 무관하게 연속 인덱스 정렬"), bOrdered);
	if (bOrdered && TestEqual(TEXT("정렬된 스타일 수"), OrderedFaces.Num(), 2))
	{
		TestEqual(TEXT("첫 스타일 인덱스 0"), OrderedFaces[0].StyleIndex, 0);
	}
	FaceRow.StyleIndex = 1;
	FaceTable->AddRow(TEXT("First"), FaceRow);
	TestFalse(TEXT("중복 인덱스가 있으면 카탈로그 전체 거부"),
		UTableManagerSubsystem::BuildFunnyFaceStyles(FaceTable, OrderedFaces));
	TestTrue(TEXT("실패 뒤 오래된 캐시를 쓰지 않음"), OrderedFaces.IsEmpty());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
