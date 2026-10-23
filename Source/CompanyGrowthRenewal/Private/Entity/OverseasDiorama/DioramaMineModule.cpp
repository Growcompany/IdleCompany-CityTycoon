#include "Entity/OverseasDiorama/DioramaModules.h"

namespace
{
	constexpr double kDioMineUnitPitch = 7.6;
	constexpr double kDioMineRectBack = -8.2;
	constexpr double kDioMineRectFront = 6.8;
	constexpr double kDioMineRectFrontAuto = 10.0;
	constexpr double kDioMineUnitZ = -3.2;
	constexpr double kDioMineStoreZ = 4.4;
	constexpr double kDioMineAutoZ = 8.4;
	constexpr double kDioMineTruckZ0 = -0.4;
	constexpr double kDioMineTruckZ1 = 3.0;
	constexpr float kDioMinePacketSpeed = 3.f;

	// 시안 색표(C)에 없는 시안 리터럴 색
	constexpr uint32 kDioMineTrack = 0x2B2F36;
	constexpr uint32 kDioMineCabGlass = 0x7FC3DB;
	constexpr uint32 kDioMineLogEnd = 0xC9A47A;
	constexpr uint32 kDioMineTankRim = 0x9AA3AF;
	constexpr uint32 kDioMineTankBand = 0x3A3F48;
	constexpr uint32 kDioMineFlame = 0xFF9A3C;
	constexpr uint32 kDioMineLamp = 0xFFE7A3;
	constexpr uint32 kDioMineOilDot = 0xFFD08A;

	FColor DioMineCol(uint32 RGB)
	{
		return DioramaModules::Hex(RGB);
	}

	double DioMineUnitX(int32 Index, int32 Count)
	{
		return (Index - (Count - 1) * 0.5) * kDioMineUnitPitch;
	}

	double DioMineHalfX(const FDioramaMineSpec& Spec)
	{
		return (FMath::Max(Spec.Units.Num(), 1) - 1) * 3.8 + 7.2;
	}

	FDioramaRigDesc& DioMineAddRig(FDioramaModuleBuild& Out, EDioramaRigKind Kind, int32 UnitIndex, const FVector& PivotUE)
	{
		FDioramaRigDesc& RigDesc = Out.Rigs.AddDefaulted_GetRef();
		RigDesc.Kind = Kind;
		RigDesc.Slot = UnitIndex;
		RigDesc.PivotUE = PivotUE;
		return RigDesc;
	}

	void DioMineRing4(FDioramaGeometry& Geo, double OuterW, double InnerW, double H, FColor Color)
	{
		const double Thick = (OuterW - InnerW) * 0.5;
		const double Off = InnerW * 0.5 + Thick * 0.5;
		Geo.BoxB(OuterW, H, Thick, Color, 0.0, 0.0, -Off);
		Geo.BoxB(OuterW, H, Thick, Color, 0.0, 0.0, Off);
		Geo.BoxB(Thick, H, InnerW, Color, -Off, 0.0, 0.0);
		Geo.BoxB(Thick, H, InnerW, Color, Off, 0.0, 0.0);
	}

	// 시안 truckUnit — 형상은 무대가 SM_Truck_Large 로 대신하므로 경로만 넘긴다.
	void DioMineTruck(FDioramaModuleBuild& Out, int32 UnitIndex, double TruckX, FColor LoadColor, int32 Rate, bool bRunning, bool bMiniature)
	{
		if (bMiniature)
		{
			return;
		}
		const double Sp = 0.22 * (1.0 + Rate * 0.3);
		const FVector Start = FDioramaGeometry::ToUE(TruckX, 0.0, kDioMineTruckZ0);
		const FVector End = FDioramaGeometry::ToUE(TruckX, 0.0, kDioMineTruckZ1);
		FDioramaRigDesc& RigDesc = DioMineAddRig(Out, EDioramaRigKind::Truck, UnitIndex, Start);
		RigDesc.PathStartUE = Start;
		RigDesc.PathEndUE = End;
		RigDesc.YawDeg = (End - Start).Rotation().Yaw;
		RigDesc.Period = static_cast<float>(1.0 / Sp);
		RigDesc.Speed = static_cast<float>(2.0 * (kDioMineTruckZ1 - kDioMineTruckZ0) * Sp);
		RigDesc.PacketColor = LoadColor;
		RigDesc.bActive = bRunning;
	}

