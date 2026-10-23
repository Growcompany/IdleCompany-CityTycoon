// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/OfficeDecorationPanelWidget.h"
#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Components/ListView.h"
#include "CommonTextBlock.h"
#include "UI/Element/Buttons/TabButtonWidget.h"
#include "Data/EntityCardData.h"
#include "Groups/CommonButtonGroupBase.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Office/OfficeDecorationCardWidget.h"
#include "UI/Element/Office/CatalogEdgeFade.h"
#include "Office/OfficeInterior.h"
#include "Kismet/GameplayStatics.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "UI/Panel/OfficeMainWidget.h"

void UOfficeDecorationPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TableManager = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	if (RelocateButton) RelocateButton->OnClicked().AddUObject(this, &ThisClass::OnRelocateClicked);
	if (BackButton) BackButton->OnCloseClicked.AddUniqueDynamic(this, &ThisClass::OnBackButtonClicked);
	if (UWidget* ListFade = GetWidgetFromName(TEXT("CatalogListFade"))) BindCatalogEdgeFade(this, Cast<UTileView>(DecorationListView), ListFade);
	else UE_LOG(LogTemp, Warning, TEXT("[OfficeDecorationPanelWidget] CatalogListFade 미배치 ― 카드 끝 흐림 없음 (WBP 확인)"));
	CatalogRows.Reset();
	CatalogGroups.Reset();
	if (TableManager) TableManager->GetDecorationCatalog().GenerateValueArray(CatalogRows);
	CatalogRows.Sort([](const FDecorationCatalogRow& A, const FDecorationCatalogRow& B)
	{
		return A.GroupOrder == B.GroupOrder ? A.CategoryOrder < B.CategoryOrder : A.GroupOrder < B.GroupOrder;
	});
	for (const FDecorationCatalogRow& Row : CatalogRows) CatalogGroups.AddUnique(Row.Group);
	CatalogGroupButtons = NewObject<UCommonButtonGroupBase>(this);
	CatalogGroupButtons->SetSelectionRequired(true);
	const TArray<UTabButtonWidget*> GroupTabs = {GroupTab0, GroupTab1, GroupTab2, GroupTab3};
	for (int32 Index = 0; Index < GroupTabs.Num(); ++Index)
	{
		UTabButtonWidget* Button = GroupTabs[Index];
		if (!Button || !Button->GetButton()) continue;
		CatalogGroupButtons->AddWidget(Button->GetButton());
		Button->GetButton()->SetIsSelectable(true);
		Button->SetVisibility(CatalogGroups.IsValidIndex(Index) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (CatalogGroups.IsValidIndex(Index))
		{
			const FDecorationCatalogRow* Row = CatalogRows.FindByPredicate([&](const FDecorationCatalogRow& Item) { return Item.Group == CatalogGroups[Index]; });
			Button->SetTabText(Row ? Row->GroupLabel : FText::GetEmpty());
		}
	}
	DecorationTabButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	DecorationTabButtonGroup->SetSelectionRequired(true);
	for (UButtonWidget* Button : GetCategoryButtons())
	{
		if (!Button) continue;
		DecorationTabButtonGroup->AddWidget(Button);
		Button->SetIsSelectable(true);
	}
	DecorationTabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &ThisClass::OnDecorationTabSelectionChanged);
	CatalogGroupButtons->OnSelectedButtonBaseChanged.AddDynamic(this, &ThisClass::OnCatalogGroupChanged);
	CurrentGroupIndex = 0;
	CurrentFilterIndex = 0;
	CatalogGroupButtons->SelectButtonAtIndex(0);
	DecorationTabButtonGroup->SelectButtonAtIndex(0);
	OnCatalogGroupChanged(nullptr, 0);
	OnDecorationTabSelectionChanged(nullptr, 0);
}

TArray<UButtonWidget*> UOfficeDecorationPanelWidget::GetCategoryButtons() const
{
	return {AllTab, FurnitureTab, ChairTab, PlantTab, PartitionTab, PictureTab, WindowTab, WallTab, FloorTileTab, CarpetTab};
}

const FDecorationCatalogRow* UOfficeDecorationPanelWidget::FindCategory(EDecorationCategory Category) const
{
	return CatalogRows.FindByPredicate([Category](const FDecorationCatalogRow& Row) { return Row.Category == Category; });
}

