#include "UI/Panel/LaunchRewardRevealWidget.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Engine/GameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Table/LaunchLootBandTable.h"
#include "Table/ShopItemTable.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Cards/ItemCardSlotWidget.h"
#include "UI/UISoundTags.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "Utils/LaunchRewardRevealMath.h"
#include "Enum/WidgetType.h"

// 비트 시각(초) — 목업 SOT(docs/05_UI/mockups/LaunchRewardReveal_MOCKUP.html C안)와 동일. 파일 고유 네임스페이스(유니티 빌드 충돌 방지).
namespace LrrFx
{
	constexpr float DimIn = 0.25f;
	constexpr float HeaderAt = 0.20f, HeaderDur = 0.40f;
	constexpr float StubAt = 0.35f, StubDur = 0.35f;
	constexpr float CountAt = 0.50f, CountDur = 1.10f;
	constexpr float StampAt = 1.55f, StampDur = 0.14f, StampShakeDur = 0.20f, InkDur = 0.50f;
	constexpr float SlotsAt = 1.80f, SlotsDur = 0.25f, SlotStep = 0.03f, CountTextAt = 1.90f;
	constexpr float RunStart = 2.15f;
	constexpr float CardIn = 0.14f, CardSettle = 0.30f, CardFlash = 0.35f, CardRing = 0.40f, CardShakeDur = 0.10f;
	constexpr float SparkleStep = 0.05f, SparkleDur = 0.45f;
	constexpr float BestGlowDelay = 0.45f, BadgeDelay = 0.18f, BadgeDur = 0.18f, ConfirmRise = 0.30f;
	constexpr float SilhouetteDimmed = 0.35f;
	constexpr int32 SparkleCount = 3;
}

namespace
{
	float Seg(float T, float Start, float Dur) { return FMath::Clamp((T - Start) / FMath::Max(Dur, KINDA_SMALL_NUMBER), 0.f, 1.f); }
	float OutCubic(float P) { return 1.f - FMath::Pow(1.f - P, 3.f); }
	float InQuad(float P) { return P * P; }
	float Bell(float P) { return FMath::Sin(P * PI); }

	// Image 의 desired size = Brush.ImageSize — Overlay 가 최대 자식 크기로 부풀지 않게 레이아웃 크기를 주입(가챠 리빌 SetImageLayoutSize 와 같은 이유)
	void ApplyVfxWithLayoutSize(UTableManagerSubsystem* TableMgr, FName Key, UImage* Image, float Size)
	{
		if (!TableMgr || !Image) { return; }
		TableMgr->ApplyUIVFXTexture(Key, Image);
		FSlateBrush B = Image->GetBrush();
		B.ImageSize = FVector2D(Size, Size);
		Image->SetBrush(B);
	}
}

void ULaunchRewardRevealWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().RemoveAll(this);
		ConfirmButton->OnClicked().AddUObject(this, &ULaunchRewardRevealWidget::HandleConfirmClicked);
	}
	if (FlashOverlay) { FlashOverlay->SetVisibility(ESlateVisibility::HitTestInvisible); FlashOverlay->SetRenderOpacity(0.f); }
	if (InkRing)
	{
		// 잉크 링 텍스처는 DT_UIVFXTexture 주입 — 브러시 없는 Image 는 사각형으로 번진다
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>()) { ApplyVfxWithLayoutSize(TableMgr, FName("GachaShockwave"), InkRing, 600.f); }
		}
		InkRing->SetVisibility(ESlateVisibility::HitTestInvisible);
		InkRing->SetRenderOpacity(0.f);
	}
}

void ULaunchRewardRevealWidget::NativeDestruct()
{
	if (ConfirmButton) { ConfirmButton->OnClicked().RemoveAll(this); }
	Super::NativeDestruct();
}

UWidget* ULaunchRewardRevealWidget::GetConfirmButtonWidget() const
{
	return ConfirmButton;
}

