// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/EnhanceStarforceModalWidget.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Data/EmployeeStatsData.h"
#include "Data/EmployeeTypes.h"
#include "Enum/ResourceType.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/Font.h"

namespace
{
	// ⚠ 별판 색 4종(StarGold/StarDim/SfStarOutline*)은 T_UIIcon_Star_Sf{Filled,Empty} 에 baked —
	//   값 변경 시 Tools/Starforce/bake_stars.py 재실행 후 재임포트 (DOBO_ASSET_INTEGRATION §5)
	// 채운 별 골드 #F5B93C (linear)
	const FLinearColor StarGold(0.9131f, 0.4851f, 0.0452f, 1.f);
	// 빈 칸 딤 — 라이트 플레이트 위 스틸 그레이 #C9D0DA (linear)
	const FLinearColor StarDim(0.584f, 0.631f, 0.701f, 1.f);
	// 유지 뮤트 잉크 (직원창 InkMute 와 동일값 — 유니티 빌드 충돌로 별도 이름)
	const FLinearColor StarInkMute(0.258f, 0.300f, 0.356f, 1.f);
	// 하락/자금부족 레드 #E0524D (linear)
	const FLinearColor AlertRed(0.745f, 0.084f, 0.074f, 1.f);

	// 스탬프 잉크 (라이트 판 가독용 다크 파생 — 목업 v3 확정색)
	const FLinearColor StampGold(0.479f, 0.238f, 0.003f, 1.f);   // #B8860B
	const FLinearColor StampRed(0.524f, 0.041f, 0.024f, 1.f);    // #C0392B

	// 비용 수치 잉크
	const FLinearColor CostInk(0.017f, 0.021f, 0.028f, 1.f);

	// v4 능력치 변화 존 잉크 (목업 v4 확정색, linear)
	const FLinearColor SfDeltaInkSub(0.102f, 0.147f, 0.205f, 1.f);   // #5A6B7D 이름/캡션
	const FLinearColor SfDeltaMute(0.254f, 0.309f, 0.381f, 1.f);     // #8A97A6 현재값
	const FLinearColor SfDeltaGain(0.027f, 0.205f, 0.061f, 1.f);     // #2E7D46 이후값
	const FLinearColor SfArrowGold(0.584f, 0.254f, 0.006f, 1.f);     // #C98A12

	// v4 별 아웃라인 — 채움 다크브라운 / 빈칸 스틸 (linear)
	const FLinearColor SfStarOutlineFill(0.107f, 0.042f, 0.004f, 1.f);   // #5C3A0E
	const FLinearColor SfStarOutlineEmpty(0.366f, 0.417f, 0.503f, 1.f);  // #A3ADBC

	// 결과 연출 타이밍/강도 (절제 — 한 박자 임팩트). Sf 접두사 = GachaReveal 동명 상수와 유니티 빌드 충돌 회피
	constexpr float SfFlashDuration = 0.45f;
	constexpr float SfShakeDuration = 0.38f;
	constexpr float ShakeAmplitude = 12.f;

	// v4.2 화려함 패스 — CTA 샤인(HQ 패널 수식 클론) / 웰 글로우 / 아이들 글린트
	constexpr float SfShinePeriod = 2.8f;
	constexpr float SfShineTravel = 0.55f;
	constexpr float SfShineHalf = 300.f;
	constexpr float SfWellGlowBaseA = 0.28f;
	constexpr float SfGlintInterval = 1.7f;
	constexpr float SfGlintDuration = 0.32f;

	// v4 스택바: 세그먼트 폭 = 확률(HBox Fill 값), 좁으면 라벨 생략, 0% = 세그먼트째 접기
	void SetOddsSegment(UTextBlock* RowText, UBorder* Segment, const TCHAR* Label, float Probability)
	{
		if (!RowText)
		{
			return;
		}

		UWidget* Toggle = Segment ? static_cast<UWidget*>(Segment) : static_cast<UWidget*>(RowText);
		if (Probability <= KINDA_SMALL_NUMBER)
		{
			Toggle->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
		Toggle->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		if (Segment)
		{
			if (UHorizontalBoxSlot* SegSlot = Cast<UHorizontalBoxSlot>(Segment->Slot))
			{
				FSlateChildSize SegSize(ESlateSizeRule::Fill);
				SegSize.Value = Probability;
				SegSlot->SetSize(SegSize);
			}
		}
		RowText->SetText(Probability >= 0.18f
			? FText::FromString(FString::Printf(TEXT("%s %d%%"), Label, FMath::RoundToInt(Probability * 100.f)))
			: FText::GetEmpty());
	}
}

void UEnhanceStarforceModalWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		EmployeeManager = GI->GetSubsystem<UEmployeeManager>();
	}

	if (EnhanceActionButton)
	{
		EnhanceActionButton->OnClicked().AddUObject(this, &UEnhanceStarforceModalWidget::OnEnhanceClicked);
		// 컬러 CTA 라벨 아웃라인 규칙 — 골드 버튼 = 다크골드 #8A5B00 2px
		EnhanceActionButton->SetTextOutlineEnabled(true, 2.f, FLinearColor(0.254f, 0.105f, 0.f, 1.f));
	}

	if (CloseButton)
	{
		CloseButton->OnCloseClicked.AddDynamic(this, &UEnhanceStarforceModalWidget::OnCloseDelegate);
	}

	// 도킹 패널 바깥 클릭 = 닫기 (ProductSellModal 패턴 — 투명 풀스크린 버튼, 카드가 위에서 클릭 소비)
	if (UButton* BgBtn = Cast<UButton>(GetWidgetFromName(TEXT("BackgroundBtn"))))
	{
		BgBtn->OnClicked.AddDynamic(this, &UEnhanceStarforceModalWidget::OnCloseDelegate);
	}

	// 비활성 오버레이라 NativeOnActivated 등장 트윈이 안 돌아감 — 여기서 직접 (매 오픈 새 인스턴스)
	StartAppearTween();
}