bool UOfficeDecorationPanelWidget::IsCategoryListed(EDecorationCategory Category) const
{
	const FDecorationCatalogRow* Row = FindCategory(Category);
	const FName Group = CatalogGroups.IsValidIndex(CurrentGroupIndex) ? CatalogGroups[CurrentGroupIndex] : NAME_None;
	return Row && Row->Group == Group && (CurrentFilterIndex == 0 || IndexToCategory(CurrentFilterIndex - 1) == Category);
}

UWidget* UOfficeDecorationPanelWidget::GetCategoryNavigationWidget(EDecorationCategory Category) const
{
	if (!IsActivated() || IsCategoryListed(Category)) return nullptr;
	const FDecorationCatalogRow* Row = FindCategory(Category);
	if (!Row) return nullptr;

	const int32 GroupIndex = CatalogGroups.IndexOfByKey(Row->Group);
	if (GroupIndex != CurrentGroupIndex)
	{
		const TArray<UTabButtonWidget*> GroupTabs = {GroupTab0, GroupTab1, GroupTab2, GroupTab3};
		return GroupTabs.IsValidIndex(GroupIndex) && GroupTabs[GroupIndex] ? GroupTabs[GroupIndex]->GetButton() : nullptr;
	}
	// 세부 필터 버튼 배열은 0 = 전체, i = IndexToCategory(i - 1) — RefreshCatalog 와 같은 대응
	const TArray<UButtonWidget*> Buttons = GetCategoryButtons();
	for (int32 Index = 1; Index < Buttons.Num(); ++Index)
	{
		if (IndexToCategory(Index - 1) == Category) return Buttons[Index];
	}
	return nullptr;
}

UWidget* UOfficeDecorationPanelWidget::GetCheapestAffordableCardWidget(EDecorationCategory Category) const
{
	if (!IsActivated() || !DecorationListView || !IsCategoryListed(Category)) return nullptr;
	const UResourceItemManager* ResMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UResourceItemManager>() : nullptr;
	const int64 Money = ResMgr ? ResMgr->GetResourceAmount(EResourceType::Money) : 0;

	UOfficeDecorationCardWidget* Cheapest = nullptr;
	for (UUserWidget* Entry : DecorationListView->GetDisplayedEntryWidgets())
	{
		UOfficeDecorationCardWidget* Card = Cast<UOfficeDecorationCardWidget>(Entry);
		if (!Card || !Card->IsVisible()) continue;
		const FDecorationCardTable& Info = Card->GetDecorationInfo();
		// 카드 탭(HandleCardClicked)이 Money 로만 값을 막는다 — 같은 기준으로 거른다
		if (Info.Category != Category || Info.Price > Money) continue;
		if (!Cheapest || Info.Price < Cheapest->GetDecorationInfo().Price) Cheapest = Card;
	}
	return Cheapest;
}

void UOfficeDecorationPanelWidget::UpdateCategoryFilters()
{
	const TArray<UButtonWidget*> Buttons = GetCategoryButtons();
	const FName Group = CatalogGroups.IsValidIndex(CurrentGroupIndex) ? CatalogGroups[CurrentGroupIndex] : NAME_None;
	int32 VisibleCategories = 0;
	for (int32 Index = 1; Index < Buttons.Num(); ++Index)
	{
		if (!Buttons[Index]) continue;
		const FDecorationCatalogRow* Row = FindCategory(IndexToCategory(Index - 1));
		const bool bVisible = Row && Row->Group == Group && !Row->CategoryLabel.IsEmpty();
		Buttons[Index]->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Buttons[Index]->SetButtonText(Row ? Row->CategoryLabel : FText::GetEmpty());
		VisibleCategories += bVisible ? 1 : 0;
	}
	if (AllTab) AllTab->SetVisibility(VisibleCategories > 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UOfficeDecorationPanelWidget::OnCatalogGroupChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	CurrentGroupIndex = ButtonIndex;
	CurrentFilterIndex = 0;
	UpdateCategoryFilters();
	// 세부 선택은 그룹이 소유한다. 한 종류뿐인 분류는 그 버튼을 선택한다.
	int32 FilterIndex = 0;
	if (AllTab && AllTab->GetVisibility() == ESlateVisibility::Collapsed)
	{
		const TArray<UButtonWidget*> Buttons = GetCategoryButtons();
		for (int32 Index = 1; Index < Buttons.Num(); ++Index)
			if (Buttons[Index] && Buttons[Index]->GetVisibility() != ESlateVisibility::Collapsed) { FilterIndex = Index; break; }
	}
	CurrentFilterIndex = FilterIndex;
	if (DecorationTabButtonGroup) DecorationTabButtonGroup->SelectButtonAtIndex(FilterIndex);
	RefreshCatalog();
}

void UOfficeDecorationPanelWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// OfficeInterior 캐시 및 현재 FloorTile RowName 가져오기
	CachedOfficeInterior = Cast<AOfficeInterior>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AOfficeInterior::StaticClass()));

	if (CachedOfficeInterior)
	{
		SelectedFloorTileRowName = CachedOfficeInterior->GetCurrentFloorTileRowName();
		UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationPanelWidget] Current floor tile: %s"),
			*SelectedFloorTileRowName.ToString());
	}

	// ListView Entry 생성 이벤트 바인딩
	if (DecorationListView)
	{
		DecorationListView->OnEntryWidgetGenerated().AddUObject(
			this, &UOfficeDecorationPanelWidget::OnFloorTileEntryGenerated);
	}

	// 마지막으로 선택한 분류와 필터를 유지한다.
	RefreshCatalog();

	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationPanelWidget] NativeOnActivated"));
}

