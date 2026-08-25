// Score Orb 비행 연출 컨테이너 위젯
// WBP_ScoreOrbContainer에서 머티리얼 참조, C++에서 WidgetTree 구성

#include "UI/Element/Effects/ScoreOrbContainerWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"
#include "Materials/MaterialInterface.h"
#include "Global/GlobalUtilFunctions.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "Manager/SoundManagerSubsystem.h"

// 점수 숫자 튜닝 상수 — cpp-only 라 값 변경 시 Live Coding 주입 가능 (에디터 닫을 필요 X)
static constexpr int32 MaxScoreNumbers = 24;        // 전역 상한
static constexpr float NumberLifetime = 1.2f;
static constexpr float NumberRise = 95.0f;          // 상승 픽셀
static constexpr int32 NumberFontSize = 58;
static constexpr int32 NumberOutlineSize = 2;       // 검은 외곽선 (얇게 — 두꺼우면 뭉개짐)
static constexpr float NumberPopInTime = 0.18f;     // EaseOutBack 바운스 팝 시간
static constexpr float NumberFadeStart = 0.6f;      // 수명의 이 지점부터 페이드
static constexpr float CritNumberLifetime = 1.6f;
static constexpr float CritNumberRise = 120.0f;
static constexpr int32 CritNumberFontSize = 92;
static constexpr int32 CritNumberOutlineSize = 3;
static constexpr int32 MaxConfettiPieces = 90;      // 모바일 상한
static constexpr float ConfettiGravity = 1800.0f;
static constexpr float ConfettiFadeTail = 0.4f;
// MedalBurst 튜닝 (cpp-only = Live Coding 주입 가능)
static constexpr float MedalBurstSpawnRadiusMin = 170.0f;   // 메달 뒤 원환 스폰 (중앙 숫자 가림 방지)
static constexpr float MedalBurstSpawnRadiusMax = 240.0f;
static constexpr float MedalBurstSpeedMin = 420.0f;
static constexpr float MedalBurstSpeedMax = 1150.0f;
static constexpr float MedalBurstStarRatio = 0.35f;         // 스타 조각 비율 (나머지 = 종이)
static constexpr float StampPopInTime = 0.22f;
static constexpr float StampHoldTime = 0.5f;
static constexpr float StampFadeTime = 0.3f;

// === Orb 산출음 (DT_GameSFX 행 이름 — 이웃 연출음 Event_ScoreSweep/Dev_Crit 과 같은 경로) ===
// 소리 교체는 이 배열만: 세라믹 톡으로 바꾸려면 { TEXT("ScoreOrb_Land_1"), TEXT("ScoreOrb_Land_2") }
static const FName ScoreOrbSoundTags[] = { TEXT("Event_ScoreSweep") };
// Event_ScoreSweep 행(0.55)은 크리/출시결과 리빌과 공유 — DT 를 낮추면 그쪽까지 죽으므로 여기서만 스케일
static constexpr float ScoreOrbSoundVolumeScale = 0.5f;
static constexpr float ScoreOrbSoundPitchJitter = 0.05f;   // 초당 반복이라 무피치변화면 기계음으로 들림
static constexpr float ScoreOrbSoundMinGap = 0.25f;        // 샘플이 0.55s — 간격이 짧으면 겹쳐서 뭉개짐

// 버스트당 1회, **스폰 시점**. 착지(비행 1.0s 뒤)에 걸면 "+N" 숫자 팝과 1초 어긋나 늦게 들림.
// 스로틀 상태를 멤버 대신 file-scope 로 둔 이유: 컨테이너가 Office 당 1개라 멤버와 동등하고,
// 헤더 불변 = 값 튜닝을 Live Coding 으로 주입 가능.
static void PlayScoreOrbSound(UWorld* World)
{
	if (!World) return;

	static float LastPlayTime = -1000.0f;
	const float Now = World->GetTimeSeconds();
	// PIE 재시작으로 월드시간이 되감기면 스로틀이 영구 잠기므로 리셋
	if (Now < LastPlayTime) { LastPlayTime = -1000.0f; }
	if (Now - LastPlayTime < ScoreOrbSoundMinGap) return;
	LastPlayTime = Now;

	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return;

	if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
	{
		const int32 Idx = FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(ScoreOrbSoundTags)) - 1);
		SoundMgr->PlaySoundWithVolume(ScoreOrbSoundTags[Idx], ScoreOrbSoundVolumeScale,
			FMath::FRandRange(1.0f - ScoreOrbSoundPitchJitter, 1.0f + ScoreOrbSoundPitchJitter));
	}
}

