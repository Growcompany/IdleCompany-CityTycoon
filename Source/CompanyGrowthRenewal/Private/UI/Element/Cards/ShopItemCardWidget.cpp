#include "UI/Element/Cards/ShopItemCardWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UI/Element/Common/IconBrushUtils.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Element/Cards/ShopBundleRowWidget.h"
#include "Enum/WidgetType.h"
#include "Manager/TableManagerSubsystem.h"

void UShopItemCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BuyButton)
	{
		BuyButton->OnClicked().AddUObject(this, &UShopItemCardWidget::HandleBuyClicked);
	}

	// 자식 위젯 구성이 끝난 뒤라 여기서 입힌 값은 덮이지 않는다
	if (!RowName.IsNone())
	{
		ApplyBuyButton();
		if (bHasState)
		{
			RefreshState(CachedRemainingCount, bCachedCanAfford);
		}
	}
}

void UShopItemCardWidget::NativeDestruct()
{
	if (BuyButton)
	{
		BuyButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UShopItemCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bEntrancePlaying)
	{
		if (EntranceDelayRemaining > 0.f)
		{
			EntranceDelayRemaining -= InDeltaTime;
		}
		else
		{
			EntranceElapsed += InDeltaTime;
			const float T = FMath::Clamp(EntranceElapsed / EntranceDuration, 0.f, 1.f);
			const float Eased = FWidgetAnimationUtils::EaseOutQuad(T);
			SetRenderOpacity(Eased);
			SetRenderTranslation(FVector2D(0.f, EntranceRise * (1.f - Eased)));
			if (T >= 1.f)
			{
				bEntrancePlaying = false;
				SetRenderOpacity(1.f);
				SetRenderTranslation(FVector2D::ZeroVector);
			}
		}
	}

	if (PurchasePunch.IsPlaying())
	{
		const float Scale = PurchasePunch.Tick(InDeltaTime);
		SetRenderScale(FVector2D(Scale, Scale));
		if (!PurchasePunch.IsPlaying())
		{
			SetRenderScale(FVector2D(1.f, 1.f));
		}
	}
}

void UShopItemCardWidget::PlayEntrance(float Delay)
{
	bEntrancePlaying = true;
	EntranceDelayRemaining = Delay;
	EntranceElapsed = 0.f;
	SetRenderOpacity(0.f);
	SetRenderTranslation(FVector2D(0.f, EntranceRise));
}

void UShopItemCardWidget::PlayPurchasePunch()
{
	// 1.0 초과는 카드가 클립 경계를 넘어 잘린다 — 축소 딥으로만 친다
	PurchasePunch.Start(0.94f, 0.22f);
}

