#pragma once

#include "CoreMinimal.h"
#include "Entity/OverseasDiorama/DioramaGeometry.h"
#include "Entity/OverseasDiorama/OverseasDioramaTypes.h"

// 시안 src.html 의 색표(C·CONT·RES·IND) — 시안과 같은 이름·같은 값. 같은 값은 한 색 버킷으로 합쳐진다.
namespace DioramaPalette
{
	constexpr uint32 Paving = 0xC7BFB1;
	constexpr uint32 GoldTop = 0xDDB56A;
	constexpr uint32 GoldMid = 0xC79A50;
	constexpr uint32 GoldLow = 0xA27638;
	constexpr uint32 Hedge = 0x9CC73A;
	constexpr uint32 Road = 0x7E7B78;
	constexpr uint32 RoadLine = 0xE8E3D8;
	constexpr uint32 Floor = 0xB4BBC4;
	constexpr uint32 Wall = 0xE4DED2;
	constexpr uint32 Cap = 0x98A2AF;
	constexpr uint32 Glass = 0x5DAFC0;
	constexpr uint32 Steel = 0x8E99A8;
	constexpr uint32 Dark = 0x3C434E;
	constexpr uint32 Frame = 0x59616E;
	constexpr uint32 Leg = 0x3E4550;
	constexpr uint32 Machine = 0xE8EBEF;
	constexpr uint32 Slate = 0x6E7B8E;
	constexpr uint32 ArmGrey = 0xD9DEE5;
	constexpr uint32 ArmYellow = 0xF2B33D;
	constexpr uint32 QcWhite = 0xF1F3F6;
	constexpr uint32 Gold = 0xE3B54A;
	constexpr uint32 RackUp = 0x4F6B8C;
	constexpr uint32 RackBeam = 0xE58A2B;
	constexpr uint32 Shelf = 0xB7C0CB;
	constexpr uint32 Pallet = 0xB98A55;
	constexpr uint32 Cardboard = 0xC99A5E;
	constexpr uint32 Tape = 0xE9D2A3;
	constexpr uint32 Gen = 0x5D6B80;
	constexpr uint32 GenTop = 0x3E4756;
	constexpr uint32 Trunk = 0x8B5A2B;
	constexpr uint32 Roof = 0x6F7886;
	constexpr uint32 Plinth = 0x8A95A5;
	constexpr uint32 Agv = 0xF0A23A;
	constexpr uint32 Line = 0xE8C547;
	constexpr uint32 Earth1 = 0xB48A5E;
	constexpr uint32 Earth2 = 0xA27A50;
	constexpr uint32 Earth3 = 0x8E6A45;
	constexpr uint32 Mach = 0xF0A23A;
	constexpr uint32 MachY = 0xF2C23D;
	constexpr uint32 Pipe = 0x5B6370;
	constexpr uint32 Silo = 0xD9DEE5;
	constexpr uint32 Tank = 0xEDEFF2;
	constexpr uint32 Wood = 0xA8743F;
	constexpr uint32 Pine = 0x4E8B3A;
	constexpr uint32 Crane = 0xC94A3B;
	constexpr uint32 Quay = 0xBFC3C8;
	constexpr uint32 Hull = 0x2F3E5C;
	constexpr uint32 Water = 0x3E86BF;
	constexpr uint32 Belt = 0x343B47;
	constexpr uint32 Containers[6] = { 0x4A79A8, 0xC0704A, 0x5E9466, 0xB99A4E, 0x7E6A9E, 0xA85A5A };

	constexpr uint32 AccentElectronics = 0xF59E0B;
	constexpr uint32 AccentAutomobile = 0xEF4444;

	// 시안 RES[*].c
	uint32 ResourceColor(EResourceType Resource);
}

enum class EDioramaRigKind : uint8
{
	BeltPackets,
	RobotArm,
	OilPump,
	Excavator,
	CraneTrolley,
	Truck,
	Ship,
	Flare
};

// 움직이는 부품 하나. 정지 형상과 달리 무대가 컴포넌트 하나로 만들고 Tick 에서 변환만 바꾼다.
struct FDioramaRigDesc
{
	EDioramaRigKind Kind = EDioramaRigKind::BeltPackets;
	// 모듈 원점 기준 UE 좌표(cm). Geometry 는 이 피벗을 원점으로 한 시안 공간 형상이다.
	FVector PivotUE = FVector::ZeroVector;
	double YawDeg = 0.0;
	FDioramaGeometry Geometry;
	int32 Slot = INDEX_NONE;
	bool bActive = true;
	// BeltPackets·CraneTrolley·Truck 의 경로(모듈 원점 기준 UE cm)
	FVector PathStartUE = FVector::ZeroVector;
	FVector PathEndUE = FVector::ZeroVector;
	float Speed = 0.f;      // BeltPackets m/s
	float MeshScale = 1.f;  // OilPump: original skeletal mesh length normalized to the diorama footprint
	float Amplitude = 0.f;  // 흔들림 각(도) 또는 이동량(m)
	float Period = 1.f;     // 초
	FColor PacketColor = FColor::White;
	FVector PacketSizeUE = FVector(60.0);
	int32 MaxPackets = 6;
};

struct FDioramaModuleBuild
{
	EDioramaModule Module = EDioramaModule::Factory;
	// 시안 미터, 모듈 원점 기준 [x0,z0]-[x1,z1] (시안 r = [x0,x1,z0,z1])
	FBox2D RectXZ = FBox2D(ForceInit);
	FDioramaGeometry Static;
	TArray<FDioramaRigDesc> Rigs;
};

struct FDioramaLayoutItem
{
	EDioramaModule Module = EDioramaModule::Factory;
	FVector2D OffsetXZ = FVector2D::ZeroVector; // 시안 ox, oz (미터)
	FBox2D RectXZ = FBox2D(ForceInit);
};

namespace DioramaModules
{
	FColor Hex(uint32 RGB);

	// 공장 (DioramaFactoryModule.cpp)
	FBox2D FactoryRect(const FDioramaFactorySpec& Spec, bool bMiniature);
	void BuildFactory(const FDioramaFactorySpec& Spec, bool bMiniature, FDioramaModuleBuild& Out);

	// 광산 (DioramaMineModule.cpp)
	FBox2D MineRect(const FDioramaMineSpec& Spec);
	void BuildMine(const FDioramaMineSpec& Spec, bool bMiniature, FDioramaModuleBuild& Out);

	// 항구 (DioramaPortModule.cpp)
	FBox2D PortRect();
	void BuildPort(const FDioramaPortSpec& Spec, bool bMiniature, FDioramaModuleBuild& Out);

	// 배치·땅 조각 (DioramaLayout.cpp)
	TArray<FDioramaLayoutItem> Layout(const FOverseasDioramaSpec& Spec, bool bMiniature);
	FBox2D BuildTile(const TArray<FDioramaLayoutItem>& Items, FDioramaGeometry& Out);
}