void UEnhanceStarforceModalWidget::NativeDestruct()
{
	if (EnhanceActionButton)
	{
		EnhanceActionButton->OnClicked().RemoveAll(this);
	}

	if (CloseButton)
	{
		CloseButton->OnCloseClicked.RemoveDynamic(this, &UEnhanceStarforceModalWidget::OnCloseDelegate);
	}

	if (UButton* BgBtn = Cast<UButton>(GetWidgetFromName(TEXT("BackgroundBtn"))))
	{
		BgBtn->OnClicked.RemoveDynamic(this, &UEnhanceStarforceModalWidget::OnCloseDelegate);
	}

	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeEnhanced.RemoveAll(this);
	}

	// teardown 중엔 GameInstance 가 먼저 사라질 수 있음
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UResourceItemManager* ResourceMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			ResourceMgr->OnResourceChanged.RemoveAll(this);
		}

		// M9 InvestFirstStatPoint — 하이라이트 타겟 등록 해제 (비활성 오버레이라 Deactivated 안 옴 → Destruct 에서)
		if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
		{
			MissionMgr->UnregisterStarforceModal(this);
		}
	}

	Super::NativeDestruct();
}

void UEnhanceStarforceModalWidget::PlayResultFx(UEmployeeManager::EEnhanceResult Result, int32 NewLevel)
{
	// 연속 강화: 이전 결과의 진행 중 연출을 끊어야 이전 별이 확대 상태로 방치되지 않음
	SlamElapsed = -1.f;
	BreakElapsed = -1.f;
	PunchStarIndex = -1;

	USoundManagerSubsystem* SoundMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USoundManagerSubsystem>() : nullptr;

	switch (Result)
	{
	case UEmployeeManager::EEnhanceResult::Success:
		// 새 별 내리꽂기 — 크게 나타나 빈 칸에 박힘. 플래시/버스트/사운드는 착지 순간(NativeTick)
		PunchStarIndex = NewLevel - 1;
		SlamElapsed = 0.f;
		if (StarImages.IsValidIndex(PunchStarIndex) && StarImages[PunchStarIndex])
		{
			StarImages[PunchStarIndex]->SetRenderScale(FVector2D(2.4f, 2.4f));
			StarImages[PunchStarIndex]->SetRenderOpacity(0.25f);
		}
		break;
	case UEmployeeManager::EEnhanceResult::Maintain:
		ResultPunch.Start(1.1f, 0.22f);
		FlashColor = StarInkMute;
		FlashPeakAlpha = 0.10f;
		FlashElapsed = 0.f;
		ResultFadeElapsed = 0.f;
		if (SoundMgr)
		{
			SoundMgr->PlayUISound(CGUISoundTags::GachaStamp);
		}
		break;
	case UEmployeeManager::EEnhanceResult::Downgrade:
		// 별 깨짐 — 붉게 흔들리며 팽창·소멸 + 풀스크린 레드 플래시 + 화면 셰이크
		PunchStarIndex = NewLevel;
		BreakElapsed = 0.f;
		if (StarImages.IsValidIndex(PunchStarIndex) && StarImages[PunchStarIndex])
		{
			StarImages[PunchStarIndex]->SetColorAndOpacity(AlertRed);
		}
		FlashColor = AlertRed;
		FlashPeakAlpha = 0.28f;
		FlashElapsed = 0.f;
		ShakeElapsed = 0.f;
		WellRedElapsed = 0.f;
		if (SoundMgr)
		{
			SoundMgr->PlayUISound(CGUISoundTags::NotificationError);
		}
		break;
	}

	if (StarImages.IsValidIndex(PunchStarIndex) && StarImages[PunchStarIndex])
	{
		StarImages[PunchStarIndex]->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
	if (ResultText)
	{
		ResultText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
}

void UEnhanceStarforceModalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UImage* FxStar = StarImages.IsValidIndex(PunchStarIndex) ? StarImages[PunchStarIndex] : nullptr;

	// 성공: 내리꽂기 — 가속(EaseIn)으로 줄어들며 착지, 착지 순간 반동+플래시+버스트+사운드
	if (SlamElapsed >= 0.f && FxStar)
	{
		SlamElapsed += InDeltaTime;
		const float T = FMath::Clamp(SlamElapsed / 0.22f, 0.f, 1.f);
		const float SlamScale = FMath::Lerp(2.4f, 1.f, T * T);
		FxStar->SetRenderScale(FVector2D(SlamScale, SlamScale));
		FxStar->SetRenderOpacity(FMath::Lerp(0.25f, 1.f, FMath::Sqrt(T)));
		if (T >= 1.f)
		{
			SlamElapsed = -1.f;
			StarPunch.Start(1.22f, 0.16f);
			FlashColor = StarGold;
			FlashPeakAlpha = 0.30f;
			FlashElapsed = 0.f;
			BurstElapsed = 0.f;
			SparkleElapsed = 0.f;
			ShineElapsed = 0.f;
			WellPulseElapsed = 0.f;
			DeltaPunch.Start(1.05f, 0.3f);
			if (USoundManagerSubsystem* SoundMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<USoundManagerSubsystem>() : nullptr)
			{
				SoundMgr->PlayUISound(CGUISoundTags::UpgradeSuccess);
			}
		}
	}

	// 성공 착지 후속 — 스파클 3연타 (스케일 팝 + 회전, 0.08s 스태거)
	if (SparkleElapsed >= 0.f)
	{
		SparkleElapsed += InDeltaTime;
		UImage* Sparkles[3] = { SparkleA, SparkleB, SparkleC };
		bool bSparkleDone = true;
		for (int32 i = 0; i < 3; ++i)
		{
			if (!Sparkles[i])
			{
				continue;
			}
			const float T = FMath::Clamp((SparkleElapsed - i * 0.08f) / 0.45f, 0.f, 1.f);
			const float Pop = FMath::Sin(PI * T) * 1.15f;
			Sparkles[i]->SetRenderScale(FVector2D(Pop, Pop));
			Sparkles[i]->SetRenderOpacity(FMath::Sin(PI * T));
			Sparkles[i]->SetRenderTransformAngle(T * 70.f + i * 40.f);
			bSparkleDone &= (T >= 1.f);
		}
		if (bSparkleDone)
		{
			SparkleElapsed = -1.f;
		}
	}

	// 성공 착지 후속 — 별판 광택 스윕 (좌→우)
	if (ShineElapsed >= 0.f && ShineSweep)
	{
		ShineElapsed += InDeltaTime;
		const float T = FMath::Clamp(ShineElapsed / 0.5f, 0.f, 1.f);
		ShineSweep->SetRenderTranslation(FVector2D(FMath::Lerp(-420.f, 420.f, FWidgetAnimationUtils::EaseOutQuad(T)), 0.f));
		ShineSweep->SetRenderOpacity(FMath::Sin(PI * T) * 0.55f);
		if (T >= 1.f)
		{
			ShineElapsed = -1.f;
			ShineSweep->SetRenderOpacity(0.f);
		}
	}

	// 앰비언트 — 카드 뒤 골드 글로우 숨쉬기
	AmbientTime += InDeltaTime;
	if (AmbientGlow)
	{
		const float Breath = 0.11f + 0.045f * FMath::Sin(AmbientTime * 1.6f);
		AmbientGlow->SetRenderOpacity(Breath);
		const float BreathScale = 1.f + 0.03f * FMath::Sin(AmbientTime * 1.6f + 0.8f);
		AmbientGlow->SetRenderScale(FVector2D(BreathScale, BreathScale));
	}

	// 하락: 별 깨짐 — 흔들리며 팽창·소멸 후 딤 상태 복원
	if (BreakElapsed >= 0.f && FxStar)
	{
		BreakElapsed += InDeltaTime;
		const float T = FMath::Clamp(BreakElapsed / 0.45f, 0.f, 1.f);
		FxStar->SetRenderTransformAngle(FMath::Sin(T * 42.f) * 16.f * (1.f - T));
		const float BreakScale = 1.f + 0.35f * T;
		FxStar->SetRenderScale(FVector2D(BreakScale, BreakScale));
		FxStar->SetRenderOpacity(1.f - T);
		if (T >= 1.f)
		{
			BreakElapsed = -1.f;
			FxStar->SetRenderTransformAngle(0.f);
			FxStar->SetRenderScale(FVector2D(1.f, 1.f));
			FxStar->SetRenderOpacity(1.f);
			ApplyStarStyle(FxStar, false);
		}
	}

	if (StarPunch.IsPlaying() && FxStar)
	{
		const float StarScale = StarPunch.Tick(InDeltaTime);
		FxStar->SetRenderScale(FVector2D(StarScale, StarScale));
	}

	if (ResultPunch.IsPlaying())
	{
		const float TextScale = ResultPunch.Tick(InDeltaTime);
		// 스탬프(도장)째 쾅 — 미배선 WBP 는 텍스트만
		UWidget* PunchTarget = ResultStamp ? static_cast<UWidget*>(ResultStamp) : static_cast<UWidget*>(ResultText);
		if (PunchTarget)
		{
			PunchTarget->SetRenderScale(FVector2D(TextScale, TextScale));
		}
	}

	if (FlashElapsed >= 0.f && ResultFlash)
	{
		FlashElapsed += InDeltaTime;
		const float T = FMath::Clamp(FlashElapsed / SfFlashDuration, 0.f, 1.f);
		FLinearColor Color = FlashColor;
		Color.A = FlashPeakAlpha * (1.f - FWidgetAnimationUtils::EaseOutQuad(T));
		ResultFlash->SetColorAndOpacity(Color);
		if (T >= 1.f)
		{
			FlashElapsed = -1.f;
		}
	}

	if (ShakeElapsed >= 0.f)
	{
		ShakeElapsed += InDeltaTime;
		const float T = FMath::Clamp(ShakeElapsed / SfShakeDuration, 0.f, 1.f);
		// 화면 전체 임팩트 — 루트 오버레이(카드+플래시+버스트)째 흔들기, 구 카드 단독 셰이크 대체
		UWidget* ShakeTarget = GetWidgetFromName(TEXT("ModalRootOverlay"));
		if (!ShakeTarget)
		{
			ShakeTarget = GetWidgetFromName(TEXT("ModalCardBox"));
		}
		if (ShakeTarget)
		{
			const float Offset = FMath::Sin(T * 30.f) * ShakeAmplitude * (1.f - T);
			ShakeTarget->SetRenderTranslation(FVector2D(T >= 1.f ? 0.f : Offset, 0.f));
		}
		if (T >= 1.f)
		{
			ShakeElapsed = -1.f;
		}
	}

	// 성공 버스트 — 골드 방사 글로우 팽창+페이드
	if (BurstElapsed >= 0.f && BurstGlow)
	{
		BurstElapsed += InDeltaTime;
		const float T = FMath::Clamp(BurstElapsed / 0.55f, 0.f, 1.f);
		const float Ease = FWidgetAnimationUtils::EaseOutQuad(T);
		const float BurstScale = FMath::Lerp(0.7f, 1.35f, Ease);
		BurstGlow->SetRenderScale(FVector2D(BurstScale, BurstScale));
		BurstGlow->SetRenderOpacity(0.55f * (1.f - Ease));
		if (T >= 1.f)
		{
			BurstElapsed = -1.f;
			BurstGlow->SetRenderOpacity(0.f);
		}
	}

	// 다음 별 펄스 — 다음에 채워질 칸을 숨쉬듯 표시 (별 연출 중엔 그 칸을 건드리지 않음)
	PulseTime += InDeltaTime;
	if (EmployeeManager)
	{
		if (FEmployeeInstance* Employee = EmployeeManager->GetEmployeeData(EmployeeID))
		{
			const int32 E = Employee->EnhancementLevel;
			const bool bFxOnPulseStar = (E == PunchStarIndex) && (SlamElapsed >= 0.f || BreakElapsed >= 0.f || StarPunch.IsPlaying());
			if (E < UEmployeeManager::MaxEnhancementLevel && StarImages.IsValidIndex(E) && StarImages[E] && !bFxOnPulseStar)
			{
				StarImages[E]->SetRenderOpacity(0.55f + 0.45f * FMath::Abs(FMath::Sin(PulseTime * 2.6f)));
			}
		}
	}

	// ===== v4.2 화려함 패스 =====
	VfxTime += InDeltaTime;

	// 성공 착지: 웰 글로우 골드 펄스
	if (WellPulseElapsed >= 0.f && WellGlow)
	{
		WellPulseElapsed += InDeltaTime;
		const float T = FMath::Clamp(WellPulseElapsed / 0.5f, 0.f, 1.f);
		const float Wave = FMath::Sin(PI * T);
		WellGlow->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, SfWellGlowBaseA + 0.45f * Wave));
		const float GlowScale = 1.f + 0.16f * Wave;
		WellGlow->SetRenderScale(FVector2D(GlowScale, GlowScale));
		if (T >= 1.f)
		{
			WellPulseElapsed = -1.f;
		}
	}

	// 하락: 웰 레드 틴트 펄스 (끝나면 골드 복원)
	if (WellRedElapsed >= 0.f && WellGlow)
	{
		WellRedElapsed += InDeltaTime;
		const float T = FMath::Clamp(WellRedElapsed / 0.5f, 0.f, 1.f);
		WellGlow->SetColorAndOpacity(FLinearColor(1.f, 0.25f, 0.2f, SfWellGlowBaseA + 0.3f * FMath::Sin(PI * T)));
		if (T >= 1.f)
		{
			WellRedElapsed = -1.f;
			WellGlow->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, SfWellGlowBaseA));
		}
	}

	// 성공 착지: 델타존(능력치 변화) 강조 펀치
	if (DeltaPunch.IsPlaying() && PreviewChip)
	{
		const float ChipScale = DeltaPunch.Tick(InDeltaTime);
		PreviewChip->SetRenderScale(FVector2D(ChipScale, ChipScale));
	}

	// 아이들 별 글린트 — 주기마다 채워진 별 하나가 살짝 팝 (FX 중인 칸은 양보)
	if (GlintElapsed < 0.f)
	{
		GlintTimer += InDeltaTime;
		FEmployeeInstance* GlintEmployee = EmployeeManager ? EmployeeManager->GetEmployeeData(EmployeeID) : nullptr;
		const int32 FilledCount = GlintEmployee ? GlintEmployee->EnhancementLevel : 0;
		if (GlintTimer >= SfGlintInterval)
		{
			GlintTimer = 0.f;
			if (FilledCount > 0)
			{
				const int32 Pick = FMath::RandRange(0, FilledCount - 1);
				if (Pick != PunchStarIndex && StarImages.IsValidIndex(Pick) && StarImages[Pick])
				{
					GlintStarIndex = Pick;
					GlintElapsed = 0.f;
					StarImages[Pick]->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
				}
			}
		}
	}
	else if (StarImages.IsValidIndex(GlintStarIndex) && StarImages[GlintStarIndex] && GlintStarIndex != PunchStarIndex)
	{
		GlintElapsed += InDeltaTime;
		const float T = FMath::Clamp(GlintElapsed / SfGlintDuration, 0.f, 1.f);
		const float GlintScale = 1.f + 0.14f * FMath::Sin(PI * T);
		StarImages[GlintStarIndex]->SetRenderScale(FVector2D(GlintScale, GlintScale));
		if (T >= 1.f)
		{
			StarImages[GlintStarIndex]->SetRenderScale(FVector2D(1.f, 1.f));
			GlintElapsed = -1.f;
		}
	}
	else
	{
		GlintElapsed = -1.f;
	}

	// 다음 레벨 숫자 배후 글로우 브리딩 (정지 상태 밀도 — 트리 LevelGlowImg, 미배선 WBP 무해)
	if (UWidget* LvGlow = GetWidgetFromName(TEXT("LevelGlowImg")))
	{
		LvGlow->SetRenderOpacity(0.45f + 0.18f * FMath::Sin(VfxTime * 1.8f));
	}

	// CTA 레디 샤인 — 강화 가능 동안 주기 스윕 (HQ 패널 수식 클론)
	if (CTAShineImage)
	{
		if (!bSheenActive)
		{
			CTAShineImage->SetRenderOpacity(0.f);
		}
		else
		{
			const float Phase = FMath::Fmod(VfxTime, SfShinePeriod);
			if (Phase < SfShineTravel)
			{
				const float T = Phase / SfShineTravel;
				CTAShineImage->SetRenderTranslation(FVector2D(FMath::Lerp(-SfShineHalf, SfShineHalf, T), 0.f));
				CTAShineImage->SetRenderOpacity(0.6f * FMath::Sin(T * PI));
			}
			else
			{
				CTAShineImage->SetRenderOpacity(0.f);
			}
		}
	}

	// 결과 스탬프 자동 페이드 (1.4s 유지 → 0.4s 페이드)
	if (ResultFadeElapsed >= 0.f && ResultText)
	{
		ResultFadeElapsed += InDeltaTime;
		if (ResultFadeElapsed > 1.4f)
		{
			const float T = FMath::Clamp((ResultFadeElapsed - 1.4f) / 0.4f, 0.f, 1.f);
			UWidget* FadeTarget = ResultStamp ? static_cast<UWidget*>(ResultStamp) : static_cast<UWidget*>(ResultText);
			FadeTarget->SetRenderOpacity(1.f - T);
			if (T >= 1.f)
			{
				SetStamp(FText::GetEmpty(), StarInkMute);
				ResultFadeElapsed = -1.f;
				// 페이드로 비운 직후 MAX 면 상시 표기 복원
				FEmployeeInstance* Employee = EmployeeManager ? EmployeeManager->GetEmployeeData(EmployeeID) : nullptr;
				if (Employee && Employee->EnhancementLevel >= UEmployeeManager::MaxEnhancementLevel)
				{
					SetStamp(FText::FromString(TEXT("MAX")), StampGold);
				}
			}
		}
	}
}

