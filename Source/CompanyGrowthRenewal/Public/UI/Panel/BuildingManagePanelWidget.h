// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "Enum/TraitSortMode.h"
#include "UI/Element/Common/BulkModeSelectorWidget.h"   // EEnhanceBulkMode + 공용 배율 선택기
#include "BuildingManagePanelWidget.generated.h"

class UTraitCardSlotWidget;
class UTraitSlotWidget;
class UTraitDetailPopupWidget;

class UButtonWidget;
class UTabButtonWidget;
class UUpgradeSlot;
class APlayerCamera;
class UCommonBorder;
class ABuildingBaseActor;
class UScrollBox;
class UVerticalBox;
class UHorizontalBox;
class UWidgetSwitcher;
class UWrapBox;
class UBuildingSkinCardWidget;
class UBuildingLightCardWidget;
class UCommonButtonGroupBase;
class UCommonButtonBase;
class UImage;
class UButton;
class UOverlay;
class UCloseButtonWidget;
class UStatRowWidget;
class UCommonTextBlock;
struct FOperationData;
struct FMissionTable;
enum class EBuildingEnhancementType : uint8;

/**
 * 탭 타입 열거형
 */
UENUM(BlueprintType)
enum class EBuildingManageTab : uint8
{
	Enhancement = 0 UMETA(DisplayName = "Enhancement"),  // 강화 (Money 펌핑, 13슬롯)
	Trait = 1       UMETA(DisplayName = "Trait"),        // 특성 (가챠 획득 패시브 3슬롯)
	Skin = 2        UMETA(DisplayName = "Skin")          // 스킨 (외관/조명)
};

