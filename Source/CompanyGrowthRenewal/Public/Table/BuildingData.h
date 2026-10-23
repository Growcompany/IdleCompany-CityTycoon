#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h" // Required for FTableRowBase
#include "BuildingData.generated.h"

// DT의 논리 칸수에 곱하는 공통 건물 점유 간격(cm). 모든 도시에서 같은 건물은 같은 공간을 예약한다.
// 메시 모듈 합집합의 XY 폭 × MeshScale / 축별 칸수의 최댓값을 cm 단위로 올림한다.
// 40종의 최대치는 B40의 1862.03cm/칸이며, 비대칭 피벗은 점유 사각형 중심으로 보정한다.
constexpr float BuildingFootprintCellSize = 1863.f;

// Plot의 저작 간격은 도시 지면 규격을 따른다. 건물 점유 간격을 줄여도 부지 공간은 유지한다.
constexpr float PlotBaseCellSizeRiverwalk = 4300.f;
constexpr float PlotBaseCellSizeCompact = 2000.f;

// 에디터와 게임 월드가 같은 Plot 간격을 쓰도록 PIE 접두사를 포함한 맵 이름으로 판정한다.
COMPANYGROWTHRENEWAL_API float ResolvePlotBaseCellSize(const UWorld* World);

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
	int32 BaseEmployees = 1;

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
	// 메시 표시 배수. 원본 메시 폭이 종마다 제각각(정사각 계열 2,500~7,626)이라 같은 칸수인데 실제 크기가
	// 1.7배씩 달랐다 — 종별로 이 값을 줘 "같은 칸수 = 같은 크기"로 수렴시킨다.
	// 적용 = ConstructVisuals 의 루트 스케일. 세이브에 직렬화된 Scale 보다 이 DT 값이 권위다.
	// 칸수(Footprint*Cells)는 건드리지 않으므로 직원수·건설비 밸런스와 무관하다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprint", meta = (ClampMin = "0.05"))
	float MeshScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprint", meta = (ClampMin = "1"))
	int32 FootprintWidthCells = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprint", meta = (ClampMin = "1"))
	int32 FootprintDepthCells = 1;

	// ========== Income ==========

	// 종마다 다른 수익 보너스. 본사 해금 레벨 순으로 (크기 바닥 배수 × (1+보너스))가 매 레벨 오르도록 DT 에서 맞춘다
	// (프로젝트 수익은 직원 수에 비례하지 않으므로 큰 빌딩도 이 배율로만 더 번다).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Income", meta = (ClampMin = "0.0"))
	float IncomeBonusPercent = 0.f;

	// ========== Helper Functions ==========

	// 칸수 클래스 바닥 배수 — 큰 빌딩의 보너스 값을 작게 유지한다(보너스 = 목표 배율 ÷ 바닥 − 1).
	float GetClassIncomeMultiplier() const
	{
		const int32 Cells = FMath::Max(1, FootprintWidthCells * FootprintDepthCells);
		if (Cells >= 9) return 2.4f;
		if (Cells >= 2) return 1.6f;
		return 1.0f;
	}

	// 이 건물 종이 가진 수익 배율 = (1 + 보너스) × 클래스 바닥. 칸수 비례분은 인원에서 이미 반영된다.
	float GetModelIncomeMultiplier() const
	{
		return (1.f + FMath::Max(0.f, IncomeBonusPercent) * 0.01f) * GetClassIncomeMultiplier();
	}

	// 최대 층수 계산 (MaxModuleCount × FloorsPerModule)
	int32 GetMaxFloors() const { return MaxModuleCount * FloorsPerModule; }

	// **게임 내 유일한 인원 상한** = (BaseEmployees + PerAddedFloor × 증축 층수) × footprint 칸수
	//
	// 한 층에 들어가는 인원이 그 층의 바닥 면적에 비례한다 — 2×2 는 1×1 의 4배다.
	// BaseEmployees 는 "1칸 1층당" 값이라 신축(증축 0층) 상한이 곧 BaseEmployees × 칸수 가 된다.
	// ⚠ 책상 개수는 상한이 아니다 — 책상은 무제한 배치이고, 플레이어가 관리하는 수는 사람 하나뿐이다.
	int32 GetEmployeeCapacity(int32 AddedFloors) const
	{
		const int32 Cells = FMath::Max(1, FootprintWidthCells * FootprintDepthCells);
		const float PerCellCapacity = static_cast<float>(FMath::Max(0, BaseEmployees))
			+ PerAddedFloor * static_cast<float>(FMath::Max(0, AddedFloors));
		return FMath::Max(0, FMath::FloorToInt(PerCellCapacity * static_cast<float>(Cells)));
	}

	FBuildingData()
		: BaseMesh(nullptr)
		, BodyMesh(nullptr)
		, TopMesh(nullptr)
		, TopEmptyMesh(nullptr)
		, MainMaterial(nullptr)
		, RoofElementsMaterial(nullptr)
		, MaxModuleCount(10)
		, FloorsPerModule(1)
		, BaseEmployees(1)
		, PerAddedFloor(1.0f)
		, PerCell(2.0f)
		, BuildCostBrick(100)
		, BuildCostMoney(10000)
		, RequiredHQLevel(0)
		, FootprintWidthCells(1)
		, FootprintDepthCells(1)
	{}
};
