// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/ProductSellModalWidget.h"
#include "UI/Element/Cards/ItemCardSlotWidget.h"
#include "UI/Element/Trade/CountrySellRowWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/StatRowWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Manager/TradePort.h"
#include "Manager/CountryMarketManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/WorldMapManager.h"
#include "Global/GlobalUtilFunctions.h"
#include "Table/CountryDemandTable.h"
#include "Table/CompanyInfoTable.h"
#include "Table/ProjectDataTable.h"
#include "CommonButtonBase.h"
#include "Groups/CommonButtonGroupBase.h"
#include "Engine/Texture2D.h"
#include "Data/WorldMapTypes.h"
#include "Enum/WidgetType.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "Engine/GameInstance.h"

void UProductSellModalWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// CLAUDE.md 룰: ButtonGroup 셋업은 NativeConstruct 에서 1회. 재진입 시 상태 꼬임 방지.
	SetupIndustryTabGroup();
	SetupSortButtonGroup();
	SetupCountrySortGroup();
}

void UProductSellModalWidget::NativeDestruct()
{
	// 그룹 콜백 해제 — NativeConstruct ↔ NativeDestruct 쌍 (위젯 라이프사이클)
	if (IndustryTabGroup)
	{
		IndustryTabGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}
	if (SortButtonGroup)
	{
		SortButtonGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}
	if (CountrySortGroup)
	{
		CountrySortGroup->OnSelectedButtonBaseChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UProductSellModalWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 매니저 바인딩 우선 — SelectButtonAtIndex 콜백이 TradePort/MarketMgr 참조하므로
	if (UGameInstance* GI = GetGameInstance())
	{
		TradePort = GI->GetSubsystem<UTradePort>();
		MarketMgr = GI->GetSubsystem<UCountryMarketManager>();
	}

	if (CloseButton)
	{
		CloseButton->OnCloseClicked.AddDynamic(this, &UProductSellModalWidget::HandleCloseClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UProductSellModalWidget::HandleDimClicked);
	}
	if (QuantityDecBtn)
	{
		QuantityDecBtn->OnClicked().AddUObject(this, &UProductSellModalWidget::HandleQtyDec);
	}
	if (QuantityIncBtn)
	{
		QuantityIncBtn->OnClicked().AddUObject(this, &UProductSellModalWidget::HandleQtyInc);
	}
	if (QuantityMaxBtn)
	{
		QuantityMaxBtn->OnClicked().AddUObject(this, &UProductSellModalWidget::HandleQtyMax);
	}
	if (QuantityMinBtn)
	{
		QuantityMinBtn->OnClicked().AddUObject(this, &UProductSellModalWidget::HandleQtyMin);
	}
	if (SellButton)
	{
		SellButton->OnClicked().AddUObject(this, &UProductSellModalWidget::HandleSellClicked);
	}

	if (MarketMgr)
	{
		MarketMgr->OnDemandChanged.AddDynamic(this, &UProductSellModalWidget::HandleDemandChanged);
	}

	// 매 활성화 시 default 상태로 리셋 (모달 재진입 시 이전 선택 잔존 방지)
	// 매니저 바인딩 이후에 수행해야 SelectButtonAtIndex 콜백(HandleIndustryTabChanged →
	// RebuildInventoryList → AutoSelectFirstOrSkeleton → RefreshPreview)이 TradePort 참조 가능
	IndustryFilter = ECompanyType::None;
	SortMode = EProductSortMode::Price;  // WBP group 첫 버튼 (SortPriceBtn) 과 일치
	CountrySortMode = ECountrySortMode::Efficiency;
	bInventorySortAscending = false;
	bCountrySortAscending = false;
	if (IndustryTabGroup) IndustryTabGroup->SelectButtonAtIndex(0);
	if (SortButtonGroup) SortButtonGroup->SelectButtonAtIndex(0);
	if (CountrySortGroup) CountrySortGroup->SelectButtonAtIndex(0);
	UpdateSortButtonTexts();  // selected 버튼에 ▼ 표시

	// 그룹 셋업은 NativeConstruct 에서 처리됨 (CLAUDE.md 룰 정합)
	// 첫 활성화에서는 SelectButtonAtIndex 가 no-op 이므로 명시적 갱신 필요
	RebuildInventoryList();
	RebuildCountryList();
	AutoSelectFirstOrSkeleton();
}

void UProductSellModalWidget::NativeOnDeactivated()
{
	if (CloseButton)
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &UProductSellModalWidget::HandleCloseClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UProductSellModalWidget::HandleDimClicked);
	}
	if (QuantityDecBtn) QuantityDecBtn->OnClicked().RemoveAll(this);
	if (QuantityIncBtn) QuantityIncBtn->OnClicked().RemoveAll(this);
	if (QuantityMaxBtn) QuantityMaxBtn->OnClicked().RemoveAll(this);
	if (QuantityMinBtn) QuantityMinBtn->OnClicked().RemoveAll(this);
	if (SellButton)
	{
		SellButton->OnClicked().RemoveAll(this);
	}

	if (MarketMgr)
	{
		MarketMgr->OnDemandChanged.RemoveDynamic(this, &UProductSellModalWidget::HandleDemandChanged);
	}

	// 그룹 콜백 해제는 NativeDestruct 에서 처리 — NativeOnDeactivated 에서 건드리지 않음
	// (CLAUDE.md 룰: NativeOnActivated/Deactivated 사이클에서 ButtonGroup 상태 안 건드림)

	// 카드의 BindLambda 는 명시적 Unbind 불필요 — 카드 GC 시 자동 정리
	SpawnedCardOriginalIndices.Reset();
	SpawnedItemCards.Reset();
	for (UCountrySellRowWidget* Row : SpawnedCountryRows)
	{
		if (Row) { Row->OnRowClicked.RemoveDynamic(this, &UProductSellModalWidget::HandleCountryRowClicked); }
	}
	// 11국 영역 정리 — 다음 모달 진입 시 RebuildCountryList 가 새로 생성
	if (CountryScrollBox) { CountryScrollBox->ClearChildren(); }
	SpawnedCountryRows.Reset();

	Super::NativeOnDeactivated();
}

