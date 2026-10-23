#include "Entity/OverseasDiorama/DioramaModules.h"

namespace
{
	constexpr double kDioLayoutGap = 0.6;
	// 항구가 있으면 옆 생울타리는 안벽 앞 끝(항구 원점 + 1.7m)에서 멈춘다.
	constexpr double kDioLayoutQuayFront = 1.7;

	FBox2D DioLayoutRect(const FOverseasDioramaSpec& Spec, EDioramaModule Module, bool bMiniature)
	{
		switch (Module)
		{
		case EDioramaModule::Factory: return DioramaModules::FactoryRect(Spec.Factory, bMiniature);
		case EDioramaModule::Mine: return DioramaModules::MineRect(Spec.Mine);
		case EDioramaModule::Port: return DioramaModules::PortRect();
		}
		return FBox2D(ForceInit);
	}

	FDioramaLayoutItem DioLayoutItem(EDioramaModule Module, const FBox2D& Rect, double OffsetZ)
	{
		FDioramaLayoutItem Item;
		Item.Module = Module;
		Item.RectXZ = Rect;
		Item.OffsetXZ = FVector2D(0.0, OffsetZ);
		return Item;
	}

	FColor DioLayoutColor(uint32 RGB)
	{
		return DioramaModules::Hex(RGB);
	}
}

TArray<FDioramaLayoutItem> DioramaModules::Layout(const FOverseasDioramaSpec& Spec, bool bMiniature)
{
	TArray<FDioramaLayoutItem> Items;
	if (!Spec.HasAny())
	{
		return Items;
	}

	const EDioramaModule Primary = Spec.PrimaryModule();
	const FBox2D PrimaryRect = DioLayoutRect(Spec, Primary, bMiniature);
	Items.Add(DioLayoutItem(Primary, PrimaryRect, 0.0));

	if (Spec.HasModule(EDioramaModule::Factory) && Spec.HasModule(EDioramaModule::Mine))
	{
		const FBox2D MineBox = MineRect(Spec.Mine);
		Items.Add(DioLayoutItem(EDioramaModule::Mine, MineBox, PrimaryRect.Min.Y - kDioLayoutGap - MineBox.Max.Y));
	}
	if (Spec.HasModule(EDioramaModule::Port) && Primary != EDioramaModule::Port)
	{
		const FBox2D PortBox = PortRect();
		Items.Add(DioLayoutItem(EDioramaModule::Port, PortBox, PrimaryRect.Max.Y + kDioLayoutGap - PortBox.Min.Y));
	}
	return Items;
}

FBox2D DioramaModules::BuildTile(const TArray<FDioramaLayoutItem>& Items, FDioramaGeometry& Out)
{
	if (Items.IsEmpty())
	{
		return FBox2D(ForceInit);
	}

	double X0 = TNumericLimits<double>::Max();
	double X1 = TNumericLimits<double>::Lowest();
	double Z0 = TNumericLimits<double>::Max();
	double Z1 = TNumericLimits<double>::Lowest();
	const FDioramaLayoutItem* PortItem = nullptr;
	for (const FDioramaLayoutItem& Item : Items)
	{
		X0 = FMath::Min(X0, Item.OffsetXZ.X + Item.RectXZ.Min.X);
		X1 = FMath::Max(X1, Item.OffsetXZ.X + Item.RectXZ.Max.X);
		Z0 = FMath::Min(Z0, Item.OffsetXZ.Y + Item.RectXZ.Min.Y);
		Z1 = FMath::Max(Z1, Item.OffsetXZ.Y + Item.RectXZ.Max.Y);
		if (Item.Module == EDioramaModule::Port)
		{
			PortItem = &Item;
		}
	}

	const double W = X1 - X0;
	const double D = Z1 - Z0;
	const double CX = (X0 + X1) * 0.5;
	const double CZ = (Z0 + Z1) * 0.5;
	Out.BoxB(W, 0.3, D, DioLayoutColor(DioramaPalette::Paving), CX, -0.3, CZ);
	Out.BoxB(W, 1.2, D, DioLayoutColor(DioramaPalette::GoldTop), CX, -1.5, CZ);
	Out.BoxB(W - 1.4, 1.4, D - 1.4, DioLayoutColor(DioramaPalette::GoldMid), CX, -2.9, CZ);
	Out.BoxB(W - 3.0, 1.2, D - 3.0, DioLayoutColor(DioramaPalette::GoldLow), CX, -4.1, CZ);

	const FColor HedgeColor = DioLayoutColor(DioramaPalette::Hedge);
	const double HedgeZ = PortItem ? PortItem->OffsetXZ.Y + kDioLayoutQuayFront : Z1;
	Out.BoxB(W, 0.45, 0.6, HedgeColor, CX, 0, Z0 + 0.3);
	Out.BoxB(0.6, 0.45, HedgeZ - Z0, HedgeColor, X0 + 0.3, 0, (Z0 + HedgeZ) * 0.5);
	Out.BoxB(0.6, 0.45, HedgeZ - Z0, HedgeColor, X1 - 0.3, 0, (Z0 + HedgeZ) * 0.5);
	if (!PortItem)
	{
		Out.BoxB(W, 0.45, 0.6, HedgeColor, CX, 0, Z1 - 0.3);
	}

	// 시안의 반투명 바닥 그림자 판은 뺀다 ― 큰 반투명 평면은 모바일(ES3.1)에서 그려지지 않는다.
	return FBox2D(FVector2D(X0, Z0), FVector2D(X1, Z1));
}
