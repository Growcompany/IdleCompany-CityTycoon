#include "UI/Element/Cards/ShopItemCardWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Global/GlobalUtilFunctions.h"

void UShopItemCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PriceButton)
	{
		PriceButton->OnClicked().AddUObject(this, &UShopItemCardWidget::HandlePriceClicked);
	}
}

void UShopItemCardWidget::NativeDestruct()
{
	if (PriceButton)
	{
		PriceButton->OnClicked().RemoveAll(this);
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
	PurchasePunch.Start(1.08f, 0.25f);
}

void UShopItemCardWidget::InitCard(FName InRowName, const FShopItemTable& InRow)
{
	RowName = InRowName;
	CachedRow = InRow;

	if (NameText)
	{
		NameText->SetText(InRow.Quantity > 1
			? FText::Format(NSLOCTEXT("Shop", "ItemNameWithQty", "{0} x{1}"), InRow.DisplayName, FText::AsNumber(InRow.Quantity))
			: InRow.DisplayName);
	}

	if (IconImage && !InRow.Icon.IsNull())
	{
		if (UTexture2D* Tex = InRow.Icon.LoadSynchronous())
		{
			// bMatchSize=true: 원본 비율 유지 (바깥 ScaleBox가 180 박스에 맞춰 균일 축소)
			IconImage->SetBrushFromTexture(Tex, true);
		}
	}

	// 가격 = 웜 칩(PriceIcon + PriceText, 크림 카드 언어), 버튼 = 행동 라벨만.
	// 마일리지는 재화 아이콘이 없어 칩을 숨기고 버튼 텍스트로 표기
	UWidget* PriceChipBox = GetWidgetFromName(TEXT("PriceChipBox"));
	UImage* PriceIcon = Cast<UImage>(GetWidgetFromName(TEXT("PriceIcon")));
	UCommonTextBlock* PriceText = Cast<UCommonTextBlock>(GetWidgetFromName(TEXT("PriceText")));

	if (PriceText && CachedRow.Currency != EShopCurrency::Mileage)
	{
		if (PriceChipBox)
		{
			PriceChipBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		PriceText->SetText(BuildPriceText());

		if (PriceIcon)
		{
			if (UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
			{
				bool bOk = false;
				const EResourceType ResType = (CachedRow.Currency == EShopCurrency::Money) ? EResourceType::Money : EResourceType::Diamond;
				const FResourceInfo Info = TableMgr->GetResourceInfo(ResType, bOk);
				if (bOk && !Info.Icon.IsNull())
				{
					// ImageSize(30)는 트리 값 유지 (bMatchSize=false)
					PriceIcon->SetBrushFromTexture(Info.Icon.LoadSynchronous());
				}
			}
		}

		if (PriceButton)
		{
			PriceButton->SetButtonText(NSLOCTEXT("Shop", "Buy", "구매"));
		}
	}
	else
	{
		if (PriceChipBox)
		{
			PriceChipBox->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (PriceButton)
		{
			PriceButton->SetButtonText(CachedRow.Currency == EShopCurrency::Mileage
				? FText::Format(NSLOCTEXT("Shop", "BuyMileage", "{0}pt 교환"), FText::AsNumber(CachedRow.Price))
				: BuildPriceText());
		}
	}
}

void UShopItemCardWidget::RefreshState(int32 RemainingCount, bool bCanAfford)
{
	const bool bSoldOut = (RemainingCount == 0);

	if (LimitText)
	{
		if (CachedRow.LimitCount > 0)
		{
			const FText Prefix = (CachedRow.Tab == EShopTab::Weekly)
				? NSLOCTEXT("Shop", "LimitWeekly", "주간")
				: NSLOCTEXT("Shop", "LimitDaily", "일일");
			LimitText->SetText(FText::Format(NSLOCTEXT("Shop", "LimitFmt", "{0} {1}/{2}"),
				Prefix, FText::AsNumber(FMath::Max(RemainingCount, 0)), FText::AsNumber(CachedRow.LimitCount)));
			LimitText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			LimitText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (SoldOutOverlay)
	{
		SoldOutOverlay->SetVisibility(bSoldOut ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (SoldOutText && bSoldOut)
	{
		SoldOutText->SetText(CachedRow.Tab == EShopTab::Weekly
			? NSLOCTEXT("Shop", "SoldOutWeekly", "다음 주에!")
			: NSLOCTEXT("Shop", "SoldOutDaily", "내일 다시!"));
	}

	if (PriceButton)
	{
		PriceButton->SetIsEnabled(!bSoldOut);
		// 부족 = 빨강 (클릭은 허용 — 패널이 실패 피드백 처리), 충분 = 크림 라벨 복원
		PriceButton->SetTextColor(bCanAfford
			? FSlateColor(FLinearColor(1.0f, 0.972f, 0.941f))
			: FSlateColor(FLinearColor(0.9f, 0.28f, 0.3f)));
	}

	// 가격 텍스트 부족 표시 (충분 = 브라운 #4B2E2B, 부족 = 빨강)
	if (UCommonTextBlock* PriceText = Cast<UCommonTextBlock>(GetWidgetFromName(TEXT("PriceText"))))
	{
		PriceText->SetColorAndOpacity(bCanAfford
			? FSlateColor(FLinearColor(0.0703f, 0.0273f, 0.0241f))
			: FSlateColor(FLinearColor(0.9f, 0.28f, 0.3f)));
	}
}

void UShopItemCardWidget::HandlePriceClicked()
{
	OnPurchaseRequested.Broadcast(RowName);
}

FText UShopItemCardWidget::BuildPriceText() const
{
	if (CachedRow.Currency == EShopCurrency::Mileage)
	{
		return FText::Format(NSLOCTEXT("Shop", "PriceMileage", "{0}pt"), FText::AsNumber(CachedRow.Price));
	}
	// 비용 표기는 Ceil (프로젝트 규칙)
	return UGlobalUtilFunctions::AbbreviateNumber(CachedRow.Price, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil);
}
