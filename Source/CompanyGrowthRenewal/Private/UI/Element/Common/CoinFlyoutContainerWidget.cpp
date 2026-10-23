// 코인 플라이아웃 컨테이너 위젯
// WBP 없이 순수 C++로 위젯 트리 구성 (NotificationContainer와 동일 패턴)

#include "UI/Element/Common/CoinFlyoutContainerWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Texture2D.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "Global/GlobalUtilFunctions.h"
#include "UI/Element/Common/IncomeGlyphMetrics.h"

static constexpr float IncomeGlyphDisplayScale = 1.50f;

bool UCoinFlyoutContainerWidget::Initialize()
{
	bool bSuccess = Super::Initialize();
	if (!bSuccess) return false;

	// WidgetTree에 RootWidget으로 CanvasPanel 직접 생성
	CoinCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CoinCanvas"));
	if (CoinCanvas)
	{
		WidgetTree->RootWidget = CoinCanvas;
	}

	IncomeGlyphMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/UI/Materials/DamageNumber/M_UI_IncomeGlyph.M_UI_IncomeGlyph"))).LoadSynchronous();
	if (!IncomeGlyphMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CoinFlyout] M_UI_IncomeGlyph material is missing"));
	}

	return bSuccess;
}

void UCoinFlyoutContainerWidget::StartFlyout(FVector2D InTargetPos, int32 InNumCoins, UTexture2D* InCoinTexture)
{
	if (bAnimating || InNumCoins <= 0) return;

	// InTargetPos는 Absolute 좌표 (ResourceWidget::CalculateIconScreenPos에서)
	TargetPosition_Absolute = InTargetPos;
	TotalCoins = InNumCoins;
	CoinsCompleted = 0;
	TotalElapsed = 0.0f;

	// 뷰포트 중앙을 Absolute 좌표로 계산
	FGeometry ViewportGeo = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	FVector2D ViewportCenter = ViewportGeo.GetLocalSize() * 0.5f;
	FVector2D AbsoluteCenter = ViewportGeo.LocalToAbsolute(ViewportCenter);

	CoinAnimations.Empty();
	CoinAnimations.Reserve(InNumCoins);

	for (int32 i = 0; i < InNumCoins; ++i)
	{
		FCoinAnimData AnimData;

		// 코인 이미지 생성
		UImage* CoinImg = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), *FString::Printf(TEXT("Coin_%d"), i));
		if (!CoinImg || !CoinCanvas) continue;

		// 텍스처 설정 + 브러시 ImageSize를 CoinSize로 강제 (텍스처 원본 크기 무시)
		if (InCoinTexture)
		{
			CoinImg->SetBrushFromTexture(InCoinTexture);
		}
		FSlateBrush CoinBrush = CoinImg->GetBrush();
		CoinBrush.ImageSize = FVector2D(CoinSize, CoinSize);
		CoinImg->SetBrush(CoinBrush);
		CoinImg->SetVisibility(ESlateVisibility::Hidden);
		CoinImg->SetRenderScale(FVector2D(0.5f, 0.5f));

		// CanvasPanel에 추가
		UCanvasPanelSlot* CanvasSlot = CoinCanvas->AddChildToCanvas(CoinImg);
		if (CanvasSlot)
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		}
		AnimData.CachedSlot = CanvasSlot;

		// 팝아웃 도착 위치: 화면 중앙에서 랜덤 오프셋 (Absolute 스케일). 안쪽 0.15R~R 로 디스크를 넓게 채움(넓은 구역 분출).
		float Angle = FMath::FRandRange(0.0f, 360.0f);
		float Radius = FMath::FRandRange(SpawnRadius * 0.15f, SpawnRadius);
		FVector2D Offset(
			FMath::Cos(FMath::DegreesToRadians(Angle)) * Radius,
			FMath::Sin(FMath::DegreesToRadians(Angle)) * Radius
		);

		AnimData.StartPos_Absolute = AbsoluteCenter;
		AnimData.SpawnPos_Absolute = AbsoluteCenter + Offset;

		AnimData.CoinImage = CoinImg;
		AnimData.Delay = i * StaggerDelay;
		// 비행 시간에 큰 랜덤 편차 → 한번에 터져도 도착이 퍼져 카운터가 다다닥 롤업(음수 방지 하한)
		AnimData.Duration = FMath::Max(0.15f, FlightDuration + FMath::FRandRange(-ArrivalSpread, ArrivalSpread));
		AnimData.Elapsed = 0.0f;
		AnimData.bStarted = false;
		AnimData.bCompleted = false;

		CoinAnimations.Add(AnimData);
	}

	bAnimating = true;
}

