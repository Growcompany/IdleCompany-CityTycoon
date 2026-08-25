// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Office/PitchTileWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// 판정 잉크 3색 — 카드 밴드(§5)와 같은 팔레트라 보드 안에서 색 언어가 갈리지 않는다
	const FLinearColor VerdictInkPass(0.22f, 0.58f, 0.40f, 1.f);
	const FLinearColor VerdictInkShort(0.72f, 0.26f, 0.24f, 1.f);
	const FLinearColor VerdictInkNeutral(0.49f, 0.55f, 0.62f, 1.f);

	const FName ChipTintParam(TEXT("TintCol"));
	const FName ChipWpxParam(TEXT("Wpx"));
	const FName ChipHpxParam(TEXT("Hpx"));
	const FName ChipThickParam(TEXT("ThickPx"));
	const FName ChipLineColParam(TEXT("LineCol"));

	// 선택 링 = 액센트 블루 #3D9BE0 3px. MIC 파라미터는 T3D 로 못 덮어 MID 로만 넣을 수 있다.
	const FLinearColor SelectRingColor(0.047f, 0.328f, 0.745f, 1.f);
	constexpr float SelectRingThickPx = 3.f;
}

void UPitchTileWidget::NativeConstruct()
{
	Super::NativeConstruct();

	EnsureChipMaterials();
}

void UPitchTileWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateChipMaterialSize(MyGeometry);
}

void UPitchTileWidget::NativeOnClicked()
{
	Super::NativeOnClicked();

	OnTileClicked.Broadcast(ProjectIndex);
}

