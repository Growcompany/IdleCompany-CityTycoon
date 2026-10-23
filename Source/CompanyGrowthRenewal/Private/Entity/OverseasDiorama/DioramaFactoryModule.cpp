#include "Entity/OverseasDiorama/DioramaModules.h"

namespace
{
	constexpr double kDioFacX0 = -19.2;
	constexpr double kDioFacX1 = 7.2;
	constexpr double kDioFacZB = -12.0;
	constexpr double kDioFacBeltTopY = 0.78;
	constexpr double kDioFacPacketStartX = -20.4;
	constexpr double kDioFacPacketEndX = 9.6;
	constexpr float kDioFacBeltSpeed = 1.7f;
	constexpr int32 kDioFacMaxPackets = 6;
	// 시안 SPEED[A] — 로봇팔 흔들림이 자동화 단계에 따라 빨라진다.
	constexpr double kDioFacArmSpeed[4] = { 1.0, 1.15, 1.35, 1.7 };
	constexpr double kDioFacArmSwayRad = 0.42;
	constexpr double kDioFacArmWaveRate = 1.8 * 0.8;
	constexpr double kDioFacShoulderRest = 0.55;
	constexpr double kDioFacElbowRest = 1.3;
	constexpr double kDioFacTruckWaitX = 12.6;
	constexpr double kDioFacTruckGateX = 19.8;
	constexpr double kDioFacTruckLaneZ = 12.9;
	constexpr double kDioFacTruckSpeed = 5.0;
	// 시안 트럭 한 바퀴 = 대기 4s + 나감 + 문 밖 3s + 들어옴
	constexpr double kDioFacTruckPeriod = 4.0 + 3.0 + 2.0 * (kDioFacTruckGateX - kDioFacTruckWaitX) / kDioFacTruckSpeed;
	constexpr double kDioFacUnitScaleElec = 0.85;
	constexpr double kDioFacUnitScaleAuto = 0.6;
	constexpr double kDioFacAgvLoadScale = 0.55;

	constexpr uint32 kDioFacSignBoard = 0x2B3A55;
	constexpr uint32 kDioFacOrange = 0xFF9F43;
	constexpr uint32 kDioFacScreen = 0x5BE39A;
	constexpr uint32 kDioFacQcLamp = 0x3FBF7A;
	constexpr uint32 kDioFacHoodTop = 0xC9CED6;
	constexpr uint32 kDioFacPackBody = 0xD9DEE5;
	constexpr uint32 kDioFacContainerRail = 0x3A3F48;
	constexpr uint32 kDioFacGenBand = 0xE7C341;
	constexpr uint32 kDioFacGenVent = 0x3A4352;
	constexpr uint32 kDioFacFanBlade = 0xC9D2DD;
	constexpr uint32 kDioFacCore = 0x7FD4E8;
	constexpr uint32 kDioFacBeacon = 0x6FD0FF;
	// 시안 고스트(흰색 55%)가 포장면 위에 비친 색 ≈ 도로선 색 — 같은 버킷으로 합쳐진다.
	constexpr uint32 kDioFacGhost = DioramaPalette::RoadLine;
	constexpr uint32 kDioFacOresElec[3] = { 0x8FA3B8, 0xC77B43, 0x8E7BB5 };
	constexpr uint32 kDioFacOresAuto[3] = { 0x7D6E66, 0xC3C9D0, 0x9AD0C2 };

	double DioFacZL(int32 Index)
	{
		return -9.6 + Index * 3.2;
	}

	double DioFacZFront(int32 LineCount)
	{
		return DioFacZL(LineCount - 1) + 2.0;
	}

	void DioFacBox(FDioramaGeometry& G, double W, double H, double D, uint32 RGB, double X, double Y, double Z, double RX = 0.0, double RY = 0.0, double RZ = 0.0)
	{
		G.Box(W, H, D, DioramaModules::Hex(RGB), X, Y, Z, RX, RY, RZ);
	}

	void DioFacBoxB(FDioramaGeometry& G, double W, double H, double D, uint32 RGB, double X, double Y, double Z, double RY = 0.0)
	{
		G.BoxB(W, H, D, DioramaModules::Hex(RGB), X, Y, Z, RY);
	}