bool UScoreOrbContainerWidget::Initialize()
{
	bool bSuccess = Super::Initialize();
	if (!bSuccess) return false;

	OrbCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("OrbCanvas"));
	if (OrbCanvas)
	{
		WidgetTree->RootWidget = OrbCanvas;
	}

	// WBP의 UPROPERTY에 지정된 머티리얼을 로드 (에디터 참조 → 쿠킹 자동 포함)
	for (int32 i = 0; i < FMath::Min(OrbMaterialRefs.Num(), 3); ++i)
	{
		if (!OrbMaterialRefs[i].IsNull())
		{
			CachedOrbMaterials[i] = OrbMaterialRefs[i].LoadSynchronous();
		}
	}

	// 점수 숫자용 폰트 = Lilita One (둥글고 묵직한 데미지 폰트, OFL). UFont 래퍼 참조.
	// Latin 전용 → Western 축약(K/M) 사용. /Game/CompanyGrowth/Font 아래라 쿠킹 안전.
	NumberFont = TSoftObjectPtr<UObject>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Font/LilitaOne_Font.LilitaOne_Font"))).LoadSynchronous();

	return bSuccess;
}

FGeometry UScoreOrbContainerWidget::GetCanvasGeometry() const
{
	if (OrbCanvas)
	{
		return OrbCanvas->GetCachedGeometry();
	}
	return FGeometry();
}

void UScoreOrbContainerWidget::SpawnOrbBurst(FVector2D StartPos, FVector2D TargetPos,
	int32 StepNumber, bool bIsCritical, int32 OrbCount)
{
	if (!OrbCanvas) return;

	// 크리는 EmployeeBehaviorComponent 가 같은 시점에 Event_ScoreSweep 을 울림 (한 액션 한 사운드)
	if (!bIsCritical)
	{
		PlayScoreOrbSound(GetWorld());
	}

	// 크리티컬이면 개수 1.5배
	int32 ActualCount = bIsCritical ? FMath::CeilToInt(OrbCount * 1.5f) : OrbCount;

	for (int32 i = 0; i < ActualCount; ++i)
	{
		// 시작 위치를 약간씩 랜덤 오프셋 (퍼지는 느낌)
		FVector2D OffsetStart = StartPos + FVector2D(
			FMath::FRandRange(-BurstSpreadRadius, BurstSpreadRadius),
			FMath::FRandRange(-BurstSpreadRadius, BurstSpreadRadius));

		// 시차: 앞쪽 구슬부터 순서대로
		float Delay = i * BurstDelayPerOrb;

		SpawnSingleOrb(OffsetStart, TargetPos, StepNumber, bIsCritical, Delay, /*bNotifyArrival*/ i == 0);
	}
}

void UScoreOrbContainerWidget::SpawnEventOrbBurst(FVector2D ScreenCenter, FVector2D TargetPos,
	int32 StepNumber, int32 OrbCount)
{
	if (!OrbCanvas || OrbCount <= 0) return;

	int32 MatIndex = FMath::Clamp(StepNumber - 1, 0, 2);
	UMaterialInterface* OrbMat = CachedOrbMaterials[MatIndex];

	for (int32 i = 0; i < OrbCount; ++i)
	{
		// Phase 1 종료 위치: 화면 중앙에서 EventPopOutRadius 반경의 랜덤 방향
		float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		float Radius = EventPopOutRadius * FMath::FRandRange(0.5f, 1.0f);
		FVector2D PopOutEnd = ScreenCenter + FVector2D(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);

		// === 글로우 후광 ===
		int32 Id = OrbIdCounter++;
		UImage* GlowImg = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("EventOrbGlow_%d"), Id));
		if (GlowImg)
		{
			if (OrbMat) GlowImg->SetBrushFromMaterial(OrbMat);
			float GlowSize = OrbSize * EventOrbScale * GlowSizeMultiplier;
			GlowImg->SetDesiredSizeOverride(FVector2D(GlowSize, GlowSize));
			GlowImg->SetRenderScale(FVector2D(0.0f, 0.0f));
			GlowImg->SetRenderOpacity(0.0f);

			UCanvasPanelSlot* GlowSlot = OrbCanvas->AddChildToCanvas(GlowImg);
			if (GlowSlot)
			{
				GlowSlot->SetAutoSize(true);
				GlowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				GlowSlot->SetPosition(ScreenCenter);
			}
		}

		// === 메인 구슬 ===
		UImage* OrbImg = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("EventOrb_%d"), Id));
		if (!OrbImg) continue;

		if (OrbMat)
		{
			OrbImg->SetBrushFromMaterial(OrbMat);
		}
		else
		{
			FSlateBrush FallbackBrush;
			FallbackBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			FallbackBrush.TintColor = FSlateColor(UGlobalUtilFunctions::GetStepColor(StepNumber));
			FallbackBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
			OrbImg->SetBrush(FallbackBrush);
		}

		float ActualSize = OrbSize * EventOrbScale;
		OrbImg->SetDesiredSizeOverride(FVector2D(ActualSize, ActualSize));
		OrbImg->SetRenderScale(FVector2D(0.0f, 0.0f));
		OrbImg->SetRenderOpacity(0.0f);

		UCanvasPanelSlot* CanvasSlot = OrbCanvas->AddChildToCanvas(OrbImg);
		if (CanvasSlot)
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetPosition(ScreenCenter);
		}

		FOrbAnimData AnimData;
		AnimData.OrbImage = OrbImg;
		AnimData.GlowImage = GlowImg;
		AnimData.OrbSlot = CanvasSlot;
		AnimData.GlowSlot = GlowImg ? Cast<UCanvasPanelSlot>(GlowImg->Slot) : nullptr;
		AnimData.StartPos = ScreenCenter;
		AnimData.TargetPos = TargetPos;
		AnimData.PopOutTargetPos = PopOutEnd;
		AnimData.Delay = i * EventStaggerDelay;
		AnimData.PopOutDuration = EventPopOutDuration;
		AnimData.Duration = EventFlightDuration + FMath::FRandRange(-0.1f, 0.1f);
		AnimData.ArcHeight = EventArcHeight + FMath::FRandRange(-15.f, 15.f);
		AnimData.Scale = EventOrbScale;
		AnimData.WobbleOffset = WobbleBase * 0.6f + FMath::FRandRange(-5.f, 5.f);
		AnimData.bEventStyle = true;
		AnimData.bPopOutDone = false;
		AnimData.StepNumber = StepNumber;
		AnimData.bNotifyArrival = (i == 0);   // 이벤트 orb 착지도 동일 정산 경로
		ActiveOrbs.Add(AnimData);
	}
}

