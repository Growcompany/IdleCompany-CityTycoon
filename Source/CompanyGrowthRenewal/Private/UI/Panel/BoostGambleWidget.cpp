// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Panel/BoostGambleWidget.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/CompanyInfoTable.h"
#include "Enum/CompanyType.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "CommonTextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/HorizontalBoxSlot.h"

namespace
{
	// 산업 매핑 실패 시 글로우/도트 폴백 (glare 없는 스틸 톤)
	const FLinearColor SigNeutral(0.4200f, 0.4700f, 0.5600f, 1.0f);

	int32 PctOf(float Frac) { return FMath::RoundToInt(Frac * 100.0f); }

	// 카운트다운 바/숫자 색 (WBP baked 값과 동기 — 긴박 시 여기서 red 로 보간)
	const FLinearColor AmberBar(0.9400f, 0.7000f, 0.1600f, 1.0f);
	const FLinearColor AmberText(0.7600f, 0.3300f, 0.0300f, 1.0f);
	const FLinearColor UrgentBar(0.9000f, 0.1300f, 0.1000f, 1.0f);
	const FLinearColor UrgentText(0.9200f, 0.1500f, 0.1000f, 1.0f);

	// 결과 플래시 틴트 (linear)
	const FLinearColor FlashWin(0.0700f, 0.5600f, 0.1600f, 1.0f);
	const FLinearColor FlashLose(0.7000f, 0.0700f, 0.0500f, 1.0f);
}

void UBoostGambleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RemainingTime = CountdownSeconds;
	bResolved = false;
	bTearingDown = false;
	bAwaitingResult = false;
	bResultKnown = false;

	// 스케일/이동 펄스가 카드 중심 기준으로 커지도록 피벗 고정 (등장 슬라이드·긴박 펄스·소멸 축소 공용)
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

	// 산업별 시나리오 주입 — 매 오픈 재바인딩(재사용 위젯 규칙). 제목/문구/버튼/리드아웃 전부 DT_BoostGamble 주도.
	UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	StageMgrWeak = StageMgr;
	if (StageMgr)
	{
		const FBoostGambleRow& Scenario = StageMgr->GetCurrentBoostGamble();
		FillReadout(Scenario);

		if (GambleButton && !Scenario.GoLabel.IsEmpty()) { GambleButton->SetButtonText(Scenario.GoLabel); }
		if (SafeButton && !Scenario.SafeLabel.IsEmpty()) { SafeButton->SetButtonText(Scenario.SafeLabel); }

		// 야근수당 등 확정 비용 여력에 따라 지른다 버튼 상태 갱신 — 재사용 위젯이라 양방향(활성/비활성) 매 오픈 세팅 필수
		const bool bCanAffordGamble = StageMgr->CanAffordCurrentBoostGamble();
		if (GambleButton)
		{
			GambleButton->SetIsEnabled(bCanAffordGamble);
		}
		if (!bCanAffordGamble && CostText)
		{
			CostText->SetText(FText::FromString(
				FString::Printf(TEXT("비용 %d 부족 ― 지금은 거절만."), Scenario.GoCost)));
		}

		// 결과 델리게이트 구독 — ResolveBoostGamble 이 동기 브로드캐스트. 쌍 해제는 NativeDestruct.
		StageMgr->OnBoostGambleResolved.RemoveDynamic(this, &UBoostGambleWidget::HandleBoostGambleResolved);
		StageMgr->OnBoostGambleResolved.AddDynamic(this, &UBoostGambleWidget::HandleBoostGambleResolved);
	}

	if (GambleButton)
	{
		GambleButton->OnClicked().AddUObject(this, &UBoostGambleWidget::OnGambleClicked);
	}
	if (SafeButton)
	{
		SafeButton->OnClicked().AddUObject(this, &UBoostGambleWidget::OnSafeClicked);
	}
	if (CountdownBar)
	{
		CountdownBar->SetPercent(1.0f);
	}
	if (CountdownText)
	{
		CountdownText->SetText(FText::AsNumber(FMath::CeilToInt(RemainingTime)));
	}

	// 결과 오버레이는 평시 숨김 (BindWidgetOptional — 있을 때만)
	if (ResultFlash)
	{
		ResultFlash->SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	}
	if (ResultText)
	{
		ResultText->SetRenderOpacity(0.0f);
	}

	// 등장 연출 시작 — 아래에서 살짝 올라오며 페이드인
	SetWidgetAlpha(0.0f);
	SetCardTranslation(0.0f, IntroRiseY);
	SetCardScale(1.0f);
	EnterState(EBoostCardAnim::Intro);
}