	void DioFacCyl(FDioramaGeometry& G, double RTop, double RBottom, double H, uint32 RGB, double X, double Y, double Z, int32 Segments = 12, double RX = 0.0, double RY = 0.0, double RZ = 0.0)
	{
		G.Cyl(RTop, RBottom, H, DioramaModules::Hex(RGB), X, Y, Z, Segments, RX, RY, RZ);
	}

	void DioFacCylB(FDioramaGeometry& G, double RTop, double RBottom, double H, uint32 RGB, double X, double Y, double Z, int32 Segments = 12)
	{
		G.CylB(RTop, RBottom, H, DioramaModules::Hex(RGB), X, Y, Z, Segments);
	}

	// three.js r128 Color.multiplyScalar(0.86).getHex() 와 같게 채널을 버림한다.
	uint32 DioFacShade(uint32 RGB)
	{
		uint32 Shaded = 0;
		for (int32 Shift = 16; Shift >= 0; Shift -= 8)
		{
			double Channel = static_cast<double>((RGB >> Shift) & 0xFF) / 255.0;
			Channel *= 0.86;
			Shaded |= static_cast<uint32>(Channel * 255.0) << Shift;
		}
		return Shaded;
	}

	// 시안 unitMesh: 전자 = productMesh 형태 3(상자), 자동차 = 형태 2(완성차). 원점 = 바닥 중심.
	void DioFacUnit(FDioramaGeometry& G, bool bAuto, double Scale, double X, double Y, double Z)
	{
		auto Part = [&G, Scale, X, Y, Z](double W, double H, double D, uint32 RGB, double PX, double PY, double PZ)
		{
			DioFacBoxB(G, W * Scale, H * Scale, D * Scale, RGB, X + PX * Scale, Y + PY * Scale, Z + PZ * Scale);
		};
		if (!bAuto)
		{
			Part(1.05, 0.6, 0.8, DioramaPalette::Cardboard, 0.0, 0.0, 0.0);
			Part(1.07, 0.02, 0.16, DioramaPalette::Tape, 0.0, 0.6, 0.0);
			Part(0.3, 0.2, 0.02, 0xF4F1EA, 0.25, 0.2, 0.41);
			return;
		}
		Part(1.5, 0.1, 0.62, 0x4A505A, 0.0, 0.12, 0.0);
		for (const FVector2D& Wheel : { FVector2D(-0.5, -0.34), FVector2D(0.5, -0.34), FVector2D(-0.5, 0.34), FVector2D(0.5, 0.34) })
		{
			DioFacCyl(G, 0.17 * Scale, 0.17 * Scale, 0.12 * Scale, 0x22262C, X + Wheel.X * Scale, Y + 0.17 * Scale, Z + Wheel.Y * Scale, 12, UE_DOUBLE_HALF_PI);
		}
		Part(1.5, 0.3, 0.72, DioramaPalette::AccentAutomobile, 0.0, 0.22, 0.0);
		Part(0.8, 0.27, 0.64, 0x7FC3DB, -0.1, 0.52, 0.0);
		Part(0.74, 0.06, 0.62, DioramaPalette::AccentAutomobile, -0.1, 0.79, 0.0);
		Part(0.02, 0.08, 0.14, 0xFFE9A8, 0.76, 0.36, -0.22);
		Part(0.02, 0.08, 0.14, 0xFFE9A8, 0.76, 0.36, 0.22);
	}

	void DioFacSideWall(FDioramaGeometry& G, double X, int32 LineCount, double ZF)
	{
		auto Segment = [&G, X](double From, double To)
		{
			DioFacBoxB(G, 0.3, 1.7, To - From, DioramaPalette::Wall, X, 0.22, (From + To) / 2.0);
			DioFacBoxB(G, 0.44, 0.14, To - From, DioramaPalette::Cap, X, 1.92, (From + To) / 2.0);
		};
		double Z = kDioFacZB;
		for (int32 Index = 0; Index < LineCount; ++Index)
		{
			const double Gap0 = DioFacZL(Index) - 0.85;
			const double Gap1 = DioFacZL(Index) + 0.85;
			if (Gap0 > Z)
			{
				Segment(Z, Gap0);
			}
			Z = Gap1;
		}
		if (ZF > Z)
		{
			Segment(Z, ZF);
		}
	}

