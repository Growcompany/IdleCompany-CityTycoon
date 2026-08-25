// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/InventoryHubWidget.h"
#include "UI/Element/Buttons/TabButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Groups/CommonButtonGroupBase.h"
#include "CommonButtonBase.h"
#include "Manager/BuildingTraitManagerSubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "Player/MainMapPlayerController.h"
#include "Engine/World.h"

void UInventoryHubWidget::SetDefaultTab(EItemHubTab Tab)
{
	PendingDefaultTab = Tab;
	bHasPendingDefaultTab = true;
}

void UInventoryHubWidget::SetContext(EItemHubContext Context)
{
	CurrentContext = Context;
}

void UInventoryHubWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bTabSoundReady = false;

	// 탭 버튼 그룹 셋업 (CLAUDE.md "탭 버튼 배타적 선택 패턴" 준수)
	TabButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	TabButtonGroup->SetSelectionRequired(true);

	auto RegisterTab = [this](UTabButtonWidget* Tab)
	{
		if (!Tab) return;
		if (UCommonButtonBase* Btn = Tab->GetButton())
		{
			TabButtonGroup->AddWidget(Btn);
			Btn->SetIsSelectable(true);
		}
	};
	RegisterTab(TraitTabBtn);
	RegisterTab(EmployeeEnhanceTabBtn);
	RegisterTab(TicketTabBtn);
	RegisterTab(SkinTabBtn);
	RegisterTab(DismantleTabBtn);
	RegisterTab(EncyclopediaTabBtn);

	TabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UInventoryHubWidget::OnTabSelectionChanged);

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UInventoryHubWidget::OnCloseButtonClicked);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UInventoryHubWidget::OnBackgroundClicked);
	}

	// CLAUDE.md "탭 버튼 배타적 선택 패턴" — 컨텍스트 해소/가시성/디폴트 탭 선택은 모두 NativeConstruct 에서.
	// NativeOnActivated 에서 SelectButtonAtIndex 호출 시 재진입 상태 꼬임 위험.
	const EItemHubContext ResolvedContext = ResolveContext(CurrentContext);
	ApplyTabVisibility(ResolvedContext);

	const EItemHubTab DefaultTab = bHasPendingDefaultTab
		? PendingDefaultTab
		: GetDefaultTabForContext(ResolvedContext);
	bHasPendingDefaultTab = false;

	if (TabButtonGroup)
	{
		TabButtonGroup->SelectButtonAtIndex(static_cast<int32>(DefaultTab));
	}
	bTabSoundReady = true;
	CurrentTab = DefaultTab;
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(static_cast<int32>(DefaultTab));
	}
}

void UInventoryHubWidget::NativeDestruct()
{
	if (TabButtonGroup)
	{
		TabButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &UInventoryHubWidget::OnCloseButtonClicked);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UInventoryHubWidget::OnBackgroundClicked);
	}

	Super::NativeDestruct();
}

void UInventoryHubWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// CLAUDE.md "탭 버튼 배타적 선택 패턴" — NativeOnActivated 는 데이터 갱신만 (ButtonGroup 상태 X)
	RefreshStatsDisplay();
	LoadTabContent(CurrentTab);

	UE_LOG(LogTemp, Log, TEXT("[InventoryHub] Activated tab=%d"), static_cast<int32>(CurrentTab));
}

void UInventoryHubWidget::NativeOnDeactivated()
{
	// CLAUDE.md "UI/Normal 입력 모드 전환 규칙" — 진입측이 GoToUIMode 했으니 종료측에서 복원
	// 다른 모드(BuildPlace 등)를 덮어쓰지 않도록 UI 모드일 때만 Normal 로 복귀
	if (UWorld* World = GetWorld())
	{
		if (AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(World->GetFirstPlayerController()))
		{
			if (PC->GetCurrentInputMode() == EInputMode::UI)
			{
				PC->GoToNormalMode();
			}
		}
	}

	Super::NativeOnDeactivated();
}

EItemHubContext UInventoryHubWidget::ResolveContext(EItemHubContext InContext) const
{
	if (InContext != EItemHubContext::Auto) return InContext;

	if (UWorld* World = GetWorld())
	{
		const FString MapName = World->GetMapName();
		if (MapName.Contains(TEXT("OfficeMap"), ESearchCase::IgnoreCase))
		{
			return EItemHubContext::OfficeMap;
		}
		if (MapName.Contains(TEXT("MainMap"), ESearchCase::IgnoreCase))
		{
			return EItemHubContext::MainMap;
		}
	}
	return EItemHubContext::MainMap; // 안전망
}

EItemHubTab UInventoryHubWidget::GetDefaultTabForContext(EItemHubContext Context) const
{
	switch (Context)
	{
		case EItemHubContext::OfficeMap: return EItemHubTab::EmployeeEnhance;
		case EItemHubContext::MainMap:   return EItemHubTab::BuildingTrait;
		default:                         return EItemHubTab::BuildingTrait;
	}
}

