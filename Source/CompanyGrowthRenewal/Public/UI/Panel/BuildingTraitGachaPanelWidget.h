// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Data/GachaRecruitmentData.h"
#include "Data/BuildingTraitSaveData.h"
#include "Data/BuildingSkinGachaSaveData.h"
#include "Enum/ItemType.h"
#include "Enum/LootBoxRarity.h"
#include "BuildingTraitGachaPanelWidget.generated.h"

class UButtonWidget;
class UIconWithButtonWidget;
class UCloseButtonWidget;
class UTextBlock;
class UProgressBar;
class UImage;
class UHorizontalBox;
class UWidgetSwitcher;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class UBuildingTraitManagerSubsystem;
class UBuildingSkinManagerSubsystem;
class UGachaRevealPresentationWidget;
class UItemInventoryManager;
class UTableManagerSubsystem;
class UUIManagerSubsystem;

/**
 * 건물 특성 가챠 패널 위젯 (MainMap 빌딩 허브 [특성] 서브뷰)
 * - 외부 카테고리 [특성]/[스킨] 토글 ([스킨]=비활성 스텁)
 * - 내부 배너 [일반]/[고급] 배타 선택 (UCommonButtonGroupBase)
 * - 히어로 + 등급 확률 필 + 천장 2종(Epic 60 / Legendary 150) + 마일리지 게이지
 * - 동적 CTA 1개(티켓 전용, 다이아 폴백 없음) + 확률 정보/마일리지 교환(스텁)
 * - 뽑기는 2D 가챠 오버레이(TraitGachaPresentationWidget)로 라우팅 (즉시 grant)
 * 백엔드는 UBuildingTraitManagerSubsystem 그대로 (획득=글로벌, 장착=건물별)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildingTraitGachaPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// ========== UI 바인딩 ==========


	// 닫기 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCloseButtonWidget* UIE_CloseButton;

	// ========== 외부 카테고리 토글 (특성 / 스킨) ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonButtonBase* CategoryTraitBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonButtonBase* CategorySkinBtn;

	// ========== 내부 배너 (일반 / 고급) ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonButtonBase* BannerNormal;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonButtonBase* BannerAdvanced;

	// ========== 히어로 ==========

	// 쇼케이스 카드 스위처 (Index0=특성 카드 UIE_TraitCard, Index1=스킨 카드 UIE_BuildingSkinCard).
	// 카테고리 전환 시 인덱스만 바뀜. 카드 이미지는 디자이너에서 각 카드 PreviewImage 로 설정.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidgetSwitcher* HeroCardSwitcher;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* HeroTitleText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* HeroDescText;

	// 등급 확률 필 동적 생성 컨테이너
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UHorizontalBox* ProbabilityPillRow;

	// ========== 우측 게이지 (천장 2종 + 마일리지) ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProgressBar* EpicPityBar;

	// 채움 끝 글로우 헤드 3종 (장식 — 없어도 바는 정상 동작)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* EpicPityHead;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* LegendaryPityHead;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* MileageHead;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* EpicPityLabel;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProgressBar* LegendaryPityBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* LegendaryPityLabel;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProgressBar* MileageBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* MileageLabel;

	// 확률 필 폰트 — 런타임 NewObject 텍스트는 스타일 상속이 없어 명시 필수(Roboto 폴백 방지). WBP Class Defaults 소유.
	UPROPERTY(EditAnywhere, Category = "Style")
	FSlateFontInfo PillFont;

	// ========== 동적 CTA + 링크 ==========

	// 동적 라벨 (티어/보유에 따라 변경) — 1x 뽑기
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UIconWithButtonWidget* PullButton;

	// 10x 뽑기 (선택). 없으면 1x만 동작.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UIconWithButtonWidget* PullButton10;

	// 마일리지 상자 교환 (스텁 — 후속 picker 연결)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* MileageExchangeButton;

	// [확률 정보]
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* ProbabilityInfoButton;

	// [분해 상점] (후속 — 현재 미바인딩/스텁)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* DismantleShopButton;

public:
	// 컨텍스트 설정 (장착 편의용 건물 인덱스 + 특성 탭 사전 선택)
	UFUNCTION(BlueprintCallable, Category = "BuildingTraitGacha")
	void SetContext(int32 BuildingIndex, bool bOpenTraitTab);

	// 미션 가이드(M11 EquipFirstTrait) [뽑기] 하이라이트 링 타겟 노출
	UWidget* GetPullButtonWidget() const;
	// [스킨] 카테고리 링 타겟 — 구 M12 가이드가 유일한 소비자였고 그 미션이 미션판 G8 로 이관돼 현재 미참조
	UWidget* GetSkinCategoryButtonWidget() const;
	// 튜토리얼 M11 — 가챠 [닫기] 버튼 하이라이트 타겟
	UWidget* GetCloseButtonWidget() const;

protected:
	// ========== UI 갱신 함수 ==========

	// 특성/스킨 공용 연출 위젯 생성 + 뷰포트 오버레이 표시 (1x/10x 공통)
	UGachaRevealPresentationWidget* CreateRevealWidget();

	// 히어로 텍스트 + 확률 필 갱신
	void UpdateHeroAndProbability();
	// 천장 2종 + 마일리지 게이지
	void UpdateGauges();
	// 동적 CTA 라벨 + 활성/비활성
	void UpdateCTALabel();
	// 배너 셀 보유 개수 배지 갱신 (현재 카테고리의 일반/고급)
	void UpdateTicketCounts();

	// ========== 델리게이트 핸들러 ==========

	void HandleTraitGachaCompleted(const FBuildingTraitGachaResult& Result);
	void HandleSkinGachaCompleted(const FBuildingSkinGachaResult& Result);
	void HandleItemChanged(EItemType ItemType, int32 NewCount, int32 Delta);

	// ========== 버튼 이벤트 ==========

	UFUNCTION()
	void OnCategorySelected(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnBannerSelected(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnPullClicked();

	void OnPull10Clicked();

	UFUNCTION()
	void OnMileageExchangeClicked();

	UFUNCTION()
	void OnProbabilityInfoClicked();

	UFUNCTION()
	void OnCloseButtonClicked();

private:
	// ========== 캐시된 서브시스템 ==========

	UPROPERTY()
	UBuildingTraitManagerSubsystem* TraitManager;

	UPROPERTY()
	UBuildingSkinManagerSubsystem* SkinManager;

	UPROPERTY()
	UItemInventoryManager* ItemMgr;

	UPROPERTY()
	UTableManagerSubsystem* TableMgr;

	UPROPERTY()
	UUIManagerSubsystem* UIMgr;

	// ========== 상태 ==========

	// 장착 편의용 건물 인덱스 (글로벌 뽑기엔 무관)
	int32 ContextBuildingIndex = INDEX_NONE;

	// 현재 배너 선택 (false=일반, true=고급)
	bool bAdvancedSelected = false;

	// 현재 카테고리 (false=특성, true=스킨). 스킨 탭 클릭 시 패널 전체가 스킨 가챠로 전환.
	bool bIsSkinCategory = false;

	// ========== 버튼 그룹 (NativeConstruct에서만 셋업) ==========

	UPROPERTY()
	UCommonButtonGroupBase* CategoryGroup;

	UPROPERTY()
	UCommonButtonGroupBase* BannerGroup;

	// ========== 델리게이트 핸들 ==========

	FDelegateHandle OnTraitGachaCompletedHandle;
	FDelegateHandle OnSkinGachaCompletedHandle;
	FDelegateHandle OnItemChangedHandle;
};