	void DioFacHall(FDioramaGeometry& G, int32 LineCount)
	{
		const double ZF = DioFacZFront(LineCount);
		const double L = kDioFacX1 - kDioFacX0;
		const double D = ZF - kDioFacZB;
		const double CX = (kDioFacX0 + kDioFacX1) / 2.0;
		const double CZ = (kDioFacZB + ZF) / 2.0;

		DioFacBoxB(G, L, 0.22, D, DioramaPalette::Floor, CX, 0.0, CZ);
		DioFacBoxB(G, L, 4.2, 0.3, DioramaPalette::Wall, CX, 0.22, kDioFacZB + 0.15);
		for (double X = kDioFacX0 + 0.9; X + 2.6 <= kDioFacX1 - 0.4; X += 3.3)
		{
			DioFacBoxB(G, 2.6, 1.1, 0.1, DioramaPalette::Glass, X + 1.3, 2.25, kDioFacZB + 0.33);
		}
		DioFacBoxB(G, L + 0.1, 0.16, 0.44, DioramaPalette::Cap, CX, 4.42, kDioFacZB + 0.15);
		DioFacSideWall(G, kDioFacX0, LineCount, ZF);
		DioFacSideWall(G, kDioFacX1, LineCount, ZF);
		DioFacBoxB(G, L, 0.9, 0.3, DioramaPalette::Wall, CX, 0.22, ZF - 0.15);
		DioFacBoxB(G, L + 0.1, 0.14, 0.44, DioramaPalette::Cap, CX, 1.12, ZF - 0.15);
		for (const double X : { kDioFacX0, kDioFacX0 + 6.6, kDioFacX0 + 13.2, kDioFacX0 + 19.8, kDioFacX1 })
		{
			DioFacBoxB(G, 0.45, 4.8, 0.45, DioramaPalette::Steel, X, 0.22, kDioFacZB + 0.2);
			DioFacBoxB(G, 0.45, 4.8, 0.45, DioramaPalette::Steel, X, 0.22, ZF - 0.2);
			DioFacBoxB(G, 0.3, 0.36, D, DioramaPalette::Steel, X, 4.66, CZ);
		}
		DioFacBoxB(G, L, 0.36, 0.3, DioramaPalette::Steel, CX, 4.66, ZF - 0.2);
		for (int32 Index = 0; Index < LineCount; ++Index)
		{
			DioFacBoxB(G, L - 0.8, 0.02, 0.1, DioramaPalette::Line, CX, 0.22, DioFacZL(Index) - 1.15);
			DioFacBoxB(G, L - 0.8, 0.02, 0.1, DioramaPalette::Line, CX, 0.22, DioFacZL(Index) + 1.15);
		}
	}

	void DioFacHopper(FDioramaGeometry& G, int32 Index, bool bAuto)
	{
		const uint32* Ores = bAuto ? kDioFacOresAuto : kDioFacOresElec;
		G.PushFrame(-21.0, 0.0, DioFacZL(Index));
		for (const FVector2D& LegXZ : { FVector2D(-0.5, -0.5), FVector2D(0.5, -0.5), FVector2D(-0.5, 0.5), FVector2D(0.5, 0.5) })
		{
			DioFacBoxB(G, 0.12, 1.3, 0.12, DioramaPalette::Leg, LegXZ.X, 0.0, LegXZ.Y);
		}
		DioFacCylB(G, 0.95, 0.42, 1.3, DioramaPalette::Slate, 0.0, 1.25, 0.0, 8);
		for (int32 K = 0; K < 5; ++K)
		{
			DioFacBoxB(G, 0.3, 0.26, 0.3, Ores[K % 3], FMath::Cos(K * 1.3) * 0.45, 2.42, FMath::Sin(K * 1.3) * 0.45, static_cast<double>(K));
		}
		G.PopFrame();
	}

