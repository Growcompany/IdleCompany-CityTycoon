// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panel/LoadingWidget.h"

#include "Components/ProgressBar.h"
#include "Components/Image.h"
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
	for (FAmbientBird& Bird : Birds)
	{
		ResetBird(Bird, /*bInitialScatter=*/true);
	}

	// 첫 스트릭은 시작부터 화면 중앙에 떠 있게 (진입 대기 없이 즉시 체감)
	Streaks.SetNum(2);
	Streaks[0].bActive = true;
	Streaks[0].HeadX = 1500.f;
	Streaks[0].BaseY = 400.f;
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
	Bird.PosX = bInitialScatter ? FMath::FRandRange(0.40f, 1.00f) : FMath::FRandRange(1.03f, 1.12f);
	Bird.BaseY = FMath::FRandRange(0.09f, 0.24f);
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
		if (Bird.PosX < 0.30f)   // 좌상단 로고 영역 침범 전에 재활용
		{
			ResetBird(Bird, false);
		}
	}
}

void ULoadingWidget::TickClouds(float InDeltaTime)
{
	CloudTime += InDeltaTime;
	// ⚠ 2026-07-29 이후 **현재 미사용** — 낱개 구름(CloudA/B)을 가로 타일링 '구름 띠'
	//   (M_UI_CloudBand + MI_UI_CloudBand_A/B, Panner 스크롤)로 대체했다.
	//   WBP 에 CloudA/B 위젯이 없으므로 BindWidgetOptional 이 null 이라 아래 루프는 no-op.
	//   띠 방식이 ① 더 풍성하고 ② 화면비가 넓어져도 빈 곳이 안 생기고 ③ 속도가 머티리얼 파라미터라 튜닝이 쉽다.
	//   되돌릴 일이 없으면 이 함수 + CloudA/CloudB/CloudTime/TickClouds 선언까지 정리 대상.
	// 슬롯 기준점 x=2600(화면 밖 우측) — 오프셋이 Span 을 넘으면 자동 랩(오른쪽 재진입)
	struct FCloudDrift { UImage* Img; float Speed; float InitOffset; float Span; };
	const FCloudDrift Drifts[] = {
		{ CloudA, 95.f, 1700.f, 3120.f },
		{ CloudB, 62.f, 1000.f, 2960.f },   // 시작 위상 주의 — 2600이면 좌측 끝+로고 뒤에서 시작해 안 보임
	};
	for (const FCloudDrift& D : Drifts)
	{
		if (D.Img)
		{
			const float Offset = FMath::Fmod(D.InitOffset + D.Speed * CloudTime, D.Span);
			D.Img->SetRenderTranslation(FVector2D(-Offset, 0.f));
		}
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
				Streak.BaseY = FMath::FRandRange(340.f, 470.f);
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
				FLinearColor(1.f, 1.f, 1.f, SegAlpha[SegIdx] * LifeAlpha), true, 4.0f);
		}
	}

	// 앰비언트 새 — 갈매기 실루엣을 날개 2획으로 (획 분리 = MakeLines 미터 한계 회피)
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FLinearColor BirdColor(0.08f, 0.13f, 0.22f, 0.75f);
	for (const FAmbientBird& Bird : Birds)
	{
		if (Bird.PosX > 1.0f)
		{
			continue;   // 아직 화면 밖(재진입 대기)
		}
		const FVector2D Center(Bird.PosX * Size.X,
			(Bird.BaseY + 0.006f * FMath::Sin(Bird.DriftPhase)) * Size.Y);
		const float TipY = FMath::Sin(Bird.FlapPhase) * Bird.WingSpan * 0.5f;
		const float Thickness = FMath::Max(1.5f, Bird.WingSpan * 0.18f);
		const TArray<FVector2D> LeftWing{ Center, Center + FVector2D(-Bird.WingSpan, -TipY) };
		const TArray<FVector2D> RightWing{ Center, Center + FVector2D(Bird.WingSpan, -TipY) };
		FSlateDrawElement::MakeLines(OutDrawElements, MaxLayer + 1,
			AllottedGeometry.ToPaintGeometry(), LeftWing, ESlateDrawEffect::None, BirdColor, true, Thickness);
		FSlateDrawElement::MakeLines(OutDrawElements, MaxLayer + 1,
			AllottedGeometry.ToPaintGeometry(), RightWing, ESlateDrawEffect::None, BirdColor, true, Thickness);
	}
	return MaxLayer + 1;
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
