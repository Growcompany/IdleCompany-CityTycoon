#include "UI/Panel/ProjectReportWidget.h"
#include "CommonTextBlock.h"
#include "UI/Element/Common/ConfirmCancelWidget.h"
#include "UI/Element/Common/DisciplineBarWidget.h"
#include "UI/Element/Cards/IconCardWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ProjectDataTable.h"
#include "Enum/QualityGrade.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

void UProjectReportWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->HideMessageText();
		ConfirmCancelWidget->SetCancelOnly(true);
	}
}

void UProjectReportWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmCancelWidget)
	{
		ConfirmCancelWidget->bAutoRemove = false;
		ConfirmCancelWidget->HideMessageText();
		// 결산서는 [닫기] 하나만 (2026-07-29 사용자 결정 — [다음 프로젝트] 제거)
		ConfirmCancelWidget->SetCancelOnly(true);

		// 재진입 시 중복 바인딩 가드 후 재바인딩
		ConfirmCancelWidget->OnCancel.RemoveAll(this);
		ConfirmCancelWidget->OnCancel.AddUObject(this, &UProjectReportWidget::OnDialogCancelled);
		ConfirmCancelWidget->OnConfirm.RemoveAll(this);
	}
}

void UProjectReportWidget::SetReportData(const FProjectReportData& InData)
{
	ReportData = InData;

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;

	// 프로젝트 이름
	if (Text_ProjectName)
	{
		Text_ProjectName->SetText(FText::FromString(InData.ProjectName));
	}

	// 프로젝트 이미지 카드 — 커버는 이미지 전용 (이름은 히어로 카드 Text_ProjectName 이 표시)
	if (UI_ProjectImageCard)
	{
		UI_ProjectImageCard->SetTextVisible(false);
		if (GI)
		{
			UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>();
			if (TableMgr)
			{
				bool bSuccess = false;
				FProjectData ProjectData = (InData.CompanyType != ECompanyType::None)
					? TableMgr->GetProjectData(InData.CompanyType, InData.ProjectNumber, bSuccess)
					: TableMgr->ResolveProjectData(InData.ProjectNumber, bSuccess);
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
	}

	// 품질 등급
	const FString GradeLetter = QualityGradeToAlphabetString(InData.QualityGrade);
	if (Text_QualityGrade)
	{
		Text_QualityGrade->SetText(FText::FromString(GradeLetter));
	}
	ApplyGradeStampColor(GradeLetterToColor(GradeLetter));

	// 품질 점수 — 배율 표기(×). 0.5~2.0 범위라 "점수 / 만점" 형태는 1.0 초과 시 거짓이 된다
	if (Text_QualityScore)
	{
		Text_QualityScore->SetText(FText::FromString(FString::Printf(TEXT("×%.2f"), InData.QualityScore)));
	}

	// 분야별 성과 — 축 고정 6칸(EProductionDiscipline 슬롯 순서). 비활성 분야는 빈 트랙으로
	// "요구 없음"을 보여 쌍둥이 출시확인 패널과 실루엣이 이어진다.
	{
		UDisciplineBarWidget* DiscBars[6] = { DiscBar1, DiscBar2, DiscBar3, DiscBar4, DiscBar5, DiscBar6 };

		UTableManagerSubsystem* DiscTableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;

		// 구 세이브의 pending report 는 슬롯 축이 없어 6칸이 전부 빈 트랙이 된다 — 조용히 지나가지 않게
		if (InData.DisciplineSlots.Num() != InData.DisciplineScores.Num())
		{
			UE_LOG(LogTemp, Warning, TEXT("[ProjectReport] DisciplineSlots(%d) != DisciplineScores(%d) — 분야 축 매핑 불가"),
				InData.DisciplineSlots.Num(), InData.DisciplineScores.Num());
		}

		for (int32 SlotIdx = 0; SlotIdx < UE_ARRAY_COUNT(DiscBars); ++SlotIdx)
		{
			if (!DiscBars[SlotIdx]) { continue; }

			// 라벨 SOT = DT_DisciplineDisplay — 매니저 스텝 표시명과 같은 출처(패키지 빌드 안전)
			const FText Name = DiscTableMgr
				? DiscTableMgr->GetDisciplineDisplayName(InData.CompanyType, SlotIdx) : FText::GetEmpty();

			// 결산 배열은 활성 직능만 압축돼 있다 — DisciplineSlots 로 원래 칸에 되흩뿌린다
			const int32 Packed = InData.DisciplineSlots.IndexOfByKey(SlotIdx);
			if (Packed != INDEX_NONE && InData.DisciplineScores.IsValidIndex(Packed))
			{
				// 배열들은 "병렬"이라는 규약만 있고 강제되진 않는다 — Targets 만 짧으면 크래시
				const float Target = InData.DisciplineTargets.IsValidIndex(Packed) ? InData.DisciplineTargets[Packed] : 0.0f;
				DiscBars[SlotIdx]->SetInfo(Name, InData.DisciplineScores[Packed], Target,
					UDisciplineBarWidget::GetLightWellSlotFill(SlotIdx));
			}
			else
			{
				DiscBars[SlotIdx]->SetEmpty(Name);
			}
		}
	}

	// 기본 수익 (초당 수익률, 콤마 포맷 + 소수점 최대 1자리)
	if (Text_RevenuePerSecond && InData.TotalOperationTime > 0.0f)
	{
		float AvgRevenuePerSec = InData.TotalRevenueEarned / InData.TotalOperationTime;
		FNumberFormattingOptions Opts;
		Opts.SetUseGrouping(true);
		// Min 0 = 정수면 소수점을 달지 않는다 ("15.0원/초" 의 .0 은 정보가 없다)
		Opts.SetMaximumFractionalDigits(1);
		Opts.SetMinimumFractionalDigits(0);
		Text_RevenuePerSecond->SetText(FText::FromString(FText::AsNumber(AvgRevenuePerSec, &Opts).ToString() + TEXT("원/초")));
	}

	// 총 수익 (실제 벌어들인 금액)
	if (Text_TotalRevenue)
	{
		int64 RevenueInt = static_cast<int64>(InData.TotalRevenueEarned);
		Text_TotalRevenue->SetText(FText::AsNumber(RevenueInt));
	}

	// 획득 시가총액 (프로젝트 완료 보상 — QualityScore × MarketCapMultiplier 배율 적용분)
	if (Text_MarketCapGained)
	{
		FNumberFormattingOptions Opts;
		Opts.SetUseGrouping(true);
		Text_MarketCapGained->SetText(FText::FromString(
			TEXT("+") + FText::AsNumber(InData.MarketCapGained, &Opts).ToString()));
	}

	// 총 운영 시간
	if (Text_OperationTime)
	{
		int32 TotalSeconds = FMath::FloorToInt(InData.TotalOperationTime);
		int32 Minutes = TotalSeconds / 60;
		int32 Seconds = TotalSeconds % 60;

		FString TimeStr;
		if (Minutes > 0)
		{
			TimeStr = FString::Printf(TEXT("%d분 %d초"), Minutes, Seconds);
		}
		else
		{
			TimeStr = FString::Printf(TEXT("%d초"), Seconds);
		}
		Text_OperationTime->SetText(FText::FromString(TimeStr));
	}
}

void UProjectReportWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateStampMaterialSize();
}

void UProjectReportWidget::UpdateStampMaterialSize()
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

void UProjectReportWidget::ApplyGradeStampColor(const FLinearColor& GradeColor)
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

void UProjectReportWidget::OnDialogCancelled()
{
	OnReportClosed.Broadcast();
	DeactivateWidget();
}