	void DioFacLine(FDioramaGeometry& G, int32 Index, bool bAuto, int32 QualityTier)
	{
		const double Z = DioFacZL(Index);

		G.PushFrame(-5.6, 0.0, Z);
		DioFacBoxB(G, 31.0, 0.27, 1.3, DioramaPalette::Frame, 0.0, 0.45, 0.0);
		for (int32 LegX = -15; LegX <= 15; LegX += 3)
		{
			DioFacBoxB(G, 0.16, 0.45, 1.1, DioramaPalette::Leg, LegX, 0.0, 0.0);
		}
		DioFacBoxB(G, 31.0, 0.06, 1.1, DioramaPalette::Belt, 0.0, 0.72, 0.0);
		DioFacBoxB(G, 31.0, 0.08, 0.08, DioramaPalette::Steel, 0.0, 0.78, -0.62);
		DioFacBoxB(G, 31.0, 0.08, 0.08, DioramaPalette::Steel, 0.0, 0.78, 0.62);
		G.PopFrame();

		G.PushFrame(-14.0, 0.22, Z);
		DioFacBoxB(G, 0.2, 2.5, 0.2, DioramaPalette::Slate, 0.0, 0.0, -0.95);
		DioFacBoxB(G, 0.2, 2.5, 0.2, DioramaPalette::Slate, 0.0, 0.0, 0.95);
		DioFacBoxB(G, 0.32, 0.32, 2.2, DioramaPalette::Slate, 0.0, 2.5, 0.0);
		DioFacCylB(G, 0.12, 0.12, 0.18, kDioFacOrange, 0.0, 2.82, 0.0);
		G.PushFrame(0.0, 2.0, 0.0);
		DioFacBoxB(G, 0.5, 0.5, 0.5, DioramaPalette::Machine, 0.0, 0.0, 0.0);
		DioFacBoxB(G, 0.2, 0.18, 0.2, kDioFacOrange, 0.0, -0.18, 0.0);
		G.PopFrame();
		G.PopFrame();

		const double HW = bAuto ? 2.6 : 2.0;
		const double HH = bAuto ? 2.1 : 1.75;
		G.PushFrame(-6.0, 0.22, Z);
		DioFacBoxB(G, HW, HH, 1.8, DioramaPalette::Machine, 0.0, 0.0, 0.0);
		DioFacBoxB(G, HW + 0.04, 0.22, 1.84, bAuto ? DioramaPalette::AccentAutomobile : DioramaPalette::AccentElectronics, 0.0, HH - 0.55, 0.0);
		DioFacBoxB(G, HW * 0.5, 0.35, 0.7, kDioFacHoodTop, 0.0, HH, 0.0);
		DioFacBoxB(G, HW + 0.06, 0.12, 1.86, DioramaPalette::Cap, 0.0, 0.05, 0.0);
		G.PopFrame();

		if (!bAuto)
		{
			G.PushFrame(4.6, 0.22, Z);
			DioFacBoxB(G, 1.6, 1.35, 1.7, kDioFacPackBody, 0.0, 0.0, 0.0);
			DioFacBoxB(G, 1.64, 0.2, 1.74, DioramaPalette::Cardboard, 0.0, 0.9, 0.0);
			G.PopFrame();
		}

		if (QualityTier >= 1)
		{
			const uint32 Arch = QualityTier >= 3 ? DioramaPalette::Gold : DioramaPalette::QcWhite;
			G.PushFrame(1.5, 0.22, Z);
			DioFacBoxB(G, 0.24, 2.2, 0.24, Arch, 0.0, 0.0, -0.98);
			DioFacBoxB(G, 0.24, 2.2, 0.24, Arch, 0.0, 0.0, 0.98);
			DioFacBoxB(G, 0.42, 0.36, 2.2, Arch, 0.0, 2.2, 0.0);
			DioFacBoxB(G, 0.16, 0.06, 1.76, kDioFacQcLamp, 0.0, 2.12, 0.0);
			DioFacBoxB(G, 0.05, 1.6, 0.05, kDioFacQcLamp, 0.0, 0.4, -0.84);
			DioFacBoxB(G, 0.05, 1.6, 0.05, kDioFacQcLamp, 0.0, 0.4, 0.84);
			if (QualityTier >= 2)
			{
				DioFacBoxB(G, 0.95, 0.7, 0.12, DioramaPalette::Dark, 0.0, 1.1, 1.32);
				DioFacBoxB(G, 0.8, 0.55, 0.02, kDioFacScreen, 0.0, 1.18, 1.39);
			}
			G.PopFrame();
		}
	}

