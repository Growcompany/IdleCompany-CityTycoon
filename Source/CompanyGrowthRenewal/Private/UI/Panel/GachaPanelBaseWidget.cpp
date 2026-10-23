// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/GachaPanelBaseWidget.h"
#include "Global/GlobalUtilFunctions.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Gacha/GachaPanelFrameWidget.h"
#include "Manager/CloudEconomySubsystem.h"
#include "Manager/ItemInventoryManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Groups/CommonButtonGroupBase.h"
#include "CommonButtonBase.h"
#include "UI/Element/Buttons/GachaBannerButton.h"
#include "UI/Element/Gacha/GachaOddsCellWidget.h"
#include "Enum/LootBoxRarity.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"

void UGachaPanelBaseWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		ItemMgr = GI->GetSubsystem<UItemInventoryManager>();
		TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
		UIMgr = GI->GetSubsystem<UUIManagerSubsystem>();
	}

	// 저작 프리뷰(고정 35%)가 첫 갱신 전에 보이지 않도록
	UGlobalUtilFunctions::InitProgressHead(MileageHead);

	UCloseButtonWidget* CloseButton = GachaFrame ? GachaFrame->GetCloseButton() : nullptr;
	if (CloseButton && !CloseButton->OnCloseClicked.IsBound())
	{
		CloseButton->OnCloseClicked.AddDynamic(this, &UGachaPanelBaseWidget::OnCloseButtonClicked);
	}
	if (PullButton)
	{
		PullButton->OnClicked().RemoveAll(this);
		PullButton->OnClicked().AddUObject(this, &UGachaPanelBaseWidget::OnPullClicked);
	}
	if (ProbabilityInfoButton)
	{
		ProbabilityInfoButton->OnClicked().RemoveAll(this);
		ProbabilityInfoButton->OnClicked().AddUObject(this, &UGachaPanelBaseWidget::OnProbabilityInfoClicked);
	}
	if (MileageExchangeButton)
	{
		MileageExchangeButton->OnClicked().RemoveAll(this);
		MileageExchangeButton->OnClicked().AddUObject(this, &UGachaPanelBaseWidget::OnMileageExchangeClicked);
	}

	// 채용·특성·스킨 세 패널이 이 베이스를 공유한다 — 한곳에서 구독해야 파생마다 빠뜨리지 않는다
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCloudEconomySubsystem* Cloud = GI->GetSubsystem<UCloudEconomySubsystem>())
		{
			Cloud->OnEconomyReady.AddUObject(this, &UGachaPanelBaseWidget::HandleEconomyReady);
		}
	}
}

void UGachaPanelBaseWidget::HandleEconomyReady(bool bReady)
{
	// 잠긴 동안에도 뽑기 버튼은 살려 둔다 — 조용히 죽은 버튼은 "티켓·다이아가 사라졌다" 로 읽힌다.
	// 해제되면 서버값으로 정정된 잔액·티켓·마일리지로 게이지와 CTA 를 다시 계산한다
	RefreshAll();
}

void UGachaPanelBaseWidget::NativeDestruct()
{
	if (UCloseButtonWidget* CloseButton = GachaFrame ? GachaFrame->GetCloseButton() : nullptr)
	{
		CloseButton->OnCloseClicked.RemoveAll(this);
	}
	if (BannerGroup)
	{
		BannerGroup->OnButtonBaseClicked.RemoveAll(this);
		// 선택 필수인 채로 비우면 엔진이 남은 배너를 재선택한다(클릭음 또는 ensure)
		BannerGroup->SetSelectionRequired(false);
		// 재구성 때 새 그룹이 같은 배너를 받으므로 옛 그룹의 배너 바인딩을 끊는다
		BannerGroup->RemoveAll();
		BannerGroup = nullptr;
	}
	if (PullButton) { PullButton->OnClicked().RemoveAll(this); }
	if (ProbabilityInfoButton) { ProbabilityInfoButton->OnClicked().RemoveAll(this); }
	if (MileageExchangeButton) { MileageExchangeButton->OnClicked().RemoveAll(this); }
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCloudEconomySubsystem* Cloud = GI->GetSubsystem<UCloudEconomySubsystem>())
		{
			Cloud->OnEconomyReady.RemoveAll(this);
		}
	}

	Super::NativeDestruct();
}

UWidget* UGachaPanelBaseWidget::GetPullButtonWidget() const
{
	return PullButton;
}

UWidget* UGachaPanelBaseWidget::GetCloseButtonWidget() const
{
	return GachaFrame ? GachaFrame->GetCloseButton() : nullptr;
}

void UGachaPanelBaseWidget::RefreshAll()
{
	UpdateHeroAndProbability();
	UpdateGauges();
	UpdateCTALabel();
}

void UGachaPanelBaseWidget::OnCloseButtonClicked()
{
	// PushPromptClass 로 열리므로 DeactivateWidget 으로 닫기
	DeactivateWidget();
}

