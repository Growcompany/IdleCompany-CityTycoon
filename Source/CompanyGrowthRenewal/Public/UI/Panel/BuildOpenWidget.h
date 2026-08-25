// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "BuildOpenWidget.generated.h"

class UChatMiniLogWidget;
class ABrickFactory;
class USaveLoadManager;
class UAlertMarkWidget;
enum class EFactoryUpgradeType : uint8;

/**
 *
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildOpenWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 미션 가이드 펄스 링 타겟 (MissionManagerSubsystem)
	class UWidget* GetBuildOpenButtonWidget() const;
	class UWidget* GetFactoryOpenButtonWidget() const;
	// 튜토리얼 M11 — MainMap 하단 [뽑기] 버튼 (빌딩 특성 가챠 진입) 하이라이트용
	class UWidget* GetGachaButtonWidget() const;
	// 미션판 G2(본사 레벨) 안내 — 하단 도크 [본사] 버튼
	class UWidget* GetHeadquartersButtonWidget() const;
	// M10b 미션 가이드 — [수집] 버튼(전 빌딩 수익 일괄 수거) 하이라이트 타겟. WBP 미배치면 nullptr.
	class UWidget* GetCollectAllButtonWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UFUNCTION()
	void OnBuildOpenButtonClicked();

	UFUNCTION()
	void OnFactoryOpenButtonClicked();

	UFUNCTION()
	void OnMenuButtonClicked();

	UFUNCTION()
	void OnShopButtonClicked();

	UFUNCTION()
	void OnWorldMapButtonClicked();

	UFUNCTION()
	void OnHeadquartersButtonClicked();

	UFUNCTION()
	void OnCollectAllButtonClicked();

	// 우상단 [뽑기] — 가챠 직접 진입 (1클릭, BM 마찰 최소화)
	// 컨텍스트별로 어느 가챠 화면으로 보낼지 결정 (MainMap=특성 가챠, OfficeMap=직원 가챠 등)
	UFUNCTION()
	void OnGachaButtonClicked();

	// 하단 메인 액션바 [인벤토리] — InventoryHub 진입 (특성/직원강화/티켓/스킨/분해/도감 통합)
	// UButtonWidget native delegate 패턴이라 UFUNCTION 불필요
	void OnInventoryOpenButtonClicked();

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UButtonWidget* BuildOpenButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UButtonWidget* FactoryOpenButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UButton* MenuBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UButton* ShopBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	class UButton* WorldMapBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UButtonWidget* HeadquartersBtn;

	// 빌딩별 StoredRevenue 를 한 번에 수거 (WBP 에서 미배치 시 비활성화)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	class UButtonWidget* CollectAllButton;

	// CollectAllButton 위 오버레이에 얹는 알림 도트 — 수거 가능한 수익이 하나라도 있으면 표시
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	class UAlertMarkWidget* CollectAllAlertMark;

	// 수거 가능 금액 — AlertMark(이진 신호)의 정량판. WBP 미배치 시 마크만 동작.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	class UCommonTextBlock* CollectAllAmountText = nullptr;

	// [뽑기] 가챠 진입 (1클릭, 우상단) — WBP 매칭 이름 GachaBtn
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	class UButton* GachaBtn;

	// 월드맵 진입 잠금 도장 — 제조업(2막) 해금 전까지 표시. 그룹 딤은 이 이미지의 부모 오버레이에 건다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	class UImage* WorldMapLockImage;

	// [인벤토리] 하단 메인 액션바 — InventoryHub 진입 (BuildOpen/Factory/HQ 옆 자리)
	// WBP 매칭 이름 InventoryOpenButton — UIE_HubMenuButton(UButtonWidget) 패턴
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	class UButtonWidget* InventoryOpenButton;

	// 채팅 미니로그 (WBP에서 직접 배치)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UChatMiniLogWidget* ChatMiniLog = nullptr;

	// 버튼 영역 슬라이드 애니메이션 (블루프린트에서 생성)
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	UWidgetAnimation* ShowButtonsAnim;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	UWidgetAnimation* HideButtonsAnim;

private:
	// 알림 마크 재평가 — 타이머 & 델리게이트 공용 엔트리
	void RefreshAlertMarks();

	// 월드맵 = 2막 콘텐츠 — 제조업 해금과 동시 개방 (판정 소스는 DT_CompanyInfo)
	bool IsWorldMapUnlocked();
	void RefreshWorldMapLock();

	UFUNCTION() void HandleHQLeveledUp(int32 NewLevel);
	UFUNCTION() void HandleFactoryUpgraded(EFactoryUpgradeType UpgradeType);

	// 2초 주기로 자원/조건 재평가 (완료 델리게이트는 즉시 꺼짐을 보장, 타이머는 누락 방지)
	FTimerHandle AlertMarkPollHandle;

	UPROPERTY()
	TObjectPtr<ABrickFactory> CachedFactory = nullptr;

	UPROPERTY()
	TObjectPtr<USaveLoadManager> CachedSaveMgr = nullptr;
};