	// 시안 arm() 을 평균 자세(어깨 0.55, 팔꿈치 1.3 rad)로 굳힌 형상. 원점 = 받침 바닥.
	void DioFacArmGeometry(FDioramaGeometry& G, bool bYellow)
	{
		const uint32 Body = bYellow ? DioramaPalette::ArmYellow : DioramaPalette::ArmGrey;
		DioFacCylB(G, 0.4, 0.44, 0.34, DioramaPalette::Dark, 0.0, 0.0, 0.0);
		G.PushFrame(0.0, 0.34, 0.0);
		DioFacCylB(G, 0.3, 0.32, 0.34, Body, 0.0, 0.0, 0.0);
		G.PushFrame(0.0, 0.42, 0.0, kDioFacShoulderRest, 0.0, 0.0);
		DioFacBoxB(G, 0.26, 1.15, 0.26, Body, 0.0, 0.0, 0.0);
		G.PushFrame(0.0, 1.15, 0.0, kDioFacElbowRest, 0.0, 0.0);
		DioFacCyl(G, 0.17, 0.17, 0.32, DioramaPalette::Dark, 0.0, 0.0, 0.0, 12, 0.0, 0.0, UE_DOUBLE_HALF_PI);
		DioFacBoxB(G, 0.2, 0.95, 0.2, Body, 0.0, 0.0, 0.0);
		G.PushFrame(0.0, 0.95, 0.0);
		DioFacBoxB(G, 0.3, 0.26, 0.3, DioramaPalette::Dark, 0.0, 0.0, 0.0);
		DioFacCylB(G, 0.06, 0.06, 0.22, kDioFacOrange, 0.0, 0.26, 0.0);
		G.PopFrame();
		G.PopFrame();
		G.PopFrame();
		G.PopFrame();
	}

	void DioFacAddArm(TArray<FDioramaRigDesc>& Rigs, int32 Index, double X, double Z, int32 Side, int32 AutomationTier)
	{
		FDioramaRigDesc& Arm = Rigs.AddDefaulted_GetRef();
		Arm.Kind = EDioramaRigKind::RobotArm;
		Arm.PivotUE = FDioramaGeometry::ToUE(X, 0.22, Z);
		// 시안 yaw0 = π(띠 반대편 팔) ↔ UE Yaw −180
		Arm.YawDeg = Side > 0 ? -180.0 : 0.0;
		Arm.Slot = Index;
		Arm.bActive = true;
		Arm.Amplitude = static_cast<float>(FMath::RadiansToDegrees(kDioFacArmSwayRad));
		Arm.Period = static_cast<float>(UE_DOUBLE_TWO_PI / (kDioFacArmWaveRate * kDioFacArmSpeed[AutomationTier]));
		DioFacArmGeometry(Arm.Geometry, AutomationTier >= 3);
	}

	void DioFacAddArms(TArray<FDioramaRigDesc>& Rigs, int32 Index, int32 AutomationTier)
	{
		const double Z = DioFacZL(Index);
		if (AutomationTier >= 1)
		{
			DioFacAddArm(Rigs, Index, -8.6, Z - 1.25, -1, AutomationTier);
		}
		if (AutomationTier >= 2)
		{
			DioFacAddArm(Rigs, Index, -11.0, Z + 1.25, 1, AutomationTier);
		}
		if (AutomationTier >= 3)
		{
			DioFacAddArm(Rigs, Index, -3.2, Z - 1.25, -1, AutomationTier);
			DioFacAddArm(Rigs, Index, -2.0, Z + 1.25, 1, AutomationTier);
		}
	}

	void DioFacAddPackets(TArray<FDioramaRigDesc>& Rigs, int32 Index, bool bAuto, bool bRunning)
	{
		FDioramaRigDesc& Packets = Rigs.AddDefaulted_GetRef();
		Packets.Kind = EDioramaRigKind::BeltPackets;
		Packets.Slot = Index;
		Packets.bActive = bRunning;
		// 수령대에 닿는 형태(전자 = 상자, 자동차 = 차체+창)의 외곽 상자. UE 축 = (깊이, 길이, 높이)
		Packets.PacketSizeUE = bAuto ? FVector(72.0, 150.0, 60.0) : FVector(80.0, 105.0, 60.0);
		Packets.PacketColor = DioramaModules::Hex(bAuto ? DioramaPalette::AccentAutomobile : DioramaPalette::Cardboard);
		// 경로는 물건 상자 중심 — 띠 윗면에 바닥이 닿게 높이 절반을 올린다.
		const double CenterY = kDioFacBeltTopY + Packets.PacketSizeUE.Z / 200.0;
		const double Z = DioFacZL(Index);
		Packets.PathStartUE = FDioramaGeometry::ToUE(kDioFacPacketStartX, CenterY, Z);
		Packets.PathEndUE = FDioramaGeometry::ToUE(kDioFacPacketEndX, CenterY, Z);
		Packets.PivotUE = Packets.PathStartUE;
		Packets.Speed = kDioFacBeltSpeed;
		Packets.MaxPackets = kDioFacMaxPackets;
	}