UWidget* UEnhanceStarforceModalWidget::GetEnhanceActionButtonWidget() const
{
	return EnhanceActionButton;
}

void UEnhanceStarforceModalWidget::Configure(int32 InEmployeeID)
{
	EmployeeID = InEmployeeID;

	// push 직후(NativeConstruct 이전) 호출 대비
	if (!EmployeeManager)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			EmployeeManager = GI->GetSubsystem<UEmployeeManager>();
		}
	}

	// 컨테이너 풀링 재사용 대비 매 오픈 재바인딩 (중복구독 가드)
	if (EmployeeManager)
	{
		EmployeeManager->OnEmployeeEnhanced.RemoveAll(this);
		EmployeeManager->OnEmployeeEnhanced.AddUObject(this, &UEnhanceStarforceModalWidget::HandleEmployeeEnhanced);
	}

	// M9 InvestFirstStatPoint — 모달 등록이 [강화 실행] 버튼(p2) 하이라이트 타겟 겸 OpenModal→PressAction 전이
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMissionManagerSubsystem* MissionMgr = GI->GetSubsystem<UMissionManagerSubsystem>())
		{
			MissionMgr->RegisterStarforceModal(this);
		}
	}

	// 연출 텍스처 주입 (DT_UIVFXTexture 단일 진실 — WBP 엔 빈 Image)
	if (UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>())
	{
		TableMgr->ApplyUIVFXTexture(TEXT("SparkleMain"), SparkleA);
		TableMgr->ApplyUIVFXTexture(TEXT("SparkleSub"), SparkleB);
		TableMgr->ApplyUIVFXTexture(TEXT("SparkleMain"), SparkleC);
		TableMgr->ApplyUIVFXTexture(TEXT("CTAShine"), CTAShineImage);
	}

	// v4.2 연출 상태 리셋 (컨테이너 재사용 대비)
	WellPulseElapsed = -1.f;
	WellRedElapsed = -1.f;
	GlintTimer = 0.f;
	GlintElapsed = -1.f;
	GlintStarIndex = -1;
	if (WellGlow)
	{
		WellGlow->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, SfWellGlowBaseA));
		WellGlow->SetRenderScale(FVector2D(1.f, 1.f));
	}
	if (PreviewChip)
	{
		PreviewChip->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}

	if (UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
	{
		ResourceMgr->OnResourceChanged.RemoveAll(this);
		ResourceMgr->OnResourceChanged.AddUObject(this, &UEnhanceStarforceModalWidget::HandleResourceChanged);
	}

	SetStamp(FText::GetEmpty(), StarInkMute);
	ResultFadeElapsed = -1.f;

	Rebuild();
}

