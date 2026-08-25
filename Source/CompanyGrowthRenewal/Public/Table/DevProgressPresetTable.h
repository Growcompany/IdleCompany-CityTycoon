#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/CompanyType.h"
#include "DevProgressPresetTable.generated.h"

/**
 * 개발용 진행 상태 프리셋 1행 = 1 시나리오 (Early/Mid/Late).
 * TargetTier 가 앵커이고 이력 개수·빌딩 레벨 하한이 여기서 역산된다 — 개별 값을 나열하지 않는다.
 */
USTRUCT(BlueprintType)
struct FDevProgressPresetRow : public FTableRowBase
{
	GENERATED_BODY()

	// 앵커. 이력 개수 = (TargetTier-1)*10 + a, 빌딩 레벨 하한 = (TargetTier-1)*2 (티어 해금 AND 조건).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor")
	int32 TargetTier = 5;

	// 재화 시나리오 (DT_TestResourceScenarios RowName). 비우면 자원 미적용.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor")
	FName ResourceScenarioRow = NAME_None;

	// 같은 프리셋을 몇 번 돌려도 같은 결과가 나오게 하는 고정 시드.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor")
	int32 RandomSeed = 12345;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	int32 HQLevel = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City")
	int32 OwnedPlotCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City")
	int32 ClearedCompanyCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Employee")
	int32 EmployeesPerBuilding = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Employee")
	int32 EmployeeLevelMin = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Employee")
	int32 EmployeeLevelMax = 20;

	// 0~15 (MaxEnhancementLevel 기준 — 치트 SetRank 의 0~12 클램프가 아니다).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Employee")
	int32 EmployeeEnhanceMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Employee")
	int32 EmployeeEnhanceMax = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project")
	int32 OperatingProjectCount = 2;

	// 금고에 쌓아둘 방치 시간. 0 이면 오프라인 정산 시드를 건너뛴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project")
	float OfflineHours = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldMap")
	int32 FactoryLinesPerCountry = 2;

	// 채광 라인을 저장 한도의 몇 % 까지 채울지 (0~1).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldMap")
	float MineFillRatio = 0.6f;
};

/** 프리셋당 N행 — 지어놓을 빌딩 1채의 스펙. PresetID 로 묶는다. */
USTRUCT(BlueprintType)
struct FDevPresetBuildingRow : public FTableRowBase
{
	GENERATED_BODY()

	// FDevProgressPresetRow 의 RowName 과 일치하는 행만 선택된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key")
	FName PresetID = NAME_None;

	// DT_CityPlot RowName. 이 부지 위에 짓는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FName PlotId = NAME_None;

	// DT_Interactable RowName. 못 찾으면 EntityManager 가 조용히 스킵하므로 시드 로그로 잡아야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FName InteractableName = NAME_None;

	// 층수 = FBuildingSaveData::Body_Module_Copies. Brick 축이라 성급(StarRating) 합산에서는 제외된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spec")
	int32 Floors = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spec")
	ECompanyType CompanyType = ECompanyType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spec")
	int32 SkinID = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spec")
	int32 LightID = 0;

	// "VaultCapacity:5;MarketingPower:3" — EBuildingEnhancementType 이름:레벨, 세미콜론 구분.
	// 강화 레벨 직접 세터가 없어 InitializeFromSaveData 경유가 유일 경로라 문자열로 받는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spec")
	FString Enhancements;
};
