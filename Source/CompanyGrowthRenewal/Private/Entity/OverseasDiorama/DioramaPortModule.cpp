#include "Entity/OverseasDiorama/DioramaModules.h"

namespace
{
	constexpr uint32 kDioPortWhite = 0xEDEFF2;
	constexpr uint32 kDioPortCable = 0x2B2F36;
	constexpr uint32 kDioPortHullStripe = 0xA84438;
	constexpr uint32 kDioPortBridgeBand = 0x2F3A48;

	constexpr int32 kDioPortMaxCranes = 6;
	constexpr double kDioPortTrolleyY = 6.9;
	constexpr double kDioPortYardZ = -4.4;
	constexpr double kDioPortShipZ = 7.2;
	// 이동 중 케이블 길이(시안 stepPort len = 0.9 + drop·3.6, drop = 0)
	constexpr double kDioPortCableRest = 0.9;
	constexpr double kDioPortTrolleyPeriod = 1.0 / 0.11;
	constexpr double kDioPortShipBobPeriod = UE_DOUBLE_TWO_PI / 1.3;
	constexpr float kDioPortShipBobM = 0.15f;

	constexpr int32 kDioPortYardColumns = 8;
	constexpr int32 kDioPortShipColumns = 7;
	constexpr int32 kDioPortShipSlots = 2 * 2 * kDioPortShipColumns;
	// 게임에 배 적재 상태가 아직 없어 정박 화물은 절반(아래 단 전부)으로 고정한다.
	constexpr int32 kDioPortShipLoaded = kDioPortShipSlots / 2;

	FColor DioPortColor(uint32 RGB)
	{
		return DioramaModules::Hex(RGB);
	}

	FColor DioPortContainer(int32 Index)
	{
		constexpr int32 ColorCount = static_cast<int32>(UE_ARRAY_COUNT(DioramaPalette::Containers));
		return DioPortColor(DioramaPalette::Containers[Index % ColorCount]);
	}

