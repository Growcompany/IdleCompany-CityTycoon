#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Utils/ManufacturingWorkSimulator.h"
#include "ManufacturingDevelopmentManager.generated.h"

struct FGameSaveData;
class UFatigueConfig;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnManufacturingPointsChanged, int32 /*BuildingID*/);

USTRUCT(BlueprintType)
struct FManufacturingResearchStatus
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bValid = false;
    UPROPERTY(BlueprintReadOnly) bool bResearched = false;
    UPROPERTY(BlueprintReadOnly) bool bInProgress = false;
    UPROPERTY(BlueprintReadOnly) bool bCanStart = false;
    UPROPERTY(BlueprintReadOnly) int32 ActiveProjectIndex = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) TArray<int32> TeamCapability;
    UPROPERTY(BlueprintReadOnly) TArray<int32> RequiredCapability;
    UPROPERTY(BlueprintReadOnly) float RequiredWorkSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) float CompletedWorkSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) FText BlockReason;
};

/** 제조업 건물의 상시 개발 포인트를 모든 맵에서 한 곳이 적립한다. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UManufacturingDevelopmentManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category = "Manufacturing|Development")
    double GetDevelopmentPoints(int32 BuildingID) const;

    UFUNCTION(BlueprintPure, Category = "Manufacturing|Development")
    double GetPointsPerSecond(int32 BuildingID) const;

    UFUNCTION(BlueprintPure, Category = "Manufacturing|Development")
    TArray<double> GetDisciplinePoints(int32 BuildingID) const;

    UFUNCTION(BlueprintPure, Category = "Manufacturing|Development")
    TArray<double> GetDisciplinePointRates(int32 BuildingID) const;

    UFUNCTION(BlueprintPure, Category = "Manufacturing|Development")
    int32 GetRestingEmployeeCount(int32 BuildingID) const;

    UFUNCTION(BlueprintPure, Category = "Manufacturing|Research")
    FManufacturingResearchStatus GetResearchStatus(int32 BuildingID, int32 ProjectIndex) const;

    UFUNCTION(BlueprintPure, Category = "Manufacturing|Research")
    FManufacturingResearchStatus GetActiveResearchStatus(int32 BuildingID) const;

    UFUNCTION(BlueprintPure, Category = "Manufacturing|Research")
    int32 GetPendingResearchResult(int32 BuildingID) const;

    UFUNCTION(BlueprintCallable, Category = "Manufacturing|Research")
    bool AcknowledgeResearchResult(int32 BuildingID, int32 ProjectIndex);

    UFUNCTION(BlueprintCallable, Category = "Manufacturing|Research")
    bool StartResearch(int32 BuildingID, int32 ProjectIndex, FText& OutFailure);

    bool GetEmployeeWorkState(int32 BuildingID, int32 EmployeeID, float& OutFatigue, bool& bOutResting) const;
    void AccrueOnline();
    void ResetAfterLoad();
    void ApplyOfflineProgress(float Seconds, const FDateTime& IntervalEndUtc);

    FOnManufacturingPointsChanged OnPointsChanged;

private:
    UPROPERTY() TObjectPtr<UFatigueConfig> WorkConfig;
    mutable int64 CachedEquipmentRevision = -1;
    mutable TMap<int32, FEquipmentEffectLoadout> EquipmentLoadouts;
    TArray<FManufacturingWorkerInput> BuildWorkers(int32 BuildingID, float& OutYield) const;
    FTimerHandle OnlineTimerHandle;
    double LastOnlineSeconds = 0.0;
    FDateTime LastOnlineUtc;
    bool bApplyingOfflineProgress = false;
    bool bAccruing = false;
    bool bHasLoadedSave = false;
    bool bHasPendingOfflineProgress = false;
    float PendingOfflineSeconds = 0.0f;
    FDateTime PendingOfflineEndUtc;
    bool bInBackground = false;
    bool bOfflineEligible = true;
    FDelegateHandle EnterBackgroundHandle;
    FDelegateHandle EnterForegroundHandle;
    bool AccrueAll(double Seconds, const FDateTime& OnlineStart = FDateTime(),
        const FDateTime& OnlineEnd = FDateTime(), const FDateTime& OfflineSettledEnd = FDateTime());
    static void RecordOnlineInterval(FGameSaveData& GameData, const FDateTime& Start, const FDateTime& End);
    void HandleEnterBackground();
    void HandleEnterForeground();
};