void UScoreOrbContainerWidget::SpawnSingleOrb(FVector2D StartPos, FVector2D TargetPos,
	int32 StepNumber, bool bIsCritical, float Delay, bool bNotifyArrival)
{
	if (!OrbCanvas) return;

	int32 MatIndex = FMath::Clamp(StepNumber - 1, 0, 2);
	UMaterialInterface* OrbMat = CachedOrbMaterials[MatIndex];
	int32 Id = OrbIdCounter++;

	// === 글로우 후광 (먼저 추가 = 뒤에 그려짐, 같은 머티리얼 사용) ===
	UImage* GlowImg = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		*FString::Printf(TEXT("OrbGlow_%d"), Id));

	if (GlowImg)
	{
		if (OrbMat)
		{
			GlowImg->SetBrushFromMaterial(OrbMat);
		}

		float GlowSize = OrbSize * GlowSizeMultiplier;
		GlowImg->SetDesiredSizeOverride(FVector2D(GlowSize, GlowSize));
		GlowImg->SetRenderScale(FVector2D(0.0f, 0.0f));
		GlowImg->SetRenderOpacity(0.0f);

		UCanvasPanelSlot* GlowSlot = OrbCanvas->AddChildToCanvas(GlowImg);
		if (GlowSlot)
		{
			GlowSlot->SetAutoSize(true);
			GlowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			GlowSlot->SetPosition(StartPos);
		}
	}

	// === 메인 구슬 ===
	UImage* OrbImg = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		*FString::Printf(TEXT("Orb_%d"), Id));
	if (!OrbImg) return;

	if (OrbMat)
	{
		OrbImg->SetBrushFromMaterial(OrbMat);
	}
	else
	{
		// 머티리얼 미로드 시 단색 원형 폴백
		FSlateBrush FallbackBrush;
		FallbackBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		FallbackBrush.TintColor = FSlateColor(UGlobalUtilFunctions::GetStepColor(StepNumber));
		FallbackBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		OrbImg->SetBrush(FallbackBrush);
	}

	float ActualSize = bIsCritical ? OrbSize * CriticalScaleMultiplier : OrbSize;
	OrbImg->SetDesiredSizeOverride(FVector2D(ActualSize, ActualSize));
	OrbImg->SetRenderScale(FVector2D(0.0f, 0.0f));
	OrbImg->SetRenderOpacity(0.0f);

	UCanvasPanelSlot* CanvasSlot = OrbCanvas->AddChildToCanvas(OrbImg);
	if (CanvasSlot)
	{
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetPosition(StartPos);
	}

	FOrbAnimData AnimData;
	AnimData.OrbImage = OrbImg;
	AnimData.GlowImage = GlowImg;
	AnimData.OrbSlot = CanvasSlot;
	AnimData.GlowSlot = GlowImg ? Cast<UCanvasPanelSlot>(GlowImg->Slot) : nullptr;
	AnimData.StartPos = StartPos;
	AnimData.TargetPos = TargetPos;
	AnimData.Delay = Delay;
	AnimData.Duration = FlightDuration + FMath::FRandRange(-0.08f, 0.08f);
	AnimData.ArcHeight = BaseArcHeight + FMath::FRandRange(-20.f, 20.f);
	AnimData.Scale = bIsCritical ? CriticalScaleMultiplier : 1.0f;
	AnimData.WobbleOffset = WobbleBase + FMath::FRandRange(-8.f, 8.f);
	AnimData.StepNumber = StepNumber;
	AnimData.bIsCritical = bIsCritical;
	AnimData.bNotifyArrival = bNotifyArrival;

	ActiveOrbs.Add(AnimData);
}