void UPitchTileWidget::Configure(const FPitchTileData& InData)
{
	ProjectIndex = InData.ProjectIndex;
	EnsureChipMaterials();

	if (Text_Index)
	{
		// 진행표 식별자라 자릿수 구분 콤마가 붙으면 안 된다
		FNumberFormattingOptions IndexFormat;
		IndexFormat.SetUseGrouping(false);
		Text_Index->SetText(FText::Format(INVTEXT("#{0}"), FText::AsNumber(ProjectIndex, &IndexFormat)));
	}

	if (Text_Name)
	{
		Text_Name->SetText(InData.ProjectName);
	}

	if (Text_Verdict)
	{
		Text_Verdict->SetText(InData.VerdictWord);
	}

	UTexture2D* CoverTexture = InData.Cover.IsNull() ? nullptr : InData.Cover.LoadSynchronous();
	if (CoverImage)
	{
		if (CoverTexture)
		{
			CoverImage->SetBrushFromTexture(CoverTexture);
		}
		CoverImage->SetVisibility(CoverTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (CoverFill)
	{
		// 커버 없는 프로젝트는 장르색 딥 플레이트로 대신 채운다 (검은 구멍 방지)
		CoverFill->SetBrushColor(FLinearColor(InData.GenreColor.R * 0.52f, InData.GenreColor.G * 0.52f, InData.GenreColor.B * 0.52f, 1.f));
		CoverFill->SetVisibility(CoverTexture ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (Img_Trend)
	{
		Img_Trend->SetVisibility(InData.bTrend ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Text_Grade)
	{
		Text_Grade->SetText(FText::FromString(InData.DevelopedGrade));
		Text_Grade->SetVisibility(InData.DevelopedGrade.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	const bool bTileLocked = (InData.Verdict == PitchBoardText::ETileVerdict::Locked);
	if (Text_Lock)
	{
		Text_Lock->SetText(InData.LockText.IsEmpty() ? InData.VerdictWord : InData.LockText);
	}
	if (LockShade)
	{
		LockShade->SetVisibility(bTileLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (RecommendRibbon)
	{
		RecommendRibbon->SetVisibility(InData.bRecommended ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (RecommendRing)
	{
		RecommendRing->SetVisibility(InData.bRecommended ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	ApplyVerdictVisual(InData.Verdict);

	// 재사용 타일이 잠금 상태로 남지 않게 매 Configure 에서 되돌린다
	SetIsEnabled(!bTileLocked);
}

void UPitchTileWidget::SetSelected(bool bInSelected)
{
	if (SelectRing)
	{
		SelectRing->SetVisibility(bInSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UPitchTileWidget::ApplyVerdictVisual(PitchBoardText::ETileVerdict Verdict)
{
	FLinearColor Tint = TileTintNeutral;
	FLinearColor Ink = VerdictInkNeutral;
	if (Verdict == PitchBoardText::ETileVerdict::Pass)
	{
		Tint = TileTintPass;
		Ink = VerdictInkPass;
	}
	else if (Verdict == PitchBoardText::ETileVerdict::Short)
	{
		Tint = TileTintShort;
		Ink = VerdictInkShort;
	}
	// Wait(로스터 0)·Redevelop·Locked·Preview 는 전부 뉴트럴 — 판정을 못 냈거나 판정 축이 아닌 칸에 의미색을 얹지 않는다

	if (Text_Verdict)
	{
		Text_Verdict->SetColorAndOpacity(FSlateColor(Ink));
	}
	if (Dot)
	{
		Dot->SetColorAndOpacity(Ink);
	}

	// SDF 칩 계약 = 채움색은 TintCol, 알파는 Image ColorAndOpacity — 둘을 쪼개야 판정 워시가 의도한 농도로 얹힌다.
	// 단 TintCol 이 없는 머티리얼이면 알파만 깎여 배경이 통째로 증발하므로 파라미터 유무를 먼저 묻는다.
	if (ChipBGMID)
	{
		FLinearColor ProbeColor;
		const bool bHasTint = ChipBGMID->GetVectorParameterValue(FMaterialParameterInfo(ChipTintParam), ProbeColor);
		if (bHasTint)
		{
			ChipBGMID->SetVectorParameterValue(ChipTintParam, FLinearColor(Tint.R, Tint.G, Tint.B, 1.f));
			if (ChipBG)
			{
				ChipBG->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, Tint.A));
			}
		}
		else
		{
			if (ChipBG)
			{
				ChipBG->SetColorAndOpacity(FLinearColor::White);
			}
			static bool bTintParamWarned = false;
			if (!bTintParamWarned)
			{
				bTintParamWarned = true;
				UE_LOG(LogTemp, Warning,
					TEXT("[PitchTile] ChipBG 머티리얼에 벡터 파라미터 '%s' 가 없다 — 판정 틴트를 건너뛰고 칩 배경을 원본으로 유지한다"),
					*ChipTintParam.ToString());
			}
		}
	}
}

void UPitchTileWidget::EnsureChipMaterials()
{
	// GetDynamicMaterial = 브러시 머티리얼로 MID 생성 후 브러시에 되꽂기 (UResourceWidget 과 같은 경로)
	if (!ChipBGMID && ChipBG)
	{
		ChipBGMID = ChipBG->GetDynamicMaterial();
	}
	if (!ChipLineMID && ChipLine)
	{
		ChipLineMID = ChipLine->GetDynamicMaterial();
	}
	if (!RecommendRingMID && RecommendRing)
	{
		RecommendRingMID = RecommendRing->GetDynamicMaterial();
	}
	if (!SelectRingMID && SelectRing)
	{
		SelectRingMID = SelectRing->GetDynamicMaterial();
		if (SelectRingMID)
		{
			// 선택 링은 칩 키라인과 같은 MIC(1.5px 베이크)를 재사용한다 — 두께 3px 은 여기서만 결정된다
			SelectRingMID->SetScalarParameterValue(ChipThickParam, SelectRingThickPx);

			FLinearColor ProbeColor;
			if (SelectRingMID->GetVectorParameterValue(FMaterialParameterInfo(ChipLineColParam), ProbeColor))
			{
				SelectRingMID->SetVectorParameterValue(ChipLineColParam, SelectRingColor);
				// 머티리얼이 색을 들었으면 위젯 틴트는 중립으로 — 양쪽 다 파랑이면 곱해져 어두워진다
				SelectRing->SetColorAndOpacity(FLinearColor::White);
			}
		}
	}
}

void UPitchTileWidget::UpdateChipMaterialSize(const FGeometry& MyGeometry)
{
	if (!ChipBGMID && !ChipLineMID && !SelectRingMID && !RecommendRingMID)
	{
		return;
	}

	// SDF 머티리얼은 Wpx/Hpx 가 실제 위젯 크기와 일치해야 코너가 정합 — 크기 변화 시에만 재주입
	// (ChipBG/ChipLine 은 루트 Overlay 를 Fill 하므로 위젯 크기 = 칩 크기)
	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	if (LocalSize.X < 1.f || LocalSize.Y < 1.f || LocalSize.Equals(LastChipMatSize, 0.5f))
	{
		return;
	}
	LastChipMatSize = LocalSize;

	for (UMaterialInstanceDynamic* ChipMID : { ChipBGMID.Get(), ChipLineMID.Get(), SelectRingMID.Get(), RecommendRingMID.Get() })
	{
		if (!ChipMID)
		{
			continue;
		}
		ChipMID->SetScalarParameterValue(ChipWpxParam, LocalSize.X);
		ChipMID->SetScalarParameterValue(ChipHpxParam, LocalSize.Y);
	}
}
