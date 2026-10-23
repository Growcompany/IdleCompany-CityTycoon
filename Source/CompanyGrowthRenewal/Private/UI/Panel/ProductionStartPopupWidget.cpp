// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/ProductionStartPopupWidget.h"
#include "UI/Element/Common/ModalFrameWidget.h"
#include "UI/Element/Cards/ProductionOrderRowWidget.h"
#include "UI/Element/Common/MaterialReqRowWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Panel/CountryDrawerModel.h"
#include "Global/KoreanJosa.h"
#include "Blueprint/WidgetTree.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "CommonButtonBase.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/WorldFactoryManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/WorldMapManager.h"
#include "Manager/OverseasActions.h"
#include "Data/WorldMapTypes.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "Global/GlobalUtilFunctions.h"
#include "Table/CountryInfoTable.h"
#include "Table/ProjectDataTable.h"
#include "Table/ProductRecipeTable.h"
#include "Table/ResourceInfo.h"
#include "Enum/WidgetType.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UProductionStartPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UCloseButtonWidget* PanelCloseButton = ModalFrame ? ModalFrame->GetCloseButton() : nullptr)    PanelCloseButton->OnCloseClicked.AddDynamic(this, &UProductionStartPopupWidget::HandleClose);
	if (BackgroundBtn)  BackgroundBtn->OnClicked.AddDynamic(this, &UProductionStartPopupWidget::HandleBackgroundClicked);

	if (MinusButton)    MinusButton->OnClicked().AddUObject(this, &UProductionStartPopupWidget::HandleMinus);
	if (PlusButton)     PlusButton->OnClicked().AddUObject(this, &UProductionStartPopupWidget::HandlePlus);
	if (MaxJumpButton)  MaxJumpButton->OnClicked().AddUObject(this, &UProductionStartPopupWidget::HandleMaxJump);
	if (AcceptButton)   AcceptButton->OnClicked().AddUObject(this, &UProductionStartPopupWidget::HandleAccept);
	if (BulkBtn_x1)     BulkBtn_x1->OnClicked().AddUObject(this, &UProductionStartPopupWidget::HandleBulkX1);
	if (BulkBtn_x10)    BulkBtn_x10->OnClicked().AddUObject(this, &UProductionStartPopupWidget::HandleBulkX10);
	if (BulkBtn_x50)    BulkBtn_x50->OnClicked().AddUObject(this, &UProductionStartPopupWidget::HandleBulkX50);
	RefreshBulkVisuals();

	EnsureManagers();

	if (ResourceMgr)
	{
		ResourceMgr->OnResourceChanged.AddUObject(this, &UProductionStartPopupWidget::HandleResourceChanged);
	}
	if (OrderMgr)
	{
		OrderMgr->OnOrderCreated.AddDynamic(this, &UProductionStartPopupWidget::HandleOrderCreated);
		OrderMgr->OnOrderConsumed.AddDynamic(this, &UProductionStartPopupWidget::HandleOrderConsumed);
		OrderMgr->OnOrderDepleted.AddDynamic(this, &UProductionStartPopupWidget::HandleOrderDepleted);
	}
	if (FactoryMgr)
	{
		FactoryMgr->OnLineStarted.AddDynamic(this, &UProductionStartPopupWidget::HandleLineStarted);
		FactoryMgr->OnLineCompleted.AddDynamic(this, &UProductionStartPopupWidget::HandleLineCompleted);
		FactoryMgr->OnLineClaimed.AddDynamic(this, &UProductionStartPopupWidget::HandleLineClaimed);
	}
	if (WorldMapMgr)
	{
		// 칩 재고도 상한을 정하므로 자원과 같은 갱신이 필요하다 — 구독이 없으면 열어 둔 채로 레일 힌트가 낡는다
		WorldMapMgr->OnProductAdded.AddDynamic(this, &UProductionStartPopupWidget::HandleProductInventoryChanged);
		WorldMapMgr->OnProductConsumed.AddDynamic(this, &UProductionStartPopupWidget::HandleProductInventoryChanged);
	}

	// InitializeForCountry 는 화면에 붙기 전에 불린다 ― 비용 버튼 PreConstruct 가 칩·라벨을 기본값으로 덮으므로 다시 입힌다
	RefreshDynamic();
}

void UProductionStartPopupWidget::EnsureManagers()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	if (!OrderMgr)    OrderMgr = GI->GetSubsystem<UProductionOrderManager>();
	if (!ResourceMgr) ResourceMgr = GI->GetSubsystem<UResourceItemManager>();
	if (!FactoryMgr)  FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>();
	if (!WorldMapMgr) WorldMapMgr = GI->GetSubsystem<UWorldMapManager>();
}

void UProductionStartPopupWidget::NativeDestruct()
{
	if (UWorld* WidgetWorld = GetWorld()) WidgetWorld->GetTimerManager().ClearTimer(OrderRailRefreshTimerHandle);
	OrderRailRefreshTimerHandle.Invalidate();
	if (UCloseButtonWidget* PanelCloseButton = ModalFrame ? ModalFrame->GetCloseButton() : nullptr)    PanelCloseButton->OnCloseClicked.RemoveDynamic(this, &UProductionStartPopupWidget::HandleClose);
	if (BackgroundBtn)  BackgroundBtn->OnClicked.RemoveDynamic(this, &UProductionStartPopupWidget::HandleBackgroundClicked);

	if (MinusButton)    MinusButton->OnClicked().RemoveAll(this);
	if (PlusButton)     PlusButton->OnClicked().RemoveAll(this);
	if (MaxJumpButton)  MaxJumpButton->OnClicked().RemoveAll(this);
	if (AcceptButton)   AcceptButton->OnClicked().RemoveAll(this);
	if (BulkBtn_x1)     BulkBtn_x1->OnClicked().RemoveAll(this);
	if (BulkBtn_x10)    BulkBtn_x10->OnClicked().RemoveAll(this);
	if (BulkBtn_x50)    BulkBtn_x50->OnClicked().RemoveAll(this);

	if (ResourceMgr)
	{
		ResourceMgr->OnResourceChanged.RemoveAll(this);
		ResourceMgr = nullptr;
	}
	if (FactoryMgr)
	{
		FactoryMgr->OnLineStarted.RemoveDynamic(this, &UProductionStartPopupWidget::HandleLineStarted);
		FactoryMgr->OnLineCompleted.RemoveDynamic(this, &UProductionStartPopupWidget::HandleLineCompleted);
		FactoryMgr->OnLineClaimed.RemoveDynamic(this, &UProductionStartPopupWidget::HandleLineClaimed);
		FactoryMgr = nullptr;
	}
	if (WorldMapMgr)
	{
		WorldMapMgr->OnProductAdded.RemoveDynamic(this, &UProductionStartPopupWidget::HandleProductInventoryChanged);
		WorldMapMgr->OnProductConsumed.RemoveDynamic(this, &UProductionStartPopupWidget::HandleProductInventoryChanged);
		WorldMapMgr = nullptr;
	}
	if (OrderMgr)
	{
		OrderMgr->OnOrderCreated.RemoveDynamic(this, &UProductionStartPopupWidget::HandleOrderCreated);
		OrderMgr->OnOrderConsumed.RemoveDynamic(this, &UProductionStartPopupWidget::HandleOrderConsumed);
		OrderMgr->OnOrderDepleted.RemoveDynamic(this, &UProductionStartPopupWidget::HandleOrderDepleted);
		OrderMgr = nullptr;
	}

	SpawnedRows.Reset();
	SpawnedMatRows.Reset();
	SpawnedProductRow = nullptr;
	Super::NativeDestruct();
}

