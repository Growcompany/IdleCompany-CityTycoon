#pragma once

#include "CoreMinimal.h"
#include "Enum/EmployeeState.h"

class AOfficeworker;

/**
 * 직원 「살펴보기」 — 패널이 고른 직원을 그 자리에서 세워 카메라 쪽으로 돌린다.
 * 시작: 행동 틱·배회·이동 정지, 앉아 있었으면 책상 앞 접근 지점에 세움, 머리 위 버블·피로 바 숨김.
 * 끝: 앉아 있던 직원은 자기 좌석으로, 배회하던 직원은 배회로 되돌린다.
 * 액터가 도중에 사라질 수 있어 약참조만 든다(해고·자리 비우기는 워커 액터를 제거한다).
 */
class COMPANYGROWTHRENEWAL_API FOfficeEmployeeInspect
{
public:
	// 다른 직원을 보고 있었다면 먼저 되돌린다. ViewLocation = 직원이 바라볼 지점(카메라 쪽).
	bool Begin(AOfficeworker* InWorker, const FVector& ViewLocation);
	void End();

	void FaceToward(const FVector& ViewLocation) const;

	AOfficeworker* GetWorker() const { return Worker.Get(); }
	bool IsActive() const { return Worker.IsValid(); }

private:
	TWeakObjectPtr<AOfficeworker> Worker;
	EEmployeeState SavedState = EEmployeeState::Wander;
	bool bWasSeated = false;
	bool bWasRoaming = false;
	bool bBehaviorTickWasEnabled = false;
};
