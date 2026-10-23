// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Data/FactorySaveData.h"
#include "UI/Element/Common/BulkModeSelectorWidget.h"   // EEnhanceBulkMode + 공용 배율 선택기
#include "FactoryPanelWidget.generated.h"

class UButton;
class UScrollBox;
class UButtonWidget;
class UIconWithButtonWidget;
class UCostActionButtonWidget;
class UPanelHeaderWidget;
class UCommonTextBlock;
class UImage;
class ABrickFactory;
class UUpgradeSlot;
enum class EResourceType : uint8;
struct FFactoryUpgradeDefinition;
struct FFactoryCostCurve;

UCLASS()
class COMPANYGROWTHRENEWAL_API UFactoryPanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UFUNCTION()
	void OnCloseButtonClicked();

	void HandleRelocateRequested();

	// 공장 강화/해금 브로드캐스트 수신 — 슬롯 구조는 유지하며 표시 상태를 갱신한다
	UFUNCTION()
	void HandleFactoryUpgraded(EFactoryUpgradeType UpgradeType);

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButton* BackgroundBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UPanelHeaderWidget> PanelHeader;

	// DT 기반 동적 슬롯 컨테이너 (WBP의 CommonHierarchicalScrollBox)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UScrollBox* UpgradeSlotContainer;

	// 강화 배율 선택기 (공용 부품 — 공장 패널은 탭이 없어 상시 표시, 가시성 토글 불필요)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBulkModeSelectorWidget* BulkModeSelector;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UIconWithButtonWidget* RelocateButton;

private:
	// DT 순서/클래스가 그대로면 슬롯을 재사용하고, 구조가 바뀌었을 때만 다시 만든다
	void RebuildUpgradeSlots();
	void ApplySlotDefinition(UUpgradeSlot* InSlot, const FFactoryUpgradeDefinition& Def);
	void BindSlotButton(UUpgradeSlot* InSlot, EFactoryUpgradeType Type);
	void UpdateUpgradeSlot(EFactoryUpgradeType Type, UUpgradeSlot* InSlot);
	void UpdateAllUpgradeSlots();

	// 통합 업그레이드 핸들러 (x1 = Pressed 1회 + 홀드, x10/x50 = 탭당 벌크 1회)
	void OnUpgradeClicked(EFactoryUpgradeType Type);

	// 벌크 1회 구매 (x10/x50 모드의 Pressed 경로 — BuildingManagePanel 미러)
	void OnBulkUpgrade(EFactoryUpgradeType Type);

	// 현재 모드의 구매 레벨 수 계산 (영구 상한·튜토리얼 허용량·액터 하드 가드 적용).
	int32 ComputeBulkCount(EFactoryUpgradeType Type, int32 CurrentLevel, int32 MaxLevel, const FFactoryCostCurve& CostCurve,
		EResourceType CostType, int64& OutTotalCost, int64& OutAvailable) const;

	// 배율 선택기 OnModeChanged 구독 핸들러 (세션 유지, 슬롯 벌크 캡션 갱신)
	void ApplyBulkMode(EEnhanceBulkMode NewMode);

	// 버튼 Hold
	void StartUpgradeHold(EFactoryUpgradeType UpgradeType);
	void StopUpgradeHold();
	void OnUpgradeHoldTick();

	void OnUpgradeSuccess(EFactoryUpgradeType UpgradeType);

	// 자동 수집 해금 연출 — 공장이 들고 있는 pending 플래그를 소비했을 때만 1회
	void TryPlayAutoUnlockCelebration();

	// 마지막으로 슬롯에 그린 잠금 상태 — 해금 브로드캐스트를 받았을 때 재빌드 필요 여부 판정용
	bool bAutoSlotsShownAsLocked = true;

	UPROPERTY()
	ABrickFactory* CurrentFactory = nullptr;

	// 동적 슬롯 캐시
	UPROPERTY()
	TMap<EFactoryUpgradeType, UUpgradeSlot*> DynamicSlots;

	// Hold 상태
	FTimerHandle UpgradeHoldTimerHandle;
	bool bCloseRequested = false;
	// 홀드 시작 전 StopUpgradeHold(펀치 억제 해제)가 먼저 불릴 수 있어 초기값 필요
	EFactoryUpgradeType CurrentHoldUpgradeType = EFactoryUpgradeType::HoldProductionSpeed;
	FName HoldMissionID = NAME_None;
	float HoldInitialDelay = 0.3f;
	float HoldRepeatInterval = 0.1f;

	// 강화 배율 모드 (패널 세션 유지 — 선택기가 없어도 x1 로 동작)
	EEnhanceBulkMode BulkMode = EEnhanceBulkMode::x1;

	void OnResourceChanged(EResourceType Type, int64 NewValue);
	void SaveFactoryData();

public:
	// 튜토리얼이 흐름 밖에 열린 공장 패널을 닫게 할 때 짚는 헤더 [닫기]
	UWidget* GetCloseButtonWidget() const;
	// M1 인부 추가 구매 버튼. 슬롯 순서와 관계없이 타입으로 찾는다.
	UWidget* GetLaborerUpgradeTarget() const;
	UWidget* GetFactoryExpansionUpgradeTarget() const;
	// 튜토리얼 M8 — 작업속도·생산능력 강화 버튼
	UWidget* GetUpgradeTarget(EFactoryUpgradeType Type) const;
	UWidget* GetFirstAffordableUpgradeTarget() const;

	void RequestClose();

	UFUNCTION(BlueprintCallable, Category = "Factory")
	void SetFactory(ABrickFactory* Factory);

	UFUNCTION(BlueprintCallable, Category = "Factory")
	void RefreshUI();
};
