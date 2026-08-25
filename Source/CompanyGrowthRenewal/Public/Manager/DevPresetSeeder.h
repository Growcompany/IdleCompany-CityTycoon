#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DevPresetSeeder.generated.h"

struct FDevProgressPresetRow;

/**
 * 개발용 진행 상태 시더 (Project Settings > Game > CG Dev > 진행 상태 프리셋).
 *
 * 중반 상태 대부분이 FGameSaveData::OfficeDataMap[BuildingIndex] 한 구조체에 살아서,
 * 매니저를 축마다 두드리는 대신 그 맵을 채우는 것이 핵심이다.
 *
 * 시드 순서가 곧 정합성이다 — 시총을 마지막에 넣어야 OnResourceChangedHandler 의
 * while(TryPromoteTitle()) 이 실제 승격 조건(빌딩/업종/이력)을 밟아 등급이 자연히 오른다.
 *
 * Shipping 에서는 GetEffectiveStartMode() 가 FullTutorial 을 강제하므로 도달 불가.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UDevPresetSeeder : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// MainMap 시드. OnGameDataLoaded 의 다음 틱에서 실행된다 — 인라인은 세이브 로드에 덮인다.
	void SeedStageA(FName PresetRow);

	// OfficeMap 최초 진입 시드 (좌석 배정 + 초상화 캡처 큐).
	void SeedStageB();

	// 정합성 단언. 실패 항목을 로그로 뱉고 실패 개수를 반환한다 (0 = 전부 통과, -1 = 판정 불가).
	int32 VerifyPreset(FName PresetRow);

	// 인세션 재적용 (치트 Preset_Apply 백엔드).
	void ReapplyPreset(FName PresetRow);

private:
	const FDevProgressPresetRow* FindPreset(FName PresetRow) const;

	// SeedStageA 가 다음 틱에 호출하는 실제 본체.
	void RunStageA(FName PresetRow);

	// 시드 단계 ― 호출 순서가 곧 정합성이다 (자원/시총이 반드시 마지막).
	void SeedPlotsAndCompanies(const FDevProgressPresetRow& Preset);
	void SeedBuildings(FName PresetRow, const FDevProgressPresetRow& Preset);
	void SeedEmployees(const FDevProgressPresetRow& Preset);
	void SeedProjectHistory(const FDevProgressPresetRow& Preset);
	void SeedOperations(const FDevProgressPresetRow& Preset);
	void SeedWorldMap(const FDevProgressPresetRow& Preset);
	void SeedResourcesAndPromote(const FDevProgressPresetRow& Preset);
	void SeedOfflineGains(const FDevProgressPresetRow& Preset);

	// 시드 1회 래치 — RestoreEntityDataFromLoad 에 중복 스폰 가드가 없어 두 번 돌면 건물이 두 배가 된다.
	bool bStageASeeded = false;
	bool bStageBSeeded = false;
};
