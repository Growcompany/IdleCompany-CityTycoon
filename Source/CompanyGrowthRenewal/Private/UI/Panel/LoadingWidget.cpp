// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panel/LoadingWidget.h"

#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "CommonTextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Core/CGGameInstance.h"
#include "Rendering/DrawElements.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	// 진행 밴드별 상태 메시지 (시스템 안내톤 — 사용자 지정). 런타임 시스템 메시지 — DT 비대상.
	struct FLoadingStageMsg
	{
		float UpTo;           // 이 진행률(0~1) 이하 구간에서 표시
		const TCHAR* Msg;
	};
	constexpr FLoadingStageMsg GLoadingStages[] = {
		{ 0.35f, TEXT("데이터 연결중") },
		{ 0.65f, TEXT("테이블 준비중") },
		{ 0.90f, TEXT("리소스 불러오는 중") },
		{ 1.01f, TEXT("마무리 중") },
	};

	const TCHAR* GetLoadingStageMsg(float Percent01)
	{
		for (const FLoadingStageMsg& Stage : GLoadingStages)
		{
			if (Percent01 <= Stage.UpTo)
			{
				return Stage.Msg;
			}
		}
		return GLoadingStages[UE_ARRAY_COUNT(GLoadingStages) - 1].Msg;
	}
}

void ULoadingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// WBP CDO 가 루트를 Collapsed/Hidden 으로 저장하면 NativeTick 이 영원히 안 돌아 0% 에서 멈춘다.
	// 풀스크린 표시 전용(입력 안 받음)이라 HitTestInvisible 로 가시성=틱 보장.
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Mode B(다운로드 UI)는 원격 다운로드가 실제로 붙기 전까지 항상 숨김
	if (DownloadBox)
	{
		DownloadBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	Birds.SetNum(3);
	for (int32 BirdIndex = 0; BirdIndex < Birds.Num(); ++BirdIndex)
	{
		Birds[BirdIndex].bSea = BirdIndex < 2;
		ResetBird(Birds[BirdIndex], /*bInitialScatter=*/true);
	}

	Sailboat = FAmbientBoat{};
	Sailboat.PosX = 0.20f;
	Sailboat.BaseY = 0.845f;
	Sailboat.SpeedX = SailboatSpeedUV;
	Sailboat.WidthUV = FMath::Max(0.001f, SailboatWidthUV);
	// 시작 화면에서도 항로가 읽히도록 최근 3초의 포말만 미리 배치한다.
	for (int32 Index = 12; Index >= 1; --Index)
	{
		const float Age = Index * 0.25f;
		Sailboat.Wake.Add({ FVector2D(Sailboat.PosX - Sailboat.SpeedX * Age - FMath::Sign(Sailboat.SpeedX) * Sailboat.WidthUV * 0.4f, Sailboat.BaseY), Age });
	}
	SmokeTime = 0.f;
	CloudTime = 0.f;

	// 첫 스트릭은 시작부터 화면 중앙에 떠 있게 (진입 대기 없이 즉시 체감)
	Streaks.SetNum(2);
	Streaks[0].bActive = true;
	Streaks[0].HeadX = 1500.f;
	Streaks[0].BaseY = 400.f + WindBandOffsetY;
	Streaks[0].Speed = 470.f;
	Streaks[0].Length = 420.f;
	Streaks[0].WaveAmp = 9.f;
	Streaks[1].Cooldown = 1.2f;

	UpdateProgressUI();

	BeginBootSequence();
}

void ULoadingWidget::ResetBird(FAmbientBird& Bird, bool bInitialScatter)
{
	// 최초엔 화면 안에 흩뿌리고, 이후엔 오른쪽 밖에서 재진입
	Bird.PosX = bInitialScatter ? FMath::FRandRange(Bird.bSea ? 0.15f : 0.48f, Bird.bSea ? 0.65f : 1.00f) : FMath::FRandRange(1.03f, 1.12f);
	Bird.BaseY = Bird.bSea ? FMath::FRandRange(0.72f, 0.80f)
		: static_cast<float>(FMath::FRandRange(SkyBirdBandY.X, SkyBirdBandY.Y));
	Bird.SpeedX = -FMath::FRandRange(0.016f, 0.030f);
	Bird.WingSpan = FMath::FRandRange(6.f, 13.f);
	Bird.FlapPhase = FMath::FRandRange(0.f, 6.28f);
	Bird.FlapSpeed = FMath::FRandRange(5.f, 8.f);
	Bird.DriftPhase = FMath::FRandRange(0.f, 6.28f);
}

