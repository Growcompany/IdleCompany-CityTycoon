#include "UI/Panel/BuildModalWidget.h"
#include "UI/Element/Common/ModalFrameWidget.h"

#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/EntityManager.h"
#include "Manager/SpawnManager.h"
#include "Entity/Plot/CityPlotActor.h" // 수용량 권위(배치형=박스 산출)
#include "Manager/SaveLoadManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Table/ResourceInfo.h"
#include "Enum/ResourceType.h"

#include "UI/UIBase.h"
#include "UI/Element/Buttons/IndustryButtonWidget.h"
#include "UI/Element/Buttons/TabButtonWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Cards/BuildEntityCardWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Panel/BuildPlacementPanelWidget.h"
#include "UI/Element/Office/CatalogEdgeFade.h"
#include "CommonTextBlock.h"

#include "Groups/CommonButtonGroupBase.h"
#include "CommonButtonBase.h"

#include "Components/TileView.h"
#include "Components/Button.h"
#include "Data/EntityCardData.h"
#include "Table/CityPlotData.h"   // FCityPlotData (부지 격자/수용)
#include "Table/BuildingData.h"   // FBuildingData (footprint 칸수)
#include "Enum/NotificationType.h"
#include "Player/MainMapPlayerController.h"
#include "Player/PlayerCamera.h"

void UBuildModalWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		TableManager = GI->GetSubsystem<UTableManagerSubsystem>();
	}

	// 산업 타일 수집 + 런타임 확정 산업 주입 (C++ 가 확정 소스)
	AllTiles.Empty();
	const TArray<TPair<UIndustryButtonWidget*, ECompanyType>> TileDefs = {
		{ GameTile, ECompanyType::Game },
		{ ITTile, ECompanyType::IT },
		{ FinanceTile, ECompanyType::Finance },
		{ SemiconductorTile, ECompanyType::Semiconductor },
		{ AutomobileTile, ECompanyType::Automobile },
		{ ElectronicsTile, ECompanyType::Electronics }
	};
	for (const TPair<UIndustryButtonWidget*, ECompanyType>& Def : TileDefs)
	{
		if (Def.Key)
		{
			Def.Key->SetCompanyType(Def.Value);
			Def.Key->SetIsSelectable(true);
			if (Def.Key == GameTile) Def.Key->SetIsInteractableWhenSelected(true);
			AllTiles.Add(Def.Key);
			Def.Key->OnClicked().AddUObject(this, &UBuildModalWidget::OnIndustryTileClicked, Def.Key);
		}
	}

	// 배타선택 = CommonButtonGroupBase (탭 정석). 초기화 전부 NativeConstruct.
	IndustryButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	IndustryButtonGroup->SetSelectionRequired(true);
	// 활성화 시 Game을 기본 선택하므로 선택 해제를 허용하지 않고 항상 한 산업을 유지한다.
	for (UIndustryButtonWidget* Tile : AllTiles)
	{
		IndustryButtonGroup->AddWidget(Tile);
	}
	IndustryButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UBuildModalWidget::OnIndustryTileSelected);
	// 활성화 전에는 캐시를 비우고, NativeOnActivated에서 버튼 그룹과 함께 Game으로 동기화한다.
	SelectedCompanyType = ECompanyType::None;

	// ListView 엔트리 생성 시 카드 선택 델리게이트 연결 (재활용 대비 AddUnique)
	if (BuildItemListView)
	{
		BuildItemListView->OnEntryWidgetGenerated().AddUObject(this, &UBuildModalWidget::HandleEntryGenerated);
		// 가로 스크롤바 항상 표시 — 스크롤바 공간을 상시 예약해 탭 전환(스크롤바 有無) 시 높이 점프를 막는다(있고/없고가 아니라 항상 있음).
		BuildItemListView->SetScrollbarVisibility(ESlateVisibility::Visible);
		// 카드 타일 크기(EntryWidth/EntryHeight)는 WBP TileView 가 단일 소스 — 여기서 코드로 덮지 않는다(WBP 디자이너에서 튜닝).
		if (UWidget* ListFade = GetWidgetFromName(TEXT("CatalogListFade"))) BindCatalogEdgeFade(this, BuildItemListView, ListFade);
		else UE_LOG(LogTemp, Warning, TEXT("[BuildModal] CatalogListFade 미배치 ― 카드 끝 흐림 없음 (WBP 확인)"));
	}

	if (UCloseButtonWidget* PanelCloseButton = ModalFrame ? ModalFrame->GetCloseButton() : nullptr)
	{
		PanelCloseButton->OnCloseClicked.AddDynamic(this, &UBuildModalWidget::OnCloseClicked);
	}

	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UBuildModalWidget::OnBackgroundClicked);
	}
}

void UBuildModalWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 부지 제약은 시드가 아니라 BeginBuild 가 켜는 bPlotBuildSession 이 쥔다 — "프리뷰 밑 소유 부지" 동적 판정(자유 배치 아님).
	// HQ 게이트 재평가 — 매 오픈마다(레벨업은 모달이 닫혀 있을 때만 일어남).
	RefreshIndustryLocks();

	// 자동선택: 첫 업종을 즉시 선택 → 카드가 바로 차서 "거대한 빈 카드 웰"(보이드) 제거.
	// 산업 선택은 필터형 내비게이션(상점 첫 탭처럼 즉시 표시가 정석)이고, 카드 목록은 산업 무관 동일이라 안전.
	// SelectedCompanyType 을 먼저 확정해 OnIndustryTileSelected 의 중복 refresh(bWasUnselected) 를 차단.
	if (AllTiles.Num() > 0 && AllTiles[0] && !AllTiles[0]->IsIndustryLocked())
	{
		SelectedCompanyType = AllTiles[0]->GetCompanyType();
		if (IndustryButtonGroup)
		{
			IndustryButtonGroup->SelectButtonAtIndex(0);
		}
		RefreshBuildingCards();
		if (EmptyStatePrompt)
		{
			EmptyStatePrompt->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		SelectedCompanyType = ECompanyType::None;
		if (BuildItemListView)
		{
			BuildItemListView->ClearListItems();
		}
		RefreshListChrome();
		if (EmptyStatePrompt)
		{
			EmptyStatePrompt->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void UBuildModalWidget::NativeOnDeactivated()
{
	if (BuildItemListView)
	{
		BuildItemListView->ClearListItems();
	}

	// 다음 위젯(배치 패널)을 연속 오픈하면 UI 모드 유지, 단독 닫힘이면 Normal 복원
	AMainMapPlayerController* PC = Cast<AMainMapPlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC && PC->GetCurrentInputMode() == EInputMode::UI)
	{
		PC->GoToNormalMode();
	}

	Super::NativeOnDeactivated();
}

void UBuildModalWidget::NativeDestruct()
{
	if (IndustryButtonGroup)
	{
		IndustryButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
		IndustryButtonGroup->RemoveAll();
	}

	if (BuildItemListView)
	{
		BuildItemListView->OnEntryWidgetGenerated().RemoveAll(this);
		BuildItemListView->OnListViewScrolled().RemoveAll(this);
	}

	if (UCloseButtonWidget* PanelCloseButton = ModalFrame ? ModalFrame->GetCloseButton() : nullptr) PanelCloseButton->OnCloseClicked.RemoveDynamic(this, &UBuildModalWidget::OnCloseClicked);
	if (BackgroundBtn) BackgroundBtn->OnClicked.RemoveDynamic(this, &UBuildModalWidget::OnBackgroundClicked);

	for (UIndustryButtonWidget* Tile : AllTiles)
	{
		if (Tile)
		{
			Tile->OnClicked().RemoveAll(this);
		}
	}

	Super::NativeDestruct();
}

void UBuildModalWidget::HandleEntryGenerated(UUserWidget& EntryWidget)
{
	if (UBuildEntityCardWidget* Card = Cast<UBuildEntityCardWidget>(&EntryWidget))
	{
		Card->OnCardSelected.AddUniqueDynamic(this, &UBuildModalWidget::OnBuildCardSelected);
	}
}

UWidget* UBuildModalWidget::GetBuildGuideCatalogWidget() const
{
	ECompanyType CompanyType = ECompanyType::None;
	if (!IsActivated() || !BuildItemListView || !TryGetSelectedCompanyType(CompanyType))
	{
		return nullptr;
	}
	const ESlateVisibility ListVisibility = BuildItemListView->GetVisibility();
	if (ListVisibility == ESlateVisibility::Hidden || ListVisibility == ESlateVisibility::Collapsed
		|| !BuildItemListView->GetIsEnabled())
	{
		return nullptr;
	}

	for (UUserWidget* Entry : BuildItemListView->GetDisplayedEntryWidgets())
	{
		UBuildEntityCardWidget* Card = Cast<UBuildEntityCardWidget>(Entry);
		if (Card && Card->GetVisibility() != ESlateVisibility::Hidden
			&& Card->GetVisibility() != ESlateVisibility::Collapsed
			&& Card->GetIsEnabled() && !Card->IsLocked())
		{
			return BuildItemListView;
		}
	}
	return nullptr;
}

UWidget* UBuildModalWidget::GetBuildGuideFirstCardWidget() const
{
	if (!GetBuildGuideCatalogWidget())
	{
		return nullptr;
	}
	UBuildEntityCardWidget* FirstCard = BuildItemListView->GetEntryWidgetFromItem<UBuildEntityCardWidget>(
		BuildItemListView->GetItemAt(0));
	return FirstCard && FirstCard->IsVisible() && FirstCard->GetIsEnabled() && !FirstCard->IsLocked()
		? FirstCard : nullptr;
}

UWidget* UBuildModalWidget::GetBuildGuideBestCardWidget(FText* OutName) const
{
	if (!GetBuildGuideCatalogWidget())
	{
		return nullptr;
	}
	const UResourceItemManager* Resources = GetGameInstance() ? GetGameInstance()->GetSubsystem<UResourceItemManager>() : nullptr;
	const UEntityCardData* Best = nullptr;
	for (UObject* Item : BuildItemListView->GetListItems())
	{
		const UEntityCardData* Data = Cast<UEntityCardData>(Item);
		EResourceType Missing = EResourceType::None;
		if (!Data || Data->bLevelLocked || !Resources || !Resources->CanAffordCosts(Data->BuildableInfo.ConstructionCosts, Missing))
		{
			continue;
		}
		// 같은 보너스면 앞 카드를 유지한다
		if (!Best || Data->IncomeBonusPercent > Best->IncomeBonusPercent)
		{
			Best = Data;
		}
	}
	UBuildEntityCardWidget* Card = Best
		? BuildItemListView->GetEntryWidgetFromItem<UBuildEntityCardWidget>(const_cast<UEntityCardData*>(Best)) : nullptr;
	if (!Card || !Card->IsVisible() || !Card->GetIsEnabled() || Card->IsLocked())
	{
		return GetBuildGuideFirstCardWidget();
	}
	if (OutName)
	{
		*OutName = FText::FromName(Best->BuildableInfo.Name);
	}
	return Card;
}

UWidget* UBuildModalWidget::GetBuildGuideIndustryWidget() const
{
	return IsActivated() && GameTile && GameTile->GetIsEnabled() && !GameTile->IsIndustryLocked()
		&& GameTile->GetVisibility() == ESlateVisibility::Visible ? GameTile : nullptr;
}

UWidget* UBuildModalWidget::GetIndustryTileWidget(ECompanyType Industry) const
{
	if (!IsActivated() || Industry == ECompanyType::None)
	{
		return nullptr;
	}
	for (UIndustryButtonWidget* Tile : AllTiles)
	{
		if (Tile && Tile->GetCompanyType() == Industry)
		{
			return Tile->GetIsEnabled() && !Tile->IsIndustryLocked()
				&& Tile->GetVisibility() == ESlateVisibility::Visible ? Tile : nullptr;
		}
	}
	return nullptr;
}

ECompanyType UBuildModalWidget::GetSelectedIndustry() const
{
	ECompanyType Selected = ECompanyType::None;
	return TryGetSelectedCompanyType(Selected) ? Selected : ECompanyType::None;
}

bool UBuildModalWidget::TryGetSelectedCompanyType(ECompanyType& OutCompanyType) const
{
	OutCompanyType = ECompanyType::None;
	if (!IndustryButtonGroup)
	{
		return false;
	}

	const UIndustryButtonWidget* SelectedTile =
		Cast<UIndustryButtonWidget>(IndustryButtonGroup->GetSelectedButtonBase());
	if (!SelectedTile || SelectedTile->IsIndustryLocked())
	{
		return false;
	}

	OutCompanyType = SelectedTile->GetCompanyType();
	return OutCompanyType != ECompanyType::None;
}

void UBuildModalWidget::OnIndustryTileSelected(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	if (UIndustryButtonWidget* Tile = Cast<UIndustryButtonWidget>(SelectedButton))
	{
		const bool bWasUnselected = (SelectedCompanyType == ECompanyType::None);
		SelectedCompanyType = Tile->GetCompanyType();

		// 카드 목록은 산업과 무관(동일 목록)이라 미선택 → 첫 선택 때만 1회 생성한다.
		// 산업 간 전환은 SelectedCompanyType(배치 시 베이크)만 갱신 — 리스트 재생성/스크롤 리셋/깜빡임 방지.
		if (bWasUnselected)
		{
			RefreshBuildingCards();
			if (EmptyStatePrompt)
			{
				EmptyStatePrompt->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void UBuildModalWidget::OnBuildCardSelected(const FBuildableCardTable& SelectedInfo)
{
	ECompanyType CompanyTypeForBuild = SelectedCompanyType;
	if (!TryGetSelectedCompanyType(CompanyTypeForBuild))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BuildModalWidget] Build blocked: no valid industry is selected."));
		return;
	}

	UCGGameInstance* GameInstance = Cast<UCGGameInstance>(GetGameInstance());
	if (!GameInstance)
	{
		return;
	}

	UUIManagerSubsystem* UIManager = GameInstance->GetSubsystem<UUIManagerSubsystem>();

	// 건물 수 전역 상한 폐지(2026-08-14) — 확장 병목은 부지 인수 Money + 건설 Brick 이 맡는다.
	// 남은 건설 제약은 아래 부지 수용량(로컬)뿐이다.

	// 부지 수용량 게이트 — 배치는 "프리뷰 밑 소유 부지"로 동적 판정되므로(PlacementHandler), 카드 선택 시점엔
	// 특정 부지로 막지 않는다. 대신 "소유 부지 중 수용량(BuildingCapacity)에 여유가 있는 곳이 하나라도 있나"만 검사:
	//   - 소유 부지 0개  → "먼저 부지를 인수하세요"
	//   - 전부 가득     → "모든 부지가 가득 찼습니다"
	// 그 외엔 통과시키고, 실제 배치 가능 여부는 placement 의 bPlotPlacementValid(빨강/초록)가 담당한다.
	// (진입 경로와 무관하게 이 전역 검사로만 게이트됨 — 특정 부지로 막지 않음.)
	{
		USpawnManager* SpawnMgr = GetWorld()->GetSubsystem<USpawnManager>();
		UEntityManager* EntityMgr = GetWorld()->GetSubsystem<UEntityManager>();

		TArray<FName> OwnedPlotIds;
		if (SpawnMgr)
		{
			SpawnMgr->GatherOwnedPlotIds(OwnedPlotIds);
		}

		if (OwnedPlotIds.Num() == 0)
		{
			if (UIManager)
			{
				UIManager->ShowNotification(
					NSLOCTEXT("Notification", "NoOwnedPlot", "먼저 부지를 인수하세요"),
					3.0f, ENotificationType::Warning);
			}
			return;
		}

		bool bAnyPlotHasRoom = false;
		if (SpawnMgr && EntityMgr)
		{
			for (const FName& OwnedId : OwnedPlotIds)
			{
				// 수용량 권위는 부지 액터다 — 레벨 배치형은 박스 크기에서 산출하므로 DT 값과 다를 수 있다.
				const ACityPlotActor* OwnedPlot = SpawnMgr ? SpawnMgr->GetSpawnedPlotById(OwnedId) : nullptr;
				if (!OwnedPlot)
				{
					continue;
				}
				if (EntityMgr->HasFreeCapacityOnPlot(OwnedId, OwnedPlot->GetBuildingCapacity()))
				{
					bAnyPlotHasRoom = true;
					break;
				}
			}
		}

		if (!bAnyPlotHasRoom)
		{
			if (UIManager)
			{
				UIManager->ShowNotification(
					NSLOCTEXT("Notification", "AllPlotsFull", "모든 부지가 가득 찼습니다"),
					3.0f, ENotificationType::Warning);
			}
			return;
		}
	}

	// 자재 부족 — 카드 누르는 시점에 차단 (배치 모드 진입 자체를 막는다). 자원명은 DT_Resource 단일 진실.
	if (UResourceItemManager* ResMgr = GameInstance->GetSubsystem<UResourceItemManager>())
	{
		EResourceType MissingType = EResourceType::None;
		if (!ResMgr->CanAffordCosts(SelectedInfo.ConstructionCosts, MissingType))
		{
			if (UIManager)
			{
				bool bNameOk = false;
				const FResourceInfo ResInfo = TableManager ? TableManager->GetResourceInfo(MissingType, bNameOk) : FResourceInfo();
				const FText Msg = FText::Format(
					NSLOCTEXT("Notification", "BuildNotEnoughResource", "{0}|hpp(이,가) 부족합니다"), ResInfo.DisplayName);
				UIManager->ShowNotification(Msg, 3.0f, ENotificationType::Failed);
			}
			return;
		}
	}

	UUIBase* UIBase = UIManager ? UIManager->GetUIBase() : nullptr;
	TSubclassOf<UUserWidget> PlacementClass = TableManager ? TableManager->GetWidgetClass(EWidgetType::BuildPlacementPanel) : nullptr;
	if (!UIBase || !PlacementClass || !PlacementClass->IsChildOf(UBuildPlacementPanelWidget::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildModalWidget] BuildPlacementPanel is unavailable."));
		return;
	}

	// 배치 시작 — 산업을 함께 넘겨 배치 확정 시 베이크
	const AMainMapPlayerController* PlayerController = Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController());
	APlayerCamera* CameraPawn = PlayerController ? Cast<APlayerCamera>(PlayerController->GetPawn()) : nullptr;
	if (!CameraPawn)
	{
		return;
	}
	CameraPawn->BeginBuild(SelectedInfo, CompanyTypeForBuild);

	// 모달 닫고 배치 패널 push (프롬프트 스택이라 DeactivateWidget 으로 닫힘)
	DeactivateWidget();

	UBuildPlacementPanelWidget* PlacementWidget = Cast<UBuildPlacementPanelWidget>(
		UIBase->PushBottomClass(PlacementClass.Get()));
	if (!PlacementWidget)
	{
		CameraPawn->EndBuild();
		UE_LOG(LogTemp, Error, TEXT("[BuildModalWidget] Failed to create BuildPlacementPanelWidget!"));
		return;
	}

	PlacementWidget->SetBuildableInfo(SelectedInfo);
}

void UBuildModalWidget::ApplyBuildCostProgression(TArray<FBuildableCardTable>& InOutCards) const
{
	UGameInstance* GI = GetGameInstance();
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	const USaveGame_GameData* SaveData = SaveMgr ? SaveMgr->GetCurrentSaveData() : nullptr;
	if (!SaveData)
	{
		return;
	}

	// 세이브의 건물 배열이 정본 — EntityManager 는 배치 프리뷰 액터까지 세어 첫 채부터 값이 튄다.
	const int32 OwnedBuildings = SaveData->GameData.Buildings.Num();
	ApplyBuildCostProgression(InOutCards, OwnedBuildings);
}

void UBuildModalWidget::ApplyBuildCostProgression(TArray<FBuildableCardTable>& InOutCards, int32 OwnedBuildings)
{
	if (OwnedBuildings <= 0)
	{
		return;
	}

	const float Multiplier = FMath::Pow(BuildCostGrowthPerBuilding, static_cast<float>(OwnedBuildings));
	for (FBuildableCardTable& Card : InOutCards)
	{
		for (FConstructionCost& Cost : Card.ConstructionCosts)
		{
			Cost.Cost = FMath::Max<int64>(1, FMath::RoundToInt64(static_cast<double>(Cost.Cost) * Multiplier));
		}
	}
}

void UBuildModalWidget::RefreshBuildingCards()
{
	if (!BuildItemListView || !TableManager)
	{
		return;
	}

	BuildItemListView->ClearListItems();

	TArray<FBuildableCardTable> BuildableInfos = TableManager->GetBuildableInfos();

	// ⚠ 누진은 여기서만 건다. TableManager 의 단건 GetBuildableInfo 는 이미 지은 건물의 카드 표시에도
	//    쓰이므로 거기서 곱하면 소유 건물의 원가가 부풀어 보인다.
	ApplyBuildCostProgression(BuildableInfos);

	// 플레이어 본사 레벨 (해금 판정용)
	int32 PlayerHQLevel = 1;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USaveLoadManager* SaveMgr = GI->GetSubsystem<USaveLoadManager>())
		{
			PlayerHQLevel = SaveMgr->GetHQLevel();
		}
	}

	// 1차 키 = 해금 여부(해금 먼저) → 2차 footprint 칸수(W*D) 오름차순 → 3차 건물 번호 숫자 오름차순(B1<B2<B10)
	BuildableInfos.Sort([this, PlayerHQLevel](const FBuildableCardTable& A, const FBuildableCardTable& B)
	{
		bool bOkA = false;
		bool bOkB = false;
		const FBuildingData BDataA = TableManager->GetBuildingData(A.RowName, bOkA);
		const FBuildingData BDataB = TableManager->GetBuildingData(B.RowName, bOkB);

		// 1차 키: 해금 여부 — 액션 가능한(해금) 카드를 앞으로, 잠긴 카드는 뒤로(여전히 보임)
		const int32 ReqLvA = bOkA ? BDataA.RequiredHQLevel : 0;
		const int32 ReqLvB = bOkB ? BDataB.RequiredHQLevel : 0;
		const bool bLockedA = (ReqLvA > 0) && (PlayerHQLevel < ReqLvA);
		const bool bLockedB = (ReqLvB > 0) && (PlayerHQLevel < ReqLvB);
		if (bLockedA != bLockedB) return !bLockedA;

		const int32 CellsA = bOkA ? (BDataA.FootprintWidthCells * BDataA.FootprintDepthCells) : 1;
		const int32 CellsB = bOkB ? (BDataB.FootprintWidthCells * BDataB.FootprintDepthCells) : 1;
		if (CellsA != CellsB) return CellsA < CellsB;

		// 2차 키: RowName(예: B1, B2, B10)에서 숫자 추출하여 숫자 오름차순
		FString NameA = A.RowName.ToString().Replace(TEXT("B"), TEXT("")).Replace(TEXT("R"), TEXT("")).Replace(TEXT("F"), TEXT(""));
		FString NameB = B.RowName.ToString().Replace(TEXT("B"), TEXT("")).Replace(TEXT("R"), TEXT("")).Replace(TEXT("F"), TEXT(""));
		const int32 NumA = FCString::Atoi(*NameA);
		const int32 NumB = FCString::Atoi(*NameB);
		if (NumA != NumB) return NumA < NumB;

		return A.RowName.LexicalLess(B.RowName);
	});

	for (const FBuildableCardTable& BuildableInfo : BuildableInfos)
	{
		// 본사 레벨 잠금 판정
		bool bBuildingDataOk = false;
		const FBuildingData BData = TableManager->GetBuildingData(BuildableInfo.RowName, bBuildingDataOk);
		const int32 ReqLv = bBuildingDataOk ? BData.RequiredHQLevel : 0;
		const bool bLocked = (ReqLv > 0) && (PlayerHQLevel < ReqLv);

		UEntityCardData* CardData = NewObject<UEntityCardData>(this);
		CardData->BuildableInfo = BuildableInfo;
		CardData->Rarity = BuildableInfo.Rarity;
		CardData->bLevelLocked = bLocked;

		// 가치존(크기·인원) 페이로드 — 빌딩 데이터에서 채움(없으면 기본값). 신축이라 증축 0층 기준.
		CardData->FootprintWidthCells = bBuildingDataOk ? BData.FootprintWidthCells : 1;
		CardData->FootprintDepthCells = bBuildingDataOk ? BData.FootprintDepthCells : 1;
		CardData->NewBuildEmployees = bBuildingDataOk ? BData.GetEmployeeCapacity(0) : 0;
		CardData->IncomeBonusPercent = bBuildingDataOk ? BData.IncomeBonusPercent : 0.f;

		if (bLocked)
		{
			CardData->LevelLockReason = FText::Format(
				NSLOCTEXT("BuildModal", "NeedHQLevel", "본사 레벨 {0} 필요"),
				FText::AsNumber(ReqLv));
		}

		BuildItemListView->AddItem(CardData);
	}
	RefreshListChrome();
}

void UBuildModalWidget::RefreshListChrome()
{
	const int32 Count = BuildItemListView ? BuildItemListView->GetNumItems() : 0;
	if (CatalogCount) CatalogCount->SetText(FText::Format(NSLOCTEXT("BuildModal", "CatalogCount", "{0}개"), FText::AsNumber(Count)));
	ScheduleCatalogEdgeFade(this, BuildItemListView, GetWidgetFromName(TEXT("CatalogListFade")));
}

void UBuildModalWidget::OnCloseClicked()
{
	CloseModal();
}

void UBuildModalWidget::OnBackgroundClicked()
{
	CloseModal();
}

void UBuildModalWidget::CloseModal()
{
	DeactivateWidget();
}

void UBuildModalWidget::RefreshIndustryLocks()
{
	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	if (!SaveMgr) return;

	for (UIndustryButtonWidget* Tile : AllTiles)
	{
		if (!Tile) continue;

		const ECompanyType Type = Tile->GetCompanyType();
		const EIndustryTileState State = DecideIndustryTileState(
			SaveMgr->IsIndustryAvailable(Type), SaveMgr->IsIndustryUnlocked(Type));

		switch (State)
		{
		case EIndustryTileState::Hidden:
			Tile->SetVisibility(ESlateVisibility::Collapsed);
			// Collapsed 는 클릭만 막는다 — 그룹의 SelectButtonAtIndex 는 GetIsEnabled() 만 보므로
			// (CommonButtonGroupBase.cpp:156) 프로그램 경로는 SetIsEnabled(false) 로 닫는다.
			Tile->SetIsEnabled(false);
			break;
		case EIndustryTileState::Locked:
			Tile->SetIsEnabled(true);
			Tile->SetVisibility(ESlateVisibility::Visible);
			Tile->SetIndustryLocked(true);
			break;
		case EIndustryTileState::Unlocked:
			Tile->SetIsEnabled(true);
			Tile->SetVisibility(ESlateVisibility::Visible);
			Tile->SetIndustryLocked(false);
			break;
		}
	}
}

void UBuildModalWidget::OnIndustryTileClicked(UIndustryButtonWidget* Tile)
{
	if (!Tile || !IsActivated()) return;
	if (!Tile->IsIndustryLocked())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
			{
				MissionMgr->NotifyBuildIndustrySelected(this, Tile->GetCompanyType());
			}
		}
		return;
	}

	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!SaveMgr || !UIMgr) return;

	const int32 ReqLevel = SaveMgr->GetIndustryRequiredHQLevel(Tile->GetCompanyType());
	UIMgr->ShowNotification(
		FText::Format(NSLOCTEXT("BuildModal", "IndustryLocked", "본사 Lv.{0} 달성 시 해금됩니다"), ReqLevel),
		3.0f, ENotificationType::Failed);
}
