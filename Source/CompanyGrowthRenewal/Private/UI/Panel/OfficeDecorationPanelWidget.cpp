// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/OfficeDecorationPanelWidget.h"
#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Components/ListView.h"
#include "Data/EntityCardData.h"
#include "Groups/CommonButtonGroupBase.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Office/OfficeDecorationCardWidget.h"
#include "Office/OfficeInterior.h"
#include "Kismet/GameplayStatics.h"

void UOfficeDecorationPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Back 버튼 이벤트 바인딩
	if (BackButton)
	{
		BackButton->OnClicked().AddUObject(this, &UOfficeDecorationPanelWidget::OnBackButtonClicked);
	}

	// 카테고리 탭 버튼 그룹 초기화
	DecorationTabButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	DecorationTabButtonGroup->SetSelectionRequired(true);

	// 카테고리 탭 버튼들을 그룹에 등록 (사용 빈도순)
	// 0: 가구, 1: 의자, 2: 화분, 3: 파티션, 4: 그림, 5: 창문, 6: 벽, 7: 바닥타일
	if (FurnitureTab)
	{
		DecorationTabButtonGroup->AddWidget(FurnitureTab);
		FurnitureTab->SetIsSelectable(true);
	}

	if (ChairTab)
	{
		DecorationTabButtonGroup->AddWidget(ChairTab);
		ChairTab->SetIsSelectable(true);
	}

	if (PlantTab)
	{
		DecorationTabButtonGroup->AddWidget(PlantTab);
		PlantTab->SetIsSelectable(true);
	}

	if (PartitionTab)
	{
		DecorationTabButtonGroup->AddWidget(PartitionTab);
		PartitionTab->SetIsSelectable(true);
	}

	if (PictureTab)
	{
		DecorationTabButtonGroup->AddWidget(PictureTab);
		PictureTab->SetIsSelectable(true);
	}

	if (WindowTab)
	{
		DecorationTabButtonGroup->AddWidget(WindowTab);
		WindowTab->SetIsSelectable(true);
	}

	if (WallTab)
	{
		DecorationTabButtonGroup->AddWidget(WallTab);
		WallTab->SetIsSelectable(true);
	}

	if (FloorTileTab)
	{
		DecorationTabButtonGroup->AddWidget(FloorTileTab);
		FloorTileTab->SetIsSelectable(true);
	}

	// 탭 선택 변경 이벤트 바인딩
	DecorationTabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(
		this, &UOfficeDecorationPanelWidget::OnDecorationTabSelectionChanged);

	// 초기 탭 설정 (가구 탭)
	if (DecorationTabButtonGroup && FurnitureTab)
	{
		DecorationTabButtonGroup->SelectButtonAtIndex(0);
	}

	// TableManager 가져오기
	UGameInstance* GI = GetWorld()->GetGameInstance();
	TableManager = GI->GetSubsystem<UTableManagerSubsystem>();

	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationPanelWidget] NativeConstruct"));
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

	// 초기 탭(0: 가구) 로드
	LoadCategoryByIndex(0);

	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationPanelWidget] NativeOnActivated"));
}

void UOfficeDecorationPanelWidget::NativeDestruct()
{
	Super::NativeDestruct();

	// Back 버튼 언바인딩
	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
	}

	// 탭 버튼 그룹 정리
	if (DecorationTabButtonGroup)
	{
		DecorationTabButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}

	// ListView Entry 생성 이벤트 언바인딩
	if (DecorationListView)
	{
		DecorationListView->OnEntryWidgetGenerated().RemoveAll(this);
	}

	CachedOfficeInterior = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationPanelWidget] NativeDestruct"));
}

void UOfficeDecorationPanelWidget::RefreshDecorationCards(EDecorationCategory Category)
{
	if (!DecorationListView || !TableManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[OfficeDecorationPanelWidget] DecorationListView or TableManager is null"));
		return;
	}

	DecorationListView->ClearListItems();
	CurrentCategory = Category;

	// 해당 카테고리 데이터 가져오기
	TArray<FDecorationCardTable> Items = TableManager->GetDecorationCardsByCategory(Category);

	// 데이터 추가 (위젯은 ListView가 자동 생성)
	for (const FDecorationCardTable& CardData : Items)
	{
		UEntityCardData* Data = NewObject<UEntityCardData>(this);
		Data->DecorationInfo = CardData;
		DecorationListView->AddItem(Data);
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationPanelWidget] Loaded %d items for category %d"), Items.Num(), (int32)Category);
}

EDecorationCategory UOfficeDecorationPanelWidget::IndexToCategory(int32 Index) const
{
	// 0=가구, 1=의자, 2=화분, 3=파티션, 4=그림, 5=창문, 6=벽, 7=바닥타일
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

	// 탭 선택 초기화 (0번 가구 탭)
	if (DecorationTabButtonGroup)
	{
		DecorationTabButtonGroup->SelectButtonAtIndex(0);
	}

	// ListView 정리
	if (DecorationListView)
	{
		DecorationListView->ClearListItems();
	}

	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationPanelWidget] NativeOnDeactivated"));
}

void UOfficeDecorationPanelWidget::OnBackButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationPanelWidget] Back button clicked"));
	CloseWithAnimation();
}

void UOfficeDecorationPanelWidget::OnDecorationTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationPanelWidget] Tab selection changed to index %d"), ButtonIndex);

	// 탭 변경 시 해당 카테고리로 ListView 갱신
	LoadCategoryByIndex(ButtonIndex);
}

void UOfficeDecorationPanelWidget::LoadCategoryByIndex(int32 CategoryIndex)
{
	// 인덱스 → 카테고리 변환 후 ListView 갱신
	EDecorationCategory Category = IndexToCategory(CategoryIndex);
	RefreshDecorationCards(Category);

	UE_LOG(LogTemp, Log, TEXT("[OfficeDecorationPanelWidget] Loaded category index: %d"), CategoryIndex);
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