void UShopItemCardWidget::InitCard(FName InRowName, const FShopItemTable& InRow)
{
	RowName = InRowName;
	CachedRow = InRow;

	if (NewRibbon)
	{
		NewRibbon->SetVisibility(InRow.bNew ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	RefreshQtyBadge();
	RefreshBundleList();

	ApplyStage(false);
	if (StageRays)
	{
		StageRays->SetVisibility(InRow.bFeatured ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (StageDots)
	{
		StageDots->SetVisibility(InRow.bFeatured ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (NameText)
	{
		// QtyBadge 로 옮기려 했으나 v4 카드 재배치가 레이아웃을 깨 철회(2026-09-24).
		// 배지 위젯이 트리에 없으므로 수량은 다시 이름이 진다 — QtyBadge 는 C++ 에 남아 있어
		// 카드 WBP 에 그 이름의 위젯을 넣으면 그때 이 줄만 되돌리면 된다.
		NameText->SetText(InRow.Quantity > 1
			? FText::Format(NSLOCTEXT("Shop", "ItemNameWithQty", "{0} x{1}"), InRow.DisplayName, FText::AsNumber(InRow.Quantity))
			: InRow.DisplayName);
	}

	if (IconImage)
	{
		if (UObject* IconResource = InRow.ResolveIconResource())
		{
			// 텍스처는 원본 비율(정사각으로 박으면 비정사각 아이콘이 눌린다), 머티리얼만 고유 크기가 없어 정사각 명시.
			// 어느 쪽이든 바깥 ScaleBox가 아이콘 칸에 맞춰 균일 축소한다.
			const UTexture2D* IconTexture = Cast<UTexture2D>(IconResource);
			const FVector2D IconDrawSize = IconTexture
				? FVector2D(IconTexture->GetSizeX(), IconTexture->GetSizeY())
				: FVector2D(256.f, 256.f);
			CGIconBrush::ApplyResource(IconImage, IconResource, IconDrawSize);
		}
	}

	ApplyBuyButton();
}

void UShopItemCardWidget::ApplyBuyButton()
{
	if (BuyButton)
	{
		// 버튼의 만렙 표면(칩 접기 + 잠금 라벨)을 품절 표면으로 쓴다
		BuyButton->MaxLevelText = NSLOCTEXT("Shop", "SoldOut", "판매 완료");

		if (CachedRow.Price <= 0)
		{
			BuyButton->SetCostVisible(false);
			BuyButton->SetButtonText(NSLOCTEXT("Shop", "BuyFree", "무료"));
			BuyButton->SetEnabled(true);
		}
		else if (CachedRow.Currency == EShopCurrency::Mileage)
		{
			// 마일리지는 재화 종류(DT_Resource)가 아니라 칩 아이콘·잔액 판정이 없다 — 칩을 접고 라벨이 값을 진다.
			// 부족 판정은 버튼 게이트 대신 패널의 CanAfford 경로가 맡도록 명시 활성화한다.
			BuyButton->SetCostVisible(false);
			BuyButton->SetButtonText(FText::Format(NSLOCTEXT("Shop", "BuyMileage", "{0}pt 교환"), FText::AsNumber(CachedRow.Price)));
			BuyButton->SetEnabled(true);
		}
		else
		{
			// 잔액 판정은 상점 매니저가 이미 하므로(RefreshState) 버튼의 자체 판정은 끈다
			const EResourceType CostType = (CachedRow.Currency == EShopCurrency::Money) ? EResourceType::Money : EResourceType::Diamond;
			BuyButton->SetCostVisible(true);
			BuyButton->SetCost(CachedRow.Price, CostType, /*bCheckAfford=*/false);
			BuyButton->SetButtonText(NSLOCTEXT("Shop", "Buy", "구매"));
		}
	}
}

void UShopItemCardWidget::ApplyStage(bool bSoldOut)
{
	const FLinearColor Tint = bSoldOut ? SoldOutStageTint : CachedRow.BandTint;
	if (BandImage)
	{
		BandImage->SetColorAndOpacity(Tint);
	}
	if (BandShade)
	{
		BandShade->SetColorAndOpacity(FLinearColor(Tint.R * StageShadeScale, Tint.G * StageShadeScale, Tint.B * StageShadeScale, Tint.A));
	}
}

void UShopItemCardWidget::RefreshState(int32 RemainingCount, bool bCanAfford)
{
	CachedRemainingCount = RemainingCount;
	bCachedCanAfford = bCanAfford;
	bHasState = true;

	const bool bSoldOut = (RemainingCount == 0);

	if (LimitText)
	{
		if (CachedRow.LimitCount > 0)
		{
			// 탭 이름(일일/주간)은 레일이 이미 말하고 있다 — 칩은 잔여 수량만
			LimitText->SetText(FText::Format(NSLOCTEXT("Shop", "LimitFmt", "구매 가능 {0}/{1}"),
				FText::AsNumber(FMath::Max(RemainingCount, 0)), FText::AsNumber(CachedRow.LimitCount)));
			LimitText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			LimitText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 품절은 버튼이 「판매 완료」로 잠기고, 무대는 무채색으로, 아이콘·이름은 흐리게 둔다
	ApplyStage(bSoldOut);
	const float SoldOutDim = bSoldOut ? 0.45f : 1.f;
	if (IconImage)
	{
		IconImage->SetRenderOpacity(SoldOutDim);
	}
	if (NameText)
	{
		NameText->SetRenderOpacity(SoldOutDim);
	}

	if (BuyButton)
	{
		BuyButton->SetMaxLevelState(bSoldOut);
		BuyButton->SetLockedState(bSoldOut);
		BuyButton->SetCanAfford(bCanAfford);
		// 판매 완료가 풀리면 버튼이 가격 칩을 다시 펴므로, 칩 없는 상품(무료·마일리지)은 도로 접는다
		if (!bSoldOut && (CachedRow.Price <= 0 || CachedRow.Currency == EShopCurrency::Mileage))
		{
			BuyButton->SetCostVisible(false);
		}
	}
}

void UShopItemCardWidget::HandleBuyClicked()
{
	OnPurchaseRequested.Broadcast(RowName);
}

void UShopItemCardWidget::RefreshQtyBadge()
{
	// 번들은 카드 전체가 묶음이라 낱개 수량 배지를 달지 않는다
	const bool bShow = (CachedRow.Quantity > 1) && !CachedRow.IsBundle();
	if (QtyBadge)
	{
		QtyBadge->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (QtyText && bShow)
	{
		QtyText->SetText(FText::FromString(FString::Printf(TEXT("\u00D7%d"), CachedRow.Quantity)));
	}
}

void UShopItemCardWidget::RefreshBundleList()
{
	if (!BundleList)
	{
		return;
	}
	BundleList->ClearChildren();

	if (!CachedRow.IsBundle())
	{
		BundleList->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> RowClass = TableMgr
		? TableMgr->GetWidgetClass(EWidgetType::ShopBundleRow)
		: nullptr;
	if (!RowClass)
	{
		// 행 클래스가 없으면 빈 목록을 남기지 말고 접는다 — 제목만 뜬 빈 판이 더 나쁘다
		BundleList->SetVisibility(ESlateVisibility::Collapsed);
		UE_LOG(LogTemp, Warning, TEXT("[Shop] ShopBundleRow 위젯 클래스 없음 — DT_WidgetClass 확인"));
		return;
	}

	BundleList->SetVisibility(ESlateVisibility::HitTestInvisible);
	for (const FShopBundleEntry& Entry : CachedRow.BundleItems)
	{
		if (Entry.Item == EItemType::None || Entry.Quantity <= 0)
		{
			continue;
		}
		if (UShopBundleRowWidget* Row = CreateWidget<UShopBundleRowWidget>(this, RowClass))
		{
			Row->SetEntry(Entry.Item, Entry.Quantity);
			if (UVerticalBoxSlot* RowSlot = BundleList->AddChildToVerticalBox(Row))
			{
				RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 9.f));
			}
		}
	}
}
