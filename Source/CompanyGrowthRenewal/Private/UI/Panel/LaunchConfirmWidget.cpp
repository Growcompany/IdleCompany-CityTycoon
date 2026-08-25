#include "UI/Panel/LaunchConfirmWidget.h"
#include "CommonTextBlock.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/Common/StatRowWidget.h"
#include "UI/Element/Common/DisciplineBarWidget.h"
#include "UI/Element/Office/ReviewCardWidget.h"
#include "UI/Element/Cards/IconCardWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/EmployeeManager.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/LaunchReactionSubsystem.h"
#include "Table/ProjectDataTable.h"
#include "Core/CGGameInstance.h"
#include "Enum/QualityGrade.h"
#include "Enum/ProjectMode.h"
#include "Data/EmployeePotentialData.h"
#include "UI/PitchBoardText.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "CommonButtonBase.h"

namespace
{
	// 리빌 펀치 진폭 — S등급만 상향 (클라이맥스 강조)
	constexpr float RevealPopScale = 1.6f;
	constexpr float RevealPopScaleGrand = 1.9f;

	// 슬라이드인 시작 오프셋 (좌 → 우)
	constexpr float RevealSlideOffsetX = -46.0f;

	// 총액 카운트업 지속 시간
	constexpr float ReviewCountUpDuration = 0.6f;

	// 사운드 스팸 게이트 — 이 간격 안에 겹치는 리빌 사운드는 버린다
	constexpr float RevealSoundMinInterval = 0.09f;
}

void ULaunchConfirmWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 에디터 디자인 타임에도 MessageText 숨김
	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->HideMessageText();
	}
}

void ULaunchConfirmWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->bAutoRemove = false;
		ConfirmCancelWidget->HideMessageText();

		// 프롬프트 스택 풀 재사용 시 NativeConstruct 재진입 — 중복 바인딩 가드 (중복되면 [출시하기] 1클릭에 확정이 2회 발화)
		ConfirmCancelWidget->OnConfirm.RemoveAll(this);
		ConfirmCancelWidget->OnConfirm.AddUObject(this, &ULaunchConfirmWidget::OnDialogConfirmed);
		ConfirmCancelWidget->OnCancel.RemoveAll(this);
		ConfirmCancelWidget->OnCancel.AddUObject(this, &ULaunchConfirmWidget::OnDialogCancelled);
	}

	// X 닫기 버튼 바인딩
	if (CloseButton)
	{
		CloseButton->OnClicked().RemoveAll(this);
		CloseButton->OnClicked().AddUObject(this, &ULaunchConfirmWidget::OnCloseBtnClicked);
	}

	// 출시 confirm 팝업 등장 = 프로젝트 출시 멘트 — ChestOpen 톤. 실패 화면엔 축하 사운드를 내지 않는다.
	UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	const bool bBlocked = StageMgr && StageMgr->IsLaunchBlocked();
	if (!bBlocked)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
			{
				SoundMgr->PlaySound(FName("Event_Launch"));
			}
		}
	}

	// M10 미션 — 출시 확인 모달 등록 ([출시] 버튼 하이라이트용)
	if (UMissionManagerSubsystem* M = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		M->RegisterLaunchConfirm(this);
	}
}

void ULaunchConfirmWidget::NativeDestruct()
{
	if (UMissionManagerSubsystem* M = GetGameInstance()->GetSubsystem<UMissionManagerSubsystem>())
	{
		M->UnregisterLaunchConfirm(this);
	}
	Super::NativeDestruct();
}

UWidget* ULaunchConfirmWidget::GetConfirmButtonWidget() const
{
	return ConfirmCancelWidget ? ConfirmCancelWidget->GetConfirmButtonWidget() : nullptr;
}

