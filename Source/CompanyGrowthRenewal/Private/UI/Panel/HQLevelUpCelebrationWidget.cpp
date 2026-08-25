#include "UI/Panel/HQLevelUpCelebrationWidget.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Element/Effects/ScoreOrbContainerWidget.h"

// 메달 글로스 스윕 튜닝 (cpp-only = Live Coding 주입 가능)
static constexpr float MedalGlossStart = 0.6f;      // 펀치/숫자 스왑과 동기
static constexpr float MedalGlossDuration = 0.4f;

void UHQLevelUpCelebrationWidget::SetLevelUpInfo(int32 InOldLevel, int32 InNewLevel, const FText& InRewardText)
{
	CachedOldLevel = InOldLevel;
	CachedNewLevel = InNewLevel;
	CachedRewardText = InRewardText;
}

void UHQLevelUpCelebrationWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 배경 클릭 바인딩
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UHQLevelUpCelebrationWidget::OnBackgroundClicked);
	}

	// 텍스트 설정
	if (OldLevelText)
		OldLevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d"), CachedOldLevel)));
	if (NewLevelText)
		NewLevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d"), CachedNewLevel)));
	if (RewardDescText)
		RewardDescText->SetText(CachedRewardText);
	if (TouchToCloseText)
		TouchToCloseText->SetText(FText::FromString(TEXT("화면을 터치하면 닫힙니다")));
	// 메달은 구 레벨로 시작 — 0.6s 펀치 순간 새 레벨로 스왑 (레벨업 핵심 비트)
	if (MedalLevelText)
		MedalLevelText->SetText(FText::AsNumber(CachedOldLevel));

	// 보상 없으면 플레이트째 접기 (호출부는 UnlockDescription 없을 때 빈 텍스트 전달)
	if (RewardPlate)
		RewardPlate->SetVisibility(CachedRewardText.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	// 스파클 팝 텍스처 주입 — DT_UIVFXTexture 단일 진실 (WBP엔 빈 Image만 배치)
	if (UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr)
	{
		TableMgr->ApplyUIVFXTexture(TEXT("SparkleMain"), SparkleA);
		TableMgr->ApplyUIVFXTexture(TEXT("SparkleSub"), SparkleB);
		TableMgr->ApplyUIVFXTexture(TEXT("SparkleMain"), SparkleC);
	}

	// 비인터랙티브 위젯 히트 테스트 무시 (BackgroundBtn 클릭 통과)
	auto SetHitTestInvisible = [](UWidget* W) {
		if (W) W->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	};
	SetHitTestInvisible(RayEffectImage);
	SetHitTestInvisible(TitleContainer);
	SetHitTestInvisible(GlowImage);
	SetHitTestInvisible(GlowSmallImage);
	SetHitTestInvisible(RaySubImage);
	SetHitTestInvisible(SparkelsImage);
	SetHitTestInvisible(ShineImage);
	if (MedalBox) MedalBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (LevelRow) LevelRow->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 초기 상태: 컨테이너가 있으면 컨테이너만 투명 (자식까지 0이면 이중 투명)
	LerpOpacity(RayEffectImage, 0.f);
	LerpOpacity(MedalBox, 0.f);
	LerpOpacity(TitleContainer, 0.f);
	if (LevelRow)
	{
		LerpOpacity(LevelRow, 0.f);
	}
	else
	{
		LerpOpacity(OldLevelText, 0.f);
		LerpOpacity(NewLevelText, 0.f);
	}
	if (RewardPlate)
	{
		LerpOpacity(RewardPlate, 0.f);
	}
	else
	{
		LerpOpacity(RewardDescText, 0.f);
	}
	LerpOpacity(TouchToCloseText, 0.f);

	// 플래시: 즉시 번쩍
	if (FlashOverlay)
	{
		FlashOverlay->SetRenderOpacity(1.f);
		FlashOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// VFX 레이어 초기화
	LerpOpacity(GlowImage, 0.f);
	LerpOpacity(GlowSmallImage, 0.f);
	LerpOpacity(RaySubImage, 0.f);
	LerpOpacity(SparkelsImage, 0.f);
	// 구 부유 샤인 폐기 — 스칠 표면이 없는 허공 스트릭이라 이질적 (메달 표면 글로스로 대체)
	if (ShineImage)
		ShineImage->SetVisibility(ESlateVisibility::Collapsed);

	// 메달 글로스/글린트 — MedalOverlay 최상단에 동적 생성 (링+숫자 위를 실제로 스침, WBP 불변)
	if (UOverlay* MedalOv = Cast<UOverlay>(GetWidgetFromName(TEXT("MedalOverlay"))))
	{
		auto EnsureFx = [&](const TCHAR* FxName, const TCHAR* TexPath, FVector2D FxSize, FLinearColor Tint)
		{
			UImage* Fx = Cast<UImage>(GetWidgetFromName(FxName));
			if (!Fx)
			{
				Fx = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FxName);
				if (!Fx) return;
				if (UTexture2D* Tex = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TexPath)).LoadSynchronous())
					Fx->SetBrushFromTexture(Tex);
				if (UOverlaySlot* FxSlot = MedalOv->AddChildToOverlay(Fx))
				{
					FxSlot->SetHorizontalAlignment(HAlign_Center);
					FxSlot->SetVerticalAlignment(VAlign_Center);
				}
			}
			Fx->SetDesiredSizeOverride(FxSize);
			Fx->SetColorAndOpacity(Tint);
			Fx->SetVisibility(ESlateVisibility::HitTestInvisible);
			Fx->SetRenderOpacity(0.f);
		};
		EnsureFx(TEXT("MedalGlossFx"), TEXT("/Game/CompanyGrowth/UI/CasualPack/VFX/Glow_02.Glow_02"),
			FVector2D(340.f, 340.f), FLinearColor(1.f, 0.85f, 0.55f, 1.f));
		EnsureFx(TEXT("MedalGlintFx"), TEXT("/Game/CompanyGrowth/UI/CasualPack/VFX/Polygon_04.Polygon_04"),
			FVector2D(52.f, 52.f), FLinearColor(1.f, 0.95f, 0.78f, 1.f));
	}

	for (UImage* Sparkle : { SparkleA, SparkleB, SparkleC })
	{
		if (Sparkle)
		{
			Sparkle->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Sparkle->SetRenderScale(FVector2D(0.f, 0.f));
			Sparkle->SetRenderOpacity(0.f);
		}
	}

	// 연출 상태 초기화
	IntroTimer = 0.f;
	RayRotation = 0.f;
	RaySubRotation = 0.f;
	SparkelsRotation = 0.f;
	bIntroComplete = false;
	bMedalPunchFired = false;

	// 타이틀 컨테이너 스케일 준비 (0.2초 후 시작하므로 여기선 대기)
	if (TitleContainer)
		TitleContainer->SetRenderScale(FVector2D(0.5f, 0.5f));
}

