#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Office/WorkstationEquipmentText.h"

namespace
{
	/** DT_WorkstationSetupLevel 6행을 코드로 복원 — 표기 로직이 DT 에셋 없이 검증되도록 */
	FComputerSetupLevelData MakeSetupLevelRow(int32 Level)
	{
		FComputerSetupLevelData Row;
		switch (Level)
		{
		case 1: Row.LaptopPlacement = ELaptopPlacement::Center; Row.PrimaryMonitorType = EPrimaryMonitorType::None;
			break;
		case 2: Row.LaptopPlacement = ELaptopPlacement::Side; Row.PrimaryMonitorType = EPrimaryMonitorType::None;
			Row.bKeyboardActive = true; Row.bMouseActive = true; break;
		case 3: Row.LaptopPlacement = ELaptopPlacement::Side; Row.PrimaryMonitorType = EPrimaryMonitorType::Flat;
			Row.bKeyboardActive = true; Row.bMouseActive = true; break;
		case 4: Row.LaptopPlacement = ELaptopPlacement::Side; Row.PrimaryMonitorType = EPrimaryMonitorType::Flat;
			Row.bKeyboardActive = true; Row.bMouseActive = true; Row.bComputerActive = true; break;
		case 5: Row.LaptopPlacement = ELaptopPlacement::Side; Row.PrimaryMonitorType = EPrimaryMonitorType::Curved;
			Row.bKeyboardActive = true; Row.bMouseActive = true; Row.bComputerActive = true; break;
		case 6: Row.LaptopPlacement = ELaptopPlacement::None; Row.PrimaryMonitorType = EPrimaryMonitorType::Curved;
			Row.SecondMonitorType = ESecondMonitorType::Vertical;
			Row.bKeyboardActive = true; Row.bMouseActive = true; Row.bComputerActive = true; break;
		case 7: // Level6Twin — 승급 경로 밖, Lv6 의 다른 외형(커브드 듀얼)
			Row.LaptopPlacement = ELaptopPlacement::None; Row.PrimaryMonitorType = EPrimaryMonitorType::Curved;
			Row.SecondMonitorType = ESecondMonitorType::Curved;
			Row.bKeyboardActive = true; Row.bMouseActive = true; Row.bComputerActive = true; break;
		default: break;
		}
		return Row;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWorkstationEquipSummaryTest,
	"CGR.Workstation.EquipSummary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorkstationEquipSummaryTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Lv1 = 노트북 단독 (중앙 노트북은 책상 중앙을 통째로 쓴다)"),
		BuildWorkstationEquipSummary(MakeSetupLevelRow(1)).ToString(),
		FString(TEXT("노트북")));

	TestEqual(TEXT("Lv4 = 본체까지"),
		BuildWorkstationEquipSummary(MakeSetupLevelRow(4)).ToString(),
		FString(TEXT("노트북, 평면 모니터, 본체, 키보드, 마우스")));

	TestEqual(TEXT("Lv6 = 노트북이 빠지고 세로 모니터가 들어온다"),
		BuildWorkstationEquipSummary(MakeSetupLevelRow(6)).ToString(),
		FString(TEXT("커브드 모니터, 세로 모니터, 본체, 키보드, 마우스")));

	TestEqual(TEXT("배치(중앙/측면)는 요약에 나오지 않는다"),
		BuildWorkstationEquipSummary(MakeSetupLevelRow(2)).ToString(),
		FString(TEXT("노트북, 키보드, 마우스")));

	TestEqual(TEXT("커브드 듀얼은 같은 이름을 두 번 읽히지 않고 개수로 묶는다"),
		BuildWorkstationEquipSummary(MakeSetupLevelRow(7)).ToString(),
		FString(TEXT("커브드 모니터 2대, 본체, 키보드, 마우스")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWorkstationEquipDeltaTest,
	"CGR.Workstation.EquipDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorkstationEquipDeltaTest::RunTest(const FString& Parameters)
{
	{	// Lv1 → 2 : 노트북이 옆으로 비키고 키보드·마우스가 깔린다 — 한꺼번에 붙는 품목은 전부 말한다
		const FComputerSetupLevelData Cur = MakeSetupLevelRow(1), Next = MakeSetupLevelRow(2);
		const FWorkstationEquipDelta D = BuildWorkstationEquipDelta(Cur, &Next);
		TestEqual(TEXT("Kind=Added"), D.Kind, EWorkstationEquipDeltaKind::Added);
		TestEqual(TEXT("아이콘은 첫 품목"), D.Item, EWorkstationEquipItem::Keyboard);
		TestEqual(TEXT("이름이 새 품목 전부"), D.ItemName.ToString(), FString(TEXT("키보드, 마우스")));
		TestEqual(TEXT("부연이 노트북 이동"), D.Detail.ToString(),
			FString(TEXT("노트북이 옆으로 옮겨집니다")));
	}
	{	// Lv2 → 3 : 평면 모니터 추가 (노트북은 이미 측면이라 이동 없음)
		const FComputerSetupLevelData Cur = MakeSetupLevelRow(2), Next = MakeSetupLevelRow(3);
		const FWorkstationEquipDelta D = BuildWorkstationEquipDelta(Cur, &Next);
		TestEqual(TEXT("Kind=Added"), D.Kind, EWorkstationEquipDeltaKind::Added);
		TestEqual(TEXT("Item=MonitorFlat"), D.Item, EWorkstationEquipItem::MonitorFlat);
		TestEqual(TEXT("부연"), D.Detail.ToString(), FString(TEXT("책상 위에 놓입니다")));
	}
	{	// Lv3 → 4 : 본체 추가
		const FComputerSetupLevelData Cur = MakeSetupLevelRow(3), Next = MakeSetupLevelRow(4);
		const FWorkstationEquipDelta D = BuildWorkstationEquipDelta(Cur, &Next);
		TestEqual(TEXT("Kind=Added"), D.Kind, EWorkstationEquipDeltaKind::Added);
		TestEqual(TEXT("Item=Tower"), D.Item, EWorkstationEquipItem::Tower);
		TestEqual(TEXT("이름"), D.ItemName.ToString(), FString(TEXT("본체")));
		TestEqual(TEXT("부연"), D.Detail.ToString(), FString(TEXT("책상 위에 놓입니다")));
	}
	{	// Lv4 → 5 : 교체
		const FComputerSetupLevelData Cur = MakeSetupLevelRow(4), Next = MakeSetupLevelRow(5);
		const FWorkstationEquipDelta D = BuildWorkstationEquipDelta(Cur, &Next);
		TestEqual(TEXT("Kind=Replaced"), D.Kind, EWorkstationEquipDeltaKind::Replaced);
		TestEqual(TEXT("Item=MonitorCurved"), D.Item, EWorkstationEquipItem::MonitorCurved);
		TestEqual(TEXT("이름"), D.ItemName.ToString(), FString(TEXT("커브드 모니터")));
		TestEqual(TEXT("부연이 이전 품목을 지목"), D.Detail.ToString(),
			FString(TEXT("평면 모니터를 대신합니다")));
	}
	{	// Lv5 → 6 : 세로 모니터가 옆자리를 가져가고 노트북이 치워진다
		const FComputerSetupLevelData Cur = MakeSetupLevelRow(5), Next = MakeSetupLevelRow(6);
		const FWorkstationEquipDelta D = BuildWorkstationEquipDelta(Cur, &Next);
		TestEqual(TEXT("Item=MonitorVertical"), D.Item, EWorkstationEquipItem::MonitorVertical);
		// 노트북↔세로 모니터는 모니터끼리의 교체가 아니다 — 빠지는 쪽을 부연으로 말하고 종류는 추가로 둔다
		TestEqual(TEXT("Kind=Added"), D.Kind, EWorkstationEquipDeltaKind::Added);
		TestEqual(TEXT("이름"), D.ItemName.ToString(), FString(TEXT("세로 모니터")));
		TestEqual(TEXT("부연이 노트북 제거"), D.Detail.ToString(),
			FString(TEXT("노트북이 빠집니다")));
	}
	{	// Lv6 → Lv6Twin : 외형 변경 — 두 번째 모니터가 세로에서 커브드로 교체된다
		const FComputerSetupLevelData Cur = MakeSetupLevelRow(6), Next = MakeSetupLevelRow(7);
		const FWorkstationEquipDelta D = BuildWorkstationEquipDelta(Cur, &Next);
		TestEqual(TEXT("Kind=Replaced"), D.Kind, EWorkstationEquipDeltaKind::Replaced);
		TestEqual(TEXT("Item=MonitorCurved"), D.Item, EWorkstationEquipItem::MonitorCurved);
		TestEqual(TEXT("부연이 이전 두 번째 모니터를 지목"), D.Detail.ToString(),
			FString(TEXT("세로 모니터를 대신합니다")));
	}
	{	// 변형 행이 없을 때만 최대 문구 (GetMaxVariantData 실패 = 데이터 결손)
		const FComputerSetupLevelData Cur = MakeSetupLevelRow(6);
		const FWorkstationEquipDelta D = BuildWorkstationEquipDelta(Cur, nullptr);
		TestEqual(TEXT("Kind=None"), D.Kind, EWorkstationEquipDeltaKind::None);
		TestTrue(TEXT("품목명 없음"), D.ItemName.IsEmpty());
		TestEqual(TEXT("최대 문구"), D.Detail.ToString(), FString(TEXT("모든 장비를 갖췄습니다")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