void UProductSellModalWidget::SetItems(const TArray<FSellableItem>& InItems)
{
	Items = InItems;
	SelectedItemIndex = INDEX_NONE;
	SelectedQuantity = 0;
	RebuildInventoryList();
	RebuildCountryList();
	AutoSelectFirstOrSkeleton();
}

void UProductSellModalWidget::RebuildInventoryList()
{
	if (!InventoryContainer) return;

	SpawnedItemCards.Reset();
	SpawnedCardOriginalIndices.Reset();
	InventoryContainer->ClearChildren();

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	// 1. 필터링 — IndustryFilter == None 이면 전체
	TArray<int32> Visible;
	Visible.Reserve(Items.Num());
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		if (IndustryFilter == ECompanyType::None || Items[i].Industry == IndustryFilter)
		{
			Visible.Add(i);
		}
	}

	// 2. 정렬 — Default 면 원본 순서 유지. bInventorySortAscending 으로 방향 토글 (default 내림차순).
	const bool bAsc = bInventorySortAscending;
	switch (SortMode)
	{
	case EProductSortMode::Price:
		Visible.Sort([this, bAsc](int32 A, int32 B)
		{
			return bAsc ? Items[A].BasePrice < Items[B].BasePrice : Items[A].BasePrice > Items[B].BasePrice;
		});
		break;
	case EProductSortMode::Quantity:
		Visible.Sort([this, bAsc](int32 A, int32 B)
		{
			return bAsc ? Items[A].Quantity < Items[B].Quantity : Items[A].Quantity > Items[B].Quantity;
		});
		break;
	case EProductSortMode::Name:
		Visible.Sort([this, bAsc](int32 A, int32 B)
		{
			if (Items[A].Industry == Items[B].Industry)
			{
				return bAsc ? Items[A].ProjectIndex < Items[B].ProjectIndex : Items[A].ProjectIndex > Items[B].ProjectIndex;
			}
			const int32 IA = static_cast<int32>(Items[A].Industry);
			const int32 IB = static_cast<int32>(Items[B].Industry);
			return bAsc ? IA < IB : IA > IB;
		});
		break;
	case EProductSortMode::Default:
	default:
		break;
	}

	// 카드 그리드 영역의 빈 안내 — 필터/인벤 상태별 메시지
	if (InventoryEmptyText)
	{
		if (Items.Num() == 0)
		{
			InventoryEmptyText->SetText(FText::FromString(TEXT("창고가 비어 있습니다 ― 물품을 생산하면 이곳에 표시됩니다")));
			InventoryEmptyText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else if (Visible.Num() == 0)
		{
			InventoryEmptyText->SetText(FText::FromString(TEXT("이 산업에는 보유한 물품이 없습니다")));
			InventoryEmptyText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			InventoryEmptyText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	TSubclassOf<UUserWidget> CardClass = TableMgr->GetWidgetClass(EWidgetType::ItemCardQtyBelow);
	if (!CardClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ProductSellModal] ItemCardQtyBelow widget class not registered"));
		return;
	}

	// 3. 카드 생성 — 표시 순서대로, 각 카드는 원본 Items 인덱스 캡쳐
	for (int32 OriginalIdx : Visible)
	{
		UItemCardSlotWidget* Card = CreateWidget<UItemCardSlotWidget>(this, CardClass);
		if (!Card) continue;

		// 프로젝트 아이콘 DT lookup
		UTexture2D* IconTex = nullptr;
		bool bOk = false;
		const FProjectData PInfo = TableMgr->GetProjectData(Items[OriginalIdx].Industry, Items[OriginalIdx].ProjectIndex, bOk);
		if (bOk && !PInfo.Icon.IsNull())
		{
			IconTex = PInfo.Icon.LoadSynchronous();
		}

		Card->SetItem(IconTex, static_cast<int32>(Items[OriginalIdx].Quantity));
		// Product 이미지는 384x288 = 4:3. 카드 외곽 144 기준 동일 비율(144x108) 적용.
		Card->SetIconSize(FVector2D(144.0f, 108.0f));

		// 클릭 핸들러: 원본 인덱스 캡쳐
		const int32 CapturedIdx = OriginalIdx;
		Card->OnItemCardClicked.BindLambda([this, CapturedIdx](FName) { OnItemClickedByIndex(CapturedIdx); });

		InventoryContainer->AddChild(Card);
		SpawnedItemCards.Add(Card);
		SpawnedCardOriginalIndices.Add(OriginalIdx);
	}
}

void UProductSellModalWidget::RebuildCountryList()
{
	if (!CountryScrollBox) return;

	const FSellableItem* Selected = GetSelectedItem();
	const ECompanyType Industry = Selected ? Selected->Industry : ECompanyType::None;

	// 이미 생성된 Row 가 있으면 — 위젯 재활용 + SetData 만 호출 (Industry 갱신 + 매니저 셀 재구독은 SetData 가 처리).
	// 매번 destroy/construct 안 함 → 깜빡임 0 + 매니저 재구독 비용 0 + push 기반 즉각 갱신.
	if (SpawnedCountryRows.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ProductSellModal] RebuildCountryList RECYCLE — Industry=%d, Rows=%d"),
			(int32)Industry, SpawnedCountryRows.Num());
		for (UCountrySellRowWidget* Row : SpawnedCountryRows)
		{
			if (!Row) continue;
			Row->SetData(Row->GetCountry(), Industry);
			Row->SetSelected(Row->GetCountry() == SelectedCountry);
			Row->OnRowClicked.AddDynamic(this, &UProductSellModalWidget::HandleCountryRowClicked);  // 재구독 (AddDynamic dedup 자동)
		}
		RefreshAllEfficiencies();
		SortCountriesByMode();
		return;
	}

	// 첫 진입 — 11개 위젯 신규 생성.
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	TSubclassOf<UUserWidget> RowClass = TableMgr->GetWidgetClass(EWidgetType::CountrySellRow);
	if (!RowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ProductSellModal] CountrySellRow widget class not registered"));
		return;
	}

	const TArray<FCountryInfoTable> AllCountries = TableMgr->GetAllCountryInfos();
	for (const FCountryInfoTable& CountryInfo : AllCountries)
	{
		UCountrySellRowWidget* Row = CreateWidget<UCountrySellRowWidget>(this, RowClass);
		if (!Row) continue;

		Row->SetData(CountryInfo.CountryType, Industry);
		Row->SetSelected(CountryInfo.CountryType == SelectedCountry);
		Row->OnRowClicked.AddDynamic(this, &UProductSellModalWidget::HandleCountryRowClicked);

		CountryScrollBox->AddChild(Row);
		SpawnedCountryRows.Add(Row);
	}

	RefreshAllEfficiencies();
	SortCountriesByMode();
}

