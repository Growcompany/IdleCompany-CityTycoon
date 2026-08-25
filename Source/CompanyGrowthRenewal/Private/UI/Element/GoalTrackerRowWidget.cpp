#include "UI/Element/GoalTrackerRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/HorizontalBox.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/RewardChipUtils.h"

namespace
{
	// 미션 카드(UIE_MissionTracker)의 시머 색 — 두 연출이 같은 색으로 읽혀야 한다
	const FLinearColor FxAmber(1.f, 0.723055f, 0.194618f, 1.f);
	const FLinearColor FxGreen = FLinearColor::FromSRGBColor(FColor(0x22, 0xC5, 0x5E));

	float EaseOutCubic(float A)
	{
		const float T = 1.0f - A;
		return 1.0f - T * T * T;
	}
}

void UGoalTrackerRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CacheFxWidgets();
}

void UGoalTrackerRowWidget::CacheFxWidgets()
{
	CardGlint = Cast<UImage>(GetWidgetFromName(TEXT("CardGlint")));
	CardLine = Cast<UImage>(GetWidgetFromName(TEXT("CardLine")));
	CardBgImage = Cast<UImage>(GetWidgetFromName(TEXT("CardBG")));
	SparkleA = Cast<UImage>(GetWidgetFromName(TEXT("SparkleA")));
	SparkleB = Cast<UImage>(GetWidgetFromName(TEXT("SparkleB")));
	SparkleC = Cast<UImage>(GetWidgetFromName(TEXT("SparkleC")));
	for (const TWeakObjectPtr<UImage>& S : { SparkleA, SparkleB, SparkleC })
	{
		if (UImage* W = S.Get())
		{
			W->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			W->SetRenderOpacity(0.f);
		}
	}

	// 브러시는 DT_UIVFXTexture 에서 — 미션 카드와 같은 텍스처를 써야 두 연출이 한 세트로 읽힌다.
	// WBP 는 빈 Image 만 두면 되고, 텍스처 교체는 DT 편집으로 끝난다
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			TableMgr->ApplyUIVFXTexture(TEXT("SparkleMain"), SparkleA.Get());
			TableMgr->ApplyUIVFXTexture(TEXT("SparkleSub"), SparkleB.Get());
			TableMgr->ApplyUIVFXTexture(TEXT("SparkleMain"), SparkleC.Get());
		}
	}
}