void UCoinFlyoutContainerWidget::SpawnStreamCoin(FVector2D StartAbsolute, FVector2D TargetAbsolute, UTexture2D* InCoinTexture, int32 SourceId)
{
	if (!CoinCanvas) return;
	// 동시 비행 상한 — 여기서 걸러도 재화는 이미 적립돼 다음 착지 갱신에 합류
	if (StreamCoins.Num() >= MaxStreamCoins) return;

	// 스로틀은 SourceId 별 — 직원마다 자기 게이트를 가져야 전원이 골고루 코인을 뿜는다.
	const float SpawnNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (const float* LastSpawn = LastStreamSpawnBySource.Find(SourceId))
	{
		if (SpawnNow - *LastSpawn < StreamSpawnMinGap) return;
	}
	LastStreamSpawnBySource.Add(SourceId, SpawnNow);

	UImage* CoinImg = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), *FString::Printf(TEXT("StreamCoin_%d"), StreamCoinIdCounter++));
	if (!CoinImg) return;

	if (InCoinTexture)
	{
		CoinImg->SetBrushFromTexture(InCoinTexture);
	}
	FSlateBrush CoinBrush = CoinImg->GetBrush();
	CoinBrush.ImageSize = FVector2D(StreamCoinDrawSize, StreamCoinDrawSize);
	CoinImg->SetBrush(CoinBrush);
	CoinImg->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	CoinImg->SetRenderScale(FVector2D(0.4f, 0.4f));

	UCanvasPanelSlot* CanvasSlot = CoinCanvas->AddChildToCanvas(CoinImg);
	if (!CanvasSlot)
	{
		CoinImg->RemoveFromParent();
		return;
	}
	CanvasSlot->SetAutoSize(true);
	CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CanvasSlot->SetPosition(GetCachedGeometry().AbsoluteToLocal(StartAbsolute));

	FStreamCoinData Data;
	Data.CoinImage = CoinImg;
	Data.CachedSlot = CanvasSlot;
	Data.StartAbs = StartAbsolute;
	Data.TargetAbs = TargetAbsolute;
	Data.Elapsed = 0.0f;
	Data.Duration = StreamFlightDuration + FMath::FRandRange(-0.1f, 0.1f);
	StreamCoins.Add(Data);
}

void UCoinFlyoutContainerWidget::UpdateIncomeGlyphColors(FLinearColor MoneyColor)
{
	if (CachedIncomeGlyphColor.Equals(MoneyColor)) return;

	const float Lum = 0.2126f * MoneyColor.R + 0.7152f * MoneyColor.G + 0.0722f * MoneyColor.B;
	auto Saturated = [Lum](float C) { return FMath::Clamp(Lum + (C - Lum) * 1.65f, 0.0f, 1.0f) * 0.85f; };
	const FLinearColor Base(Saturated(MoneyColor.R), Saturated(MoneyColor.G), Saturated(MoneyColor.B), 1.0f);
	auto Brightened = [](float C) { return FMath::Min(C * 1.35f, 1.0f); };

	for (const TPair<int32, UMaterialInstanceDynamic*>& Entry : IncomeGlyphMIDs)
	{
		UMaterialInstanceDynamic* MID = Entry.Value;
		MID->SetVectorParameterValue(TEXT("TopColor"),
			FLinearColor(Brightened(Base.R), Brightened(Base.G), Brightened(Base.B), 1.0f));
		MID->SetVectorParameterValue(TEXT("MidColor"), Base);
		MID->SetVectorParameterValue(TEXT("BottomColor"),
			FLinearColor(Base.R * 0.5f, Base.G * 0.5f, Base.B * 0.5f, 1.0f));
		MID->SetScalarParameterValue(TEXT("MidStop"), 0.44f);
	}
	CachedIncomeGlyphColor = MoneyColor;
}

