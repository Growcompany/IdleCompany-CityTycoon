#include "Misc/AutomationTest.h"
#include "Enum/CountryType.h"
#include "Enum/MedallionAnchor.h"
#include "Data/WorldMapProjection.h"
#include "Table/CountryInfoTable.h"
#include "Engine/DataTable.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// 서수는 세이브에, 이름은 DT RowName 매핑에 쓰인다 — 12행 전부가 계약이다.
	// 양 끝점과 개수만 고정하면 중간 스왑(China<->Japan)이 그대로 통과한다.
	struct FCountryTypeContractRow
	{
		ECountryType Value;
		const TCHAR* Name;
	};

	const FCountryTypeContractRow CountryTypeContract[] =
	{
		{ ECountryType::None,        TEXT("None")        },
		{ ECountryType::Korea,       TEXT("Korea")       },
		{ ECountryType::China,       TEXT("China")       },
		{ ECountryType::Japan,       TEXT("Japan")       },
		{ ECountryType::Germany,     TEXT("Germany")     },
		{ ECountryType::USA,         TEXT("USA")         },
		{ ECountryType::SouthAfrica, TEXT("SouthAfrica") },
		{ ECountryType::Australia,   TEXT("Australia")   },
		{ ECountryType::Canada,      TEXT("Canada")      },
		{ ECountryType::Brazil,      TEXT("Brazil")      },
		{ ECountryType::Saudi,       TEXT("Saudi")       },
		{ ECountryType::Singapore,   TEXT("Singapore")   }
	};

	constexpr int32 NumContractRows = static_cast<int32>(UE_ARRAY_COUNT(CountryTypeContract));

	// 판 배치 계약. 스펙 6.3 이 원본이고 여기가 코드 쪽 유일본이다 ― Pts[] 같은 두 번째 사본을 만들지 말 것.
	// 위경도는 실제 지리가 아니라 메달리온 겹침을 피해 민 값이다. RowName 은 DT_CountryInfo 의 행 키와 같다.
	struct FCountryContract
	{
		const TCHAR* RowName;
		double Lon;
		double Lat;
		EMedallionAnchor LabelSide;
		// 위경도 투영 대신 판 좌표를 직접 쓰는 국가. (0,0) = 투영값 사용.
		FVector2D AnchorOverride = FVector2D::ZeroVector;
	};

	// 독일·호주는 2026-09-26 에 뒤집었다 ― 수요 닷이 라벨판 안으로 들어오며 판이 70.5 -> 122.5px 로
	// 자라 판 가장자리를 9.3px 넘쳤다(2304 실측). 뒤집으니 최소 여유 +12.5px · 판↔판·판↔원반 겹침 0.
	// 미국·캐나다는 시설 미니어처가 커져 투영 위치에서 서로 겹치므로 판 좌표를 직접 둔다(디오라마 설계 §지도 위치).
	const FCountryContract CountryContract[] =
	{
		{ TEXT("USA"),         -98.0,  40.0, EMedallionAnchor::Bottom, FVector2D(0.178, 0.37) },
		{ TEXT("Canada"),     -115.0,  48.0, EMedallionAnchor::Top,    FVector2D(0.07,  0.15) },
		{ TEXT("Brazil"),      -51.0, -11.0, EMedallionAnchor::Bottom },
		{ TEXT("Germany"),      10.0,  51.0, EMedallionAnchor::Bottom },
		{ TEXT("SouthAfrica"),  25.0, -30.0, EMedallionAnchor::Right  },
		{ TEXT("Saudi"),        46.0,  23.0, EMedallionAnchor::Bottom },
		{ TEXT("China"),       101.0,  35.0, EMedallionAnchor::Bottom },
		{ TEXT("Singapore"),   104.0,   0.0, EMedallionAnchor::Bottom },
		{ TEXT("Korea"),       126.0,  39.0, EMedallionAnchor::Top    },
		{ TEXT("Japan"),       145.0,  34.0, EMedallionAnchor::Bottom },
		{ TEXT("Australia"),   134.0, -25.0, EMedallionAnchor::Top    }
	};

	constexpr int32 NumCountryContract = static_cast<int32>(UE_ARRAY_COUNT(CountryContract));

	FVector2D ContractAnchor(const FCountryContract& Spec)
	{
		return FWorldMapProjection::IsAnchorSet(Spec.AnchorOverride)
			? Spec.AnchorOverride
			: FWorldMapProjection::ToAnchor(Spec.Lon, Spec.Lat);
	}

	// 마스크 텍스처는 아직 없어 소프트 포인터가 null 인 게 정상이다 ― 로드가 아니라 경로 문자열을 봐야 오타가 잡힌다.
	FString ExpectedRegionMaskPath(const TCHAR* RowName)
	{
		return FString::Printf(
			TEXT("/Game/CompanyGrowth/UI/WorldMap/T_WorldRegion_%s.T_WorldRegion_%s"), RowName, RowName);
	}

	const TCHAR* MedallionAnchorName(EMedallionAnchor Side)
	{
		switch (Side)
		{
		case EMedallionAnchor::Top:    return TEXT("Top");
		case EMedallionAnchor::Bottom: return TEXT("Bottom");
		case EMedallionAnchor::Left:   return TEXT("Left");
		case EMedallionAnchor::Right:  return TEXT("Right");
		}
		return TEXT("<unknown>");
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapCountryTypeEnumTest,
	"CGR.WorldMap.CountryTypeEnum",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapCountryTypeEnumTest::RunTest(const FString& Parameters)
{
	// enum 값은 세이브·DT RowName 매핑에 쓰이므로 순서와 개수가 계약이다.
	const UEnum* E = StaticEnum<ECountryType>();
	TestNotNull(TEXT("enum is reflected"), E);
	if (!E)
	{
		return false; // 아래가 전부 E 를 역참조한다 — 크래시 대신 여기서 끝낸다
	}

	// 계약 표를 줄이면 개수 단언도 같이 줄어 통과하므로 리터럴로 따로 못 박는다
	TestEqual(TEXT("계약 표가 12행"), NumContractRows, 12);
	// NumEnums 는 _MAX 를 포함하므로 12 + 1
	TestEqual(TEXT("11 countries + None"), E->NumEnums() - 1, NumContractRows);
	if (E->NumEnums() - 1 != NumContractRows)
	{
		return false; // 개수가 어긋난 채 인덱스로 훑으면 엉뚱한 행끼리 비교된다
	}

	for (int32 Index = 0; Index < NumContractRows; ++Index)
	{
		const FCountryTypeContractRow& ContractRow = CountryTypeContract[Index];

		// (1) C++ 서수 — 세이브에 이 숫자가 그대로 직렬화된다
		TestEqual(*FString::Printf(TEXT("%s 서수"), ContractRow.Name),
			static_cast<int32>(ContractRow.Value), Index);

		// (2) 리플렉션 순서 — DT RowName 매핑이 인덱스로 이 이름을 찾는다.
		// TestEqual 의 문자열 오버로드는 전부 TestEqualInsensitive 로 내려가므로 Equals 로 정확 비교한다.
		const FString ReflectedName = E->GetNameStringByIndex(Index);
		TestTrue(*FString::Printf(TEXT("index %d 이름 '%s' (기대 '%s')"), Index, *ReflectedName, ContractRow.Name),
			ReflectedName.Equals(ContractRow.Name, ESearchCase::CaseSensitive));
	}

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapProjectionTest,
	"CGR.WorldMap.Projection",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapProjectionTest::RunTest(const FString& Parameters)
{
	// 판 네 귀퉁이 ― 국가 좌표가 아니라 투영 상수 자체라 계약 표에서 읽을 값이 없다.
	const FVector2D TopLeft  = FWorldMapProjection::ToAnchor(-140.0, 74.0);
	const FVector2D BotRight = FWorldMapProjection::ToAnchor(160.0, -48.0);
	TestEqual(TEXT("left edge"),   TopLeft.X,  0.0, 1e-6);
	TestEqual(TEXT("top edge"),    TopLeft.Y,  0.0, 1e-6);
	TestEqual(TEXT("right edge"),  BotRight.X, 1.0, 1e-6);
	TestEqual(TEXT("bottom edge"), BotRight.Y, 1.0, 1e-6);

	TestEqual(TEXT("aspect 2.459"), FWorldMapProjection::AspectRatio, 300.0/122.0, 1e-9);
	return true;
}

// ★ 겹침 회귀 — 목업에서 실제로 한국·일본이 완전히 겹쳤던 사고를 고정한다.
//    디스크 지름 / 판 너비 비는 해상도 불변이므로 해상도를 올려도 안 풀린다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapMedallionSeparationTest,
	"CGR.WorldMap.MedallionSeparation",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapMedallionSeparationTest::RunTest(const FString& Parameters)
{
	// 좌표는 파일 상단 CountryContract 한 벌만 쓴다 ― 여기에 두 번째 사본을 두면 DT 와 어긋나도 아무도 모른다.
	TestEqual(TEXT("11 countries"), NumCountryContract, 11);

	// ★ 아래 55쌍 단언은 "충분히 떨어졌는가" 만 보므로 거리를 부풀리는 버그를 못 잡는다.
	//   그래서 거리 계산 자체를 먼저 못 박는다 ― AspectRatio 나눗셈이 빠지면 세로가 2.459배로 부풀고 55쌍은 전부 통과한다.
	const FVector2D VertTop    = FWorldMapProjection::ToAnchor(100.0, 40.0);
	const FVector2D VertBottom = FWorldMapProjection::ToAnchor(100.0, 10.0);
	TestEqual(TEXT("vertical distance is divided by AspectRatio"),
		FWorldMapProjection::AnchorDistance(VertTop, VertBottom),
		(30.0 / 122.0) / FWorldMapProjection::AspectRatio, 1e-9);

	// 대칭 ― 가로 성분은 AspectRatio 와 무관해야 한다.
	const FVector2D HorzLeft  = FWorldMapProjection::ToAnchor(0.0, 30.0);
	const FVector2D HorzRight = FWorldMapProjection::ToAnchor(60.0, 30.0);
	TestEqual(TEXT("horizontal distance is untouched by AspectRatio"),
		FWorldMapProjection::AnchorDistance(HorzLeft, HorzRight),
		60.0 / 300.0, 1e-9);

	for (int32 i = 0; i < NumCountryContract; ++i)
	{
		const FVector2D A = ContractAnchor(CountryContract[i]);
		TestTrue(FString::Printf(TEXT("%s in board"), CountryContract[i].RowName),
				 A.X >= 0.0 && A.X <= 1.0 && A.Y >= 0.0 && A.Y <= 1.0);

		for (int32 j = i + 1; j < NumCountryContract; ++j)
		{
			const FVector2D B = ContractAnchor(CountryContract[j]);
			const double D = FWorldMapProjection::AnchorDistance(A, B);
			TestTrue(FString::Printf(TEXT("%s<->%s sep %.4f >= %.4f"),
						CountryContract[i].RowName, CountryContract[j].RowName, D, FWorldMapProjection::MinSeparation),
					 D >= FWorldMapProjection::MinSeparation);
		}
	}
	return true;
}

// Review Focus #1 — MapAnchor 가 (0,0) 인 행은 배치에서 빠져야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapUnsetAnchorTest,
	"CGR.WorldMap.UnsetAnchor",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapUnsetAnchorTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("(0,0) is not a placeable anchor"),
				  FWorldMapProjection::IsAnchorSet(FVector2D::ZeroVector));
	TestTrue(TEXT("(0.5,0.5) is placeable"),
				 FWorldMapProjection::IsAnchorSet(FVector2D(0.5, 0.5)));
	// 한 축만 보는 판정은 판 가장자리 국가(캐나다 X=0.0833)를 통째로 지운다.
	TestTrue(TEXT("(0, 0.5) 는 배치 가능 ― 판 좌변 국가를 지우면 안 된다"),
				 FWorldMapProjection::IsAnchorSet(FVector2D(0.0, 0.5)));
	TestTrue(TEXT("(0.5, 0) 는 배치 가능 ― 판 상변 국가를 지우면 안 된다"),
				 FWorldMapProjection::IsAnchorSet(FVector2D(0.5, 0.0)));
	// 판 좌상단에 실제로 놓고 싶은 국가는 없다. (0,0) 은 "안 채웠다"의 신호로 예약한다.
	return true;
}