	void DioFacRecv(FDioramaGeometry& G, int32 Index, int32 StorageTier, bool bAuto, bool bFull)
	{
		TArray<FVector, TInlineAllocator<16>> Spots;
		G.PushFrame(0.0, 0.0, DioFacZL(Index));
		if (StorageTier <= 0)
		{
			DioFacBoxB(G, 2.1, 0.16, 1.5, DioramaPalette::Pallet, 11.2, 0.0, 0.0);
			for (const double SpotX : { 10.7, 11.7 })
			{
				for (const double SpotZ : { -0.36, 0.36 })
				{
					Spots.Add(FVector(SpotX, 0.16, SpotZ));
				}
			}
		}
		else if (StorageTier <= 2)
		{
			TArray<double, TInlineAllocator<3>> Levels = { 0.18, 1.2, 2.22 };
			if (StorageTier == 1)
			{
				Levels = { 0.18, 1.25 };
			}
			const double RackH = StorageTier == 1 ? 2.3 : 3.3;
			for (const double PostX : { 10.25, 14.15 })
			{
				for (const double PostZ : { -0.62, 0.62 })
				{
					DioFacBoxB(G, 0.12, RackH, 0.12, DioramaPalette::RackUp, PostX, 0.0, PostZ);
				}
			}
			for (const double LevelY : Levels)
			{
				DioFacBoxB(G, 3.96, 0.05, 1.2, DioramaPalette::Shelf, 12.2, LevelY - 0.05, 0.0);
				DioFacBoxB(G, 4.0, 0.12, 0.08, DioramaPalette::RackBeam, 12.2, LevelY - 0.15, -0.62);
				DioFacBoxB(G, 4.0, 0.12, 0.08, DioramaPalette::RackBeam, 12.2, LevelY - 0.15, 0.62);
				for (const double SpotX : { 10.75, 11.73, 12.71, 13.69 })
				{
					Spots.Add(FVector(SpotX, LevelY, 0.0));
				}
			}
		}
		else
		{
			const uint32 Shell = DioramaPalette::Containers[Index % 6];
			DioFacBoxB(G, 6.0, 0.15, 2.4, Shell, 13.0, 0.0, 0.0);
			DioFacBoxB(G, 6.0, 2.3, 0.12, Shell, 13.0, 0.15, -1.14);
			DioFacBoxB(G, 6.0, 2.3, 0.12, Shell, 13.0, 0.15, 1.14);
			DioFacBoxB(G, 0.12, 2.3, 2.4, Shell, 15.94, 0.15, 0.0);
			DioFacBoxB(G, 6.0, 0.14, 0.2, kDioFacContainerRail, 13.0, 2.45, -1.1);
			DioFacBoxB(G, 6.0, 0.14, 0.2, kDioFacContainerRail, 13.0, 2.45, 1.1);
			const uint32 Rib = DioFacShade(Shell);
			for (double RibX = 10.6; RibX < 15.8; RibX += 0.6)
			{
				DioFacBoxB(G, 0.06, 2.1, 0.04, Rib, RibX, 0.25, 1.21);
			}
			for (const double SpotY : { 0.15, 0.73 })
			{
				for (const double SpotZ : { -0.5, 0.5 })
				{
					for (const double SpotX : { 11.0, 12.2, 13.4, 14.6 })
					{
						Spots.Add(FVector(SpotX, SpotY, SpotZ));
					}
				}
			}
		}
		// 완료 라인은 보관 칸이 전부 찬다(시안 fill = CAP[S] = 칸 수).
		if (bFull)
		{
			const double Scale = bAuto ? kDioFacUnitScaleAuto : kDioFacUnitScaleElec;
			for (const FVector& Spot : Spots)
			{
				DioFacUnit(G, bAuto, Scale, Spot.X, Spot.Y, Spot.Z);
			}
		}
		G.PopFrame();
	}