	void DioMinePitUnit(FDioramaModuleBuild& Out, int32 UnitIndex, double X, FColor ResColor, int32 Rate, bool bRunning, bool bMiniature)
	{
		FDioramaGeometry& Geo = Out.Static;
		Geo.PushFrame(X, 0.0, kDioMineUnitZ);
		DioMineRing4(Geo, 7.2, 5.2, 0.95, DioMineCol(DioramaPalette::Earth1));
		DioMineRing4(Geo, 5.2, 3.4, 0.6, DioMineCol(DioramaPalette::Earth2));
		DioMineRing4(Geo, 3.4, 2.0, 0.3, DioMineCol(DioramaPalette::Earth3));
		Geo.BoxB(2.0, 0.05, 2.0, ResColor, 0.0, 0.0, 0.0);
		const double Chunks[4][2] = { { -0.5, -0.4 }, { 0.4, 0.3 }, { 0.1, -0.6 }, { -0.3, 0.5 } };
		for (int32 k = 0; k < 4; ++k)
		{
			Geo.BoxB(0.45, 0.32, 0.4, ResColor, Chunks[k][0], 0.05, Chunks[k][1], k * 0.7);
		}

		if (Rate == 0)
		{
			const double Legs[3][2] = { { 0.7, 0.0 }, { -0.35, 0.6 }, { -0.35, -0.6 } };
			for (const auto& Leg : Legs)
			{
				Geo.Box(0.1, 2.6, 0.1, DioMineCol(DioramaPalette::Slate), Leg[0] * 0.8, 1.3, Leg[1] * 0.8, -Leg[1] * 0.22, 0.0, Leg[0] * 0.22);
			}
			// 시안 stepMine 이 드릴 중심을 1.1~1.35 로 덮어쓴다 — 정지 형상은 그 아래 끝.
			Geo.Box(0.36, 0.9, 0.36, DioMineCol(DioramaPalette::Mach), 0.0, 1.1, 0.0);
			Geo.PopFrame();
			return;
		}
		Geo.PopFrame();

		const bool bYellow = Rate >= 3;
		const FColor BodyColor = DioMineCol(bYellow ? DioramaPalette::MachY : DioramaPalette::Mach);
		const double Sc = bYellow ? 1.3 : 1.0;
		const double ExX = X + 3.0;
		const double ExZ = -0.4;

		Geo.PushFrame(ExX, 0.0, ExZ);
		Geo.BoxB(1.8 * Sc, 0.35 * Sc, 0.5 * Sc, DioMineCol(kDioMineTrack), 0.0, 0.0, -0.45 * Sc);
		Geo.BoxB(1.8 * Sc, 0.35 * Sc, 0.5 * Sc, DioMineCol(kDioMineTrack), 0.0, 0.0, 0.45 * Sc);
		Geo.PopFrame();

		// 회전체(운전석+붐)만 리그 — 붐 끄덕임은 평균각으로 고정하고 선회만 무대가 흔든다.
		const double Sp = 1.0 + Rate * 0.35;
		FDioramaRigDesc& RigDesc = DioMineAddRig(Out, EDioramaRigKind::Excavator, UnitIndex, FDioramaGeometry::ToUE(ExX, 0.35 * Sc, ExZ));
		RigDesc.YawDeg = FMath::RadiansToDegrees(0.72);
		RigDesc.Amplitude = static_cast<float>(FMath::RadiansToDegrees(0.45));
		RigDesc.Period = static_cast<float>(UE_DOUBLE_TWO_PI / (Sp * 1.4 * 0.6));
		RigDesc.bActive = bRunning;
		FDioramaGeometry& Arm = RigDesc.Geometry;
		Arm.BoxB(1.5 * Sc, 0.7 * Sc, 1.2 * Sc, BodyColor, 0.25 * Sc, 0.0, 0.0);
		Arm.BoxB(0.7 * Sc, 0.65 * Sc, 0.8 * Sc, BodyColor, 0.55 * Sc, 0.7 * Sc, 0.15 * Sc);
		Arm.BoxB(0.55 * Sc, 0.4 * Sc, 0.82 * Sc, DioMineCol(kDioMineCabGlass), 0.5 * Sc, 0.85 * Sc, 0.16 * Sc);
		Arm.PushFrame(-0.4 * Sc, 0.55 * Sc, 0.0, 0.0, 0.0, 0.3);
		Arm.BoxB(2.6 * Sc, 0.28 * Sc, 0.3 * Sc, BodyColor, -1.3 * Sc, -0.14 * Sc, 0.0);
		Arm.BoxB(0.55 * Sc, 0.5 * Sc, 0.62 * Sc, DioMineCol(DioramaPalette::Dark), -2.6 * Sc, -0.45 * Sc, 0.0);
		Arm.PopFrame();

		const int32 TruckCount = Rate >= 2 ? 2 : 1;
		for (int32 k = 0; k < TruckCount; ++k)
		{
			DioMineTruck(Out, UnitIndex, X - 2.9 + k * 1.15, ResColor, Rate, bRunning, bMiniature);
		}
	}

