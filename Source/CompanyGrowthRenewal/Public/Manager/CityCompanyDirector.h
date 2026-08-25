#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Table/CityCompanyData.h"
#if WITH_EDITOR
#include "Containers/Ticker.h"
#endif
#include "CityCompanyDirector.generated.h"

class UMaterialInterface;
class UMeshComponent;

USTRUCT()
struct FCityFacadeMaterialSnapshot
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<UMeshComponent> Component = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;
};

// 스카이라인 빌딩 1개 ↔ 가상회사 매핑 1건. (마커/사인형 변형 표시에 사용)
USTRUCT(BlueprintType)
struct FCitySkylineEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "City Company")
	int32 BuildingKey = 0;

	UPROPERTY(BlueprintReadOnly, Category = "City Company")
	TWeakObjectPtr<AActor> Building = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "City Company")
	FCityCompanyData Data;

	// 옥상 상단 월드 위치(마커 앵커). BP_MB는 IBubbleAnchorProvider 미구현이라 디렉터가 직접 계산.
	UPROPERTY(BlueprintReadOnly, Category = "City Company")
	FVector AnchorWorld = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "City Company")
	float Height = 0.f;

	// 대표 타워(가장 높은 N개) = '사인형' 마커 변형 대상
	UPROPERTY(BlueprintReadOnly, Category = "City Company")
	bool bFlagship = false;
};

// 메인맵 스카이라인(BP_MB###)을 가상회사로 매핑하는 월드 서브시스템.
// (UI 단일안: 3D 사인 스폰 없음. 표시는 InGameLayer 마커가 담당.)
UCLASS()
class COMPANYGROWTHRENEWAL_API UCityCompanyDirector : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	UFUNCTION(BlueprintCallable, Category = "City Company|Editor Preview")
	bool EnableMobileFacadePreview();

	UFUNCTION(BlueprintCallable, Category = "City Company|Editor Preview")
	bool RefreshMobileFacadePreview();

	UFUNCTION(BlueprintCallable, Category = "City Company|Editor Preview")
	void DisableMobileFacadePreview();

	UFUNCTION(BlueprintPure, Category = "City Company|Editor Preview")
	bool IsMobileFacadePreviewEnabled() const;

	UFUNCTION(BlueprintPure, Category = "City Company|Editor Preview")
	int32 GetMobileFacadePreviewActorCount() const;

	// InGameLayer가 마커 생성 시 순회한다.
	const TArray<FCitySkylineEntry>& GetSkylineEntries() const { return SkylineEntries; }

	// "BP_MB036_EonSpire_C" → 36
	static int32 ParseBuildingKey(const FString& ClassName);

	// 가장 높은 N개를 '사인형'(flagship)으로 표시
	UPROPERTY(EditAnywhere, Category = "City Company")
	int32 FlagshipCount = 5;

	UPROPERTY(EditAnywhere, Category = "City Company")
	FString CityBuildingClassPrefix = TEXT("BP_MB");

protected:
	void BuildSkylineMapping();

	UPROPERTY()
	TArray<FCitySkylineEntry> SkylineEntries;

private:
#if WITH_EDITOR
	bool IsEligibleEditorPreviewWorld() const;
	bool ApplyMobileFacadePreview();
	void RestoreMobileFacadePreview();
	bool TickEditorPreview(float DeltaTime);
	void HandlePreSaveWorld(UWorld* SavedWorld, class FObjectPreSaveContext SaveContext);
	void HandlePostSaveWorld(UWorld* SavedWorld, class FObjectPostSaveContext SaveContext);
	void HandlePreBeginPIE(bool bIsSimulating);
	void HandleEndPIE(bool bIsSimulating);

	FTSTicker::FDelegateHandle EditorPreviewTickerHandle;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TArray<FCityFacadeMaterialSnapshot> EditorMaterialSnapshots;

	bool bMobileFacadePreviewEnabled = false;
	bool bMobileFacadePreviewRequested = true;
	bool bSuspendedForSave = false;
	bool bSuspendedForPIE = false;
	int32 MobileFacadePreviewActorCount = 0;
	float EditorPreviewRetryRemainingSeconds = 0.0f;
	static constexpr float EditorPreviewRetryBackoffSeconds = 2.0f;
#endif
};