void UProductSellModalWidget::OnItemClickedByIndex(int32 ItemIndex)
{
	UE_LOG(LogTemp, Warning, TEXT("[Modal] OnItemClickedByIndex — ItemIndex=%d, Items.Num=%d"),
		ItemIndex, Items.Num());
	if (!Items.IsValidIndex(ItemIndex)) return;

	SelectedItemIndex = ItemIndex;
	SetSelectedQuantity(1);  // 기본 1개로 시작

	// 라디오 패턴: SpawnedCardOriginalIndices[k] 가 클릭된 원본 인덱스와 일치하는 카드만 ON
	for (int32 k = 0; k < SpawnedItemCards.Num(); ++k)
	{
		if (SpawnedItemCards[k] && SpawnedCardOriginalIndices.IsValidIndex(k))
		{
			SpawnedItemCards[k]->SetSelected(SpawnedCardOriginalIndices[k] == ItemIndex);
		}
	}

	// 선택 아이템의 산업으로 11국 라디오 게이지 갱신
	RebuildCountryList();
	RefreshSelectedItemUI();
	RefreshPreview();
}

void UProductSellModalWidget::OnCountryClicked(UCountrySellRowWidget* Row)
{
	if (!Row) return;

	SelectedCountry = Row->GetCountry();
	for (UCountrySellRowWidget* R : SpawnedCountryRows)
	{
		if (R) { R->SetSelected(R == Row); }
	}
	RefreshPreview();
}

