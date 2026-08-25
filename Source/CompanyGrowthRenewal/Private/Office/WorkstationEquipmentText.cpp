#include "Office/WorkstationEquipmentText.h"

#define LOCTEXT_NAMESPACE "WorkstationEquipment"

namespace
{
	EWorkstationEquipItem MonitorItemOf(EPrimaryMonitorType Type)
	{
		switch (Type)
		{
		case EPrimaryMonitorType::Flat:   return EWorkstationEquipItem::MonitorFlat;
		case EPrimaryMonitorType::Curved: return EWorkstationEquipItem::MonitorCurved;
		default:                          return EWorkstationEquipItem::None;
		}
	}

	EWorkstationEquipItem SecondMonitorItemOf(ESecondMonitorType Type)
	{
		switch (Type)
		{
		case ESecondMonitorType::Vertical: return EWorkstationEquipItem::MonitorVertical;
		case ESecondMonitorType::Curved:   return EWorkstationEquipItem::MonitorCurved;
		default:                           return EWorkstationEquipItem::None;
		}
	}

	bool IsMonitor(EWorkstationEquipItem Item)
	{
		return Item == EWorkstationEquipItem::MonitorFlat
			|| Item == EWorkstationEquipItem::MonitorCurved
			|| Item == EWorkstationEquipItem::MonitorVertical;
	}

	/** 다중집합 차집합 — 같은 품목이 두 개면 두 개만 상쇄된다(커브드 듀얼에서 한 대만 새 것). */
	TArray<EWorkstationEquipItem> SubtractItems(
		const TArray<EWorkstationEquipItem>& From, const TArray<EWorkstationEquipItem>& Sub)
	{
		TArray<EWorkstationEquipItem> Remaining = Sub;
		TArray<EWorkstationEquipItem> Result;
		for (EWorkstationEquipItem Item : From)
		{
			const int32 Found = Remaining.Find(Item);
			if (Found != INDEX_NONE) { Remaining.RemoveAt(Found); }
			else                     { Result.Add(Item); }
		}
		return Result;
	}

	FText JoinItemNames(const TArray<EWorkstationEquipItem>& Items)
	{
		TArray<FString> Names;
		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			// 커브드 듀얼처럼 같은 품목이 연달아 나오면 이름을 두 번 읽히지 않게 개수로 묶는다
			if (Items.IsValidIndex(Index + 1) && Items[Index] == Items[Index + 1])
			{
				Names.Add(FString::Printf(TEXT("%s 2대"), *GetWorkstationEquipItemName(Items[Index]).ToString()));
				++Index;
				continue;
			}
			Names.Add(GetWorkstationEquipItemName(Items[Index]).ToString());
		}
		return FText::FromString(FString::Join(Names, TEXT(", ")));
	}
}

FText GetWorkstationEquipItemName(EWorkstationEquipItem Item)
{
	switch (Item)
	{
	case EWorkstationEquipItem::Laptop:          return LOCTEXT("Laptop", "노트북");
	case EWorkstationEquipItem::MonitorFlat:     return LOCTEXT("MonitorFlat", "평면 모니터");
	case EWorkstationEquipItem::MonitorCurved:   return LOCTEXT("MonitorCurved", "커브드 모니터");
	case EWorkstationEquipItem::MonitorVertical: return LOCTEXT("MonitorVertical", "세로 모니터");
	case EWorkstationEquipItem::Tower:           return LOCTEXT("Tower", "본체");
	case EWorkstationEquipItem::Keyboard:        return LOCTEXT("Keyboard", "키보드");
	case EWorkstationEquipItem::Mouse:           return LOCTEXT("Mouse", "마우스");
	default:                                     return FText::GetEmpty();
	}
}

TArray<EWorkstationEquipItem> GetWorkstationEquipItems(const FComputerSetupLevelData& Row)
{
	TArray<EWorkstationEquipItem> Items;

	if (Row.LaptopPlacement != ELaptopPlacement::None) { Items.Add(EWorkstationEquipItem::Laptop); }

	const EWorkstationEquipItem Monitor = MonitorItemOf(Row.PrimaryMonitorType);
	if (Monitor != EWorkstationEquipItem::None) { Items.Add(Monitor); }

	const EWorkstationEquipItem SecondMonitor = SecondMonitorItemOf(Row.SecondMonitorType);
	if (SecondMonitor != EWorkstationEquipItem::None) { Items.Add(SecondMonitor); }

	if (Row.bComputerActive)        { Items.Add(EWorkstationEquipItem::Tower); }
	if (Row.bKeyboardActive)        { Items.Add(EWorkstationEquipItem::Keyboard); }
	if (Row.bMouseActive)           { Items.Add(EWorkstationEquipItem::Mouse); }

	return Items;
}

