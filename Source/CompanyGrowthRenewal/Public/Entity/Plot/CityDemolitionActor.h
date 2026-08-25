#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityDemolitionActor.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCityDemolitionVisualCleared, int32 /*CompanyKey*/);

// 철거 연출 전담 액터 — 대상 건물을 층수만큼 순차 Z축소(스텝마다 먼지) 후
// 최종 "쿵"(숨김 + 바닥 먼지 폭발 + 카메라 셰이크 + 임팩트 음)을 처리하고 자가 소멸한다.
UCLASS()
class COMPANYGROWTHRENEWAL_API ACityDemolitionActor : public AActor
{
    GENERATED_BODY()
public:
	ACityDemolitionActor();

	// 스폰 직후 호출 — 대상/층수를 캐시하고 연출을 시작한다.
	void BeginDemolition(AActor* InBuilding, int32 InFloors, int32 InCompanyKey);

	// 대상 Actor가 실제로 Hidden/소멸된 뒤 발생한다. 드레싱 등 시각 점유 해제의 정확한 경계다.
	FOnCityDemolitionVisualCleared OnVisualCleared;

    virtual void Tick(float DeltaSeconds) override;

private:
    // 마지막 스텝 도달 시 1회 실행(쿵 + 정리 + Destroy). bFinished 가드.
    void Finish();
    // "건물 한 층 상승"과 동일한 VFX(NS_Ground_Jump_Fx) 를 Loc 에 AbsScale 로 스폰(붕괴 층/바닥 공용).
    void SpawnFloorVFX(const FVector& Loc, float AbsScale);

    TWeakObjectPtr<AActor> Building;
    FVector OrigScale = FVector::OneVector;
    FVector BaseLoc = FVector::ZeroVector;        // 건물 피벗(바닥) 월드 위치
    FVector2D CenterXY = FVector2D::ZeroVector;   // 바운드 중심 XY
    float FullHeight = 0.f;                        // 원본(축소 전) 전체 높이

	int32 Floors = 6;
	int32 CompanyKey = 0;
	int32 StepsDone = 0;
    float StepInterval = 0.12f;   // 다음 스텝까지 간격 — 스텝마다 가속(붕괴 가속감). 살짝 느리게(0.10→0.12)
    float StepTimer = 0.f;
    bool bFinished = false;
};