/**
 * 건물 클릭 시 표시되는 관리 패널 위젯
 * 강화/스킬/스킨 등 건물 자체 속성을 관리하고, 오피스 입장 진입점을 제공
 * (직원 관리/프로젝트 진행은 OfficeMap에서 처리)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildingManagePanelWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	APlayerCamera* Player = nullptr;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeOnActivated() override;

	// 타겟 건물 설정. InitialTab = 열릴 때 활성화할 탭 (딥링크용 — 예: 금고 강화 유도는 Enhancement).
	// 풀링 재사용 인스턴스는 이전 탭이 잔존하므로 매 셋업에서 이 탭으로 리셋한다.
	void SetTargetBuilding(ABuildingBaseActor* Building, EBuildingManageTab InitialTab = EBuildingManageTab::Enhancement);

	// 닫기 버튼 콜백
	UFUNCTION()
	void OnCloseButtonClicked();

	// 배경 클릭 시 닫기
	UFUNCTION()
	void OnBackgroundClicked();

	// 탭 선택 변경 콜백 (ButtonGroup에서 호출)
	UFUNCTION()
	void OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	// 건물 업그레이드 버튼 콜백
	UFUNCTION()
	void OnBuildingUpgradeButtonClicked();

protected:
	// 배경 클릭 시 닫기용 투명 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* BackgroundBtn;

	// 닫기 버튼 (UIE_CloseButton)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCloseButtonWidget* UIE_CloseButton;

	// 탭 버튼들 — 합성 래퍼(Overlay+ClipToBounds 마스킹 + 디자인 baked-in)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTabButtonWidget* EnhancementTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTabButtonWidget* TraitTab;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTabButtonWidget* SkinTab;

	// 우측 컬럼(탭바+콘텐츠) 전체 — 등장 시 이 컬럼만 아래→위 트윈(좌측 정보 패널은 고정)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UVerticalBox* RightColumn;

	// Widget Switcher - 탭 콘텐츠 전환용
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidgetSwitcher* ContentSwitcher;

	// Enhancement 탭 - 강화 슬롯들이 동적으로 생성되는 컨테이너 (DT 기반)
	// WBP 에는 이 ScrollBox 하나만 배치. 내부 슬롯은 CompanyType + DT_BuildingEnhancementDefinition 기반으로 런타임 생성.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UScrollBox* EnhancementSlotContainer;

	// 강화 배율 선택기 (공용 부품 — 패널 왼쪽 바깥 하단, 강화 탭에서만 표시. SwitchToTab 가시성 토글)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBulkModeSelectorWidget* BulkModeSelector;

	// 특성 탭 - 카드 그리드 (WrapBox, UIE_TraitCard_QtyBelow 인스턴스)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWrapBox* TraitCardContainer;

	// 특성 탭 상단 - 장착 슬롯 row (UIE_TraitSlot 5개 런타임 생성)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UHorizontalBox* TraitSlotRow;

	// 좌측 컬럼 컨테이너 — (2026-07-21) 특성 팝업 부착처 역할은 TraitModalLayer 로 이관됨
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UVerticalBox* LeftPanelRightBox;

	// ===== 특성 상세 중앙 모달 (2026-07-21 B안: 딤 + 중앙 팝업 + 이전/다음 내비) =====
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UOverlay* TraitModalLayer;

	// 딤 = 닫기 (ProductSellModal BackgroundBtn 패턴)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* TraitModalDimButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* TraitNavPrevButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* TraitNavNextButton;

	// 특성 상세 팝업 — 모달 레이어에 정적 배치. paste 소실 대비 EnsureTraitDetailPopup 동적 생성 폴백
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UTraitDetailPopupWidget> TraitDetailPopup;

	// 특성 탭 정렬 토글 (라디오, ProductSellModal 패턴)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* SortRarityBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* SortQuantityBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* SortNameBtn;

	// 특성 탭 - 장착 밴드 "슬롯 N/5 해금"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* TraitSlotCountText;

	// 특성 탭 - 보유 헤더 "N종"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* TraitCountText;

	// 특성 탭 - 세트 보너스 칩 밴드 (장착 특성 0개면 Collapsed)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UHorizontalBox* SetBonusBand;

	// 특성 탭 - 빈 상태 박스 (카드 0장일 때 표시)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UVerticalBox* TraitEmptyStateBox;

	// 스킨 카드 컨테이너 (외관 - 머티리얼 교체)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWrapBox* SkinCardContainer;

	// 조명 카드 컨테이너 (Window_Emissive_Color 변경)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWrapBox* SkinLightCardContainer;

	// 스킨 탭 내 외관/조명 토글 — ProductSellModal SortButtonGroup 패턴
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* ExteriorBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* LightingBtn;

	// 외관 카드 ScrollBox vs 조명 카드 ScrollBox 전환용
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidgetSwitcher* SkinSwitcher;

	// 오피스 입장 버튼 (공용 영역 - 직원 관리/프로젝트 진행의 단일 진입점)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* EnterOfficeButton;

	// [재배치] 버튼 — 좌하단 정적 배치, 입장과 같은 IconWithButton 패밀리(블루 스타일).
	// Optional 사유: 동시 세션의 패널 트리 paste 로 위젯이 일시 소실될 수 있어 컴파일은 살리고 경고 로그로 감지
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	class UIconWithButtonWidget* RelocateButton = nullptr;

	// ===== 좌측 패널 - 건물 정보 =====

	// 건물 아이콘 이미지 (WBP의 EntityImage와 바인딩)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* EntityImage;

	// 건물 이름 버튼 (기존 BP 위젯 바인딩)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButtonWidget* BuildingNameButton;

	// 건물 스탯 StatRow 컨테이너 (동적 생성 대상)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UVerticalBox* BuildingStatContainer;

	// 건물 레벨 텍스트 (좌측 패널 상단)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* LevelText;

	// 숫자 단계 배지 — 탭하면 해금 로드맵
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* LevelBadgeButton;
	UFUNCTION()
	void HandleLevelChipClicked();

	// 프로젝트/생산 상태 텍스트 (읽기 전용, 업종별 상태 표시)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* ProjectStatusText;

	// 카메라 포커싱에 사용할 패딩 값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraFocusTopPadding = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraFocusBottomPadding = 150.f;

	// 스킨 패널 전용 카메라 재포커싱 패딩
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraRefocusBottomPadding = 50.f;

private:
	// 우측 컬럼 등장 트윈 상태 (좌측 고정, 우측만 아래→위) — 슬라이드는 코드, 페이드는 스택 FADE_ONLY
	bool bRightAppearing = false;
	float RightAppearElapsed = 0.f;
	static constexpr float RightRiseDistance = 48.f;
	static constexpr float RightAppearDuration = 0.2f;

	// 현재 관리 중인 건물
	UPROPERTY()
	ABuildingBaseActor* TargetBuilding = nullptr;

	// 이 패널이 카메라 가림 고스트의 주인인지. 활성/비활성이 오가도 유지되고 세션 시작(NativeConstruct)에만 리셋 —
	// 풀 재사용 인스턴스가 이전 세션의 TargetBuilding 으로 고스트를 켜지 않게 한다
	bool bOwnsFocusTarget = false;

	// 현재 활성화된 탭
	UPROPERTY()
	EBuildingManageTab CurrentTab = EBuildingManageTab::Enhancement;

	// 현재 선택된 스킨 카드 (스킨 탭용)
	UPROPERTY()
	UBuildingSkinCardWidget* CurrentlySelectedCard = nullptr;

	// 현재 선택된 조명 카드
	UPROPERTY()
	UBuildingLightCardWidget* CurrentlySelectedLightCard = nullptr;

	// 탭 버튼 그룹 (자동 배타적 선택 관리)
	UPROPERTY()
	UCommonButtonGroupBase* TabButtonGroup = nullptr;

	// 스킨 탭 내부의 외관/조명 토글 그룹
	UPROPERTY()
	UCommonButtonGroupBase* SkinSubTabGroup = nullptr;

	// 동적 생성된 StatRow 캐시 (갱신용)
	UPROPERTY()
	TArray<UStatRowWidget*> BuildingStatRows;

	// 강화 탭의 동적 슬롯 캐시 (EnhancementType -> 런타임 생성된 UpgradeSlot 위젯)
	// RebuildEnhancementSlots 호출 시 전체 재생성 (ClearChildren + 순회 생성)
	UPROPERTY()
	TMap<EBuildingEnhancementType, UUpgradeSlot*> DynamicSlots;

	// 표시 전용 잠금 캐시 — 구매 권위는 매 입력마다 Actor API에서 직접 판정한다.
	TSet<EBuildingEnhancementType> LockedEnhancementTypes;
	bool bLastTutorialCompleted = false;

	// 강화 배율 모드 (패널 세션 유지)
	EEnhanceBulkMode BulkMode = EEnhanceBulkMode::x1;

	// 탭 전환 함수
	void SwitchToTab(EBuildingManageTab NewTab);

	// 강화 탭 관련 함수들
	// CompanyType에 맞는 슬롯을 DT_BuildingEnhancementDefinition 에서 가져와 EnhancementSlotContainer에 재생성
	void RebuildEnhancementSlots();
	// 단일 슬롯 UI 갱신 (레벨/비용/Locked 상태 반영)
	void UpdateEnhancementSlot(EBuildingEnhancementType EnhancementType, UUpgradeSlot* InSlot, bool bForceLocked = false);
	// UpgradeSlot 에 DT 정의(이름/설명/아이콘/단위) 주입 — 파라미터 Slot 이름은 UWidget::Slot 과 충돌하므로 InSlot 사용
	void ApplySlotDefinition(UUpgradeSlot* InSlot, const struct FBuildingEnhancementDefinition& Def);
	// UpgradeBtn 클릭/홀드 이벤트 동적 바인딩 (슬롯 생성 시 1회)
	void BindSlotButton(UUpgradeSlot* InSlot, EBuildingEnhancementType Type);
	// Actor 권위 판정과 표시 사유를 합성. true면 잠금이며 OutLockConditionText를 카드에 표시한다.
	bool ResolveEnhancementLockState(EBuildingEnhancementType EnhancementType, FText& OutLockConditionText) const;
	bool IsEnhancementUnlockedByTier(EBuildingEnhancementType EnhancementType) const;
	int32 FindEnhancementUnlockTier(EBuildingEnhancementType EnhancementType) const;
	// 클릭/홀드/벌크는 표시 캐시와 무관하게 Actor 권위를 실시간 확인한다.
	bool IsEnhancementInteractionAllowed(EBuildingEnhancementType EnhancementType) const;
	void RefreshEnhancementLocksIfTutorialStateChanged();
	void HandleMissionCompleted(FName CompletedMissionID, const FMissionTable& CompletedMission);
	// 슬롯별 업그레이드 클릭 핸들러 (공통 경로 — 층수 포함)
	void OnEnhancementUpgradeClicked(EBuildingEnhancementType EnhancementType);

	// 홀드 연속 강화 (층수 제외, x1 모드 전용 — x10 이상은 탭=1회 벌크 구매)
	void StartEnhancementHold(EBuildingEnhancementType EnhancementType);
	void StopEnhancementHold();
	void OnEnhancementHoldTick();

	FTimerHandle EnhancementHoldTimerHandle;
	// 전방선언 enum 이라 enumerator 사용 불가 — 0(BuildingFloor)으로 초기화 (미시작 Stop 호출 시 무해)
	EBuildingEnhancementType CurrentHoldEnhancementType = static_cast<EBuildingEnhancementType>(0);
	float HoldInitialDelay = 0.3f;
	float HoldRepeatInterval = 0.1f;

	// ===== 강화 배율 / 벌크 구매 =====
	// 배율 선택기 OnModeChanged 구독 핸들러 (세션 유지, 슬롯 벌크 캡션 갱신)
	void ApplyBulkMode(EEnhanceBulkMode NewMode);
	// Actor 권위와 튜토리얼/티어 잠금을 재계산한 뒤 전 슬롯 갱신 — 모드 전환/벌크 구매 후 호출
	void RefreshAllEnhancementSlots();
	// 벌크 1회 구매 (x10/x50 모드의 Pressed 경로)
	void OnEnhancementBulkPurchase(EBuildingEnhancementType EnhancementType);
	// 현재 모드의 구매 레벨 수 계산 (MaxLevel 잔여/액터 하드 가드 100 클램프). Count=0 이어도 OutTotalCost 는 1레벨 비용
	int32 ComputeBulkCount(EBuildingEnhancementType EnhancementType, int32 CurrentLevel, int32 MaxLevel,
		int64& OutTotalCost, int64& OutAvailable) const;

	// 스킨 탭 관련 함수들 — 외관(Skin)
	void LoadAvailableSkins();
	void OnSkinCardClicked(UBuildingSkinCardWidget* ClickedCard);
	bool CheckIfSkinUnlocked(int32 SkinID) const;

	// 특성 탭 관련 함수들 (BUILDING_TRAIT_SYSTEM v1.1)
	void LoadTraitTabContent();
	void RebuildTraitCardList();
	void SetupTraitSortButtonGroup();
	void UpdateTraitSortButtonTexts();

	UFUNCTION() void HandleSortRarityClicked();
	UFUNCTION() void HandleSortQuantityClicked();
	UFUNCTION() void HandleSortNameClicked();

	UPROPERTY()
	UCommonButtonGroupBase* TraitSortGroup = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UTraitCardSlotWidget>> SpawnedTraitCards;

	ETraitSortMode TraitSortMode = ETraitSortMode::Rarity;
	bool bTraitSortAscending = false;

	// ===== 특성 슬롯 row / 상세 팝업 / 장착 플로우 =====
	void RebuildTraitSlots();

	// 세트 보너스 칩 밴드 재구성 (CalculateSetBonuses → 칩 런타임 생성)
	void RefreshSetBonusBand();

	UPROPERTY()
	TArray<TObjectPtr<class UTraitSetChipWidget>> SpawnedSetChips;

	void HandleTraitSlotClicked(int32 SlotIndex);
	// 잠긴 슬롯 클릭 → 다이아 충분 시 개방 확인 다이얼로그(ConfirmCancel), 부족 시 Toast
	void ShowSlotUnlockConfirm(int32 SlotIndex, int32 DiamondCost);
	void HandleSlotUnlockConfirmed(int32 SlotIndex);
	void HandleTraitCardClicked(FName TraitID);
	void OpenTraitDetailForInventory(FName TraitID);
	void OpenTraitDetailForSlot(int32 SlotIndex);
	void CloseTraitDetailPopup();
	// 첫 특성 클릭 시 LeftPanelRightBox 에 팝업 1회 동적 생성 + 델리게이트 바인딩 (lazy, 이후 재사용)
	void EnsureTraitDetailPopup();
	void SetPendingTargetSlot(int32 SlotIndex);
	int32 FindFirstEmptyUnlockedSlot() const;

	// 상세 팝업 델리게이트 핸들러 (단일캐스트 BindUObject)
	void HandlePopupEquip(FName TraitID);
	void HandlePopupUnequip(int32 SlotIndex);
	void HandlePopupClose();

	// 모달 내비 — 인벤 모드 전용, SpawnedTraitCards(그리드 정렬 순서) 순환
	void NavigateTraitDetail(int32 Delta);
	void UpdateTraitNavVisibility();
	UFUNCTION() void OnTraitModalDimClicked();
	void HandleTraitNavPrev();
	void HandleTraitNavNext();

	// 현재 모달 표시 특성 (내비 기준점, 닫으면 NAME_None)
	FName CurrentDetailTraitID = NAME_None;
	bool bDetailInventoryMode = false;

	// 매니저 멀티캐스트 델리게이트 핸들러 (장착 변경 / 슬롯 해금)
	void HandleTraitSlotChanged(int32 ChangedBuildingIndex, int32 SlotIndex, FName NewTraitID);
	void HandleTraitSlotUnlocked(int32 ChangedBuildingIndex, int32 SlotIndex);

	UPROPERTY()
	TArray<TObjectPtr<UTraitSlotWidget>> SpawnedTraitSlots;

	// 현재 장착 타겟 슬롯 (-1 = 없음 → [장착] 시 첫 빈 슬롯 자동)
	int32 PendingTargetSlotIndex = INDEX_NONE;

	// 매니저/팝업 델리게이트 바인딩 1회 가드
	bool bTraitDelegatesBound = false;

	// 스킨 탭 관련 함수들 — 조명(Light)
	void SetupSkinSubTabGroup();
	void HandleExteriorBtnClicked();
	void HandleLightingBtnClicked();
	void LoadAvailableLights();
	void OnLightCardClicked(UBuildingLightCardWidget* ClickedCard);
	bool CheckIfLightUnlocked(int32 LightID) const;

	// 오피스 입장
	void OnEnterOfficeButtonClicked();

	// [재배치] 버튼 클릭 — 패널 닫고 재배치 모드 진입
	void HandleRelocateRequested();

	// 좌측 패널 건물 정보 갱신 (StatRow 동적 생성)
	void PopulateBuildingStats();

	// 티어 배지 숫자만 갱신 — 패널 오픈/탭 전환 시 호출
	void RefreshLevelBadge();

	// 프로젝트/생산 상태 표시 갱신 (읽기 전용)
	void UpdateProjectStatus();

	// 운영 업데이트 델리게이트 핸들러 (실시간 갱신)
	UFUNCTION()
	void HandleOperationUpdated(int32 BuildingID, const FOperationData& Data);

	// 운영 완료 델리게이트 핸들러 (완료 상태 갱신)
	UFUNCTION()
	void HandleOperationCompleted(int32 BuildingID, const FOperationData& Data);

	// 기대 수익 변경 델리게이트 핸들러 (비-Dynamic 멀티캐스트라 UFUNCTION 불요)
	void HandleExpectedRevenueChanged(int32 BuildingID, float NewRate);

	// 1분 폴링 타이머 틱 — 열린 위젯의 TargetBuilding 만 재계산 요청
	void OnExpectedRevenuePollTick();

	// 기대 수익 폴링 타이머 핸들
	FTimerHandle ExpectedRevenuePollHandle;
	static constexpr float ExpectedRevenuePollIntervalSeconds = 60.0f;

public:
	// Getter 함수
	UFUNCTION(BlueprintCallable, Category = "Camera")
	float GetCameraFocusTopPadding() const { return CameraFocusTopPadding; }

	UFUNCTION(BlueprintCallable, Category = "Camera")
	float GetCameraFocusBottomPadding() const { return CameraFocusBottomPadding; }

	ABuildingBaseActor* GetTargetBuilding() const { return TargetBuilding; }

	// 미션 가이드(M3 EnterOffice)가 [입장] 버튼을 하이라이트 링 타겟으로 쓰기 위한 노출
	UWidget* GetEnterOfficeButtonWidget() const;

	// 미션 가이드(M11 EquipFirstTrait) 하이라이트 링 타겟 노출
	UWidget* GetTraitTabButtonWidget() const;
	UWidget* GetTraitEquipButtonIfOpen() const;   // 보이고 활성인 [장착] 버튼, 아니면 nullptr
	UWidget* GetFirstTraitSlotWidget() const;      // TraitSlotRow 첫 슬롯, 없으면 nullptr
	UWidget* GetFirstOwnedTraitCardWidget() const; // TraitCardContainer 첫 인벤토리 특성 카드, 없으면 nullptr
	// 지정 TraitID 카드. 못 찾으면 첫 카드로 폴백 — "첫 카드 = 방금 뽑은 것"은
	// 인벤토리가 비어 있을 때만 성립하는 우연이라 ID 로 짚는다(M11 SelectTrait).
	UWidget* GetOwnedTraitCardWidgetById(FName InTraitID) const;

	// 스킨 하이라이트 링 타겟 — 구 M12 가이드가 유일한 소비자였고 그 미션이 미션판 G8 로 이관돼 현재 미참조
	UWidget* GetSkinTabButtonWidget() const;
	UWidget* GetFirstSkinCardWidget() const;       // SkinCardContainer 첫 카드, 없으면 nullptr
	UWidget* GetSkinCardWidgetById(int32 InSkinID) const;

	// 미션판 G1 RaiseBuildingFloor 하이라이트 링 타겟 노출
	bool IsEnhancementTabActive() const { return CurrentTab == EBuildingManageTab::Enhancement; }
	UWidget* GetEnhancementTabButtonWidget() const;       // [강화] 탭 버튼
	UWidget* GetBuildingFloorUpgradeButtonWidget() const; // 빌드업 슬롯 강화 버튼 (강화탭 활성 시), 없으면 nullptr
};
