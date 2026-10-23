#pragma once

#include "CoreMinimal.h"
#include "QuestTypes.generated.h"

UENUM(BlueprintType)
enum class EQuestPeriod : uint8
{
	Daily    UMETA(DisplayName = "일일"),
	Weekly   UMETA(DisplayName = "주간"),
	Lifetime UMETA(DisplayName = "업적"),
};

// 진행을 세는 축. Increment 계열은 ReportCounter 로 누적(주기 롤오버 시 0), Absolute 계열은 게터로 현재값을 읽는다.
UENUM(BlueprintType)
enum class EQuestCounter : uint8
{
	None UMETA(Hidden),
	// ---- Increment ----
	RevenueCollect,
	BrickCollect,
	ProjectLaunch,
	EnhanceAttempt,
	ShopPurchase,
	GachaPull,
	RecruitHire,
	LoginDay,
	CompanyAcquire,
	PlotAcquire,
	TraitEquip,
	SkinEquip,
	FloorRaise,
	// ---- Absolute ----
	TotalRevenue,
	BuildingCount,
	EmployeeCount,
	ProjectsCompleted,
	SGradeProjects,
	HQLevel,
	MaxStar,
	CodexDiscovered,
	CompanyTitleRank,
	OwnedPlots,
};

UENUM(BlueprintType)
enum class EQuestState : uint8
{
	InProgress,
	Claimable,
	Claimed,
	Hidden,
};
