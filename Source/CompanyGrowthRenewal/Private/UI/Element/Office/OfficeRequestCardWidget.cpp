#include "UI/Element/Office/OfficeRequestCardWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/WrapBox.h"
#include "Enum/QualityGrade.h"
#include "Enum/WidgetType.h"
#include "Data/StageProgressData.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Office/TeamPipItemWidget.h"
#include "UI/PitchBoardText.h"
#include "Manager/TableManagerSubsystem.h"
#include "Global/GlobalUtilFunctions.h"
#include "Core/CGGameInstance.h"
#include "Enum/CompanyType.h"
#include "Materials/MaterialInstanceDynamic.h"

void UOfficeRequestCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 통과/미달 두 CTA 는 같은 결과(착수)를 내므로 핸들러를 공유한다
	if (AcceptButton)
	{
		AcceptButton->OnClicked().AddUObject(this, &UOfficeRequestCardWidget::OnAcceptClicked);
	}
	if (AcceptButtonSecondary)
	{
		AcceptButtonSecondary->OnClicked().AddUObject(this, &UOfficeRequestCardWidget::OnAcceptClicked);
	}
	if (ViewEmployeesButton)
	{
		ViewEmployeesButton->OnClicked().AddUObject(this, &UOfficeRequestCardWidget::OnViewEmployeesClicked);
	}
	if (MoreButton)
	{
		MoreButton->OnClicked().AddUObject(this, &UOfficeRequestCardWidget::OnMoreClicked);
	}

	// 캐시된 데이터가 있으면 적용 (CreateWidget 후 SetSlotData가 먼저 호출된 경우)
	if (bHasCachedData)
	{
		ApplySlotData();
	}
	else
	{
		ApplyExpanded();
	}

	ApplyRecommendedVisual();
}

