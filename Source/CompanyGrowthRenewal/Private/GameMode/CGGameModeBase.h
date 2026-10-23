// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameMode/CityOpeningCamera.h"
#include "Components/SlateWrapperTypes.h"
#include "CGGameModeBase.generated.h"

class UVisitModeOverlayWidget;
class UCityOpeningOverlayWidget;
class UUIBase;
class UUserWidget;
class ACityOpeningSequence;
class ATimeCycleManager;

/**
 * 
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API ACGGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;
	virtual void StartPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// 오피스에서 복귀한 경우 그 건물 구도로 카메라를 정착시킨다. 포커스를 잡았으면 true
	// (호출부는 이 값으로 기본 시작 구도를 양보시킨다).
	bool TryFocusCameraOnReturnedOfficeBuilding();

	// 도시 기본 시작 구도 = Brick Factory. 더 구체적인 포커스(오피스 복귀)가 없을 때만.
	void FocusCameraOnCityHomeAnchor();

public:
	ACGGameModeBase();

	bool IsVisitPresentationReady() const;
	bool IsCityOpeningActive() const { return bCityOpeningActive; }
	bool TryGetCityOpeningView(FMinimalViewInfo& OutView) const;
	bool IsCityOpeningReadyForReveal() const { return !bCityOpeningActive || bCityOpeningFramingReady; }
#if !UE_BUILD_SHIPPING
	bool PreviewCityOpening();
#endif

private:
	bool TryPrepareCityOpening();
	bool PrepareCityOpeningPresentation(bool bPreview);
	bool PrepareCityOpeningViews();
	void HandleCityOpeningLayerReady();
	void FinishCityOpening();
	void ClearCityOpeningPresentation();

	bool bCityOpeningActive = false;
	bool bCityOpeningPreview = false;
	bool bCityOpeningLayerReady = false;
	bool bCityOpeningViewReady = false;
	bool bCityOpeningFramingReady = false;
	double CityOpeningPreparedAt = 0.0;
	double CityOpeningStartedAt = 0.0;
	FMinimalViewInfo CityOpeningView;
	TWeakObjectPtr<ACityOpeningSequence> CityOpeningSequence;
	TWeakObjectPtr<ATimeCycleManager> CityOpeningClock;
	int32 CityOpeningSection = INDEX_NONE;
	bool bCityOpeningClockOverride = false;
	TWeakObjectPtr<UUIBase> CityOpeningHiddenUI;
	ESlateVisibility CityOpeningPreviousVisibility = ESlateVisibility::SelfHitTestInvisible;
	TArray<TWeakObjectPtr<UUserWidget>> CityOpeningHiddenWidgets;
	TArray<ESlateVisibility> CityOpeningPreviousWidgetVisibilities;

	UPROPERTY(Transient)
	TObjectPtr<UCityOpeningOverlayWidget> CityOpeningOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UVisitModeOverlayWidget> VisitOverlay;

};
