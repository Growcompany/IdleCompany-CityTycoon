#pragma once
#include "CoreMinimal.h"
#include "Enum/EmployeeEquipmentTypes.h"
#include "Enum/ItemType.h"
#include "Data/EmployeeEquipmentRuntimeData.h"
#include "EmployeeEquipmentData.generated.h"

USTRUCT(BlueprintType)
struct FEmployeeEquipmentInstance
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) FGuid InstanceId;
    UPROPERTY(SaveGame, BlueprintReadOnly) FName DefinitionId;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 EnhanceLevel = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) bool bLocked = false;
    UPROPERTY(SaveGame) FGuid AcquisitionOperationId;
};

USTRUCT(BlueprintType)
struct FEmployeeEquipmentLoadout
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) TMap<EEmployeeEquipmentSlot, FGuid> Slots;
};

USTRUCT(BlueprintType)
struct FEquipmentAppearanceSelection
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) EEquipmentAppearanceMode Mode = EEquipmentAppearanceMode::UseEquipment;
    UPROPERTY(SaveGame, BlueprintReadOnly) FName AppearanceId;
};

USTRUCT(BlueprintType)
struct FEmployeeEquipmentAppearanceSelections
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) TMap<EEmployeeEquipmentSlot, FEquipmentAppearanceSelection> Slots;
};

USTRUCT(BlueprintType)
struct FEmployeeEquipmentPityState
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 SinceEpic = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 SinceLegendary = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 SinceAscendant = 0;
};

USTRUCT(BlueprintType)
struct FEmployeeEquipmentRequest
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) FGuid OperationId;
    UPROPERTY(BlueprintReadWrite) int64 ExpectedRevision = 0;
    UPROPERTY(BlueprintReadWrite) FName CatalogVersion;
    UPROPERTY(BlueprintReadWrite) EEquipmentCommand Command = EEquipmentCommand::SetSlot;
    UPROPERTY(BlueprintReadWrite) FName BannerId;
    UPROPERTY(BlueprintReadWrite) int32 Count = 1;
    UPROPERTY(BlueprintReadWrite) int32 EmployeeId = INDEX_NONE;
    UPROPERTY(BlueprintReadWrite) int32 ExpectedSourceEmployeeId = INDEX_NONE;
    UPROPERTY(BlueprintReadWrite) EEmployeeEquipmentSlot EquipmentSlot = EEmployeeEquipmentSlot::Head;
    UPROPERTY(BlueprintReadWrite) FGuid InstanceId;
    UPROPERTY(BlueprintReadWrite) TArray<FGuid> InstanceIds;
    UPROPERTY(BlueprintReadWrite) EEquipmentMaterialMode MaterialMode = EEquipmentMaterialMode::Core;
    UPROPERTY(BlueprintReadWrite) int32 ExpectedLevel = 0;
    UPROPERTY(BlueprintReadWrite) bool bLocked = false;
    UPROPERTY(BlueprintReadWrite) bool bConfirmEnhancedDismantle = false;
    UPROPERTY(BlueprintReadWrite) EEquipmentAppearanceMode AppearanceMode = EEquipmentAppearanceMode::UseEquipment;
    UPROPERTY(BlueprintReadWrite) FName AppearanceId;
};

USTRUCT(BlueprintType)
struct FEmployeeEquipmentPullQuote
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName BannerId;
    UPROPERTY(BlueprintReadOnly) EItemType TicketType = EItemType::None;
    UPROPERTY(BlueprintReadOnly) int32 RequiredTickets = 0;
    UPROPERTY(BlueprintReadOnly) int32 AvailableTickets = 0;
    UPROPERTY(BlueprintReadOnly) int32 MissingTickets = 0;
    UPROPERTY(BlueprintReadOnly) bool bCanPull = false;
    UPROPERTY(BlueprintReadOnly) FName ErrorCode;
};

USTRUCT(BlueprintType)
struct FEmployeeEquipmentResult
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) FGuid OperationId;
    UPROPERTY(SaveGame, BlueprintReadOnly) bool bCommitted = false;
    UPROPERTY(SaveGame, BlueprintReadOnly) FName ErrorCode;
    UPROPERTY(SaveGame, BlueprintReadOnly) int64 Revision = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) FName CatalogVersion;
    UPROPERTY(SaveGame, BlueprintReadOnly) FName BannerId;
    UPROPERTY(SaveGame, BlueprintReadOnly) EItemType TicketType = EItemType::None;
    UPROPERTY(SaveGame, BlueprintReadOnly) TArray<FEmployeeEquipmentInstance> GrantedInstances;
    UPROPERTY(SaveGame, BlueprintReadOnly) TArray<FName> NewAppearanceIds;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 SpentTickets = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 CoreDelta = 0;
};

USTRUCT()
struct FEmployeeEquipmentOperationReceipt
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FString RequestHash;
    UPROPERTY(SaveGame) FEmployeeEquipmentResult Result;
};

USTRUCT(BlueprintType)
struct FEmployeeEquipmentState
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) int64 Revision = 0;
    UPROPERTY(SaveGame, BlueprintReadOnly) TMap<FGuid, FEmployeeEquipmentInstance> Instances;
    UPROPERTY(SaveGame, BlueprintReadOnly) TMap<int32, FEmployeeEquipmentLoadout> Loadouts;
    UPROPERTY(SaveGame, BlueprintReadOnly) TSet<FName> UnlockedDefinitionIds;
    UPROPERTY(SaveGame, BlueprintReadOnly) TSet<FName> UnlockedAppearanceIds;
    UPROPERTY(SaveGame, BlueprintReadOnly) TMap<int32, FEmployeeEquipmentAppearanceSelections> AppearanceSelections;
    UPROPERTY(SaveGame, BlueprintReadOnly) TMap<FName, FEmployeeEquipmentPityState> BannerStates;
    UPROPERTY(SaveGame) bool bStarterClaimed = false;
    UPROPERTY(SaveGame) FEmployeeEquipmentRuntimeData RuntimeData;
    UPROPERTY(SaveGame) TMap<FGuid, FEmployeeEquipmentOperationReceipt> CompletedOperations;
    UPROPERTY(SaveGame) TArray<FGuid> UnseenResultIds;
};