void ULaunchConfirmWidget::SetPreviewData(const FStageProgressData& StageData, int32 ProjectNumber)
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;

	// 프로젝트 이름
	if (Text_ProjectName)
	{
		Text_ProjectName->SetText(FText::FromString(StageData.ProjectName));
	}

	// 프로젝트 이미지 카드 — 커버는 이미지 전용 (이름은 히어로 카드 Text_ProjectName 이 표시)
	if (UI_ProjectImageCard)
	{
		UI_ProjectImageCard->SetTextVisible(false);
	}
	if (UI_ProjectImageCard && GI)
	{
		UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
		if (TableMgr)
		{
			bool bSuccess = false;
			FProjectData ProjectData = (StageData.CompanyType != ECompanyType::None)
				? TableMgr->GetProjectData(StageData.CompanyType, ProjectNumber, bSuccess)
				: TableMgr->ResolveProjectData(ProjectNumber, bSuccess);
			if (bSuccess)
			{
				UTexture2D* IconTexture = nullptr;
				if (!ProjectData.Icon.IsNull())
				{
					IconTexture = ProjectData.Icon.LoadSynchronous();
				}
				UI_ProjectImageCard->SetIcon(IconTexture);
			}
		}
	}

	// 품질 점수/등급 계산
	float QualityScore = StageData.CalculateQualityScore();
	EQualityGrade Grade = StageData.CalculateQualityGrade();

	const FString GradeLetter = QualityGradeToAlphabetString(Grade);
	if (Text_QualityGrade)
	{
		Text_QualityGrade->SetText(FText::FromString(GradeLetter));
	}
	ApplyGradeStampColor(GradeLetterToColor(GradeLetter));

	if (Text_QualityScore)
	{
		// 배율 표기(×) — 이 값은 0.5~2.0 이라 "점수 / 만점" 형태로 쓰면 1.0 초과 시 분모를 넘어 거짓이 된다.
		Text_QualityScore->SetText(FText::FromString(FString::Printf(TEXT("×%.2f"), QualityScore)));
	}

	// 분야별 성과 — 축 고정 6칸(EProductionDiscipline 슬롯 순서). 비활성 분야는 빈 트랙으로
	// "요구 없음"을 보여 픽칭 카드 DevPlate와 실루엣이 이어진다.
	{
		UDisciplineBarWidget* DiscBars[6] = { DiscBar1, DiscBar2, DiscBar3, DiscBar4, DiscBar5, DiscBar6 };

		UTableManagerSubsystem* DiscTableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		const ECompanyType Industry = StageData.CompanyType;

		for (int32 SlotIdx = 0; SlotIdx < UE_ARRAY_COUNT(DiscBars); ++SlotIdx)
		{
			if (!DiscBars[SlotIdx]) { continue; }

			// 라벨 SOT = DT_DisciplineDisplay — 매니저 스텝 표시명과 같은 출처(패키지 빌드 안전)
			const FText Name = DiscTableMgr
				? DiscTableMgr->GetDisciplineDisplayName(Industry, SlotIdx) : FText::GetEmpty();

			const FStepRoundData* Found = StageData.Steps.FindByPredicate(
				[SlotIdx](const FStepRoundData& S) { return S.DisciplineSlot == SlotIdx; });

			if (Found)
			{
				// 채움색 = 직능 슬롯 위치색 (orb·플로팅 텍스트와 단일 출처 GetStepColor)
				DiscBars[SlotIdx]->SetInfo(Name, Found->AcquiredScore, Found->TargetScore,
					UDisciplineBarWidget::GetLightWellSlotFill(SlotIdx));
			}
			else
			{
				DiscBars[SlotIdx]->SetEmpty(Name);
			}
		}
	}

	{
		// 자체개발/제조업: 운영 정보 표시
		UProjectOperationManager* OpMgr = GI ? GI->GetSubsystem<UProjectOperationManager>() : nullptr;
		UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		// 착수(StartOperation)와 같은 함수 — 곡선만 읽으면 강화/특성 배율이 빠져 실제 운영시간과 갈린다
		UCGGameInstance* OpTimeGI = UCGGameInstance::GetInstance();
		const float OperationSeconds = OpMgr
			? OpMgr->ComputeEffectiveOperationTime(StageData.CalculateQualityGrade(), StageData.ProjectNumber,
				OpTimeGI ? OpTimeGI->GetCurrentManagedBuildingIndex() : INDEX_NONE)
			: 0.0f;

		if (Text_OperationTime)
		{
			int32 TotalSeconds = FMath::FloorToInt(OperationSeconds);
			int32 Minutes = TotalSeconds / 60;
			int32 Seconds = TotalSeconds % 60;

			FString TimeStr;
			if (Minutes >= 60)
			{
				int32 Hours = Minutes / 60;
				int32 Mins = Minutes % 60;
				TimeStr = FString::Printf(TEXT("%d시간 %d분"), Hours, Mins);
			}
			else if (Minutes > 0)
			{
				TimeStr = FString::Printf(TEXT("%d분 %d초"), Minutes, Seconds);
			}
			else
			{
				TimeStr = FString::Printf(TEXT("%d초"), Seconds);
			}
			Text_OperationTime->SetText(FText::FromString(TimeStr));
		}

		// 총수익은 피치 카드 추정(EstimatePitchEconomy)과 같은 항으로 잰다 — 산업 피크 배율 × 품질 배율 × 감쇠 적분.
		float LCPeakMult = 1.0f, LCHalf = 0.5f, LCVol = 0.0f;
		if (OpMgr)
		{
			OpMgr->GetIndustryCurveKnobs(StageData.CompanyType, LCPeakMult, LCHalf, LCVol);
		}
		float LCLeverage = 1.0f;
		if (TableMgr)
		{
			bool bProfOK = false;
			const FIndustryProfileRow Prof = TableMgr->GetIndustryProfile(StageData.CompanyType, bProfOK);
			if (bProfOK) { LCLeverage = Prof.ReviewLeverage; }
		}
		// 안정성(3번째 인자)을 빼면 결과 화면만 특성을 무시해 카드·운영과 갈린다.
		// 빌딩 출처도 착수·추정과 같은 게터여야 한다 — 이 위젯엔 빌딩 컨텍스트가 없어 GameInstance 에서 직접 읽는다.
		int32 LCBuildingIndex = INDEX_NONE;
		if (UCGGameInstance* LCGI = UCGGameInstance::GetInstance())
		{
			LCBuildingIndex = LCGI->GetCurrentManagedBuildingIndex();
		}
		const float LCStability = (OpMgr && LCBuildingIndex != INDEX_NONE)
			? OpMgr->GetRevenueStabilityFrac(LCBuildingIndex) : 0.0f;
		const float QualityMult = UProjectOperationManager::ComputeQualityRevenueMult(Grade, LCLeverage, LCStability);
		// 기준 레이트는 ProjectOperationManager 가 단일 소유 — 여기서 산식을 베끼면 결과 화면만 옛 스케일로 남는다
		// (실제로 그렇게 뒤처져 있던 자리다). 표기=실지급 계약이 이 위젯에서 깨지면 가장 늦게 발견된다.
		const float BaseRevenue = OpMgr ? OpMgr->CalculateBaseRevenueForProjectNumber(ProjectNumber) : 0.0f;
		// 트렌드 매칭은 결과 화면이 표시하지 않는다 — 중립 1.0 (추정부 호출자들도 동일).
		const float LCTrendMult = 1.0f;
		const float RevenuePerSecond = UProjectOperationManager::ComputePeakRevenuePerSec(
			BaseRevenue, LCPeakMult, LCTrendMult, QualityMult);

		if (Text_RevenuePerSecond)
		{
			FNumberFormattingOptions Opts;
			Opts.SetUseGrouping(true);
			// Min 0 = 정수면 소수점을 달지 않는다 ("15.0원/초" 의 .0 은 정보가 없다)
			Opts.SetMaximumFractionalDigits(1);
			Opts.SetMinimumFractionalDigits(0);
			Text_RevenuePerSecond->SetText(FText::FromString(FText::AsNumber(RevenuePerSecond, &Opts).ToString() + TEXT("원/초")));
		}

		PendingTotalRevenue = static_cast<int64>(UProjectOperationManager::ComputeLaunchRevenue(
			BaseRevenue, LCPeakMult, LCTrendMult, QualityMult, OperationSeconds, LCHalf));
		if (Text_TotalRevenue)
		{
			Text_TotalRevenue->SetText(FText::AsNumber(PendingTotalRevenue));
		}
	}

	// 미리보기 페이지(0) 고정 — 출시 확정 뒤의 보상 리빌은 호스트가 별도 위젯으로 띄운다.
	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(0);
	}
	// 풀 재사용 대비: 직전 실패 페이즈가 Collapsed 로 남긴 X/취소 버튼 복원
	if (CloseButton)
	{
		CloseButton->SetVisibility(ESlateVisibility::Visible);
	}
	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->SetConfirmOnly(false);
		// 짝 필수 — 직전 실패 페이즈의 SetCancelOnly 가 Confirm 을 접은 채 남으면
		// 다음 프로젝트가 미달이 아니어도 [출시하기] 가 영영 사라진다 (2026-08-12)
		ConfirmCancelWidget->SetCancelOnly(false);
	}
	// 직전 리빌이 잠가둔 확인 버튼 복원 (풀 재사용 대비)
	SetConfirmInteractionEnabled(true);

	// 풀 재사용 대비 — 직전 실패 페이즈("추가 개발")가 덮어쓴 제목·라벨을 WBP 기본값으로 되돌린다.
	// Default* 는 NativePreConstruct 에서만 적용되는데 재사용 시엔 PreConstruct 가 다시 돌지 않는다.
	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->RestoreDefaultTexts();
	}

	// 수준 미달 = 출시 경로를 아예 열지 않는다 — 미리보기를 건너뛰고 실패 페이지로 직행
	UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	bFailurePhase = StageMgr && StageMgr->IsLaunchBlocked();
	if (bFailurePhase)
	{
		if (ContentSwitcher)
		{
			ContentSwitcher->SetActiveWidgetIndex(FailurePageIndex);
		}
		FillFailureSection(StageData);
	}
	else
	{
		QueueRevenueReveal(PendingTotalRevenue);
	}
}