void UGoalTrackerRowWidget::Configure(const FGoalBoardEntry& InEntry, bool bInTracked, bool bInExpanded, const FText& InPrereqTitle, const FText& InPlaceHint)
{
	Entry = InEntry;
	bTracked = bInTracked;
	bExpanded = bInExpanded;

	const bool bLocked = (Entry.State == EGoalState::Locked);
	const bool bClaimable = (Entry.State == EGoalState::Claimable);
	const bool bHasProgress = (Entry.ProgressTarget > 0);

	// 재사용 위젯 — 매 Configure 마다 재바인딩 (중복 구독 가드)
	if (RowButton)
	{
		RowButton->OnClicked.RemoveDynamic(this, &UGoalTrackerRowWidget::HandleRowClicked);
		RowButton->OnClicked.AddDynamic(this, &UGoalTrackerRowWidget::HandleRowClicked);
	}
	// UButtonWidget 은 UCommonButtonBase 파생 — 네이티브 OnClicked() 게터 방식(AddDynamic 아님)
	if (ActionButton)
	{
		ActionButton->OnClicked().RemoveAll(this);
		ActionButton->OnClicked().AddUObject(this, &UGoalTrackerRowWidget::HandleActionClicked);
	}
	if (ClaimButton)
	{
		ClaimButton->OnClicked().RemoveAll(this);
		ClaimButton->OnClicked().AddUObject(this, &UGoalTrackerRowWidget::HandleClaimClicked);
	}

	if (TitleText)
	{
		TitleText->SetText(Entry.Row.Title);
	}

	if (ProgressShortText)
	{
		if (bHasProgress && !bLocked)
		{
			ProgressShortText->SetText(FText::FromString(FString::Printf(TEXT("%lld/%lld"), Entry.ProgressCurrent, Entry.ProgressTarget)));
			ProgressShortText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ProgressShortText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	{
		// 칩 색은 상태에 따라 달라지는 값이라 코드 소유 — 모양(코너/패딩)은 WBP 소유.
		// ⚠ 잉크는 반드시 밝게 — StateTagText 폰트에 어두운 아웃라인(0.042 / 80%)이 구워져 있어
		//    어두운 잉크를 쓰면 22px 한글 획이 아웃라인에 먹혀 검은 덩어리로 뭉친다(2026-08-08 "완료" 실사고).
		//    그래서 네 상태 모두 "밝은 잉크 + 반투명 플레이트" 한 가지 처리로 통일한다.
		FText TagLabel;
		FLinearColor TagInk(1.f, 1.f, 1.f, 0.62f);
		FLinearColor TagFill(1.f, 1.f, 1.f, 0.10f);
		if (bTracked && !InPlaceHint.IsEmpty())
		{
			// 추적 중이나 이 맵엔 점등할 대상이 없다 — "안내 중"이라 쓰면 켜졌는데 안 보이는 것으로 읽힌다.
			// 색은 중립 유지(파랑은 실제로 링이 켜졌을 때만) — 사무실에 들어가면 아래 분기로 넘어가며 파랗게 바뀐다
			TagLabel = InPlaceHint;
		}
		else if (bTracked)
		{
			TagLabel = NSLOCTEXT("GoalTracker", "TagTracking", "안내 중");
			TagInk = FLinearColor(0.520946f, 0.745394f, 0.930117f, 1.f);   // #BFE0F7
			TagFill = FLinearColor(0.046650f, 0.327780f, 0.745440f, 0.22f); // #3D9BE0 22%
		}
		else if (bClaimable)
		{
			// 수령 가능은 네 상태 중 가장 강한 신호 — 같은 처리 안에서 채도(28%)로 차이를 낸다
			TagLabel = NSLOCTEXT("GoalTracker", "TagDone", "완료");
			TagInk = FLinearColor(0.274677f, 0.807075f, 0.381326f, 1.f);   // #8FE8A6
			TagFill = FLinearColor(0.056130f, 0.584120f, 0.111910f, 0.28f); // #43C95E 28%
		}
		else if (Entry.State == EGoalState::Claimed)
		{
			// "완료" 는 수령 대기(Claimable)가 쓴다 — 같은 단어가 두 상태를 뜻하지 않게 분리
			TagLabel = NSLOCTEXT("GoalTracker", "TagClaimed", "수령 완료");
		}

		const ESlateVisibility TagVis = TagLabel.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible;
		if (StateTagText)
		{
			StateTagText->SetText(TagLabel);
			StateTagText->SetColorAndOpacity(FSlateColor(TagInk));
			StateTagText->SetVisibility(TagVis);
		}
		// 텍스트만 숨기면 빈 칩이 남는다 — 토글은 플레이트가 소유
		if (StateTagPlate)
		{
			StateTagPlate->SetBrushColor(TagFill);
			StateTagPlate->SetVisibility(TagVis);
		}
	}

	if (LockIcon)
	{
		LockIcon->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (ExpandRoot)
	{
		ExpandRoot->SetVisibility(bExpanded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (DescText)
	{
		// 잠금 행은 사유가 먼저 — 조사를 코드가 붙이지 않도록 대괄호 패턴으로 회피
		DescText->SetText(bLocked
			? FText::Format(NSLOCTEXT("GoalTracker", "LockReason", "[{0}] 완료 후 진행할 수 있습니다"), InPrereqTitle)
			: Entry.Row.Desc);
	}

	if (ProgressBar)
	{
		if (bHasProgress && !bLocked)
		{
			ProgressBar->SetPercent(FMath::Clamp(
				static_cast<float>(Entry.ProgressCurrent) / static_cast<float>(Entry.ProgressTarget), 0.f, 1.f));
			ProgressBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ProgressBar->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (ProgressLongText)
	{
		if (bHasProgress && !bLocked)
		{
			ProgressLongText->SetText(FText::FromString(FString::Printf(TEXT("%lld / %lld"), Entry.ProgressCurrent, Entry.ProgressTarget)));
			ProgressLongText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ProgressLongText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	RebuildRewardChips();

	if (ClaimButton)
	{
		ClaimButton->SetVisibility(bClaimable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (ActionButton)
	{
		// 안내 버튼은 진행 중일 때만 — 잠금·수령가능·수령완료에는 없다.
		// (SetTrackedGoal 이 Locked/Claimed 를 조용히 early-return 하므로 버튼을 남기면 무반응 탭이 된다)
		// 그리고 **안내 데이터가 없는 미션엔 아예 없다** — 눌러도 켜질 게 없으니 버튼이 거짓말이 된다(반복 미션)
		const bool bShowAction = (Entry.State == EGoalState::InProgress) && Entry.Row.StepLines.Num() > 0;
		ActionButton->SetVisibility(bShowAction ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bShowAction)
		{
			ActionButton->SetButtonText(bTracked
				? NSLOCTEXT("GoalTracker", "BtnStopGuide", "안내 끄기")
				: NSLOCTEXT("GoalTracker", "BtnStart", "안내하기"));

			if (UClass* StyleClass = (bTracked ? ActionStyle_Stop : ActionStyle_Start).LoadSynchronous())
			{
				ActionButton->SetStyle(TSubclassOf<UCommonButtonStyle>(StyleClass));
			}
			// 고스트 버튼엔 컬러 CTA 아웃라인이 어울리지 않는다(대비가 없어 얼룩으로 읽힘)
			ActionButton->SetTextOutlineEnabled(!bTracked, 2.f, FLinearColor(0.010965f, 0.078170f, 0.191214f, 1.f));
		}
	}

	// linear 값으로 넣을 것 — sRGB hex 를 그대로 쓰면 물빠진다(카탈로그 규칙)
	FLinearColor StateColor(1.f, 0.723055f, 0.194618f, 0.95f);                        // 진행 중 = 골드(원본 미션 카드와 동일)
	if (bLocked)         { StateColor = FLinearColor(0.309450f, 0.366240f, 0.467770f, 0.55f); }  // #97A3B6
	else if (bClaimable) { StateColor = FLinearColor(0.056130f, 0.584120f, 0.111910f, 1.f); }    // #43C95E
	else if (bTracked)   { StateColor = FLinearColor(0.046650f, 0.327780f, 0.745440f, 1.f); }    // #3D9BE0
	if (AccentBar)
	{
		AccentBar->SetColorAndOpacity(StateColor);
	}
	if (StateDot)
	{
		StateDot->SetColorAndOpacity(StateColor);
		StateDot->SetRenderScale(FVector2D(bLocked ? LockedDotScale : 1.f));
	}

	// ===== 완료 연출 상태 전이 =====
	// 캐시는 NativeConstruct 1회지만, 트래커가 행을 만든 직후 Construct 전에 Configure 가 올 수 있다
	if (!CardGlint.IsValid() && !CardLine.IsValid())
	{
		CacheFxWidgets();
	}
	if (FxGoalID != Entry.GoalID)
	{
		// 풀 재사용으로 다른 미션이 들어왔다 — 이전 미션의 연출을 물려주지 않는다
		StopCompleteGlow();
		FxGoalID = Entry.GoalID;
	}
	else if (FxLastState != EGoalState::Claimable && bClaimable)
	{
		StartCompleteGlow();
	}
	FxLastState = Entry.State;

	bAwaitingClaim = bClaimable;
	if (!bClaimable)
	{
		ClaimPulseTime = 0.f;
		StopCompleteGlow();   // 대기 펄스가 남긴 색/알파를 기본값으로 되돌린다
	}
}

void UGoalTrackerRowWidget::PlayIntro(int32 OrderIndex)
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

void UGoalTrackerRowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bIntroPlaying)
	{
		IntroElapsed += InDeltaTime;
		if (IntroElapsed >= IntroDelay)
		{
			const float T = FMath::Clamp((IntroElapsed - IntroDelay) / IntroDuration, 0.f, 1.f);
			const float E = EaseOutCubic(T);

			FWidgetTransform Xform;
			Xform.Translation = FVector2D(IntroSlideX * (1.f - E), 0.f);
			SetRenderTransform(Xform);
			SetRenderOpacity(E);

			if (T >= 1.f)
			{
				// ⚠ 행은 풀에서 재사용된다 — 알파/트랜스폼을 반드시 원복해야 유령 행이 안 생긴다
				bIntroPlaying = false;
				SetRenderTransform(FWidgetTransform());
				SetRenderOpacity(1.f);
			}
		}
	}

	if (bAwaitingClaim)
	{
		ClaimPulseTime += InDeltaTime;
		const float Pulse = 0.5f + 0.5f * FMath::Sin(ClaimPulseTime * (2.f * PI / 1.1f));

		if (UImage* Line = CardLine.Get())
		{
			FLinearColor C = FMath::Lerp(FxGreen, FxAmber, Pulse);
			C.A = 0.55f + 0.45f * Pulse;
			Line->SetColorAndOpacity(C);
		}
		if (UImage* Bg = CardBgImage.Get())
		{
			Bg->SetColorAndOpacity(FMath::Lerp(FLinearColor::White,
				FLinearColor(0.78f, 1.f, 0.55f, 1.f), 0.18f + 0.18f * Pulse));
		}
		// 글린트는 외곽선과 다른 주기로 — 같은 주기면 둘이 한 덩어리로 보여 스윕이 안 읽힌다
		if (UImage* Glint = CardGlint.Get())
		{
			const float P = FMath::Fmod(ClaimPulseTime, ClaimGlintPeriod) / ClaimGlintPeriod;
			float GA;
			if (P < 0.35f)     { GA = P / 0.35f; }
			else if (P < 0.8f) { GA = 1.f - (P - 0.35f) / 0.45f; }
			else               { GA = 0.f; }
			FLinearColor C = Glint->GetColorAndOpacity();
			C.A = GA;
			Glint->SetColorAndOpacity(C);
		}
	}

	// 리스트 행이라 인스턴스가 10개 — 연출이 없을 때 매 프레임 도는 비용을 여기서 끊는다
	if (!bGlowActive)
	{
		return;
	}

	GlowElapsed += InDeltaTime;
	const float T = GlowElapsed;
	if (T >= GlowDuration)
	{
		StopCompleteGlow();
		return;
	}

	float GlintAlpha;
	if (T <= GlintRiseEnd)
	{
		GlintAlpha = GlintPeak * (T / GlintRiseEnd);
	}
	else
	{
		const float Fall = (T - GlintRiseEnd) / (GlowDuration - GlintRiseEnd);
		GlintAlpha = GlintPeak * (1.f - Fall) * (1.f - Fall);
	}

	float OutlineAlpha;
	if (T <= OutlineRiseEnd)
	{
		OutlineAlpha = FMath::Lerp(OutlineBase, OutlinePeak, T / OutlineRiseEnd);
	}
	else
	{
		OutlineAlpha = FMath::Lerp(OutlinePeak, OutlineBase,
			(T - OutlineRiseEnd) / (GlowDuration - OutlineRiseEnd));
	}

	if (UImage* Glint = CardGlint.Get())
	{
		FLinearColor C = Glint->GetColorAndOpacity();
		C.A = GlintAlpha;
		Glint->SetColorAndOpacity(C);
	}
	if (UImage* Line = CardLine.Get())
	{
		FLinearColor C = Line->GetColorAndOpacity();
		C.A = OutlineAlpha;
		Line->SetColorAndOpacity(C);
	}

	// 스파클: 시차 팝 (스케일 0→1.2→0.9 + 회전, 후반 페이드아웃)
	auto DriveSparkle = [T](const TWeakObjectPtr<UImage>& Img, float StartTime, float BaseAngle)
	{
		UImage* W = Img.Get();
		if (!W)
		{
			return;
		}
		const float L = (T - StartTime) / SparkleLife;
		if (L < 0.f || L > 1.f)
		{
			W->SetRenderOpacity(0.f);
			return;
		}
		const float S = (L < 0.4f) ? FMath::Lerp(0.f, 1.2f, L / 0.4f)
		                           : FMath::Lerp(1.2f, 0.9f, (L - 0.4f) / 0.6f);
		W->SetRenderScale(FVector2D(S, S));
		W->SetRenderTransformAngle(BaseAngle + L * 90.f);
		W->SetRenderOpacity(FMath::Min(L * 5.f, 1.f) * (1.f - FMath::Max(0.f, (L - 0.7f) / 0.3f)));
	};
	DriveSparkle(SparkleA, 0.f, 0.f);
	DriveSparkle(SparkleB, SparkleStagger, 30.f);
	DriveSparkle(SparkleC, SparkleStagger * 2.f, -20.f);
}

void UGoalTrackerRowWidget::StartCompleteGlow()
{
	// 이름 계약을 하나도 못 찾으면 연출할 게 없다 — 틱도 켜지 않는다
	if (!CardGlint.Get() && !CardLine.Get() && !SparkleA.Get() && !SparkleB.Get() && !SparkleC.Get())
	{
		return;
	}
	bGlowActive = true;
	GlowElapsed = 0.f;

	// 첫 틱 전 한 프레임 깜빡임 방지
	if (UImage* Glint = CardGlint.Get())
	{
		FLinearColor C = Glint->GetColorAndOpacity();
		C.A = 0.f;
		Glint->SetColorAndOpacity(C);
	}
	if (UImage* Line = CardLine.Get())
	{
		FLinearColor C = Line->GetColorAndOpacity();
		C.A = OutlineBase;
		Line->SetColorAndOpacity(C);
	}
}

void UGoalTrackerRowWidget::StopCompleteGlow()
{
	bGlowActive = false;
	GlowElapsed = 0.f;

	if (UImage* Glint = CardGlint.Get())
	{
		FLinearColor C = Glint->GetColorAndOpacity();
		C.A = 0.f;
		Glint->SetColorAndOpacity(C);
	}
	if (UImage* Line = CardLine.Get())
	{
		// 색까지 흰색으로 — 대기 펄스가 초록/앰버를 칠해 놓고 갈 수 있다
		Line->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, OutlineBase));
	}
	if (UImage* Bg = CardBgImage.Get())
	{
		Bg->SetColorAndOpacity(FLinearColor::White);
	}
	for (const TWeakObjectPtr<UImage>& S : { SparkleA, SparkleB, SparkleC })
	{
		if (UImage* W = S.Get())
		{
			W->SetRenderOpacity(0.f);
		}
	}
}

void UGoalTrackerRowWidget::NativeDestruct()
{
	if (RowButton)
	{
		RowButton->OnClicked.RemoveDynamic(this, &UGoalTrackerRowWidget::HandleRowClicked);
	}
	if (ActionButton)
	{
		ActionButton->OnClicked().RemoveAll(this);
	}
	if (ClaimButton)
	{
		ClaimButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UGoalTrackerRowWidget::HandleRowClicked()
{
	OnRowTapped.Broadcast(Entry.GoalID);
}

void UGoalTrackerRowWidget::HandleActionClicked()
{
	if (bTracked)
	{
		OnStopRequested.Broadcast(Entry.GoalID);
	}
	else
	{
		OnStartRequested.Broadcast(Entry.GoalID);
	}
}

void UGoalTrackerRowWidget::HandleClaimClicked()
{
	OnClaimRequested.Broadcast(Entry.GoalID);
}

void UGoalTrackerRowWidget::RebuildRewardChips()
{
	if (!RewardBox || !WidgetTree)
	{
		return;
	}
	const bool bAny = CGRewardChip::RenderIcons(*WidgetTree, GetGameInstance(), *RewardBox, Entry.Row.Rewards);
	RewardBox->SetVisibility(bAny ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
