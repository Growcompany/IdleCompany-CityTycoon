#pragma once

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"

// 공용 틀로 옮긴 위젯은 패널 트리가 아니라 부품 트리에 있다 ― 패널에서 못 찾으면 자식 유저 위젯 트리까지 내려간다
inline UWidget* FindWidgetDeep(UUserWidget* Root, FName Name, int32 Depth = 0)
{
	if (!Root || Depth > 4) { return nullptr; }
	if (UWidget* Found = Root->GetWidgetFromName(Name)) { return Found; }
	UWidget* Result = nullptr;
	if (Root->WidgetTree)
	{
		Root->WidgetTree->ForEachWidget([&](UWidget* Child)
		{
			if (!Result)
			{
				if (UUserWidget* Nested = Cast<UUserWidget>(Child)) { Result = FindWidgetDeep(Nested, Name, Depth + 1); }
			}
		});
	}
	return Result;
}