	void DioMinePumpUnit(FDioramaModuleBuild& Out, int32 UnitIndex, double X, int32 Rate, bool bRunning)
	{
		FDioramaGeometry& Geo = Out.Static;
		const FColor PipeColor = DioMineCol(DioramaPalette::Pipe);
		const double Sc = Rate >= 3 ? 1.2 : 1.0;
		FDioramaRigDesc& RigDesc = DioMineAddRig(Out, EDioramaRigKind::OilPump, UnitIndex,
			FDioramaGeometry::ToUE(X, 0.0, kDioMineUnitZ));
		RigDesc.MeshScale = static_cast<float>(Sc);
		RigDesc.bActive = bRunning;

		if (Rate >= 2)
		{
			Geo.PushFrame(X + 2.8, 0.0, -0.2);
			Geo.BoxB(1.4, 0.9, 1.0, PipeColor, 0.0, 0.0, 0.0);
			Geo.CylB(0.15, 0.15, 1.2, DioMineCol(DioramaPalette::Steel), 0.3, 0.9, 0.0);
			Geo.PopFrame();
		}
		Geo.BoxB(0.2, 0.2, 6.4, PipeColor, X - 2.35 * Sc, 0.15, 0.9);
	}

	void DioMineWoodUnit(FDioramaModuleBuild& Out, int32 UnitIndex, double X, int32 Rate, bool bRunning, bool bMiniature)
	{
		FDioramaGeometry& Geo = Out.Static;
		Geo.PushFrame(X, 0.0, kDioMineUnitZ);
		const double Pines[3][2] = { { -2.2, -2.0 }, { -0.6, -2.5 }, { 1.2, -2.1 } };
		for (const auto& Pine : Pines)
		{
			Geo.CylB(0.16, 0.2, 0.6, DioMineCol(DioramaPalette::Trunk), Pine[0], 0.0, Pine[1]);
			Geo.Cyl(0.0, 0.85, 2.4, DioMineCol(DioramaPalette::Pine), Pine[0], 1.8, Pine[1], 7);
		}
		const double Stumps[2][2] = { { 2.4, -1.3 }, { 2.9, -0.2 } };
		for (const auto& Stump : Stumps)
		{
			Geo.CylB(0.28, 0.3, 0.32, DioMineCol(kDioMineLogEnd), Stump[0], 0.0, Stump[1]);
		}
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 k = 0; k < 3 - Row; ++k)
			{
				Geo.Cyl(0.22, 0.22, 2.6, DioMineCol(DioramaPalette::Wood), -0.4, 0.22 + Row * 0.38, 0.3 + k * 0.46 + Row * 0.23, 8, 0.0, 0.0, UE_DOUBLE_HALF_PI);
			}
		}
		Geo.BoxB(2.0, 1.2, 1.6, DioMineCol(DioramaPalette::Pallet), 2.4, 0.0, 1.4);
		Geo.BoxB(2.3, 0.18, 1.9, DioMineCol(DioramaPalette::Roof), 2.4, 1.2, 1.4);
		Geo.PopFrame();

		if (Rate >= 1)
		{
			const int32 TruckCount = Rate >= 2 ? 2 : 1;
			for (int32 k = 0; k < TruckCount; ++k)
			{
				DioMineTruck(Out, UnitIndex, X - 2.9 + k * 1.15, DioMineCol(DioramaPalette::Wood), Rate, bRunning, bMiniature);
			}
		}
	}

	void DioMineSilo(FDioramaGeometry& Geo, double SiloX, double Radius, double H, FColor ResColor)
	{
		Geo.CylB(Radius, Radius, H, DioMineCol(DioramaPalette::Silo), SiloX, 0.0, 0.0, 14);
		Geo.CylB(Radius + 0.02, Radius + 0.02, 0.35, ResColor, SiloX, H - 0.8, 0.0, 14);
		Geo.Cyl(0.0, Radius + 0.08, 0.7, DioMineCol(DioramaPalette::Steel), SiloX, H + 0.35, 0.0, 14);
	}

	// 저장 더미 크기 = 시안 stepMine 의 mPiles 규칙을 지을 때의 채움 비율로 굳힌다.
	void DioMineStoreUnit(FDioramaModuleBuild& Out, int32 UnitIndex, double X, EDioramaMineKind Kind, FColor ResColor, int32 Storage, double Fill, bool bRefinery)
	{
		FDioramaGeometry& Geo = Out.Static;
		const double PileScale = 0.12 + 0.88 * Fill;
		Geo.PushFrame(X, 0.0, kDioMineStoreZ);
		switch (Kind)
		{
		case EDioramaMineKind::Pump:
		{
			// [저장 단계][탱크] = (x, 반지름, 높이)
			const double TankTable[4][2][3] = {
				{ { 0.0, 0.9, 1.4 }, { 0.0, 0.0, 0.0 } },
				{ { 0.0, 1.2, 1.9 }, { 0.0, 0.0, 0.0 } },
				{ { -1.3, 1.0, 1.8 }, { 1.3, 1.0, 1.8 } },
				{ { -1.7, 1.1, 2.0 }, { 0.7, 1.1, 2.0 } } };
			const int32 TankCount = Storage >= 2 ? 2 : 1;
			for (int32 k = 0; k < TankCount; ++k)
			{
				const double TankX = TankTable[Storage][k][0];
				const double TankR = TankTable[Storage][k][1];
				const double TankH = TankTable[Storage][k][2];
				Geo.CylB(TankR, TankR, TankH, DioMineCol(DioramaPalette::Tank), TankX, 0.0, 0.0, 16);
				Geo.CylB(TankR + 0.02, TankR + 0.02, 0.1, DioMineCol(kDioMineTankRim), TankX, TankH, 0.0, 16);
				Geo.Cyl(TankR + 0.04, TankR + 0.04, 0.16, DioMineCol(kDioMineTankBand), TankX, 0.18 + Fill * (TankH - 0.36), 0.0, 16);
			}
			if (bRefinery)
			{
				Geo.CylB(0.5, 0.5, 6.4, DioMineCol(DioramaPalette::Silo), 2.9, 0.0, -0.8, 12);
				for (const double RingY : { 1.6, 3.2, 4.8 })
				{
					Geo.CylB(0.64, 0.64, 0.12, DioMineCol(DioramaPalette::Steel), 2.9, RingY, -0.8, 12);
				}
				Geo.CylB(0.12, 0.12, 7.2, DioMineCol(DioramaPalette::Steel), -3.3, 0.0, -1.4, 8);

				FDioramaRigDesc& RigDesc = DioMineAddRig(Out, EDioramaRigKind::Flare, UnitIndex, FDioramaGeometry::ToUE(X - 3.3, 7.7, kDioMineStoreZ - 1.4));
				RigDesc.Amplitude = 0.175f;
				RigDesc.Period = static_cast<float>(UE_DOUBLE_PI / 9.0);
				RigDesc.Geometry.Cyl(0.0, 0.34, 1.0, DioMineCol(kDioMineFlame), 0.0, 0.0, 0.0, 8);
			}
			break;
		}
		case EDioramaMineKind::Lumber:
		{
			const int32 StackCounts[4] = { 1, 2, 3, 3 };
			for (int32 k = 0; k < StackCounts[Storage]; ++k)
			{
				const double StackX = -2.2 + k * 1.6;
				Geo.BoxB(1.3, 1.2 * PileScale, 1.0, DioMineCol(DioramaPalette::Wood), StackX, 0.0, 0.0);
				Geo.BoxB(1.32, 0.08 * PileScale, 1.02, DioMineCol(kDioMineLogEnd), StackX, 0.6 * PileScale, 0.0);
			}
			if (Storage >= 3)
			{
				const double Posts[4][2] = { { -3.0, -0.8 }, { 2.2, -0.8 }, { -3.0, 0.8 }, { 2.2, 0.8 } };
				for (const auto& Post : Posts)
				{
					Geo.BoxB(0.14, 2.2, 0.14, DioMineCol(DioramaPalette::Steel), Post[0], 0.0, Post[1]);
				}
				Geo.BoxB(5.6, 0.15, 2.0, DioMineCol(DioramaPalette::Roof), -0.4, 2.2, 0.0);
			}
			break;
		}
		default:
		{
			Geo.CylB(0.0, 1.5, 1.6 * PileScale, ResColor, Storage == 0 ? 0.0 : -1.6, 0.0, 0.0, 8);
			if (Storage == 1)
			{
				DioMineSilo(Geo, 1.4, 0.85, 3.0, ResColor);
			}
			else if (Storage == 2)
			{
				DioMineSilo(Geo, 1.0, 0.75, 3.0, ResColor);
				DioMineSilo(Geo, 2.8, 0.75, 3.0, ResColor);
			}
			else if (Storage == 3)
			{
				DioMineSilo(Geo, 1.9, 1.25, 4.4, ResColor);
				Geo.Box(0.4, 0.16, 3.6, DioMineCol(DioramaPalette::Frame), 0.2, 2.28, 0.0, 0.6, UE_DOUBLE_HALF_PI, 0.0);
			}
			break;
		}
		}
		Geo.PopFrame();
	}

	void DioMineLights(FDioramaGeometry& Geo, int32 Offline, double HalfX, double FrontZ)
	{
		const int32 TowerCounts[4] = { 0, 1, 2, 4 };
		const double Spots[4][2] = { { -HalfX + 1.2, -6.9 }, { HalfX - 1.2, -6.9 }, { -HalfX + 1.2, FrontZ - 1.4 }, { HalfX - 1.2, FrontZ - 1.4 } };
		for (int32 k = 0; k < TowerCounts[Offline]; ++k)
		{
			const double TowerX = Spots[k][0];
			const double TowerZ = Spots[k][1];
			Geo.PushFrame(TowerX, 0.0, TowerZ, 0.0, FMath::Atan2(-TowerX, -TowerZ), 0.0);
			Geo.CylB(0.12, 0.16, 5.4, DioMineCol(DioramaPalette::Pipe), 0.0, 0.0, 0.0);
			Geo.BoxB(1.3, 0.55, 0.4, DioMineCol(DioramaPalette::Dark), 0.0, 5.3, 0.0);
			Geo.BoxB(1.1, 0.38, 0.04, DioMineCol(kDioMineLamp), 0.0, 5.38, 0.22);
			Geo.PopFrame();
		}
	}

	void DioMineAutoLine(FDioramaModuleBuild& Out, const FDioramaMineSpec& Spec, double HalfX)
	{
		FDioramaGeometry& Geo = Out.Static;
		const int32 Count = Spec.Units.Num();
		const double Len = 2.0 * HalfX - 2.4;
		const double HalfLen = Len * 0.5;
		const bool bOil = Spec.IsAllOil();

		Geo.PushFrame(0.0, 0.0, kDioMineAutoZ);
		for (int32 i = 0; i < Count; ++i)
		{
			Geo.BoxB(0.5, 0.16, 2.6, DioMineCol(DioramaPalette::Frame), DioMineUnitX(i, Count), 0.45, -1.5);
		}
		Geo.BoxB(1.4, 2.2, 1.6, DioMineCol(DioramaPalette::Pipe), HalfX - 1.0, 0.0, 0.0);
		if (bOil)
		{
			Geo.Cyl(0.32, 0.32, Len, DioMineCol(DioramaPalette::Pipe), 0.0, 0.9, 0.0, 10, 0.0, 0.0, UE_DOUBLE_HALF_PI);
			for (double SupportX = -HalfLen + 1.0; SupportX <= HalfLen - 1.0; SupportX += 4.0)
			{
				Geo.BoxB(0.2, 0.75, 0.6, DioMineCol(DioramaPalette::Steel), SupportX, 0.0, 0.0);
			}
		}
		else
		{
			Geo.BoxB(Len, 0.3, 1.0, DioMineCol(DioramaPalette::Frame), 0.0, 0.35, 0.0);
			Geo.BoxB(Len, 0.05, 0.85, DioMineCol(DioramaPalette::Belt), 0.0, 0.65, 0.0);
		}
		Geo.PopFrame();

		const double PacketY = bOil ? 1.25 : 0.86;
		FDioramaRigDesc& RigDesc = DioMineAddRig(Out, EDioramaRigKind::BeltPackets, INDEX_NONE, FDioramaGeometry::ToUE(0.0, PacketY, kDioMineAutoZ));
		RigDesc.PathStartUE = FDioramaGeometry::ToUE(-HalfLen, PacketY, kDioMineAutoZ);
		RigDesc.PathEndUE = FDioramaGeometry::ToUE(HalfLen, PacketY, kDioMineAutoZ);
		RigDesc.YawDeg = (RigDesc.PathEndUE - RigDesc.PathStartUE).Rotation().Yaw;
		RigDesc.Speed = kDioMinePacketSpeed;
		RigDesc.MaxPackets = bOil ? 7 : 9;
		RigDesc.PacketColor = bOil ? DioMineCol(kDioMineOilDot) : DioMineCol(DioramaPalette::ResourceColor(Spec.Units[0].Resource));
		RigDesc.PacketSizeUE = bOil ? FVector(32.0) : FVector(50.0, 50.0, 32.0);
		RigDesc.bActive = Spec.Units.ContainsByPredicate([](const FDioramaMineUnit& Unit) { return Unit.State == EDioramaSlotState::Running; });
	}
}