void UProductionStartPopupWidget::InitializeForCountry(ECountryType InCountry, int32 PreferredOrderID)
{
	// 호출자가 AddToViewport 보다 먼저 부른다 — NativeConstruct 를 기다리면 매니저가 전부 null
	EnsureManagers();

	Country = InCountry;
	InitialPreferredOrderID = PreferredOrderID;
	SelectedOrderID = 0;
	CurrentQuantity = 1;
	RefreshHeader();
	RebuildOrderRail();
}

// ══════════════ 좌 레일 ══════════════

void UProductionStartPopupWidget::RebuildOrderRail()
{
	if (!OrderRailBox) return;

	OrderRailBox->ClearChildren();
	SpawnedRows.Reset();

	EnsureManagers();
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	// 조용히 빠져나가면 "빈 팝업 + 로그 0줄" 이 되어 진단이 막힌다 (2026-07-29 실측)
	if (!TableMgr || !OrderMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ProductionStartPopup] 매니저 없음 (Table=%d Order=%d) — 목록 비움"),
			TableMgr != nullptr, OrderMgr != nullptr);
		return;
	}

	bool bInfoOk = false;
	const FCountryInfoTable Info = TableMgr->GetCountryInfo(Country, bInfoOk);
	if (!bInfoOk)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ProductionStartPopup] DT_CountryInfo 행 없음 — Country=%d"),
			static_cast<int32>(Country));
		return;
	}

	TSubclassOf<UUserWidget> RowClass = TableMgr->GetWidgetClass(EWidgetType::ProductionOrderRow);
	if (!RowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ProductionStartPopup] EWidgetType::ProductionOrderRow 미등록"));
		return;
	}

	int32 FirstOrderID = 0;
	for (const FProductionOrder& Order : OrderMgr->GetAllOrders())
	{
		if (Order.IsConsumed()) continue;
		if (Info.SupportedIndustries.Num() > 0 && !Info.SupportedIndustries.Contains(Order.CompanyType))
		{
			continue;
		}

		UProductionOrderRowWidget* Row = CreateWidget<UProductionOrderRowWidget>(this, RowClass);
		if (!Row) continue;

		EResourceType Bottleneck = EResourceType::None;
		bool bSupplyBound = false;
		const int32 MaxProducible = OrderMgr->GetMaxProducible(Order, Bottleneck, bSupplyBound);

		// 자원 이름이 비는 것 = 제품 입력(칩)이 상한을 정했다는 신호(GetMaxProducible 규약).
		// 그 때만 이름을 조회한다 — 평시엔 주문서마다 DT 두 번을 더 타게 된다.
		const FText ProductInputName = (bSupplyBound && Bottleneck == EResourceType::None)
			? GetProductInputName(Order) : FText::GetEmpty();
		bool bRecipeOk = false;
		const FProductRecipeTable Recipe = TableMgr->GetProductRecipe(Order.CompanyType, Order.ProjectIndex, bRecipeOk);
		const float EffectiveTimePerUnitSec = GetEffectiveProductionTimeSec(
			bRecipeOk ? Recipe.ProductionTimeSec : 0.0f);

		Row->SetOrderData(Order, MaxProducible, Bottleneck, bSupplyBound, ProductInputName,
			EffectiveTimePerUnitSec);
		Row->OnOrderRowClicked.BindUObject(this, &UProductionStartPopupWidget::HandleOrderRowClicked);

		OrderRailBox->AddChild(Row);
		SpawnedRows.Add(Row);

		if (FirstOrderID == 0) FirstOrderID = Order.OrderID;
	}

	if (OrderCountText)
	{
		// 비는 라인은 활성 라인 뒤에 번호가 붙는다(서랍 빈 줄과 같은 번호) ― 라인이 다 찼으면 번호를 빼고 공장만
		const int32 Active = FactoryMgr ? FactoryMgr->GetActiveLineCount(Country) : 0;
		const int32 MaxLines = FactoryMgr ? FactoryMgr->GetMaxLines(Country) : 0;
		OrderCountText->SetText(Active < MaxLines
			? FText::Format(NSLOCTEXT("ProductionStartPopup", "OrderCountLine", "주문 {0}건 · {1} 공장 {2}번 라인"),
				FText::AsNumber(SpawnedRows.Num()), Info.DisplayName, FText::AsNumber(Active + 1))
			: FText::Format(NSLOCTEXT("ProductionStartPopup", "OrderCountFactory", "주문 {0}건 · {1} 공장"),
				FText::AsNumber(SpawnedRows.Num()), Info.DisplayName));
	}

	const bool bEmpty = (SpawnedRows.Num() == 0);
	if (EmptyStateBox)
	{
		if (UCommonTextBlock* EmptyStateText = Cast<UCommonTextBlock>(EmptyStateBox))
		{
			EmptyStateText->SetText(NSLOCTEXT("ProductionStartPopup", "NoProductionOrdersGuide",
				"제작할 수 있는 주문서가 없습니다\n제조업 오피스에서 제품을 개발하고 생산 수량을 확보하세요."));
		}
		EmptyStateBox->SetVisibility(bEmpty ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DetailRoot)
	{
		DetailRoot->SetVisibility(bEmpty ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	// 선택이 사라졌으면(소진 등) 첫 주문으로. 살아 있으면 유지 — 자원 변화마다 선택이 튀면 안 된다.
	const bool bSelectionAlive = SpawnedRows.ContainsByPredicate(
		[this](const UProductionOrderRowWidget* R) { return R && R->GetOrderID() == SelectedOrderID; });
	const bool bPreferredAlive = SpawnedRows.ContainsByPredicate(
		[this](const UProductionOrderRowWidget* R) { return R && R->GetOrderID() == InitialPreferredOrderID; });
	SelectOrder(bSelectionAlive ? SelectedOrderID : (bPreferredAlive ? InitialPreferredOrderID : FirstOrderID));
	InitialPreferredOrderID = 0;
}

void UProductionStartPopupWidget::HandleOrderRowClicked(int32 OrderID)
{
	if (OrderID == SelectedOrderID) return;
	SelectOrder(OrderID);
}

void UProductionStartPopupWidget::SelectOrder(int32 OrderID)
{
	const bool bChanged = (SelectedOrderID != OrderID);
	SelectedOrderID = OrderID;

	for (UProductionOrderRowWidget* Row : SpawnedRows)
	{
		if (Row) Row->SetRowSelected(Row->GetOrderID() == OrderID);
	}

	FProductionOrder SelectedOrder;
	if (TryGetSelectedOrder(SelectedOrder))
	{
		if (bChanged)
		{
			EResourceType Bottleneck = EResourceType::None;
			bool bSupplyBound = false;
			CurrentQuantity = FMath::Max(1, OrderMgr->GetMaxProducible(SelectedOrder, Bottleneck, bSupplyBound));
		}
		CurrentQuantity = FMath::Clamp(CurrentQuantity, 1, FMath::Max(1, SelectedOrder.RemainingQuantity));
	}

	RefreshDetail();
	RefreshMaterials();
	RefreshDynamic();
}

// ══════════════ 우 상세 ══════════════

bool UProductionStartPopupWidget::TryGetSelectedOrder(FProductionOrder& OutOrder) const
{
	if (!OrderMgr || SelectedOrderID == 0) return false;
	for (const FProductionOrder& Order : OrderMgr->GetAllOrders())
	{
		if (Order.OrderID == SelectedOrderID)
		{
			OutOrder = Order;
			return true;
		}
	}
	return false;
}

void UProductionStartPopupWidget::RefreshDetail()
{
	FProductionOrder Order;
	if (!TryGetSelectedOrder(Order)) return;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	bool bProjOk = false;
	const FProjectData Proj = TableMgr->GetProjectData(Order.CompanyType, Order.ProjectIndex, bProjOk);

	if (DetailNameText)
	{
		DetailNameText->SetText((bProjOk && !Proj.ProjectName.IsEmpty())
			? Proj.ProjectName : FText::FromString(Order.ProductName));
	}
	if (DetailSubText)
	{
		const bool bHasSub = bProjOk && !Proj.SubName.IsEmpty();
		DetailSubText->SetText(bHasSub ? Proj.SubName : FText::GetEmpty());
		DetailSubText->SetVisibility(bHasSub ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DetailGradeText)
	{
		const UEnum* GradeEnum = StaticEnum<EQualityGrade>();
		DetailGradeText->SetText(FText::FromString(
			GradeEnum ? GradeEnum->GetNameStringByValue(static_cast<int64>(Order.Grade)) : TEXT("?")));
	}
	if (DetailGradeBorder)
	{
		// 등급색 SOT = 레일 행의 노브 하나. 팝업이 자기 사본을 들면 두 곳이 갈린다.
		for (UProductionOrderRowWidget* Row : SpawnedRows)
		{
			if (Row && Row->GetOrderID() == SelectedOrderID)
			{
				DetailGradeBorder->SetBrushColor(Row->GetGradeColor(Order.Grade));
				break;
			}
		}
	}

	if (DetailCoverImage)
	{
		DetailCoverImage->SetBrushFromTexture(nullptr);
	}
	if (DetailCoverImage && bProjOk && !Proj.Icon.IsNull())
	{
		const TSoftObjectPtr<UTexture2D> SoftTex = Proj.Icon;
		if (UTexture2D* Already = SoftTex.Get())
		{
			DetailCoverImage->SetBrushFromTexture(Already);
		}
		else
		{
			TWeakObjectPtr<UProductionStartPopupWidget> WeakThis(this);
			const int32 RequestedOrderID = Order.OrderID;
			UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftTex.ToSoftObjectPath(),
				FStreamableDelegate::CreateLambda([WeakThis, SoftTex, RequestedOrderID]()
				{
					UProductionStartPopupWidget* Strong = WeakThis.Get();
					if (Strong && Strong->SelectedOrderID == RequestedOrderID && Strong->DetailCoverImage)
					{
						if (UTexture2D* Tex = SoftTex.Get())
						{
							Strong->DetailCoverImage->SetBrushFromTexture(Tex);
						}
					}
				}));
		}
	}

	if (RemainChipText)
	{
		RemainChipText->SetText(FText::Format(
			NSLOCTEXT("ProductionStartPopup", "RemainChip", "남은 수량 {0}개"),
			FText::AsNumber(Order.RemainingQuantity)));
	}
	if (PerUnitCapText) PerUnitCapText->SetVisibility(ESlateVisibility::Collapsed);
	if (PerUnitEnergyText) PerUnitEnergyText->SetVisibility(ESlateVisibility::Collapsed);

	if (DetailInfoText)
	{
		bool bRecipeOk = false;
		const FProductRecipeTable Recipe = TableMgr->GetProductRecipe(Order.CompanyType, Order.ProjectIndex, bRecipeOk);
		const UEnum* GradeEnum = StaticEnum<EQualityGrade>();
		DetailInfoText->SetText(FText::Format(
			NSLOCTEXT("ProductionStartPopup", "DetailInfo", "{0} 등급 · 주문 남은 {1}개 · 1개에 {2}"),
			FText::FromString(GradeEnum ? GradeEnum->GetNameStringByValue(static_cast<int64>(Order.Grade)) : TEXT("?")),
			FText::AsNumber(Order.RemainingQuantity),
			UGlobalUtilFunctions::FormatDurationKorean(GetEffectiveProductionTimeSec(bRecipeOk ? Recipe.ProductionTimeSec : 0.0f))));
	}
}

void UProductionStartPopupWidget::RefreshMaterials()
{
	if (!MaterialGridBox) return;

	MaterialGridBox->ClearChildren();
	SpawnedMatRows.Reset();
	SpawnedProductRow = nullptr;
	MaterialOrder.Reset();

	FProductionOrder Order;
	if (!TryGetSelectedOrder(Order)) return;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	bool bRecipeOk = false;
	const FProductRecipeTable Recipe = TableMgr->GetProductRecipe(Order.CompanyType, Order.ProjectIndex, bRecipeOk);
	if (!bRecipeOk)
	{
		// DT 누락은 조용히 넘어가면 안 된다 — 빈 재료 목록 + 경고로 즉시 드러나게
		UE_LOG(LogTemp, Warning,
			TEXT("[ProductionStartPopup] DT_Recipe 누락 — CompanyType=%d, ProjectIndex=%d. CSV reimport 필요"),
			static_cast<int32>(Order.CompanyType), Order.ProjectIndex);
		return;
	}

	TSubclassOf<UUserWidget> RowClass = TableMgr->GetWidgetClass(EWidgetType::MaterialReqRow);
	if (!RowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ProductionStartPopup] EWidgetType::MaterialReqRow 미등록"));
		return;
	}

	// 칸 = 원자재 + 부품 입력(칩, 키 None) 한 목록. 순서 = 최대를 정하는 칸이 맨 앞, 나머지는 빠듯한 순
	// (내림 몫 → 실수 비율 ― GetMaxProducible 의 동률 판정과 같다). 판 높이상 첫 줄만 보일 때가 많다.
	Recipe.CollectMaterials(MaterialOrder);
	int32 NeedIndex = 0;
	int32 NeedQty = 0;
	const bool bHasProductInput = Recipe.GetProductInput(NeedIndex, NeedQty) && NeedQty > 0;
	if (bHasProductInput)
	{
		MaterialOrder.Emplace(EResourceType::None, NeedQty);
	}
	auto HaveOf = [this, &Order, NeedIndex](const TPair<EResourceType, int32>& Cell) -> int64
	{
		if (Cell.Key == EResourceType::None)
		{
			return WorldMapMgr ? WorldMapMgr->GetProductAmount(MakeProductKey(Order.CompanyType, NeedIndex)) : 0;
		}
		return ResourceMgr ? ResourceMgr->GetResourceAmount(Cell.Key) : 0;
	};
	MaterialOrder.StableSort([&HaveOf](const TPair<EResourceType, int32>& A, const TPair<EResourceType, int32>& B)
	{
		const int64 HaveA = HaveOf(A);
		const int64 HaveB = HaveOf(B);
		const int64 FloorA = A.Value > 0 ? HaveA / A.Value : MAX_int64;
		const int64 FloorB = B.Value > 0 ? HaveB / B.Value : MAX_int64;
		if (FloorA != FloorB) return FloorA < FloorB;
		const double RatioA = A.Value > 0 ? static_cast<double>(HaveA) / A.Value : TNumericLimits<double>::Max();
		const double RatioB = B.Value > 0 ? static_cast<double>(HaveB) / B.Value : TNumericLimits<double>::Max();
		return RatioA < RatioB;
	});
	EResourceType Bottleneck = EResourceType::None;
	bool bSupplyBound = false;
	if (OrderMgr)
	{
		OrderMgr->GetMaxProducible(Order, Bottleneck, bSupplyBound);
	}
	if (bSupplyBound)
	{
		// 매니저가 지목한 칸(칩 병목이면 None = 부품 칸)을 맨 앞으로 ― 띠가 말하는 칸이 항상 보이게
		const int32 CapIndex = MaterialOrder.IndexOfByPredicate(
			[Bottleneck](const TPair<EResourceType, int32>& Cell) { return Cell.Key == Bottleneck; });
		if (CapIndex > 0)
		{
			const TPair<EResourceType, int32> CapCell = MaterialOrder[CapIndex];
			MaterialOrder.RemoveAt(CapIndex);
			MaterialOrder.Insert(CapCell, 0);
		}
	}

	// 칸 격자는 3열 ― UniformGrid 면 행·열을 직접 주고, 모자란 열은 빈 칸으로 채워 카드 폭을 지킨다
	UUniformGridPanel* Grid = Cast<UUniformGridPanel>(MaterialGridBox);
	constexpr int32 Columns = 3;
	int32 CellIndex = 0;
	auto Place = [this, Grid, &CellIndex](UWidget* Cell)
	{
		if (UUniformGridSlot* GridSlot = Grid ? Grid->AddChildToUniformGrid(Cell, CellIndex / Columns, CellIndex % Columns) : nullptr)
		{
			// 격자 칸 기본 정렬은 왼쪽 ― 카드가 칸 폭을 채워야 3열 간격이 슬롯 여백(16)과 같아진다
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}
		else if (!Grid)
		{
			MaterialGridBox->AddChild(Cell);
		}
		++CellIndex;
	};

	// 부품 입력도 같은 카드로 같은 목록에 둔다 — 다른 자리에 두면 "무엇이 모자라는가"를 두 곳에서 읽어야 한다
	for (const TPair<EResourceType, int32>& Cell : MaterialOrder)
	{
		UMaterialReqRowWidget* Row = CreateWidget<UMaterialReqRowWidget>(this, RowClass);
		if (!Row) continue;
		Place(Row);
		SpawnedMatRows.Add(Row);
		if (Cell.Key == EResourceType::None)
		{
			SpawnedProductRow = Row;
		}
	}

	while (Grid && CellIndex > 0 && CellIndex < Columns)
	{
		Place(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()));
	}
}