void UEnhanceStarforceModalWidget::EnsureStarImages()
{
	if (!StarBox || StarImages.Num() == UEmployeeManager::MaxEnhancementLevel)
	{
		return;
	}

	StarImages.Reset();
	UPanelWidget* Groups[3] = { StarGroup0, StarGroup1, StarGroup2 };
	const bool bGrouped = Groups[0] && Groups[1] && Groups[2];
	if (bGrouped)
	{
		// ⚠ 그룹 모드에서 StarBox.ClearChildren 금지 — 그룹이 StarBox 의 자식이라 그룹째 날아가 별이 고아가 됨
		for (UPanelWidget* G : Groups)
		{
			G->ClearChildren();
		}
	}
	else
	{
		StarBox->ClearChildren();
	}

	// 사전 합성 별 2장(dobo Star_Round 실루엣 + 코드 상수색 bake) — 아웃라인/섀도는 텍스처가 소유
	StarTexFilled = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Rating/T_UIIcon_Star_SfFilled.T_UIIcon_Star_SfFilled"))).LoadSynchronous();
	StarTexEmpty = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/Rating/T_UIIcon_Star_SfEmpty.T_UIIcon_Star_SfEmpty"))).LoadSynchronous();

	for (int32 i = 0; i < UEmployeeManager::MaxEnhancementLevel; ++i)
	{
		UImage* Star = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		ApplyStarStyle(Star, false);
		// v2: 5·5·2 그룹 분배(그룹 간격은 WBP 슬롯), 그룹 미배선 WBP 는 일렬 폴백
		UPanelWidget* Host = bGrouped ? Groups[i < 5 ? 0 : (i < 10 ? 1 : 2)] : StarBox;
		Host->AddChild(Star);
		StarImages.Add(Star);
	}
}

