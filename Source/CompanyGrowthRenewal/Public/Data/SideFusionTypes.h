#pragma once

#include "CoreMinimal.h"
#include "Data/SideContentSaveData.h"
#include "Enum/CompanyType.h"
#include "Table/MissionTable.h"
#include "Templates/Function.h"
#include "SideFusionTypes.generated.h"

struct FFusionProjectTable;

UENUM(BlueprintType)
enum class EFusionBlockReason : uint8
{
	None,
	NotLoaded,
	NoProject,
	ActiveProject,
	HQLocked,
	MissingIndustry,
	SlotCountMismatch,
	EmptySlot,
	InvalidBuilding,
	DuplicateBuilding,
	IndustryMismatch,
	NoCrew,
	PowerShort
};

USTRUCT(BlueprintType)
struct FFusionClaimResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName FusionKey;

	UPROPERTY(BlueprintReadOnly)
	int32 PreviousLevel = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 NewLevel = 0;

	UPROPERTY(BlueprintReadOnly)
	TArray<FMissionReward> Rewards;
};

/** 저장된 보유 건물을 기준으로 수집한 판정 입력. 오피스 방문 여부와 무관하다. */
struct FFusionBuildingCandidate
{
	int32 BuildingIndex = INDEX_NONE;
	ECompanyType Industry = ECompanyType::None;
	float TeamPower = 0.0f;
	bool bHasCrew = false;
};

namespace FusionRules
{
	COMPANYGROWTHRENEWAL_API bool IsValidProject(const FFusionProjectTable& Row);
	/** 요구 업종이 전부 이번 빌드에 있어야 참. 판정자를 주입받아 월드·세이브 없이 테스트된다. */
	COMPANYGROWTHRENEWAL_API bool IsProjectAvailable(const FFusionProjectTable& Row,
		TFunctionRef<bool(ECompanyType)> IsIndustryAvailable);
	/** Keys 순서를 보존하며 미가용 콜라보만 뺀다. 행을 못 찾은 키는 남긴다 — 표시는 여는 쪽으로 실패한다. */
	COMPANYGROWTHRENEWAL_API TArray<FName> FilterAvailableKeys(const TArray<FName>& Keys,
		TFunctionRef<bool(FName, FFusionProjectTable&)> FindRow,
		TFunctionRef<bool(ECompanyType)> IsIndustryAvailable);
	COMPANYGROWTHRENEWAL_API float Requirement(const FFusionProjectTable& Row, int32 Level);
	COMPANYGROWTHRENEWAL_API TArray<FMissionReward> Rewards(const FFusionProjectTable& Row, int32 Level);
	COMPANYGROWTHRENEWAL_API EFusionBlockReason ValidateSelection(const TArray<ECompanyType>& Industries,
		const TArray<FFusionBuildingCandidate>& OwnedBuildings, const TArray<int32>& Selection, float RequiredPower);
	/** 상태를 먼저 소비한다. 이후 보상 방송이 같은 수령을 다시 호출해도 재지급되지 않는다. */
	COMPANYGROWTHRENEWAL_API bool ConsumeClaim(FSideContentSaveData& State, const FFusionProjectTable& Row,
		int64 NowUnix, bool bServerTimeValid, FFusionClaimResult& OutResult);
}