// ══════════════ 경량 갱신 (수량/자원 변화마다) ══════════════

void UProductionStartPopupWidget::RefreshDynamic()
{
	FProductionOrder Order;
	if (!TryGetSelectedOrder(Order))
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	bool bRecipeOk = false;
	const FProductRecipeTable Recipe = TableMgr->GetProductRecipe(Order.CompanyType, Order.ProjectIndex, bRecipeOk);

	EResourceType Bottleneck = EResourceType::None;
	bool bSupplyBound = false;
	const int32 MaxProducible = GetSelectedMaxProducible(Bottleneck, bSupplyBound);

	const int32 Qty = FMath::Max(1, CurrentQuantity);
	const bool bOverCap = (Qty > MaxProducible);

	// 최대를 정하는 칸 = 매니저가 지목한 칸(칩이면 부품 칸). 1개도 못 만드는데 지목이 없으면 맨 앞 칸(가장 빠듯함)
	int32 CapCell = INDEX_NONE;
	if (bSupplyBound)
	{
		CapCell = MaterialOrder.IndexOfByPredicate(
			[Bottleneck](const TPair<EResourceType, int32>& Cell) { return Cell.Key == Bottleneck; });
	}
	if (CapCell == INDEX_NONE && MaxProducible <= 0 && MaterialOrder.Num() > 0)
	{
		CapCell = 0;
	}
	const EResourceType CapMaterial = MaterialOrder.IsValidIndex(CapCell) ? MaterialOrder[CapCell].Key : Bottleneck;

	// ── 부품 입력(칩) 이름·보유 ──
	int32 ProductNeedIndex = 0;
	int32 ProductNeedQty = 0;
	const bool bHasProductInput = bRecipeOk && Recipe.GetProductInput(ProductNeedIndex, ProductNeedQty);
	int64 ProductHave = 0;
	FText ProductInputName;
	TSoftObjectPtr<UTexture2D> ProductInputIcon;
	if (bHasProductInput)
	{
		ProductHave = WorldMapMgr ? WorldMapMgr->GetProductAmount(MakeProductKey(Order.CompanyType, ProductNeedIndex)) : 0;
		// DT 행이 없으면 GetProjectData 가 빈 구조체를 주므로 이름·아이콘이 빈 채로 간다 (코드 폴백 금지)
		bool bInputProjOk = false;
		const FProjectData InputProj = TableMgr->GetProjectData(Order.CompanyType, ProductNeedIndex, bInputProjOk);
		ProductInputName = InputProj.ProjectName;
		ProductInputIcon = InputProj.Icon;
	}

	// ── 재료 칸 (1개당 필요 · 보유 · 막대 = 필요×수량 ÷ 보유) ──
	int32 LackCount = 0;
	if (SpawnedMatRows.Num() == MaterialOrder.Num())
	{
		for (int32 i = 0; i < MaterialOrder.Num(); ++i)
		{
			const bool bPart = MaterialOrder[i].Key == EResourceType::None;
			const int64 Have = bPart ? ProductHave : (ResourceMgr ? ResourceMgr->GetResourceAmount(MaterialOrder[i].Key) : 0);
			if (Have < static_cast<int64>(MaterialOrder[i].Value) * Qty) ++LackCount;
			UMaterialReqRowWidget* Row = SpawnedMatRows[i];
			if (!Row) continue;
			const int32 CapMark = (i == CapCell) ? FMath::Max(0, MaxProducible) : INDEX_NONE;
			if (bPart)
			{
				Row->SetProductRequirement(ProductInputName, ProductInputIcon, Have, MaterialOrder[i].Value, Qty, CapMark);
			}
			else
			{
				Row->SetRequirement(MaterialOrder[i].Key, Have, MaterialOrder[i].Value, Qty, CapMark);
			}
		}
	}

	const int32 CellCount = MaterialOrder.Num();
	if (MaterialLabelText)
	{
		// 부족 수는 색이 아니라 글로도 ― 수량을 최대보다 올렸을 때만 붙는다
		MaterialLabelText->SetText(LackCount > 0
			? FText::Format(NSLOCTEXT("ProductionStartPopup", "MatLabelCountLack", "필요 재료 {0}종 (1개 만들 때) ― {1}종 부족"),
				FText::AsNumber(CellCount), FText::AsNumber(LackCount))
			: FText::Format(NSLOCTEXT("ProductionStartPopup", "MatLabelCount", "필요 재료 {0}종 (1개 만들 때)"),
				FText::AsNumber(CellCount)));
	}
	if (MaterialMoreText)
	{
		// 첫 줄 밖 칸은 스크롤에 숨는다 ― 몇 종이 더 있는지 글로 알린다
		const int32 Hidden = CellCount - FMath::Max(1, MaterialVisibleCells);
		MaterialMoreText->SetText(Hidden > 0
			? FText::Format(NSLOCTEXT("ProductionStartPopup", "MatMore", "+{0}종 더 있습니다"), FText::AsNumber(Hidden))
			: FText::GetEmpty());
		MaterialMoreText->SetVisibility(Hidden > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// ── 수량 줄 ──
	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(Qty));
		QuantityText->SetColorAndOpacity(bOverCap ? CapacityInkOver : CapacityInkNormal);
	}
	RefreshBulkVisuals();
	ApplyStepEnabled(BulkBtn_x1, Qty > 1);
	ApplyStepEnabled(MinusButton, Qty > 1);
	ApplyStepEnabled(PlusButton, Qty < Order.RemainingQuantity);
	if (MaxJumpButton)
	{
		MaxJumpButton->SetVisibility(ESlateVisibility::Visible);
		ApplyStepEnabled(MaxJumpButton, (MaxProducible > 0) && (Qty != MaxProducible));
	}

	const float PerUnitSec = GetEffectiveProductionTimeSec(bRecipeOk ? Recipe.ProductionTimeSec : 0.0f);
	if (PerUnitTimeText)
	{
		PerUnitTimeText->SetText(FText::Format(
			NSLOCTEXT("ProductionStartPopup", "PerUnitTime", "개당 {0}"),
			UGlobalUtilFunctions::FormatDurationKorean(PerUnitSec)));
	}
	if (SummaryText)
	{
		SummaryText->SetText(FText::Format(
			NSLOCTEXT("ProductionStartPopup", "SummaryTotal", "총 {0}"),
			UGlobalUtilFunctions::FormatDurationKorean(PerUnitSec * Qty)));
	}

	// ── 최대를 정한 재료 띠 ──
	const FText BandText = BuildLimitBandText(MaxProducible, CapMaterial,
		bSupplyBound || MaxProducible <= 0, ProductInputName, ProductHave);
	if (CapacityText)
	{
		CapacityText->SetText(BandText);
	}
	if (LimitBand)
	{
		LimitBand->SetVisibility(BandText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (ChainTimeText)
	{
		ChainTimeText->SetVisibility(ESlateVisibility::HitTestInvisible);
		ChainTimeText->SetText(NSLOCTEXT("ProductionStartPopup", "ClaimFreesLine", "다 만든 뒤 받으면 라인이 비워집니다"));
	}

	// ── CTA ──
	const bool bLineOk = IsLineAvailable();
	const bool bCanProduce = !bOverCap && (MaxProducible > 0) && bLineOk;
	ApplyAcceptLook(bCanProduce,
		!bLineOk ? NSLOCTEXT("ProductionStartPopup", "LineFullAction", "빈 라인 없음")
		: bCanProduce ? NSLOCTEXT("ProductionStartPopup", "StartAction", "생산 시작")
		: NSLOCTEXT("ProductionStartPopup", "MaterialsShortAction", "재료 부족"),
		Qty);
	// 사유는 상시 라벨이 아니라 클릭 시 토스트 (HandleAccept) — 재료 행·띠가 이미 같은 사실을 말한다
}

FText UProductionStartPopupWidget::BuildLimitBandText(int32 MaxProducible, EResourceType Bottleneck,
	bool bSupplyBound, const FText& ProductInputName, int64 ProductHave) const
{
	if (!bSupplyBound)
	{
		return FText::GetEmpty();
	}
	const bool bProductBound = (Bottleneck == EResourceType::None) && !ProductInputName.IsEmpty();
	const FText Name = bProductBound ? ProductInputName : GetResourceDisplayName(Bottleneck);
	const int64 Have = bProductBound ? ProductHave
		: (ResourceMgr && Bottleneck != EResourceType::None ? ResourceMgr->GetResourceAmount(Bottleneck) : 0);
	if (MaxProducible > 0)
	{
		// 이름을 못 구하면 문장이 「가 3개뿐이라」로 꼬인다 ― 수치만 말한다
		return Name.IsEmpty()
			? FText::Format(NSLOCTEXT("ProductionStartPopup", "LimitBandPlain", "재료가 모자라 최대 {0}개까지 만들 수 있습니다"),
				FText::AsNumber(MaxProducible))
			: CountryDrawer::ProductionLimitBand(Name, Have, MaxProducible);
	}
	if (Name.IsEmpty())
	{
		return NSLOCTEXT("ProductionStartPopup", "BlockedBandPlain", "재료가 모자라 1개도 만들 수 없습니다");
	}
	return FText::Format(NSLOCTEXT("ProductionStartPopup", "BlockedBand", "{0} {1}개뿐이라 1개도 만들 수 없습니다"),
		FText::FromString(KoreanJosa::Attach(Name.ToString(), TEXT("가"), TEXT("이"))), FText::AsNumber(Have));
}

void UProductionStartPopupWidget::ApplyStepEnabled(UButtonWidget* Button, bool bEnabled) const
{
	if (!Button) return;
	Button->SetIsEnabled(bEnabled);
	Button->SetTextColor(bEnabled ? StepLabelInk : StepDisabledLabelInk);
}

void UProductionStartPopupWidget::ApplyAcceptLook(bool bCanProduce, const FText& Label, int32 Quantity)
{
	if (!AcceptButton) return;

	// 회색이어도 누르면 사유 토스트가 떠야 한다 ― 비용 게이트를 끄고 모양만 바꾼다
	AcceptButton->SetEnabled(true);
	if (const TSubclassOf<UCommonButtonStyle> Target = bCanProduce ? AcceptStyle_Enabled : AcceptStyle_Disabled)
	{
		AcceptButton->ButtonStyleClass = Target;
		if (UCommonButtonBase* Inner = AcceptButton->WidgetTree
			? Cast<UCommonButtonBase>(AcceptButton->WidgetTree->FindWidget(TEXT("Btn"))) : nullptr)
		{
			Inner->SetStyle(Target);
		}
	}
	AcceptButton->SetButtonText(Label);
	const FSlateColor& Ink = bCanProduce ? AcceptLabelInk : AcceptDisabledLabelInk;
	AcceptButton->BtnTextColor = Ink;
	if (UTextBlock* LabelText = AcceptButton->WidgetTree
		? Cast<UTextBlock>(AcceptButton->WidgetTree->FindWidget(TEXT("BtnText"))) : nullptr)
	{
		LabelText->SetColorAndOpacity(Ink);
	}
	AcceptButton->SetCostVisible(true);
	AcceptButton->SetCostLabel(FText::Format(NSLOCTEXT("ProductionStartPopup", "AcceptChip", "{0}개"), FText::AsNumber(Quantity)));
	CountryDrawer::StyleChipCount(AcceptButton, /*bIncoming=*/false);
}

void UProductionStartPopupWidget::RefreshHeader()
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	if (FactoryNameText && TableMgr)
	{
		bool bOk = false;
		const FCountryInfoTable Info = TableMgr->GetCountryInfo(Country, bOk);
		if (bOk)
		{
			FactoryNameText->SetText(FText::Format(
				NSLOCTEXT("ProductionStartPopup", "FactoryName", "{0} 공장"), Info.DisplayName));
		}
	}

	if (!FactoryMgr) return;

	const int32 Active = FactoryMgr->GetActiveLineCount(Country);
	const int32 Max = FactoryMgr->GetMaxLines(Country);

	if (LineCountText)
	{
		LineCountText->SetText(FText::Format(
			NSLOCTEXT("ProductionStartPopup", "LineCount", "라인 {0} / {1}"),
			FText::AsNumber(Active), FText::AsNumber(Max)));
	}
	if (LineChipBorder)
	{
		LineChipBorder->SetBrushColor(Active >= Max ? LineChipFull : LineChipNormal);
	}
}

// ══════════════ 조회 헬퍼 ══════════════

int32 UProductionStartPopupWidget::GetSelectedMaxProducible(EResourceType& OutBottleneck, bool& bOutSupplyBound) const
{
	OutBottleneck = EResourceType::None;
	bOutSupplyBound = false;

	FProductionOrder Order;
	if (!OrderMgr || !TryGetSelectedOrder(Order)) return 0;
	return OrderMgr->GetMaxProducible(Order, OutBottleneck, bOutSupplyBound);
}

float UProductionStartPopupWidget::GetEffectiveProductionTimeSec(float BaseTimePerUnitSec) const
{
	const float SafeTimePerUnit = BaseTimePerUnitSec > 0.0f ? BaseTimePerUnitSec : 5.0f;
	const float Speed = FactoryMgr ? FMath::Max(FactoryMgr->GetSpeedMultiplier(Country), KINDA_SMALL_NUMBER) : 1.0f;
	return SafeTimePerUnit / Speed;
}

bool UProductionStartPopupWidget::IsLineAvailable() const
{
	if (!FactoryMgr) return true;  // 매니저 없음 = 판단 불가, fail-open (StartProduction 이 최종 게이트)
	return FactoryMgr->GetActiveLineCount(Country) < FactoryMgr->GetMaxLines(Country);
}

FText UProductionStartPopupWidget::GetResourceDisplayName(EResourceType Type) const
{
	if (Type == EResourceType::None) return FText::GetEmpty();

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return FText::GetEmpty();

	bool bOk = false;
	const FResourceInfo Info = TableMgr->GetResourceInfo(Type, bOk);
	return bOk ? Info.DisplayName : FText::GetEmpty();
}

// 이름을 코드에서 만들면 DT_Project_* 가 표시명 단일 출처 자리를 잃는다 — 못 구하면 빈 FText 로 둔다
FText UProductionStartPopupWidget::GetProductInputName(const FProductionOrder& Order) const
{
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return FText::GetEmpty();

	bool bRecipeOk = false;
	const FProductRecipeTable Recipe = TableMgr->GetProductRecipe(Order.CompanyType, Order.ProjectIndex, bRecipeOk);

	int32 NeedIndex = 0;
	int32 NeedQty = 0;
	if (!bRecipeOk || !Recipe.GetProductInput(NeedIndex, NeedQty)) return FText::GetEmpty();

	bool bProjOk = false;
	const FProjectData Proj = TableMgr->GetProjectData(Order.CompanyType, NeedIndex, bProjOk);
	return bProjOk ? Proj.ProjectName : FText::GetEmpty();
}

// ══════════════ 수량 ══════════════

void UProductionStartPopupWidget::RefreshBulkVisuals()
{
	auto Apply = [this](UButtonWidget* Btn, int32 Preset)
	{
		if (!Btn) return;
		TSubclassOf<UCommonButtonStyle> Target = (CurrentQuantity == Preset) ? BulkStyle_Selected : BulkStyle_Unselected;
		if (Target) Btn->SetStyle(Target);
	};
	// BulkBtn_x1 = 「최소」 스텝 버튼이라 선택 스타일을 갈지 않는다(ApplyStepEnabled 가 맡는다)
	Apply(BulkBtn_x10, 10);
	Apply(BulkBtn_x50, 50);
}

void UProductionStartPopupWidget::SetQuantity(int32 NewQuantity)
{
	FProductionOrder Order;
	if (!TryGetSelectedOrder(Order)) return;

	// 소프트 캡 — 재료 상한이 아니라 주문서 잔여까지 올라간다.
	// 그 위 구간에서 무엇이 얼마나 모자란지가 재료 행에 드러나는 것이 이 화면의 목적 중 하나다.
	const int32 Cap = FMath::Max(1, Order.RemainingQuantity);
	CurrentQuantity = FMath::Clamp(NewQuantity, 1, Cap);
	RefreshDynamic();
}

void UProductionStartPopupWidget::HandleMinus()  { SetQuantity(CurrentQuantity - 1); }
void UProductionStartPopupWidget::HandlePlus()   { SetQuantity(CurrentQuantity + 1); }
void UProductionStartPopupWidget::HandleBulkX1()  { SetQuantity(1); }
void UProductionStartPopupWidget::HandleBulkX10() { SetQuantity(10); }
void UProductionStartPopupWidget::HandleBulkX50() { SetQuantity(50); }

void UProductionStartPopupWidget::HandleMaxJump()
{
	EResourceType Bottleneck = EResourceType::None;
	bool bSupplyBound = false;
	const int32 MaxProducible = GetSelectedMaxProducible(Bottleneck, bSupplyBound);
	if (MaxProducible > 0) SetQuantity(MaxProducible);
}

// ══════════════ 제작 시작 ══════════════

void UProductionStartPopupWidget::HandleAccept()
{
	if (bStartingProduction) return;
	FProductionOrder Order;
	if (!TryGetSelectedOrder(Order) || CurrentQuantity <= 0) return;

	UGameInstance* GI = GetGameInstance();
	UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;

	EResourceType Bottleneck = EResourceType::None;
	bool bSupplyBound = false;
	const int32 MaxProducible = GetSelectedMaxProducible(Bottleneck, bSupplyBound);
	if (CurrentQuantity > Order.RemainingQuantity)
	{
		if (UIMgr)
		{
			UIMgr->ShowNotification(NSLOCTEXT("ProductionStartPopup", "OrderQuantityChanged", "주문 잔량이 변경되었습니다"),
				2.5f, ENotificationType::Warning);
		}
		ScheduleOrderRailRefresh();
		return;
	}
	if (!IsLineAvailable())
	{
		if (UIMgr)
		{
			UIMgr->ShowNotification(
				NSLOCTEXT("ProductionStartPopup", "LinesFullToast", "생산 라인이 가득 찼습니다"),
				2.5f, ENotificationType::Warning);
		}
		return;
	}

	// 시각은 스타일로만 막고 클릭은 항상 받는다 — 막힌 사유를 눌러서 알 수 있게
	if (CurrentQuantity > MaxProducible)
	{
		FText Message = NSLOCTEXT("ProductionStartPopup", "InsufficientMaterials", "재료가 부족합니다");

		// 자원 이름이 비는 것 = 칩이 상한을 정했다는 신호(GetMaxProducible 규약). 그 때만 제품 문구로 간다 —
		// 재료가 더 빠듯하면 재료를 먼저 말해야 상한 표시와 사유가 갈리지 않는다.
		const bool bProductBound = bSupplyBound && (Bottleneck == EResourceType::None);
		int32 ShortIndex = 0;
		int32 ShortBy = 0;
		if (bProductBound && OrderMgr && OrderMgr->IsProductInputShort(Order, CurrentQuantity, ShortIndex, ShortBy))
		{
			const FText PartName = GetProductInputName(Order);
			Message = PartName.IsEmpty()
				? FText::Format(NSLOCTEXT("ProductionStartPopup", "ProductInputShortPlain", "필요한 부품이 {0}개 부족합니다"),
					FText::AsNumber(ShortBy))
				: FText::Format(NSLOCTEXT("ProductionStartPopup", "ProductInputShort", "{0} {1}개가 부족합니다"),
					PartName, FText::AsNumber(ShortBy));
		}

		if (UIMgr)
		{
			UIMgr->ShowNotification(Message, 2.5f, ENotificationType::Failed);
		}
		return;
	}
	// 시작·차감 본체는 OverseasActions 가 맡고, 여기서는 이유 안내와 창 닫기만 맡는다.
	FText FailMessage;
	bStartingProduction = true;
	const int32 NewLineId = OverseasActions::StartLineFromOrder(GI, Country, Order.OrderID, CurrentQuantity, FailMessage);
	bStartingProduction = false;
	if (NewLineId <= 0)
	{
		ScheduleOrderRailRefresh();
		if (UIMgr && !FailMessage.IsEmpty())
		{
			UIMgr->ShowNotification(FailMessage, 2.5f, ENotificationType::Failed);
		}
		return;
	}

	// AddToViewport 모델 — RemoveFromParent 가 정통 close (stack 밖이라 DeactivateWidget 무효)
	PlayCloseSound();
	RemoveFromParent();
}

// ══════════════ 외부 변화 구독 ══════════════

void UProductionStartPopupWidget::ScheduleOrderRailRefresh()
{
	if (bStartingProduction || OrderRailRefreshTimerHandle.IsValid()) return;
	if (UWorld* WidgetWorld = GetWorld())
	{
		OrderRailRefreshTimerHandle = WidgetWorld->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UProductionStartPopupWidget::FlushOrderRailRefresh));
	}
}

