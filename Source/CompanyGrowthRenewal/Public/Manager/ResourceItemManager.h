#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Enum/ResourceType.h"
#include "Table/ConstructionCost.h"
#include "ResourceItemManager.generated.h"

// ResourceItemManager.h
UCLASS()
class COMPANYGROWTHRENEWAL_API UResourceItemManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()


public:
    // 자원 타입과 변경된 값을 전달하는 델리게이트
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnResourceChanged, EResourceType /*Type*/, int64 /*NewValue*/);

    // 자원이 변경될 때마다 브로드캐스트됩니다
    FOnResourceChanged OnResourceChanged;

    // Subsystem이 처음 초기화될 때 호출됩니다
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Subsystem이 종료될 때 호출됩니다
    virtual void Deinitialize() override;

    // 현재 보유 중인 자원량을 반환합니다
    int64 GetResourceAmount(EResourceType Type) const;

    // 요구량(Amount)이 충족되는지 여부를 반환합니다
    bool HasResource(EResourceType Type, int64 Amount) const;

    // 비용 목록(건설/배치 등)을 전부 감당할 수 있는지 검사. 하나라도 부족하면 false, 첫 부족 자원을 OutMissingType 에 담는다.
    bool CanAffordCosts(const TArray<FConstructionCost>& Costs, EResourceType& OutMissingType) const;

    // 지정한 양만큼 자원을 보관(+Amount)합니다
    // bShouldSave: true일 때 자동 저장 (기본값: true)
    // bShouldBroadcast: true일 때 변경 델리게이트 발행 (기본값: true)
    void StoreResource(
        EResourceType Type,
        int64 Amount,
        bool bShouldSave = true,
        bool bShouldBroadcast = true);

    // 지정한 양만큼 자원을 소비(-Amount)합니다
    // bShouldSave: true일 때 자동 저장 (기본값: true)
    void SpendResource(EResourceType Type, int64 Amount, bool bShouldSave = true);

    // 요청한 양만큼 자원을 인출하고, 실제 인출된 양을 Withdrawn에 담아 반환합니다 (충분치 않으면 false 반환)
    // bShouldSave: true일 때 자동 저장 (기본값: true)
    bool ExtractResource(EResourceType Type, int64 Requested, int64& Withdrawn, bool bShouldSave = true);

    // 모든 자원 데이터를 반환합니다 (저장용)
    const TMap<EResourceType, int64>& GetAllResources() const { return ResourceBank; }

    // 모든 자원 데이터를 설정합니다 (로드용)
    void SetAllResources(
        const TMap<EResourceType, int64>& InResources,
        bool bShouldBroadcast = true);

    // ===== Test/Debug =====

    /**
     * DT_TestResourceScenarios 의 한 행을 골라 자원 일괄 set.
     * @param ScenarioRowName CSV 의 RowName (예: "Default", "Rich", "Poor", "Empty")
     * @return 적용 성공 여부
     */
    UFUNCTION(BlueprintCallable, Exec, Category = "Resource|Debug")
    bool GrantTestResources(FName ScenarioRowName);

private:
    void HandleGameDataLoaded();

private:
    // 내부적으로 자원별 값을 저장하는 맵
    TMap<EResourceType, int64> ResourceBank;

    // 주기적 자동 저장 타이머
    FTimerHandle PeriodicSaveTimerHandle;

    // 주기적 저장 실행
    void PeriodicSave();
};