	void DioFacGen(FDioramaGeometry& G, double X, double Z)
	{
		G.PushFrame(X, 0.0, Z);
		DioFacBoxB(G, 2.6, 1.9, 2.6, DioramaPalette::Gen, 0.0, 0.0, 0.0);
		DioFacBoxB(G, 2.64, 0.18, 2.64, kDioFacGenBand, 0.0, 0.25, 0.0);
		for (int32 K = 0; K < 4; ++K)
		{
			DioFacBoxB(G, 0.08, 1.0, 0.04, kDioFacGenVent, -0.9 + K * 0.6, 0.6, 1.31);
		}
		DioFacCylB(G, 1.05, 1.05, 0.28, DioramaPalette::GenTop, 0.0, 1.9, 0.0, 16);
		G.PushFrame(0.0, 2.24, 0.0);
		DioFacCylB(G, 0.18, 0.18, 0.12, DioramaPalette::Dark, 0.0, -0.06, 0.0);
		for (int32 K = 0; K < 3; ++K)
		{
			DioFacBox(G, 1.8, 0.04, 0.28, kDioFacFanBlade, 0.0, 0.0, 0.0, 0.0, K * UE_DOUBLE_PI / 3.0);
		}
		G.PopFrame();
		G.PopFrame();
	}

	void DioFacGenerators(FDioramaGeometry& G, int32 RushTier)
	{
		if (RushTier >= 1)
		{
			DioFacGen(G, 19.6, -10.4);
		}
		if (RushTier >= 2)
		{
			DioFacGen(G, 19.6, -6.8);
		}
		if (RushTier >= 3)
		{
			G.PushFrame(19.6, 0.0, -3.0);
			DioFacBoxB(G, 2.4, 0.6, 2.4, DioramaPalette::Gen, 0.0, 0.0, 0.0);
			DioFacCylB(G, 0.8, 0.8, 3.0, kDioFacCore, 0.0, 0.6, 0.0, 16);
			for (const double RingY : { 1.3, 2.3, 3.3 })
			{
				DioFacCylB(G, 0.92, 0.92, 0.18, DioramaPalette::GenTop, 0.0, RingY, 0.0, 16);
			}
			DioFacCylB(G, 0.95, 0.95, 0.3, DioramaPalette::GenTop, 0.0, 3.6, 0.0, 16);
			G.PopFrame();
		}
	}

	void DioFacYard(FDioramaGeometry& G, TArray<FDioramaRigDesc>& Rigs, bool bAuto)
	{
		// AGV 는 맞는 리그 종류가 없어 시안 시작 자리(z −8, 짐 실은 방향)에 세워 둔다.
		G.PushFrame(17.3, 0.0, -8.0, 0.0, -UE_DOUBLE_HALF_PI, 0.0);
		DioFacBoxB(G, 1.0, 0.34, 0.7, DioramaPalette::Agv, 0.0, 0.1, 0.0);
		DioFacCylB(G, 0.07, 0.07, 0.12, kDioFacBeacon, 0.32, 0.44, 0.0);
		DioFacUnit(G, bAuto, kDioFacAgvLoadScale, -0.05, 0.44, 0.0);
		G.PopFrame();

		FDioramaRigDesc& TruckRig = Rigs.AddDefaulted_GetRef();
		TruckRig.Kind = EDioramaRigKind::Truck;
		TruckRig.PathStartUE = FDioramaGeometry::ToUE(kDioFacTruckWaitX, 0.0, kDioFacTruckLaneZ);
		TruckRig.PathEndUE = FDioramaGeometry::ToUE(kDioFacTruckGateX, 0.0, kDioFacTruckLaneZ);
		TruckRig.PivotUE = TruckRig.PathStartUE;
		// 대기 자세 = 시안 차체 앞(−x)을 공장 쪽으로 = UE −Y 방향
		TruckRig.YawDeg = -90.0;
		TruckRig.Speed = static_cast<float>(kDioFacTruckSpeed);
		TruckRig.Period = static_cast<float>(kDioFacTruckPeriod);
	}

	void DioFacExtras(FDioramaGeometry& G)
	{
		DioFacBoxB(G, 17.6, 0.04, 3.4, DioramaPalette::Road, 14.2, 0.0, 12.9);
		for (double X = 6.4; X < 22.0; X += 2.2)
		{
			DioFacBoxB(G, 1.1, 0.05, 0.14, DioramaPalette::RoadLine, X, 0.0, 12.9);
		}
		DioFacBoxB(G, 0.3, 3.6, 0.3, DioramaPalette::Steel, 21.9, 0.0, 11.0);
		DioFacBoxB(G, 0.3, 3.6, 0.3, DioramaPalette::Steel, 21.9, 0.0, 14.8);
		DioFacBoxB(G, 0.4, 0.4, 4.1, DioramaPalette::Steel, 21.9, 3.6, 12.9);
		DioFacBoxB(G, 0.14, 0.7, 2.4, kDioFacSignBoard, 21.9, 4.0, 12.9);

		G.PushFrame(-3.2, 0.0, 13.0);
		DioFacBoxB(G, 0.18, 1.3, 0.18, DioramaPalette::Steel, -1.5, 0.0, 0.0);
		DioFacBoxB(G, 0.18, 1.3, 0.18, DioramaPalette::Steel, 1.5, 0.0, 0.0);
		DioFacBox(G, 3.8, 1.7, 0.16, kDioFacSignBoard, 0.0, 1.3 + 0.85, 0.0);
		G.PopFrame();
	}

