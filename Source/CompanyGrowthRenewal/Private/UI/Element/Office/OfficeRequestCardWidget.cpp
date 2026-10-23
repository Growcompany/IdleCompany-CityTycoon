#include "UI/Element/Office/OfficeRequestCardWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Components/ProgressBar.h"
#include "Data/StageProgressData.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Common/StatRowWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Global/GlobalUtilFunctions.h"
#include "Core/CGGameInstance.h"
#include "Enum/CompanyType.h"
#include "Enum/QualityGrade.h"
#include "UI/Element/Common/QualityGradeMedalUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/PitchBoardText.h"
#include "UI/Element/Common/ItemTooltipWidget.h"
#include "Enum/WidgetType.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"

void UOfficeRequestCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (AcceptButton)
	{
		AcceptButton->OnClicked().AddUObject(this, &UOfficeRequestCardWidget::OnAcceptClicked);
	}

	// 6칸 플레이트 탭 = 직능별 예상/요구 팝오버 툴팁. UBorder 는 OnMouseButtonDownEvent 를 public 멤버로 노출 — 직접 바인딩.
	if (DevPlate)
	{
		DevPlate->OnMouseButtonDownEvent.BindDynamic(this, &UOfficeRequestCardWidget::HandleDevPlateClicked);
	}

	// 캐시된 데이터가 있으면 적용 (CreateWidget 후 SetSlotData가 먼저 호출된 경우)
	if (bHasCachedData)
	{
		ApplySlotData();
	}

	ApplyRecommendedVisual();
}

void UOfficeRequestCardWidget::NativeDestruct()
{
	if (AcceptButton)
	{
		AcceptButton->OnClicked().RemoveAll(this);
	}
	if (DevPlate)
	{
		DevPlate->OnMouseButtonDownEvent.Unbind();
	}
	if (ActiveTooltip)
	{
		ActiveTooltip->Hide();
	}
	Super::NativeDestruct();
}

void UOfficeRequestCardWidget::SetSlotData(const FProjectBoardSlot& SlotData, int32 InSlotIndex, bool bInHasShipped)
{
	SlotIndex = InSlotIndex;
	CachedSlotData = SlotData;
	bHasShipped = bInHasShipped;
	bHasCachedData = true;

	// NativeConstruct가 이미 호출된 상태면 즉시 적용
	if (IsConstructed())
	{
		ApplySlotData();
	}
}