void ULaunchRewardRevealWidget::Setup(const FLaunchRewardRevealData& InData)
{
	Data = InData;
	StageTime = 0.f;
	bConfirmEnabled = false;
	bClosing = false;
	bScoreSoundFired = bStampSoundFired = bSlotsSoundFired = bBestSoundFired = bBadgeSoundFired = false;

	if (ProjectNameText) { ProjectNameText->SetText(Data.ProjectName); }
	if (ScoreText) { ScoreText->SetText(FText::FromString(TEXT("0"))); }
	if (ScoreRail) { ScoreRail->SetPercent(0.f); }
	ApplyBandStyle();

	const TArray<FMissionReward> Ordered = LaunchRewardRevealMath::BuildRevealOrder(Data.Loot);
	ClearCards();
	BuildCards(Ordered);
	CardTimes = LaunchRewardRevealMath::StampRunTimes(Cards.Num(), LrrFx::RunStart, Stagger, StaggerDecay, StaggerMin);
	for (int32 i = 0; i < Cards.Num(); ++i) { Cards[i].StartAt = CardTimes[i]; }

	if (CountText)
	{
		CountText->SetText(FText::Format(NSLOCTEXT("LaunchReveal", "Count", "보상 {0}장"), FText::AsNumber(Cards.Num())));
	}
	if (DiscoveryBadge)
	{
		DiscoveryBadge->SetVisibility(Data.bFirstDiscovery ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		DiscoveryBadge->SetRenderOpacity(0.f);
	}

	const float LastCard = Cards.Num() > 0 ? CardTimes.Last() + LrrFx::CardIn + LrrFx::BestGlowDelay : LrrFx::SlotsAt + LrrFx::SlotsDur;
	BadgeAt = LastCard + LrrFx::BadgeDelay;
	ConfirmAt = (Data.bFirstDiscovery ? BadgeAt + LrrFx::BadgeDur : LastCard) + ConfirmDelay;
	EndTime = ConfirmAt + LrrFx::ConfirmRise;

	if (ConfirmButton) { ConfirmButton->SetRenderOpacity(0.f); ConfirmButton->SetIsEnabled(false); }
	bSetupDone = true;
	RenderAt(0.f);
}

void ULaunchRewardRevealWidget::ApplyBandStyle()
{
	BandColor = FLinearColor::White;
	FText Label;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			FLaunchLootBandRow Row;
			if (TableMgr->GetLaunchLootBand(Data.BandKey, Row)) { BandColor = Row.Color; Label = Row.DisplayName; }
		}
	}
	if (StampText) { StampText->SetText(Label); StampText->SetColorAndOpacity(FSlateColor(BandColor)); }
	if (StampFrame)
	{
		// 테두리만 밴드색 — 채움은 WBP 가 투명으로 둔다(도장은 잉크 선만 찍힌다)
		FSlateBrush B = StampFrame->Background;
		B.OutlineSettings.Color = FSlateColor(BandColor);
		StampFrame->SetBrush(B);
	}
	if (InkRing) { InkRing->SetColorAndOpacity(BandColor); }
}

void ULaunchRewardRevealWidget::ClearCards()
{
	if (SlotRow) { SlotRow->ClearChildren(); }
	Cards.Reset();
	CardTimes.Reset();
}