void ULoadingWidget::TickAmbientBirds(float InDeltaTime)
{
	for (FAmbientBird& Bird : Birds)
	{
		Bird.PosX += Bird.SpeedX * InDeltaTime;
		Bird.FlapPhase += Bird.FlapSpeed * InDeltaTime;
		Bird.DriftPhase += 0.8f * InDeltaTime;
		if (Bird.PosX < (Bird.bSea ? -0.05f : SkyBirdExitX))
		{
			ResetBird(Bird, false);
		}
	}
}

void ULoadingWidget::TickAmbientBoat(FAmbientBoat& Boat, float InDeltaTime)
{
	const float Delta = FMath::Max(0.f, InDeltaTime);
	for (FWakeMark& Mark : Boat.Wake)
	{
		Mark.Age += Delta;
	}
	Boat.Wake.RemoveAll([](const FWakeMark& Mark) { return Mark.Age >= 5.5f; });

	Boat.PosX += Boat.SpeedX * Delta;
	Boat.BobPhase += Delta * 1.5f;
	if ((Boat.SpeedX > 0.f && Boat.PosX > 1.f + Boat.WidthUV)
		|| (Boat.SpeedX < 0.f && Boat.PosX < -Boat.WidthUV))
	{
		Boat.PosX = Boat.SpeedX > 0.f ? -Boat.WidthUV : 1.f + Boat.WidthUV;
		Boat.Wake.Reset();
		Boat.WakeClock = 0.f;
		return;
	}

	// 긴 프레임에서도 새 포말 수를 제한하고, 남은 시간으로 실제 항로 위 위치를 복원한다.
	Boat.WakeClock += FMath::Min(Delta, 0.75f);
	int32 Emitted = 0;
	while (Boat.WakeClock >= 0.25f && Emitted < 3)
	{
		Boat.WakeClock -= 0.25f;
		const float SternX = Boat.PosX - Boat.SpeedX * Boat.WakeClock - FMath::Sign(Boat.SpeedX) * Boat.WidthUV * 0.4f;
		Boat.Wake.Add({ FVector2D(SternX, Boat.BaseY), Boat.WakeClock });
		++Emitted;
	}
	if (Boat.Wake.Num() > 32)
	{
		Boat.Wake.RemoveAt(0, Boat.Wake.Num() - 32);
	}
}

void ULoadingWidget::TickClouds(float InDeltaTime)
{
	CloudTime += FMath::Max(0.f, InDeltaTime) * FMath::Max(0.f, CloudDriftScale);
	const double ArtWidth = BackgroundImage ? BackgroundImage->GetCachedGeometry().GetLocalSize().X : 0.0;
	if (ArtWidth <= 1.0)
	{
		return;
	}
	struct FCloudDrift { UImage* Img; float SpeedUV; };
	const FCloudDrift Drifts[] = {
		{ CloudA, 0.0018f },
		{ CloudB, 0.0011f },
	};
	for (const FCloudDrift& D : Drifts)
	{
		if (!D.Img)
		{
			continue;
		}
		const UCanvasPanelSlot* CloudSlot = Cast<UCanvasPanelSlot>(D.Img->Slot);
		const double CloudWidth = D.Img->GetCachedGeometry().GetLocalSize().X;
		if (!CloudSlot || CloudWidth <= 1.0)
		{
			continue;
		}
		const double InitialUV = 0.5 + CloudSlot->GetOffsets().Left / ArtWidth;
		const double CloudPadding = CloudWidth * 0.5 / ArtWidth + 0.02;
		const double Span = 1.0 + CloudPadding * 2.0;
		double PositionUV = FMath::Fmod(InitialUV + CloudPadding - CloudTime * D.SpeedUV, Span);
		if (PositionUV < 0.0)
		{
			PositionUV += Span;
		}
		// 구름 전체가 화면 밖으로 빠진 뒤 반대편 바깥에서 재진입한다.
		D.Img->SetRenderTranslation(FVector2D((PositionUV - CloudPadding - InitialUV) * ArtWidth, 0.0));
	}
}

