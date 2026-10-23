// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Enum/WidgetType.h"
#include "UI/Panel/CountryRouterPanelWidget.h"
#include "Manager/MineManager.h"
#include "Manager/WorldFactoryManager.h"
#include "WorldMapBottomWidget.generated.h"

class UButton;
class UButtonWidget;
class UIconWithButtonWidget;
class UAlertMarkWidget;

/**
 * WorldMap 하단 네비게이션 위젯 (BottomStack)
 *
 * 역할:
 *  - 메인맵 복귀 버튼
 *  - 자원/공장/완성품 패널 호출 액션바 (각각 우측 슬라이드인)
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UWorldMapBottomWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/**
	 * 이 도크를 품은 활성화 위젯(= 월드맵 패널). 2D 패널 전환 이후 도크는 스택에 push 되는 대신
	 * `UI_WorldMapPanel` 의 자식으로 들어가는데, 그 모드에는 `ALegacyWorldMapPlayerController` 가 없어
	 * 홈 버튼의 기존 복귀 경로가 통째로 무동작이 된다. 그 자리를 이게 메운다.
	 *
	 * ⚠ 위젯 부모 체인(`GetParent`)은 같은 WidgetTree 안에서만 올라가 트리를 못 벗어난다 ―
	 *    소유 UserWidget 은 아우터 쪽에 있으므로 반드시 아우터로 거슬러야 한다.
	 */
	UCommonActivatableWidget* FindOwningActivatable() const;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UFUNCTION()
	void OnHomeBtnClicked();

	UFUNCTION()
	void OnResourcePanelClicked();

	UFUNCTION()
	void OnFactoryPanelClicked();

	UFUNCTION()
	void OnProductsPanelClicked();

	void OnCollectAllClicked();

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButton> HomeBtn;

	// 액션바 3개 (UButtonWidget — UCommonButtonBase native 이벤트)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> ResourcePanelBtn = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> FactoryPanelBtn = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButtonWidget> ProductsPanelBtn = nullptr;

	// 수집 버튼 — 채광 1개 이상 + 공장 completed 라인 모두 수령 + 토스트로 요약 표시.
	// 항상 활성 (disabled 회색 처리 안 함). 수령 가능 라인 없을 때는 "수집할 게 없습니다" 토스트.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UIconWithButtonWidget> CollectAllButton = nullptr;

	// UI_WorldMapBottom 에 같은 이름으로 이미 놓여 있으나 여태 C++ 이 잡지 않아 아무도 켜고 끄지 않던 도트다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UAlertMarkWidget> CollectAllAlertMark = nullptr;

private:
	void PushPanel(EWidgetType PanelType);

	// 채광/공장은 wall-clock lazy 모델이라 자연 성숙을 알리는 델리게이트가 없다 —
	// 다음 라인이 성숙하는 시각을 직접 계산해 1회용 타이머로 깨운다(주기 폴링 대체).
	void BindCollectBadgeEvents();
	void UnbindCollectBadgeEvents();
	void RefreshCollectBadge();
	void SetCollectBadgeVisible(bool bVisible);

	UFUNCTION() void HandleMineLineCreated(ECountryType Country, FMineLineState LineState) { RefreshCollectBadge(); }
	UFUNCTION() void HandleMineLineSaturated(ECountryType Country, EResourceType Resource, bool bSaturated) { RefreshCollectBadge(); }
	UFUNCTION() void HandleMineResourceClaimed(ECountryType Country, EResourceType Resource, int64 ClaimedQty) { RefreshCollectBadge(); }
	UFUNCTION() void HandleMineUpgradeChanged(ECountryType Country, EMineUpgradeType UpgradeType, int32 NewLevel) { RefreshCollectBadge(); }
	UFUNCTION() void HandleFactoryLineStarted(ECountryType Country, FWorldFactoryLineState LineState) { RefreshCollectBadge(); }
	UFUNCTION() void HandleFactoryLineCompleted(ECountryType Country, int32 LineId) { RefreshCollectBadge(); }
	UFUNCTION() void HandleFactoryLineClaimed(ECountryType Country, int32 LineId, int64 FinalQty) { RefreshCollectBadge(); }
	UFUNCTION() void HandleFactoryUpgradeChanged(ECountryType Country, EWorldFactoryUpgradeType UpgradeType, int32 NewLevel) { RefreshCollectBadge(); }

	FTimerHandle CollectBadgeEtaHandle;
	void PushRouterPanel(ECountryRouterMode Mode);
};