void ULaunchRewardRevealWidget::BuildCards(const TArray<FMissionReward>& Ordered)
{
	if (!SlotRow || !WidgetTree) { return; }
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const TSubclassOf<UUserWidget> CardClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::ItemCard) : nullptr;
	if (!TableMgr || !CardClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaunchReveal] ItemCard widget class not registered - 보상 카드 생략"));
		return;
	}

	const float Gap = FMath::RoundToFloat(CardSize * 0.15f);
	const float IconSize = FMath::RoundToFloat(CardSize * 0.7f);
	bool bLoggedResourceSkip = false;

	// 최고 보상 = 마지막 카드가 될 항목(자원 스킵분 제외). 배후광은 Wrap 이 라이브가 되기 전에
	// 첫 자식으로 넣어야 SOverlay 순서에 반영된다 — 라이브 뒤 InsertChildAt(0)은 Slots 배열만 바꾼다
	int32 BestOrderedIndex = INDEX_NONE;
	for (int32 i = 0; i < Ordered.Num(); ++i)
	{
		if (Ordered[i].ItemType != EItemType::None) { BestOrderedIndex = i; }
	}

	for (int32 i = 0; i < Ordered.Num(); ++i)
	{
		const FMissionReward& R = Ordered[i];
		// 출시 전리품은 티켓 전용(2026-08-12 결정) — 자원 항목은 카드가 없다
		if (R.ItemType == EItemType::None)
		{
			if (!bLoggedResourceSkip)
			{
				bLoggedResourceSkip = true;
				UE_LOG(LogTemp, Warning, TEXT("[LaunchReveal] 자원 전용 보상은 카드로 표시하지 않는다 - 건너뜀"));
			}
			continue;
		}
		FRevealCard Entry;

		UOverlay* Wrap = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Wrap->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Wrap->SetVisibility(ESlateVisibility::HitTestInvisible);

		// 배후광(최고 카드 전용) — Overlay 는 나중 자식이 위로 그려지므로 반드시 "첫" 자식으로
		if (i == BestOrderedIndex)
		{
			Entry.bBest = true;
			UImage* Glow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			// 레이아웃 크기는 카드와 동일 — 카드 2배(480)는 RenderScale 로 낸다(레이아웃 480 = 그 칸만 부푸는 F1 함정)
			ApplyVfxWithLayoutSize(TableMgr, FName("GachaCardGlow"), Glow, CardSize);
			Glow->SetColorAndOpacity(BandColor);
			Glow->SetVisibility(ESlateVisibility::HitTestInvisible);
			Glow->SetRenderOpacity(0.f);
			Glow->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			Glow->SetRenderScale(FVector2D(2.f, 2.f));
			if (UOverlaySlot* GSlot = Wrap->AddChildToOverlay(Glow))
			{
				GSlot->SetHorizontalAlignment(HAlign_Center);
				GSlot->SetVerticalAlignment(VAlign_Center);
			}
			Entry.Glow = Glow;
		}

		// 크기 권위 = SizeBox 1개 (비율 박스 — 고정 정당). 나머지 자식은 Fill/Center 로 얹는다.
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(CardSize);
		Box->SetHeightOverride(CardSize);
		Wrap->AddChildToOverlay(Box);

		UBorder* Silhouette = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		{
			FSlateBrush SB;
			SB.DrawAs = ESlateBrushDrawType::RoundedBox;
			SB.OutlineSettings.Width = 5.f;
			SB.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			SB.OutlineSettings.CornerRadii = FVector4(22.f, 22.f, 22.f, 22.f);
			SB.OutlineSettings.Color = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.28f));
			SB.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.03f));
			Silhouette->SetBrush(SB);
		}
		Silhouette->SetRenderOpacity(0.f);
		if (UOverlaySlot* SSlot = Wrap->AddChildToOverlay(Silhouette))
		{
			SSlot->SetHorizontalAlignment(HAlign_Fill);
			SSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UItemCardSlotWidget* Card = CreateWidget<UItemCardSlotWidget>(this, CardClass);
		if (Card)
		{
			bool bIconFound = false;
			UTexture2D* Icon = TableMgr->GetItemIcon(R.ItemType, bIconFound);
			FShopItemTable ItemRow;
			const FText Label = TableMgr->GetShopItemByItemType(R.ItemType, ItemRow) ? ItemRow.DisplayName : FText::GetEmpty();
			Card->SetIcon(Icon);
			Card->SetLabel(Label);
			Card->SetQuantity(R.ItemAmount);   // 아이템 수량은 ItemAmount (Amount 는 자원용)
			Card->SetCardSize(FVector2D(CardSize, CardSize), FVector2D(IconSize, IconSize));
			Card->SetLabelFontSize(30);
			Card->SetVisibility(ESlateVisibility::HitTestInvisible);
			Card->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			Card->SetRenderOpacity(0.f);
			if (UOverlaySlot* CSlot = Wrap->AddChildToOverlay(Card))
			{
				CSlot->SetHorizontalAlignment(HAlign_Center);
				CSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		UImage* Flash = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		{
			FSlateBrush FB;
			FB.DrawAs = ESlateBrushDrawType::RoundedBox;
			FB.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			FB.OutlineSettings.CornerRadii = FVector4(22.f, 22.f, 22.f, 22.f);
			FB.TintColor = FSlateColor(FLinearColor::White);
			Flash->SetBrush(FB);
		}
		Flash->SetVisibility(ESlateVisibility::HitTestInvisible);
		Flash->SetRenderOpacity(0.f);
		if (UOverlaySlot* FSlot = Wrap->AddChildToOverlay(Flash))
		{
			FSlot->SetHorizontalAlignment(HAlign_Fill);
			FSlot->SetVerticalAlignment(VAlign_Fill);
		}

		UImage* Ring = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		ApplyVfxWithLayoutSize(TableMgr, FName("GachaShockwave"), Ring, CardSize);
		Ring->SetColorAndOpacity(FLinearColor::White);
		Ring->SetVisibility(ESlateVisibility::HitTestInvisible);
		Ring->SetRenderOpacity(0.f);
		Ring->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		if (UOverlaySlot* RSlot = Wrap->AddChildToOverlay(Ring))
		{
			RSlot->SetHorizontalAlignment(HAlign_Center);
			RSlot->SetVerticalAlignment(VAlign_Center);
		}

		for (int32 s = 0; s < LrrFx::SparkleCount; ++s)
		{
			UImage* Spark = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			ApplyVfxWithLayoutSize(TableMgr, FName("SparkleMain"), Spark, 64.f);
			Spark->SetVisibility(ESlateVisibility::HitTestInvisible);
			Spark->SetRenderOpacity(0.f);
			Spark->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			// 3발 산개: 좌상 / 우상 / 하중 — 카드 반폭 기준 오프셋
			static const FVector2D Offsets[3] = { FVector2D(-0.32f, -0.28f), FVector2D(0.28f, -0.20f), FVector2D(0.f, 0.30f) };
			Spark->SetRenderTranslation(Offsets[s] * CardSize);
			if (UOverlaySlot* PSlot = Wrap->AddChildToOverlay(Spark))
			{
				PSlot->SetHorizontalAlignment(HAlign_Center);
				PSlot->SetVerticalAlignment(VAlign_Center);
			}
			Entry.Sparkles.Add(Spark);
		}

		if (UHorizontalBoxSlot* RowSlot = SlotRow->AddChildToHorizontalBox(Wrap))
		{
			RowSlot->SetPadding(FMargin(Cards.Num() == 0 ? 0.f : Gap, 0.f, 0.f, 0.f));
			RowSlot->SetVerticalAlignment(VAlign_Center);
		}

		Entry.Wrap = Wrap;
		Entry.Silhouette = Silhouette;
		Entry.Card = Card;
		Entry.Flash = Flash;
		Entry.Ring = Ring;
		Cards.Add(MoveTemp(Entry));
	}

	// 배후광/bBest 는 루프 안(BestOrderedIndex)에서 처리 — Wrap 이 SlotRow 에 붙어 라이브가 된 뒤에는
	// InsertChildAt(0) 이 SOverlay 순서를 바꾸지 못한다(2026-08-22 재리뷰 실측: Slots 배열만 재배열)
}

void ULaunchRewardRevealWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bSetupDone) { return; }
	if (StageTime < EndTime)
	{
		StageTime = FMath::Min(EndTime, StageTime + InDeltaTime);
	}
	RenderAt(StageTime);
}

