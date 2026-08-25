#include "UI/Panel/BuildModalWidget.h"

#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "Manager/EntityManager.h"
#include "Manager/SpawnManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/ResourceItemManager.h"
#include "Table/ResourceInfo.h"
#include "Enum/ResourceType.h"

#include "UI/UIBase.h"
#include "UI/Element/Buttons/IndustryButtonWidget.h"
#include "UI/Element/Buttons/TabButtonWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Cards/BuildEntityCardWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Panel/BuildPlacementPanelWidget.h"

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

	// 카드 [일반]/[특수] 탭 그룹 — 두 탭이 모두 WBP 에 존재할 때만 셋업(미배치면 필터/탭 없이 전체 목록).
	// 탭 래퍼(UTabButtonWidget)는 UUserWidget 이라 GetButton() 으로 내부 CommonButton 을 꺼내 등록(ManagePanel 패턴).
	CurrentCardTabIndex = 0;
	if (NormalCardTab && SpecialCardTab)
	{
		CardTabButtonGroup = NewObject<UCommonButtonGroupBase>(this);
		CardTabButtonGroup->SetSelectionRequired(true); // 항상 하나는 선택
		auto RegisterCardTab = [this](UButtonWidget* Tab)
		{
			if (!Tab) return;
			// UButtonWidget = CommonButtonBase 파생 → 그룹에 직접 등록(GetButton 래퍼 불필요, SortToggle 패턴)
			CardTabButtonGroup->AddWidget(Tab);
			Tab->SetIsSelectable(true);
		};
		RegisterCardTab(NormalCardTab);   // index 0 = 일반
		RegisterCardTab(SpecialCardTab);  // index 1 = 특수
		CardTabButtonGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UBuildModalWidget::OnCardTabSelectionChanged);
		CardTabButtonGroup->SelectButtonAtIndex(0);
	}

	// ListView 엔트리 생성 시 카드 선택 델리게이트 연결 (재활용 대비 AddUnique)
	if (BuildItemListView)
	{
		BuildItemListView->OnEntryWidgetGenerated().AddUObject(this, &UBuildModalWidget::HandleEntryGenerated);
		// 가로 스크롤바 항상 표시 — 스크롤바 공간을 상시 예약해 탭 전환(스크롤바 有無) 시 높이 점프를 막는다(있고/없고가 아니라 항상 있음).
		BuildItemListView->SetScrollbarVisibility(ESlateVisibility::Visible);
		// 카드 타일 크기(EntryWidth/EntryHeight)는 WBP TileView 가 단일 소스 — 여기서 코드로 덮지 않는다(WBP 디자이너에서 튜닝).
	}

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UBuildModalWidget::OnCloseClicked);
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
		if (CardTabButtonGroup)
		{
			CardTabButtonGroup->SelectButtonAtIndex(0); // 항상 [일반] 탭으로 연다
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

	if (CardTabButtonGroup)
	{
		CardTabButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
		CardTabButtonGroup->RemoveAll();
	}

	if (BuildItemListView)
	{
		BuildItemListView->OnEntryWidgetGenerated().RemoveAll(this);
	}

	if (UIE_CloseButton) UIE_CloseButton->OnCloseClicked.RemoveDynamic(this, &UBuildModalWidget::OnCloseClicked);
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

void UBuildModalWidget::OnCardTabSelectionChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	CurrentCardTabIndex = ButtonIndex;

	// 특수(1) 탭 = 모뉴먼트(키스톤). 업종은 모뉴먼트엔 무시되지만, 산업 타일을 Collapsed 로 숨기면
	// 위 영역이 줄어 카드 웰이 위로 점프하는 UX 단절이 생긴다 → 탭 전환과 무관하게 항상 표시해 레이아웃 고정.
	const bool bSpecialTab = (ButtonIndex == 1);
	for (UIndustryButtonWidget* Tile : AllTiles)
	{
		if (Tile)
		{
			// 특수: 업종은 모뉴먼트에 무관 → 숨기지 말고(레이아웃 점프 방지) "보이되 비활성/흐림"으로.
			// 선택 상태는 유지(일반 복귀 시 복원), 클릭은 차단, 시각적으로 "여기선 안 쓰임" 신호. 일반은 활성/선명.
			Tile->SetVisibility(ESlateVisibility::Visible);
			Tile->SetIsEnabled(!bSpecialTab);
			Tile->SetRenderOpacity(bSpecialTab ? 0.45f : 1.0f);
		}
	}
	if (!bSpecialTab)
	{
		// 위 루프가 전 타일을 RenderOpacity 1.0 으로 되돌리므로, 일반 탭 복귀 시 HQ 잠금 상태를 재적용해
		// 잠긴 타일의 회색 티저를 복원(그대로 두면 특수 탭 왕복 후 잠긴 산업이 풀린 것처럼 보임).
		RefreshIndustryLocks();
	}
	// 카드 표시 조건: 특수(업종 무관) 또는 일반+업종선택됨. 그 외(일반+미선택)엔 카드 비우고 안내만 표시.
	// → 카드와 "산업을 선택하세요" 안내가 절대 동시에 뜨지 않게(탭 전환 시 겹침 버그 방지).
	const bool bShowCards = bSpecialTab || (SelectedCompanyType != ECompanyType::None);
	if (bShowCards)
	{
		RefreshBuildingCards();
		if (EmptyStatePrompt)
		{
			EmptyStatePrompt->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		if (BuildItemListView)
		{
			BuildItemListView->ClearListItems();
		}
		if (EmptyStatePrompt)
		{
			EmptyStatePrompt->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void UBuildModalWidget::OnBuildCardSelected(const FBuildableCardTable& SelectedInfo)
{
	// 선택 빌딩이 키스톤(모뉴먼트)인지 — 키스톤은 업종 무관이라 산업 선택 게이트를 우회한다(탭 유무와 독립).
	bool bSelectedKeystone = false;
	if (UTableManagerSubsystem* KTMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr)
	{
		bool bKOk = false;
		const FBuildingData KData = KTMgr->GetBuildingData(SelectedInfo.RowName, bKOk);
		bSelectedKeystone = bKOk && KData.KeystoneAura.bIsKeystone;
	}
	// 일반 빌딩은 현재 버튼 그룹 선택을 검증하고, 키스톤은 기존 산업 선택 게이트를 우회한다.
	ECompanyType CompanyTypeForBuild = SelectedCompanyType;
	if (!bSelectedKeystone && !TryGetSelectedCompanyType(CompanyTypeForBuild))
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
		if (TableManager && EntityMgr)
		{
			for (const FName& OwnedId : OwnedPlotIds)
			{
				bool bPlotOk = false;
				const FCityPlotData PlotData = TableManager->GetCityPlotData(OwnedId, bPlotOk);
				if (bPlotOk &&
					EntityMgr->HasFreeCapacityOnPlot(OwnedId, PlotData.BuildingCapacity))
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
					NSLOCTEXT("Notification", "BuildNotEnoughResource", "{0}이(가) 부족합니다"), ResInfo.DisplayName);
				UIManager->ShowNotification(Msg, 3.0f, ENotificationType::Failed);
			}
			return;
		}
	}

	// 배치 시작 — 산업을 함께 넘겨 배치 확정 시 베이크
	const AMainMapPlayerController* PlayerController = Cast<AMainMapPlayerController>(GameInstance->GetCurrentPlayerController());
	APlayerCamera* CameraPawn = PlayerController ? Cast<APlayerCamera>(PlayerController->GetPawn()) : nullptr;
	if (CameraPawn)
	{
		CameraPawn->BeginBuild(SelectedInfo, CompanyTypeForBuild);
	}

	// 모달 닫고 배치 패널 push (프롬프트 스택이라 DeactivateWidget 으로 닫힘)
	DeactivateWidget();

	if (!UIManager)
	{
		return;
	}

	TSubclassOf<UUserWidget> PlacementClass = TableManager ? TableManager->GetWidgetClass(EWidgetType::BuildPlacementPanel) : nullptr;
	if (!PlacementClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildModalWidget] BuildPlacementPanel widget class not found!"));
		return;
	}

	UBuildPlacementPanelWidget* PlacementWidget = Cast<UBuildPlacementPanelWidget>(
		UIManager->GetUIBase()->PushBottomClass(PlacementClass.Get()));
	if (!PlacementWidget)
	{
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

	// 키스톤 필터 — 두 탭 버튼이 모두 존재(WBP 업데이트됨)할 때만 적용. 미배치면 필터 없이 전체 표시(현 동작 유지,
	// 특수 4종이 탭 없이 사라지지 않게). 일반 탭(0)=비키스톤만, 특수 탭(1)=키스톤만. 식별은 데이터 플래그만 사용(B번호 하드코딩 금지).
	if (NormalCardTab && SpecialCardTab)
	{
		const int32 CardTabIndex = CurrentCardTabIndex;
		BuildableInfos.RemoveAll([this, CardTabIndex](const FBuildableCardTable& Card)
		{
			bool bKeystoneOk = false;
			const FBuildingData KeyData = TableManager->GetBuildingData(Card.RowName, bKeystoneOk);
			const bool bKeystone = bKeystoneOk && KeyData.KeystoneAura.bIsKeystone;
			return (CardTabIndex == 0) ? bKeystone : !bKeystone;
		});
	}

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

		if (bLocked)
		{
			CardData->LevelLockReason = FText::Format(
				NSLOCTEXT("BuildModal", "NeedHQLevel", "본사 레벨 {0} 필요"),
				FText::AsNumber(ReqLv));
		}

		BuildItemListView->AddItem(CardData);
	}
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
		if (Tile)
		{
			Tile->SetIndustryLocked(!SaveMgr->IsIndustryUnlocked(Tile->GetCompanyType()));
		}
	}
}

void UBuildModalWidget::OnIndustryTileClicked(UIndustryButtonWidget* Tile)
{
	if (!Tile || !Tile->IsIndustryLocked()) return;

	USaveLoadManager* SaveMgr = GetGameInstance()->GetSubsystem<USaveLoadManager>();
	UUIManagerSubsystem* UIMgr = GetGameInstance()->GetSubsystem<UUIManagerSubsystem>();
	if (!SaveMgr || !UIMgr) return;

	const int32 ReqLevel = SaveMgr->GetIndustryRequiredHQLevel(Tile->GetCompanyType());
	UIMgr->ShowNotification(
		FText::Format(NSLOCTEXT("BuildModal", "IndustryLocked", "본사 Lv.{0} 달성 시 해금됩니다"), ReqLevel),
		3.0f, ENotificationType::Failed);
}