void ULoadingWidget::TickWindStreaks(float InDeltaTime)
{
	for (FWindStreak& Streak : Streaks)
	{
		if (!Streak.bActive)
		{
			Streak.Cooldown -= InDeltaTime;
			if (Streak.Cooldown <= 0.f)
			{
				Streak.bActive = true;
				Streak.HeadX = 2620.f;   // 우측 밖에서 진입
				Streak.BaseY = FMath::FRandRange(340.f, 470.f) + WindBandOffsetY;
				Streak.Speed = FMath::FRandRange(380.f, 560.f);
				Streak.Length = FMath::FRandRange(320.f, 480.f);
				Streak.WavePhase = FMath::FRandRange(0.f, 6.28f);
				Streak.WaveAmp = FMath::FRandRange(6.f, 12.f);
			}
			continue;
		}
		Streak.HeadX -= Streak.Speed * InDeltaTime;
		if (Streak.HeadX + Streak.Length < -80.f)
		{
			Streak.bActive = false;
			Streak.Cooldown = FMath::FRandRange(1.8f, 4.0f);
		}
	}
}

int32 ULoadingWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FLinearColor ParentTint = InWidgetStyle.GetColorAndOpacityTint();
	// Slate가 RenderOpacity를 부모 스타일에 이미 합성해 NativePaint로 전달한다.
	const float AmbientAlpha = ParentTint.A;
	int32 AmbientLayer = MaxLayer;
	FVector2D BirdOrigin = FVector2D::ZeroVector;
	FVector2D BirdExtent = AllottedGeometry.GetLocalSize();

	if (BackgroundImage && AmbientAlpha > KINDA_SMALL_NUMBER)
	{
		const FGeometry& ImageGeometry = BackgroundImage->GetPaintSpaceGeometry();
		const FVector2D ImageSize = ImageGeometry.GetLocalSize();
		if (ImageSize.X > 1.f && ImageSize.Y > 1.f)
		{
			const FVector2D ImageOrigin = AllottedGeometry.AbsoluteToLocal(ImageGeometry.LocalToAbsolute(FVector2D::ZeroVector));
			const FVector2D ImageFar = AllottedGeometry.AbsoluteToLocal(ImageGeometry.LocalToAbsolute(ImageSize));
			const FVector2D DrawSize = ImageFar - ImageOrigin;
			if (DrawSize.X > 1.f && DrawSize.Y > 1.f)
			{
				BirdOrigin = ImageOrigin;
				BirdExtent = DrawSize;
				const auto DrawBrush = [&](const FSlateBrush& Brush, const FVector2D& Center, const FVector2D& Size, const FLinearColor& Tint, int32 DrawLayer)
				{
					if (Brush.GetResourceObject() && Brush.DrawAs != ESlateBrushDrawType::NoDrawType && Size.X > 0.f && Size.Y > 0.f)
					{
						FSlateDrawElement::MakeBox(OutDrawElements, DrawLayer,
							AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Center - Size * 0.5f)),
							&Brush, ESlateDrawEffect::None, Tint);
					}
				};
				const auto DrawBoat = [&](const FAmbientBoat& Boat, const FSlateBrush& Brush)
				{
					const float Width = DrawSize.X * Boat.WidthUV;
					const float Aspect = Brush.ImageSize.X > 0.f ? Brush.ImageSize.Y / Brush.ImageSize.X : 0.f;
					if (Aspect <= 0.f)
					{
						return;
					}
					const FVector2D SpriteSize(Width, Width * Aspect);
					const FVector2D Center(ImageOrigin.X + DrawSize.X * Boat.PosX,
						ImageOrigin.Y + DrawSize.Y * (Boat.BaseY + 0.0015f * FMath::Sin(Boat.BobPhase)) - SpriteSize.Y * 0.5f);
					DrawBrush(Brush, Center, SpriteSize, FLinearColor(ParentTint.R, ParentTint.G, ParentTint.B, AmbientAlpha), MaxLayer + 2);
				};
				if (FactorySmokeBrush.GetResourceObject())
				{
					constexpr float PuffLifetime = 6.0f;
					constexpr int32 PuffCount = 14;
					for (int32 ChimneyIndex = 0; ChimneyIndex < FactoryChimneyUVs.Num(); ++ChimneyIndex)
					{
						const FVector2D MouthUV = FactoryChimneyUVs[ChimneyIndex];
						const FVector2D Mouth = ImageOrigin + MouthUV * DrawSize;
						for (int32 PuffIndex = 0; PuffIndex < PuffCount; ++PuffIndex)
						{
							const float Age = FMath::Fmod(SmokeTime + PuffIndex * PuffLifetime / PuffCount
								+ ChimneyIndex * 0.31f, PuffLifetime);
							const float Progress = Age / PuffLifetime;
							const float Width = DrawSize.X * (0.0048f + 0.023f * Progress);
							const FVector2D PuffSize(Width, Width * 0.86f);
							const float Drift = DrawSize.X * (0.012f * Progress * Progress
								+ 0.0020f * Progress * FMath::Sin(Age * 1.7f + ChimneyIndex));
							const FVector2D Center = Mouth + FVector2D(Drift,
								-DrawSize.Y * 0.115f * Progress - PuffSize.Y * 0.25f);
							const float PuffAlpha = 0.78f * FMath::Sin(Progress * PI) * AmbientAlpha;
							DrawBrush(FactorySmokeBrush, Center, PuffSize,
								FLinearColor(0.90f * ParentTint.R, 0.94f * ParentTint.G, ParentTint.B, PuffAlpha), MaxLayer + 1);
						}
					}
				}
				const auto DrawWake = [&](const FAmbientBoat& Boat)
				{
					for (const FWakeMark& Mark : Boat.Wake)
					{
						if (Mark.UV.X < -0.04f || Mark.UV.X > 1.04f)
						{
							continue;
						}
						const float Life = FMath::Clamp(1.f - Mark.Age / 5.5f, 0.f, 1.f);
						const FVector2D WakeSize(DrawSize.X * (0.017f + 0.010f * (1.f - Life)),
							DrawSize.Y * (0.006f + 0.003f * (1.f - Life)));
						const FVector2D Center = ImageOrigin + FVector2D(Mark.UV.X * DrawSize.X, (Mark.UV.Y + 0.003f) * DrawSize.Y);
						DrawBrush(WakeBrush, Center, WakeSize,
							FLinearColor(0.90f * ParentTint.R, 0.97f * ParentTint.G, ParentTint.B, WakeOpacity * Life * AmbientAlpha), MaxLayer + 1);
					}
				};
				DrawWake(Sailboat);
				DrawBoat(Sailboat, SailboatBrush);
				AmbientLayer = MaxLayer + 2;
			}
		}
	}

	// 바람 스트릭 — 사인 궤적을 65px 간격으로 샘플한 "약간 각진" 흰 획, 꼬리로 갈수록 옅게
	for (const FWindStreak& Streak : Streaks)
	{
		if (!Streak.bActive)
		{
			continue;
		}
		const float Travel = 2620.f + 80.f + Streak.Length;
		const float Progress = FMath::Clamp((2620.f - Streak.HeadX) / Travel, 0.f, 1.f);
		const float LifeAlpha = FMath::Sin(Progress * PI);   // 스윕 중간에 가장 진함
		TArray<FVector2D> Pts;
		const int32 NumPts = FMath::Max(4, FMath::CeilToInt(Streak.Length / 65.f) + 1);
		for (int32 i = 0; i < NumPts; ++i)
		{
			const float X = Streak.HeadX + Streak.Length * i / (NumPts - 1);
			Pts.Add(FVector2D(X, Streak.BaseY + Streak.WaveAmp * FMath::Sin(X * 0.012f + Streak.WavePhase)));
		}
		// 선두→꼬리 3구간 알파 감쇠 (MakeLines 는 단색이라 구간 분할로 페이드)
		const float SegAlpha[3] = { 0.50f, 0.32f, 0.16f };
		const int32 PerSeg = (NumPts - 1) / 3;
		for (int32 SegIdx = 0; SegIdx < 3; ++SegIdx)
		{
			const int32 Begin = SegIdx * PerSeg;
			const int32 End = (SegIdx == 2) ? NumPts - 1 : (SegIdx + 1) * PerSeg;
			if (End <= Begin)
			{
				continue;
			}
			TArray<FVector2D> SegPts(Pts.GetData() + Begin, End - Begin + 1);
			FSlateDrawElement::MakeLines(OutDrawElements, MaxLayer + 1,
				AllottedGeometry.ToPaintGeometry(), SegPts, ESlateDrawEffect::None,
				FLinearColor(ParentTint.R, ParentTint.G, ParentTint.B, SegAlpha[SegIdx] * LifeAlpha * AmbientAlpha), true, 4.0f);
		}
	}

	// 앰비언트 새 — 갈매기 실루엣을 날개 2획으로 (획 분리 = MakeLines 미터 한계 회피)
	for (const FAmbientBird& Bird : Birds)
	{
		if (Bird.PosX > 1.0f)
		{
			continue;   // 아직 화면 밖(재진입 대기)
		}
		const FVector2D Center = BirdOrigin + FVector2D(Bird.PosX * BirdExtent.X,
			(Bird.BaseY + 0.006f * FMath::Sin(Bird.DriftPhase)) * BirdExtent.Y);
		const float TipY = FMath::Sin(Bird.FlapPhase) * Bird.WingSpan * 0.5f;
		const float Thickness = FMath::Max(1.5f, Bird.WingSpan * 0.18f);
		const float EntryAlpha = FMath::Clamp((1.05f - Bird.PosX) / 0.08f, 0.f, 1.f);
		const float ExitAlpha = FMath::Clamp((Bird.PosX - (Bird.bSea ? -0.05f : SkyBirdExitX)) / 0.08f, 0.f, 1.f);
		const float BirdAlpha = AmbientAlpha * EntryAlpha * ExitAlpha;
		const FLinearColor BirdColor(Bird.bSea ? 0.94f * ParentTint.R : 0.88f * ParentTint.R,
			Bird.bSea ? 0.96f * ParentTint.G : 0.91f * ParentTint.G, ParentTint.B, 0.82f * BirdAlpha);
		const TArray<FVector2D> LeftWing{ Center, Center + FVector2D(-Bird.WingSpan * 0.50f, -TipY * 0.78f), Center + FVector2D(-Bird.WingSpan, -TipY) };
		const TArray<FVector2D> RightWing{ Center, Center + FVector2D(Bird.WingSpan * 0.50f, -TipY * 0.78f), Center + FVector2D(Bird.WingSpan, -TipY) };
		FSlateDrawElement::MakeLines(OutDrawElements, MaxLayer + 1,
			AllottedGeometry.ToPaintGeometry(), LeftWing, ESlateDrawEffect::None, BirdColor, true, Thickness);
		FSlateDrawElement::MakeLines(OutDrawElements, MaxLayer + 1,
			AllottedGeometry.ToPaintGeometry(), RightWing, ESlateDrawEffect::None, BirdColor, true, Thickness);
		const FLinearColor Underside(0.30f * ParentTint.R, 0.42f * ParentTint.G, 0.55f * ParentTint.B, 0.30f * BirdAlpha);
		FSlateDrawElement::MakeLines(OutDrawElements, MaxLayer + 2,
			AllottedGeometry.ToPaintGeometry(), LeftWing, ESlateDrawEffect::None, Underside, true, 0.8f);
		FSlateDrawElement::MakeLines(OutDrawElements, MaxLayer + 2,
			AllottedGeometry.ToPaintGeometry(), RightWing, ESlateDrawEffect::None, Underside, true, 0.8f);
	}
	return FMath::Max(AmbientLayer, MaxLayer + 2);
}