void ULaunchRewardRevealWidget::RenderAt(float T)
{
	using namespace LrrFx;

	if (DimBG) { DimBG->SetRenderOpacity(Seg(T, 0.f, DimIn) * DimAlpha); }

	if (HeaderBox)
	{
		const float P = Seg(T, HeaderAt, HeaderDur);
		HeaderBox->SetRenderOpacity(P);
		HeaderBox->SetRenderTranslation(FVector2D(0.f, (1.f - OutCubic(P)) * 30.f));
	}

	if (StubPlate)
	{
		const float P = Seg(T, StubAt, StubDur);
		StubPlate->SetRenderOpacity(P);
		StubPlate->SetRenderTranslation(FVector2D(0.f, (1.f - FWidgetAnimationUtils::EaseOutBack(P)) * 60.f));
	}

	// 평점 카운트업 (0.6s 클립 1회 — 스윕 길이는 클립이 맡는다)
	{
		const float P = OutCubic(Seg(T, CountAt, CountDur));
		const float Cur = P * static_cast<float>(Data.ReviewScore);
		if (ScoreText) { ScoreText->SetText(FText::AsNumber(FMath::RoundToInt(Cur))); }
		if (ScoreRail) { ScoreRail->SetPercent(Cur / 40.f); }
		if (!bScoreSoundFired && T >= CountAt) { bScoreSoundFired = true; PlayKey(FName("Event_ScoreSweep")); }
	}

	// 판정 도장 + 잉크 링 + 셰이크
	if (StampBox)
	{
		const float SK = (T - StampAt) / StampDur;
		if (SK <= 0.f) { StampBox->SetRenderOpacity(0.f); }
		else
		{
			StampBox->SetRenderOpacity(FWidgetAnimationUtils::StampInOpacity(SK, 0.96f));
			const float Settle = Seg(T, StampAt + StampDur, 0.25f);
			const float S = FWidgetAnimationUtils::StampInScale(SK) - Bell(Settle) * 0.04f;
			StampBox->SetRenderScale(FVector2D(S, S));
		}
		if (!bStampSoundFired && T >= StampAt) { bStampSoundFired = true; PlayUITag(CGUISoundTags::GachaStamp); }
	}
	if (InkRing)
	{
		const float P = Seg(T, StampAt + 0.10f, InkDur);
		InkRing->SetRenderOpacity(P > 0.f && P < 1.f ? (1.f - P) * 0.9f : 0.f);
		const float S = 0.4f + OutCubic(P) * 1.6f;
		InkRing->SetRenderScale(FVector2D(S, S));
	}

	// 빈 슬롯 일괄 등장 + 보상 N장
	if (!bSlotsSoundFired && T >= SlotsAt) { bSlotsSoundFired = true; PlayUITag(CGUISoundTags::GachaCardPop); }
	if (CountText)
	{
		const float P = Seg(T, CountTextAt, 0.30f);
		CountText->SetRenderOpacity(P);
		CountText->SetRenderTranslation(FVector2D(0.f, (1.f - OutCubic(P)) * 20.f));
	}

	// 카드 연타
	for (int32 i = 0; i < Cards.Num(); ++i)
	{
		FRevealCard& C = Cards[i];
		if (UBorder* Sil = C.Silhouette.Get())
		{
			const float P = Seg(T, SlotsAt + i * SlotStep, SlotsDur);
			Sil->SetRenderOpacity(P * (T < C.StartAt + 0.10f ? 1.f : SilhouetteDimmed));
			Sil->SetRenderTranslation(FVector2D(0.f, (1.f - FWidgetAnimationUtils::EaseOutBack(P)) * 80.f));
		}
		UItemCardSlotWidget* Card = C.Card.Get();
		const float P = Seg(T, C.StartAt, CardIn);
		if (Card)
		{
			if (P <= 0.f) { Card->SetRenderOpacity(0.f); }
			else
			{
				const float Settle = Seg(T, C.StartAt + CardIn, CardSettle);
				const float Wob = Bell(Settle) * 0.05f * (CardShakeAmp / 7.f);
				const float S = 1.6f - InQuad(P) * 0.6f;
				Card->SetRenderOpacity(FMath::Clamp(P * 4.f, 0.f, 1.f));
				Card->SetRenderScale(FVector2D(S + Wob, S - Wob));
			}
		}
		if (UImage* Flash = C.Flash.Get())
		{
			const float F = Seg(T, C.StartAt + 0.10f, CardFlash);
			Flash->SetRenderOpacity(F > 0.f && F < 1.f ? (1.f - F) * 0.8f : 0.f);
		}
		if (UImage* Ring = C.Ring.Get())
		{
			const float R = Seg(T, C.StartAt + 0.12f, CardRing);
			Ring->SetRenderOpacity(R > 0.f && R < 1.f ? (1.f - R) * 0.9f : 0.f);
			const float S = 0.4f + OutCubic(R) * 1.2f;
			Ring->SetRenderScale(FVector2D(S, S));
		}
		for (int32 s = 0; s < C.Sparkles.Num(); ++s)
		{
			if (UImage* Spark = C.Sparkles[s].Get())
			{
				const float SP = Seg(T, C.StartAt + CardIn + s * SparkleStep, SparkleDur);
				if (SP <= 0.f || SP >= 1.f) { Spark->SetRenderOpacity(0.f); continue; }
				const float Sc = 0.2f + FWidgetAnimationUtils::EaseOutBack(FMath::Clamp(SP * 1.6f, 0.f, 1.f));
				Spark->SetRenderOpacity(SP < 0.2f ? SP / 0.2f : 1.f - (SP - 0.2f) / 0.8f);
				Spark->SetRenderScale(FVector2D(Sc, Sc));
				Spark->SetRenderTransformAngle(SP * 90.f);
			}
		}
		if (C.bBest)
		{
			// 배후광은 한 번 차오르면 유지 (Seg 가 1 에서 클램프)
			if (UImage* Glow = C.Glow.Get())
			{
				Glow->SetRenderOpacity(Seg(T, C.StartAt + CardIn + BestGlowDelay, 0.85f) * 0.85f);
			}
		}
		if (!C.bSoundFired && T >= C.StartAt)
		{
			C.bSoundFired = true;
			PlayKey(FName("Event_LootPop"), FMath::Pow(0.85f, static_cast<float>(i)) + 0.15f, 1.f + 0.05f * i);
		}
		if (C.bBest && !bBestSoundFired && Data.BandKey == FName("High") && T >= C.StartAt + CardIn + BestGlowDelay)
		{
			bBestSoundFired = true;
			PlayUITag(CGUISoundTags::GachaResultLegendary);
		}
	}

	// 첫 발견 도장(소)
	if (DiscoveryBadge && Data.bFirstDiscovery)
	{
		const float P = Seg(T, BadgeAt, BadgeDur);
		DiscoveryBadge->SetRenderOpacity(FMath::Clamp(P * 3.f, 0.f, 1.f));
		const float S = 1.5f - InQuad(P) * 0.5f;
		DiscoveryBadge->SetRenderScale(FVector2D(S, S));
		DiscoveryBadge->SetRenderTransformAngle(-3.f);
		if (!bBadgeSoundFired && T >= BadgeAt) { bBadgeSoundFired = true; PlayUITag(CGUISoundTags::RewardGeneric); }
	}

	// [확인]
	if (ConfirmButton)
	{
		const float P = Seg(T, ConfirmAt, ConfirmRise);
		ConfirmButton->SetRenderOpacity(FWidgetAnimationUtils::EaseOutQuad(P));
		ConfirmButton->SetRenderTranslation(FVector2D(0.f, (1.f - OutCubic(P)) * 50.f));
		if (!bConfirmEnabled && T >= ConfirmAt) { bConfirmEnabled = true; ConfirmButton->SetIsEnabled(true); }
	}

	// 전역 셰이크 — 루트가 아니라 SlotRow/StubPlate 가 아닌 HeaderBox 를 제외한 본문에 거는 대신, 딤을 뺀 콘텐츠 컨테이너가 WBP 에 없으므로
	// 도장/카드 셰이크는 각 대상(StampBox·SlotRow)에 직접 건다. 딤은 흔들지 않는다.
	{
		FVector2D Off = FWidgetAnimationUtils::ShakeOffset(T - StampAt, StampShakeDur, StampShakeAmp);
		for (const FRevealCard& C : Cards)
		{
			Off += FWidgetAnimationUtils::ShakeOffset(T - C.StartAt, CardShakeDur, CardShakeAmp);
		}
		if (SlotRow) { SlotRow->SetRenderTranslation(Off); }
		if (StubPlate && T >= StampAt) { StubPlate->SetRenderTranslation(Off * 0.6f); }
	}
}