void UScoreOrbContainerWidget::SpawnScoreNumber(FVector2D StartPos, int64 Amount, int32 StepNumber, bool bIsCritical)
{
	if (!OrbCanvas || Amount <= 0) return;

	// 전역 상한 — 오래된 것부터 제거
	while (ActiveNumbers.Num() >= MaxScoreNumbers)
	{
		if (ActiveNumbers[0].Text)
		{
			ActiveNumbers[0].Text->RemoveFromParent();
		}
		ActiveNumbers.RemoveAt(0);
	}

	UTextBlock* Num = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), *FString::Printf(TEXT("ScoreNum_%d"), OrbIdCounter++));
	if (!Num) return;

	// Western 축약(K/M/B) — Bungee 는 Latin 전용이라 한글 단위(만/억) 미지원
	Num->SetText(FText::FromString(FString::Printf(TEXT("+%s"),
		*UGlobalUtilFunctions::AbbreviateNumber(Amount, ENumberAbbrevStyle::WesternSuffix, ENumberRoundMode::Floor).ToString())));
	Num->SetJustification(ETextJustify::Center);

	if (NumberFont)
	{
		FSlateFontInfo Font(NumberFont, bIsCritical ? CritNumberFontSize : NumberFontSize);
		// 두꺼운 검은 외곽선 = 메이플 데미지 팝 (배경 위 가독성 + 임팩트)
		Font.OutlineSettings.OutlineSize = bIsCritical ? CritNumberOutlineSize : NumberOutlineSize;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
		Num->SetFont(Font);
	}
	const FLinearColor NumColor = bIsCritical ? FLinearColor(1.0f, 0.84f, 0.2f, 1.0f)
	                                          : UGlobalUtilFunctions::GetStepColor(StepNumber);
	Num->SetColorAndOpacity(FSlateColor(NumColor));
	Num->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	// 드롭 섀도우로 깊이감
	Num->SetShadowOffset(FVector2D(1.5f, 2.5f));
	Num->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));

	UCanvasPanelSlot* NumSlot = OrbCanvas->AddChildToCanvas(Num);
	if (!NumSlot)
	{
		Num->RemoveFromParent();
		return;
	}
	// 매 틱 같은 자리 스트로브 방지 — 시작점 소량 지터
	const FVector2D JitteredStart = StartPos + FVector2D(
		FMath::FRandRange(-22.0f, 22.0f), FMath::FRandRange(-10.0f, 6.0f));

	NumSlot->SetAutoSize(true);
	NumSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	NumSlot->SetPosition(JitteredStart);

	FScoreNumberAnimData Data;
	Data.Text = Num;
	Data.Slot = NumSlot;
	Data.StartPos = JitteredStart;
	Data.Lifetime = bIsCritical ? CritNumberLifetime : NumberLifetime;
	Data.RiseDistance = bIsCritical ? CritNumberRise : NumberRise;
	ActiveNumbers.Add(Data);
}

