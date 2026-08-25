#include "UI/HUD/RewardClaimSplashWidget.h"

#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

#include "Manager/TableManagerSubsystem.h"
#include "Table/ResourceInfo.h"
#include "Global/GlobalUtilFunctions.h"

void URewardClaimSplashWidget::InitSplash(const FMissionTable& Mission)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SplashRoot"));
	WidgetTree->RootWidget = Root;

	ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SplashBox"));
	if (UCanvasPanelSlot* CSlot = Root->AddChildToCanvas(ContentBox))
	{
		// 화면 중앙보다 살짝 위 (시선 중심) — 포인트 앵커 + 중앙 정렬 + 오토사이즈
		CSlot->SetAnchors(FAnchors(0.5f, 0.42f, 0.5f, 0.42f));
		CSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CSlot->SetAutoSize(true);
	}

	// 타이포 SOT 폰트 — 로드 실패 시 해당 텍스트만 엔진 폴백 폰트 (soft-fail)
	UFont* NexonBold = TSoftObjectPtr<UFont>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicBold_Font.NEXONLv1GothicBold_Font"))).LoadSynchronous();

	const FLinearColor Amber(1.f, 0.723055f, 0.194618f, 1.f);
	const FLinearColor DarkOutline(0.03f, 0.03f, 0.03f, 0.85f);

	// 타이틀 "보상 획득!"
	UCommonTextBlock* Title = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("SplashTitle"));
	Title->SetText(NSLOCTEXT("MissionReward", "SplashTitle", "보상 획득!"));
	Title->SetColorAndOpacity(FSlateColor(Amber));
	if (NexonBold)
	{
		FSlateFontInfo TitleFont(NexonBold, 38);
		TitleFont.TypefaceFontName = TEXT("Default");
		TitleFont.OutlineSettings.OutlineSize = 3;
		TitleFont.OutlineSettings.OutlineColor = DarkOutline;
		Title->SetFont(TitleFont);
	}
	if (UVerticalBoxSlot* TitleSlot = ContentBox->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
	}

	// 보상 행 — [아이콘 64 + "+축약수치"] 페어를 가로로
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SplashRewards"));
	if (UVerticalBoxSlot* RowSlot = ContentBox->AddChildToVerticalBox(Row))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Center);
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	bool bAny = false;
	for (const FMissionReward& Reward : Mission.Rewards)
	{
		if (Reward.Amount <= 0 || Reward.ResourceType == EResourceType::None)
		{
			continue;
		}

		// 아이콘 — DT_Resource Icon 소프트 로드. 없으면 수치만 (아이콘이 자원 정체성)
		UTexture2D* IconTex = nullptr;
		if (TableMgr)
		{
			bool bFound = false;
			const FResourceInfo Info = TableMgr->GetResourceInfo(Reward.ResourceType, bFound);
			if (bFound && !Info.Icon.IsNull())
			{
				IconTex = Info.Icon.LoadSynchronous();
			}
		}

		if (IconTex)
		{
			UImage* IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			FSlateBrush IconBrush;
			IconBrush.SetResourceObject(IconTex);
			IconBrush.ImageSize = FVector2D(64.f, 64.f);
			IconImage->SetBrush(IconBrush);
			if (UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconImage))
			{
				IconSlot->SetVerticalAlignment(VAlign_Center);
				IconSlot->SetPadding(FMargin(bAny ? 28.f : 0.f, 0.f, 8.f, 0.f));
			}
		}

		UCommonTextBlock* ValueText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass());
		ValueText->SetText(FText::FromString(
			TEXT("+") + UGlobalUtilFunctions::AbbreviateNumber(Reward.Amount).ToString()));
		ValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		if (NexonBold)
		{
			FSlateFontInfo ValueFont(NexonBold, 34);
			ValueFont.TypefaceFontName = TEXT("Default");
			ValueFont.OutlineSettings.OutlineSize = 2;
			ValueFont.OutlineSettings.OutlineColor = DarkOutline;
			ValueText->SetFont(ValueFont);
		}
		if (UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(ValueText))
		{
			ValueSlot->SetVerticalAlignment(VAlign_Center);
			ValueSlot->SetPadding(FMargin((!IconTex && bAny) ? 28.f : 0.f, 0.f, 0.f, 0.f));
		}

		bAny = true;
	}

	// 팝인 시작 상태 — 작게 + 투명 (NativeTick 이 끌어올림)
	ContentBox->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	ContentBox->SetRenderScale(FVector2D(0.6f, 0.6f));
	SetRenderOpacity(0.f);
}

void URewardClaimSplashWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!ContentBox)
	{
		RemoveFromParent();
		return;
	}

	Elapsed += InDeltaTime;

	if (Elapsed <= PopDuration)
	{
		// ease-out-back — 가볍게 오버슈트했다 정착
		const float T = Elapsed / PopDuration;
		const float C1 = 1.70158f;
		const float C3 = C1 + 1.f;
		const float Eased = 1.f + C3 * FMath::Pow(T - 1.f, 3.f) + C1 * FMath::Pow(T - 1.f, 2.f);
		const float Scale = FMath::Lerp(0.6f, 1.f, Eased);
		ContentBox->SetRenderScale(FVector2D(Scale, Scale));
		SetRenderOpacity(FMath::Min(T * 2.5f, 1.f));
	}
	else if (Elapsed <= PopDuration + HoldDuration)
	{
		ContentBox->SetRenderScale(FVector2D(1.f, 1.f));
		SetRenderOpacity(1.f);
	}
	else if (Elapsed <= PopDuration + HoldDuration + FadeDuration)
	{
		// 살짝 떠오르며 페이드아웃
		const float T = (Elapsed - PopDuration - HoldDuration) / FadeDuration;
		SetRenderOpacity(1.f - T);
		ContentBox->SetRenderTranslation(FVector2D(0.f, -24.f * T));
	}
	else
	{
		RemoveFromParent();
	}
}