void UBoostGambleWidget::FillReadout(const FBoostGambleRow& Row)
{
	if (PromptText && !Row.Title.IsEmpty()) { PromptText->SetText(Row.Title); }

	// --- 공통: 산업 표시명 + 시그니처색 (DT_CompanyInfo 주도, 실패 시 뉴트럴) ---
	FText IndName = FText::FromString(Row.Industry);
	FLinearColor SigColor = SigNeutral;
	const ECompanyType Industry = StringToCompanyType(Row.Industry);
	if (Industry != ECompanyType::None)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
			{
				bool bOk = false;
				const FCompanyInfoTable Info = TableMgr->GetCompanyInfo(Industry, bOk);
				if (bOk)
				{
					if (!Info.DisplayName.IsEmpty()) { IndName = Info.DisplayName; }
					SigColor = Info.AccentColor; // 데이터 존중 — DT 값 그대로
				}
			}
		}
	}
	SigColor.A = 1.0f;
	if (IndustryChipText) { IndustryChipText->SetText(IndName); }
	// 트리에서 AccentGlow=Glow_Oval(투명배경) 브러시라 이 틴트가 상단 소프트 글로우가 됨.
	if (AccentGlow)  { AccentGlow->SetColorAndOpacity(SigColor); }
	if (IndustryDot) { IndustryDot->SetColorAndOpacity(SigColor); }

	// --- 오즈 바: 성공/실패 비율 세그 (폭=확률) + 세그 확률 텍스트 ---
	const float SuccChance = FMath::Clamp(Row.SuccessChance, 0.0f, 1.0f);
	const int32 SuccPct = PctOf(SuccChance);
	if (SuccessChanceText) { SuccessChanceText->SetText(FText::FromString(FString::Printf(TEXT("성공 %d%%"), SuccPct))); }
	if (FailChanceText)    { FailChanceText->SetText(FText::FromString(FString::Printf(TEXT("실패 %d%%"), 100 - SuccPct))); }
	SetOddsSegWidth(OddsSuccessSeg, SuccChance);
	SetOddsSegWidth(OddsFailSeg, 1.0f - SuccChance);

	// --- 결과 금액: 효과별 공유 단위 분리 (겹치면 헤더로 빼고 값은 짧게, 안 겹치면 값에 명사 유지) ---
	const int32 SPct = PctOf(Row.SuccessFrac);
	const int32 FPct = PctOf(Row.FailFrac);
	const int32 Secs = FMath::RoundToInt(Row.TimeSeconds);

	FString Unit, WinVal, LoseVal;
	switch (Row.EffectType)
	{
	case EDevEventEffect::TimeCut:
		Unit = FString();  // 성공=출시 / 실패=점수 → 공유 명사 없음
		WinVal  = FString::Printf(TEXT("출시 -%d초"), Secs);
		LoseVal = FString::Printf(TEXT("전체 점수 -%d%%"), FPct);
		break;

	case EDevEventEffect::PayoffGamble:
		Unit = TEXT("수익");
		WinVal  = FString::Printf(TEXT("×%.2f"), 1.0f + Row.SuccessFrac);
		LoseVal = FString::Printf(TEXT("×%.2f"), 1.0f - Row.FailFrac);
		break;

	case EDevEventEffect::ScoreSwing:
	case EDevEventEffect::TimeExtend:
	default:
		Unit = TEXT("전체 점수");
		WinVal  = FString::Printf(TEXT("+%d%%"), SPct);
		LoseVal = FString::Printf(TEXT("-%d%%"), FPct);
		break;
	}
	if (WinValueText)  { WinValueText->SetText(FText::FromString(WinVal)); }
	if (LoseValueText) { LoseValueText->SetText(FText::FromString(LoseVal)); }
	if (OutcomeUnitText)
	{
		OutcomeUnitText->SetText(FText::FromString(Unit));
		OutcomeUnitText->SetVisibility(Unit.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	// --- 비용 줄: TimeExtend(GoCost>0)만 야근수당, 그 외 빈 줄 ---
	FString Cost;
	if (Row.EffectType == EDevEventEffect::TimeExtend && Row.GoCost > 0)
	{
		Cost = FString::Printf(TEXT("비용 -%d · 개발 +%d초"), Row.GoCost, Secs);
	}
	if (CostText) { CostText->SetText(FText::FromString(Cost)); }
}

void UBoostGambleWidget::SetOddsSegWidth(UWidget* Seg, float Fill)
{
	if (!Seg) { return; }
	if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Seg->Slot))
	{
		FSlateChildSize Size;
		Size.SizeRule = ESlateSizeRule::Fill;
		Size.Value = FMath::Max(0.0001f, Fill);  // 0 폭 세그(텍스트 클리핑) 방지
		HSlot->SetSize(Size);
	}
}