void ULaunchConfirmWidget::QueueRevenueReveal(int64 TotalRevenue)
{
	// 큐/시계/탭 스킵이 NativeTick 에 이미 물려 있다 — 상태만 되감아 재사용한다.
	RevealQueue.Reset();
	RevealClock = 0.0f;
	bRevealActive = false;
	RevenueCountStartAt = -1.0f;
	RevealEndTime = 0.0f;
	LastRevealSoundClock = -10.0f;

	// 계산 순서대로 = 읽는 순서대로. 배율이 먼저 와야 "무엇에 곱해지는지"가 성립한다.
	float StartAt = 0.12f;
	UWidget* CalcRows[3] = { RevRow_Multiplier, RevRow_Duration, RevRow_PerSecond };
	for (UWidget* Row : CalcRows)
	{
		if (!Row) { continue; }
		QueueReveal(Row, StartAt, false, FName("Event_ScoreSweep"), false, 0.0f, 0.7f);
		StartAt += 0.11f;
	}

	// 총액 — 행들이 다 들어온 뒤 잠깐 뜸을 들이고 굴린다
	StartAt += 0.10f;
	if (RevTotalBlock)
	{
		QueueReveal(RevTotalBlock, StartAt, false, FName("Event_ScoreSweep"));
	}
	if (Text_TotalRevenue && TotalRevenue > 0)
	{
		RevenueCountTarget = TotalRevenue;
		RevenueCountStartAt = StartAt;
		Text_TotalRevenue->SetText(FText::AsNumber(static_cast<int64>(0)));
		RevealEndTime = FMath::Max(RevealEndTime, StartAt + ReviewCountUpDuration);
	}

	// 확인 버튼은 잠그지 않는다 — 여기 숫자는 못 보고 눌러도 손해가 없고,
	// 매 프로젝트마다 반복되는 화면이라 대기를 강제하면 금방 성가셔진다. 탭하면 즉시 완료된다.

	// QueueReveal 이 행들을 opacity 0 으로 눕혀놨으므로, 이 줄이 없으면 NativeTick 이 조기반환해 영영 안 깨운다.
	// 카운트업 대상(Text_TotalRevenue)은 큐에 안 들어가므로 별도 조건 — RevTotalBlock 미배치 시에도 총액은 굴러야 한다.
	bRevealActive = RevealQueue.Num() > 0 || RevenueCountStartAt >= 0.0f;
}