void UGachaPanelBaseWidget::MakeBannerGroup(const TArray<UCommonButtonBase*>& Banners)
{
	BannerGroup = NewObject<UCommonButtonGroupBase>(this);
	BannerGroup->SetSelectionRequired(true);
	for (UCommonButtonBase* Banner : Banners)
	{
		BannerGroup->AddWidget(Banner);
	}
	// OnButtonBaseClicked = 매 클릭 발동. OnSelectedButtonBaseChanged 는 selectable 아닌 버튼에서 침묵해 전환이 안 먹는다.
	BannerGroup->OnButtonBaseClicked.AddDynamic(this, &UGachaPanelBaseWidget::OnBannerSelected);
	BannerGroup->SelectButtonAtIndex(0);
	// 그룹 선택은 클릭 델리게이트를 타지 않으므로 파생 티어 상태도 직접 맞춘다
	SetBannerIndex(0);
}

void UGachaPanelBaseWidget::OnBannerSelected(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	SetBannerIndex(ButtonIndex);
	// 클릭 경로라 선택 하이라이트는 수동 동기화. 선택 변경 델리게이트엔 안 묶여 있어 재진입 없음.
	if (BannerGroup) { BannerGroup->SelectButtonAtIndex(ButtonIndex); }
	RefreshAll();
}

void UGachaPanelBaseWidget::RebuildProbabilityPills(const TArray<FGachaRarityChance>& Rows)
{
	if (!ProbabilityPillRow) { return; }
	ProbabilityPillRow->ClearChildren();
	const TSubclassOf<UUserWidget> CellClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::GachaOddsCell) : nullptr;
	if (!CellClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaPanel] GachaOddsCell missing from DT_WidgetClass"));
		return;
	}
	int32 VisibleIndex = 0;
	for (const FGachaRarityChance& Row : Rows)
	{
		if (Row.Percent <= 0.f) { continue; }
		UGachaOddsCellWidget* Cell = CreateWidget<UGachaOddsCellWidget>(GetOwningPlayer(), CellClass.Get());
		if (!Cell) { continue; }
		Cell->SetOdds(FText::FromString(FLootBoxRarityUtility::GetKoreanName(Row.Rarity)),
			Row.Percent, FLootBoxRarityUtility::GetRarityColor(Row.Rarity), VisibleIndex > 0);
		if (UHorizontalBoxSlot* CellSlot = ProbabilityPillRow->AddChildToHorizontalBox(Cell))
		{
			CellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
		++VisibleIndex;
	}
}

void UGachaPanelBaseWidget::SetGauge(UProgressBar* Bar, UImage* Head, UTextBlock* Label, float Pct, const FText& Text)
{
	if (Label) { Label->SetText(Text); }
	if (Bar)
	{
		Bar->SetPercent(Pct);
		UGlobalUtilFunctions::UpdateProgressHead(Bar, Head, Pct);
	}
}

void UGachaPanelBaseWidget::SetBannerOwnedCount(UCommonButtonBase* Banner, EItemType Ticket)
{
	// 배너 BindWidget 타입은 UCommonButtonBase 계약 유지 — 실제 위젯만 캐스팅해서 수량을 넣는다
	if (!ItemMgr) { return; }
	if (UGachaBannerButton* B = Cast<UGachaBannerButton>(Banner))
	{
		B->SetOwnedCount(ItemMgr->GetItemCount(Ticket), true);
	}
}

void UGachaPanelBaseWidget::ApplyPullButton(UIconWithButtonWidget* Button, const FText& Label, bool bEnabled, EItemType TicketForIcon)
{
	if (!Button) { return; }
	Button->SetButtonText(Label);
	Button->SetIsEnabled(bEnabled);
	Button->SetCount(0);   // 비용 칸은 호출부가 SetItemCost 로 채운다
	if (TableMgr)
	{
		bool bIconFound = false;
		UObject* Icon = TableMgr->GetItemIconResource(TicketForIcon, bIconFound);
		Button->SetIconResource(bIconFound ? Icon : nullptr);
	}
}

void UGachaPanelBaseWidget::SetExchangeLabel(int32 Points)
{
	if (!MileageExchangeButton) { return; }
	MileageExchangeButton->SetButtonText(FText::FromString(FString::Printf(TEXT("마일리지 교환 %dP"), Points)));
}

UUserWidget* UGachaPanelBaseWidget::CreateOverlayWidget(EWidgetType Type)
{
	if (!TableMgr) { return nullptr; }
	TSubclassOf<UUserWidget> Cls = TableMgr->GetWidgetClass(Type);
	// 결과 오버레이는 뽑기가 끝난 뒤 만들어지므로, 조용히 실패하면 플레이어는 결과를 못 본다
	if (!Cls)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaPanel] %s: 오버레이 위젯 클래스 없음 (%s) - DT_WidgetClass 행 확인"),
			*GetName(), *UEnum::GetValueAsString(Type));
		return nullptr;
	}
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GachaPanel] %s: 소유 플레이어가 없어 오버레이 생성 불가 (%s)"),
			*GetName(), *UEnum::GetValueAsString(Type));
		return nullptr;
	}
	return CreateWidget<UUserWidget>(PC, Cls.Get());
}