void UHQLevelUpCelebrationWidget::NativeOnDeactivated()
{
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UHQLevelUpCelebrationWidget::OnBackgroundClicked);
	}

	Super::NativeOnDeactivated();
}

void UHQLevelUpCelebrationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	IntroTimer += InDeltaTime;

	// ===== 상시 회전/펄스 (VFX 레이어) =====

	// Ray 메인 회전 — 느릴수록 웅장 (25는 어지러움 실측)
	RayRotation += InDeltaTime * 9.f;
	if (RayEffectImage)
		RayEffectImage->SetRenderTransformAngle(RayRotation);

	// Ray 서브 반대 회전 — 저알파 깔개용 미세 역회전 (빠르면 barber-pole 간섭)
	RaySubRotation -= InDeltaTime * 4.f;
	if (RaySubImage)
		RaySubImage->SetRenderTransformAngle(RaySubRotation);

	// Sparkels 느린 회전
	SparkelsRotation += InDeltaTime * 5.f;
	if (SparkelsImage)
		SparkelsImage->SetRenderTransformAngle(SparkelsRotation);

	// Glow 펄스
	if (GlowImage)
	{
		float S = 1.05f + 0.15f * FMath::Sin(IntroTimer * 2.f);
		GlowImage->SetRenderScale(FVector2D(S, S));
	}
	if (GlowSmallImage)
	{
		float S = 0.975f + 0.125f * FMath::Sin(IntroTimer * 2.5f + 1.f);
		GlowSmallImage->SetRenderScale(FVector2D(S, S));
	}

	// ===== VFX 페이드인 타임라인 =====

	// 0.0~0.4s: Glow, RaySub, Sparkels 페이드인
	if (IntroTimer <= 0.4f)
	{
		float T = FMath::Clamp(IntroTimer / 0.4f, 0.f, 1.f);
		LerpOpacity(GlowImage, T * 0.6f);
		LerpOpacity(RaySubImage, T * 0.5f);
		LerpOpacity(SparkelsImage, T * 0.7f);
	}

	// 0.1~0.4s: GlowSmall 페이드인
	if (IntroTimer >= 0.1f && IntroTimer <= 0.4f)
	{
		float T = FMath::Clamp((IntroTimer - 0.1f) / 0.3f, 0.f, 1.f);
		LerpOpacity(GlowSmallImage, T * 0.8f);
	}

	// 0.6~1.0s: 메달 글로스 스윕 — 펀치/숫자 스왑 순간 빛 덩어리+글린트가 메달 얼굴을 좌상→우하로 쓸고 지나감
	// T 클램프 + sin 봉투라 창 밖에선 자동으로 투명 (별도 분기 불필요)
	{
		const float T = FMath::Clamp((IntroTimer - MedalGlossStart) / MedalGlossDuration, 0.f, 1.f);
		const FVector2D GlossPos(FMath::Lerp(-150.f, 150.f, T), FMath::Lerp(-110.f, 110.f, T));
		const float Envelope = FMath::Sin(T * PI);
		if (UImage* Gloss = Cast<UImage>(GetWidgetFromName(TEXT("MedalGlossFx"))))
		{
			Gloss->SetRenderTranslation(GlossPos);
			Gloss->SetRenderOpacity(Envelope * 0.5f);
		}
		if (UImage* Glint = Cast<UImage>(GetWidgetFromName(TEXT("MedalGlintFx"))))
		{
			Glint->SetRenderTranslation(GlossPos);
			Glint->SetRenderOpacity(Envelope * 0.9f);
			Glint->SetRenderTransformAngle(120.f * T);
			const float GlintScale = 0.7f + 0.5f * Envelope;
			Glint->SetRenderScale(FVector2D(GlintScale, GlintScale));
		}
	}

	// ===== 기존 연출 타임라인 =====

	// 0.0~0.2s: 플래시 페이드아웃
	if (FlashOverlay && IntroTimer <= 0.2f)
	{
		float Alpha = FMath::Clamp(1.f - (IntroTimer / 0.2f), 0.f, 1.f);
		FlashOverlay->SetRenderOpacity(Alpha);
	}
	else if (FlashOverlay && IntroTimer > 0.2f)
	{
		FlashOverlay->SetRenderOpacity(0.f);
		FlashOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 0.0~0.4s: Ray + 메달 페이드인
	if (IntroTimer <= 0.4f)
	{
		float Alpha = FMath::Clamp(IntroTimer / 0.4f, 0.f, 1.f);
		LerpOpacity(RayEffectImage, Alpha);
		LerpOpacity(MedalBox, Alpha);
	}

	// 0.2~0.6s: 타이틀 컨테이너 (리본+텍스트) — 아래에서 떠오르며 스케일 바운스
	if (IntroTimer >= 0.2f && IntroTimer <= 0.6f)
	{
		float T = FMath::Clamp((IntroTimer - 0.2f) / 0.4f, 0.f, 1.f);
		LerpOpacity(TitleContainer, FMath::Min(T * 2.f, 1.f));

		// Y 오프셋: +60 → 0 (아래에서 위로 떠오름)
		float EasedT = FWidgetAnimationUtils::EaseOutBack(T);
		float OffsetY = FMath::Lerp(60.f, 0.f, EasedT);

		// 스케일: 0.5 → 1.15 → 1.0 (바운스)
		float Scale;
		if (T < 0.4f)
		{
			float HalfT = T / 0.4f;
			Scale = FMath::Lerp(0.5f, 1.15f, FWidgetAnimationUtils::EaseOutQuad(HalfT));
		}
		else
		{
			float HalfT = (T - 0.4f) / 0.6f;
			Scale = FMath::Lerp(1.15f, 1.0f, FWidgetAnimationUtils::EaseOutBack(HalfT));
		}

		if (TitleContainer)
		{
			TitleContainer->SetRenderTranslation(FVector2D(0.f, OffsetY));
			TitleContainer->SetRenderScale(FVector2D(Scale, Scale));
		}
	}

	// 0.4~0.7s: 레벨 행 등장
	if (IntroTimer >= 0.4f && IntroTimer <= 0.7f)
	{
		float Alpha = FMath::Clamp((IntroTimer - 0.4f) / 0.3f, 0.f, 1.f);
		if (LevelRow)
		{
			LerpOpacity(LevelRow, Alpha);
		}
		else
		{
			LerpOpacity(OldLevelText, Alpha);
			LerpOpacity(NewLevelText, Alpha);
		}
	}

	// 0.7~1.0s: 보상 플레이트 등장 (빈 보상 = Collapsed 유지)
	if (IntroTimer >= 0.7f && IntroTimer <= 1.0f)
	{
		float Alpha = FMath::Clamp((IntroTimer - 0.7f) / 0.3f, 0.f, 1.f);
		if (RewardPlate)
		{
			if (RewardPlate->GetVisibility() != ESlateVisibility::Collapsed)
				LerpOpacity(RewardPlate, Alpha);
		}
		else
		{
			LerpOpacity(RewardDescText, Alpha);
		}
	}

	// 0.6s: 메달 + 새 레벨 스케일 펀치
	if (!bMedalPunchFired && IntroTimer >= 0.6f)
	{
		bMedalPunchFired = true;
		MedalPunchAnim.Start(1.25f, 0.25f);
		if (MedalLevelText)
			MedalLevelText->SetText(FText::AsNumber(CachedNewLevel));
		if (UScoreOrbContainerWidget* Confetti = Cast<UScoreOrbContainerWidget>(GetWidgetFromName(TEXT("ConfettiLayer"))))
			Confetti->PlayConfettiBurst(48, 0.4f, EConfettiStyle::MedalBurst);
	}
	if (MedalPunchAnim.IsPlaying())
	{
		// 유니티 빌드에서 GachaReveal 익명 네임스페이스 PunchScale 과 셰도잉(C4459) — 구체 이름 사용
		const float MedalScale = MedalPunchAnim.Tick(InDeltaTime);
		if (MedalBox) MedalBox->SetRenderScale(FVector2D(MedalScale, MedalScale));
		if (NewLevelText) NewLevelText->SetRenderScale(FVector2D(MedalScale, MedalScale));
	}

	// 스파클 팝 3연타
	DriveSparklePop(SparkleA, 0.5f);
	DriveSparklePop(SparkleB, 0.7f);
	DriveSparklePop(SparkleC, 0.9f);

	// 1.2s: 터치 안내 등장 + 등장 완료
	if (!bIntroComplete && IntroTimer >= 1.2f)
	{
		bIntroComplete = true;
	}

	// 등장 완료 후: 터치 안내 투명도 펄스 (0.3 ~ 1.0 사이 반복)
	if (bIntroComplete && TouchToCloseText)
	{
		float Pulse = 0.65f + 0.35f * FMath::Sin((IntroTimer - 1.2f) * 3.f);
		TouchToCloseText->SetRenderOpacity(Pulse);
	}
}

void UHQLevelUpCelebrationWidget::OnBackgroundClicked()
{
	if (!bIntroComplete) return;

	CloseWithAnimation();
}

void UHQLevelUpCelebrationWidget::DriveSparklePop(UImage* Sparkle, float StartTime) const
{
	if (!Sparkle) return;

	const float T = (IntroTimer - StartTime) / 0.5f;
	if (T < 0.f) return;
	if (T >= 1.f)
	{
		Sparkle->SetRenderOpacity(0.f);
		return;
	}
	// 사인 아치로 커졌다 사라짐 + 살짝 회전
	const float Arch = FMath::Sin(T * PI);
	Sparkle->SetRenderScale(FVector2D(1.15f * Arch, 1.15f * Arch));
	Sparkle->SetRenderOpacity(FMath::Min(Arch * 1.4f, 1.f));
	Sparkle->SetRenderTransformAngle(40.f * T);
}

void UHQLevelUpCelebrationWidget::LerpOpacity(UWidget* Widget, float Alpha)
{
	if (Widget)
	{
		Widget->SetRenderOpacity(Alpha);
	}
}