	TArray<double> DioPortCraneXs(int32 Count)
	{
		if (Count == 3)
		{
			return TArray<double>{ -12.0, 0.0, 12.0 };
		}
		if (Count == 2)
		{
			return TArray<double>{ -7.0, 7.0 };
		}
		// 시안에 없는 개수는 3기 간격(12m)으로 가운데 정렬하되 안벽(±18m) 안에 들어오게 좁힌다.
		const double Spacing = Count > 1 ? FMath::Min(12.0, 36.0 / (Count - 1)) : 0.0;
		TArray<double> Xs;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Xs.Add((Index - (Count - 1) * 0.5) * Spacing);
		}
		return Xs;
	}

	void DioPortQuayAndWater(FDioramaGeometry& G)
	{
		G.BoxB(44, 0.4, 1.6, DioPortColor(DioramaPalette::Quay), 0, 0, 0.9);
		for (int32 BollardX = -20; BollardX <= 20; BollardX += 5)
		{
			G.CylB(0.18, 0.22, 0.5, DioPortColor(DioramaPalette::Dark), BollardX, 0.4, 1.5);
		}
		G.Box(44, 0.06, 10.3, DioPortColor(DioramaPalette::Water), 0, 0.03, 6.85);
	}

	void DioPortYard(FDioramaGeometry& G)
	{
		int32 StackIndex = 0;
		for (const double RowZ : { -7.0, -4.4 })
		{
			for (int32 Column = 0; Column < kDioPortYardColumns; ++Column)
			{
				const double StackX = -19.0 + 4.3 * Column;
				const int32 Height = 1 + (StackIndex * 7) % 3;
				++StackIndex;
				for (int32 Tier = 0; Tier < Height; ++Tier)
				{
					G.BoxB(3.9, 1.15, 1.6, DioPortContainer(StackIndex + Tier), StackX, Tier * 1.18, RowZ);
				}
			}
		}
	}

	void DioPortOffice(FDioramaGeometry& G)
	{
		G.PushFrame(18.6, 0, -5.4);
		G.BoxB(3.4, 2.6, 2.8, DioPortColor(DioramaPalette::Wall), 0, 0, 0);
		G.BoxB(3.5, 0.2, 2.9, DioPortColor(DioramaPalette::Roof), 0, 2.6, 0);
		G.BoxB(3.42, 0.5, 0.1, DioPortColor(DioramaPalette::Glass), 0, 1.4, 1.42);
		G.PopFrame();
	}

	void DioPortCraneFrame(FDioramaGeometry& G, double CraneX)
	{
		const FColor CraneColor = DioPortColor(DioramaPalette::Crane);
		G.PushFrame(CraneX, 0, 0);
		for (const double LegZ : { 0.25, 1.55 })
		{
			for (const double LegX : { -1.8, 1.8 })
			{
				G.BoxB(0.36, 7.0, 0.36, CraneColor, LegX, 0, LegZ);
			}
		}
		for (const double BeamZ : { 0.25, 1.55 })
		{
			G.BoxB(3.96, 0.36, 0.36, CraneColor, 0, 6.7, BeamZ);
		}
		for (const double BeamX : { -1.8, 1.8 })
		{
			G.BoxB(0.36, 0.36, 1.66, CraneColor, BeamX, 6.7, 0.9);
		}
		G.BoxB(1.0, 0.5, 16.6, CraneColor, 0, 7.06, 2.8);
		G.BoxB(1.8, 1.1, 2.2, DioPortColor(kDioPortWhite), 0, 7.56, -3.6);
		G.PopFrame();
	}

	FDioramaRigDesc DioPortCraneTrolley(int32 CraneIndex, double CraneX)
	{
		FDioramaRigDesc Rig;
		Rig.Kind = EDioramaRigKind::CraneTrolley;
		// 크레인마다 위상을 엇갈리게(시안 ph = i·0.37) 무대가 이 번호로 계산한다.
		Rig.Slot = CraneIndex;
		Rig.PathStartUE = FDioramaGeometry::ToUE(CraneX, kDioPortTrolleyY, kDioPortYardZ);
		Rig.PathEndUE = FDioramaGeometry::ToUE(CraneX, kDioPortTrolleyY, kDioPortShipZ);
		Rig.PivotUE = Rig.PathStartUE;
		Rig.Period = static_cast<float>(kDioPortTrolleyPeriod);

		FDioramaGeometry& G = Rig.Geometry;
		G.BoxB(1.1, 0.45, 1.1, DioPortColor(DioramaPalette::Dark), 0, -0.2, 0);
		G.Box(0.06, kDioPortCableRest, 0.06, DioPortColor(kDioPortCable), 0, -kDioPortCableRest * 0.5, 0);
		G.Box(3.4, 0.22, 1.25, DioPortColor(DioramaPalette::MachY), 0, -kDioPortCableRest - 0.11, 0);
		return Rig;
	}

	FDioramaRigDesc DioPortShip()
	{
		FDioramaRigDesc Rig;
		Rig.Kind = EDioramaRigKind::Ship;
		Rig.PivotUE = FDioramaGeometry::ToUE(0, 0, kDioPortShipZ);
		Rig.Amplitude = kDioPortShipBobM;
		Rig.Period = static_cast<float>(kDioPortShipBobPeriod);

		FDioramaGeometry& G = Rig.Geometry;
		const FColor HullColor = DioPortColor(DioramaPalette::Hull);
		G.BoxB(21, 1.7, 4.6, HullColor, 0, -0.7, 0);
		G.BoxB(21.05, 0.3, 4.65, DioPortColor(kDioPortHullStripe), 0, 0.06, 0);
		G.BoxB(3.2, 1.7, 3.2, HullColor, -10.3, -0.7, 0, UE_DOUBLE_PI / 4.0);
		G.BoxB(19.6, 0.12, 4.2, DioPortColor(DioramaPalette::Pipe), 0, 1.0, 0);
		G.BoxB(2.6, 2.4, 4.0, DioPortColor(kDioPortWhite), 8.4, 1.1, 0);
		G.BoxB(2.62, 0.45, 4.02, DioPortColor(kDioPortBridgeBand), 8.4, 2.6, 0);
		G.CylB(0.55, 0.6, 1.4, DioPortColor(DioramaPalette::Crane), 9.1, 3.5, 0, 12);

		int32 CargoIndex = 0;
		for (const double CargoY : { 1.12, 2.14 })
		{
			for (const double CargoZ : { -0.78, 0.78 })
			{
				for (int32 Column = 0; Column < kDioPortShipColumns; ++Column, ++CargoIndex)
				{
					if (CargoIndex < kDioPortShipLoaded)
					{
						G.BoxB(1.8, 1.0, 1.42, DioPortContainer(CargoIndex), -8.4 + 1.92 * Column, CargoY, CargoZ);
					}
				}
			}
		}
		return Rig;
	}
}

FBox2D DioramaModules::PortRect()
{
	return FBox2D(FVector2D(-22.0, -9.0), FVector2D(22.0, 12.0));
}

// 지도 그림도 정박한 배·크레인을 그대로 보여 주므로(시안 grab 은 제품·효과만 숨김) 미니어처 차이가 없다.
void DioramaModules::BuildPort(const FDioramaPortSpec& Spec, bool /*bMiniature*/, FDioramaModuleBuild& Out)
{
	Out.Module = EDioramaModule::Port;
	Out.RectXZ = PortRect();
	Out.Static.Reset();
	Out.Rigs.Reset();

	FDioramaGeometry& G = Out.Static;
	DioPortQuayAndWater(G);
	DioPortYard(G);
	DioPortOffice(G);

	const TArray<double> CraneXs = DioPortCraneXs(FMath::Clamp(Spec.CraneCount, 0, kDioPortMaxCranes));
	for (int32 CraneIndex = 0; CraneIndex < CraneXs.Num(); ++CraneIndex)
	{
		DioPortCraneFrame(G, CraneXs[CraneIndex]);
		Out.Rigs.Add(DioPortCraneTrolley(CraneIndex, CraneXs[CraneIndex]));
	}

	Out.Rigs.Add(DioPortShip());
}