	void DioFacGhost(FDioramaGeometry& G, double X0, double X1, double Z0, double Z1)
	{
		auto DashAlongX = [&G, X0, X1](double LaneZ)
		{
			for (double DashX = X0; DashX < X1 - 0.4; DashX += 1.5)
			{
				DioFacBox(G, 0.8, 0.04, 0.16, kDioFacGhost, DashX + 0.4, 0.03, LaneZ);
			}
		};
		auto DashAlongZ = [&G, Z0, Z1](double LaneX)
		{
			for (double DashZ = Z0; DashZ < Z1 - 0.4; DashZ += 1.5)
			{
				DioFacBox(G, 0.16, 0.04, 0.8, kDioFacGhost, LaneX, 0.03, DashZ + 0.4);
			}
		};
		DashAlongX(Z0);
		DashAlongX(Z1);
		DashAlongZ(X0);
		DashAlongZ(X1);
		for (double MarkX = X0 + 1.2; MarkX < X1; MarkX += 4.4)
		{
			DioFacBox(G, 0.5, 0.04, 0.5, kDioFacGhost, MarkX, 0.03, (Z0 + Z1) / 2.0, 0.0, UE_DOUBLE_PI / 4.0);
		}
	}
}

FBox2D DioramaModules::FactoryRect(const FDioramaFactorySpec& Spec, bool bMiniature)
{
	if (bMiniature)
	{
		const double X1 = Spec.RushTier > 0 ? 21.8 : 17.4;
		return FBox2D(FVector2D(-22.6, kDioFacZB - 2.2), FVector2D(X1, DioFacZFront(3 + Spec.LineTier()) + 2.2));
	}
	return FBox2D(FVector2D(-23.0, -15.0), FVector2D(23.0, 15.0));
}

void DioramaModules::BuildFactory(const FDioramaFactorySpec& Spec, bool bMiniature, FDioramaModuleBuild& Out)
{
	Out.Module = EDioramaModule::Factory;
	Out.RectXZ = FactoryRect(Spec, bMiniature);
	Out.Static.Reset();
	Out.Rigs.Reset();

	const bool bAuto = Spec.Industry == ECompanyType::Automobile;
	const int32 LineCount = 3 + Spec.LineTier();
	const int32 AutomationTier = FMath::Clamp(Spec.AutomationTier, 0, 3);
	const int32 QualityTier = FMath::Clamp(Spec.QualityTier, 0, 3);
	const int32 StorageTier = FMath::Clamp(Spec.StorageTier, 0, 3);
	const int32 RushTier = FMath::Clamp(Spec.RushTier, 0, 3);
	FDioramaGeometry& G = Out.Static;

	DioFacHall(G, LineCount);
	for (int32 Index = 0; Index < LineCount; ++Index)
	{
		const EDioramaSlotState State = Spec.Slots.IsValidIndex(Index) ? Spec.Slots[Index] : EDioramaSlotState::Empty;
		DioFacHopper(G, Index, bAuto);
		DioFacLine(G, Index, bAuto, QualityTier);
		DioFacAddPackets(Out.Rigs, Index, bAuto, State == EDioramaSlotState::Running);
		DioFacAddArms(Out.Rigs, Index, AutomationTier);
		DioFacRecv(G, Index, StorageTier, bAuto, State == EDioramaSlotState::Completed);
	}
	DioFacGenerators(G, RushTier);

	if (!bMiniature)
	{
		DioFacYard(G, Out.Rigs, bAuto);
		DioFacExtras(G);
		if (Spec.LineTier() < 3)
		{
			DioFacGhost(G, kDioFacX0, kDioFacX1, DioFacZFront(LineCount) + 0.3, DioFacZFront(LineCount + 1));
		}
	}
}