FText BuildWorkstationEquipSummary(const FComputerSetupLevelData& Row)
{
	return JoinItemNames(GetWorkstationEquipItems(Row));
}

FWorkstationEquipDelta BuildWorkstationEquipDelta(
	const FComputerSetupLevelData& CurrentRow, const FComputerSetupLevelData* NextRow)
{
	FWorkstationEquipDelta Delta;

	if (!NextRow)
	{
		Delta.Detail = LOCTEXT("EquipMaxed", "모든 장비를 갖췄습니다");
		return Delta;
	}

	// 노트북 배치 변화는 품목이 아니라 부연이다 — 위치는 상태가 아니라 변화의 순간에만 의미가 있다
	const bool bLaptopMoved =
		CurrentRow.LaptopPlacement != NextRow->LaptopPlacement &&
		NextRow->LaptopPlacement != ELaptopPlacement::None;

	// 무엇이 새로 생기는지는 두 행의 품목 목록 차이가 곧 답이다 — 품목별 if 사다리를 두면
	// Lv1→2 처럼 둘이 한꺼번에 붙는 단계에서 한 품목만 말하고 나머지를 삼킨다
	const TArray<EWorkstationEquipItem> CurrentItems = GetWorkstationEquipItems(CurrentRow);
	const TArray<EWorkstationEquipItem> NextItems    = GetWorkstationEquipItems(*NextRow);
	const TArray<EWorkstationEquipItem> AddedItems   = SubtractItems(NextItems, CurrentItems);
	const TArray<EWorkstationEquipItem> RemovedItems = SubtractItems(CurrentItems, NextItems);

	if (AddedItems.Num() == 0)
	{
		Delta.Detail = RemovedItems.Num() > 0
			? FText::Format(LOCTEXT("EquipRemoved", "{0}|hpp(이,가) 빠집니다"), JoinItemNames(RemovedItems))
			// 품목 변화 없이 배치만 바뀌는 행이 생기면 여기로 온다. 조용히 비우지 말고 드러낸다.
			: LOCTEXT("EquipNoChange", "장비 구성은 그대로입니다");
		return Delta;
	}

	Delta.Item = AddedItems[0];
	Delta.ItemName = JoinItemNames(AddedItems);

	// 모니터가 모니터를 밀어내면 추가가 아니라 교체다 — 자리 수는 그대로고 주인만 바뀐다
	const EWorkstationEquipItem* ReplacedMonitor = RemovedItems.FindByPredicate(IsMonitor);
	const EWorkstationEquipItem* AddedMonitor    = AddedItems.FindByPredicate(IsMonitor);
	if (ReplacedMonitor && AddedMonitor)
	{
		Delta.Kind = EWorkstationEquipDeltaKind::Replaced;
		Delta.Item = *AddedMonitor;
		// 을/를 판정은 엔진 hpp 수식어에 맡긴다 — 받침 계산에 숫자로 끝나는 품목명까지 포함된다
		Delta.Detail = FText::Format(
			LOCTEXT("EquipReplaces", "{0}|hpp(을,를) 대신합니다"),
			GetWorkstationEquipItemName(*ReplacedMonitor));
		return Delta;
	}

	Delta.Kind = EWorkstationEquipDeltaKind::Added;
	// 새 품목이 들어오면서 무언가 빠지는 단계 — 사라지는 걸 말하지 않으면 플레이어가 손해로 읽는다
	Delta.Detail = RemovedItems.Num() > 0
		? FText::Format(LOCTEXT("EquipRemoved", "{0}|hpp(이,가) 빠집니다"), JoinItemNames(RemovedItems))
		: bLaptopMoved
			? LOCTEXT("EquipLaptopMoved", "노트북이 옆으로 옮겨집니다")
			: LOCTEXT("EquipPlaced", "책상 위에 놓입니다");

	return Delta;
}

#undef LOCTEXT_NAMESPACE
