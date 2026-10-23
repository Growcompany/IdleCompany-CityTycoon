#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/EmployeeEquipmentData.h"
#include "Table/EmployeeEquipmentTable.h"
#include "EmployeeEquipmentManagerSubsystem.generated.h"

struct FGameSaveData;
struct FMissionTable;

UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeEquipmentManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    const FEmployeeEquipmentState& GetState() const { return State; }
    void RestoreState(const FEmployeeEquipmentState& InState);
    const FEmployeeEquipmentInstance* FindInstance(const FGuid& InstanceId) const;
    const FEmployeeEquipmentInstance* GetEquippedInstance(int32 EmployeeId, EEmployeeEquipmentSlot EquipmentSlot) const;
    int32 GetInstanceEmployee(const FGuid& InstanceId) const;
    FEquipmentAppearanceSelection GetAppearanceSelection(int32 EmployeeId, EEmployeeEquipmentSlot EquipmentSlot) const;
    const FEmployeeEquipmentRuntimeData& GetRuntimeData() const { return State.RuntimeData; }
    void SetRuntimeData(const FEmployeeEquipmentRuntimeData& InData) { State.RuntimeData = InData; }

    FEmployeeEquipmentRequest MakeRequest(EEquipmentCommand Command) const;
    FEmployeeEquipmentResult Execute(const FEmployeeEquipmentRequest& Request);

    // 채용 확정 직후 1회. 확률에 걸리면 기본 등급 장비 1점을 지급하고 곧바로 장착한다.
    bool TryGrantRecruitmentGift(int32 EmployeeId);
    // 튜토리얼을 마친 직후 1회. 받기 버튼 없이 첫 장비 뽑기권을 지급한다.
    bool TryGrantStarterTickets();
    FEmployeeEquipmentResult GetOperation(const FGuid& OperationId) const;
    bool IsCatalogValid(FString& OutError) const;
    bool QuotePull(FName BannerId, int32 Count, FEmployeeEquipmentPullQuote& OutQuote) const;
    TArray<int32> GetNextGradeWeights(FName BannerId) const;
    TMap<FName, float> GetNextDefinitionProbabilities(FName BannerId) const;
    bool QuoteEnhance(const FGuid& InstanceId, EEquipmentMaterialMode Mode, int32& OutCoreCost, int32& OutDuplicateCount) const;
    bool QuoteDismantle(const TArray<FGuid>& InstanceIds, int32& OutCores) const;
    void RemoveEmployeeFromCandidate(int32 EmployeeId, FGameSaveData& Candidate) const;

    DECLARE_MULTICAST_DELEGATE(FOnEquipmentChanged);
    FOnEquipmentChanged OnEquipmentChanged;
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnEquipmentOperation, const FEmployeeEquipmentResult&);
    FOnEquipmentOperation OnOperationCompleted;
    FOnEquipmentOperation OnOperationFailed;

private:
    friend struct FEquipmentThirtyPullValidation;
    friend class UOfficeStageProgressManager;
    UPROPERTY() FEmployeeEquipmentState State;
    bool bExecuting = false;
    FDelegateHandle MissionCompletedHandle;
    FDelegateHandle StarterNoticeHandle;
    void HandleLoaded();
    void HandleMissionCompleted(FName MissionId, const FMissionTable& Mission);
    void ShowStarterNoticeWhenFree();
    bool EmployeeExists(int32 EmployeeId, const FGameSaveData& Snapshot) const;
    bool BuildCandidate(const FEmployeeEquipmentRequest& Request, FGameSaveData& Candidate, FEmployeeEquipmentResult& OutResult);
    bool GrantDefinition(FName DefinitionId, const FGuid& OperationId, FEmployeeEquipmentState& Candidate, FEmployeeEquipmentResult& OutResult) const;
};
