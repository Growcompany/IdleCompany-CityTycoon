#include "UI/Element/Cards/RankingEntryCardWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Global/GlobalUtilFunctions.h"
#include "Table/ProfileImageData.h"
#include "Enum/CompanyTitle.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "CommonButtonBase.h"

void URankingEntryCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MoveBtn)
	{
		MoveBtn->OnClicked().AddUObject(this, &URankingEntryCardWidget::OnMoveBtnClicked);
	}
}

void URankingEntryCardWidget::NativeDestruct()
{
	if (MoveBtn)
	{
		MoveBtn->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

void URankingEntryCardWidget::SetEntryData(const FRankingEntry& InEntry)
{
	CachedEntry = InEntry;

	if (RankText)
	{
		if (InEntry.Rank > 0)
		{
			RankText->SetText(FText::FromString(FString::Printf(TEXT("%d위"), InEntry.Rank)));
		}
		else
		{
			RankText->SetText(FText::FromString(TEXT("-")));
		}
	}

	if (UButtonWidget* NameBtn = Cast<UButtonWidget>(PlayerNameButton))
	{
		NameBtn->SetButtonText(FText::FromString(InEntry.DisplayName));
	}

	if (HQLevelText)
	{
		HQLevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d"), InEntry.HQLevel)));
	}

	if (RevenueText)
	{
		RevenueText->SetText(UGlobalUtilFunctions::AbbreviateNumber(InEntry.TotalRevenue, ENumberAbbrevStyle::Auto, ENumberRoundMode::Floor));
	}

	// 프로필 이미지 로드
	if (IconImage)
	{
		UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
		if (TableMgr)
		{
			bool bSuccess = false;
			FProfileImageData ImgData = TableMgr->GetProfileImageData(InEntry.ProfileImageID, bSuccess);
			if (bSuccess && !ImgData.Icon.IsNull())
			{
				UTexture2D* Texture = ImgData.Icon.LoadSynchronous();
				if (Texture)
				{
					IconImage->SetBrushFromTexture(Texture);
				}
			}
		}
	}
}

void URankingEntryCardWidget::OnMoveBtnClicked()
{
	OnEntryCardClicked.Broadcast(CachedEntry);
}

float URankingEntryCardWidget::EaseOutCubic(float A)
{
	const float T = 1.0f - A;
	return 1.0f - T * T * T;
}

void URankingEntryCardWidget::PlayIntro(int32 OrderIndex)
{
	IntroDelay = FMath::Min(OrderIndex * IntroStagger, IntroMaxDelay);
	IntroElapsed = 0.f;
	bIntroPlaying = true;

	// 첫 틱 전 제자리 1프레임 깜빡임 방지 — 시작 상태(투명 + 우측 오프셋) 즉시 적용
	FWidgetTransform Init;
	Init.Translation = FVector2D(IntroSlideX, 0.f);
	SetRenderTransform(Init);
	SetRenderOpacity(0.f);
}

void URankingEntryCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bIntroPlaying) return;

	IntroElapsed += InDeltaTime;
	if (IntroElapsed < IntroDelay) return;

	const float T = FMath::Clamp((IntroElapsed - IntroDelay) / IntroDuration, 0.f, 1.f);
	const float E = EaseOutCubic(T);

	FWidgetTransform Xform;
	Xform.Translation = FVector2D(IntroSlideX * (1.f - E), 0.f);
	SetRenderTransform(Xform);
	SetRenderOpacity(E);

	if (T >= 1.f)
	{
		bIntroPlaying = false;
		SetRenderTransform(FWidgetTransform());
		SetRenderOpacity(1.f);
	}
}