void UScoreOrbContainerWidget::PlayConfettiBurst(int32 PieceCount, float GoldRatio,
	EConfettiStyle Style, FVector2D CenterNorm)
{
	if (!OrbCanvas || PieceCount <= 0) return;

	// 대포(착지 정산) 팔레트 — 액센트 블루/스텝 그린/골드/크림 (스타일 카탈로그 톤)
	static const FLinearColor CannonPalette[] = {
		FLinearColor(0.046665f, 0.327778f, 0.745404f, 1.0f),
		FLinearColor(0.104616f, 0.630757f, 0.254152f, 1.0f),
		FLinearColor(0.791298f, 0.421407f, 0.086500f, 1.0f),
		FLinearColor(0.904661f, 0.838799f, 0.693872f, 1.0f),
	};
	// 메달 버스트(레벨업) 팔레트 — 골드/샴페인/앰버/크림 (다크+골드 장면 전용 웜톤)
	static const FLinearColor WarmPalette[] = {
		FLinearColor(0.791298f, 0.421407f, 0.086500f, 1.0f),
		FLinearColor(0.937000f, 0.610000f, 0.212000f, 1.0f),
		FLinearColor(0.745000f, 0.254000f, 0.027000f, 1.0f),
		FLinearColor(0.904661f, 0.838799f, 0.693872f, 1.0f),
	};
	static const FLinearColor Gold(0.791298f, 0.421407f, 0.086500f, 1.0f);

	const bool bMedal = (Style == EConfettiStyle::MedalBurst);

	// 스타 조각 텍스처 lazy load — 흰색 온 투명이라 틴트 안전 (2026-07-20 실측)
	if (bMedal && CachedConfettiStarTextures.Num() == 0)
	{
		for (int32 i = 1; i <= 6; ++i)
		{
			const FString Path = FString::Printf(
				TEXT("/Game/CompanyGrowth/UI/CasualPack/VFX/Polygon_%02d.Polygon_%02d"), i, i);
			if (UTexture2D* Tex = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path)).LoadSynchronous())
			{
				CachedConfettiStarTextures.Add(Tex);
			}
		}
	}

	const FVector2D CanvasSize = OrbCanvas->GetCachedGeometry().GetLocalSize();
	if (CanvasSize.X < 100.0f || CanvasSize.Y < 100.0f) return;   // 지오메트리 미확정 프레임 가드

	const FVector2D Center(CanvasSize.X * CenterNorm.X, CanvasSize.Y * CenterNorm.Y);
	const int32 Count = FMath::Min(PieceCount, MaxConfettiPieces - ActiveConfetti.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		FVector2D Origin;
		FVector2D Vel;
		if (bMedal)
		{
			// 메달 뒤 원환에서 바깥+위 바이어스로 방사 분출
			const float SpawnAng = FMath::FRandRange(0.0f, 2.0f * PI);
			const float SpawnR = FMath::FRandRange(MedalBurstSpawnRadiusMin, MedalBurstSpawnRadiusMax);
			Origin = Center + FVector2D(FMath::Cos(SpawnAng), FMath::Sin(SpawnAng)) * SpawnR;
			FVector2D Dir = (Origin - Center).GetSafeNormal();
			Dir.Y -= FMath::FRandRange(0.35f, 0.95f);
			Dir.Normalize();
			Vel = Dir * FMath::FRandRange(MedalBurstSpeedMin, MedalBurstSpeedMax);
		}
		else
		{
			const bool bLeft = (i % 2 == 0);
			Origin = FVector2D(CanvasSize.X * (bLeft ? 0.08f : 0.92f), CanvasSize.Y * 1.05f);

			// 수평 기준 60~85° 위-안쪽 분출
			const float AngleDeg = FMath::FRandRange(60.0f, 85.0f);
			const float Speed = FMath::FRandRange(1100.0f, 1600.0f);
			const float Rad = FMath::DegreesToRadians(AngleDeg);
			Vel = FVector2D(FMath::Cos(Rad) * Speed * (bLeft ? 1.0f : -1.0f), -FMath::Sin(Rad) * Speed);
		}

		UImage* Piece = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), *FString::Printf(TEXT("Confetti_%d"), OrbIdCounter++));
		if (!Piece) continue;

		const bool bStar = bMedal && CachedConfettiStarTextures.Num() > 0
			&& FMath::FRand() < MedalBurstStarRatio;
		const FLinearColor Color = (FMath::FRand() < GoldRatio) ? Gold
			: (bMedal ? WarmPalette[FMath::RandRange(0, UE_ARRAY_COUNT(WarmPalette) - 1)]
			          : CannonPalette[FMath::RandRange(0, UE_ARRAY_COUNT(CannonPalette) - 1)]);

		FSlateBrush Brush;
		Brush.TintColor = FSlateColor(Color);
		if (bStar)
		{
			Brush.SetResourceObject(CachedConfettiStarTextures[
				FMath::RandRange(0, CachedConfettiStarTextures.Num() - 1)]);
			const float StarSize = FMath::FRandRange(20.0f, 34.0f);
			Piece->SetDesiredSizeOverride(FVector2D(StarSize, StarSize));
		}
		else
		{
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			Brush.OutlineSettings.CornerRadii = FVector4(3.0, 3.0, 3.0, 3.0);
			Piece->SetDesiredSizeOverride(FVector2D(
				FMath::FRandRange(11.0f, 17.0f), FMath::FRandRange(17.0f, 26.0f)));
		}
		Piece->SetBrush(Brush);
		Piece->SetVisibility(ESlateVisibility::HitTestInvisible);

		UCanvasPanelSlot* PieceSlot = OrbCanvas->AddChildToCanvas(Piece);
		if (!PieceSlot) { Piece->RemoveFromParent(); continue; }
		PieceSlot->SetAutoSize(true);
		PieceSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PieceSlot->SetPosition(Origin);

		FConfettiAnimData Data;
		Data.Image = Piece;
		Data.Slot = PieceSlot;
		Data.Pos = Origin;
		Data.Velocity = Vel;
		Data.SpinRate = FMath::FRandRange(180.0f, 540.0f)
			* (FMath::RandBool() ? 1.0f : -1.0f) * (bStar ? 0.4f : 1.0f);
		Data.Angle = FMath::FRandRange(0.0f, 360.0f);
		Data.Lifetime = bMedal ? FMath::FRandRange(1.9f, 2.6f) : FMath::FRandRange(1.6f, 2.2f);
		Data.bStar = bStar;
		Data.FlutterRate = FMath::FRandRange(bStar ? 6.0f : 7.0f, bStar ? 10.0f : 12.0f);
		Data.FlutterPhase = FMath::FRandRange(0.0f, 2.0f * PI);
		if (bMedal)
		{
			// 종이 공기저항 — 초반 분출 후 팔랑이며 천천히 낙하
			Data.TerminalFall = FMath::FRandRange(300.0f, 520.0f);
			Data.HorizontalDrag = FMath::FRandRange(0.6f, 1.2f);
		}
		ActiveConfetti.Add(Data);
	}
}

