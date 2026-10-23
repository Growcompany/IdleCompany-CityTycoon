// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ListView.h"

/**
 * UListView 를 쓰는 창들의 공용 배선 — 엔트리 재바인딩 + 아이템 풀 동기화.
 * 라이브옵스 창 4개가 같은 코드를 복제하지 않도록 뺐다(플랜 D 수정안 G12).
 */
namespace CGListEntry
{
	/** 엔트리 위젯은 풀에서 재사용된다 — 생성 방송마다 전체를 다시 걸어야 중복/유령 바인딩이 안 남는다. */
	template <class TEntry>
	void RebindGeneratedEntries(UListView* InListView, TFunctionRef<void(TEntry&)> Rebind)
	{
		if (!InListView)
		{
			return;
		}

		for (UUserWidget* Entry : InListView->GetDisplayedEntryWidgets())
		{
			if (TEntry* Typed = Cast<TEntry>(Entry))
			{
				Rebind(*Typed);
			}
		}
	}

	/** 아이템 UObject 를 Num 개로 맞추고(부족분만 새로 만든다) 리스트에 다시 넣는다. */
	template <class TItem>
	void SyncItems(TArray<TObjectPtr<TItem>>& Items, int32 Num, UObject* ItemOuter, UListView* InListView)
	{
		if (!ItemOuter)
		{
			return;
		}

		// 아이템은 매 갱신마다 버리지 않고 재사용한다 — 창을 여는 동안 GC 부담이 없다
		Items.SetNum(FMath::Max(0, Num));
		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			if (!Items[Index])
			{
				Items[Index] = NewObject<TItem>(ItemOuter);
			}
		}

		if (!InListView)
		{
			return;
		}

		InListView->ClearListItems();
		for (TItem* Item : Items)
		{
			InListView->AddItem(Item);
		}
	}
}