void UEnhanceStarforceModalWidget::ApplyStarStyle(UImage* Star, bool bFilled) const
{
	if (!Star)
	{
		return;
	}

	// 통브러시 교체 + 틴트 화이트 리셋 (하락 연출의 레드 곱 원복)
	FSlateBrush Brush;
	Brush.SetResourceObject(bFilled ? StarTexFilled : StarTexEmpty);
	Brush.ImageSize = FVector2D(StarSize, StarSize);
	Star->SetBrush(Brush);
	Star->SetColorAndOpacity(FLinearColor::White);
}

void UEnhanceStarforceModalWidget::EnsureDeltaRows()
{
	if (!DeltaGrid || DeltaCurTexts.Num() == 6)
	{
		return;
	}

	DeltaCurTexts.Reset();
	DeltaNextTexts.Reset();
	DeltaGrid->ClearChildren();

	UFont* NexonRegular = TSoftObjectPtr<UFont>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Font/NEXONLv1GothicRegular_Font.NEXONLv1GothicRegular_Font"))).LoadSynchronous();
	UFont* Pretendard = TSoftObjectPtr<UFont>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Font/Pretendard/F_Pretendard.F_Pretendard"))).LoadSynchronous();

	for (int32 i = 0; i < 6; ++i)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NameText->SetText(UEmployeeStatsHelper::GetStatDisplayName(static_cast<uint8>(i)));
		if (NexonRegular)
		{
			NameText->SetFont(FSlateFontInfo(NexonRegular, 23));
		}
		NameText->SetColorAndOpacity(FSlateColor(SfDeltaInkSub));
		if (UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(NameText))
		{
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NameSlot->SetVerticalAlignment(VAlign_Bottom);
		}

		UTextBlock* CurText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		if (Pretendard)
		{
			CurText->SetFont(FSlateFontInfo(Pretendard, 23, FName(TEXT("SemiBold"))));
		}
		CurText->SetColorAndOpacity(FSlateColor(SfDeltaMute));
		if (UHorizontalBoxSlot* CurSlot = Row->AddChildToHorizontalBox(CurText))
		{
			CurSlot->SetVerticalAlignment(VAlign_Bottom);
		}

		UTextBlock* ArrowText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		ArrowText->SetText(FText::FromString(TEXT("▶")));
		if (NexonRegular)
		{
			ArrowText->SetFont(FSlateFontInfo(NexonRegular, 15));
		}
		ArrowText->SetColorAndOpacity(FSlateColor(SfArrowGold));
		if (UHorizontalBoxSlot* ArrowSlot = Row->AddChildToHorizontalBox(ArrowText))
		{
			ArrowSlot->SetPadding(FMargin(7.f, 0.f, 7.f, 3.f));
			ArrowSlot->SetVerticalAlignment(VAlign_Bottom);
		}

		UTextBlock* NextText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		if (Pretendard)
		{
			NextText->SetFont(FSlateFontInfo(Pretendard, 27, FName(TEXT("SemiBold"))));
		}
		NextText->SetColorAndOpacity(FSlateColor(SfDeltaGain));
		if (UHorizontalBoxSlot* NextSlot = Row->AddChildToHorizontalBox(NextText))
		{
			NextSlot->SetVerticalAlignment(VAlign_Bottom);
		}

		// 좌열 0~3 / 우열 4~7 (enum 순서 유지)
		if (UUniformGridSlot* Cell = DeltaGrid->AddChildToUniformGrid(Row, i % 4, i / 4))
		{
			Cell->SetHorizontalAlignment(HAlign_Fill);
		}

		DeltaCurTexts.Add(CurText);
		DeltaNextTexts.Add(NextText);
	}
}

