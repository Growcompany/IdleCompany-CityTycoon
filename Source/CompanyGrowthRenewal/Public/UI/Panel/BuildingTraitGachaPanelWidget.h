// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Panel/GachaPanelBaseWidget.h"
#include "Data/GachaRecruitmentData.h"
#include "Data/BuildingTraitSaveData.h"
#include "Data/BuildingSkinGachaSaveData.h"
#include "Enum/ItemType.h"
#include "Enum/LootBoxRarity.h"
#include "BuildingTraitGachaPanelWidget.generated.h"

class UButtonWidget;
class UIconWithButtonWidget;
class UTextBlock;
class UProgressBar;
class UImage;
class UWidgetSwitcher;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UBuildingTraitManagerSubsystem;
class UBuildingSkinManagerSubsystem;
class UGachaRevealPresentationWidget;

/**
 * 건물 특성 가챠 패널 위젯 (MainMap 빌딩 허브 [특성] 서브뷰)
 * - 외부 카테고리 [특성]/[스킨] 토글
 * - 내부 배너 [일반]/[고급] 배타 선택 (UCommonButtonGroupBase)
 * - 히어로 + 등급 확률 필 + 천장 2종(Epic 60 / Legendary 150) + 마일리지 게이지
 * - 동적 CTA 1개(티켓 전용, 다이아 폴백 없음) + 확률 정보/마일리지 교환(스텁)
 * - 뽑기는 2D 가챠 오버레이(TraitGachaPresentationWidget)로 라우팅 (즉시 grant)
 * 백엔드는 UBuildingTraitManagerSubsystem 그대로 (획득=글로벌, 장착=건물별)
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UBuildingTraitGachaPanelWidget : public UGachaPanelBaseWidget
{
	GENERATED_BODY()

public:
	// 튜토리얼 M11/M12 — 풀링된 패널은 마지막 분류·배너를 기억하므로 안내가 분류 탭·일반 배너를 먼저 맞춘다
	UWidget* GetTraitCategoryTabWidget() const;
	UWidget* GetSkinCategoryTabWidget() const;
	UWidget* GetNormalBannerWidget() const;
	bool IsSkinCategory() const { return bIsSkinCategory; }
	bool IsAdvancedBannerSelected() const { return bAdvancedSelected; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// ========== 외부 카테고리 토글 (특성 / 스킨) ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonButtonBase* CategoryTraitBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonButtonBase* CategorySkinBtn;

	// ========== 히어로 ==========

	// 쇼케이스 카드 스위처 (Index0=특성 카드 UIE_TraitCard, Index1=스킨 카드 UIE_BuildingSkinCard).
	// 카테고리 전환 시 인덱스만 바뀜. 카드 이미지는 디자이너에서 각 카드 PreviewImage 로 설정.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidgetSwitcher* HeroCardSwitcher;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* HeroBoxImage;

	// ========== 우측 게이지 (천장 2종 — 마일리지는 베이스) ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProgressBar* EpicPityBar;

	// 채움 끝 글로우 헤드 2종 (장식 — 없어도 바는 정상 동작)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* EpicPityHead;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* LegendaryPityHead;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* EpicPityLabel;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProgressBar* LegendaryPityBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* LegendaryPityLabel;

	// ========== 10연 CTA + 링크 ==========

	// 10x 뽑기 (선택). 없으면 1x만 동작.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UIconWithButtonWidget* PullButton10;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UIconWithButtonWidget* PullButton30;

	// [분해 상점] (후속 — 현재 미바인딩/스텁)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* DismantleShopButton;

protected:
	// ========== UI 갱신 함수 ==========

	// 특성/스킨 공용 연출 위젯 생성 + 뷰포트 오버레이 표시 (1x/10x 공통)
	UGachaRevealPresentationWidget* CreateRevealWidget();

	virtual void UpdateHeroAndProbability() override;
	virtual void UpdateGauges() override;
	virtual void UpdateCTALabel() override;
	virtual void UpdateTicketCounts() override;
	virtual void SetBannerIndex(int32 Index) override;
	void UpdateBannerPresentation(UCommonButtonBase* Banner, EItemType Ticket);

	// ========== 델리게이트 핸들러 ==========

	void HandleTraitGachaCompleted(const FBuildingTraitGachaResult& Result);
	void HandleSkinGachaCompleted(const FBuildingSkinGachaResult& Result);
	void HandleItemChanged(EItemType ItemType, int32 NewCount, int32 Delta);

	// ========== 버튼 이벤트 ==========

	UFUNCTION()
	void OnCategorySelected(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	virtual void OnPullClicked() override;
	void OnPull10Clicked();
	void OnPull30Clicked();
	virtual void OnMileageExchangeClicked() override;
	virtual void OnProbabilityInfoClicked() override;

private:
	// ========== 캐시된 서브시스템 ==========

	UPROPERTY()
	UBuildingTraitManagerSubsystem* TraitManager;

	UPROPERTY()
	UBuildingSkinManagerSubsystem* SkinManager;

	// ========== 상태 ==========


	// 현재 배너 선택 (false=일반, true=고급)
	bool bAdvancedSelected = false;

	// 현재 카테고리 (false=특성, true=스킨). 스킨 탭 클릭 시 패널 전체가 스킨 가챠로 전환.
	bool bIsSkinCategory = false;

	// 서버 응답 대기 중 CTA 를 잠근다. 매니저의 pending 플래그가 중복 요청을 막지만 버튼이 죽은 듯 보이면 안 된다.
	bool bPullRequestInFlight = false;

	// 서버 권위 뽑기 요청 1건 — 카테고리 분기·대기 표시·응답 후 연출을 한곳에서 처리한다.
	void RequestServerPull(int32 RequestedCount);

	// 활성 카테고리·배너의 티켓 종류 (라벨·게이트·요청이 같은 값을 본다)
	EItemType GetActiveTicketType() const;

	// 10회 단추 수량(특성·스킨 공통). 1 op 상한 MaxPullsPerOp 와는 별개다
	int32 GetMultiPullCount() const;

	// ========== 버튼 그룹 (NativeConstruct에서만 셋업) ==========

	UPROPERTY()
	UCommonButtonGroupBase* CategoryGroup;

	// ========== 델리게이트 핸들 ==========

	FDelegateHandle OnTraitGachaCompletedHandle;
	FDelegateHandle OnSkinGachaCompletedHandle;
	FDelegateHandle OnItemChangedHandle;
};
