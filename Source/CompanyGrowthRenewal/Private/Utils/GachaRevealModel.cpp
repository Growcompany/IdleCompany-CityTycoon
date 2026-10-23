#include "Utils/GachaRevealModel.h"

int32 GachaRevealModel::FindTopIndex(const TArray<FGachaRevealEntry>& Entries)
{
	int32 Top = INDEX_NONE;
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FGachaRevealEntry& E = Entries[i];
		if (E.Grade < HighGrade) { continue; }
		if (Top == INDEX_NONE || E.Grade > Entries[Top].Grade || (E.Grade == Entries[Top].Grade && E.PullIndex < Entries[Top].PullIndex))
		{
			Top = i;
		}
	}
	return Top;
}

TArray<TPair<int32, TArray<int32>>> GachaRevealModel::BuildStampGroups(const TArray<FGachaRevealEntry>& Entries, int32 TopIndex)
{
	TArray<int32> Grades;
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		if (i != TopIndex && Entries[i].Grade >= HighGrade) { Grades.AddUnique(Entries[i].Grade); }
	}
	Grades.Sort();

	TArray<TPair<int32, TArray<int32>>> Groups;
	for (const int32 G : Grades)
	{
		TArray<int32> Members;
		for (int32 i = 0; i < Entries.Num(); ++i)
		{
			if (i != TopIndex && Entries[i].Grade == G) { Members.Add(i); }
		}
		Members.StableSort([&Entries](int32 A, int32 B) { return Entries[A].PullIndex < Entries[B].PullIndex; });
		Groups.Add(TPair<int32, TArray<int32>>(G, MoveTemp(Members)));
	}
	return Groups;
}

TArray<int32> GachaRevealModel::UpgradeSteps(int32 BestGrade)
{
	if (BestGrade <= 2) { return TArray<int32>(); }
	if (BestGrade == 3) { return TArray<int32>{2, 3}; }
	if (BestGrade == 4) { return TArray<int32>{3, 4}; }
	return TArray<int32>{3, 4, 5};
}

TArray<int32> GachaRevealModel::SortedOrder(const TArray<FGachaRevealEntry>& Entries, EGachaRevealSort SortMode, const TSet<int32>& Recommended)
{
	TArray<int32> Order;
	Order.Reserve(Entries.Num());
	for (int32 i = 0; i < Entries.Num(); ++i) { Order.Add(i); }

	if (SortMode == EGachaRevealSort::PullOrder)
	{
		Order.StableSort([&Entries](int32 A, int32 B) { return Entries[A].PullIndex < Entries[B].PullIndex; });
		return Order;
	}

	Order.StableSort([&Entries, &Recommended](int32 A, int32 B)
	{
		const FGachaRevealEntry& EA = Entries[A];
		const FGachaRevealEntry& EB = Entries[B];
		if (EA.Grade != EB.Grade) { return EA.Grade > EB.Grade; }
		if (EA.bNew != EB.bNew) { return EA.bNew; }
		const bool bRecA = Recommended.Contains(A);
		const bool bRecB = Recommended.Contains(B);
		if (bRecA != bRecB) { return bRecA; }
		return EA.PullIndex < EB.PullIndex;
	});
	return Order;
}

TArray<int32> GachaRevealModel::PickRecommended(const TArray<FGachaRevealEntry>& Entries)
{
	TMap<int32, int32> BestBySlot;
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FGachaRevealEntry& E = Entries[i];
		if (!E.bEmployeeKnown || E.SlotKey == INDEX_NONE || E.Grade <= E.EquippedGrade) { continue; }
		int32* Best = BestBySlot.Find(E.SlotKey);
		if (!Best)
		{
			BestBySlot.Add(E.SlotKey, i);
		}
		else if (E.Grade > Entries[*Best].Grade || (E.Grade == Entries[*Best].Grade && E.PullIndex < Entries[*Best].PullIndex))
		{
			*Best = i;
		}
	}

	TArray<int32> Picked;
	BestBySlot.GenerateValueArray(Picked);
	Picked.Sort();
	return Picked;
}

TArray<FGachaRevealRect> GachaRevealModel::ComputeSlots(int32 Count, float CanvasWidth, bool bSettled)
{
	TArray<FGachaRevealRect> Out;
	if (Count <= 0) { return Out; }

	// 다회 배치의 floor/round 순서는 기존 그리드 계산을 따른다.
	constexpr float GridTop = 270.f;
	constexpr float GridBottom = 1258.f;
	float X0 = 100.f;
	const float X1 = CanvasWidth - 100.f;

	if (Count == 1)
	{
		constexpr float SingleSize = 200.f;
		const float SingleX = bSettled ? CanvasWidth / 2.f - 340.f : CanvasWidth / 2.f - SingleSize / 2.f;
		Out.Add(FGachaRevealRect{SingleX, 460.f, SingleSize, SingleSize});
		return Out;
	}

	// 시안은 1·10·30만 다룬다. 일부만 지급된 장수는 그 등급(2..10 / 11..30)의 배치를 쓴다.
	const bool bTen = Count <= 10;
	const float BestColumnW = bTen ? 470.f : 430.f;
	if (bSettled) { X0 += BestColumnW + 50.f; }
	const int32 Cols = bTen ? 5 : 8;
	const int32 Rows = FMath::DivideAndRoundUp(Count, Cols);
	const float Gap = bTen ? 26.f : 16.f;
	const float Ratio = bTen ? 1.2f : 1.f;
	const float AvailW = X1 - X0;
	const float AvailH = GridBottom - GridTop;
	const float CardW = FMath::FloorToFloat(FMath::Min3(
		bTen ? 290.f : 224.f,
		(AvailW - Gap * static_cast<float>(Cols - 1)) / static_cast<float>(Cols),
		(AvailH - Gap * static_cast<float>(Rows - 1)) / static_cast<float>(Rows) / Ratio));
	const float CardH = FMath::RoundToFloat(CardW * Ratio);
	const float GridH = static_cast<float>(Rows) * CardH + static_cast<float>(Rows - 1) * Gap;
	const float Y0 = GridTop + (AvailH - GridH) / 2.f;

	Out.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 RowIdx = i / Cols;
		const int32 ColIdx = i % Cols;
		const int32 InRow = RowIdx == Rows - 1 ? Count - RowIdx * Cols : Cols;
		const float RowW = static_cast<float>(InRow) * CardW + static_cast<float>(InRow - 1) * Gap;
		Out.Add(FGachaRevealRect{
			X0 + (AvailW - RowW) / 2.f + static_cast<float>(ColIdx) * (CardW + Gap),
			Y0 + static_cast<float>(RowIdx) * (CardH + Gap),
			CardW,
			CardH});
	}
	return Out;
}

TArray<int32> GachaRevealModel::TallyGrades(const TArray<int32>& Weights, const TArray<FGachaRevealEntry>& Entries)
{
	TArray<int32> Shown;
	for (int32 G = MaxGrade; G >= 0; --G)
	{
		const bool bWeighted = Weights.IsValidIndex(G) && Weights[G] > 0;
		const bool bRolled = Entries.ContainsByPredicate([G](const FGachaRevealEntry& E) { return E.Grade == G; });
		if (bWeighted || bRolled) { Shown.Add(G); }
	}
	return Shown;
}