void ULoadingWidget::BeginBootSequence()
{
	if (bBootStarted)
	{
		return;
	}
	bBootStarted = true;

	ElapsedTime = 0.f;
	DisplayedPercent = 0.f;
	FadeAlpha = 1.f;
	SetRenderOpacity(1.f);

	TargetPackageFName = FName(*TargetLevelPackagePath);

	StartTargetLevelLoad();
}

void ULoadingWidget::StartTargetLevelLoad()
{
	// MainMap 패키지를 비동기 프리로드 → 이후 OpenLevel 이 빠르게 전환.
	// WeakLambda: 위젯이 파괴되면 콜백 무시 (안전).
	LoadPackageAsync(TargetLevelPackagePath,
		FLoadPackageAsyncDelegate::CreateWeakLambda(this,
			[this](const FName& /*PackageName*/, UPackage* /*LoadedPackage*/, EAsyncLoadingResult::Type Result)
			{
				if (Result != EAsyncLoadingResult::Succeeded)
				{
					UE_LOG(LogTemp, Error, TEXT("[LoadingWidget] MainMap 비동기 로드 실패(Result=%d) — 전환 강행"), (int32)Result);
				}
				bTargetLoaded = true;
			}));
}

void ULoadingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bBootStarted)
	{
		return;
	}

	TickAmbientBirds(InDeltaTime);   // 페이드 중에도 계속 (배경 생동감)
	TickAmbientBoat(Sailboat, InDeltaTime);
	SmokeTime = FMath::Fmod(SmokeTime + FMath::Max(0.f, InDeltaTime), 120.f);
	TickClouds(InDeltaTime);
	TickWindStreaks(InDeltaTime);

	// 페이드 진행 중이면 페이드만 처리
	if (bFading)
	{
		FadeAlpha = FMath::Max(0.f, FadeAlpha - InDeltaTime / FMath::Max(FadeOutDuration, 0.01f));
		SetRenderOpacity(FadeAlpha);
		if (FadeAlpha <= 0.f)
		{
			BeginTransition();
		}
		return;
	}

	ElapsedTime += InDeltaTime;

	// 콜백 미발화/지연 대비 강제 전환 안전망 (영구 hang 방지)
	if (!bTargetLoaded && ElapsedTime > LoadTimeout)
	{
		UE_LOG(LogTemp, Error, TEXT("[LoadingWidget] 로드 타임아웃(%.0fs) — 전환 강행"), LoadTimeout);
		bTargetLoaded = true;
	}

	// 시간 페이스: MinDisplayTime 에 걸쳐 0→1 (바가 항상 부드럽게 기어감)
	const float Pace = FMath::Clamp(ElapsedTime / FMath::Max(MinDisplayTime, 0.01f), 0.f, 1.f);

	// 실제 로드 상한: 추적 안 되면 0.95 로 캡 (로드 완료 전 페이스가 100% 찍지 않게)
	const float RealPct = GetAsyncLoadPercentage(TargetPackageFName); // 0..100, 미추적 -1
	const float LoadCeil = bTargetLoaded ? 1.f : (RealPct >= 0.f ? RealPct / 100.f : 0.95f);

	// 목표 = min(시간 페이스, 실제 로드 상한), 단조 증가
	float Target = FMath::Min(Pace, LoadCeil);
	Target = FMath::Max(Target, DisplayedPercent);

	DisplayedPercent = FMath::FInterpConstantTo(DisplayedPercent, Target, InDeltaTime, SmoothSpeed);
	DisplayedPercent = FMath::Clamp(DisplayedPercent, 0.f, 1.f);

	UpdateProgressUI();

	// 완료 판정: 로드 끝 + 최소시간 경과 + 바 도달 → 페이드 시작
	const bool bReady = bTargetLoaded && ElapsedTime >= MinDisplayTime && DisplayedPercent >= 0.999f;
	if (bReady && !bTransitionStarted)
	{
		if (FadeOutDuration > 0.f)
		{
			bFading = true;
		}
		else
		{
			BeginTransition();
		}
	}
}

