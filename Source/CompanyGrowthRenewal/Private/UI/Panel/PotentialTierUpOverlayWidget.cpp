#include "UI/Panel/PotentialTierUpOverlayWidget.h"

#include "CommonTextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/Element/Employee/EmployeeRarityDisplay.h"
#include "UI/UISoundTags.h"

namespace
{
	// 시간표 — 승인 시안 docs/05_UI/mockups/PotentialRerollFx_2026-10-11 (v4)
	constexpr float TierUpHitTime = .12f;
	constexpr float TierUpHoldEnd = 1.05f;
	constexpr float TierUpFadeTime = .2f;
	constexpr float TierUpDimAlpha = .6f;
	constexpr float TierUpReduceIn = .15f;
	// 칼끝 — 출시 결과창 칼끝과 같은 식(화면 밖에서 가속해 꽂히고 짧아지며 사라짐)
	constexpr float TierUpThrustTravel = .12f;
	constexpr float TierUpBladeSquash = .62f;
	constexpr float TierUpBladeCore = .36f;
	constexpr float TierUpThrustAngles[] = { -168.f, -142.f, -118.f, -62.f, -38.f, -14.f, 14.f, 38.f, 62.f, 118.f, 142.f, 166.f };
	constexpr float TierUpBurstAngles[] = { -160.f, -110.f, -70.f, -20.f, 20.f, 70.f, 110.f, 160.f };

	float TierUpEaseOut(float T)
	{
		return 1.f - FMath::Pow(1.f - FMath::Clamp(T, 0.f, 1.f), 3.f);
	}

	float TierUpIntensity(ELootBoxRarity Rarity)
	{
		switch (Rarity)
		{
		case ELootBoxRarity::Rare:      return .85f;
		case ELootBoxRarity::Epic:      return 1.f;
		case ELootBoxRarity::Legendary:
		case ELootBoxRarity::Mythic:    return 1.12f;
		default:                        return .75f;
		}
	}

	// CSS color-mix(in srgb) 와 같은 섞기 — 시안의 띠·빛 색을 그대로 낸다
	FLinearColor TierUpMixSRGB(const FColor& From, const FColor& To, float T)
	{
		auto Mix = [T](uint8 A, uint8 B) { return static_cast<uint8>(FMath::RoundToInt(FMath::Lerp(static_cast<float>(A), static_cast<float>(B), T))); };
		return FLinearColor::FromSRGBColor(FColor(Mix(From.R, To.R), Mix(From.G, To.G), Mix(From.B, To.B)));
	}

	void TierUpHide(UWidget* Layer)
	{
		if (!Layer) return;
		Layer->SetRenderOpacity(0.f);
		Layer->SetRenderTransform(FWidgetTransform());
	}

	// 칼끝: Tip 이 뾰족한 끝. 이미지 피벗(1,0.5)·캔버스 정렬(1,0.5) 계약이라 끝을 축으로 회전·축소된다.
	void TierUpDriveBlade(UImage* Blade, const FVector2D& Tip, float AngleRadians, float Length, float CoreThickness, float Alpha)
	{
		const UCanvasPanelSlot* BladeSlot = Blade ? Cast<UCanvasPanelSlot>(Blade->Slot) : nullptr;
		if (!BladeSlot || BladeSlot->GetSize().X < 1.f || BladeSlot->GetSize().Y < 1.f) return;
		const float BaseX = static_cast<float>(BladeSlot->GetSize().X);
		const float BaseY = static_cast<float>(BladeSlot->GetSize().Y);
		Blade->SetRenderTransform(FWidgetTransform(Tip,
			FVector2D(FMath::Min(1.f, Length / BaseX), FMath::Min(1.f, CoreThickness / TierUpBladeCore / BaseY)),
			FVector2D::ZeroVector, FMath::RadiansToDegrees(AngleRadians)));
		Blade->SetRenderOpacity(FMath::Clamp(Alpha, 0.f, 1.f));
	}
}

void UPotentialTierUpOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CollectLayers();
	if (BandRoot) BandRoot->SetRenderTransformPivot(FVector2D(.5f, .5f));
	if (ToText) ToText->SetRenderTransformPivot(FVector2D(.5f, 1.f));
	if (Elapsed < 0.f)
	{
		// Play 전에는 아무것도 보이지 않는다
		UWidget* Layers[] = { DimImage, BandRoot, TitleText, FromChip, ArrowText, ToText, BandSheen, CoreGlow };
		for (UWidget* Layer : Layers) TierUpHide(Layer);
	}
}

void UPotentialTierUpOverlayWidget::CollectLayers()
{
	auto Collect = [this](TArray<TWeakObjectPtr<UImage>>& Out, const TCHAR* Prefix)
	{
		Out.Reset();
		for (int32 Index = 0; UImage* Layer = Cast<UImage>(GetWidgetFromName(FName(*FString::Printf(TEXT("%s%02d"), Prefix, Index)))); ++Index)
		{
			Out.Add(Layer);
		}
	};
	Collect(ThrustBlades, TEXT("TierUpThrust"));
	Collect(BurstBlades, TEXT("TierUpBurst"));
	Collect(Stars, TEXT("TierUpStar"));
	BandFills.Reset();
	BandLines.Reset();
	for (const TCHAR* Name : { TEXT("BandFillL"), TEXT("BandFillC"), TEXT("BandFillR") })
	{
		if (UImage* Layer = Cast<UImage>(GetWidgetFromName(Name))) BandFills.Add(Layer);
	}
	for (const TCHAR* Name : { TEXT("BandLineTopL"), TEXT("BandLineTopR"), TEXT("BandLineBottomL"), TEXT("BandLineBottomR") })
	{
		if (UImage* Layer = Cast<UImage>(GetWidgetFromName(Name))) BandLines.Add(Layer);
	}
}

void UPotentialTierUpOverlayWidget::Play(ELootBoxRarity From, ELootBoxRarity To, bool bPlaySound)
{
	ToRarity = To;
	Intensity = TierUpIntensity(To);
	bPlaySoundOnHit = bPlaySound;
	bReleased = false;
	bHitPlayed = false;
	FadeAt = TierUpHoldEnd;

	// 시안 변수 그대로 — 빛(--lt) = 등급색 45% + 흰색, 띠(--dk) = 등급색 70% + 남색 #0B1020
	const FLinearColor Grade = EmployeeRarityDisplay::VividColor(To);
	const FColor GradeHex = Grade.ToFColor(true);
	const FLinearColor Light = TierUpMixSRGB(FColor::White, GradeHex, .45f);
	BandDeep = TierUpMixSRGB(FColor(0x0B, 0x10, 0x20), GradeHex, .7f);
	for (const TWeakObjectPtr<UImage>& Fill : BandFills)
	{
		if (UImage* Layer = Fill.Get()) Layer->SetColorAndOpacity(BandDeep);
	}
	for (const TWeakObjectPtr<UImage>& Line : BandLines)
	{
		if (UImage* Layer = Line.Get()) Layer->SetColorAndOpacity(Light);
	}
	if (CoreGlow) CoreGlow->SetColorAndOpacity(Light);
	for (int32 Index = 0; Index < Stars.Num(); ++Index)
	{
		if (UImage* Star = Stars[Index].Get()) Star->SetColorAndOpacity(Index % 2 ? FLinearColor::White : Light);
	}
	if (FromText) FromText->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(From)));
	if (ToText)
	{
		ToText->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(To)));
		// 새 등급 글자는 흰 글자에 등급색 외곽 — 어떤 배경 위에서도 등급이 읽힌다
		FSlateFontInfo Font = ToText->GetFont();
		Font.OutlineSettings.OutlineColor = Grade;
		ToText->SetFont(Font);
	}

	Elapsed = 0.f;
	Apply(0.f);
}

void UPotentialTierUpOverlayWidget::Skip()
{
	if (Elapsed >= 0.f && Elapsed < FadeAt) FadeAt = Elapsed;
}

FReply UPotentialTierUpOverlayWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 연출 중 누름은 아래 판으로 새지 않는다 — 넘기기만 한다
	Skip();
	return FReply::Handled();
}

void UPotentialTierUpOverlayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Elapsed < 0.f || bFrozen) return;
	Elapsed += InDeltaTime;
	if (Apply(Elapsed)) Finish();
}

void UPotentialTierUpOverlayWidget::Release()
{
	if (bReleased) return;
	bReleased = true;
	OnRelease.ExecuteIfBound();
}

void UPotentialTierUpOverlayWidget::Finish()
{
	Elapsed = -1.f;
	Release();
	RemoveFromParent();
}

bool UPotentialTierUpOverlayWidget::Apply(float Time)
{
	const bool bReduce = USettingsManagerSubsystem::IsReduceMotion(this);
	if (Time >= FadeAt) Release();
	const float Keep = 1.f - FMath::Clamp((Time - FadeAt) / TierUpFadeTime, 0.f, 1.f);

	if (DimImage) DimImage->SetRenderOpacity(FMath::Min(TierUpDimAlpha, Time / .1f * TierUpDimAlpha) * Keep);

	if (Time >= TierUpHitTime && !bHitPlayed)
	{
		bHitPlayed = true;
		if (bPlaySoundOnHit)
		{
			if (USoundManagerSubsystem* SoundMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USoundManagerSubsystem>() : nullptr)
			{
				switch (ToRarity)
				{
				case ELootBoxRarity::Epic:      SoundMgr->PlayUISound(CGUISoundTags::GachaResultEpic); break;
				case ELootBoxRarity::Legendary:
				case ELootBoxRarity::Mythic:    SoundMgr->PlayUISound(CGUISoundTags::GachaResultLegendary); break;
				default:                        SoundMgr->PlayUISound(CGUISoundTags::GachaResultRare); break;
				}
			}
		}
	}

	UWidget* Words[] = { BandRoot, TitleText, FromChip, ArrowText, ToText };
	if (bReduce)
	{
		// 모션 줄이기: 칼끝·파편·빛 없이 딤과 띠·글자만 제자리에서 나타났다 걷힌다
		for (const TWeakObjectPtr<UImage>& Blade : ThrustBlades) TierUpHide(Blade.Get());
		for (const TWeakObjectPtr<UImage>& Blade : BurstBlades) TierUpHide(Blade.Get());
		for (const TWeakObjectPtr<UImage>& Star : Stars) TierUpHide(Star.Get());
		TierUpHide(CoreGlow);
		TierUpHide(BandSheen);
		const float In = FMath::Clamp(Time / TierUpReduceIn, 0.f, 1.f) * Keep;
		for (UWidget* Word : Words)
		{
			if (!Word) continue;
			Word->SetRenderTransform(FWidgetTransform());
			Word->SetRenderOpacity(In);
		}
		return Time >= FadeAt + TierUpFadeTime;
	}

	// 띠 한가운데 = 칼끝이 꽂히고 파편이 퍼지는 자리. 띠는 가운데를 축으로 가로만 바뀌므로 중심이 흔들리지 않는다
	FVector2D Center(-10000.f, -10000.f);
	if (BandRoot && FxFront)
	{
		const FGeometry& CanvasGeometry = FxFront->GetCachedGeometry();
		const FGeometry& BandGeometry = BandRoot->GetCachedGeometry();
		if (CanvasGeometry.GetLocalSize().X > 1.f && BandGeometry.GetLocalSize().X > 1.f)
		{
			Center = CanvasGeometry.AbsoluteToLocal(BandGeometry.LocalToAbsolute(BandGeometry.GetLocalSize() * .5f));
		}
	}
	const bool bCenter = Center.X > -9000.f;

	for (int32 Index = 0; Index < ThrustBlades.Num(); ++Index)
	{
		UImage* Blade = ThrustBlades[Index].Get();
		const float Phase = (Time - .012f * (Index % 4)) / TierUpThrustTravel;
		if (!Blade || !bCenter || Phase < 0.f || Index >= static_cast<int32>(UE_ARRAY_COUNT(TierUpThrustAngles))) { TierUpHide(Blade); continue; }
		const float Radians = FMath::DegreesToRadians(TierUpThrustAngles[Index]);
		const FVector2D Direction(FMath::Cos(Radians), FMath::Sin(Radians) * TierUpBladeSquash);
		const float After = FMath::Clamp((Phase - 1.f) / .7f, 0.f, 1.f);
		const float Radius = Phase < 1.f ? FMath::Lerp(1800.f, 340.f, Phase * Phase) : FMath::Lerp(340.f, 280.f, TierUpEaseOut(After));
		const float Alpha = Phase < 1.f ? FMath::Clamp(Phase * 3.f, 0.f, 1.f) : 1.f - After;
		const float Length = Phase < 1.f ? 900.f : FMath::Lerp(900.f, 140.f, TierUpEaseOut(After));
		TierUpDriveBlade(Blade, Center + Direction * Radius, FMath::Atan2(-Direction.Y, -Direction.X), Length, 30.f, Alpha * Intensity * Keep);
	}
	for (int32 Index = 0; Index < BurstBlades.Num(); ++Index)
	{
		UImage* Blade = BurstBlades[Index].Get();
		const float Phase = (Time - TierUpHitTime - Index * .006f) / .22f;
		if (!Blade || !bCenter || Phase < 0.f || Phase > 1.f || Index >= static_cast<int32>(UE_ARRAY_COUNT(TierUpBurstAngles))) { TierUpHide(Blade); continue; }
		const float Radians = FMath::DegreesToRadians(TierUpBurstAngles[Index]);
		const FVector2D Direction(FMath::Cos(Radians), FMath::Sin(Radians) * TierUpBladeSquash);
		TierUpDriveBlade(Blade, Center + Direction * FMath::Lerp(220.f, 820.f, TierUpEaseOut(Phase)), FMath::Atan2(Direction.Y, Direction.X),
			FMath::Lerp(320.f, 80.f, Phase), 18.f, (1.f - Phase) * Intensity * Keep);
	}

	// 원광 — 커지지 않고 제자리에서 번쩍인다
	if (CoreGlow)
	{
		const float K = (Time - TierUpHitTime) / .34f;
		if (!bCenter || K < 0.f || K >= 1.f)
		{
			TierUpHide(CoreGlow);
		}
		else
		{
			CoreGlow->SetRenderTransform(FWidgetTransform(Center, FVector2D(1.f, 1.f), FVector2D::ZeroVector, 0.f));
			const float Curve = K < .12f ? K / .12f : FMath::Square(1.f - (K - .12f) / .88f);
			CoreGlow->SetRenderOpacity(FMath::Clamp(Curve * .85f * Intensity, 0.f, 1.f) * Keep);
		}
	}

	// 띠 — 착지에 그 자리에서 바로 켜지고, 흰빛에서 등급색으로 0.22초에 가라앉는다
	if (BandRoot)
	{
		const float K = (Time - TierUpHitTime) / .04f;
		BandRoot->SetRenderOpacity((K < 0.f ? 0.f : FMath::Min(1.f, K)) * Keep);
		// 시안처럼 흰빛(최대 85%)을 sRGB 에서 덮었다 걷는다 — 선형 보간은 같은 시점에 훨씬 하얗게 남는다
		const float Flash = .85f * (1.f - TierUpEaseOut((Time - TierUpHitTime) / .22f));
		const FLinearColor Fill = TierUpMixSRGB(BandDeep.ToFColor(true), FColor::White, Flash);
		for (const TWeakObjectPtr<UImage>& BandFill : BandFills)
		{
			if (UImage* Layer = BandFill.Get()) Layer->SetColorAndOpacity(Fill);
		}
	}
	if (BandSheen && BandRoot)
	{
		const float K = (Time - .3f) / .35f;
		const float BandWidth = static_cast<float>(BandRoot->GetCachedGeometry().GetLocalSize().X);
		const float SheenWidth = FMath::Max(1.f, static_cast<float>(BandSheen->GetCachedGeometry().GetLocalSize().X));
		if (K >= 0.f && K < 1.f && BandWidth > 1.f)
		{
			BandSheen->SetRenderTransform(FWidgetTransform(FVector2D(FMath::Lerp(-SheenWidth, BandWidth, K), 0.f),
				FVector2D(1.f, 1.f), FVector2D::ZeroVector, 0.f));
			BandSheen->SetRenderOpacity(FMath::Sin(PI * K) * .6f);
		}
		else
		{
			TierUpHide(BandSheen);
		}
	}

	// 글자는 떠오르지 않는다 — 제자리에서 나타난다
	if (TitleText)
	{
		TitleText->SetRenderOpacity((Time < .16f ? 0.f : TierUpEaseOut((Time - .16f) / .15f)) * Keep);
	}
	const float FromAlpha = (Time < TierUpHitTime ? 0.f : TierUpEaseOut((Time - TierUpHitTime) / .15f)) * Keep;
	if (FromChip) FromChip->SetRenderOpacity(FromAlpha);
	if (ArrowText) ArrowText->SetRenderOpacity(FromAlpha);

	// 새 등급 글자 — 커지거나 떠오르지 않는다. 위에서 가속해 내리꽂혀 그대로 선다
	if (ToText)
	{
		const float K = (Time - TierUpHitTime) / .1f;
		if (K < 0.f)
		{
			TierUpHide(ToText);
		}
		else
		{
			const float Drop = K < 1.f ? -60.f * (1.f - K * K) : 0.f;
			ToText->SetRenderTransform(FWidgetTransform(FVector2D(0.f, Drop), FVector2D(1.f, 1.f), FVector2D::ZeroVector, 0.f));
			ToText->SetRenderOpacity(FMath::Min(1.f, K) * Keep);
		}
	}

	for (int32 Index = 0; Index < Stars.Num(); ++Index)
	{
		UImage* Star = Stars[Index].Get();
		const float K = (Time - TierUpHitTime - .02f * (Index % 3)) / .6f;
		if (!Star || !bCenter || K < 0.f || K >= 1.f) { TierUpHide(Star); continue; }
		const float Radians = FMath::DegreesToRadians(Index * 360.f / Stars.Num() + (Index % 2 ? 10.f : -8.f));
		const FVector2D Direction(FMath::Cos(Radians), FMath::Sin(Radians) * .55f);
		const float Distance = FMath::Lerp(100.f, (Index % 2 ? 520.f : 700.f) * Intensity, TierUpEaseOut(K));
		const float Scale = 1.f - .6f * K;
		Star->SetRenderTransform(FWidgetTransform(Center + Direction * Distance, FVector2D(Scale, Scale), FVector2D::ZeroVector, 0.f));
		Star->SetRenderOpacity((K < .08f ? K / .08f : 1.f - (K - .08f) / .92f) * Keep);
	}

	return Time >= FadeAt + TierUpFadeTime;
}

#if !UE_BUILD_SHIPPING
void UPotentialTierUpOverlayWidget::DebugSeek(float Time)
{
	bFrozen = true;
	bHitPlayed = true;
	Apply(FMath::Max(0.f, Time));
}

bool UPotentialTierUpOverlayWidget::DebugIsLive(FString& OutDetail) const
{
	OutDetail = FString::Printf(TEXT("thrust=%d burst=%d stars=%d fills=%d lines=%d to_opacity=%.2f band_opacity=%.2f dim=%.2f"),
		ThrustBlades.Num(), BurstBlades.Num(), Stars.Num(), BandFills.Num(), BandLines.Num(),
		ToText ? ToText->GetRenderOpacity() : -1.f, BandRoot ? BandRoot->GetRenderOpacity() : -1.f, DimImage ? DimImage->GetRenderOpacity() : -1.f);
	return ThrustBlades.Num() == 12 && BurstBlades.Num() == 8 && Stars.Num() == 10 && BandFills.Num() == 3
		&& ToText && ToText->GetRenderOpacity() > .5f && BandRoot && BandRoot->GetRenderOpacity() > .5f;
}
#endif