void UEnhanceStarforceModalWidget::Rebuild()
{
	FEmployeeInstance* Employee = EmployeeManager ? EmployeeManager->GetEmployeeData(EmployeeID) : nullptr;
	const int32 E = Employee ? Employee->EnhancementLevel : 0;
	const bool bMaxed = Employee && E >= UEmployeeManager::MaxEnhancementLevel;
	const bool bCanTry = Employee && !bMaxed;

	EnsureStarImages();
	for (int32 i = 0; i < StarImages.Num(); ++i)
	{
		if (StarImages[i])
		{
			ApplyStarStyle(StarImages[i], i < E);
			// 펄스/연출 잔여값 초기화 — 연속 강화 시 이전 FX 별의 확대·회전이 남는 버그 방지
			StarImages[i]->SetRenderOpacity(1.f);
			StarImages[i]->SetRenderScale(FVector2D(1.f, 1.f));
			StarImages[i]->SetRenderTransformAngle(0.f);
		}
	}

	// 대상 칩 — 누굴 강화하는지 상시 표기
	if (TargetNameText)
	{
		TargetNameText->SetText(Employee ? FText::FromString(Employee->EmployeeName) : FText::GetEmpty());
	}
	if (TargetStarsText)
	{
		TargetStarsText->SetText(FText::FromString(FString::Printf(TEXT("★%d"), E)));
	}

	// 레벨 전환 리드아웃 "N ▶ N+1" — MAX 는 행째 숨김
	if (LevelFromText)
	{
		LevelFromText->SetText(FText::AsNumber(E));
	}
	if (LevelToText)
	{
		LevelToText->SetText(FText::AsNumber(FMath::Min(E + 1, UEmployeeManager::MaxEnhancementLevel)));
	}
	if (UWidget* ReadoutRow = GetWidgetFromName(TEXT("LevelReadoutRow")))
	{
		ReadoutRow->SetVisibility(bMaxed ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	// 배후 글로우는 별도 오버레이라 리드아웃 숨김에 동반
	if (UWidget* LvGlow = GetWidgetFromName(TEXT("LevelGlowImg")))
	{
		LvGlow->SetVisibility(bMaxed ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	// MAX 는 시도 자체가 없음 — 3행 전부 숨김
	float Success = 0.f, Maintain = 0.f, Downgrade = 0.f;
	if (bCanTry && EmployeeManager)
	{
		EmployeeManager->GetEnhanceOdds(E, Success, Maintain, Downgrade);
	}
	SetOddsSegment(OddsSuccessText, OddsSuccessChip, TEXT("성공"), Success);
	SetOddsSegment(OddsMaintainText, OddsMaintainChip, TEXT("유지"), Maintain);
	SetOddsSegment(OddsDowngradeText, OddsDowngradeChip, TEXT("하락"), Downgrade);
	if (UWidget* OddsBar = GetWidgetFromName(TEXT("OddsHBox")))
	{
		OddsBar->SetVisibility(bCanTry ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	// 글로스 밴드는 별도 오버레이라 바 숨김에 같이 따라가야 함
	if (UWidget* OddsGloss = GetWidgetFromName(TEXT("OddsGloss")))
	{
		OddsGloss->SetVisibility(bCanTry ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	const int64 Cost = bCanTry ? UEmployeeManager::GetEnhanceCost(E) : 0;
	if (CostChip)
	{
		CostChip->SetVisibility(bCanTry ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		CostChip->SetValue(Cost);
	}
	if (CostRow)
	{
		CostRow->SetVisibility(bCanTry ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (CostValueText)
	{
		CostValueText->SetText(UGlobalUtilFunctions::FormatExactNumber(Cost));
	}

	if (EnhanceActionButton)
	{
		EnhanceActionButton->SetButtonText(FText::FromString(bMaxed ? TEXT("MAX") : TEXT("강화")));
	}

	// v4: 프리뷰 존 = 능력치 변화 6행 (PreviewChip 컨테이너 + PreviewText 캡션 재용도, MAX 는 존째 숨김)
	if (UWidget* PreviewToggle = PreviewChip ? static_cast<UWidget*>(PreviewChip) : static_cast<UWidget*>(PreviewText))
	{
		PreviewToggle->SetVisibility(bMaxed ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (PreviewText && !bMaxed)
	{
		// ★당 보너스가 밴드제로 바뀌어 고정 상수가 없다 — 다음 ★의 실제 증가폭을 보여준다
		const int32 NextStarDelta = UEmployeeTypeHelper::GetEnhanceStatBonus(E + 1) - UEmployeeTypeHelper::GetEnhanceStatBonus(E);
		PreviewText->SetText(FText::FromString(FString::Printf(TEXT("성공 시 능력치 (전 능력치 +%d)"), NextStarDelta)));
	}

	EnsureDeltaRows();
	if (Employee && !bMaxed && DeltaCurTexts.Num() == 6)
	{
		const int32 CurBonus = UEmployeeTypeHelper::GetEnhanceStatBonus(E);
		const int32 NextBonus = UEmployeeTypeHelper::GetEnhanceStatBonus(E + 1);
		for (int32 i = 0; i < 6; ++i)
		{
			// 직원창 스탯 그리드와 같은 체감 단위 — 한쪽만 포인트면 "52 ▶ 65" 와 "0.50초" 가 같은 화면에서 갈린다
			const int32 Base = UEmployeeStatsHelper::GetStatValueByIndex(Employee->Stats, static_cast<uint8>(i));
			if (DeltaCurTexts[i])
			{
				DeltaCurTexts[i]->SetText(UEmployeeStatsHelper::GetStatFeltText(static_cast<uint8>(i), Base + CurBonus));
			}
			if (DeltaNextTexts[i])
			{
				DeltaNextTexts[i]->SetText(UEmployeeStatsHelper::GetStatFeltText(static_cast<uint8>(i), Base + NextBonus));
			}
		}
	}

	// 결과 직후 리빌드가 성공/하락 리드아웃을 덮지 않도록 — 비어 있을 때만 MAX 채움 (상시 표기, 페이드 없음)
	if (bMaxed && ResultText && ResultText->GetText().IsEmpty())
	{
		SetStamp(FText::FromString(TEXT("MAX")), StampGold);
		ResultFadeElapsed = -1.f;
	}

	RefreshAffordability();
}

void UEnhanceStarforceModalWidget::SetStamp(const FText& InText, const FLinearColor& InColor)
{
	if (ResultText)
	{
		ResultText->SetText(InText);
		ResultText->SetColorAndOpacity(FSlateColor(InColor));
	}
	if (ResultStamp)
	{
		ResultStamp->SetVisibility(InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		FSlateBrush StampBrush = ResultStamp->Background;
		StampBrush.OutlineSettings.Color = FSlateColor(InColor);
		ResultStamp->SetBrush(StampBrush);
		ResultStamp->SetRenderOpacity(1.f);
	}
	if (ResultText)
	{
		ResultText->SetRenderOpacity(1.f);
	}
}

void UEnhanceStarforceModalWidget::RefreshAffordability()
{
	FEmployeeInstance* Employee = EmployeeManager ? EmployeeManager->GetEmployeeData(EmployeeID) : nullptr;
	const bool bCanTry = Employee && Employee->EnhancementLevel < UEmployeeManager::MaxEnhancementLevel;

	bool bCanAfford = false;
	if (bCanTry)
	{
		if (UResourceItemManager* ResourceMgr = GetGameInstance()->GetSubsystem<UResourceItemManager>())
		{
			bCanAfford = ResourceMgr->HasResource(
				EResourceType::Money, UEmployeeManager::GetEnhanceCost(Employee->EnhancementLevel));
		}
	}

	if (EnhanceActionButton)
	{
		EnhanceActionButton->SetIsEnabled(bCanTry && bCanAfford);
	}
	bSheenActive = bCanTry && bCanAfford;

	// 비용 표시 = 미니멀 수치 (부족 시 빨강). 구 CostText 병기 = 중복 표시 버그로 제거
	if (CostChip)
	{
		CostChip->SetCanAfford(!(bCanTry && !bCanAfford));
	}
	if (CostValueText)
	{
		CostValueText->SetColorAndOpacity(FSlateColor(bCanTry && !bCanAfford ? AlertRed : CostInk));
	}
}

void UEnhanceStarforceModalWidget::OnEnhanceClicked()
{
	if (EmployeeID < 0 || !EmployeeManager)
	{
		return;
	}

	// 결과(성공/유지/하락)는 OnEmployeeEnhanced 델리게이트로 수신
	EmployeeManager->EnhanceEmployee(EmployeeID);
}

void UEnhanceStarforceModalWidget::OnCloseDelegate()
{
	// 비활성 오버레이 — DeactivateWidget 은 no-op 이라 직접 제거 (M18 해제는 NativeDestruct)
	RemoveFromParent();
}

void UEnhanceStarforceModalWidget::HandleEmployeeEnhanced(int32 InEmployeeID, UEmployeeManager::EEnhanceResult Result)
{
	if (InEmployeeID != EmployeeID)
	{
		return;
	}

	// 성공/하락은 별이 스토리텔러(내리꽂힘/깨짐) — 스탬프는 별판에 변화가 없는 유지에만
	SetStamp(Result == UEmployeeManager::EEnhanceResult::Maintain
		? FText::FromString(TEXT("유지")) : FText::GetEmpty(), StarInkMute);

	Rebuild();

	// 리빌드(별색 갱신) 후 결과 임팩트 — 새 강화 레벨 기준
	FEmployeeInstance* Employee = EmployeeManager->GetEmployeeData(EmployeeID);
	PlayResultFx(Result, Employee ? Employee->EnhancementLevel : 0);
}

void UEnhanceStarforceModalWidget::HandleResourceChanged(EResourceType Type, int64 NewValue)
{
	if (Type != EResourceType::Money)
	{
		return;
	}

	RefreshAffordability();
}