FBox2D DioramaModules::MineRect(const FDioramaMineSpec& Spec)
{
	const double HalfX = DioMineHalfX(Spec);
	return FBox2D(FVector2D(-HalfX, kDioMineRectBack), FVector2D(HalfX, Spec.bAutoClaim ? kDioMineRectFrontAuto : kDioMineRectFront));
}

void DioramaModules::BuildMine(const FDioramaMineSpec& Spec, bool bMiniature, FDioramaModuleBuild& Out)
{
	Out.Module = EDioramaModule::Mine;
	Out.RectXZ = MineRect(Spec);
	Out.Static.Reset();
	Out.Rigs.Reset();

	const int32 Count = Spec.Units.Num();
	if (Count == 0)
	{
		return;
	}

	const int32 Rate = FMath::Clamp(Spec.RateTier, 0, 3);
	const int32 Storage = FMath::Clamp(Spec.StorageTier, 0, 3);
	const int32 Offline = FMath::Clamp(Spec.OfflineTier, 0, 3);
	// 정유 = 유전만 있는 광산의 저장 3단계(국가 분기 아님). 섞인 광산의 유류 저장은 정제탑 없이 3단계 탱크만.
	const bool bRefinery = Spec.IsAllOil() && Storage == 3;

	for (int32 i = 0; i < Count; ++i)
	{
		const FDioramaMineUnit& Unit = Spec.Units[i];
		const double X = DioMineUnitX(i, Count);
		const bool bRunning = Unit.State == EDioramaSlotState::Running;
		const double Fill = Unit.State == EDioramaSlotState::Completed ? 1.0 : FMath::Clamp(static_cast<double>(Unit.Fill01), 0.0, 1.0);
		const EDioramaMineKind Kind = OverseasDiorama::MineKindFor(Unit.Resource);
		const FColor ResColor = DioMineCol(DioramaPalette::ResourceColor(Unit.Resource));

		switch (Kind)
		{
		case EDioramaMineKind::Pump: DioMinePumpUnit(Out, i, X, Rate, bRunning); break;
		case EDioramaMineKind::Lumber: DioMineWoodUnit(Out, i, X, Rate, bRunning, bMiniature); break;
		default: DioMinePitUnit(Out, i, X, ResColor, Rate, bRunning, bMiniature); break;
		}
		DioMineStoreUnit(Out, i, X, Kind, ResColor, Storage, Fill, bRefinery);
	}

	const double HalfX = DioMineHalfX(Spec);
	DioMineLights(Out.Static, Offline, HalfX, Out.RectXZ.Max.Y);
	if (Spec.bAutoClaim)
	{
		DioMineAutoLine(Out, Spec, HalfX);
	}
}