void UProductSellModalWidget::RefreshSelectedItemUI()
{
	const FSellableItem* Item = GetSelectedItem();

	// 빈 상태 (Item == nullptr) 통합 처리 — 안내 멘트 + 슬롯 hide + 컨트롤 disable
	SetEmptyState(Item == nullptr);

	if (!Item) return;

	// 선택 정보 lookup — 한 번만, 모든 슬롯에서 공유
	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	bool bProjOk = false;
	FProjectData PInfo;
	if (TableMgr)
	{
		PInfo = TableMgr->GetProjectData(Item->Industry, Item->ProjectIndex, bProjOk);
	}

	// 아이콘 (소형/대형 둘 다 동일 텍스처)
	UTexture2D* IconTex = nullptr;
	if (bProjOk && !PInfo.Icon.IsNull())
	{
		IconTex = PInfo.Icon.LoadSynchronous();
	}
	if (SelectedIconImage)
	{
		if (IconTex) { SelectedIconImage->SetBrushFromTexture(IconTex); SelectedIconImage->SetVisibility(ESlateVisibility::HitTestInvisible); }
		else { SelectedIconImage->SetVisibility(ESlateVisibility::Hidden); }
	}
	if (LargeSelectedIconImage)
	{
		if (IconTex) { LargeSelectedIconImage->SetBrushFromTexture(IconTex); LargeSelectedIconImage->SetVisibility(ESlateVisibility::HitTestInvisible); }
		else { LargeSelectedIconImage->SetVisibility(ESlateVisibility::Hidden); }
	}

	// 이름: DisplayName > ProjectName > fallback
	if (SelectedNameText)
	{
		FText Name = Item->DisplayName;
		if (Name.IsEmpty() && bProjOk && !PInfo.ProjectName.IsEmpty())
		{
			Name = PInfo.ProjectName;
		}
		if (Name.IsEmpty())
		{
			// 산업명 = DT_CompanyInfo 단일 진실 (구 enum DisplayName 은 UMETA 가 에디터 전용이라 패키징 빌드에서 "Game" 으로 떨어졌다)
			bool bCompanyOk = false;
			const FCompanyInfoTable Company = TableMgr ? TableMgr->GetCompanyInfo(Item->Industry, bCompanyOk) : FCompanyInfoTable();
			const FText Industry = bCompanyOk ? Company.DisplayName : FText::GetEmpty();
			Name = FText::FromString(FString::Printf(TEXT("%s T%d"), *Industry.ToString(), Item->ProjectIndex));
		}
		SelectedNameText->SetText(Name);
	}

	if (SelectedHoldingText)
	{
		SelectedHoldingText->SetStatValue(FString::Printf(TEXT("%lld개"), Item->Quantity));
	}
	if (SelectedPriceText)
	{
		SelectedPriceText->SetStatValue(FString::Printf(TEXT("%s원/개"),
			*UGlobalUtilFunctions::AbbreviateNumber(Item->BasePrice, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil).ToString()));
	}

	// 등급 — EQualityGrade enum DisplayName
	if (SelectedGradeText)
	{
		// EQualityGrade 는 UMETA DisplayName 이 식별자와 같아("F"/"D"/.../"S") 패키징 빌드 폴백값이 동일 — 여기만 enum 조회 허용.
		// ⚠ UMETA 를 "F등급" 처럼 바꾸면 그 순간 패키징에서 "F" 로 떨어진다 → 그때는 다른 enum 들처럼 헬퍼로 옮길 것.
		const UEnum* GradeEnum = StaticEnum<EQualityGrade>();
		const FText GradeText = GradeEnum ? GradeEnum->GetDisplayNameTextByValue(static_cast<int64>(Item->Grade)) : FText::GetEmpty();
		SelectedGradeText->SetText(FText::FromString(FString::Printf(TEXT("[%s 등급]"), *GradeText.ToString())));
	}

	// 총 자산 = 단가 x 보유 (등급 multiplier 는 판매 시점에 적용 — 여기선 단순 BasePrice 기준)
	if (SelectedTotalValueText)
	{
		const int64 Total = Item->BasePrice * Item->Quantity;
		SelectedTotalValueText->SetStatValue(FString::Printf(TEXT("%s원"),
			*UGlobalUtilFunctions::AbbreviateNumber(Total, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString()));
	}

	// 설명: DT_Project.SubName 활용
	if (SelectedDescriptionText)
	{
		const FText Desc = (bProjOk && !PInfo.SubName.IsEmpty()) ? PInfo.SubName : FText::GetEmpty();
		SelectedDescriptionText->SetText(Desc);
	}

	if (QuantityInputBox)
	{
		QuantityInputBox->SetText(FText::FromString(FString::Printf(TEXT("%lld"), SelectedQuantity)));
	}
}

void UProductSellModalWidget::RefreshPreview()
{
	const FSellableItem* Item = GetSelectedItem();

	int64 Money = 0;
	int64 MarketCap = 0;
	int64 SoldQty = 0;

	if (Item && SelectedCountry != ECountryType::None && SelectedQuantity > 0 && TradePort)
	{
		const FSellResult Result = TradePort->PreviewSell(SelectedCountry, Item->Industry, Item->ProjectIndex, SelectedQuantity);
		Money = Result.MoneyGained;
		MarketCap = Result.MarketCapGained;
		SoldQty = Result.QuantitySold;
	}

	if (PreviewMoneyText)
	{
		PreviewMoneyText->SetStatValue(FString::Printf(TEXT("%s원"),
			*UGlobalUtilFunctions::AbbreviateNumber(Money, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor).ToString()));
	}
	if (PreviewMarketCapText)
	{
		PreviewMarketCapText->SetStatValue(FString::Printf(TEXT("+%lld MC"), MarketCap));
	}
	if (PreviewQuantityText)
	{
		PreviewQuantityText->SetStatValue(FString::Printf(TEXT("%lld개"), SoldQty));
	}
}

void UProductSellModalWidget::RefreshAllEfficiencies()
{
	const FSellableItem* Item = GetSelectedItem();
	if (!Item || !TradePort)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Modal] RefreshAllEfficiencies EARLY — Item=%d TradePort=%d"), Item ? 1 : 0, TradePort ? 1 : 0);
		return;
	}

	// 11국 각각 PreviewSell → MoneyGained 평균 → 평균 대비 ±%
	struct FRowResult { UCountrySellRowWidget* Row; int64 Money; };
	TArray<FRowResult> Results;
	int64 TotalMoney = 0;
	const int64 PreviewQty = SelectedQuantity > 0 ? SelectedQuantity : 1;

	for (UCountrySellRowWidget* Row : SpawnedCountryRows)
	{
		if (!Row) continue;
		const FSellResult Result = TradePort->PreviewSell(Row->GetCountry(), Item->Industry, Item->ProjectIndex, PreviewQty);
		Results.Add({ Row, Result.MoneyGained });
		TotalMoney += Result.MoneyGained;
		UE_LOG(LogTemp, Warning, TEXT("[Modal] PreviewSell Country=%d Industry=%d Project=%d Qty=%lld → Money=%lld"),
			(int32)Row->GetCountry(), (int32)Item->Industry, Item->ProjectIndex, PreviewQty, Result.MoneyGained);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Modal] RefreshAllEfficiencies Total=%lld Count=%d"), TotalMoney, Results.Num());
	if (Results.Num() == 0 || TotalMoney <= 0) return;
	const double Average = static_cast<double>(TotalMoney) / Results.Num();

	for (const FRowResult& R : Results)
	{
		const float Delta = Average > 0.0
			? static_cast<float>((static_cast<double>(R.Money) - Average) / Average * 100.0)
			: 0.0f;
		UE_LOG(LogTemp, Warning, TEXT("[Modal] SetDelta Country=%d Delta=%.2f"), (int32)R.Row->GetCountry(), Delta);
		R.Row->SetEfficiencyDelta(Delta);
	}
}

