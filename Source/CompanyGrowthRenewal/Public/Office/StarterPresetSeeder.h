#pragma once

#include "CoreMinimal.h"

class ABuildingBaseActor;

/**
 * 스타터 프리셋 1단계 시드 (건설 확정 시점, MainMap)
 * 빌딩 카드 희귀도로 프리셋을 골라 FOfficeSaveData에 예약만 한다.
 * 실제 데코 배치는 첫 오피스 입장 시 OfficeManager::ApplyStarterPresetToData (2단계).
 */
class COMPANYGROWTHRENEWAL_API FStarterPresetSeeder
{
public:
	static void SeedPendingForBuilding(ABuildingBaseActor* Building);
};
