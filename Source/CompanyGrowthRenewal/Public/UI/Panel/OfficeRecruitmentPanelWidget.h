// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Data/GachaRecruitmentData.h"
#include "Enum/ResourceType.h"
#include "Enum/GachaTier.h"
#include "Enum/ItemType.h"
#include "OfficeRecruitmentPanelWidget.generated.h"

class UButtonWidget;
class UIconWithButtonWidget;
class UCloseButtonWidget;
class UTextBlock;
class UProgressBar;
class UImage;
class UHorizontalBox;
class UCommonButtonBase;
class UCommonButtonGroupBase;
class URecruitmentManagerSubsystem;
class UItemInventoryManager;
class UResourceItemManager;
class UResourceWidget;
class UTableManagerSubsystem;
class UUIManagerSubsystem;
class UOfficeManager;
class AWorkstationActorBase;

/**
 * 오피스 채용 패널 위젯 (가챠 기반, SOT §2 리디자인)
 * - 배너 3종(일반/고급/프리미엄) 배타 선택 (UCommonButtonGroupBase)
 * - 히어로 + 등급 확률 필 + 천장/마일리지 게이지
 * - 동적 CTA 1개 + 마일리지 교환 + [확률 정보]/[마일리지 상점] 링크
 * - 뽑기는 2D 가챠 오버레이(GachaPresentationWidget)로 라우팅
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeRecruitmentPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// M4 미션 가이드 — [뽑기] 버튼 하이라이트 타겟
	UWidget* GetPullButtonWidget() const;

	// M4 미션 가이드 — [×N 한번에 채용] 하이라이트 타겟. 멀티 버튼이 미배선이면 단발 버튼으로 폴백한다.
	UWidget* GetPullButtonMultiWidget() const;

	// M4 미션 가이드 — [닫기] 버튼 하이라이트 타겟 (채용 후 패널 나가기)
	UWidget* GetCloseButtonWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// ========== UI 바인딩 ==========

	// 닫기 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCloseButtonWidget* UIE_CloseButton;

	// 블루프린트에 배치된 리소스 위젯 참조 (OfficeLayerWidget 패턴)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Money_1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Diamond_1;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UResourceWidget* UIE_Resource_Employee;

	// ========== 배너 3 (UCommonButtonGroupBase) ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonButtonBase* BannerNormal;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonButtonBase* BannerAdvanced;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonButtonBase* BannerPremium;

	// ========== 히어로 ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* HeroTitleText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* HeroDescText;

	// 등급 확률 필 동적 생성 컨테이너
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UHorizontalBox* ProbabilityPillRow;

	// ========== 우측 게이지 ==========

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProgressBar* PityBar;

	// "천장 N/60" 또는 일반 탭 "HR 파워 N"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* PityLabel;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProgressBar* MileageBar;

	// "마일리지 N/200"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* MileageLabel;

	// 채움 끝 글로우 헤드 (장식 — 없어도 바는 정상 동작. 특성 패널 미러)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* PityHead;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* MileageHead;

	// 확률 필 폰트 — 런타임 NewObject 텍스트는 스타일 상속이 없어 명시 필수(Roboto 폴백 방지). WBP Class Defaults 소유.
	UPROPERTY(EditAnywhere, Category = "Style")
	FSlateFontInfo PillFont;

	// ========== 동적 CTA + 마일리지 교환 + 링크 ==========

	// 동적 라벨 (티어/보유에 따라 변경)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UIconWithButtonWidget* PullButton;

	// 전량 뽑기 ×N (동적 라벨). WBP 미배선 시 자동 스킵 — 단발 전용 패널과 호환.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UIconWithButtonWidget* PullButtonMulti;

	// "교환 200pt"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* MileageExchangeButton;

	// [확률 정보]
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* ProbabilityInfoButton;

	// [마일리지 상점]
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* MileageShopButton;

	// ========== 오피스 정보 ==========

	int32 CurrentBuildingIndex = INDEX_NONE;

	// ========== 캐시된 서브시스템 ==========

	UPROPERTY()
	URecruitmentManagerSubsystem* RecruitmentManager;

	UPROPERTY()
	UItemInventoryManager* ItemMgr;

	UPROPERTY()
	UResourceItemManager* ResourceMgr;

	UPROPERTY()
	UTableManagerSubsystem* TableMgr;

	UPROPERTY()
	UUIManagerSubsystem* UIMgr;

	// ========== 리소스 위젯 ==========

	UPROPERTY()
	TMap<EResourceType, UResourceWidget*> ResourceWidgets;

	FDelegateHandle UIResourceChangedHandle;

	void HandleResourceChanged(EResourceType Type, int64 NewValue);

	// 인원 칩 = 고용 게이트와 같은 술어(로스터 / 인원 상한). 갱신 진입점이 갈라지지 않게 여기로 모은다.
	void UpdateEmployeeChip();
	int32 GetEmployeeChipNumerator() const;
	int32 GetEmployeeChipDenominator() const;

	// ========== 델리게이트 핸들 ==========

	FDelegateHandle OnMileageChangedHandle;
	FDelegateHandle OnItemChangedHandle;
	// 오버레이 재뽑기로 게이지가 바뀔 때 패널 동기화
	FDelegateHandle OnGachaPullCompletedHandle;

public:
	// 건물 인덱스 설정
	UFUNCTION(BlueprintCallable, Category = "OfficeRecruitment")
	void SetBuildingIndex(int32 BuildingIndex);

	// 현재 건물 인덱스 반환
	UFUNCTION(BlueprintPure, Category = "OfficeRecruitment")
	int32 GetCurrentBuildingIndex() const { return CurrentBuildingIndex; }

	// 책상 패널 [채용하기] 진입 시 그 책상을 자동 착석 1순위로. 정의는 cpp — 전방선언 타입을 TWeakObjectPtr에 대입하려면 완전한 타입 필요.
	void SetPreferredWorkstation(AWorkstationActorBase* Workstation);

protected:
	// ========== UI 갱신 함수 ==========

	void RefreshAllDisplay();

	// 히어로 텍스트 + 확률 필 갱신
	void UpdateHeroAndProbability();
	// 피티/마일리지 게이지
	void UpdateGauges();
	// 동적 CTA 라벨 + 활성/비활성
	void UpdateCTALabel();
	// 배너 셀 보유 개수 배지 갱신 (티어별)
	void UpdateTicketCounts();

	// ========== 델리게이트 핸들러 ==========

	void HandleMileageChanged(int32 NewPoints);
	void HandleItemChanged(EItemType ItemType, int32 NewCount, int32 Delta);
	void HandleGachaPullCompleted(const FGachaResultData& Result);

private:
	// ========== 배너 그룹 + 선택 티어 ==========

	UPROPERTY()
	UCommonButtonGroupBase* BannerGroup;

	EGachaTier SelectedTier = EGachaTier::Normal;

	TWeakObjectPtr<AWorkstationActorBase> PreferredWorkstationWeak;

	// 정원이 찼으면 안내 후 true. 가챠는 뽑은 뒤에 고용하므로 뽑기 전에 막아야 결과가 붕 뜨지 않는다.
	bool NotifyIfBuildingFull();

	// 멀티 뽑기 수량(티켓·정원·상한 클램프). 라벨과 클릭이 같은 수를 봐야 해서 한 곳으로 모은다.
	int32 ComputeMultiPullCount(EItemType& OutTicket) const;

	// ========== 버튼 이벤트 ==========

	UFUNCTION()
	void OnCloseButtonClicked();

	UFUNCTION()
	void OnBannerSelected(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnPullClicked();

	UFUNCTION()
	void OnPullMultiClicked();

	UFUNCTION()
	void OnMileageExchangeClicked();

	UFUNCTION()
	void OnProbabilityInfoClicked();

	UFUNCTION()
	void OnMileageShopClicked();
};