void UProductSellModalWidget::SetSelectedQuantity(int64 NewQty)
{
	const FSellableItem* Item = GetSelectedItem();
	const int64 Max = Item ? Item->Quantity : 0;
	SelectedQuantity = FMath::Clamp<int64>(NewQty, 0, Max);

	if (QuantityInputBox)
	{
		QuantityInputBox->SetText(FText::FromString(FString::Printf(TEXT("%lld"), SelectedQuantity)));
	}
	RefreshPreview();
	RefreshAllEfficiencies();
}

void UProductSellModalWidget::HandleSellClicked()
{
	const FSellableItem* Item = GetSelectedItem();
	if (!Item || SelectedCountry == ECountryType::None || SelectedQuantity <= 0 || !TradePort)
	{
		return;
	}

	const FSellResult Result = TradePort->SellProduct(SelectedCountry, Item->Industry, Item->ProjectIndex, SelectedQuantity);
	if (Result.QuantitySold <= 0) return;

	// 모달의 인벤 캐시 동기화 — 외부 인벤 시스템과 합칠 때까지의 임시 처리
	if (Items.IsValidIndex(SelectedItemIndex))
	{
		Items[SelectedItemIndex].Quantity -= Result.QuantitySold;
		if (Items[SelectedItemIndex].Quantity <= 0)
		{
			Items.RemoveAt(SelectedItemIndex);
			SelectedItemIndex = INDEX_NONE;
		}
	}

	SetSelectedQuantity(0);
	RebuildInventoryList();

	// 정책 B: 판매 후 마지막 1개 소진으로 SelectedItemIndex 가 INDEX_NONE 이면
	// 인벤이 남아있는 한 표시 순서 첫 카드 자동 선택, 0 이면 빈 상태.
	AutoSelectFirstOrSkeleton();
}

void UProductSellModalWidget::HandleQtyDec()
{
	SetSelectedQuantity(SelectedQuantity - 1);
}

void UProductSellModalWidget::HandleQtyInc()
{
	SetSelectedQuantity(SelectedQuantity + 1);
}

void UProductSellModalWidget::HandleQtyMax()
{
	const FSellableItem* Item = GetSelectedItem();
	SetSelectedQuantity(Item ? Item->Quantity : 0);
}

void UProductSellModalWidget::HandleQtyMin()
{
	SetSelectedQuantity(1);
}

void UProductSellModalWidget::HandleCloseClicked()
{
	DeactivateWidget();
}

void UProductSellModalWidget::HandleDimClicked()
{
	DeactivateWidget();
}

void UProductSellModalWidget::HandleCountryRowClicked(UCountrySellRowWidget* Row)
{
	OnCountryClicked(Row);
}

void UProductSellModalWidget::HandleDemandChanged(ECountryType Country, ECompanyType Industry, float NewRatio)
{
	// 게이지는 각 행이 자체 구독으로 갱신. 모달은 효율% 만 재계산.
	const FSellableItem* Item = GetSelectedItem();
	if (Item && Item->Industry == Industry)
	{
		RefreshAllEfficiencies();
		RefreshPreview();
	}
}

const FSellableItem* UProductSellModalWidget::GetSelectedItem() const
{
	return Items.IsValidIndex(SelectedItemIndex) ? &Items[SelectedItemIndex] : nullptr;
}

void UProductSellModalWidget::AutoSelectFirstOrSkeleton()
{
	// 표시 카드가 있으면 첫 번째 카드의 원본 인덱스로 자동 선택.
	// 필터 결과 0 인데 인벤은 있는 경우엔 SelectedItemIndex 유지 (마지막 선택 유지).
	if (SpawnedCardOriginalIndices.Num() > 0)
	{
		OnItemClickedByIndex(SpawnedCardOriginalIndices[0]);
	}
	else if (Items.Num() == 0)
	{
		// 진짜 인벤 0 → 빈 상태
		SelectedItemIndex = INDEX_NONE;
		RefreshSelectedItemUI();
		RefreshPreview();
	}
	// else: 필터 결과 0, 인벤은 있음 → 마지막 선택 유지 (좌하단/우측 그대로)
}