FReply ULaunchRewardRevealWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 탭 = 종료 시각으로 점프. 미재생 사운드는 버린다(카드 N 장 클립이 한 프레임에 뭉치는 것 방지).
	if (bSetupDone && StageTime < EndTime)
	{
		StageTime = EndTime;
		bScoreSoundFired = bStampSoundFired = bSlotsSoundFired = bBestSoundFired = bBadgeSoundFired = true;
		for (FRevealCard& C : Cards) { C.bSoundFired = true; }
		RenderAt(StageTime);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void ULaunchRewardRevealWidget::HandleConfirmClicked()
{
	if (!bConfirmEnabled) { return; }
	bClosing = true;
	DeactivateWidget();
}

void ULaunchRewardRevealWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	// 다른 프롬프트가 위에 덮이면 CommonUI 가 deactivate 를 부른다 — 닫힘(확인)과 구분
	if (!bClosing) { return; }
	bSetupDone = false;
	bClosing = false;
	OnRevealClosed.Broadcast();
}

void ULaunchRewardRevealWidget::PlayKey(FName Key, float Volume, float Pitch)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SoundMgr->PlaySoundWithVolume(Key, Volume, Pitch);
		}
	}
}

void ULaunchRewardRevealWidget::PlayUITag(const FGameplayTag& Tag)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SoundMgr->PlayUISound(Tag);
		}
	}
}