void UOfficeRequestCardWidget::SetReleaseGrade(ECompanyType InCompanyType, EQualityGrade InGrade)
{
	ReleaseCompanyType = InCompanyType;
	ReleaseGrade = InGrade;

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

UWidget* UOfficeRequestCardWidget::GetPrimaryCtaWidget() const
{
	// 대기(로스터 0)엔 착수 CTA 자체가 없다 — 보일 때만 앵커, 아니면 없음
	if (AcceptButton && AcceptButton->GetVisibility() == ESlateVisibility::Visible)
	{
		return AcceptButton;
	}
	return nullptr;
}

void UOfficeRequestCardWidget::ApplySlotData()
{
	ApplyPitchPresentation(CachedSlotData);
}

void UOfficeRequestCardWidget::ApplyPitchPresentation(const FProjectBoardSlot& SlotData)
{
	FDisciplineOutlook Outlook;
	ComputeDisciplineOutlook(SlotData, Outlook);
	// 값 표기 토글(플레이트 탭)이 재계산 없이 셀 텍스트만 갈아끼우도록 계산 한 벌을 캐시
	CachedOutlook = Outlook;
	const bool bOutlookUsable = Outlook.bOutlookUsable;

	// 미출시 커버 모자이크 — 파라미터 기본값(가로 48셀 = CellPx 12.083 / Bright 0.75)을 그대로 쓴다.
	// 튜닝 = Tools/UIMaterial/make_cover_mosaic.py 재실행 (마스터 기본값 세터가 없어 재생성이 유일 경로, 웰 580x410이 상수로 박혀 있음).
	if (CoverImage)
	{
		UTexture2D* CoverTex = SlotData.Cover.IsNull() ? nullptr : SlotData.Cover.LoadSynchronous();

		if (CoverTex && !bHasShipped && !CoverMID)
		{
			UMaterialInterface* MosaicMat = TSoftObjectPtr<UMaterialInterface>(
				FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Materials/M_UI_CoverMosaic.M_UI_CoverMosaic"))).LoadSynchronous();
			if (MosaicMat)
			{
				CoverMID = UMaterialInstanceDynamic::Create(MosaicMat, this);
			}
		}

		if (CoverTex && bHasShipped)
		{
			CoverImage->SetBrushFromTexture(CoverTex);
			CoverImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else if (CoverTex && CoverMID)
		{
			CoverMID->SetTextureParameterValue(FName("Cover"), CoverTex);
			CoverImage->SetBrushFromMaterial(CoverMID);
			CoverImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			// 커버 결손·머티리얼 부재 → 아래 WellFill 장르색이 그대로 노출된다
			CoverImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (UWidget* UnknownMark = GetWidgetFromName(TEXT("Text_Well")))
		{
			UnknownMark->SetVisibility(bHasShipped && CoverTex ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
		if (UWidget* UndiscoveredLabel = GetWidgetFromName(TEXT("Undisc")))
		{
			UndiscoveredLabel->SetVisibility(bHasShipped ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
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

	// 장르 · 소재
	if (Text_Tagline)
	{
		FString TagLine = FString::Printf(TEXT("%s · %s"),
			*SlotData.Genre.ToString(), *SlotData.Material.ToString());
		if (SlotData.bHasUnmodeledEquipmentEffects) TagLine += TEXT("\n예상치: 조건부 장비 효과 미포함");
		Text_Tagline->SetText(FText::FromString(TagLine));
		Text_Tagline->SetVisibility(ESlateVisibility::Visible);
	}

	const bool bShowReleaseGrade = ReleaseGrade != EQualityGrade::Max;
	if (bShowReleaseGrade && ReleaseGradePlate && Text_ReleaseGrade)
	{
		Text_ReleaseGrade->SetText(FText::FromString(QualityGradeToAlphabetString(ReleaseGrade)));
		CGQualityGradeMedal::Apply(GetGameInstance(), ReleaseCompanyType, ReleaseGrade,
			ReleaseGradePlate, Text_ReleaseGrade);
	}
	else
	{
		if (ReleaseGradePlate)
		{
			ReleaseGradePlate->SetBrushFromMaterial(nullptr);
			ReleaseGradePlate->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (Text_ReleaseGrade)
		{
			Text_ReleaseGrade->SetText(FText::GetEmpty());
			Text_ReleaseGrade->SetVisibility(ESlateVisibility::Collapsed);
			FSlateFontInfo EmptyGradeFont = Text_ReleaseGrade->GetFont();
			EmptyGradeFont.FontMaterial = nullptr;
			Text_ReleaseGrade->SetFont(EmptyGradeFont);
		}
	}

	if (ReleaseGradeHost)
	{
		const bool bGradeReady = bShowReleaseGrade && ReleaseGradePlate && Text_ReleaseGrade
			&& ReleaseGradePlate->GetVisibility() != ESlateVisibility::Collapsed
			&& Text_ReleaseGrade->GetVisibility() != ESlateVisibility::Collapsed;
		ReleaseGradeHost->SetVisibility(bGradeReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 축 라벨 = 산업별 직능 DT — 현재 진입 건물의 산업이 곧 이 피치의 산업
	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	UCGGameInstance* CGGI = Cast<UCGGameInstance>(GI);
	const ECompanyType Industry = CGGI ? CGGI->GetCurrentBuildingCompanyType() : ECompanyType::None;

	// ── 상시 6칸 전망 (카드 본문) ──
	// 값 = 기준 대비 달성률(%), 바 = 달성률 0~100% 클램프, 색 = 통과/미달. 축 순서는 EProductionDiscipline 고정.
	// 정확한 예상/요구 원점수는 6칸 위에 마우스를 올리면 툴팁(SetToolTipText)으로 뜬다 → 호버 받게 Visible.
	if (DevPlate)
	{
		DevPlate->SetVisibility(ESlateVisibility::Visible);
	}

	UProgressBar* Bars[6] = { DevBar1, DevBar2, DevBar3, DevBar4, DevBar5, DevBar6 };
	UCommonTextBlock* Vals[6] = { Text_DevVal1, Text_DevVal2, Text_DevVal3, Text_DevVal4, Text_DevVal5, Text_DevVal6 };
	UCommonTextBlock* Names[6] = { Text_DevName1, Text_DevName2, Text_DevName3, Text_DevName4, Text_DevName5, Text_DevName6 };

	TArray<FString> TipLines;
	TipLines.Add(TEXT("직원 점수 = 레벨 출력 × 업무속도 × 직능 효과 × 장비·건물"));
	TipLines.Add(TEXT("필요한 직능 중 어피니티 비율로 무작위 선택"));
	TipLines.Add(TEXT("피로 감산 · 졸음/이탈 중 0점 · 크리티컬 반영"));
	TipLines.Add(TEXT("표시 점수 = 직능별 기대 합계 − 변동 1표준편차"));
	TipLines.Add(TEXT("출시 = 모든 필요 직능이 목표의 50% 이상"));
	TipLines.Add(TEXT(""));
	for (int32 i = 0; i < 6; ++i)
	{
		// 요구(가중치>0) 칸만 값을 채우고, 비요구 칸은 딤 처리해 이 카드가 안 쓰는 분야임을 흐리게 보인다
		const bool bActive = Outlook.Weights[i] > 0;
		const float Target = Outlook.Targets.IsValidIndex(i) ? Outlook.Targets[i] : 0.0f;
		const float Score = Outlook.Scores.IsValidIndex(i) ? Outlook.Scores[i] : 0.0f;

		float Ratio = 0.0f;
		FLinearColor CellColor = OutlookInactiveColor;
		if (bOutlookUsable && bActive && Target > 0.0f)
		{
			Ratio = Score / Target;
			CellColor = Outlook.Colors.IsValidIndex(i) ? Outlook.Colors[i] : OutlookPassColor;
		}

		if (Bars[i])
		{
			Bars[i]->SetPercent(FMath::Clamp(Ratio, 0.0f, 1.0f));
			Bars[i]->SetFillColorAndOpacity(CellColor);
		}
		if (Vals[i])
		{
			// 값 = 기준 대비 달성률(%) = 팀 예상점수 ÷ 프로젝트 요구점수. 100%면 요구 충족, 그 이상은 여유. 비요구·전망없음 칸은 줄표.
			Vals[i]->SetText(FormatOutlookCellValue(i));
			Vals[i]->SetColorAndOpacity(FSlateColor(CellColor));
		}

		// 축 라벨의 단일 출처 = DT_DisciplineDisplay (매니저 스텝 표시명과 같은 출처라 둘이 갈리지 않는다)
		const FText DiscName = TableMgr ? TableMgr->GetDisciplineDisplayName(Industry, i) : FText::GetEmpty();
		if (Names[i])
		{
			Names[i]->SetText(DiscName);
			Names[i]->SetColorAndOpacity(FSlateColor(CellColor));
		}

		if (bOutlookUsable && bActive && Target > 0.0f)
		{
			TipLines.Add(FString::Printf(TEXT("%s  전망 %.1f / 최소 %.1f / 목표 %.1f (%d%%)"),
				*DiscName.ToString(), Score, Target * FStageProgressData::MinimumScoreRatio, Target,
				FStageProgressData::ComputeDisciplinePercentDisplayValue(Score / Target)));
		}
	}

	if (SlotData.bHasUnmodeledEquipmentEffects) TipLines.Add(TEXT("조건부 장비 효과는 예상 점수·등급·수익에 포함되지 않습니다."));
	// 6칸 탭 팝오버 툴팁용 설명 = 활성 직능별 예상/요구 원점수. HandleDevPlateClicked 가 ShowAt 에 넘긴다.
	DisciplineTooltipDesc = TipLines.Num() > 0
		? FText::FromString(FString::Join(TipLines, TEXT("\n")))
		: FText::GetEmpty();

	// 구 난이도 배지 — 현재 미사용, 바인딩돼 있으면 접어 숨긴다
	if (DiffBadge)
	{
		DiffBadge->SetVisibility(ESlateVisibility::Collapsed);
	}

	// ── 수익/운영 스탯행 (UIE_StatRow) — 이름은 WBP DefaultStatName, 값만 주입 후 노출 ──
	if (Text_OperationTime)
	{
		const int32 Sec = SlotData.EstimatedOpTimeSec;
		const FString Dur = (Sec < 60)
			? FString::Printf(TEXT("%d초"), Sec)
			: ((Sec % 60 == 0) ? FString::Printf(TEXT("%d분"), Sec / 60)
			                   : FString::Printf(TEXT("%.1f분"), Sec / 60.0f));
		Text_OperationTime->SetStatValue(Dur);
		Text_OperationTime->SetVisibility(ESlateVisibility::Visible);
	}
	if (Text_RevenueRange)
	{
		// 대기(전망 없음)엔 수익을 못 낸다 — 값 대신 "?"
		const FString RevStr = bOutlookUsable
			? UGlobalUtilFunctions::AbbreviateNumber(SlotData.EstimatedRevenue, ENumberAbbrevStyle::Auto, ENumberRoundMode::Ceil).ToString() + TEXT("원")
			: FString(TEXT("?"));
		Text_RevenueRange->SetStatValue(RevStr);
		Text_RevenueRange->SetVisibility(ESlateVisibility::Visible);
	}

	// CTA 3상태를 한 버튼에 — 대기(로스터 0)엔 착수 CTA 를 감춘다. 통과/미달은 CTA 색으로만 판정을 보인다
	const bool bWait = !bOutlookUsable;
	const bool bFail = bOutlookUsable && SlotData.bGateRisk;
	bCtaGateRisk = bFail;
	if (AcceptButton)
	{
		if (bWait)
		{
			AcceptButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			// 미달도 착수는 가능 — 라벨+스타일만 회색 경고로 바꾼다
			AcceptButton->SetButtonText(FText::FromString(bFail ? TEXT("그래도 개발") : TEXT("개발 시작")));
			ApplyAcceptStyle(bFail ? AcceptStyle_Fail : AcceptStyle_Pass);
			AcceptButton->SetVisibility(ESlateVisibility::Visible);
		}
	}

	// 미달 사유 캡션 — 미달일 때만. 최저 달성률 직능을 카드 자신의 Outlook(Scores/Targets)에서 뽑아 문장으로.
	if (Text_ShortageReason)
	{
		int32 WorstSlot = INDEX_NONE;
		float WorstRatio = 0.0f;
		const bool bFound = bFail
			&& PitchBoardText::FindWorstDiscipline(Outlook.Scores, Outlook.Targets, WorstSlot, WorstRatio);
		const FText WorstName = (bFound && TableMgr)
			? TableMgr->GetDisciplineDisplayName(Industry, WorstSlot) : FText::GetEmpty();
		if (bFail && !WorstName.IsEmpty())
		{
			// 앞에 대시 프리픽스 — 태그라인 아래 사유를 목록처럼 보이게(사용자 지시).
			Text_ShortageReason->SetText(FText::Format(
				NSLOCTEXT("PitchBoard", "ShortageDash", "- {0}"),
				PitchBoardText::MakeShortageSentence(WorstName)));
			Text_ShortageReason->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			Text_ShortageReason->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UOfficeRequestCardWidget::ComputeDisciplineOutlook(const FProjectBoardSlot& SlotData, FDisciplineOutlook& Out) const
{
	// 칸 인덱스가 곧 EProductionDiscipline 슬롯이라 정렬하지 않는다
	Out.Weights[0] = SlotData.Weight_Plan;
	Out.Weights[1] = SlotData.Weight_Dev;
	Out.Weights[2] = SlotData.Weight_Graphics;
	Out.Weights[3] = SlotData.Weight_Sound;
	Out.Weights[4] = SlotData.Weight_Server;
	Out.Weights[5] = SlotData.Weight_QA;

	bool bUsable = SlotData.bHasOutlookEstimate
		&& SlotData.ExpectedDisciplineScores.Num() == 6
		&& FMath::IsFinite(SlotData.ExpectedQuality);
	if (bUsable)
	{
		for (const float ExpectedScore : SlotData.ExpectedDisciplineScores)
		{
			if (!FMath::IsFinite(ExpectedScore))
			{
				bUsable = false;
				break;
			}
		}
	}
	Out.bOutlookUsable = bUsable;

	int32 MaxW = 0;
	for (int32 i = 0; i < 6; ++i)
	{
		MaxW = FMath::Max(MaxW, Out.Weights[i]);
	}

	const float TargetBase = FStageProgressData::ComputeDisciplineTargetBase(
		SlotData.RequiredScore_Step1, SlotData.RequiredScore_Step2, SlotData.RequiredScore_Step3);

	Out.Targets.Reserve(6);
	Out.Scores.Reserve(6);
	for (int32 i = 0; i < 6; ++i)
	{
		Out.Targets.Add(FStageProgressData::ComputeDisciplineTarget(Out.Weights[i], MaxW, TargetBase));
		Out.Scores.Add(SlotData.ExpectedDisciplineScores.IsValidIndex(i) ? SlotData.ExpectedDisciplineScores[i] : 0.0f);
	}

	// 직능별 통과/미달 색 — 6칸이 공유. 비활성·전망없음은 통과색으로 두고, 표현 단계에서 딤 색으로 덮는다
	Out.Colors.Init(OutlookPassColor, 6);
	for (int32 i = 0; i < 6; ++i)
	{
		const float Target = Out.Targets[i];
		if (bUsable && Target > 0.0f)
		{
			const float Ratio = Out.Scores[i] / Target;
			// 게이트 하한(50%)이 유일한 경계 — 그 사이 앰버 구간은 "부족한데 빨갛지 않다"를 만들어 폐기
			Out.Colors[i] = (Ratio < FStageProgressData::MinimumScoreRatio) ? OutlookShortColor : OutlookPassColor;
		}
	}
}

FText UOfficeRequestCardWidget::FormatOutlookCellValue(int32 i) const
{
	if (!CachedOutlook.Targets.IsValidIndex(i))
	{
		return FText::FromString(TEXT("―"));
	}
	const bool bActive = CachedOutlook.Weights[i] > 0;
	const float Target = CachedOutlook.Targets[i];
	const float Score = CachedOutlook.Scores.IsValidIndex(i) ? CachedOutlook.Scores[i] : 0.0f;

	// 전망 없음·비요구·목표 0 은 비교 기준 자체가 없으므로 줄표(0% 로 오해되지 않게)
	if (!CachedOutlook.bOutlookUsable || !bActive || Target <= 0.0f)
	{
		return FText::FromString(TEXT("―"));
	}

	// 기준 대비 달성률 — 100% 면 요구 충족, 초과는 여유. 정확 예상/요구는 6칸 탭 팝오버 툴팁에서.
	const int32 Pct = FMath::RoundToInt((Score / Target) * 100.0f);
	return FText::FromString(FString::Printf(TEXT("%d%%"), Pct));
}

FEventReply UOfficeRequestCardWidget::HandleDevPlateClicked(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	// 6칸 탭 → 직능별 예상/요구 팝오버(UIE_ItemTooltip 재사용, 누른 지점 아래). 정확 수치 확인용.
	if (DisciplineTooltipDesc.IsEmpty())
	{
		return FEventReply(true);
	}
	if (!ActiveTooltip)
	{
		UGameInstance* GI = GetGameInstance();
		UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		const TSubclassOf<UUserWidget> TooltipClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::ItemTooltip) : nullptr;
		if (TooltipClass)
		{
			ActiveTooltip = CreateWidget<UItemTooltipWidget>(this, TooltipClass);
		}
	}
	if (ActiveTooltip)
	{
		const FVector2D AbsPos = MouseEvent.GetScreenSpacePosition();
		ActiveTooltip->ShowAt(AbsPos,
			NSLOCTEXT("PitchBoard", "DiscTipTitle", "팀 업무력 계산"),
			nullptr, DisciplineTooltipDesc, 0, 5.0f, ETooltipAnchor::BelowAnchor);
	}
	return FEventReply(true);
}

void UOfficeRequestCardWidget::ApplyAcceptStyle(const TSoftClassPtr<UCommonButtonStyle>& StylePtr)
{
	if (!AcceptButton || StylePtr.IsNull())
	{
		return;
	}
	if (UClass* StyleClass = StylePtr.LoadSynchronous())
	{
		AcceptButton->SetStyle(TSubclassOf<UCommonButtonStyle>(StyleClass));
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
	if (RecommendBadge) { RecommendBadge->SetVisibility(Vis); }
}

void UOfficeRequestCardWidget::OnAcceptClicked()
{
	OnCardSelected.Broadcast(SlotIndex);
}