void UOfficeRequestCardWidget::NativeDestruct()
{
	if (AcceptButton)
	{
		AcceptButton->OnClicked().RemoveAll(this);
	}
	if (AcceptButtonSecondary)
	{
		AcceptButtonSecondary->OnClicked().RemoveAll(this);
	}
	if (ViewEmployeesButton)
	{
		ViewEmployeesButton->OnClicked().RemoveAll(this);
	}
	if (MoreButton)
	{
		MoreButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UOfficeRequestCardWidget::SetSlotData(const FProjectBoardSlot& SlotData, int32 InSlotIndex)
{
	SlotIndex = InSlotIndex;
	CachedSlotData = SlotData;
	bHasCachedData = true;

	// NativeConstruct가 이미 호출된 상태면 즉시 적용
	if (IsConstructed())
	{
		ApplySlotData();
	}
}

void UOfficeRequestCardWidget::SetDevelopedGrade(const FString& InGradeLetter)
{
	DevelopedGrade = InGradeLetter;

	if (IsConstructed() && bHasCachedData)
	{
		ApplySlotData();
	}
}

void UOfficeRequestCardWidget::SetTeamDisciplinePoints(const TArray<int32>& TotalsBySlot)
{
	TeamPointsBySlot = TotalsBySlot;

	if (IsConstructed() && bHasCachedData)
	{
		ApplySlotData();
	}
}

void UOfficeRequestCardWidget::SetExpanded(bool bInExpanded)
{
	bExpanded = bInExpanded;

	if (IsConstructed())
	{
		ApplyExpanded();
	}
}

UWidget* UOfficeRequestCardWidget::GetPrimaryCtaWidget() const
{
	if (AcceptButton && AcceptButton->GetVisibility() == ESlateVisibility::Visible)
	{
		return AcceptButton;
	}
	if (AcceptButtonSecondary && AcceptButtonSecondary->GetVisibility() == ESlateVisibility::Visible)
	{
		return AcceptButtonSecondary;
	}
	// 대기 상태엔 착수 CTA 가 없다 — 앵커는 유일하게 보이는 「직원 보기」
	return ViewEmployeesButton;
}

void UOfficeRequestCardWidget::ApplySlotData()
{
	ApplyPitchPresentation(CachedSlotData);
}

void UOfficeRequestCardWidget::ApplyPitchPresentation(const FProjectBoardSlot& SlotData)
{
	bool bOutlookUsable = SlotData.bHasOutlookEstimate
		&& SlotData.ExpectedDisciplineScores.Num() == 6
		&& FMath::IsFinite(SlotData.ExpectedQuality);
	if (bOutlookUsable)
	{
		for (const float ExpectedScore : SlotData.ExpectedDisciplineScores)
		{
			if (!FMath::IsFinite(ExpectedScore))
			{
				bOutlookUsable = false;
				break;
			}
		}
	}

	// 커버 모자이크 — 파라미터 기본값(가로 48셀 = CellPx 12.083 / Bright 0.75)을 그대로 쓴다.
	// 튜닝 = Tools/UIMaterial/make_cover_mosaic.py 재실행 (마스터 기본값 세터가 없어 재생성이 유일 경로, 웰 580x410이 상수로 박혀 있음).
	if (CoverImage)
	{
		UTexture2D* CoverTex = SlotData.Cover.IsNull() ? nullptr : SlotData.Cover.LoadSynchronous();

		if (CoverTex && !CoverMID)
		{
			UMaterialInterface* MosaicMat = TSoftObjectPtr<UMaterialInterface>(
				FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Materials/M_UI_CoverMosaic.M_UI_CoverMosaic"))).LoadSynchronous();
			if (MosaicMat)
			{
				CoverMID = UMaterialInstanceDynamic::Create(MosaicMat, this);
				CoverImage->SetBrushFromMaterial(CoverMID);  // 브러시 DrawAs(RoundedBox)는 보존됨 — 리소스만 교체
			}
		}

		if (CoverTex && CoverMID)
		{
			CoverMID->SetTextureParameterValue(FName("Cover"), CoverTex);
			CoverImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			// 커버 결손·머티리얼 부재 → 아래 WellFill 장르색이 그대로 노출된다
			CoverImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 커버가 덮지 못한 카드의 폴백 면 — 장르 딥톤(라이트는 WBP의 WellTopGlow가 담당).
	// 원색 그대로면 웰 위 흰 "?"·타이틀 대비가 죽어 0.52를 곱한다.
	if (WellFill)
	{
		const FLinearColor Deep = SlotData.GenreColor * 0.52f;
		WellFill->SetBrushColor(FLinearColor(Deep.R, Deep.G, Deep.B, 1.f));
	}

	// 프로젝트명 (웰 아래 대표 타이틀)
	if (Text_ProjectName)
	{
		Text_ProjectName->SetText(SlotData.ProjectName);
		Text_ProjectName->SetVisibility(ESlateVisibility::Visible);
	}

	// 장르 · 소재 (+ 이미 개발한 프로젝트면 기록 등급)
	if (Text_Tagline)
	{
		FString TagLine = FString::Printf(TEXT("%s · %s"),
			*SlotData.Genre.ToString(), *SlotData.Material.ToString());
		if (!DevelopedGrade.IsEmpty())
		{
			TagLine += FString::Printf(TEXT(" · 개발함 ★%s"), *DevelopedGrade);
		}
		if (SlotData.bTrendMatched)
		{
			TagLine += TEXT(" · 트렌드 ×1.25");
		}
		Text_Tagline->SetText(FText::FromString(TagLine));
		Text_Tagline->SetVisibility(ESlateVisibility::Visible);
	}

	// 축 라벨 = 산업별 직능 DT — 현재 진입 건물의 산업이 곧 이 피치의 산업
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UCGGameInstance* CGGI = Cast<UCGGameInstance>(GI);
	const ECompanyType Industry = CGGI ? CGGI->GetCurrentBuildingCompanyType() : ECompanyType::None;

	// 현재 팀 예상 = 직능별 예상 점수 / 착수 목표. 칸 인덱스가 곧 EProductionDiscipline 슬롯이라 정렬하지 않는다.
	const int32 W[6] = {
		SlotData.Weight_Plan, SlotData.Weight_Dev, SlotData.Weight_Graphics,
		SlotData.Weight_Sound, SlotData.Weight_Server, SlotData.Weight_QA
	};

	int32 MaxW = 0;
	for (int32 i = 0; i < 6; ++i)
	{
		MaxW = FMath::Max(MaxW, W[i]);
	}

	UProgressBar* Bars[6] = { DevBar1, DevBar2, DevBar3, DevBar4, DevBar5, DevBar6 };
	UCommonTextBlock* Vals[6] = { Text_DevVal1, Text_DevVal2, Text_DevVal3, Text_DevVal4, Text_DevVal5, Text_DevVal6 };
	UCommonTextBlock* Names[6] = { Text_DevName1, Text_DevName2, Text_DevName3, Text_DevName4, Text_DevName5, Text_DevName6 };

	const float TargetBase = FStageProgressData::ComputeDisciplineTargetBase(
		SlotData.RequiredScore_Step1, SlotData.RequiredScore_Step2, SlotData.RequiredScore_Step3);

	TArray<float> Targets; Targets.Reserve(6);
	TArray<float> Scores;  Scores.Reserve(6);
	for (int32 i = 0; i < 6; ++i)
	{
		Targets.Add(FStageProgressData::ComputeDisciplineTarget(W[i], MaxW, TargetBase));
		Scores.Add(SlotData.ExpectedDisciplineScores.IsValidIndex(i) ? SlotData.ExpectedDisciplineScores[i] : 0.0f);
	}

	// 처방 직능은 보드 타일과 같은 결정을 써야 카드와 타일이 서로 다른 직능을 가리키지 않는다
	int32 WorstSlot = INDEX_NONE;
	float WorstRatio = 0.0f;
	if (bOutlookUsable)
	{
		PitchBoardText::FindWorstDiscipline(Scores, Targets, WorstSlot, WorstRatio);
	}

	for (int32 i = 0; i < 6; ++i)
	{
		const float Target = Targets[i];
		const bool bIsActive = Target > 0.0f;
		float Ratio = 0.0f;
		FLinearColor OutlookColor = OutlookInactiveColor;

		if (bOutlookUsable && bIsActive)
		{
			Ratio = Scores[i] / Target;
			// 게이트 하한(50%)이 유일한 경계 — 그 사이 앰버 구간은 "부족한데 빨갛지 않다"를 만들어 폐기
			OutlookColor = (Ratio < FStageProgressData::MinimumScoreRatio) ? OutlookShortColor : OutlookPassColor;
		}

		if (Bars[i])
		{
			Bars[i]->SetPercent(FMath::Clamp(Ratio, 0.0f, 1.0f));
			Bars[i]->SetFillColorAndOpacity(OutlookColor);
		}
		if (Vals[i])
		{
			if (bOutlookUsable && bIsActive)
			{
				const int32 DisplayPercent = FStageProgressData::ComputeDisciplinePercentDisplayValue(Ratio);
				Vals[i]->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), DisplayPercent)));
			}
			else
			{
				Vals[i]->SetText(FText::FromString(TEXT("―")));
			}
			Vals[i]->SetColorAndOpacity(FSlateColor(OutlookColor));
		}
		// 축 라벨의 단일 출처 = DT_DisciplineDisplay (매니저의 스텝 표시명과 같은 출처라 둘이 갈리지 않는다)
		if (Names[i])
		{
			Names[i]->SetText(TableMgr ? TableMgr->GetDisciplineDisplayName(Industry, i) : FText::GetEmpty());
			Names[i]->SetColorAndOpacity(FSlateColor(OutlookColor));
		}
	}

	// 유효하지 않은 프로젝트도 고정 6축을 유지해 팀 전망을 사용할 수 없음을 직접 보여준다.
	if (DevPlate)
	{
		DevPlate->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// ── 판정 밴드 — 카드가 내리는 결론 한 줄 + 처방 ──
	FText BandTitle;
	FText BandSub;
	FLinearColor BandCol = BandWaitColor;
	bool bShowGrade = false;
	bool bShowShort = false;

	// DT 미스면 빈 이름 그대로 — 직능명을 코드에 박으면 산업별 표시명 SOT 가 두 벌이 된다
	const FText WorstName = (TableMgr && WorstSlot != INDEX_NONE)
		? TableMgr->GetDisciplineDisplayName(Industry, WorstSlot)
		: FText::GetEmpty();

	if (!bOutlookUsable)
	{
		BandTitle = FText::FromString(TEXT("팀 배치 필요"));
		BandSub = FText::FromString(TEXT("직원을 앉히면 예상이 표시됩니다"));
	}
	else if (SlotData.bGateRisk)
	{
		BandTitle = FText::FromString(TEXT("출시 불가"));
		BandSub = PitchBoardText::MakeShortageSentence(WorstName);
		BandCol = BandFailColor;
		bShowShort = true;

		if (Text_BandShortName)
		{
			Text_BandShortName->SetText(WorstName);
		}
		if (Text_BandShortPct)
		{
			// 우칸은 세 자리까지만 버틴다 — 목표가 극히 낮은 프로젝트의 폭주값을 999 로 눕힌다
			const int32 ShortPct = (WorstSlot != INDEX_NONE)
				? FMath::Min(999, FStageProgressData::ComputeDisciplinePercentDisplayValue(WorstRatio))
				: 0;
			Text_BandShortPct->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), ShortPct)));
		}
	}
	else
	{
		const FString Grade = QualityGradeToAlphabetString(QualityScoreToGrade(SlotData.ExpectedQuality));
		BandTitle = FText::FromString(DevelopedGrade.IsEmpty() ? TEXT("출시 가능") : TEXT("재개발 가능"));
		BandSub = PitchBoardText::MakeNextGradeHint(WorstName, PitchBoardText::NextGradeLetter(Grade));
		BandCol = BandPassColor;
		bShowGrade = true;

		if (Text_BandGrade)
		{
			Text_BandGrade->SetText(FText::FromString(Grade));
		}
	}

	if (Band)
	{
		Band->SetBrushColor(BandCol);
	}
	if (Text_BandTitle)
	{
		Text_BandTitle->SetText(BandTitle);
	}
	if (Text_BandSub)
	{
		Text_BandSub->SetText(BandSub);
	}
	if (BandRightGrade)
	{
		BandRightGrade->SetVisibility(bShowGrade ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (BandRightShort)
	{
		BandRightShort->SetVisibility(bShowShort ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Img_BandGlyph)
	{
		const TSoftObjectPtr<UTexture2D>& GlyphPtr = bShowShort ? GlyphExclaim : GlyphCheck;
		UTexture2D* GlyphTex = (!bOutlookUsable || GlyphPtr.IsNull()) ? nullptr : GlyphPtr.LoadSynchronous();
		Img_BandGlyph->SetVisibility(GlyphTex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (GlyphTex)
		{
			Img_BandGlyph->SetBrushFromTexture(GlyphTex, false);
		}
	}

	// 수익 · 운영 한 줄 (구 StatRow 2행 대체)
	if (Text_Economy)
	{
		FString EconomyLine = FString::Printf(TEXT("예상 수익 %s원"),
			*UGlobalUtilFunctions::AbbreviateNumber(SlotData.EstimatedRevenue, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil).ToString());
		if (SlotData.bTrendMatched)
		{
			// 배수는 수익값에 이미 곱해져 있다 — 왜 이 수익이 높은지를 숫자 옆에서 밝히는 표기
			EconomyLine += TEXT(" ×1.25");
		}
		if (SlotData.EstimatedOpTimeSec > 0)
		{
			const int32 Sec = SlotData.EstimatedOpTimeSec;
			const FString Dur = (Sec < 60)
				? FString::Printf(TEXT("%d초"), Sec)
				: ((Sec % 60 == 0) ? FString::Printf(TEXT("%d분"), Sec / 60)
				                   : FString::Printf(TEXT("%.1f분"), Sec / 60.0f));
			EconomyLine += FString::Printf(TEXT(" · 운영 %s"), *Dur);
		}
		Text_Economy->SetText(FText::FromString(EconomyLine));
		Text_Economy->SetVisibility(ESlateVisibility::Visible);
	}

	// CTA 3상태 — 대기(로스터 0)에 초록 「개발 시작」이 뜨면 밴드의 「팀 배치 필요」와 카드가 서로 다른 말을 한다
	const bool bWait = !bOutlookUsable;
	const bool bFail = bOutlookUsable && SlotData.bGateRisk;
	if (AcceptButton)
	{
		AcceptButton->SetButtonText(FText::FromString(TEXT("개발 시작")));
		AcceptButton->SetVisibility((bWait || bFail) ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (AcceptButtonSecondary)
	{
		AcceptButtonSecondary->SetButtonText(FText::FromString(TEXT("그래도 개발")));
		AcceptButtonSecondary->SetVisibility(bFail ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ViewEmployeesButton)
	{
		ViewEmployeesButton->SetButtonText(FText::FromString(TEXT("직원 보기")));
		ViewEmployeesButton->SetVisibility((bWait || bFail) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	CachedWorstSlot = WorstSlot;

	RebuildTeamLine(W);
	ApplyExpanded();
}

void UOfficeRequestCardWidget::RebuildTeamLine(const int32 W[6])
{
	if (!TeamWrap)
	{
		return;
	}

	TeamWrap->ClearChildren();
	TeamItems.Reset();

	if (TeamPointsBySlot.Num() == 0)
	{
		TeamWrap->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const TSubclassOf<UUserWidget> PipClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::TeamPipItem) : nullptr;
	if (!PipClass)
	{
		// WBP/DT 행이 아직 없는 단계 — 줄만 접고 카드의 나머지는 그대로 동작시킨다
		UE_LOG(LogTemp, Warning, TEXT("[OfficeRequestCard] EWidgetType::TeamPipItem 미등록(DT_WidgetClass) - 내 팀 줄 생략"));
		TeamWrap->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UCGGameInstance* CGGI = Cast<UCGGameInstance>(GI);
	const ECompanyType Industry = CGGI ? CGGI->GetCurrentBuildingCompanyType() : ECompanyType::None;

	for (int32 i = 0; i < 6; ++i)
	{
		// 요구하지 않는 직능은 줄에서 뺀다 — 이 카드에 필요한 팀만 보여야 비교가 성립한다
		if (W[i] <= 0)
		{
			continue;
		}

		UTeamPipItemWidget* Item = CreateWidget<UTeamPipItemWidget>(this, PipClass);
		if (!Item)
		{
			continue;
		}

		const int32 Total = TeamPointsBySlot.IsValidIndex(i) ? TeamPointsBySlot[i] : 0;
		Item->SetPips(TableMgr->GetDisciplineDisplayName(Industry, i), FMath::Min(5, FMath::RoundToInt(Total / 3.0f)));
		TeamWrap->AddChildToWrapBox(Item);
		TeamItems.Add(Item);
	}

	TeamWrap->SetVisibility(TeamItems.Num() > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UOfficeRequestCardWidget::ApplyExpanded()
{
	if (DetailSection)
	{
		// 상세는 표시 전용이라 펼쳐도 클릭을 삼키지 않는다
		DetailSection->SetVisibility(bExpanded ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (MoreButton)
	{
		MoreButton->SetButtonText(FText::FromString(bExpanded ? TEXT("접기") : TEXT("자세히")));
	}
}

void UOfficeRequestCardWidget::SetRecommended(bool bInRecommended)
{
	bRecommended = bInRecommended;

	// SetSlotData와 같은 순서 보호 — Construct 전에 오면 캐시만 하고 NativeConstruct가 적용
	if (IsConstructed())
	{
		ApplyRecommendedVisual();
	}
}

void UOfficeRequestCardWidget::ApplyRecommendedVisual()
{
	// 오버레이가 카드 클릭을 막으면 안 된다 — Visible 금지, HitTestInvisible 고정
	const ESlateVisibility Vis = bRecommended
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (Img_RecommendGlint) { Img_RecommendGlint->SetVisibility(Vis); }
	if (RecommendRibbon) { RecommendRibbon->SetVisibility(Vis); }
}

void UOfficeRequestCardWidget::OnAcceptClicked()
{
	OnCardSelected.Broadcast(SlotIndex);
}

void UOfficeRequestCardWidget::OnMoreClicked()
{
	SetExpanded(!bExpanded);
}

void UOfficeRequestCardWidget::OnViewEmployeesClicked()
{
	OnViewEmployeesRequested.Broadcast(CachedWorstSlot);
}