// ★ 런타임이 읽는 것은 DT 의 MapAnchor 뿐이다. 위 55쌍 테스트는 계약 표의 위경도만 보므로
//    DT 에 한 자리 오타(0.886667 -> 0.88667)가 들어가도 초록이고 화면에서만 겹친다.
//    그래서 여기서는 "떨어졌는가"가 아니라 "계약값과 같은가"를 RowName 키로 대조한다 ―
//    판 범위(0~1) 검사로는 0.05 든 0.95 든 똑같이 통과해 두 국가가 통째로 맞바뀐 것도 못 본다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapDataTableAnchorTest,
	"CGR.WorldMap.DataTableAnchors",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWorldMapDataTableAnchorTest::RunTest(const FString& Parameters)
{
	// 자동화 컨텍스트에는 GameInstance 서브시스템이 없어 TableManager 를 못 거친다.
	// 경로 SOT = TableManagerSubsystem 생성자의 FObjectFinder.
	UDataTable* CountryTable = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/CompanyGrowth/Table/WorldMap/DT_CountryInfo.DT_CountryInfo"));
	if (!CountryTable)
	{
		AddError(TEXT("DT_CountryInfo 로드 실패 ― /Game/CompanyGrowth/Table/WorldMap/DT_CountryInfo.DT_CountryInfo"));
		return false; // 조용히 넘기면 이 테스트가 존재할 이유가 없다
	}

	const UEnum* CountryEnum = StaticEnum<ECountryType>();

	// RowName -> 계약 인덱스. 행 순서에 기대지 않아야 두 국가 값이 통째로 맞바뀐 경우도 잡힌다.
	TMap<FName, int32> ContractByRowName;
	for (int32 ContractIdx = 0; ContractIdx < NumCountryContract; ++ContractIdx)
	{
		ContractByRowName.Add(FName(CountryContract[ContractIdx].RowName), ContractIdx);
	}

	struct FAnchorEntry
	{
		FString Country;
		FVector2D Anchor;
	};
	TArray<FAnchorEntry> AnchorEntries;
	TSet<ECountryType> SeenTypes;

	for (const FName& RowKey : CountryTable->GetRowNames())
	{
		const FCountryInfoTable* InfoRow = CountryTable->FindRow<FCountryInfoTable>(
			RowKey, TEXT("CGR.WorldMap.DataTableAnchors"));
		if (!InfoRow || InfoRow->CountryType == ECountryType::None)
		{
			continue; // TableManagerSubsystem 도 None 행은 캐시에서 뺀다 ― 판에도 안 올라간다
		}

		const FString RowLabel = RowKey.ToString();
		const int32* FoundIdx = ContractByRowName.Find(RowKey);
		if (!TestNotNull(*FString::Printf(TEXT("%s 가 스펙 6.3 계약 표에 있는 RowName"), *RowLabel), FoundIdx))
		{
			continue; // 계약에 없는 행은 기대 좌표 자체가 없다
		}
		const FCountryContract& Spec = CountryContract[*FoundIdx];
		const FVector2D ExpectedAnchor = ContractAnchor(Spec);

		// (1) 기입 여부 ― (0,0) 이면 그 국가가 판에서 사라진다
		TestTrue(*FString::Printf(TEXT("%s MapAnchor 기입됨 (실측 %.6f, %.6f)"),
					*RowLabel, InfoRow->MapAnchor.X, InfoRow->MapAnchor.Y),
				 FWorldMapProjection::IsAnchorSet(InfoRow->MapAnchor));

		// (2) 계약값 일치 ― 판 범위 검사로는 한 자리 오타도 좌우 뒤바뀜도 통과한다.
		// 허용오차 1e-6 = 6자리 반올림의 **이론 상한 5.0e-7** 의 2.0배 (현 데이터 실측 최대는 4.754e-7, USA.Y).
		// 데이터가 아니라 저장 형식에 근거하므로 스펙 §6.3 좌표가 바뀌어도 이 값은 유효하다.
		// 부수 계약: DT 는 6자리 이상 정밀도로 저장돼야 한다 — 5자리면 상한이 5.0e-6 이라 여기서 빨개진다.
		// 더 조이면(1e-7) 정상 DT 가 빨개지고, 더 풀면(1e-5) 자리 누락 오타가 새어 나간다.
		TestEqual(*FString::Printf(TEXT("%s MapAnchor.X (실측 %.9f / 기대 %.9f)"),
					*RowLabel, InfoRow->MapAnchor.X, ExpectedAnchor.X),
			InfoRow->MapAnchor.X, ExpectedAnchor.X, 1e-6);
		TestEqual(*FString::Printf(TEXT("%s MapAnchor.Y (실측 %.9f / 기대 %.9f)"),
					*RowLabel, InfoRow->MapAnchor.Y, ExpectedAnchor.Y),
			InfoRow->MapAnchor.Y, ExpectedAnchor.Y, 1e-6);

		// (3) 라벨 방향 ― 뒤집히면 이웃 라벨과 부딪힌다
		TestTrue(*FString::Printf(TEXT("%s LabelSide (실측 %s / 기대 %s)"),
					*RowLabel, MedallionAnchorName(InfoRow->LabelSide), MedallionAnchorName(Spec.LabelSide)),
				 InfoRow->LabelSide == Spec.LabelSide);

		// (4) 영역 마스크 ― 텍스처가 아직 없어 null 이 정상이므로 로드가 아니라 경로 문자열을 본다
		const FString MaskPath = InfoRow->RegionMask.ToSoftObjectPath().ToString();
		const FString ExpectedMask = ExpectedRegionMaskPath(Spec.RowName);
		TestTrue(*FString::Printf(TEXT("%s RegionMask 경로 (실측 '%s' / 기대 '%s')"),
					*RowLabel, *MaskPath, *ExpectedMask),
				 MaskPath.Equals(ExpectedMask, ESearchCase::CaseSensitive));

		// (5) CountryType 셀 ― 런타임은 행이 아니라 TMap<ECountryType,...> 으로 접는다
		const FString TypeName = CountryEnum
			? CountryEnum->GetNameStringByValue(static_cast<int64>(InfoRow->CountryType))
			: FString();
		TestTrue(*FString::Printf(TEXT("%s CountryType 셀 (실측 '%s')"), *RowLabel, *TypeName),
				 TypeName.Equals(RowLabel, ESearchCase::CaseSensitive));
		SeenTypes.Add(InfoRow->CountryType);

		FAnchorEntry Collected;
		Collected.Country = RowLabel;
		Collected.Anchor = InfoRow->MapAnchor;
		AnchorEntries.Add(Collected);
	}

	// 계약 표끼리 먼저 맞물린다 — 이걸 안 하면 계약 표 오타가 "DT 가 틀렸다" 로 보고돼 엉뚱한 데를 판다.
	{
		TSet<FString> TypeNames;
		for (const FCountryTypeContractRow& TypeRow : CountryTypeContract)
		{
			if (TypeRow.Value != ECountryType::None) { TypeNames.Add(FString(TypeRow.Name)); }
		}
		TSet<FString> AnchorNames;
		for (const FCountryContract& Spec : CountryContract) { AnchorNames.Add(FString(Spec.RowName)); }
		TestTrue(*FString::Printf(TEXT("두 계약 표의 국가 이름 집합 일치 (enum %d / 앵커 %d, 차집합 %d)"),
				TypeNames.Num(), AnchorNames.Num(), TypeNames.Difference(AnchorNames).Num() + AnchorNames.Difference(TypeNames).Num()),
			TypeNames.Num() == AnchorNames.Num() && TypeNames.Difference(AnchorNames).IsEmpty());
	}

	const int32 NumAnchorRows = AnchorEntries.Num();
	TestEqual(TEXT("DT_CountryInfo 유효 행 11개"), NumAnchorRows, 11);
	// 중복이면 TMap 으로 접힐 때 한 국가가 통째로 사라진다 (행 수는 11 그대로라 개수만으로는 안 잡힌다)
	TestEqual(TEXT("CountryType 이 서로 다른 None 아닌 값 11개"), SeenTypes.Num(), 11);

	for (int32 FirstIdx = 0; FirstIdx < NumAnchorRows; ++FirstIdx)
	{
		for (int32 SecondIdx = FirstIdx + 1; SecondIdx < NumAnchorRows; ++SecondIdx)
		{
			const FAnchorEntry& Lhs = AnchorEntries[FirstIdx];
			const FAnchorEntry& Rhs = AnchorEntries[SecondIdx];
			const double Separation = FWorldMapProjection::AnchorDistance(Lhs.Anchor, Rhs.Anchor);
			TestTrue(*FString::Printf(TEXT("%s(%.6f, %.6f)<->%s(%.6f, %.6f) DT 간격 %.4f >= %.4f"),
						*Lhs.Country, Lhs.Anchor.X, Lhs.Anchor.Y,
						*Rhs.Country, Rhs.Anchor.X, Rhs.Anchor.Y,
						Separation, FWorldMapProjection::MinSeparation),
					 Separation >= FWorldMapProjection::MinSeparation);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
