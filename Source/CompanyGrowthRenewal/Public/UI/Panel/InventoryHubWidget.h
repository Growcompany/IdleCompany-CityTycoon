// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "InventoryHubWidget.generated.h"

class UTabButtonWidget;
class UWidgetSwitcher;
class UCloseButtonWidget;
class UCommonButtonGroupBase;
class UCommonButtonBase;
class UTextBlock;
class UButton;

// 인벤토리 허브 탭 (BUILDING_TRAIT_SYSTEM v1.1 §13.2)
UENUM(BlueprintType)
enum class EItemHubTab : uint8
{
	BuildingTrait    = 0 UMETA(DisplayName = "특성"),     // MainMap 디폴트
	EmployeeEnhance  = 1 UMETA(DisplayName = "직원강화"), // OfficeMap 디폴트
	Ticket           = 2 UMETA(DisplayName = "티켓"),
	Skin             = 3 UMETA(DisplayName = "스킨"),
	Dismantle        = 4 UMETA(DisplayName = "분해"),
	Encyclopedia     = 5 UMETA(DisplayName = "도감")
};

// 인벤토리 허브 진입 컨텍스트 — 어느 레벨에서 열렸느냐에 따라 탭 가시성 분기
// MainMap 진입: 특성/스킨/분해/도감 보임 (직원강화/티켓 숨김)
// OfficeMap 진입: 직원강화/티켓/도감 보임 (특성/스킨/분해 숨김)
// Auto: 현재 맵 이름으로 자동 추론 (안전망)
UENUM(BlueprintType)
enum class EItemHubContext : uint8
{
	Auto      = 0 UMETA(DisplayName = "Auto"),       // GetMapName 기반 자동
	MainMap   = 1 UMETA(DisplayName = "MainMap"),
	OfficeMap = 2 UMETA(DisplayName = "OfficeMap")
};

/**
 * 글로벌 인벤토리 허브 위젯
 * - 6개 탭 (특성/직원강화/티켓/스킨/분해/도감) — 어디서 열든 같은 위젯
 * - 자동 탭 선택: MainMap 진입 시 [특성], OfficeMap 진입 시 [직원강화] 디폴트
 * - 외부에서 SetDefaultTab() 으로 강제 지정도 가능 (열기 전 호출)
 * - 진입점: BuildOpenWidget(우상단) + OfficeLayerWidget(우상단, 후속)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UInventoryHubWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 외부에서 디폴트 탭 강제 지정 (PushPromptClass 직후 호출)
	UFUNCTION(BlueprintCallable, Category = "Inventory Hub")
	void SetDefaultTab(EItemHubTab Tab);

	// 외부에서 진입 컨텍스트 지정 — 탭 가시성 + 디폴트 탭 동시 결정
	// PushPromptClass 직후 호출. Auto 면 GetMapName 으로 추론
	UFUNCTION(BlueprintCallable, Category = "Inventory Hub")
	void SetContext(EItemHubContext Context);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UFUNCTION()
	void OnCloseButtonClicked();

	UFUNCTION()
	void OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex);

	UFUNCTION()
	void OnBackgroundClicked();

	// 6 탭 버튼 — 순서 = EItemHubTab enum 값과 일치
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTabButtonWidget* TraitTabBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTabButtonWidget* EmployeeEnhanceTabBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTabButtonWidget* TicketTabBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTabButtonWidget* SkinTabBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTabButtonWidget* DismantleTabBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTabButtonWidget* EncyclopediaTabBtn;

	// 탭 콘텐츠 전환
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UWidgetSwitcher* ContentSwitcher;

	// 닫기 버튼 (UIE_CloseButton)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCloseButtonWidget* UIE_CloseButton;

	// 배경 클릭 시 닫기
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UButton* BackgroundBtn;

	// 통계 라벨 (옵션)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* OwnedCountText;        // "보유 47장"

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* EncyclopediaText;      // "도감 23/105"

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* DustText;              // "dust 1,240"

private:
	// 탭 버튼 그룹 (배타적 선택)
	UPROPERTY()
	UCommonButtonGroupBase* TabButtonGroup = nullptr;

	// 외부에서 SetDefaultTab 으로 지정된 값. NativeOnActivated 에서 사용 후 초기화
	UPROPERTY()
	EItemHubTab PendingDefaultTab = EItemHubTab::BuildingTrait;

	UPROPERTY()
	bool bHasPendingDefaultTab = false;

	// 외부에서 SetContext 로 지정된 값. NativeOnActivated 에서 사용
	UPROPERTY()
	EItemHubContext CurrentContext = EItemHubContext::Auto;

	// 현재 활성 탭
	UPROPERTY()
	EItemHubTab CurrentTab = EItemHubTab::BuildingTrait;

	// 초기 SelectButtonAtIndex(디폴트 탭이 비-0이면 핸들러 동기 발화)에 탭 사운드가 울리지 않게 하는 가드.
	// 위젯 풀 재사용 대비 NativeConstruct 시작에서 false 리셋
	bool bTabSoundReady = false;

	// 현재 맵 이름으로 디폴트 탭 결정 (MainMap→Trait, OfficeMap→EmployeeEnhance)
	EItemHubTab ResolveDefaultTabFromMap() const;

	// Auto → 실제 컨텍스트로 해소 (GetMapName 기반)
	EItemHubContext ResolveContext(EItemHubContext InContext) const;

	// 컨텍스트별 탭 버튼 가시성 토글 (특성/스킨/분해 vs 직원강화/티켓, 도감은 공통)
	void ApplyTabVisibility(EItemHubContext Context);

	// 컨텍스트의 디폴트 탭 (특성 vs 직원강화)
	EItemHubTab GetDefaultTabForContext(EItemHubContext Context) const;

	// 통계 라벨 갱신 (인벤토리 변경 시)
	void RefreshStatsDisplay();

	// 탭별 콘텐츠 로드 (서브탭 콘텐츠 위젯들은 추후 단계에 추가)
	void LoadTabContent(EItemHubTab Tab);
};