void UInventoryHubWidget::ApplyTabVisibility(EItemHubContext Context)
{
	// 옵션 A — 모든 탭 항상 표시. 컨텍스트는 디폴트 탭 선택에만 영향.
	// 사용자가 자유롭게 다른 컨텍스트 탭도 탐색 가능 (OfficeMap에서 빌딩 도감 진행도 확인 등).
	// 향후 옵션 C (컨텍스트별 숨김) 로 회귀하려면 본 함수만 수정.
	auto Show = [](UWidget* Widget)
	{
		if (Widget) Widget->SetVisibility(ESlateVisibility::Visible);
	};

	Show(TraitTabBtn);
	Show(EmployeeEnhanceTabBtn);
	Show(TicketTabBtn);
	Show(SkinTabBtn);
	Show(DismantleTabBtn);
	Show(EncyclopediaTabBtn);

	(void)Context; // 디폴트 탭 선택은 GetDefaultTabForContext 에서 처리
}

EItemHubTab UInventoryHubWidget::ResolveDefaultTabFromMap() const
{
	if (UWorld* World = GetWorld())
	{
		const FString MapName = World->GetMapName();
		// PIE prefix("UEDPIE_0_") 또는 패키지 경로 prefix 가 붙을 수 있으므로 Contains 매칭
		if (MapName.Contains(TEXT("OfficeMap"), ESearchCase::IgnoreCase))
		{
			return EItemHubTab::EmployeeEnhance;
		}
		if (MapName.Contains(TEXT("MainMap"), ESearchCase::IgnoreCase))
		{
			return EItemHubTab::BuildingTrait;
		}
		// 그 외 맵(LootBox, Recruitment, WorldMap 등)은 빌딩 특성 디폴트
	}
	return EItemHubTab::BuildingTrait;
}

void UInventoryHubWidget::OnTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	if (ButtonIndex < 0 || ButtonIndex > static_cast<int32>(EItemHubTab::Encyclopedia)) return;

	if (bTabSoundReady)
	{
		if (USoundManagerSubsystem* SM = GetGameInstance()->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::TabSwitch);
		}
	}

	const EItemHubTab NewTab = static_cast<EItemHubTab>(ButtonIndex);
	CurrentTab = NewTab;

	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(ButtonIndex);
	}

	LoadTabContent(NewTab);

	UE_LOG(LogTemp, Log, TEXT("[InventoryHub] Tab switched to %d"), ButtonIndex);
}

void UInventoryHubWidget::LoadTabContent(EItemHubTab Tab)
{
	// Phase 4-B 후속 단계에서 각 탭별 콘텐츠 위젯 (그리드/스크롤 리스트) 동적 채움
	// 현재는 디버그 로그만
	switch (Tab)
	{
		case EItemHubTab::BuildingTrait:
			UE_LOG(LogTemp, Log, TEXT("[InventoryHub] Load BuildingTrait tab — TODO 후속: 보유 특성 그리드"));
			break;
		case EItemHubTab::EmployeeEnhance:
			UE_LOG(LogTemp, Log, TEXT("[InventoryHub] Load EmployeeEnhance tab — TODO 후속: 강화 주문서/큐브/보호권 리스트"));
			break;
		case EItemHubTab::Ticket:
			UE_LOG(LogTemp, Log, TEXT("[InventoryHub] Load Ticket tab — TODO 후속: 채용권/스킬권/특성권 카드"));
			break;
		case EItemHubTab::Skin:
			UE_LOG(LogTemp, Log, TEXT("[InventoryHub] Load Skin tab — TODO 후속: 보유 빌딩 스킨 그리드"));
			break;
		case EItemHubTab::Dismantle:
			UE_LOG(LogTemp, Log, TEXT("[InventoryHub] Load Dismantle tab — TODO 후속: dust 교환소"));
			break;
		case EItemHubTab::Encyclopedia:
			UE_LOG(LogTemp, Log, TEXT("[InventoryHub] Load Encyclopedia tab — TODO 후속: 도감 분야별 그리드"));
			break;
	}
}

void UInventoryHubWidget::RefreshStatsDisplay()
{
	UBuildingTraitManagerSubsystem* TraitMgr = GetGameInstance()->GetSubsystem<UBuildingTraitManagerSubsystem>();
	if (!TraitMgr) return;

	if (OwnedCountText)
	{
		int32 TotalOwned = 0;
		for (const TPair<FName, int32>& Pair : TraitMgr->GetAllOwnedTraits())
		{
			TotalOwned += Pair.Value;
		}
		OwnedCountText->SetText(FText::FromString(FString::Printf(TEXT("보유 %d장"), TotalOwned)));
	}

	if (EncyclopediaText)
	{
		const int32 EncyclopediaCount = TraitMgr->GetEncyclopediaCount();
		EncyclopediaText->SetText(FText::FromString(FString::Printf(TEXT("도감 %d/105"), EncyclopediaCount)));
	}

	if (DustText)
	{
		const int32 Dust = TraitMgr->GetDust();
		DustText->SetText(FText::FromString(FString::Printf(TEXT("dust %d"), Dust)));
	}
}

void UInventoryHubWidget::OnCloseButtonClicked()
{
	DeactivateWidget();
}

void UInventoryHubWidget::OnBackgroundClicked()
{
	DeactivateWidget();
}