void UScoreOrbContainerWidget::SpawnStampText(FVector2D CenterPos, const FText& Text,
	const FLinearColor& Color, const FSlateFontInfo& Font)
{
	if (!OrbCanvas) return;

	UTextBlock* Stamp = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), *FString::Printf(TEXT("Stamp_%d"), OrbIdCounter++));
	if (!Stamp) return;

	Stamp->SetText(Text);
	Stamp->SetJustification(ETextJustify::Center);
	FSlateFontInfo StampFont = Font;
	StampFont.OutlineSettings.OutlineSize = 2;
	StampFont.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	Stamp->SetFont(StampFont);
	Stamp->SetColorAndOpacity(FSlateColor(Color));
	Stamp->SetVisibility(ESlateVisibility::HitTestInvisible);
	Stamp->SetShadowOffset(FVector2D(1.5f, 2.5f));
	Stamp->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	Stamp->SetRenderTransformAngle(-8.0f);
	Stamp->SetRenderScale(FVector2D(1.6f, 1.6f));
	Stamp->SetRenderOpacity(0.0f);

	UCanvasPanelSlot* StampSlot = OrbCanvas->AddChildToCanvas(Stamp);
	if (!StampSlot) { Stamp->RemoveFromParent(); return; }
	StampSlot->SetAutoSize(true);
	StampSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	StampSlot->SetPosition(CenterPos);

	FStampAnimData Data;
	Data.Text = Stamp;
	ActiveStamps.Add(Data);
}

void UScoreOrbContainerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Slate 가 넘기는 InDeltaTime 은 실시간이라 배속(TimeDilation)을 안 탄다 — 연출을 개발 배속에 묶으려면 월드 델타.
	const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : InDeltaTime;

	// === 점수 숫자 애니메이션 (상승 + 팝인 + 페이드) ===
	for (int32 i = ActiveNumbers.Num() - 1; i >= 0; --i)
	{
		FScoreNumberAnimData& N = ActiveNumbers[i];
		N.Elapsed += DeltaTime;
		const float T = FMath::Clamp(N.Elapsed / N.Lifetime, 0.0f, 1.0f);

		const float RiseT = 1.0f - FMath::Square(1.0f - T);
		if (N.Slot)
		{
			N.Slot->SetPosition(N.StartPos - FVector2D(0.0f, N.RiseDistance * RiseT));
		}

		if (N.Text)
		{
			// 팝인: EaseOutBack 오버슈트로 "팡" 튀는 바운스 (메이플 데미지 느낌)
			float Scale = 1.0f;
			if (N.Elapsed < NumberPopInTime)
			{
				const float a = N.Elapsed / NumberPopInTime;
				const float c1 = 1.70158f;
				const float c3 = c1 + 1.0f;
				const float tb = a - 1.0f;
				const float EaseOutBack = 1.0f + c3 * tb * tb * tb + c1 * tb * tb;   // 0 → ~1.1 → 1
				Scale = FMath::Lerp(0.5f, 1.0f, EaseOutBack);   // 0.5 → 오버슈트(>1) → 1.0
			}
			N.Text->SetRenderScale(FVector2D(Scale, Scale));

			const float Opacity = (T < NumberFadeStart)
				? 1.0f
				: 1.0f - (T - NumberFadeStart) / (1.0f - NumberFadeStart);
			N.Text->SetRenderOpacity(Opacity);
		}

		if (T >= 1.0f)
		{
			if (N.Text)
			{
				N.Text->RemoveFromParent();
			}
			ActiveNumbers.RemoveAt(i);
		}
	}

	// === 컨페티 (분출→중력 낙하→팔랑임/트윙클→페이드) ===
	for (int32 i = ActiveConfetti.Num() - 1; i >= 0; --i)
	{
		FConfettiAnimData& C = ActiveConfetti[i];
		C.Elapsed += DeltaTime;
		C.Velocity.Y = FMath::Min(C.Velocity.Y + ConfettiGravity * DeltaTime, C.TerminalFall);
		C.Velocity.X *= FMath::Max(0.0f, 1.0f - C.HorizontalDrag * DeltaTime);
		C.Pos += C.Velocity * DeltaTime;
		C.Angle += C.SpinRate * DeltaTime;

		if (C.Slot) C.Slot->SetPosition(C.Pos);
		if (C.Image)
		{
			C.Image->SetRenderTransformAngle(C.Angle);
			if (C.bStar)
			{
				// 스타 트윙클 — 스케일 펄스
				const float Pulse = 0.9f + 0.25f * FMath::Sin(C.Elapsed * C.FlutterRate + C.FlutterPhase);
				C.Image->SetRenderScale(FVector2D(Pulse, Pulse));
			}
			else if (C.FlutterRate > 0.0f)
			{
				// 종이 뒤집힘 — X폭이 |cos|로 얇아졌다 넓어지는 가짜 3D 팔랑임
				const float Flip = FMath::Abs(FMath::Cos(C.Elapsed * C.FlutterRate + C.FlutterPhase));
				C.Image->SetRenderScale(FVector2D(FMath::Max(0.12f, Flip), 1.0f));
			}
			const float Remain = C.Lifetime - C.Elapsed;
			C.Image->SetRenderOpacity(Remain < ConfettiFadeTail ? FMath::Max(0.0f, Remain / ConfettiFadeTail) : 1.0f);
		}

		if (C.Elapsed >= C.Lifetime)
		{
			if (C.Image) C.Image->RemoveFromParent();
			ActiveConfetti.RemoveAt(i);
		}
	}

	// === 도장 텍스트 (쾅 스케일인→유지→페이드) ===
	for (int32 i = ActiveStamps.Num() - 1; i >= 0; --i)
	{
		FStampAnimData& S = ActiveStamps[i];
		S.Elapsed += DeltaTime;
		if (!S.Text) { ActiveStamps.RemoveAt(i); continue; }

		if (S.Elapsed < StampPopInTime)
		{
			// EaseOutBack — 1.6→1.0 으로 눌러 찍히는 도장
			const float a = S.Elapsed / StampPopInTime;
			const float c1 = 1.70158f;
			const float c3 = c1 + 1.0f;
			const float tb = a - 1.0f;
			const float Eased = 1.0f + c3 * tb * tb * tb + c1 * tb * tb;
			const float Sc = FMath::Lerp(1.6f, 1.0f, Eased);
			S.Text->SetRenderScale(FVector2D(Sc, Sc));
			S.Text->SetRenderOpacity(FMath::Min(1.0f, a * 3.0f));
		}
		else
		{
			S.Text->SetRenderScale(FVector2D(1.0f, 1.0f));
			const float AfterHold = S.Elapsed - StampPopInTime - StampHoldTime;
			S.Text->SetRenderOpacity(AfterHold <= 0.0f ? 1.0f : FMath::Max(0.0f, 1.0f - AfterHold / StampFadeTime));
		}

		if (S.Elapsed >= StampPopInTime + StampHoldTime + StampFadeTime)
		{
			S.Text->RemoveFromParent();
			ActiveStamps.RemoveAt(i);
		}
	}

	for (int32 i = ActiveOrbs.Num() - 1; i >= 0; --i)
	{
		FOrbAnimData& Orb = ActiveOrbs[i];
		if (Orb.bCompleted) continue;

		// 시차 대기 처리
		if (Orb.Delay > 0.0f)
		{
			Orb.Delay -= DeltaTime;
			continue;
		}

		Orb.Elapsed += DeltaTime;

		// === 이벤트 스타일: Phase 1 (팝아웃) 처리 ===
		if (Orb.bEventStyle && !Orb.bPopOutDone)
		{
			float PopAlpha = FMath::Clamp(Orb.Elapsed / Orb.PopOutDuration, 0.0f, 1.0f);

			// EaseOutBack: 살짝 오버슈트하며 튀어나감
			float EasedT;
			{
				const float c1 = 1.70158f;
				const float c3 = c1 + 1.0f;
				float t = PopAlpha - 1.0f;
				EasedT = 1.0f + c3 * t * t * t + c1 * t * t;
			}

			FVector2D PopPos = FMath::Lerp(Orb.StartPos, Orb.PopOutTargetPos, EasedT);
			float PopScale = FMath::Lerp(0.5f * Orb.Scale, 1.4f * Orb.Scale, PopAlpha);
			float PopOpacity = FMath::Min(1.0f, PopAlpha * 2.0f);

			if (Orb.OrbImage)
			{
				if (UCanvasPanelSlot* OrbSlot = Orb.OrbSlot)
				{
					OrbSlot->SetPosition(PopPos);
				}
				Orb.OrbImage->SetRenderScale(FVector2D(PopScale, PopScale));
				Orb.OrbImage->SetRenderOpacity(PopOpacity);
			}
			if (Orb.GlowImage)
			{
				if (UCanvasPanelSlot* GlowSlot = Orb.GlowSlot)
				{
					GlowSlot->SetPosition(PopPos);
				}
				float GlowScale = PopScale * GlowSizeMultiplier * 0.5f;
				Orb.GlowImage->SetRenderScale(FVector2D(GlowScale, GlowScale));
				Orb.GlowImage->SetRenderOpacity(PopOpacity * 0.6f);
			}

			if (PopAlpha >= 1.0f)
			{
				// Phase 1 종료 → Phase 2 비행 시작점으로 StartPos 갱신
				Orb.StartPos = Orb.PopOutTargetPos;
				Orb.Elapsed = 0.0f;
				Orb.bPopOutDone = true;
			}
			continue;
		}

		float Alpha = FMath::Clamp(Orb.Elapsed / Orb.Duration, 0.0f, 1.0f);

		// === 이동: 이차 베지어 + EaseOut ===
		float MoveAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.5f);

		FVector2D MidPoint = (Orb.StartPos + Orb.TargetPos) * 0.5f;
		FVector2D ControlPoint = MidPoint + FVector2D(0.f, -Orb.ArcHeight);

		float t = MoveAlpha;
		float OneMinusT = 1.0f - t;
		FVector2D CurrentPos =
			OneMinusT * OneMinusT * Orb.StartPos +
			2.0f * OneMinusT * t * ControlPoint +
			t * t * Orb.TargetPos;

		// === 좌우 흔들림 (감쇄 사인파, 비행 경로에 수직) ===
		FVector2D FlightDir = Orb.TargetPos - Orb.StartPos;
		float FlightLen = FlightDir.Size();
		if (FlightLen > 1.0f)
		{
			FVector2D Perpendicular(-FlightDir.Y / FlightLen, FlightDir.X / FlightLen);
			float Damping = 1.0f - Alpha;
			float Wobble = FMath::Sin(Alpha * PI * 3.0f) * Damping * Orb.WobbleOffset;
			CurrentPos += Perpendicular * Wobble;
		}

		// === 스케일: 팝 인 → 유지 → 축소 ===
		float ScaleFactor;
		if (Alpha < 0.10f)
		{
			// 팝 인: 0 → 1.4 (탄성 느낌)
			float PopAlpha = Alpha / 0.10f;
			ScaleFactor = FMath::Lerp(0.0f, 1.4f * Orb.Scale, PopAlpha);
		}
		else if (Alpha < 0.22f)
		{
			// 오버슈트 복귀: 1.4 → 1.0
			float SettleAlpha = (Alpha - 0.10f) / 0.12f;
			ScaleFactor = FMath::Lerp(1.4f * Orb.Scale, Orb.Scale, SettleAlpha);
		}
		else if (Alpha < 0.88f)
		{
			// 비행 중 숨쉬기 (미세 스케일 진동)
			float Breathe = 1.0f + FMath::Sin(Alpha * PI * 4.0f) * 0.06f;
			ScaleFactor = Orb.Scale * Breathe;
		}
		else
		{
			// 도착 직전: 1.0 → 0.0 (짧고 빠르게 축소)
			float ShrinkAlpha = (Alpha - 0.88f) / 0.12f;
			float EasedShrink = ShrinkAlpha * ShrinkAlpha;
			ScaleFactor = FMath::Lerp(Orb.Scale, 0.0f, EasedShrink);
		}

		// === 오파시티 ===
		float Opacity = 1.0f;
		if (Alpha < 0.08f)
		{
			// 페이드 인
			Opacity = Alpha / 0.08f;
		}
		else if (Alpha > 0.90f)
		{
			// 도착 직전 페이드 아웃
			Opacity = FMath::Lerp(1.0f, 0.0f, (Alpha - 0.90f) / 0.10f);
		}

		// === 글로우 후광: 메인보다 크게 (맥동은 머티리얼 PulseSpeed에서 처리) ===
		float GlowScale = ScaleFactor * GlowSizeMultiplier * 0.5f;
		float GlowOpacity = Opacity * 0.5f;

		// 메인 구슬 업데이트
		if (Orb.OrbImage)
		{
			if (UCanvasPanelSlot* OrbSlot = Orb.OrbSlot)
			{
				OrbSlot->SetPosition(CurrentPos);
			}
			Orb.OrbImage->SetRenderScale(FVector2D(ScaleFactor, ScaleFactor));
			Orb.OrbImage->SetRenderOpacity(Opacity);
		}

		// 글로우 후광 업데이트
		if (Orb.GlowImage)
		{
			if (UCanvasPanelSlot* GlowSlot = Orb.GlowSlot)
			{
				GlowSlot->SetPosition(CurrentPos);
			}
			Orb.GlowImage->SetRenderScale(FVector2D(GlowScale, GlowScale));
			Orb.GlowImage->SetRenderOpacity(GlowOpacity);
		}

		// 도착 완료 → 제거
		if (Alpha >= 1.0f)
		{
			Orb.bCompleted = true;
			if (Orb.bNotifyArrival)
			{
				OnScoreOrbArrived.Broadcast(Orb.StepNumber, Orb.bIsCritical);
			}
			if (Orb.GlowImage)
			{
				OrbCanvas->RemoveChild(Orb.GlowImage);
			}
			if (Orb.OrbImage)
			{
				OrbCanvas->RemoveChild(Orb.OrbImage);
			}
			ActiveOrbs.RemoveAt(i);
		}
	}
}