UMaterialInstanceDynamic* UCoinFlyoutContainerWidget::GetIncomeGlyphMID(int32 GlyphIndex, FLinearColor MoneyColor)
{
	if (!IncomeGlyphMaterial) return nullptr;
	if (UMaterialInstanceDynamic** Cached = IncomeGlyphMIDs.Find(GlyphIndex))
	{
		UpdateIncomeGlyphColors(MoneyColor);
		return *Cached;
	}

	const TCHAR* AssetName = IncomeGlyphMetrics::Glyphs[GlyphIndex].AssetName;
	const FString TexturePath = FString::Printf(
		TEXT("/Game/CompanyGrowth/UI/Textures/Effect/IncomeGlyph/48/T_UI_IncomeGlyph_%s.T_UI_IncomeGlyph_%s"),
		AssetName, AssetName);
	UTexture2D* Texture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TexturePath)).LoadSynchronous();
	if (!Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CoinFlyout] Missing income glyph texture: %s"), *TexturePath);
		return nullptr;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(IncomeGlyphMaterial, this);
	if (!MID) return nullptr;
	MID->SetTextureParameterValue(TEXT("Glyph"), Texture);
	MID->SetVectorParameterValue(TEXT("RimColor"), FLinearColor::Black);
	MID->SetVectorParameterValue(TEXT("KeyColor"), FLinearColor::White);
	MID->SetScalarParameterValue(TEXT("GlossStrength"), 0.18f);
	MID->SetScalarParameterValue(TEXT("GlossPos"), 0.18f);
	MID->SetScalarParameterValue(TEXT("GlossWidth"), 0.18f);
	IncomeGlyphMIDs.Add(GlyphIndex, MID);
	CachedIncomeGlyphColor = FLinearColor::Transparent;
	UpdateIncomeGlyphColors(MoneyColor);
	return MID;
}

