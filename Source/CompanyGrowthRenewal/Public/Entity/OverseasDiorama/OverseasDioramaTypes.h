#pragma once

#include "CoreMinimal.h"
#include "Enum/CountryType.h"
#include "Enum/CompanyType.h"
#include "Enum/ResourceType.h"
#include "OverseasDioramaTypes.generated.h"

class UGameInstance;

UENUM()
enum class EDioramaModule : uint8
{
	Factory,
	Mine,
	Port
};

UENUM()
enum class EDioramaSlotState : uint8
{
	Empty,
	Running,
	Completed
};

UENUM()
enum class EDioramaMineKind : uint8
{
	Pit,
	Pump,
	Lumber
};

USTRUCT()
struct COMPANYGROWTHRENEWAL_API FDioramaFactorySpec
{
	GENERATED_BODY()

	UPROPERTY() bool bPresent = false;
	UPROPERTY() ECompanyType Industry = ECompanyType::None;
	// 설비 자리 수 = 강화가 정한 최대 라인 수. 라인이 없는 자리는 Empty.
	UPROPERTY() TArray<EDioramaSlotState> Slots;
	UPROPERTY() int32 AutomationTier = 0;
	UPROPERTY() int32 QualityTier = 0;
	UPROPERTY() int32 StorageTier = 0;
	UPROPERTY() int32 RushTier = 0;

	int32 LineTier() const { return FMath::Clamp(Slots.Num() - 3, 0, 3); }
};

USTRUCT()
struct COMPANYGROWTHRENEWAL_API FDioramaMineUnit
{
	GENERATED_BODY()

	UPROPERTY() EResourceType Resource = EResourceType::None;
	UPROPERTY() EDioramaSlotState State = EDioramaSlotState::Empty;
	UPROPERTY() float Fill01 = 0.f;
};

USTRUCT()
struct COMPANYGROWTHRENEWAL_API FDioramaMineSpec
{
	GENERATED_BODY()

	UPROPERTY() bool bPresent = false;
	UPROPERTY() TArray<FDioramaMineUnit> Units;
	UPROPERTY() int32 RateTier = 0;
	UPROPERTY() int32 StorageTier = 0;
	UPROPERTY() int32 OfflineTier = 0;
	UPROPERTY() bool bAutoClaim = false;

	bool IsAllOil() const;
};

USTRUCT()
struct COMPANYGROWTHRENEWAL_API FDioramaPortSpec
{
	GENERATED_BODY()

	UPROPERTY() bool bPresent = false;
	UPROPERTY() int32 CraneCount = 0;
};

USTRUCT()
struct COMPANYGROWTHRENEWAL_API FOverseasDioramaSpec
{
	GENERATED_BODY()

	UPROPERTY() ECountryType Country = ECountryType::None;
	UPROPERTY() FDioramaFactorySpec Factory;
	UPROPERTY() FDioramaMineSpec Mine;
	UPROPERTY() FDioramaPortSpec Port;

	bool HasAny() const { return Factory.bPresent || Mine.bPresent || Port.bPresent; }
	bool HasModule(EDioramaModule Module) const;
	EDioramaModule PrimaryModule() const;
	// 지도 그림을 다시 찍을지 판단하는 키 — 시간에 따라 계속 변하는 채움 비율은 뺀다.
	uint32 GetLookHash() const;
};

// 지도 그림 크기와 땅 조각 중심이 그림 안에서 찍힌 위치(px). UI 가 무대 헤더를 몰라도 되게 여기 둔다.
USTRUCT()
struct COMPANYGROWTHRENEWAL_API FDioramaStillInfo
{
	GENERATED_BODY()

	UPROPERTY() FIntPoint SizePx = FIntPoint::ZeroValue;
	UPROPERTY() FVector2D PivotPx = FVector2D::ZeroVector;
};

struct COMPANYGROWTHRENEWAL_API FDioramaSpecInputs
{
	ECountryType Country = ECountryType::None;
	bool bFactory = false;
	bool bMine = false;
	bool bPort = false;
	ECompanyType Industry = ECompanyType::None;
	TArray<EResourceType> MinableResources;

	int32 FactoryMaxLines = 0;
	TArray<bool> FactoryLineCompleted;
	// Automation, Quality, Storage, Rush 순서의 외형 단계(0~3)
	int32 FactoryTiers[4] = { 0, 0, 0, 0 };

	int32 MineMaxLines = 0;
	TArray<FDioramaMineUnit> MineActiveLines;
	// Rate, Storage, Offline 순서의 외형 단계(0~3)
	int32 MineTiers[3] = { 0, 0, 0 };
	bool bMineAutoClaim = false;
};

namespace OverseasDiorama
{
	COMPANYGROWTHRENEWAL_API int32 AppearanceTier(int32 Level, const TArray<int32>& AppearanceLevels);
	COMPANYGROWTHRENEWAL_API EDioramaMineKind MineKindFor(EResourceType Resource);
	COMPANYGROWTHRENEWAL_API FOverseasDioramaSpec AssembleSpec(const FDioramaSpecInputs& In);
	COMPANYGROWTHRENEWAL_API bool GatherInputs(const UGameInstance* GI, ECountryType Country, FDioramaSpecInputs& Out);
}