void ULaunchConfirmWidget::FillFailureSection(const FStageProgressData& StageData)
{
	// 사내 테스트 리포트 — 미달 분야만 게이지 행으로. 채움 = 획득/목표,
	// 최소선 마커는 WBP 고정 50%(MinimumScore = TargetScore×0.5 계약 — 출시실패 시스템 §1).
	UStatRowWidget* FailRows[6] = { FailStatRow1, FailStatRow2, FailStatRow3, FailStatRow4, FailStatRow5, FailStatRow6 };
	TArray<FString> ShortNames;
	int32 RowIdx = 0;
	for (const FStepRoundData& S : StageData.Steps)
	{
		if (S.DisciplineSlot == INDEX_NONE || S.HasPassedMinimum())
		{
			continue;
		}
		// UI 표기는 "분야" — 코드/기획 용어인 직능(EProductionDiscipline)을 그대로 노출하지 않는다
		const FString DisciplineName = S.StepName.IsEmpty() ? TEXT("분야") : S.StepName;
		ShortNames.Add(DisciplineName);
		if (RowIdx < 6 && FailRows[RowIdx])
		{
			FailRows[RowIdx]->SetVisibility(ESlateVisibility::HitTestInvisible);
			// SetCurrentProgress 가 StatValueText 를 "획득/목표"로 덮으므로 바 먼저, 표기("획득 / 최소")는 나중
			FailRows[RowIdx]->SetCurrentProgress(S.AcquiredScore, S.TargetScore);
			FailRows[RowIdx]->SetStatInfo(DisciplineName, FString::Printf(TEXT("%d / %d"),
				FMath::FloorToInt(S.AcquiredScore), FMath::CeilToInt(S.MinimumScore)));
			++RowIdx;
		}
	}
	for (int32 Index = RowIdx; Index < 6; ++Index)
	{
		if (FailRows[Index]) { FailRows[Index]->SetVisibility(ESlateVisibility::Collapsed); }
	}

	if (Text_FailSummary)
	{
		Text_FailSummary->SetText(FText::FromString(
			FString::Printf(TEXT("%s 분야가 최소 기준에 미치지 못했습니다 ― 추가 개발로 보완할 수 있습니다."),
				*FString::Join(ShortNames, TEXT(" · ")))));
	}

	// 테스터 의견 2장 — 실패 페이지의 서사(출시 전이라 외부 반응 금지). Worst 컨텍스트 행이 우선 후보.
	UGameInstance* TokenGI = GetGameInstance();
	UTableManagerSubsystem* TokenTableMgr = TokenGI ? TokenGI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FReviewTokenValues TesterTokens;
	TSet<FName> TesterContexts;
	ULaunchReactionSubsystem::BuildReviewTokens(StageData, TokenTableMgr, TesterTokens, TesterContexts);
	TArray<FText> TesterNicks, TesterComments;
	if (TokenTableMgr)
	{
		TokenTableMgr->GetReviewComments(FName(TEXT("Tester")), StageData.CompanyType, FName(TEXT("Fail")),
			TesterContexts, 2, TesterTokens, TesterNicks, TesterComments);
	}
	UReviewCardWidget* TesterCards[2] = { TesterCard1, TesterCard2 };
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UReviewCardWidget* Card = TesterCards[Index];
		if (!Card) { continue; }
		if (!TesterNicks.IsValidIndex(Index))
		{
			Card->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		Card->SetVisibility(ESlateVisibility::HitTestInvisible);
		Card->SetSnsData(TesterNicks[Index], TesterComments[Index]);   // 사내 직함 — @ 접두사 없음
	}

	UOfficeStageProgressManager* StageMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	const int32 Cost = StageMgr ? StageMgr->GetRetryDiamondCost() : 0;
	const bool bRetryLeft = StageMgr && StageMgr->CanRetryDevelopment();

	bool bCanAfford = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UResourceItemManager* ResMgr = GI->GetSubsystem<UResourceItemManager>())
		{
			bCanAfford = ResMgr->HasResource(EResourceType::Diamond, Cost);
		}
	}

	if (RetryCostChip)
	{
		RetryCostChip->SetVisibility(bRetryLeft ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		RetryCostChip->SetResourceType(EResourceType::Diamond);
		RetryCostChip->SetValue(Cost);
		RetryCostChip->SetCanAfford(bCanAfford);
	}

	if (ConfirmCancelWidget)
	{
		// 실패 페이지는 자체 타이틀(FailTitle)을 가지므로 프레임 제목을 비운다 —
		// 제목을 갈아끼우지 않는 이유: TitleText 가 이 WBP 에서 Collapsed 로 baked 됐을 수 있고,
		// 그러면 SetTitle 은 가시성을 못 바꿔 타이틀이 통째로 사라진다. 비우기는 어느 쪽이든 안전하다.
		ConfirmCancelWidget->SetTitle(FText::GetEmpty());

		// 프레임의 두 버튼을 재해석 — 확인 = 추가 개발, 취소 = 폐기
		ConfirmCancelWidget->SetConfirmButtonText(FText::FromString(TEXT("추가 개발")));
		ConfirmCancelWidget->SetCancelButtonText(FText::FromString(TEXT("폐기")));
		// 컨티뉴를 이미 썼거나 다이아가 모자라면 남는 출구는 폐기뿐
		ConfirmCancelWidget->SetCancelOnly(!bRetryLeft || !bCanAfford);
	}

	// 경험치는 게이트 판정보다 먼저 지급된다 — 실패해도 팀은 자랐다는 걸 이 줄만 말해준다
	if (Text_FailXp)
	{
		UCGGameInstance* XpGI = UCGGameInstance::GetInstance();
		UEmployeeManager* XpEmpMgr = XpGI ? XpGI->GetSubsystem<UEmployeeManager>() : nullptr;
		const float RoundExp = StageMgr ? StageMgr->GetLastRoundEmployeeExp() : 0.0f;
		if (RoundExp <= 0.0f)
		{
			// 지급이 없었으면 성장을 말할 근거가 없다 — "+0" 을 남기느니 줄을 접는다
			Text_FailXp->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			// 헤드라인은 건물이 뿌린 팀 공통 기준량, 「N판」은 그 직원이 실제로 받는 양(스탯·큐브 배율 포함)으로 센다
			FString XpLine = FString::Printf(TEXT("경험치 +%d"), FMath::RoundToInt(RoundExp));
			if (XpGI && XpEmpMgr)
			{
				// 레벨업에 가장 가까운 직원 1명 — "N판이면 레벨업"이 실패를 성장으로 읽게 한다
				int32 BestRounds = MAX_int32;
				int32 BestLevel = 0;
				FString BestName;
				const TArray<FEmployeeInstance> Roster = XpEmpMgr->GetEmployeesInBuilding(XpGI->GetCurrentManagedBuildingIndex());
				for (const FEmployeeInstance& Employee : Roster)
				{
					const float PerRound = RoundExp * XpEmpMgr->GetExpGainMultiplier(Employee)
						* UEmployeePotentialHelper::AggregateModifiers(Employee.PotentialAbility).ExpMult;
					const int32 Rounds = PitchBoardText::ComputeRoundsToLevelUp(
						Employee.Experience, static_cast<float>(XpEmpMgr->GetMaxExperienceForLevel(Employee.Level)), PerRound);
					if (Rounds < BestRounds)
					{
						BestRounds = Rounds;
						BestLevel = Employee.Level;
						BestName = Employee.EmployeeName;
					}
				}
				if (BestRounds != MAX_int32)
				{
					XpLine += FString::Printf(TEXT(" · %s Lv%d까지 %d판"), *BestName, BestLevel + 1, BestRounds);
				}
			}
			Text_FailXp->SetText(FText::FromString(XpLine));
			Text_FailXp->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
	else if (!bFailXpWarned)
	{
		bFailXpWarned = true;
		UE_LOG(LogTemp, Warning, TEXT("[LaunchConfirm] Text_FailXp 미바인딩 ― pb_failxp.py 실행 필요"));
	}
}

void ULaunchConfirmWidget::UpdateStampMaterialSize()
{
	if (!StampBG && !StampLine && !StampGlint)
	{
		return;
	}

	// 스탬프는 캡션 길이에 따라 폭이 달라진다 — 위젯을 고정하면 글자가 넘치므로(2026-07-29 실측)
	// 위젯은 콘텐츠 크기로 두고 SDF 에 실제 크기를 넣는다.
	const FVector2D LocalSize = StampBG ? StampBG->GetCachedGeometry().GetLocalSize()
		: StampLine->GetCachedGeometry().GetLocalSize();
	if (LocalSize.X < 1.f || LocalSize.Y < 1.f || LocalSize.Equals(LastStampMatSize, 0.5f))
	{
		return;
	}
	LastStampMatSize = LocalSize;

	static const FName WpxParam(TEXT("Wpx"));
	static const FName HpxParam(TEXT("Hpx"));
	for (UImage* Layer : { StampBG, StampLine, StampGlint })
	{
		if (!Layer)
		{
			continue;
		}
		if (UMaterialInstanceDynamic* MID = Layer->GetDynamicMaterial())
		{
			MID->SetScalarParameterValue(WpxParam, LocalSize.X);
			MID->SetScalarParameterValue(HpxParam, LocalSize.Y);
		}
	}
}

void ULaunchConfirmWidget::ApplyGradeStampColor(const FLinearColor& GradeColor)
{
	// 스탬프는 SDF 머티리얼 3층이라 SetBrushColor 가 아니라 MID 파라미터로 색이 들어간다.
	// RoundedBox 가 단색 채움만 돼서 그라데이션/글린트가 안 나오는 것이 머티리얼로 간 이유.
	FLinearColor Deep = GradeColor * 0.62f;
	Deep.A = 1.0f;

	if (StampBG)
	{
		if (UMaterialInstanceDynamic* MID = StampBG->GetDynamicMaterial())
		{
			// M_UI_ChipBG 의 채움 파라미터는 TintCol 하나뿐이다. 없는 이름에 넣으면 조용히 무시돼
			// 모든 등급이 baked 색으로 남는다 (M_UIPanel_Rounded 의 FillTop/FillBottom 과 혼동 주의).
			// 세로 그라데이션은 StampSheen/StampShade 두 장이 담당.
			MID->SetVectorParameterValue(TEXT("TintCol"), GradeColor);
		}
	}
	if (StampLine)
	{
		if (UMaterialInstanceDynamic* MID = StampLine->GetDynamicMaterial())
		{
			MID->SetVectorParameterValue(TEXT("LineCol"), Deep);
		}
	}

	// 컬러 면 위 라벨은 그 색 계열 진한 섀이드로 아웃라인 (UI_STYLE_CATALOG 마감 §6)
	if (Text_QualityGrade)
	{
		FSlateFontInfo Font = Text_QualityGrade->GetFont();
		Font.OutlineSettings.OutlineSize = 2;
		Font.OutlineSettings.OutlineColor = Deep;   // FLinearColor 필드 — sRGB 왕복 변환 불필요
		Text_QualityGrade->SetFont(Font);
	}
}

void ULaunchConfirmWidget::QueueReveal(UWidget* Widget, float StartAt, bool bPop, FName SoundKey,
	bool bGrand, float SoundLead, float SoundVolume, bool bSkipSilent)
{
	if (!Widget) { return; }
	// 레이아웃 자리는 유지한 채 투명으로 대기 — 등장 순간까지 리플로 없음
	Widget->SetRenderOpacity(0.0f);
	if (bPop)
	{
		const float PopScale = bGrand ? RevealPopScaleGrand : RevealPopScale;
		Widget->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(PopScale, PopScale), FVector2D::ZeroVector, 0.0f));
	}
	else
	{
		Widget->SetRenderTranslation(FVector2D(RevealSlideOffsetX, 0.0f));
	}
	FRevealItem Item;
	Item.Widget = Widget;
	Item.StartAt = StartAt;
	Item.bPop = bPop;
	Item.bGrand = bGrand;
	Item.SoundKey = SoundKey;
	Item.SoundLead = SoundLead;
	Item.SoundVolume = SoundVolume;
	Item.bSkipSilent = bSkipSilent;
	RevealQueue.Add(Item);

	RevealEndTime = FMath::Max(RevealEndTime, StartAt + Item.Duration);
}

