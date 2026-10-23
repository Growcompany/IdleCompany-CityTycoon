#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Enum/WidgetType.h"
#include "Manager/TableManagerSubsystem.h"
#include "UObject/StrongObjectPtr.h"

// TableManagerSubsystem.h 의 friend(이름 고정) ― 뽑기 공개 시험이 필요한 표만 채운다.
// 전체 Initialize 는 80여 개 표를 읽어, 시험과 무관한 표의 Error/Warning 이 판정을 깬다
struct FGachaIconBoundsTableTestAccessor
{
	static void Initialize(UTableManagerSubsystem& Tables) { Tables.InitializeGachaIconBoundsTable(); }
};

struct FGachaBoxVisualTableTestAccessor
{
	static void Initialize(UTableManagerSubsystem& Tables, UDataTable* Fixture)
	{
		if (Fixture)
		{
			Tables.GachaBoxVisualDataTable = Fixture;
		}
		Tables.InitializeGachaBoxVisualTable();
	}

	static const UDataTable* LoadedTable(const UTableManagerSubsystem& Tables)
	{
		return Tables.GachaBoxVisualDataTable;
	}

	static void InitializeWidgets(UTableManagerSubsystem& Tables)
	{
		Tables.InitializeWidgetTable();
	}
};

namespace GachaRevealTestTables
{
	// 표 에셋은 생성자가 불러 둔다. 바탕 GameInstance 는 Init 하지 않는다 ― 다른 하위 시스템·세이브 없음
	// (InitializeStandalone 은 종료 때 다른 매니저가 저장해 사용자 세이브를 덮는다)
	inline TStrongObjectPtr<UTableManagerSubsystem> MakeWidgetTables()
	{
		TStrongObjectPtr<UTableManagerSubsystem> Tables(NewObject<UTableManagerSubsystem>(NewObject<UGameInstance>()));
		FGachaBoxVisualTableTestAccessor::InitializeWidgets(*Tables);
		return Tables;
	}

	// DT_WidgetClass·DT_GachaBoxVisual 만 채운 TableManager ― 상자 위젯이 읽는 동안 쥐고 있을 것
	inline TStrongObjectPtr<UTableManagerSubsystem> MakeBoxTables()
	{
		TStrongObjectPtr<UTableManagerSubsystem> Tables = MakeWidgetTables();
		FGachaBoxVisualTableTestAccessor::Initialize(*Tables, nullptr);
		FGachaIconBoundsTableTestAccessor::Initialize(*Tables);
		return Tables;
	}

	// 공개 화면과 같은 경로(TableManager::GetWidgetClass ← DT_WidgetClass 의 WidgetType 행)
	inline TSubclassOf<UUserWidget> RegisteredWidgetClass(EWidgetType Type)
	{
		return MakeWidgetTables()->GetWidgetClass(Type);
	}
}

#endif