void UCoinFlyoutContainerWidget::SpawnIncomeText(
	FVector2D StartAbsolute,
	int64 Amount,
	UTexture2D* InMoneyTexture,
	FLinearColor MoneyColor)
{
	if (!CoinCanvas || Amount <= 0) return;
	const FString AmountLabel = Amount < 1000
		? FString::Printf(TEXT("+%lld"), Amount)
		: Amount < 10000
			? FString::Printf(TEXT("+%lld,%03lld"), Amount / 1000, Amount % 1000)
			: UGlobalUtilFunctions::FormatFundsAmount(Amount, true).ToString();
	TArray<int32> GlyphIndices;
	GlyphIndices.Reserve(AmountLabel.Len());
	float AdvanceWidth = 0.0f;
	for (TCHAR Character : AmountLabel)
	{
		int32 GlyphIndex = INDEX_NONE;
		for (int32 Candidate = 0; Candidate < UE_ARRAY_COUNT(IncomeGlyphMetrics::Glyphs); ++Candidate)
		{
			if (IncomeGlyphMetrics::Glyphs[Candidate].Character == Character)
			{
				GlyphIndex = Candidate;
				break;
			}
		}
		if (GlyphIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CoinFlyout] Unsupported income glyph: %c"), Character);
			return;
		}
		GlyphIndices.Add(GlyphIndex);
		AdvanceWidth += IncomeGlyphMetrics::Glyphs[GlyphIndex].Advance * IncomeGlyphDisplayScale;
	}
	if (!IncomeGlyphMaterial || GlyphIndices.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CoinFlyout] Income glyph row cannot be drawn"));
		return;
	}

	// 전역 상한 — 오래된 것부터 제거
	while (IncomeTexts.Num() >= MaxIncomeTexts)
	{
		if (IncomeTexts[0].Row)
		{
			IncomeTexts[0].Row->RemoveFromParent();
		}
		IncomeTexts.RemoveAt(0);
	}

	const int32 IncomeId = StreamCoinIdCounter++;
	UHorizontalBox* IncomeRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), *FString::Printf(TEXT("IncomeRow_%d"), IncomeId));
	if (!IncomeRow) return;

	if (InMoneyTexture)
	{
		UImage* IncomeIcon = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), *FString::Printf(TEXT("IncomeIcon_%d"), IncomeId));
		if (IncomeIcon)
		{
			IncomeIcon->SetBrushFromTexture(InMoneyTexture);
			IncomeIcon->SetDesiredSizeOverride(FVector2D(IncomeIconSize, IncomeIconSize));
			FSlateBrush IconBrush = IncomeIcon->GetBrush();
			IconBrush.ImageSize = FVector2D(IncomeIconSize, IncomeIconSize);
			IncomeIcon->SetBrush(IconBrush);
			IncomeIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			if (UHorizontalBoxSlot* IconBoxSlot = IncomeRow->AddChildToHorizontalBox(IncomeIcon))
			{
				IconBoxSlot->SetPadding(FMargin(0.0f, 0.0f, IncomeIconGap, 0.0f));
				IconBoxSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}

	USizeBox* GlyphBounds = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), *FString::Printf(TEXT("IncomeGlyphBounds_%d"), IncomeId));
	UCanvasPanel* GlyphCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), *FString::Printf(TEXT("IncomeGlyphCanvas_%d"), IncomeId));
	if (!GlyphBounds || !GlyphCanvas) return;
	const float Inset = (IncomeGlyphMetrics::RasterPadding - IncomeGlyphMetrics::Rim) * IncomeGlyphDisplayScale;
	GlyphBounds->SetWidthOverride(AdvanceWidth + 2.0f * IncomeGlyphMetrics::Rim * IncomeGlyphDisplayScale);
	GlyphBounds->SetHeightOverride(IncomeGlyphMetrics::BoxHeight * IncomeGlyphDisplayScale - 2.0f * Inset);
	GlyphBounds->AddChild(GlyphCanvas);

	float Pen = 0.0f;
	for (int32 GlyphOffset = 0; GlyphOffset < GlyphIndices.Num(); ++GlyphOffset)
	{
		const int32 GlyphIndex = GlyphIndices[GlyphOffset];
		const IncomeGlyphMetrics::FGlyph& Glyph = IncomeGlyphMetrics::Glyphs[GlyphIndex];
		UMaterialInstanceDynamic* MID = GetIncomeGlyphMID(GlyphIndex, MoneyColor);
		if (!MID)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CoinFlyout] Income glyph row failed for '%s'"), *AmountLabel);
			return;
		}
		UImage* GlyphImage = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), *FString::Printf(TEXT("IncomeGlyph_%d_%d"), IncomeId, GlyphOffset));
		if (!GlyphImage) return;
		GlyphImage->SetBrushFromMaterial(MID);
		FSlateBrush GlyphBrush = GlyphImage->GetBrush();
		GlyphBrush.ImageSize = FVector2D(Glyph.BoxWidth, IncomeGlyphMetrics::BoxHeight) * IncomeGlyphDisplayScale;
		GlyphImage->SetBrush(GlyphBrush);
		GlyphImage->SetDesiredSizeOverride(GlyphBrush.ImageSize);
		GlyphImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UCanvasPanelSlot* GlyphCanvasSlot = GlyphCanvas->AddChildToCanvas(GlyphImage))
		{
			GlyphCanvasSlot->SetAutoSize(false);
			GlyphCanvasSlot->SetSize(GlyphBrush.ImageSize);
			GlyphCanvasSlot->SetPosition(FVector2D(Pen - Inset, -Inset));
		}
		Pen += Glyph.Advance * IncomeGlyphDisplayScale;
	}
	if (UHorizontalBoxSlot* GlyphBoxSlot = IncomeRow->AddChildToHorizontalBox(GlyphBounds))
	{
		GlyphBoxSlot->SetVerticalAlignment(VAlign_Center);
	}

	UCanvasPanelSlot* RowCanvasSlot = CoinCanvas->AddChildToCanvas(IncomeRow);
	if (!RowCanvasSlot)
	{
		IncomeRow->RemoveFromParent();
		return;
	}
	RowCanvasSlot->SetAutoSize(true);
	RowCanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));

	// 같은 자리 스트로브 방지 지터
	FIncomeTextData Data;
	Data.Row = IncomeRow;
	Data.CachedSlot = RowCanvasSlot;
	Data.StartAbs = StartAbsolute + FVector2D(
		FMath::FRandRange(-18.0f, 18.0f), FMath::FRandRange(-8.0f, 6.0f));
	Data.Elapsed = 0.0f;
	Data.Duration = IncomeTextLifetime;
	RowCanvasSlot->SetPosition(GetCachedGeometry().AbsoluteToLocal(Data.StartAbs));
	IncomeTexts.Add(Data);
}

void UCoinFlyoutContainerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// === 수익 텍스트 "+N" (상승 + 잔잔한 팝 + 페이드) ===
	for (int32 i = IncomeTexts.Num() - 1; i >= 0; --i)
	{
		FIncomeTextData& T = IncomeTexts[i];
		T.Elapsed += InDeltaTime;
		const float A = FMath::Clamp(T.Elapsed / T.Duration, 0.0f, 1.0f);

		const float RiseT = 1.0f - FMath::Square(1.0f - A);   // ease-out 상승
		const FVector2D CurAbs = T.StartAbs - FVector2D(0.0f, IncomeTextRise * RiseT);
		if (T.CachedSlot)
		{
			T.CachedSlot->SetPosition(MyGeometry.AbsoluteToLocal(CurAbs));
		}
		if (T.Row)
		{
			// 오버슈트 없는 잔잔한 팝인 (상시 요소)
			const float Scale = (T.Elapsed < 0.12f) ? FMath::Lerp(0.6f, 1.0f, T.Elapsed / 0.12f) : 1.0f;
			T.Row->SetRenderScale(FVector2D(Scale, Scale));
			const float Opacity = (A < IncomeTextFadeStart)
				? 1.0f
				: 1.0f - (A - IncomeTextFadeStart) / (1.0f - IncomeTextFadeStart);
			T.Row->SetRenderOpacity(Opacity);
		}
		if (A >= 1.0f)
		{
			if (T.Row)
			{
				T.Row->RemoveFromParent();
			}
			IncomeTexts.RemoveAt(i);
		}
	}

	// === 스트리밍 코인 (운영 수익 드립 — 원샷 bAnimating 과 독립) ===
	for (int32 i = StreamCoins.Num() - 1; i >= 0; --i)
	{
		FStreamCoinData& Coin = StreamCoins[i];
		Coin.Elapsed += InDeltaTime;
		const float Alpha = FMath::Clamp(Coin.Elapsed / Coin.Duration, 0.0f, 1.0f);
		const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

		// 시작점 위로 볼록한 2차 베지어 — 머리 위로 살짝 팝 후 아이콘으로 비행
		const FVector2D Ctrl = Coin.StartAbs + FVector2D(0.0f, -StreamPopHeight);
		const FVector2D P01 = FMath::Lerp(Coin.StartAbs, Ctrl, Eased);
		const FVector2D P12 = FMath::Lerp(Ctrl, Coin.TargetAbs, Eased);
		const FVector2D CurrentAbs = FMath::Lerp(P01, P12, Eased);

		if (Coin.CachedSlot)
		{
			Coin.CachedSlot->SetPosition(MyGeometry.AbsoluteToLocal(CurrentAbs));
		}
		if (Coin.CoinImage)
		{
			const float Scale = (Alpha < 0.2f)
				? FMath::Lerp(0.4f, 1.1f, Alpha / 0.2f)
				: FMath::Lerp(1.1f, 0.85f, (Alpha - 0.2f) / 0.8f);
			Coin.CoinImage->SetRenderScale(FVector2D(Scale, Scale));
		}

		if (Alpha >= 1.0f)
		{
			if (Coin.CoinImage)
			{
				Coin.CoinImage->RemoveFromParent();
			}
			StreamCoins.RemoveAt(i);

			PlayCoinArriveSound(StreamSoundMinGap);
			OnStreamCoinArrived.ExecuteIfBound();
		}
	}

	if (!bAnimating) return;

	TotalElapsed += InDeltaTime;

	for (int32 i = 0; i < CoinAnimations.Num(); ++i)
	{
		FCoinAnimData& Coin = CoinAnimations[i];

		if (Coin.bCompleted) continue;

		// 출발 지연 체크
		if (!Coin.bStarted)
		{
			if (TotalElapsed >= Coin.Delay)
			{
				Coin.bStarted = true;
				Coin.Elapsed = 0.0f;
				if (Coin.CoinImage)
				{
					Coin.CoinImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				}
			}
			continue;
		}

		Coin.Elapsed += InDeltaTime;

		FVector2D CurrentAbsolute;
		float Scale;

		if (Coin.Elapsed < PopOutDuration)
		{
			// Phase 1: 중앙에서 SpawnPos로 팝아웃
			float PopAlpha = FMath::Clamp(Coin.Elapsed / PopOutDuration, 0.0f, 1.0f);
			float EasedPop = FMath::InterpEaseOut(0.0f, 1.0f, PopAlpha, 3.0f);

			CurrentAbsolute = FMath::Lerp(Coin.StartPos_Absolute, Coin.SpawnPos_Absolute, EasedPop);
			Scale = FMath::Lerp(0.5f, 1.4f, EasedPop);
		}
		else
		{
			// Phase 2: SpawnPos에서 타겟으로 비행
			float FlyElapsed = Coin.Elapsed - PopOutDuration;
			float FlyAlpha = FMath::Clamp(FlyElapsed / Coin.Duration, 0.0f, 1.0f);
			float EasedFly = FMath::InterpEaseInOut(0.0f, 1.0f, FlyAlpha, 2.5f);

			CurrentAbsolute = FMath::Lerp(Coin.SpawnPos_Absolute, TargetPosition_Absolute, EasedFly);
			Scale = FMath::Lerp(1.4f, 1.0f, EasedFly);

			// 도착 체크
			if (FlyAlpha >= 1.0f)
			{
				Coin.bCompleted = true;
				OnSingleCoinArrived(i);
				continue;
			}
		}

		// Absolute → Canvas 로컬 좌표 변환
		FVector2D LocalPos = MyGeometry.AbsoluteToLocal(CurrentAbsolute);

		if (Coin.CoinImage)
		{
			if (Coin.CachedSlot)
			{
				Coin.CachedSlot->SetPosition(LocalPos);
			}
			Coin.CoinImage->SetRenderScale(FVector2D(Scale, Scale));
		}
	}
}