void ULaunchConfirmWidget::PlayRevealSound(FName SoundKey, float VolumeScale, bool bForce)
{
	if (SoundKey.IsNone()) { return; }
	// 연속 등장 구간에서 클립이 뭉쳐 스팸이 되는 걸 차단 (클라이맥스는 예외)
	if (!bForce && (RevealClock - LastRevealSoundClock) < RevealSoundMinInterval) { return; }

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SoundMgr->PlaySoundWithVolume(SoundKey, VolumeScale);
		}
	}
	LastRevealSoundClock = RevealClock;
}

void ULaunchConfirmWidget::SetConfirmInteractionEnabled(bool bEnable)
{
	if (UCommonButtonBase* ConfirmBtn = Cast<UCommonButtonBase>(GetConfirmButtonWidget()))
	{
		ConfirmBtn->SetIsInteractionEnabled(bEnable);
	}
}

void ULaunchConfirmWidget::SkipRevealQueue()
{
	if (!bRevealActive) { return; }

	for (FRevealItem& Item : RevealQueue)
	{
		// 미재생 사운드는 버린다 — 단 클라이맥스(펀치)는 1회 보상 재생.
		// bSkipSilent 항목은 제외 — 여러 건이 한 프레임에 뭉쳐 터진다
		if (!Item.bStarted)
		{
			Item.bStarted = true;
			if (Item.bPop && !Item.bSkipSilent) { PlayRevealSound(Item.SoundKey, Item.SoundVolume, true); }
		}
		if (UWidget* W = Item.Widget.Get())
		{
			W->SetRenderOpacity(1.0f);
			W->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(1.0f, 1.0f), FVector2D::ZeroVector, 0.0f));
		}
	}

	// 카운트업은 NativeTick 이 굴리는데 여기서 bRevealActive 를 끄므로, 최종값은 스킵이 직접 확정해야 한다
	if (Text_TotalRevenue && RevenueCountStartAt >= 0.0f && RevenueCountTarget > 0)
	{
		Text_TotalRevenue->SetText(FText::AsNumber(RevenueCountTarget));
	}

	RevealClock = RevealEndTime;
	bRevealActive = false;
	SetConfirmInteractionEnabled(true);
}