void UBoostGambleWidget::NativeDestruct()
{
	// 레일 트림/오피스 이탈로 카드가 먼저 사라지는 경로 대비 — 미해결로 사라지면 매니저의
	// bBoostGamblePending 이 남아 개발 타이머가 영구 정지한다. 타임아웃과 동일하게 안전으로 정산.
	// bTearingDown 을 먼저 세워 Resolve 가 연출(플래시/소멸)로 진입하지 않고 정산만 하게 한다(위젯 소멸 중).
	bTearingDown = true;
	if (!bResolved)
	{
		Resolve(false);
	}

	if (UOfficeStageProgressManager* StageMgr = StageMgrWeak.Get())
	{
		StageMgr->OnBoostGambleResolved.RemoveDynamic(this, &UBoostGambleWidget::HandleBoostGambleResolved);
	}
	if (GambleButton)
	{
		GambleButton->OnClicked().RemoveAll(this);
	}
	if (SafeButton)
	{
		SafeButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UBoostGambleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	AnimElapsed += InDeltaTime;

	switch (AnimState)
	{
	case EBoostCardAnim::Intro:
		TickIntro(InDeltaTime);
		break;

	case EBoostCardAnim::Countdown:
	case EBoostCardAnim::Urgent:
		TickCountdown(InDeltaTime);
		break;

	case EBoostCardAnim::Result:
		TickResult(InDeltaTime);
		break;

	case EBoostCardAnim::Dismiss:
		TickDismiss(InDeltaTime);
		break;

	default:
		break;
	}
}

void UBoostGambleWidget::EnterState(EBoostCardAnim NewState)
{
	AnimState = NewState;
	AnimElapsed = 0.0f;
}

void UBoostGambleWidget::TickIntro(float DeltaTime)
{
	const float T = FMath::Clamp(AnimElapsed / IntroSeconds, 0.0f, 1.0f);
	const float E = EaseOutQuad(T);

	SetCardTranslation(0.0f, FMath::Lerp(IntroRiseY, 0.0f, E));
	SetWidgetAlpha(E);

	if (T >= 1.0f)
	{
		SetCardTranslation(0.0f, 0.0f);
		SetWidgetAlpha(1.0f);
		EnterState(EBoostCardAnim::Countdown);
	}
}

void UBoostGambleWidget::TickCountdown(float DeltaTime)
{
	// 기존 카운트다운 로직 그대로 — 감소/바/숫자/타임아웃.
	RemainingTime -= DeltaTime;

	const float Ratio = FMath::Clamp(RemainingTime / CountdownSeconds, 0.0f, 1.0f);
	if (CountdownBar)
	{
		CountdownBar->SetPercent(Ratio);
	}
	if (CountdownText)
	{
		CountdownText->SetText(FText::AsNumber(FMath::Max(0, FMath::CeilToInt(RemainingTime))));
	}

	if (RemainingTime <= 0.0f)
	{
		Resolve(false); // 타임아웃 = 안전
		return;
	}

	// 대기 → 긴박 전환 (마지막 구간)
	if (AnimState == EBoostCardAnim::Countdown && RemainingTime <= UrgentThreshold)
	{
		EnterState(EBoostCardAnim::Urgent);
	}

	// 긴박이면 카운트다운 위에 붉은 펄스를 얹는다 (기존 로직은 그대로 두고 오버레이만)
	if (AnimState == EBoostCardAnim::Urgent)
	{
		ApplyUrgentPulse();
	}
}

void UBoostGambleWidget::ApplyUrgentPulse()
{
	const float Pulse = 0.5f + 0.5f * FMath::Sin(AnimElapsed * UrgentPulseHz * 2.0f * PI); // 0..1

	// 카드 미세 스케일 — "빨리!" 신호만 (1.0 ↔ 1.015)
	SetCardScale(1.0f + UrgentScaleAmp * Pulse);

	// 앰버 → 레드 (남은시간 깊이) + 펄스 밝기 스로브
	const float DepthT = FMath::Clamp(1.0f - RemainingTime / UrgentThreshold, 0.0f, 1.0f);
	const float Bright = 0.82f + 0.18f * Pulse;

	if (CountdownBar)
	{
		FLinearColor BarCol = FMath::Lerp(AmberBar, UrgentBar, DepthT);
		BarCol.R *= Bright; BarCol.G *= Bright; BarCol.B *= Bright; BarCol.A = 1.0f;
		CountdownBar->SetFillColorAndOpacity(BarCol);
	}
	if (CountdownText)
	{
		FLinearColor TxtCol = FMath::Lerp(AmberText, UrgentText, DepthT);
		TxtCol.R *= Bright; TxtCol.G *= Bright; TxtCol.B *= Bright; TxtCol.A = 1.0f;
		CountdownText->SetColorAndOpacity(FSlateColor(TxtCol));
	}
}

void UBoostGambleWidget::TickResult(float DeltaTime)
{
	const float T = FMath::Clamp(AnimElapsed / ResultFlashSeconds, 0.0f, 1.0f);

	// 플래시 알파 — 빠르게 팝 → 천천히 낮은 유지값으로 감쇠 (소멸 애니가 카드째 페이드해 마무리)
	const float Peak = bResultWin ? 0.50f : 0.55f;
	const float FlashA = (T < 0.28f)
		? (T / 0.28f) * Peak
		: FMath::Lerp(Peak, 0.12f, (T - 0.28f) / 0.72f);

	if (ResultFlash)
	{
		FLinearColor C = bResultWin ? FlashWin : FlashLose;
		C.A = FlashA;
		ResultFlash->SetColorAndOpacity(C);
	}
	if (ResultText)
	{
		ResultText->SetRenderOpacity(FMath::Min(1.0f, T / 0.18f));
	}

	// 실패면 좌우 흔들림 (감쇠). 성공은 정중앙 유지.
	float ShakeX = 0.0f;
	if (!bResultWin)
	{
		ShakeX = FMath::Sin(AnimElapsed * ShakeHz * 2.0f * PI) * ShakeAmpPx * (1.0f - T);
	}
	SetCardTranslation(ShakeX, 0.0f);

	if (T >= 1.0f)
	{
		SetCardTranslation(0.0f, 0.0f);
		EnterState(EBoostCardAnim::Dismiss);
	}
}

void UBoostGambleWidget::TickDismiss(float DeltaTime)
{
	const float T = FMath::Clamp(AnimElapsed / DismissSeconds, 0.0f, 1.0f);
	const float E = EaseInQuad(T);

	SetCardScale(FMath::Lerp(1.0f, DismissEndScale, E));
	SetWidgetAlpha(1.0f - E);

	if (T >= 1.0f)
	{
		RequestRemoval();
	}
}

void UBoostGambleWidget::RequestRemoval()
{
	// 레일 엔트리라 스택이 없다 — DeactivateWidget 은 아무것도 닫지 못하므로 소유 레일에 제거를 요청.
	// 레일이 이미 사라진 경우(오피스 이탈)엔 ExecuteIfBound 가 false → 직접 떼어낸다.
	if (!OnRailRemoveRequested.ExecuteIfBound())
	{
		RemoveFromParent();
	}
}

void UBoostGambleWidget::HandleBoostGambleResolved(bool bGambled, bool bSuccess)
{
	// 우리 Resolve 가 트리거한 브로드캐스트만 수용 (다른 소스의 stray 방지)
	if (!bAwaitingResult)
	{
		return;
	}
	bResultKnown = true;
	bLastGambled = bGambled;
	bLastSuccess = bSuccess;
}

void UBoostGambleWidget::OnGambleClicked()
{
	Resolve(true);
}

void UBoostGambleWidget::OnSafeClicked()
{
	Resolve(false);
}

void UBoostGambleWidget::Resolve(bool bGamble)
{
	if (bResolved)
	{
		return; // 버튼 클릭 · 타임아웃 · 더블리졸브 경합 방지
	}
	bResolved = true; // 이후 입력/타이머/재정산 전부 차단

	// 매니저 정산 — ResolveBoostGamble 이 동기적으로 OnBoostGambleResolved 를 쏘므로,
	// 이 호출이 끝나면 HandleBoostGambleResolved 가 결과를 채워 둔 상태다.
	bResultKnown = false;
	bAwaitingResult = true;
	if (UOfficeStageProgressManager* StageMgr = StageMgrWeak.Get())
	{
		StageMgr->ResolveBoostGamble(bGamble);
	}
	bAwaitingResult = false;

	// 소멸 중(NativeDestruct 안전정산)이면 연출 없이 정산만 하고 끝 — 더 틱이 없으므로 상태머신 진입은 무의미.
	if (bTearingDown)
	{
		return;
	}

	// 지른 결과만 결과 플래시. 안전/여력부족(매니저가 false,false 로 브로드캐스트)/미확정 = 조용히 소멸.
	if (bResultKnown && bLastGambled)
	{
		bResultWin = bLastSuccess;

		// 결과 진입 초기화 — 스케일/이동/알파 원복 + 결과 라벨 세팅
		SetCardScale(1.0f);
		SetWidgetAlpha(1.0f);
		SetCardTranslation(0.0f, 0.0f);
		if (ResultText)
		{
			ResultText->SetText(bResultWin ? FText::FromString(TEXT("성공!")) : FText::FromString(TEXT("실패…")));
			ResultText->SetRenderOpacity(0.0f);
		}
		EnterState(EBoostCardAnim::Result);
	}
	else
	{
		EnterState(EBoostCardAnim::Dismiss);
	}
}

void UBoostGambleWidget::SetWidgetAlpha(float Alpha)
{
	SetRenderOpacity(Alpha);
}

void UBoostGambleWidget::SetCardTranslation(float X, float Y)
{
	SetRenderTranslation(FVector2D(X, Y));
}

void UBoostGambleWidget::SetCardScale(float Scale)
{
	SetRenderScale(FVector2D(Scale, Scale));
}