void UOfficeDecorationPanelWidget::NativeDestruct()
{
	if (RelocateButton) RelocateButton->OnClicked().RemoveAll(this);
	if (CatalogGroupButtons) CatalogGroupButtons->OnSelectedButtonBaseChanged.RemoveAll(this);
	Super::NativeDestruct();

	if (BackButton) BackButton->OnCloseClicked.RemoveDynamic(this, &ThisClass::OnBackButtonClicked);

	// 탭 버튼 그룹 정리
	if (DecorationTabButtonGroup)
	{
		DecorationTabButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}

	// ListView Entry 생성 이벤트 언바인딩
	if (DecorationListView)
	{
		DecorationListView->OnEntryWidgetGenerated().RemoveAll(this);
		DecorationListView->OnListViewScrolled().RemoveAll(this);
	}

	CachedOfficeInterior = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationPanelWidget] NativeDestruct"));
}

void UOfficeDecorationPanelWidget::RefreshCatalog()
{
	if (!DecorationListView || !TableManager) return;
	DecorationListView->ClearListItems();
	const FName Group = CatalogGroups.IsValidIndex(CurrentGroupIndex) ? CatalogGroups[CurrentGroupIndex] : NAME_None;
	TArray<FDecorationCardTable> Items = TableManager->GetDecorationCardInfos();
	Items.RemoveAll([&](const FDecorationCardTable& Item)
	{
		const FDecorationCatalogRow* Row = FindCategory(Item.Category);
		return !Row || Row->Group != Group || (CurrentFilterIndex > 0 && Item.Category != IndexToCategory(CurrentFilterIndex - 1));
	});
	Items.StableSort([&](const FDecorationCardTable& A, const FDecorationCardTable& B)
	{
		const int32 OrderA = FindCategory(A.Category)->CategoryOrder;
		const int32 OrderB = FindCategory(B.Category)->CategoryOrder;
		return OrderA < OrderB;
	});
	for (const FDecorationCardTable& CardData : Items)
	{
		UEntityCardData* Data = NewObject<UEntityCardData>(this);
		Data->DecorationInfo = CardData;
		DecorationListView->AddItem(Data);
	}
	DecorationListView->ScrollToTop();
	ScheduleCatalogEdgeFade(this, Cast<UTileView>(DecorationListView), GetWidgetFromName(TEXT("CatalogListFade")));
	if (CatalogCount) CatalogCount->SetText(FText::Format(NSLOCTEXT("Office", "CatalogCount", "{0}개"), FText::AsNumber(Items.Num())));
}

EDecorationCategory UOfficeDecorationPanelWidget::IndexToCategory(int32 Index) const
{
	// GetCategoryButtons에서 전체 버튼을 제외한 순서와 일치한다.
	switch (Index)
	{
	case 0: return EDecorationCategory::Furniture;
	case 1: return EDecorationCategory::Chair;
	case 2: return EDecorationCategory::Plant;
	case 3: return EDecorationCategory::Partition;
	case 4: return EDecorationCategory::Picture;
	case 5: return EDecorationCategory::Window;
	case 6: return EDecorationCategory::Wall;
	case 7: return EDecorationCategory::FloorTile;
	case 8: return EDecorationCategory::Carpet;
	default: return EDecorationCategory::Furniture;
	}
}

void UOfficeDecorationPanelWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	// ListView Entry 생성 이벤트 언바인딩
	if (DecorationListView)
	{
		DecorationListView->OnEntryWidgetGenerated().RemoveAll(this);
	}

	// ListView 정리
	if (DecorationListView)
	{
		DecorationListView->ClearListItems();
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationPanelWidget] NativeOnDeactivated"));
	if (bPendingPlacementEdit)
	{
		bPendingPlacementEdit = false;
		if (UUIManagerSubsystem* UIManager = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>())
			if (UOfficeMainWidget* Main = UIManager->GetOfficeMain()) Main->RequestPlacementEdit();
	}
}

void UOfficeDecorationPanelWidget::OnRelocateClicked()
{
	if (IsCloseRequested()) return;
	bPendingPlacementEdit = true;
	CloseWithAnimation();
}

UWidget* UOfficeDecorationPanelWidget::GetCloseButtonWidget() const
{
	return BackButton;
}

void UOfficeDecorationPanelWidget::OnBackButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationPanelWidget] Back button clicked"));
	CloseWithAnimation();
}

void UOfficeDecorationPanelWidget::OnDecorationTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	CurrentFilterIndex = ButtonIndex;
	const TArray<UButtonWidget*> Buttons = GetCategoryButtons();
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
		if (Buttons[Index])
		{
			Buttons[Index]->SetTextColor(FSlateColor(FLinearColor::FromSRGBColor(
				Index == ButtonIndex ? FColor(255, 248, 240) : FColor(185, 194, 207))));
			Buttons[Index]->SetTextOutlineEnabled(false);
		}
	RefreshCatalog();
}

void UOfficeDecorationPanelWidget::OnFloorTileEntryGenerated(UUserWidget& EntryWidget)
{
	UOfficeDecorationCardWidget* DecoCard = Cast<UOfficeDecorationCardWidget>(&EntryWidget);
	if (!DecoCard)
	{
		return;
	}

	const FDecorationCardTable& CardInfo = DecoCard->GetDecorationInfo();

	// FloorTile이 아닌 카테고리는 선택 해제 (위젯 재활용 시 이전 상태 초기화)
	if (CardInfo.Category != EDecorationCategory::FloorTile)
	{
		DecoCard->SetSelected(false);
		return;
	}

	// 현재 적용된 타일이면 선택 표시
	bool bShouldSelect = (CardInfo.RowName == SelectedFloorTileRowName);
	DecoCard->SetSelected(bShouldSelect);

	// FloorTile 선택 이벤트 바인딩 (중복 바인딩 방지를 위해 먼저 제거)
	DecoCard->OnDecorationCardSelected.RemoveDynamic(
		this, &UOfficeDecorationPanelWidget::OnFloorTileCardSelected);
	DecoCard->OnDecorationCardSelected.AddDynamic(
		this, &UOfficeDecorationPanelWidget::OnFloorTileCardSelected);
}

void UOfficeDecorationPanelWidget::OnFloorTileCardSelected(const FDecorationCardTable& CardData)
{
	// FloorTile 카테고리만 처리
	if (CardData.Category != EDecorationCategory::FloorTile)
	{
		return;
	}

	// 모든 FloorTile 카드 선택 해제
	DeselectAllFloorTileCards();

	// 선택된 FloorTile RowName 업데이트
	SelectedFloorTileRowName = CardData.RowName;

	// 해당 카드 찾아서 선택 표시
	if (DecorationListView)
	{
		TArray<UUserWidget*> DisplayedWidgets = DecorationListView->GetDisplayedEntryWidgets();
		for (UUserWidget* Widget : DisplayedWidgets)
		{
			UOfficeDecorationCardWidget* DecoCard = Cast<UOfficeDecorationCardWidget>(Widget);
			if (DecoCard && DecoCard->GetDecorationInfo().RowName == SelectedFloorTileRowName)
			{
				DecoCard->SetSelected(true);
				break;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationPanelWidget] FloorTile selected: %s"),
		*SelectedFloorTileRowName.ToString());
}

void UOfficeDecorationPanelWidget::DeselectAllFloorTileCards()
{
	if (!DecorationListView)
	{
		return;
	}

	TArray<UUserWidget*> DisplayedWidgets = DecorationListView->GetDisplayedEntryWidgets();
	for (UUserWidget* Widget : DisplayedWidgets)
	{
		UOfficeDecorationCardWidget* DecoCard = Cast<UOfficeDecorationCardWidget>(Widget);
		if (DecoCard && DecoCard->GetDecorationInfo().Category == EDecorationCategory::FloorTile)
		{
			DecoCard->SetSelected(false);
		}
	}
}
