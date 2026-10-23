#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Table/LoadingTipTable.h"
#include "UObject/SoftObjectPath.h"

namespace LoadingTipTestUtils
{
	FLoadingTipTable MakeRow(const TCHAR* Key, ELoadingTipContext Context, int32 Weight,
		const TCHAR* Body = TEXT("본문입니다."))
	{
		FLoadingTipTable Row;
		Row.TipKey = FName(Key);
		Row.Category = FName(TEXT("Test"));
		Row.Context = Context;
		Row.Weight = Weight;
		Row.Body = FText::FromString(Body);
		return Row;
	}

	/** 저작 CSV 한 줄 → (머리 5칸, 본문). 본문은 항상 마지막이고 따옴표로 감싸져 있다 */
	bool SplitAuthoredLine(const FString& Line, TArray<FString>& OutHead, FString& OutBody)
	{
		int32 QuoteIndex = INDEX_NONE;
		if (!Line.FindChar(TEXT('"'), QuoteIndex))
		{
			return false;
		}
		Line.Left(QuoteIndex).ParseIntoArray(OutHead, TEXT(","), false);
		OutBody = Line.Mid(QuoteIndex + 1).TrimEnd();
		OutBody.RemoveFromEnd(TEXT("\""));
		return true;
	}

	/** NEXON 글리프가 없거나 금지된 문자 ― em/en dash, 변형선택자, BMP 밖(이모지) */
	bool HasForbiddenGlyph(const FString& Body, FString& OutWhy)
	{
		for (const TCHAR Char : Body)
		{
			const uint32 Code = static_cast<uint32>(Char);
			if (Code == 0x2014 || Code == 0x2013)
			{
				OutWhy = FString::Printf(TEXT("U+%04X dash (한글 줄표 U+2015 를 쓸 것)"), Code);
				return true;
			}
			if (Code == 0xFE0F || (Code >= 0xD800 && Code <= 0xDFFF))
			{
				OutWhy = FString::Printf(TEXT("U+%04X (이모지/BMP 밖)"), Code);
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadingTipContextTest, "CGR.LoadingTip.ContextFilter",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLoadingTipContextTest::RunTest(const FString& Parameters)
{
	using namespace LoadingTipTestUtils;

	const FLoadingTipTable Anywhere = MakeRow(TEXT("Any"), ELoadingTipContext::Any, 10);
	const FLoadingTipTable OfficeOnly = MakeRow(TEXT("Office"), ELoadingTipContext::Office, 10);

	TestTrue(TEXT("Any 행은 어느 컨텍스트에서도 뽑힌다"),
		FLoadingTipRules::IsEligible(Anywhere, ELoadingTipContext::WorldMap));
	TestTrue(TEXT("Office 행은 Office 에서 뽑힌다"),
		FLoadingTipRules::IsEligible(OfficeOnly, ELoadingTipContext::Office));
	TestFalse(TEXT("Office 행은 WorldMap 에서 제외된다"),
		FLoadingTipRules::IsEligible(OfficeOnly, ELoadingTipContext::WorldMap));
	TestTrue(TEXT("컨텍스트 Any 조회는 전 행을 본다 (치트 미리보기 경로)"),
		FLoadingTipRules::IsEligible(OfficeOnly, ELoadingTipContext::Any));

	TestFalse(TEXT("Weight 0 은 문구가 있어도 제외 ― 지우지 않고 끄는 축"),
		FLoadingTipRules::IsEligible(MakeRow(TEXT("Off"), ELoadingTipContext::Any, 0),
			ELoadingTipContext::Any));
	TestFalse(TEXT("빈 Body 는 제외 ― 빈 밴드가 뜨면 안 된다"),
		FLoadingTipRules::IsEligible(MakeRow(TEXT("Empty"), ELoadingTipContext::Any, 10, TEXT("")),
			ELoadingTipContext::Any));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadingTipWeightTest, "CGR.LoadingTip.WeightedPick",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLoadingTipWeightTest::RunTest(const FString& Parameters)
{
	using namespace LoadingTipTestUtils;

	// 누적 구간: A=[0,1) B=[1,4) C 는 Weight 0 이라 구간 없음
	TArray<FLoadingTipTable> Rows;
	Rows.Add(MakeRow(TEXT("A"), ELoadingTipContext::Any, 1));
	Rows.Add(MakeRow(TEXT("B"), ELoadingTipContext::Any, 3));
	Rows.Add(MakeRow(TEXT("C"), ELoadingTipContext::Any, 0));

	TestEqual(TEXT("Roll 0 -> A"),
		FLoadingTipRules::Pick(Rows, ELoadingTipContext::Any, NAME_None, 0), 0);
	TestEqual(TEXT("Roll 1 -> B (A 구간 끝 바로 다음)"),
		FLoadingTipRules::Pick(Rows, ELoadingTipContext::Any, NAME_None, 1), 1);
	TestEqual(TEXT("Roll 3 -> B (B 구간 끝)"),
		FLoadingTipRules::Pick(Rows, ELoadingTipContext::Any, NAME_None, 3), 1);
	TestEqual(TEXT("Roll 은 총가중치로 모듈러 ― 4 는 다시 A"),
		FLoadingTipRules::Pick(Rows, ELoadingTipContext::Any, NAME_None, 4), 0);

	for (uint32 Roll = 0; Roll < 64; ++Roll)
	{
		const int32 Picked = FLoadingTipRules::Pick(Rows, ELoadingTipContext::Any, NAME_None, Roll);
		if (Picked == 2)
		{
			AddError(FString::Printf(TEXT("Weight 0 행이 Roll %u 에서 뽑혔다"), Roll));
			break;
		}
	}

	TArray<FLoadingTipTable> AllOff;
	AllOff.Add(MakeRow(TEXT("Off"), ELoadingTipContext::Any, 0));
	TestEqual(TEXT("후보가 전무하면 INDEX_NONE ― 호출자가 LastResort 로 떨어진다"),
		FLoadingTipRules::Pick(AllOff, ELoadingTipContext::Any, NAME_None, 0), int32(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadingTipRepeatTest, "CGR.LoadingTip.AvoidRepeat",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLoadingTipRepeatTest::RunTest(const FString& Parameters)
{
	using namespace LoadingTipTestUtils;

	TArray<FLoadingTipTable> Rows;
	Rows.Add(MakeRow(TEXT("A"), ELoadingTipContext::Any, 10));
	Rows.Add(MakeRow(TEXT("B"), ELoadingTipContext::Any, 10));

	for (uint32 Roll = 0; Roll < 32; ++Roll)
	{
		const int32 Picked = FLoadingTipRules::Pick(Rows, ELoadingTipContext::Any, FName(TEXT("A")), Roll);
		if (Picked != 1)
		{
			AddError(FString::Printf(TEXT("직전 팁 A 가 Roll %u 에서 또 뽑혔다 (picked=%d)"), Roll, Picked));
			break;
		}
	}

	// 해당 컨텍스트에 팁이 하나뿐이면 반복이 빈 밴드보다 낫다
	TArray<FLoadingTipTable> Single;
	Single.Add(MakeRow(TEXT("Only"), ELoadingTipContext::Any, 10));
	TestEqual(TEXT("후보가 직전 팁 하나뿐이면 다시 쓴다"),
		FLoadingTipRules::Pick(Single, ELoadingTipContext::Any, FName(TEXT("Only")), 0), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadingTipAuthoringTest, "CGR.LoadingTip.CsvAuthoring",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLoadingTipAuthoringTest::RunTest(const FString& Parameters)
{
	using namespace LoadingTipTestUtils;

	// 임포트 전 저작 파일을 직접 검사한다 ― DT 재임포트는 전체 교체라 CSV 가 SOT 다
	const FString CsvPath = FPaths::ProjectDir() / TEXT("DataImport/DT_LoadingTip_Import.csv");
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *CsvPath))
	{
		// 쿠킹된 실행에는 DataImport 가 없다 ― 그 환경에서 실패시키면 거짓 빨강이 된다
		AddInfo(FString::Printf(TEXT("DataImport CSV 없음 (검사 생략): %s"), *CsvPath));
		return true;
	}
	if (!TestTrue(TEXT("헤더 + 최소 1행"), Lines.Num() >= 2))
	{
		return false;
	}

	const TSet<FString> ValidContexts = { TEXT("Any"), TEXT("City"), TEXT("Office"),
		TEXT("WorldMap"), TEXT("Recruitment") };
	TSet<FString> SeenKeys;
	int32 Counted = 0;
	for (int32 Index = 1; Index < Lines.Num(); ++Index)
	{
		const FString Line = Lines[Index].TrimStartAndEnd();
		if (Line.IsEmpty())
		{
			continue;
		}
		TArray<FString> Head;
		FString Body;
		if (!TestTrue(FString::Printf(TEXT("%d행: 따옴표로 감싼 Body"), Index),
			SplitAuthoredLine(Line, Head, Body)))
		{
			continue;
		}
		++Counted;
		TestEqual(FString::Printf(TEXT("%d행: 6칸"), Index), Head.Num(), 6);
		if (Head.Num() < 5)
		{
			continue;
		}
		const FString RowName = Head[0];
		TestEqual(FString::Printf(TEXT("%s: RowName == TipKey (임포트 규약)"), *RowName), Head[1], RowName);
		TestTrue(FString::Printf(TEXT("%s: Context 가 enum 값"), *RowName), ValidContexts.Contains(Head[3]));
		TestTrue(FString::Printf(TEXT("%s: Weight 가 정수"), *RowName), Head[4].IsNumeric());
		TestFalse(FString::Printf(TEXT("%s: 키 중복"), *RowName), SeenKeys.Contains(RowName));
		SeenKeys.Add(RowName);

		TestFalse(FString::Printf(TEXT("%s: Body 비어 있음"), *RowName), Body.IsEmpty());
		if (Body.Len() > FLoadingTipRules::MaxBodyChars)
		{
			AddError(FString::Printf(TEXT("%s: Body %d자 > 상한 %d자 ― 인게임에서 2줄로 접힌다: %s"),
				*RowName, Body.Len(), FLoadingTipRules::MaxBodyChars, *Body));
		}
		FString Why;
		if (HasForbiddenGlyph(Body, Why))
		{
			AddError(FString::Printf(TEXT("%s: 금지 글리프 %s ― %s"), *RowName, *Why, *Body));
		}
	}
	TestTrue(TEXT("저작 행이 1개 이상"), Counted > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadingTipLiveTableTest, "CGR.LoadingTip.LiveTable",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLoadingTipLiveTableTest::RunTest(const FString& Parameters)
{
	// 임포트된 실제 에셋으로 「DT -> 행 추출 -> 추첨」 전 구간을 돌린다.
	// TableManagerSubsystem 과 같은 경로/같은 추출 규칙을 쓰므로, 여기가 녹색이면 런타임도 같은 행을 본다.
	const UDataTable* Table = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Table/UI/DT_LoadingTip.DT_LoadingTip"))).LoadSynchronous();
	if (!Table)
	{
		AddError(TEXT("DT_LoadingTip 로드 실패 ― Tools/LoadingTip/reimport_tips.py 를 먼저 돌릴 것"));
		return false;
	}
	TestEqual(TEXT("RowStruct 가 FLoadingTipTable"),
		Table->GetRowStruct(), static_cast<const UScriptStruct*>(FLoadingTipTable::StaticStruct()));

	TArray<FLoadingTipTable> Rows;
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FLoadingTipTable* Row = reinterpret_cast<const FLoadingTipTable*>(Pair.Value);
		if (!Row || Row->Body.IsEmpty())
		{
			continue;
		}
		FLoadingTipTable Entry = *Row;
		if (Entry.TipKey.IsNone())
		{
			Entry.TipKey = Pair.Key;
		}
		Rows.Add(MoveTemp(Entry));
	}
	Rows.Sort([](const FLoadingTipTable& A, const FLoadingTipTable& B)
		{ return A.TipKey.LexicalLess(B.TipKey); });

	TestEqual(TEXT("본문 있는 행 수가 CSV 저작분과 같다"), Rows.Num(), 32);
	for (const FLoadingTipTable& Row : Rows)
	{
		if (Row.TipKey.IsNone())
		{
			AddError(TEXT("TipKey 가 비어 있는 행이 있다 ― 직전 팁 회피가 동작하지 않는다"));
		}
		const FString Body = Row.Body.ToString();
		if (Body.Len() > FLoadingTipRules::MaxBodyChars)
		{
			AddError(FString::Printf(TEXT("%s: 임포트된 Body %d자 > 상한 %d자"),
				*Row.TipKey.ToString(), Body.Len(), FLoadingTipRules::MaxBodyChars));
		}
	}

	// 전환이 도달할 수 있는 모든 컨텍스트에서 팁이 하나는 나와야 한다 ― 빈 밴드가 뜨면 안 된다
	const ELoadingTipContext Contexts[] = { ELoadingTipContext::Any, ELoadingTipContext::City,
		ELoadingTipContext::Office, ELoadingTipContext::WorldMap, ELoadingTipContext::Recruitment };
	const UEnum* ContextEnum = StaticEnum<ELoadingTipContext>();
	for (const ELoadingTipContext Context : Contexts)
	{
		const FString Name = ContextEnum
			? ContextEnum->GetNameStringByValue(static_cast<int64>(Context)) : TEXT("?");
		FName LastKey = NAME_None;
		int32 Repeats = 0;
		// 같은 컨텍스트로 연속 전환해도 직전 팁이 다시 나오지 않는지 32회 돌려본다
		for (uint32 Roll = 0; Roll < 32; ++Roll)
		{
			const int32 Picked = FLoadingTipRules::Pick(Rows, Context, LastKey, Roll * 7919u);
			if (!Rows.IsValidIndex(Picked))
			{
				AddError(FString::Printf(TEXT("%s 컨텍스트에서 뽑힌 팁이 없다 (roll %u)"), *Name, Roll));
				break;
			}
			if (Rows[Picked].TipKey == LastKey)
			{
				++Repeats;
			}
			LastKey = Rows[Picked].TipKey;
		}
		TestEqual(*FString::Printf(TEXT("%s: 직전 팁 연속 노출 0회"), *Name), Repeats, 0);
	}
	return true;
}

#endif
