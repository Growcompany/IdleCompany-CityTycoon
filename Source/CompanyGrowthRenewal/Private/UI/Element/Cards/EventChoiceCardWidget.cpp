#include "UI/Element/Cards/EventChoiceCardWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Entity/Officeworker/Officeworker.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Data/StageProgressData.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Engine/World.h"

void UEventChoiceCardWidget::SetChoiceData(const FProjectEventChoice& Choice, int32 InChoiceIndex)
{
	ChoiceIndex = InChoiceIndex;

	if (TitleText)
	{
		TitleText->SetText(Choice.ChoiceText);
	}

	// 선택지 성격(Tone) → 메달 글로우 색
	if (MedallionGlow)
	{
		MedallionGlow->SetColorAndOpacity(GetToneColor(Choice));
	}

	// 효과: 카테고리 행(아이콘+값) 우선, 행 미바인딩(구 WBP)이면 EffectDescription 폴백
	const bool bHasRows = (ScoreRow || TimeRow || PauseRow || BuffRow || RandomRow);
	if (bHasRows)
	{
		FString S, T, P, B, R;
		BuildCategoryTexts(Choice, GetCurrentWorkerCount(), S, T, P, B, R);
		ApplyRow(ScoreRow, ScoreText, S);
		ApplyRow(TimeRow, TimeText, T);
		ApplyRow(PauseRow, PauseText, P);
		ApplyRow(BuffRow, BuffText, B);
		ApplyRow(RandomRow, RandomText, R);

		// 보상 배율(예: "보상 -20%")은 전용 행이 없어 EffectDescription 으로 노출.
		// 행이 하나라도 있으면 EffectDescription 숨김. 행도 보상도 없을 때만 "변화 없음".
		const bool bAnyRow = !(S.IsEmpty() && T.IsEmpty() && P.IsEmpty() && B.IsEmpty() && R.IsEmpty());
		const FString RewardText = BuildRewardText(Choice, GetActiveProjectMode(GetWorld()));
		if (EffectDescription)
		{
			if (!bAnyRow && !RewardText.IsEmpty())
			{
				EffectDescription->SetText(FText::FromString(RewardText));
				EffectDescription->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				EffectDescription->SetText(FText::FromString(TEXT("변화 없음")));
				EffectDescription->SetVisibility(bAnyRow ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
			}
		}
	}
	else if (EffectDescription)
	{
		EffectDescription->SetText(FText::FromString(BuildEffectText(Choice)));
	}

	if (IconImage)
	{
		// 행동 아키타입 → DT_EventChoiceIcon 에서 아이콘 조회. 없으면 아이콘 영역 숨김.
		UTexture2D* Icon = nullptr;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
			{
				Icon = TableMgr->GetEventChoiceIcon(Choice.Archetype);
			}
		}
		if (Icon)
		{
			IconImage->SetBrushFromTexture(Icon);
			IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			IconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UEventChoiceCardWidget::NativeOnClicked()
{
	Super::NativeOnClicked();
	OnCardSelected.Broadcast(ChoiceIndex);
}

FString UEventChoiceCardWidget::BuildEffectText(const FProjectEventChoice& Choice) const
{
	TArray<FString> Sections;

	// === 1. 점수 효과 (3개 카테고리) ===
	{
		const float P = Choice.PlanningScoreBonus;
		const float D = Choice.DevScoreBonus;
		const float Q = Choice.QAScoreBonus;
		const bool bAnyScore = !FMath::IsNearlyZero(P) || !FMath::IsNearlyZero(D) || !FMath::IsNearlyZero(Q);

		if (bAnyScore)
		{
			// 3개 모두 같은 값이면 "전체 +X"로 압축
			if (!FMath::IsNearlyZero(P) && FMath::IsNearlyEqual(P, D) && FMath::IsNearlyEqual(D, Q))
			{
				Sections.Add(FString::Printf(TEXT("[점수] 전체 %+d"), int32(P)));
			}
			else
			{
				TArray<FString> ScoreLines;
				if (!FMath::IsNearlyZero(P)) ScoreLines.Add(FString::Printf(TEXT("기획 %+d"), int32(P)));
				if (!FMath::IsNearlyZero(D)) ScoreLines.Add(FString::Printf(TEXT("개발 %+d"), int32(D)));
				if (!FMath::IsNearlyZero(Q)) ScoreLines.Add(FString::Printf(TEXT("QA %+d"), int32(Q)));
				Sections.Add(FString(TEXT("[점수] ")) + FString::Join(ScoreLines, TEXT("  ")));
			}
		}
	}

	// === 2. 랜덤 카테고리 페널티 ===
	if (Choice.bRandomCategoryPenalty && !FMath::IsNearlyZero(Choice.RandomPenaltyAmount))
	{
		Sections.Add(FString::Printf(TEXT("[랜덤] 카테고리 %+d"), int32(Choice.RandomPenaltyAmount)));
	}

	// === 3. 시간 변화 ===
	if (!FMath::IsNearlyZero(Choice.TimerAdjustment))
	{
		Sections.Add(FString::Printf(TEXT("[시간] %+d초"), int32(Choice.TimerAdjustment)));
	}

	// === 4. 일시정지 ===
	if (Choice.PauseDuration > 0.0f)
	{
		Sections.Add(FString::Printf(TEXT("[정지] %d초"), int32(Choice.PauseDuration)));
	}

	// === 5. 직원 버프 ===
	if (Choice.BuffType != EBuffType::None && Choice.BuffDuration > 0.0f && Choice.BuffTargetCount > 0)
	{
		const int32 ActualWorkers = GetCurrentWorkerCount();
		const int32 EffectiveCount = FMath::Min(Choice.BuffTargetCount, FMath::Max(ActualWorkers, 1));

		FString WorkerText;
		if (ActualWorkers <= 0)
		{
			WorkerText = TEXT("(직원 없음)");
		}
		else if (EffectiveCount >= ActualWorkers)
		{
			WorkerText = TEXT("전 직원");
		}
		else
		{
			WorkerText = FString::Printf(TEXT("랜덤 %d명"), EffectiveCount);
		}

		const TCHAR* BuffName;
		FString ValueText;
		switch (Choice.BuffType)
		{
		case EBuffType::ScoreMultiplier:
			BuffName = TEXT("점수");
			ValueText = FString::Printf(TEXT("×%.1f"), Choice.BuffValue);
			break;
		case EBuffType::CritChance:
			BuffName = TEXT("크리율");
			ValueText = FString::Printf(TEXT("+%d%%"), int32(Choice.BuffValue * 100.0f));
			break;
		case EBuffType::WorkSpeed:
			BuffName = TEXT("작업속도");
			// WorkSpeed는 작은 값이 빠름 — 0.7 → 1.43배 빠르게
			ValueText = (Choice.BuffValue < 1.0f && Choice.BuffValue > 0.0f)
				? FString::Printf(TEXT("×%.1f"), 1.0f / Choice.BuffValue)
				: FString::Printf(TEXT("×%.1f"), Choice.BuffValue);
			break;
		default:
			BuffName = TEXT("?");
			ValueText = FString::Printf(TEXT("×%.1f"), Choice.BuffValue);
			break;
		}

		Sections.Add(FString::Printf(TEXT("[버프] %s %s%s (%d초)"),
			*WorkerText, BuffName, *ValueText, int32(Choice.BuffDuration)));
	}

	// === 6. 폴백 ===
	if (Sections.Num() == 0)
	{
		return TEXT("변화 없음");
	}

	return FString::Join(Sections, TEXT("\n"));
}

int32 UEventChoiceCardWidget::GetCurrentWorkerCount() const
{
	return CountWorkersInWorld(GetWorld());
}

int32 UEventChoiceCardWidget::CountWorkersInWorld(const UWorld* World)
{
	if (!World) return 0;
	int32 Count = 0;
	for (TActorIterator<AOfficeworker> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (*It) ++Count;
	}
	return Count;
}

EProjectMode UEventChoiceCardWidget::GetActiveProjectMode(const UWorld* World)
{
	if (World)
	{
		if (UOfficeStageProgressManager* SM = World->GetSubsystem<UOfficeStageProgressManager>())
		{
			return SM->GetProgressData().ActiveMode;
		}
	}
	return EProjectMode::None;
}

FString UEventChoiceCardWidget::BuildRewardText(const FProjectEventChoice& Choice, EProjectMode Mode)
{
	if (FMath::IsNearlyEqual(Choice.RewardMultiplier, 1.0f)) return FString();
	const int32 Pct = FMath::RoundToInt((Choice.RewardMultiplier - 1.0f) * 100.0f);
	// 수주는 계약금 일회성, 자체개발은 운영수익 — 의미가 달라 라벨로 구분
	const TCHAR* Label =
		(Mode == EProjectMode::InHouse)      ? TEXT("운영수익") :
		(Mode == EProjectMode::Commissioned) ? TEXT("계약금")   :
		                                       TEXT("보상");
	return FString::Printf(TEXT("%s %+d%%"), Label, Pct);
}

FString UEventChoiceCardWidget::BuildEffectSummaryText(const FProjectEventChoice& Choice, int32 WorkerCount, EProjectMode Mode)
{
	FString S, T, P, B, R;
	BuildCategoryTexts(Choice, WorkerCount, S, T, P, B, R);

	TArray<FString> Lines;
	if (!S.IsEmpty()) Lines.Add(S);
	if (!T.IsEmpty()) Lines.Add(T);
	if (!P.IsEmpty()) Lines.Add(P);
	if (!B.IsEmpty()) Lines.Add(B);
	if (!R.IsEmpty()) Lines.Add(R);
	const FString RewardText = BuildRewardText(Choice, Mode);
	if (!RewardText.IsEmpty()) Lines.Add(RewardText);

	return FString::Join(Lines, TEXT("\n"));
}

void UEventChoiceCardWidget::ApplyRow(UHorizontalBox* Row, UCommonTextBlock* Text, const FString& Value)
{
	const bool bShow = !Value.IsEmpty();
	if (Text && bShow)
	{
		Text->SetText(FText::FromString(Value));
	}
	if (Row)
	{
		Row->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

FLinearColor UEventChoiceCardWidget::GetToneColor(const FProjectEventChoice& Choice) const
{
	const float Pl = Choice.PlanningScoreBonus;
	const float D = Choice.DevScoreBonus;
	const float Q = Choice.QAScoreBonus;
	const bool bGain = (Pl > 0.f || D > 0.f || Q > 0.f || Choice.TimerAdjustment < 0.f);
	const bool bPenalty = (Choice.bRandomCategoryPenalty || Pl < 0.f || D < 0.f || Q < 0.f || Choice.TimerAdjustment > 0.f);

	if (Choice.BuffType != EBuffType::None)
	{
		return FLinearColor(0.13f, 0.83f, 0.93f, 0.32f);   // 버프 = 틸
	}
	if (bPenalty && !bGain)
	{
		return FLinearColor(0.94f, 0.27f, 0.27f, 0.32f);   // 위험 = 레드
	}
	if (bGain)
	{
		return FLinearColor(0.35f, 0.80f, 0.45f, 0.32f);   // 긍정 = 그린
	}
	return FLinearColor(0.96f, 0.86f, 0.55f, 0.28f);       // 중립 = 웜 골드
}

void UEventChoiceCardWidget::BuildCategoryTexts(const FProjectEventChoice& Choice, int32 WorkerCount,
	FString& OutScore, FString& OutTime, FString& OutPause, FString& OutBuff, FString& OutRandom)
{
	OutScore.Empty(); OutTime.Empty(); OutPause.Empty(); OutBuff.Empty(); OutRandom.Empty();

	// 점수 (3 카테고리)
	{
		const float P = Choice.PlanningScoreBonus;
		const float D = Choice.DevScoreBonus;
		const float Q = Choice.QAScoreBonus;
		const bool bAny = !FMath::IsNearlyZero(P) || !FMath::IsNearlyZero(D) || !FMath::IsNearlyZero(Q);
		if (bAny)
		{
			if (!FMath::IsNearlyZero(P) && FMath::IsNearlyEqual(P, D) && FMath::IsNearlyEqual(D, Q))
			{
				OutScore = FString::Printf(TEXT("전체 %+d"), int32(P));
			}
			else
			{
				TArray<FString> Lines;
				if (!FMath::IsNearlyZero(P)) Lines.Add(FString::Printf(TEXT("기획 %+d"), int32(P)));
				if (!FMath::IsNearlyZero(D)) Lines.Add(FString::Printf(TEXT("개발 %+d"), int32(D)));
				if (!FMath::IsNearlyZero(Q)) Lines.Add(FString::Printf(TEXT("QA %+d"), int32(Q)));
				OutScore = FString::Join(Lines, TEXT("  "));
			}
		}
	}

	// 랜덤 카테고리 페널티
	if (Choice.bRandomCategoryPenalty && !FMath::IsNearlyZero(Choice.RandomPenaltyAmount))
	{
		OutRandom = FString::Printf(TEXT("랜덤 카테고리 %+d"), int32(Choice.RandomPenaltyAmount));
	}

	// 시간
	if (!FMath::IsNearlyZero(Choice.TimerAdjustment))
	{
		OutTime = FString::Printf(TEXT("시간 %+d초"), int32(Choice.TimerAdjustment));
	}

	// 일시정지
	if (Choice.PauseDuration > 0.0f)
	{
		OutPause = FString::Printf(TEXT("%d초 정지"), int32(Choice.PauseDuration));
	}

	// 직원 버프
	if (Choice.BuffType != EBuffType::None && Choice.BuffDuration > 0.0f && Choice.BuffTargetCount > 0)
	{
		const int32 ActualWorkers = WorkerCount;
		const int32 EffectiveCount = FMath::Min(Choice.BuffTargetCount, FMath::Max(ActualWorkers, 1));

		FString WorkerText;
		if (ActualWorkers <= 0)            WorkerText = TEXT("직원");
		else if (EffectiveCount >= ActualWorkers) WorkerText = TEXT("전 직원");
		else                               WorkerText = FString::Printf(TEXT("랜덤 %d명"), EffectiveCount);

		const TCHAR* BuffName;
		FString ValueText;
		switch (Choice.BuffType)
		{
		case EBuffType::ScoreMultiplier:
			BuffName = TEXT("점수");
			ValueText = FString::Printf(TEXT("×%.1f"), Choice.BuffValue);
			break;
		case EBuffType::CritChance:
			BuffName = TEXT("크리율");
			ValueText = FString::Printf(TEXT("+%d%%"), int32(Choice.BuffValue * 100.0f));
			break;
		case EBuffType::WorkSpeed:
			BuffName = TEXT("작업속도");
			ValueText = (Choice.BuffValue < 1.0f && Choice.BuffValue > 0.0f)
				? FString::Printf(TEXT("×%.1f"), 1.0f / Choice.BuffValue)
				: FString::Printf(TEXT("×%.1f"), Choice.BuffValue);
			break;
		default:
			BuffName = TEXT("?");
			ValueText = FString::Printf(TEXT("×%.1f"), Choice.BuffValue);
			break;
		}
		OutBuff = FString::Printf(TEXT("%s %s%s (%d초)"), *WorkerText, BuffName, *ValueText, int32(Choice.BuffDuration));
	}
}
