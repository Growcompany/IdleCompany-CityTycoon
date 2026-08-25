#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h" // Required for FTableRowBase
#include "Enum/BuildingTraitTarget.h"
#include "BuildingData.generated.h"

// Footprint 격자 셀 한 변 크기(cm). 43m = 한 칸(= 가장 큰 1x1 건물이 들어맞는 크기).
// footprint 분류는 ceil(올림): 셀보다 크면 2칸+ 로 올라가 건물이 칸을 절대 안 삐져나옴.
// FootprintWidthCells/DepthCells 와 도시 부지(CityPlot) 격자가 공유하는 단일 진실 소스 상수.
constexpr float FootprintCellSize = 4300.f;

// 모뉴먼트 특수빌딩의 키스톤 오라 + 강화 파라미터. 비-특수 빌딩은 bIsKeystone=false 기본.
USTRUCT(BlueprintType)
struct FKeystoneAuraData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keystone")
	bool bIsKeystone = false;

	// 오라가 가산되는 대상 수량(효과레이어 6종 중 하나).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keystone")
	EBuildingTraitTarget AuraTarget = EBuildingTraitTarget::None;

	// true = 전 도시(반경 무시). 랜드마크 타워 전용.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keystone")
	bool bGlobal = false;

	// Lv1 기준 반경(셀). cm = 셀 × FootprintCellSize. bGlobal이면 무시.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keystone", meta = (ClampMin = "0"))
	float BaseRadiusCells = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keystone", meta = (ClampMin = "0"))
	float RadiusPerLevelCells = 1.0f;

	// 오라 기본 효과 %. KeystoneAuraPower 강화 배수가 적용된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keystone", meta = (ClampMin = "0"))
	float BasePercent = 10.0f;

	// 자체 패시브 산출(직원 없는 모뉴먼트의 단독 수익). 레벨당 증가.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keystone", meta = (ClampMin = "0"))
	int64 BasePassiveOutput = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keystone", meta = (ClampMin = "0"))
	int64 PassivePerLevel = 0;
};

/**
 * FBuildingData
 *
 * 건물의 비주얼 및 스탯 데이터를 관리하는 구조체
 * - Mesh: 건물의 Base, Body, Top 메시들
 * - Material: 건물의 메인 머티리얼과 지붕 머티리얼
 * - Building Stats: 등급, 층수, 인원 관련 데이터
 */
USTRUCT(BlueprintType)
struct FBuildingData : public FTableRowBase
{
	GENERATED_BODY()

public:
	// ========== Mesh ==========
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TSoftObjectPtr<UStaticMesh> BaseMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TSoftObjectPtr<UStaticMesh> BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TSoftObjectPtr<UStaticMesh> TopMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TSoftObjectPtr<UStaticMesh> TopEmptyMesh;

	// ========== Material ==========
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	TSoftObjectPtr<UMaterialInterface> MainMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	TSoftObjectPtr<UMaterialInterface> RoofElementsMaterial;

	// ========== Building Stats ==========

	// 최대 모듈 개수 (고도 제한)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Stats", meta = (ClampMin = "1"))
	int32 MaxModuleCount = 10;

	// 모듈 1개당 층수 (시각적으로 몇 층처럼 보이는지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Stats", meta = (ClampMin = "1"))
	int32 FloorsPerModule = 1;

	// 1칸 건물의 인원(칸수·층수 가산 전). 밸런스 시뮬 결과로 조정하는 노브 — 코드 상수로 올리지 말 것.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Stats", meta = (ClampMin = "0"))
	int32 BaseEmployees = 2;

	// 증축 1층당 추가 인원. 인원 상한이 자라는 유일한 축(기본 구성) — 코드 상수로 올리지 말 것.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Stats", meta = (ClampMin = "0.0"))
	float PerAddedFloor = 1.0f;

	// 1칸 초과분 1칸당 추가 인원. BaseEmployees 와 같은 값이면 "칸당 N명" 정비례가 된다(현 구성: 칸당 2명).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Stats", meta = (ClampMin = "0.0"))
	float PerCell = 2.0f;

	// 건설 비용 (Brick)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Stats", meta = (ClampMin = "0"))
	int32 BuildCostBrick = 100;

	// 건설 비용 (Money)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Stats", meta = (ClampMin = "0"))
	int64 BuildCostMoney = 10000;

	// 언락 조건 (필요 본사 레벨, 0이면 처음부터 해금)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Stats", meta = (ClampMin = "0"))
	int32 RequiredHQLevel = 0;

	// Footprint 격자 칸수 (28.4m 셀 기준, 메시 비율로 산출). 시각 오토핏 + 경제(값∝칸수)에 사용.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprint", meta = (ClampMin = "1"))
	int32 FootprintWidthCells = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprint", meta = (ClampMin = "1"))
	int32 FootprintDepthCells = 1;

	// ========== Keystone (특수 모뉴먼트) ==========
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Keystone")
	FKeystoneAuraData KeystoneAura;

	// ========== Helper Functions ==========

	// 최대 층수 계산 (MaxModuleCount × FloorsPerModule)
	int32 GetMaxFloors() const { return MaxModuleCount * FloorsPerModule; }

	// **게임 내 유일한 인원 상한** = (BaseEmployees + PerAddedFloor × 증축 층수) × footprint 칸수
	//
	// 한 층에 들어가는 인원이 그 층의 바닥 면적에 비례한다 — 2×2 는 1×1 의 4배다.
	// BaseEmployees 는 "1칸 1층당" 값이라 신축(증축 0층) 상한이 곧 BaseEmployees × 칸수 가 된다.
	// 모뉴먼트(키스톤)는 직원을 받지 않는다.
	// ⚠ 책상 개수는 상한이 아니다 — 책상은 무제한 배치이고, 플레이어가 관리하는 수는 사람 하나뿐이다.
	int32 GetEmployeeCapacity(int32 AddedFloors) const
	{
		if (IsKeystone())
		{
			return 0;
		}
		const int32 Cells = FMath::Max(1, FootprintWidthCells * FootprintDepthCells);
		const float PerCellCapacity = static_cast<float>(FMath::Max(0, BaseEmployees))
			+ PerAddedFloor * static_cast<float>(FMath::Max(0, AddedFloors));
		return FMath::Max(0, FMath::FloorToInt(PerCellCapacity * static_cast<float>(Cells)));
	}

	// 키스톤(특수 모뉴먼트) 빌딩 여부
	bool IsKeystone() const { return KeystoneAura.bIsKeystone; }

	FBuildingData()
		: BaseMesh(nullptr)
		, BodyMesh(nullptr)
		, TopMesh(nullptr)
		, TopEmptyMesh(nullptr)
		, MainMaterial(nullptr)
		, RoofElementsMaterial(nullptr)
		, MaxModuleCount(10)
		, FloorsPerModule(1)
		, BaseEmployees(2)
		, PerAddedFloor(1.0f)
		, PerCell(2.0f)
		, BuildCostBrick(100)
		, BuildCostMoney(10000)
		, RequiredHQLevel(0)
		, FootprintWidthCells(1)
		, FootprintDepthCells(1)
	{}
};
