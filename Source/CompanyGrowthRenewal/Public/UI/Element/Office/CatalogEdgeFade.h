#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TileView.h"
#include "Engine/World.h"
#include "TimerManager.h"

/**
 * 가로 카탈로그 목록 오른쪽 끝의 흐림. 뒤에 카드가 더 남았을 때만 켠다 ―
 * 끝까지 넘긴 뒤에도 남아 있으면 마지막 카드를 덮는다.
 * 목록이 아직 배치 전(폭 0)이면 판정하지 않고 false 를 돌려준다.
 */
inline bool UpdateCatalogEdgeFade(const UTileView* List, UWidget* Fade, float ItemOffset)
{
	if (!List || !Fade) return true;
	const float Width = List->GetCachedGeometry().GetLocalSize().X;
	if (Width <= 0.f) return false;
	// 한 줄짜리 가로 타일뷰라 스크롤 오프셋 1 = 카드 한 칸 폭
	const float Remaining = (List->GetNumItems() - ItemOffset) * List->GetEntryWidth() - Width;
	Fade->SetVisibility(Remaining > 1.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	return true;
}

// 스크롤 이벤트는 레이아웃 갱신 전에 오므로 남은 거리는 스크롤바가 아니라 오프셋으로 계산한다.
inline void BindCatalogEdgeFade(UUserWidget* Owner, UTileView* List, UWidget* Fade)
{
	if (!Owner || !List || !Fade) return;
	List->OnListViewScrolled().AddWeakLambda(Owner, [List, Fade](float ItemOffset, float)
	{
		UpdateCatalogEdgeFade(List, Fade, ItemOffset);
	});
}

// 항목을 다시 채운 직후에 부른다. 배치가 끝난 다음 틱에 판정하고, 아직 배치 전이면 틱마다 다시 본다.
inline void ScheduleCatalogEdgeFade(UUserWidget* Owner, UTileView* List, UWidget* Fade, int32 TicksLeft = 60)
{
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || !List || !Fade) return;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Owner, [Owner, List, Fade, TicksLeft]()
	{
		if (!UpdateCatalogEdgeFade(List, Fade, List->GetScrollOffset()) && TicksLeft > 1)
		{
			ScheduleCatalogEdgeFade(Owner, List, Fade, TicksLeft - 1);
		}
	}));
}
