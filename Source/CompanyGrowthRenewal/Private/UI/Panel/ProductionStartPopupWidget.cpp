// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/ProductionStartPopupWidget.h"
#include "UI/Element/Cards/ProductionOrderRowWidget.h"
#include "UI/Element/Common/MaterialReqRowWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ProductionOrderManager.h"
#include "Manager/WorldFactoryManager.h"
#include "Manager/ResourceItemManager.h"
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

void UProductionStartPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)    CloseButton->OnCloseClicked.AddDynamic(this, &UProductionStartPopupWidget::HandleClose);
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
	if (FactoryMgr)
	{
		FactoryMgr->OnLineStarted.AddDynamic(this, &UProductionStartPopupWidget::HandleLineStarted);
		FactoryMgr->OnLineCompleted.AddDynamic(this, &UProductionStartPopupWidget::HandleLineCompleted);
		FactoryMgr->OnLineClaimed.AddDynamic(this, &UProductionStartPopupWidget::HandleLineClaimed);
	}
}

void UProductionStartPopupWidget::EnsureManagers()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	if (!OrderMgr)    OrderMgr = GI->GetSubsystem<UProductionOrderManager>();
	if (!ResourceMgr) ResourceMgr = GI->GetSubsystem<UResourceItemManager>();
	if (!FactoryMgr)  FactoryMgr = GI->GetSubsystem<UWorldFactoryManager>();
}

void UProductionStartPopupWidget::NativeDestruct()
{
	if (CloseButton)    CloseButton->OnCloseClicked.RemoveDynamic(this, &UProductionStartPopupWidget::HandleClose);
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
	OrderMgr = nullptr;

	SpawnedRows.Reset();
	SpawnedMatRows.Reset();
	Super::NativeDestruct();
}

void UProductionStartPopupWidget::InitializeForCountry(ECountryType InCountry)
{
	// 호출자가 AddToViewport 보다 먼저 부른다 — NativeConstruct 를 기다리면 매니저가 전부 null
	EnsureManagers();

	Country = InCountry;
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
		bool bMaterialBound = false;
		const int32 MaxProducible = OrderMgr->GetMaxProducible(Order, Bottleneck, bMaterialBound);

		Row->SetOrderData(Order, MaxProducible, Bottleneck, bMaterialBound);
		Row->OnOrderRowClicked.BindUObject(this, &UProductionStartPopupWidget::HandleOrderRowClicked);

		OrderRailBox->AddChild(Row);
		SpawnedRows.Add(Row);

		if (FirstOrderID == 0) FirstOrderID = Order.OrderID;
	}

	if (OrderCountText)
	{
		OrderCountText->SetText(FText::Format(
			NSLOCTEXT("ProductionStartPopup", "OrderCount", "생산 주문서 {0}건"),
			FText::AsNumber(SpawnedRows.Num())));
	}

	const bool bEmpty = (SpawnedRows.Num() == 0);
	if (EmptyStateBox)
	{
		EmptyStateBox->SetVisibility(bEmpty ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DetailRoot)
	{
		DetailRoot->SetVisibility(bEmpty ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	// 선택이 사라졌으면(소진 등) 첫 주문으로. 살아 있으면 유지 — 자원 변화마다 선택이 튀면 안 된다.
	const bool bSelectionAlive = SpawnedRows.ContainsByPredicate(
		[this](const UProductionOrderRowWidget* R) { return R && R->GetOrderID() == SelectedOrderID; });
	SelectOrder(bSelectionAlive ? SelectedOrderID : FirstOrderID);
}

void UProductionStartPopupWidget::HandleOrderRowClicked(int32 OrderID)
{
	if (OrderID == SelectedOrderID) return;
	SelectOrder(OrderID);
}

void UProductionStartPopupWidget::SelectOrder(int32 OrderID)
{
	SelectedOrderID = OrderID;

	for (UProductionOrderRowWidget* Row : SpawnedRows)
	{
		if (Row) Row->SetRowSelected(Row->GetOrderID() == OrderID);
	}

	// 주문이 바뀌면 수량은 1부터 — 이전 주문의 수량을 물려받으면 상한이 달라 즉시 부족 상태로 열린다
	CurrentQuantity = 1;

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
			UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftTex.ToSoftObjectPath(),
				FStreamableDelegate::CreateLambda([WeakThis, SoftTex]()
				{
					UProductionStartPopupWidget* Strong = WeakThis.Get();
					if (Strong && Strong->DetailCoverImage)
					{
						if (UTexture2D* Tex = SoftTex.Get())
						{
							Strong->DetailCoverImage->SetBrushFromTexture(Tex);
						}
					}
				}));
		}
	}

	bool bRecipeOk = false;
	const FProductRecipeTable Recipe = TableMgr->GetProductRecipe(Order.CompanyType, Order.ProjectIndex, bRecipeOk);

	if (RemainChipText)
	{
		RemainChipText->SetText(FText::Format(
			NSLOCTEXT("ProductionStartPopup", "RemainChip", "남은 수량 {0}개"),
			FText::AsNumber(Order.RemainingQuantity)));
	}
	if (PerUnitTimeText)
	{
		PerUnitTimeText->SetText(FText::Format(
			NSLOCTEXT("ProductionStartPopup", "PerUnitTime", "개당 {0}"),
			UGlobalUtilFunctions::FormatDurationKorean(bRecipeOk ? Recipe.ProductionTimeSec : 0.0f)));
	}
	if (PerUnitCapText)
	{
		PerUnitCapText->SetText(FText::Format(
			NSLOCTEXT("ProductionStartPopup", "PerUnitCap", "개당 시총 {0}"),
			UGlobalUtilFunctions::AbbreviateNumber(bRecipeOk ? Recipe.BaseMarketCap : 0)));
	}
	if (PerUnitEnergyText)
	{
		PerUnitEnergyText->SetText(FText::Format(
			NSLOCTEXT("ProductionStartPopup", "PerUnitEnergy", "에너지 {0}"),
			FText::AsNumber(bRecipeOk ? Recipe.EnergyCost : 0)));
	}
}