void UProductionStartPopupWidget::FlushOrderRailRefresh()
{
	OrderRailRefreshTimerHandle.Invalidate();
	RebuildOrderRail();
}

void UProductionStartPopupWidget::HandleOrderCreated(int32 /*BuildingID*/, const FProductionOrder& /*Order*/)
{
	ScheduleOrderRailRefresh();
}

void UProductionStartPopupWidget::HandleOrderConsumed(int32 /*OrderID*/, int32 /*ConsumedAmount*/, int32 /*RemainingQuantity*/)
{
	ScheduleOrderRailRefresh();
}

void UProductionStartPopupWidget::HandleOrderDepleted(int32 /*OrderID*/)
{
	ScheduleOrderRailRefresh();
}

// 상한/병목만 다시 칠한다 — 이름·커버·선택상태는 건드리지 않는다(스크롤/선택 유지)
void UProductionStartPopupWidget::RefreshOrderRailHints()
{
	if (!OrderMgr) return;

	for (UProductionOrderRowWidget* Row : SpawnedRows)
	{
		if (!Row) continue;
		for (const FProductionOrder& Order : OrderMgr->GetAllOrders())
		{
			if (Order.OrderID != Row->GetOrderID()) continue;

			EResourceType Bottleneck = EResourceType::None;
			bool bSupplyBound = false;
			const int32 MaxProducible = OrderMgr->GetMaxProducible(Order, Bottleneck, bSupplyBound);

			// 칩 병목일 때만 이름을 조회한다 — 평시엔 주문서마다 DT 두 번을 더 타게 된다
			const FText ProductInputName = (bSupplyBound && Bottleneck == EResourceType::None)
				? GetProductInputName(Order) : FText::GetEmpty();
			Row->UpdateCapacityHint(MaxProducible, Bottleneck, bSupplyBound, ProductInputName);
			break;
		}
	}
}

