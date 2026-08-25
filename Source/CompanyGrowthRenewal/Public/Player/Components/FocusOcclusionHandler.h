#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FocusOcclusionHandler.generated.h"

class ABuildingBaseActor;

// 카메라와 포커스 타깃 사이를 막는 건물을 고스트 처리한다.
// 판정(여기) 과 표현(ABuildingBaseActor::SetOccluderGhost) 은 분리되어 있다.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class COMPANYGROWTHRENEWAL_API UFocusOcclusionHandler : public UActorComponent
{
	GENERATED_BODY()

public:
	UFocusOcclusionHandler();

	// 포커스 타깃 등록. 이 액터를 가리는 건물이 고스트가 된다.
	void SetFocusTarget(AActor* InTarget);

	// 해제. 고스트는 즉시 사라지지 않고 페이드아웃 후 복원된다.
	void ClearFocusTarget();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// 타이머 진입점 전용. 입력 모드 안전망을 본 뒤 RefreshOccluders 로 넘긴다.
	void UpdateOcclusion();

	// 실제 판정 본체. 등록(SetFocusTarget) 은 안전망을 건너뛰고 이걸 직접 부른다.
	void RefreshOccluders();

	void ForceRestoreAllBuildings();

	// 0 이하면 타이머가 조용히 꺼져 고스트가 켜진 채 멈추므로 하한을 강제한다
	UPROPERTY(EditAnywhere, Category = "Focus Occlusion", meta = (ClampMin = "0.01"))
	float UpdateInterval = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Focus Occlusion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SampleInset = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Focus Occlusion", meta = (ClampMin = "0"))
	int32 MaxGhostCount = 5;

	UPROPERTY(EditAnywhere, Category = "Focus Occlusion", meta = (ClampMin = "0.0"))
	float TransitionTime = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Focus Occlusion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GhostOpacity = 0.35f;

	struct FGhostState
	{
		float Alpha = 0.f;
		float Goal = 1.f;
	};

	TWeakObjectPtr<AActor> FocusTarget;
	TMap<TWeakObjectPtr<ABuildingBaseActor>, FGhostState> GhostStates;
	FTimerHandle UpdateTimerHandle;
};