void UProductionStartPopupWidget::RefreshMaterials()
{
	if (!MaterialGridBox) return;

	MaterialGridBox->ClearChildren();
	SpawnedMatRows.Reset();

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

	TArray<TPair<EResourceType, int32>> Materials;
	Recipe.CollectMaterials(Materials);

	for (const TPair<EResourceType, int32>& Pair : Materials)
	{
		UMaterialReqRowWidget* Row = CreateWidget<UMaterialReqRowWidget>(this, RowClass);
		if (!Row) continue;
		MaterialGridBox->AddChild(Row);
		SpawnedMatRows.Add(Row);
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
	bool bMaterialBound = false;
	const int32 MaxProducible = GetSelectedMaxProducible(Bottleneck, bMaterialBound);

	const int32 Qty = FMath::Max(1, CurrentQuantity);
	const bool bOverCap = (Qty > MaxProducible);

	// ── 재료 행 ──
	TArray<TPair<EResourceType, int32>> Materials;
	if (bRecipeOk) Recipe.CollectMaterials(Materials);

	if (SpawnedMatRows.Num() == Materials.Num())
	{
		for (int32 i = 0; i < Materials.Num(); ++i)
		{
			UMaterialReqRowWidget* Row = SpawnedMatRows[i];
			if (!Row) continue;

			const int64 Need = static_cast<int64>(Materials[i].Value) * Qty;
			const int64 Have = ResourceMgr ? ResourceMgr->GetResourceAmount(Materials[i].Key) : 0;
			const bool bIsBottleneck = bMaterialBound && (Materials[i].Key == Bottleneck);
			Row->SetRequirement(Materials[i].Key, Have, Need, bIsBottleneck ? MaxProducible : INDEX_NONE);
		}
	}

	if (MaterialLabelText)
	{
		int32 LackCount = 0;
		for (const TPair<EResourceType, int32>& M : Materials)
		{
			const int64 Need = static_cast<int64>(M.Value) * Qty;
			const int64 Have = ResourceMgr ? ResourceMgr->GetResourceAmount(M.Key) : 0;
			if (Have < Need) ++LackCount;
		}
		MaterialLabelText->SetText(LackCount > 0
			? FText::Format(NSLOCTEXT("ProductionStartPopup", "MatLabelLack", "필요 재료 {0}종 ― {1}종 부족"),
				FText::AsNumber(Materials.Num()), FText::AsNumber(LackCount))
			: FText::Format(NSLOCTEXT("ProductionStartPopup", "MatLabel", "필요 재료 {0}종"),
				FText::AsNumber(Materials.Num())));
		MaterialLabelText->SetColorAndOpacity(LackCount > 0 ? CapacityInkOver : CapacityInkNormal);
	}

	// ── 수량 / 상한 판독부 ──
	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(Qty));
		QuantityText->SetColorAndOpacity(bOverCap ? CapacityInkOver : CapacityInkNormal);
	}
	if (MinusButton) MinusButton->SetIsEnabled(Qty > 1);
	if (PlusButton)  PlusButton->SetIsEnabled(Qty < Order.RemainingQuantity);
	if (MaxJumpButton)
	{
		const bool bUseful = (MaxProducible > 0) && (Qty != MaxProducible);
		MaxJumpButton->SetVisibility(bUseful ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		MaxJumpButton->SetButtonText(FText::Format(
			NSLOCTEXT("ProductionStartPopup", "MaxJump", "최대 {0}"), FText::AsNumber(MaxProducible)));
	}

	if (CapacityText)
	{
		FText Line;
		if (MaxProducible <= 0)
		{
			Line = NSLOCTEXT("ProductionStartPopup", "CapNone", "1개도 만들 수 없습니다");
		}
		else if (bMaterialBound)
		{
			// 조사 폴백 "이(가)" 금지 — 조사가 필요 없는 어순
			Line = FText::Format(NSLOCTEXT("ProductionStartPopup", "CapBound", "제작 가능 {0}개 · 상한 {1}"),
				FText::AsNumber(MaxProducible), GetResourceDisplayName(Bottleneck));
		}
		else
		{
			Line = FText::Format(NSLOCTEXT("ProductionStartPopup", "CapFree", "제작 가능 {0}개"),
				FText::AsNumber(MaxProducible));
		}
		CapacityText->SetText(Line);
		CapacityText->SetColorAndOpacity(
			bOverCap ? CapacityInkOver : (bMaterialBound ? CapacityInkTight : CapacityInkNormal));
	}

	if (SummaryText)
	{
		const float TotalSec = (bRecipeOk ? Recipe.ProductionTimeSec : 0.0f) * Qty;
		SummaryText->SetText(FText::Format(
			NSLOCTEXT("ProductionStartPopup", "Summary", "총 소요 {0} · 시총 {1} · 에너지 {2}"),
			UGlobalUtilFunctions::FormatDurationKorean(TotalSec),
			UGlobalUtilFunctions::AbbreviateNumber(static_cast<int64>(bRecipeOk ? Recipe.BaseMarketCap : 0) * Qty),
			UGlobalUtilFunctions::AbbreviateNumber(static_cast<int64>(bRecipeOk ? Recipe.EnergyCost : 0) * Qty)));
	}

	// ── CTA ──
	const bool bLineOk = IsLineAvailable();
	const bool bCanProduce = !bOverCap && (MaxProducible > 0) && bLineOk;

	if (AcceptButton)
	{
		TSubclassOf<UCommonButtonStyle> Target = bCanProduce ? AcceptStyle_Enabled : AcceptStyle_Disabled;
		if (Target) AcceptButton->SetStyle(Target);
	}
	// 사유는 상시 라벨이 아니라 클릭 시 토스트 (HandleAccept) — 재료 행·헤더 칩이 이미 같은 사실을 말한다
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

int32 UProductionStartPopupWidget::GetSelectedMaxProducible(EResourceType& OutBottleneck, bool& bOutMaterialBound) const
{
	OutBottleneck = EResourceType::None;
	bOutMaterialBound = false;

	FProductionOrder Order;
	if (!OrderMgr || !TryGetSelectedOrder(Order)) return 0;
	return OrderMgr->GetMaxProducible(Order, OutBottleneck, bOutMaterialBound);
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

// ══════════════ 수량 ══════════════

void UProductionStartPopupWidget::SetBulkStep(int32 NewStep)
{
	if (BulkStep == NewStep) return;
	BulkStep = NewStep;
	RefreshBulkVisuals();
	// 배율만 바뀌고 수량은 유지 — 배율 칩이 수량을 건드리면 "왜 숫자가 튀지"가 된다
}

void UProductionStartPopupWidget::RefreshBulkVisuals()
{
	auto Apply = [this](UButtonWidget* Btn, int32 Step)
	{
		if (!Btn) return;
		TSubclassOf<UCommonButtonStyle> Target = (BulkStep == Step) ? BulkStyle_Selected : BulkStyle_Unselected;
		if (Target) Btn->SetStyle(Target);
	};
	Apply(BulkBtn_x1, 1);
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

void UProductionStartPopupWidget::HandleMinus()  { SetQuantity(CurrentQuantity - BulkStep); }
void UProductionStartPopupWidget::HandlePlus()   { SetQuantity(CurrentQuantity + BulkStep); }
void UProductionStartPopupWidget::HandleBulkX1()  { SetBulkStep(1); }
void UProductionStartPopupWidget::HandleBulkX10() { SetBulkStep(10); }
void UProductionStartPopupWidget::HandleBulkX50() { SetBulkStep(50); }

void UProductionStartPopupWidget::HandleMaxJump()
{
	EResourceType Bottleneck = EResourceType::None;
	bool bMaterialBound = false;
	const int32 MaxProducible = GetSelectedMaxProducible(Bottleneck, bMaterialBound);
	if (MaxProducible > 0) SetQuantity(MaxProducible);
}

// ══════════════ 제작 시작 ══════════════

void UProductionStartPopupWidget::HandleAccept()
{
	FProductionOrder Order;
	if (!TryGetSelectedOrder(Order) || CurrentQuantity <= 0) return;

	UGameInstance* GI = GetGameInstance();
	UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;

	EResourceType Bottleneck = EResourceType::None;
	bool bMaterialBound = false;
	const int32 MaxProducible = GetSelectedMaxProducible(Bottleneck, bMaterialBound);

	// 시각은 스타일로만 막고 클릭은 항상 받는다 — 막힌 사유를 눌러서 알 수 있게
	if (CurrentQuantity > MaxProducible)
	{
		if (UIMgr)
		{
			UIMgr->ShowNotification(
				NSLOCTEXT("ProductionStartPopup", "InsufficientMaterials", "재료가 부족합니다"),
				2.5f, ENotificationType::Failed);
		}
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

	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr || !OrderMgr || !FactoryMgr || !ResourceMgr) return;

	bool bRecipeOk = false;
	const FProductRecipeTable Recipe = TableMgr->GetProductRecipe(Order.CompanyType, Order.ProjectIndex, bRecipeOk);
	if (!bRecipeOk) return;

	// 표시 데이터 단일 진실 = DT_Project_*
	bool bProjOk = false;
	const FProjectData Proj = TableMgr->GetProjectData(Order.CompanyType, Order.ProjectIndex, bProjOk);
	const FText DisplayName = (bProjOk && !Proj.ProjectName.IsEmpty())
		? Proj.ProjectName : FText::FromString(Order.ProductName);
	TSoftObjectPtr<UTexture2D> IconSoftPtr;
	if (bProjOk) IconSoftPtr = Proj.Icon;

	// 표시한 시간(per unit)을 그대로 BaseRate 로 환산 → 표시-실제 일치.
	// Speed 는 강화 multiplier — 실제는 표시보다 빠르다(under-promise).
	const float Speed = FactoryMgr->GetSpeedMultiplier(Country);
	const float SafeTimePerUnit = (Recipe.ProductionTimeSec > 0.0f) ? Recipe.ProductionTimeSec : 5.0f;
	const float BaseRate = (1.0f / SafeTimePerUnit) * Speed;

	// 라인 여유를 CTA 시점에 이미 확인했으므로 "차감 → 실패 → 환불" 경로가 정상 동선에서 사라진다.
	// 그래도 StartProduction 이 최종 권위라, 실패하면 차감 전에 빠져나온다.
	const int32 NewLineId = FactoryMgr->StartProduction(
		Country, Order.ProjectIndex, DisplayName, IconSoftPtr, BaseRate, static_cast<int64>(CurrentQuantity));

	if (NewLineId <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ProductionStartPopup] StartProduction 실패 — 재료 미차감"));
		if (UIMgr)
		{
			UIMgr->ShowNotification(
				NSLOCTEXT("ProductionStartPopup", "StartFailed", "생산을 시작할 수 없습니다"),
				2.5f, ENotificationType::Failed);
		}
		return;
	}

	TArray<TPair<EResourceType, int32>> Materials;
	Recipe.CollectMaterials(Materials);
	for (const TPair<EResourceType, int32>& M : Materials)
	{
		const int64 Spend = static_cast<int64>(M.Value) * static_cast<int64>(CurrentQuantity);
		if (Spend > 0) ResourceMgr->SpendResource(M.Key, Spend, /*bShouldSave=*/true);
	}
	OrderMgr->ConsumeQuantity(Order.OrderID, CurrentQuantity);

	// AddToViewport 모델 — RemoveFromParent 가 정통 close (stack 밖이라 DeactivateWidget 무효)
	PlayCloseSound();
	RemoveFromParent();
}

// ══════════════ 외부 변화 구독 ══════════════

void UProductionStartPopupWidget::HandleResourceChanged(EResourceType Type, int64 /*NewAmount*/)
{
	// ⚠ OnResourceChanged 는 Money 에도 발화하고, 오피스 수익은 1초 드립이다.
	//   타입 필터가 없으면 팝업이 열려 있는 동안 초당 한 번씩 전 행을 재조회하게 된다.
	if (!FProductRecipeTable::GetAllRawMaterialTypes().Contains(Type)) return;
	if (!OrderMgr) return;

	// 상한/병목만 다시 칠한다 — 이름·커버·선택상태는 건드리지 않는다(스크롤/선택 유지)
	for (UProductionOrderRowWidget* Row : SpawnedRows)
	{
		if (!Row) continue;
		for (const FProductionOrder& Order : OrderMgr->GetAllOrders())
		{
			if (Order.OrderID != Row->GetOrderID()) continue;

			EResourceType Bottleneck = EResourceType::None;
			bool bMaterialBound = false;
			const int32 MaxProducible = OrderMgr->GetMaxProducible(Order, Bottleneck, bMaterialBound);
			Row->UpdateCapacityHint(MaxProducible, Bottleneck, bMaterialBound);
			break;
		}
	}
	RefreshDynamic();
}

void UProductionStartPopupWidget::HandleLineStarted(ECountryType InCountry, FWorldFactoryLineState /*LineState*/)
{
	if (InCountry != Country) return;
	RefreshHeader();
	RefreshDynamic();
}

void UProductionStartPopupWidget::HandleLineCompleted(ECountryType InCountry, int32 /*LineId*/)
{
	if (InCountry != Country) return;
	RefreshHeader();
	RefreshDynamic();
}

void UProductionStartPopupWidget::HandleLineClaimed(ECountryType InCountry, int32 /*LineId*/, int64 /*FinalQty*/)
{
	if (InCountry != Country) return;
	RefreshHeader();
	RefreshDynamic();
}

// ══════════════ 닫기 ══════════════

void UProductionStartPopupWidget::HandleClose()
{
	PlayCloseSound();
	RemoveFromParent();
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
