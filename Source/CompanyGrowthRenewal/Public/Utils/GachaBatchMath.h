#pragma once

#include "CoreMinimal.h"

// 멀티 뽑기 순수 계산 — 월드 의존 0(자동화 테스트 대상). 소비처: 패널 라벨·배치 API·연출 슬롯.
namespace GachaBatchMath
{
	constexpr int32 MaxBatchPull = 5;

	// 티켓·정원·상한 3중 클램프. 음수 입력은 0으로 방어.
	inline int32 ComputeBatchPullCount(int32 TicketCount, int32 FreeCapacity, int32 MaxCount = MaxBatchPull)
	{
		const int32 N = FMath::Min3(FMath::Max(TicketCount, 0), FMath::Max(FreeCapacity, 0), FMath::Max(MaxCount, 0));
		return N;
	}

	// 표시 정렬 — 등급 내림차순, 동률=뽑기순(stable). 반환 = 입력 인덱스 배열(슬롯 C,R1,L1,R2,L2 순서).
	inline TArray<int32> ComputeDisplayOrder(const TArray<int32>& RarityTiers)
	{
		TArray<int32> Order;
		Order.Reserve(RarityTiers.Num());
		for (int32 i = 0; i < RarityTiers.Num(); ++i) { Order.Add(i); }
		Order.StableSort([&RarityTiers](int32 A, int32 B) { return RarityTiers[A] > RarityTiers[B]; });
		return Order;
	}

	// 슬롯 X 오프셋(px). SlotIndex: 0=C, 1=R1, 2=L1, 3=R2, 4=L2 (스펙 §2 교대 배치)
	inline float SlotXOffset(int32 SlotIndex, float CardW, float SideScale, float GapPx)
	{
		if (SlotIndex <= 0) { return 0.f; }
		const float SideW = CardW * SideScale;
		const float X1 = CardW * 0.5f + GapPx + SideW * 0.5f;
		const float X2 = X1 + SideW + GapPx;
		switch (SlotIndex)
		{
		case 1: return  X1;
		case 2: return -X1;
		case 3: return  X2;
		default: return -X2;
		}
	}
}