void UProductionStartPopupWidget::HandleResourceChanged(EResourceType Type, int64 /*NewAmount*/)
{
	if (bStartingProduction) return;
	// ⚠ OnResourceChanged 는 Money 에도 발화하고, 오피스 수익은 1초 드립이다.
	//   타입 필터가 없으면 팝업이 열려 있는 동안 초당 한 번씩 전 행을 재조회하게 된다.
	if (!FProductRecipeTable::GetAllRawMaterialTypes().Contains(Type)) return;

	RefreshOrderRailHints();
	RefreshDynamic();
}

void UProductionStartPopupWidget::HandleProductInventoryChanged(FIntPoint /*ProductKey*/, int64 /*NewAmount*/)
{
	if (bStartingProduction) return;
	// 완성품은 유닛 1개 단위로 발화한다(WorldMapManager Tick) — 자원 경로와 달리 타입 필터를 둘 자리가 없지만
	// 재조회 범위가 주문서 수로 묶여 있어 비용은 그 경로와 같다. 안 받으면 칩 힌트가 열어 둔 채로 낡는다.
	RefreshOrderRailHints();
	RefreshDynamic();
}

void UProductionStartPopupWidget::HandleLineStarted(ECountryType InCountry, FWorldFactoryLineState /*LineState*/)
{
	if (InCountry != Country || bStartingProduction) return;
	RefreshHeader();
	RefreshDynamic();
}

void UProductionStartPopupWidget::HandleLineCompleted(ECountryType InCountry, int32 /*LineId*/)
{
	if (InCountry != Country || bStartingProduction) return;
	RefreshHeader();
	RefreshDynamic();
}

void UProductionStartPopupWidget::HandleLineClaimed(ECountryType InCountry, int32 /*LineId*/, int64 /*FinalQty*/)
{
	if (InCountry != Country || bStartingProduction) return;
	RefreshHeader();
	RefreshDynamic();
}

// ══════════════ 닫기 ══════════════

void UProductionStartPopupWidget::HandleClose()
{
	PlayCloseSound();
	RemoveFromParent();
}

void UProductionStartPopupWidget::CloseFromBack()
{
	HandleClose();
}

void UProductionStartPopupWidget::HandleBackgroundClicked()
{
	PlayCloseSound();
	RemoveFromParent();
}

void UProductionStartPopupWidget::PlayCloseSound()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::ModalClose);
		}
	}
}
