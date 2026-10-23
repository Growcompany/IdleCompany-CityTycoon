#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Entity/OverseasDiorama/OverseasDioramaTypes.h"
#include "Manager/MineManager.h"
#include "Manager/WorldFactoryManager.h"
#include "OverseasDioramaSubsystem.generated.h"

class AOverseasDioramaStage;
class UTexture;
class UTextureRenderTarget2D;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDioramaMiniatureUpdated, ECountryType);

USTRUCT()
struct FDioramaMiniatureEntry
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Target;
	UPROPERTY() FDioramaStillInfo Info;
	uint32 LookHash = 0;
	bool bCaptured = false;
};

/**
 * 해외 시설 디오라마의 무대·지도 그림·라이브 화면 관리.
 * 무대는 처음 필요할 때 한 번 스폰하고, 지도 그림은 나라별 RT 로 보관한다.
 * 공장·광산 관리자 이벤트로 그 나라를 다시 찍을 대상으로 표시하고, 라이브 화면이 아닐 때 한 틱에 한 나라씩 찍는다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOverseasDioramaSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UTexture* GetMiniature(ECountryType Country, FDioramaStillInfo& OutInfo) const;
	void RequestMiniatures(const TArray<ECountryType>& Countries);
	void MarkDirty(ECountryType Country);

	UTextureRenderTarget2D* BeginLiveView(ECountryType Country, FIntPoint SizePx, EDioramaModule Focus);
	void SetLiveViewSize(FIntPoint SizePx);
	void FocusModule(EDioramaModule Module);
	void PlayLiveIntro();
	void EndLiveView();
	bool IsLiveView() const { return LiveCountry != ECountryType::None; }
	ECountryType GetLiveCountry() const { return LiveCountry; }

	FOnDioramaMiniatureUpdated OnMiniatureUpdated;

	void SetSpecProviderForTest(TFunction<bool(ECountryType, FOverseasDioramaSpec&)> Provider) { SpecProviderForTest = MoveTemp(Provider); }
	AOverseasDioramaStage* GetStageForTest() const { return Stage; }

private:
	UPROPERTY() TObjectPtr<AOverseasDioramaStage> Stage;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> LiveTarget;
	UPROPERTY() TMap<ECountryType, FDioramaMiniatureEntry> Miniatures;

	TArray<ECountryType> PendingCaptures;
	TFunction<bool(ECountryType, FOverseasDioramaSpec&)> SpecProviderForTest;
	ECountryType LiveCountry = ECountryType::None;
	bool bLiveDirty = false;
	bool bBoundManagers = false;

	bool BuildSpec(ECountryType Country, FOverseasDioramaSpec& OutSpec) const;
	AOverseasDioramaStage* EnsureStage();
	void Enqueue(ECountryType Country);
	void ProcessOneCapture();
	void BindManagers();
	void UnbindManagers();

	UFUNCTION() void HandleFactoryLineStarted(ECountryType Country, FWorldFactoryLineState LineState);
	UFUNCTION() void HandleFactoryLineCompleted(ECountryType Country, int32 LineId);
	UFUNCTION() void HandleFactoryLineClaimed(ECountryType Country, int32 LineId, int64 FinalQty);
	UFUNCTION() void HandleFactoryUpgradeChanged(ECountryType Country, EWorldFactoryUpgradeType UpgradeType, int32 NewLevel);
	UFUNCTION() void HandleMineLineCreated(ECountryType Country, FMineLineState LineState);
	UFUNCTION() void HandleMineLineSaturated(ECountryType Country, EResourceType Resource, bool bSaturated);
	UFUNCTION() void HandleMineResourceClaimed(ECountryType Country, EResourceType Resource, int64 ClaimedQty);
	UFUNCTION() void HandleMineLineRemoved(ECountryType Country, EResourceType Resource);
	UFUNCTION() void HandleMineUpgradeChanged(ECountryType Country, EMineUpgradeType UpgradeType, int32 NewLevel);
};