void UCoinFlyoutContainerWidget::PlayCoinArriveSound(float MinGap)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now - LastCoinArriveSoundTime < MinGap)
	{
		return;
	}

	// 조용하다 첫 도착 = 또렷(1.0), 연타는 곱연산으로 감쇠 — 100개 도착이 겹쳐도 청각적으로 하나의 "촤르륵"
	CoinArriveSoundVolume = (Now - LastCoinArriveSoundTime >= CoinSoundResetGap)
		? 1.0f
		: FMath::Max(CoinSoundVolumeFloor, CoinArriveSoundVolume * CoinSoundDecay);
	LastCoinArriveSoundTime = Now;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISoundWithParams(CGUISoundTags::RewardCoin,
				CoinArriveSoundVolume, FMath::FRandRange(0.94f, 1.06f));
		}
	}
}

void UCoinFlyoutContainerWidget::OnSingleCoinArrived(int32 CoinIndex)
{
	if (CoinAnimations.IsValidIndex(CoinIndex) && CoinAnimations[CoinIndex].CoinImage)
	{
		CoinAnimations[CoinIndex].CoinImage->SetVisibility(ESlateVisibility::Hidden);
	}

	PlayCoinArriveSound(CoinSoundMinGap);

	CoinsCompleted++;
	OnCoinArrived.ExecuteIfBound(CoinIndex);

	if (CoinsCompleted >= TotalCoins)
	{
		bAnimating = false;
		OnAllCoinsComplete.ExecuteIfBound();

		// FadeOutDelay 후 자동 제거 — raw this 람다는 UI teardown(월드 유지) 시 dangling 크래시 → WeakLambda
		if (UWorld* World = GetWorld())
		{
			FTimerHandle RemoveTimerHandle;
			World->GetTimerManager().SetTimer(
				RemoveTimerHandle,
				FTimerDelegate::CreateWeakLambda(this, [this]()
				{
					RemoveFromParent();
				}),
				FadeOutDelay,
				false
			);
		}
	}
}