void UProductSellModalWidget::SetEmptyState(bool bShow)
{
	// 컨트롤 enable/disable (Switcher 사용 여부 무관 — 항상 적용)
	const bool bEnabled = !bShow;
	if (QuantityDecBtn) QuantityDecBtn->SetIsEnabled(bEnabled);
	if (QuantityIncBtn) QuantityIncBtn->SetIsEnabled(bEnabled);
	if (QuantityMaxBtn) QuantityMaxBtn->SetIsEnabled(bEnabled);
	if (QuantityMinBtn) QuantityMinBtn->SetIsEnabled(bEnabled);
	if (QuantityInputBox) QuantityInputBox->SetIsEnabled(bEnabled);
	if (SellButton) SellButton->SetIsEnabled(bEnabled);

	// Switcher 우선: Index 0 = 정상 정보, Index 1 = 빈 상태 안내
	if (SelectedInfoSwitcher)
	{
		SelectedInfoSwitcher->SetActiveWidgetIndex(bShow ? 1 : 0);
		return;
	}

	// Fallback (Switcher 없을 때): 개별 위젯 Visibility 토글 + 안내 텍스트
	if (EmptyStateHintText)
	{
		EmptyStateHintText->SetText(FText::FromString(TEXT("물품을 선택하면 상세 정보가 표시됩니다")));
		EmptyStateHintText->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	const ESlateVisibility SlotVis = bShow ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
	if (SelectedIconImage)       SelectedIconImage->SetVisibility(bShow ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	if (LargeSelectedIconImage)  LargeSelectedIconImage->SetVisibility(bShow ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	if (SelectedHoldingText)     SelectedHoldingText->SetVisibility(SlotVis);
	if (SelectedPriceText)       SelectedPriceText->SetVisibility(SlotVis);
	if (SelectedGradeText)       SelectedGradeText->SetVisibility(SlotVis);
	if (SelectedTotalValueText)  SelectedTotalValueText->SetVisibility(SlotVis);
	if (SelectedDescriptionText) SelectedDescriptionText->SetVisibility(SlotVis);

	if (SelectedNameText)
	{
		if (EmptyStateHintText)
		{
			SelectedNameText->SetVisibility(SlotVis);
		}
		else
		{
			SelectedNameText->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (bShow)
			{
				SelectedNameText->SetText(FText::FromString(TEXT("선택된 물품이 없습니다")));
			}
		}
	}
}

void UProductSellModalWidget::SetupIndustryTabGroup()
{
	IndustryTabGroup = NewObject<UCommonButtonGroupBase>(this);
	if (!IndustryTabGroup) return;

	IndustryTabGroup->SetSelectionRequired(true);

	if (TabAllBtn)           { IndustryTabGroup->AddWidget(TabAllBtn);           TabAllBtn->SetIsSelectable(true); }
	if (TabElectronicsBtn)   { IndustryTabGroup->AddWidget(TabElectronicsBtn);   TabElectronicsBtn->SetIsSelectable(true); }
	if (TabSemiconductorBtn) { IndustryTabGroup->AddWidget(TabSemiconductorBtn); TabSemiconductorBtn->SetIsSelectable(true); }
	if (TabAutomobileBtn)    { IndustryTabGroup->AddWidget(TabAutomobileBtn);    TabAutomobileBtn->SetIsSelectable(true); }

	IndustryTabGroup->OnSelectedButtonBaseChanged.AddDynamic(this, &UProductSellModalWidget::HandleIndustryTabChanged);
	IndustryTabGroup->SelectButtonAtIndex(0);  // 전체 탭 기본
}

void UProductSellModalWidget::SetupSortButtonGroup()
{
	SortButtonGroup = NewObject<UCommonButtonGroupBase>(this);
	if (!SortButtonGroup) return;

	SortButtonGroup->SetSelectionRequired(true);

	// 라디오 UX: group + AddWidget + SelectionRequired → selection 자동 관리.
	// SetIsInteractableWhenSelected(true) 필수 — selected + !bToggleable 이면 RootButton interaction 자동 disabled 라
	// 같은 selected 버튼 재클릭 시 OnPressed 발화 안 함. 이 플래그로 selected 상태에서도 클릭 받게.
	auto Setup = [this](UButtonWidget* Btn, void (UProductSellModalWidget::*Handler)())
	{
		if (!Btn) return;
		Btn->SetIsSelectable(true);
		Btn->SetIsInteractableWhenSelected(true);
		SortButtonGroup->AddWidget(Btn);
		Btn->OnPressed().AddUObject(this, Handler);
	};
	Setup(SortDefaultBtn,  &UProductSellModalWidget::HandleSortDefaultClicked);
	Setup(SortPriceBtn,    &UProductSellModalWidget::HandleSortPriceClicked);
	Setup(SortQuantityBtn, &UProductSellModalWidget::HandleSortQuantityClicked);
	Setup(SortNameBtn,     &UProductSellModalWidget::HandleSortNameClicked);
	SortButtonGroup->SelectButtonAtIndex(0);  // Default 기본
	UpdateSortButtonTexts();
}

void UProductSellModalWidget::SetupCountrySortGroup()
{
	CountrySortGroup = NewObject<UCommonButtonGroupBase>(this);
	if (!CountrySortGroup) return;

	CountrySortGroup->SetSelectionRequired(true);

	UE_LOG(LogTemp, Warning, TEXT("[Modal] SetupCountrySortGroup — SortEfficiencyBtn=%d, SortDemandBtn=%d"),
		SortEfficiencyBtn ? 1 : 0, SortDemandBtn ? 1 : 0);

	// 라디오 UX + SetIsInteractableWhenSelected(true) 로 selected 버튼 재클릭도 OnPressed 발화.
	auto Setup = [this](UButtonWidget* Btn, void (UProductSellModalWidget::*Handler)())
	{
		if (!Btn) return;
		Btn->SetIsSelectable(true);
		Btn->SetIsInteractableWhenSelected(true);
		CountrySortGroup->AddWidget(Btn);
		Btn->OnPressed().AddUObject(this, Handler);
	};
	Setup(SortEfficiencyBtn, &UProductSellModalWidget::HandleSortEfficiencyClicked);
	Setup(SortDemandBtn,     &UProductSellModalWidget::HandleSortDemandClicked);

	CountrySortGroup->SelectButtonAtIndex(0);  // Efficiency 기본
	UpdateSortButtonTexts();
}

void UProductSellModalWidget::HandleCountrySortChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	UE_LOG(LogTemp, Warning, TEXT("[Modal] HandleCountrySortChanged — ButtonIndex=%d, IsEfficiency=%d, IsDemand=%d"),
		ButtonIndex,
		SelectedButton == SortEfficiencyBtn ? 1 : 0,
		SelectedButton == SortDemandBtn ? 1 : 0);

	if (SelectedButton == SortEfficiencyBtn)   CountrySortMode = ECountrySortMode::Efficiency;
	else if (SelectedButton == SortDemandBtn)  CountrySortMode = ECountrySortMode::Demand;
	else return;

	SortCountriesByMode();
}

void UProductSellModalWidget::SortCountriesByMode()
{
	if (!CountryScrollBox || SpawnedCountryRows.Num() == 0) return;

	const FSellableItem* Item = GetSelectedItem();
	const bool bHasItem = Item != nullptr;

	// 카드 미선택 시 Efficiency 모드는 의미 없음 — Demand 로 fallback
	const ECountrySortMode EffectiveMode = (bHasItem && CountrySortMode == ECountrySortMode::Efficiency)
		? ECountrySortMode::Efficiency
		: ECountrySortMode::Demand;

	// 정렬 키 계산 (Row → float, 큰 값이 우선)
	TMap<UCountrySellRowWidget*, float> SortKeys;
	const int64 PreviewQty = SelectedQuantity > 0 ? SelectedQuantity : 1;

	for (UCountrySellRowWidget* Row : SpawnedCountryRows)
	{
		if (!Row) continue;
		float Key = 0.0f;

		if (EffectiveMode == ECountrySortMode::Efficiency && TradePort && bHasItem)
		{
			const FSellResult Result = TradePort->PreviewSell(Row->GetCountry(), Item->Industry, Item->ProjectIndex, PreviewQty);
			Key = static_cast<float>(Result.MoneyGained);
		}
		else if (MarketMgr)
		{
			// Demand 모드: Current 절대값 비교 (사우디 9000 vs 일본 15000 같은 직관적 정렬).
			// Ratio (0~1) 는 시장 크기 무시하므로 작은 시장의 만수요 = 큰 시장의 절반과 같은 키 → 부적합.
			if (bHasItem)
			{
				bool bFound = false;
				const FCountryMarketState State = MarketMgr->GetMarketState(Row->GetCountry(), Item->Industry, bFound);
				Key = bFound ? static_cast<float>(State.Current) : 0.0f;
			}
			else
			{
				int64 Total = 0;
				int32 Count = 0;
				for (const FCountryMarketState& S : MarketMgr->GetAllStates())
				{
					if (S.Country == Row->GetCountry())
					{
						Total += S.Current;
						++Count;
					}
				}
				Key = Count > 0 ? static_cast<float>(Total) / Count : 0.0f;
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("[Modal] SortKey Country=%d Mode=%d Key=%.2f bHasItem=%d"),
			(int32)Row->GetCountry(), (int32)EffectiveMode, Key, bHasItem ? 1 : 0);
		SortKeys.Add(Row, Key);
	}

	// bCountrySortAscending 으로 방향 토글 (default 내림차순 = 높은값 우선)
	const bool bAsc = bCountrySortAscending;
	SpawnedCountryRows.Sort([&SortKeys, bAsc](const TObjectPtr<UCountrySellRowWidget>& A, const TObjectPtr<UCountrySellRowWidget>& B)
	{
		const float KeyA = SortKeys.FindRef(A);
		const float KeyB = SortKeys.FindRef(B);
		return bAsc ? KeyA < KeyB : KeyA > KeyB;
	});

	UE_LOG(LogTemp, Warning, TEXT("[Modal] SortCountriesByMode AFTER SORT — order:"));
	for (int32 i = 0; i < SpawnedCountryRows.Num(); ++i)
	{
		if (SpawnedCountryRows[i])
		{
			UE_LOG(LogTemp, Warning, TEXT("  [%d] Country=%d"), i, (int32)SpawnedCountryRows[i]->GetCountry());
		}
	}

	// ScrollBox 자식 순서 재배치 — ShiftChild 는 ScrollBox 에서 Slate 측 전파 누락이 있어
	// ClearChildren + AddChild 순서대로 재추가. SpawnedCountryRows 의 강한 참조가 있어
	// 위젯 객체는 유지되지만 SWidget 재attach 가 NativeConstruct 재호출 트리거 →
	// ApplyStaticTexts 가 EfficiencyText 를 "-" 로 리셋. RefreshAllEfficiencies 로 복원.
	CountryScrollBox->ClearChildren();
	for (UCountrySellRowWidget* Row : SpawnedCountryRows)
	{
		if (Row)
		{
			CountryScrollBox->AddChild(Row);
		}
	}
	RefreshAllEfficiencies();
}

// 정렬 토글 핸들러 — 같은 모드 재클릭 시 방향 토글, 다른 모드 클릭 시 mode 변경 + default 내림차순.
// OnClicked 단일 경로로 OnSelectedButtonBaseChanged 와의 순서 의존성 회피.

// 정렬 토글 핸들러 — group 이 selection 자체 관리 (라디오 UX). 우리는 mode + bool 만 변경.
// 같은 모드 재클릭 → ascending 토글, 다른 모드 클릭 → mode 변경 + ascending=false (default 내림).
// OnPressed (raw) 가 selected 버튼 재클릭 시에도 발화하므로 같은 모드 감지 가능.

void UProductSellModalWidget::HandleSortDefaultClicked()
{
	if (SortMode == EProductSortMode::Default) { bInventorySortAscending = !bInventorySortAscending; }
	else { SortMode = EProductSortMode::Default; bInventorySortAscending = false; }
	UpdateSortButtonTexts();
	RebuildInventoryList();
}

void UProductSellModalWidget::HandleSortPriceClicked()
{
	if (SortMode == EProductSortMode::Price) { bInventorySortAscending = !bInventorySortAscending; }
	else { SortMode = EProductSortMode::Price; bInventorySortAscending = false; }
	UpdateSortButtonTexts();
	RebuildInventoryList();
}

void UProductSellModalWidget::HandleSortQuantityClicked()
{
	if (SortMode == EProductSortMode::Quantity) { bInventorySortAscending = !bInventorySortAscending; }
	else { SortMode = EProductSortMode::Quantity; bInventorySortAscending = false; }
	UpdateSortButtonTexts();
	RebuildInventoryList();
}

void UProductSellModalWidget::HandleSortNameClicked()
{
	if (SortMode == EProductSortMode::Name) { bInventorySortAscending = !bInventorySortAscending; }
	else { SortMode = EProductSortMode::Name; bInventorySortAscending = false; }
	UpdateSortButtonTexts();
	RebuildInventoryList();
}

void UProductSellModalWidget::HandleSortEfficiencyClicked()
{
	if (CountrySortMode == ECountrySortMode::Efficiency) { bCountrySortAscending = !bCountrySortAscending; }
	else { CountrySortMode = ECountrySortMode::Efficiency; bCountrySortAscending = false; }
	UpdateSortButtonTexts();
	SortCountriesByMode();
}

void UProductSellModalWidget::HandleSortDemandClicked()
{
	if (CountrySortMode == ECountrySortMode::Demand) { bCountrySortAscending = !bCountrySortAscending; }
	else { CountrySortMode = ECountrySortMode::Demand; bCountrySortAscending = false; }
	UpdateSortButtonTexts();
	SortCountriesByMode();
}

void UProductSellModalWidget::UpdateSortButtonTexts()
{
	// 게임 UI 표준 컨벤션 (Excel/Steam/MMORPG):
	// 오름차순 (작은→큰, A→Z) → ▲
	// 내림차순 (큰→작, Z→A, default) → ▼
	auto ApplyText = [](UButtonWidget* Btn, bool bIsSelected, bool bAsc, const FString& Label)
	{
		if (!Btn) return;
		const FString Suffix = bIsSelected ? (bAsc ? FString(TEXT(" ▲")) : FString(TEXT(" ▼"))) : FString();
		Btn->SetButtonText(FText::FromString(Label + Suffix));
		Btn->SetLetterSpacing(-3);
	};

	ApplyText(SortDefaultBtn,    SortMode == EProductSortMode::Default,  bInventorySortAscending, TEXT("기본순"));
	ApplyText(SortPriceBtn,      SortMode == EProductSortMode::Price,    bInventorySortAscending, TEXT("가격순"));
	ApplyText(SortQuantityBtn,   SortMode == EProductSortMode::Quantity, bInventorySortAscending, TEXT("보유순"));
	ApplyText(SortNameBtn,       SortMode == EProductSortMode::Name,     bInventorySortAscending, TEXT("이름순"));
	ApplyText(SortEfficiencyBtn, CountrySortMode == ECountrySortMode::Efficiency, bCountrySortAscending, TEXT("효율순"));
	ApplyText(SortDemandBtn,     CountrySortMode == ECountrySortMode::Demand,     bCountrySortAscending, TEXT("수요순"));
}

void UProductSellModalWidget::HandleIndustryTabChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	// 어떤 버튼이 클릭됐는지로 분기 (WBP 에서 일부 탭 omit 해도 의미 일관)
	if (SelectedButton == TabAllBtn)                 IndustryFilter = ECompanyType::None;
	else if (SelectedButton == TabElectronicsBtn)    IndustryFilter = ECompanyType::Electronics;
	else if (SelectedButton == TabSemiconductorBtn)  IndustryFilter = ECompanyType::Semiconductor;
	else if (SelectedButton == TabAutomobileBtn)     IndustryFilter = ECompanyType::Automobile;
	else return;

	RebuildInventoryList();
	AutoSelectFirstOrSkeleton();
}

void UProductSellModalWidget::HandleSortChanged(UCommonButtonBase* SelectedButton, int32 ButtonIndex)
{
	if (SelectedButton == SortDefaultBtn)       SortMode = EProductSortMode::Default;
	else if (SelectedButton == SortPriceBtn)    SortMode = EProductSortMode::Price;
	else if (SelectedButton == SortQuantityBtn) SortMode = EProductSortMode::Quantity;
	else if (SelectedButton == SortNameBtn)     SortMode = EProductSortMode::Name;
	else return;

	RebuildInventoryList();
	AutoSelectFirstOrSkeleton();
}