void ULoadingWidget::UpdateProgressUI()
{
	if (HairlineBar)
	{
		HairlineBar->SetPercent(DisplayedPercent);
	}
	if (StatusText)
	{
		// 점 애니메이션 ". / .. / ..." 0.4s 스텝 순환 — 살아있는 느낌
		const int32 DotCount = 1 + (FMath::FloorToInt(ElapsedTime / 0.4f) % 3);
		const FString Dots = FString::ChrN(DotCount, TEXT('.'));
		const int32 Pct = FMath::RoundToInt(DisplayedPercent * 100.f);
		StatusText->SetText(FText::FromString(FString::Printf(
			TEXT("%s%s %d%%"), GetLoadingStageMsg(DisplayedPercent), *Dots, Pct)));
	}
}

void ULoadingWidget::BeginTransition()
{
	if (bTransitionStarted)
	{
		return;
	}
	bTransitionStarted = true;

	// BGM resolve 용 맵 타입 설정 (TransitionToLevel 의 세이브 단계는 의도적으로 우회 —
	// 부팅 시점엔 매니저에 게임 데이터가 로드되기 전이라 빈 데이터로 세이브를 덮을 위험)
	if (UCGGameInstance* GI = Cast<UCGGameInstance>(GetGameInstance()))
	{
		GI->SetCurrentMapType(ECurrentMapType::MainMap);
	}

	// 프리로드한 풀 패키지 경로 그대로 사용 (short name 동명 충돌/쿠킹 경로 차이 회피)
	UGameplayStatics::OpenLevel(this, FName(*TargetLevelPackagePath));
}