FReply ULaunchConfirmWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 리빌 중 탭 = 스킵 (최종 상태로 시간 점프)
	if (bRevealActive)
	{
		SkipRevealQueue();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void ULaunchConfirmWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// ⚠ 아래 리빌 조기반환보다 앞에 — 미리보기 페이지(리빌 비활성)에서도 스탬프 크기는 맞아야 한다
	UpdateStampMaterialSize();

	if (!bRevealActive) { return; }
	RevealClock += InDeltaTime;

	bool bAllDone = true;
	for (FRevealItem& Item : RevealQueue)
	{
		UWidget* W = Item.Widget.Get();
		if (!W) { continue; }

		// 사운드 판정은 T<0 early-out 앞에서 — SoundLead 만큼 앞서 재생해 어택이 착지에 겹치게 한다
		if (!Item.bStarted && RevealClock >= (Item.StartAt - Item.SoundLead))
		{
			Item.bStarted = true;
			PlayRevealSound(Item.SoundKey, Item.SoundVolume, Item.bPop);
		}

		const float T = (RevealClock - Item.StartAt) / Item.Duration;
		if (T < 0.0f) { bAllDone = false; continue; }

		const float Clamped = FMath::Clamp(T, 0.0f, 1.0f);
		const float Ease = 1.0f - FMath::Pow(1.0f - Clamped, 3.0f);   // ease-out cubic
		W->SetRenderOpacity(Ease);
		if (Item.bPop)
		{
			const float PopStart = Item.bGrand ? RevealPopScaleGrand : RevealPopScale;
			const float Scale = PopStart - (PopStart - 1.0f) * Ease;
			W->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(Scale, Scale), FVector2D::ZeroVector, 0.0f));
		}
		else
		{
			W->SetRenderTranslation(FVector2D(RevealSlideOffsetX * (1.0f - Ease), 0.0f));
		}
		if (Clamped < 1.0f) { bAllDone = false; }
	}

	// 수익 총액 카운트업 — 등장 시점부터 0 → 목표
	if (Text_TotalRevenue && RevenueCountStartAt >= 0.0f && RevenueCountTarget > 0)
	{
		const float P = FMath::Clamp((RevealClock - RevenueCountStartAt) / ReviewCountUpDuration, 0.0f, 1.0f);
		Text_TotalRevenue->SetText(FText::AsNumber(
			static_cast<int64>(FMath::RoundToDouble(P * static_cast<double>(RevenueCountTarget)))));
		if (P < 1.0f) { bAllDone = false; }
	}

	if (bAllDone)
	{
		bRevealActive = false;
		SetConfirmInteractionEnabled(true);
	}
}

void ULaunchConfirmWidget::OnDialogConfirmed()
{
	// 실패 페이지에서는 확인 = [추가 개발]. 개발이 재개되므로 모달은 닫는다.
	// ⚠ 닫기 먼저, broadcast 나중 — 핸들러가 다음 모달을 push 할 때 이 위젯이 스택 top 이어야 잔류/부활이 없다.
	DeactivateWidget();
	if (bFailurePhase)
	{
		OnLaunchRetryRequested.Broadcast();
		return;
	}
	// [출시하기] = 커밋. 보상 리빌은 호스트(OfficeMainWidget)가 닫힌 자리에 push 한다.
	OnLaunchConfirmed.Broadcast();
}

void ULaunchConfirmWidget::OnDialogCancelled()
{
	OnLaunchCancelled.Broadcast();
	DeactivateWidget();
}

void ULaunchConfirmWidget::OnCloseBtnClicked()
{
	OnLaunchCancelled.Broadcast();
	DeactivateWidget();
}
