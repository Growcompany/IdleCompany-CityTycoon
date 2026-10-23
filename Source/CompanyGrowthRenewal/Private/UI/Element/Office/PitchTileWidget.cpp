// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Office/PitchTileWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Enum/QualityGrade.h"
#include "UI/Element/Common/QualityGradeMedalUtils.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "Manager/TableManagerSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Utils/UIVFXTextureUtils.h"
#include "Utils/FWidgetAnimationUtils.h"

#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

namespace
{
	constexpr float RecommendationIntroDuration = 0.2f;
	constexpr float RecommendationCycleDuration = 3.f;
	constexpr float RecommendationShineStart = 0.3f;
	constexpr float RecommendationShineDuration = 0.65f;
}

void UPitchTileWidget::NativeOnClicked()
{
	Super::NativeOnClicked();

	OnTileClicked.Broadcast(ProjectIndex);
}

void UPitchTileWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 타일 전체를 덮는 Plate(Border)가 Visible 이라 CommonButtonBase 클릭 표면을 가로채 클릭이 안 먹던 버그.
	// 이 타일은 비-버튼 위젯(UIE_PortfolioCell)에서 복제돼 와 덮개 콘텐츠가 히트테스트로 남아 있었다 —
	// 작동하는 CommonButtonBase 카드들의 패턴대로 덮개를 히트 불가로 내려 버튼이 클릭을 수신하게 한다.
	if (Plate)
	{
		// HitTestInvisible 는 자식까지 히트 불가로 내린다 ― 타일 안엔 상호작용 자식이 없어(전부 표시용)
		// 덮개 전체를 히트에서 빼도 안전하고, 버튼이 모든 클릭을 확실히 받는다.
		Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	// 선택 글로우도 장식이라 히트 불가로 강제(WBP 기본이 Visible 이면 클릭을 다시 가로챈다).
	if (SelectedGlow)
	{
		SelectedGlow->SetVisibility(bTileSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	const UGameInstance* TileGameInstance = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = TileGameInstance ? TileGameInstance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	bRecommendShineReady = RecommendShine && FUIVFXTextureUtils::SetTextureKeepStyle(TableMgr, TEXT("CTAShine"), RecommendShine);
	bRecommendSparkleReady = RecommendSparkle && FUIVFXTextureUtils::SetTextureKeepStyle(TableMgr, TEXT("SparkleMain"), RecommendSparkle);
	bRecommendGlowReady = RecommendGlow && FUIVFXTextureUtils::SetTextureKeepStyle(TableMgr, TEXT("LevelGlow"), RecommendGlow);
	RefreshRecommendation(true);
}

void UPitchTileWidget::NativeDestruct()
{
	ResetRecommendationVisuals();
	bRecommendShineReady = false;
	bRecommendSparkleReady = false;
	bRecommendGlowReady = false;
	Super::NativeDestruct();
}

void UPitchTileWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bRecommendationActive)
	{
		UpdateRecommendationVisuals(InDeltaTime);
	}
}

void UPitchTileWidget::Configure(const FPitchTileData& InData)
{
	const bool bNextRecommended = InData.bRecommended && InData.Verdict != PitchBoardText::ETileVerdict::Locked;
	const bool bRestartRecommendation = bNextRecommended && (!bRecommended || ProjectIndex != InData.ProjectIndex);
	bRecommended = bNextRecommended;
	ProjectIndex = InData.ProjectIndex;
	CurrentVerdict = InData.Verdict;
	bTileHasShipped = InData.bHasShipped;
	const bool bIsLocked = (InData.Verdict == PitchBoardText::ETileVerdict::Locked);

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
	if (Text_Combo)
	{
		Text_Combo->SetText(InData.Combo);
		Text_Combo->SetVisibility(InData.bManufacturing || InData.Combo.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (ManufacturingCoverFrame)
	{
		ManufacturingCoverFrame->SetVisibility(InData.bManufacturing
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UWidget* MetadataRow = GetWidgetFromName(TEXT("MetaRow")))
	{
		MetadataRow->SetVisibility(InData.bManufacturing
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	// 이력 행과 같은 출시 기록을 사용한다. 회사별 티어 클리어 여부와 섞지 않는다.
	if (CoverImage)
	{
		UTexture2D* CoverTex = InData.Cover.IsNull() ? nullptr : InData.Cover.LoadSynchronous();
		if (CoverTex)
		{
#if WITH_EDITOR
			// 컴파일 중 기본 텍스처 치수 대신 실제 표지 치수를 사용한다.
			FTextureCompilingManager::Get().FinishCompilation({ CoverTex });
#endif
			// Configure는 Slate 생성 전에도 호출되므로 크기를 브러시에 보존한다.
			FSlateBrush CoverBrush = CoverImage->GetBrush();
			CoverBrush.SetImageSize(FVector2D(CoverTex->GetSizeX(), CoverTex->GetSizeY()));
			CoverImage->SetBrush(CoverBrush);
		}
		if (!CoverTex)
		{
			// 표지가 없으면 재사용 타일에 이전 표지가 남지 않도록 접는다.
			CoverImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (InData.bHasShipped || InData.bManufacturing)
		{
			// 이미 만든 것 → 원본 커버 선명하게(모자이크 없음). 브러시 DrawAs(RoundedBox)는 보존.
			ApplyCoverTexture(CoverTex, InData.bManufacturing && ProductBandSwitcher != nullptr);
			CoverImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			// 미개발 = 미지의 작품 → 카드와 같은 모자이크 + 더 어둡게. 타일 커버가 작아 CellPx 를 키워 굵은 셀.
			if (!CoverMID)
			{
				UMaterialInterface* MosaicMat = TSoftObjectPtr<UMaterialInterface>(
					FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Materials/M_UI_CoverMosaic.M_UI_CoverMosaic"))).LoadSynchronous();
				if (MosaicMat)
				{
					CoverMID = UMaterialInstanceDynamic::Create(MosaicMat, this);
					CoverMID->SetScalarParameterValue(FName("CellPx"), 29.0f);   // 카드 12px 대비 굵은 ~20열
					CoverMID->SetScalarParameterValue(FName("Bright"), 0.25f);   // 선형 밝기를 낮춰 출시 원본 커버와 구분한다.
				}
			}
			if (CoverMID)
			{
				CoverMID->SetTextureParameterValue(FName("Cover"), CoverTex);
				// 개발→미개발 재사용 시 텍스처 브러시를 다시 모자이크 재질로 되돌린다
				CoverImage->SetBrushFromMaterial(CoverMID);
				CoverImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				CoverImage->SetBrushFromTexture(CoverTex);   // 재질 로드 실패 → 원본이라도(검은 구멍 방지)
				CoverImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
	}

	ApplyReleaseHistory(InData);
	ApplyProductBand(InData);
	RefreshRecommendation(bRestartRecommendation);
	ApplyPlateColor();

	// 재사용 타일이 잠금 상태로 남지 않게 매 Configure 에서 되돌린다
	SetIsEnabled(!bIsLocked);
}

void UPitchTileWidget::SetSelected(bool bInSelected)
{
	bTileSelected = bInSelected;
	ApplyPlateColor();
}

void UPitchTileWidget::ApplyPlateColor()
{
	if (Plate)
	{
		Plate->SetBrushColor(ResolvePlateColor());
	}
	// RoundedBox의 독립 외곽선 알파도 선택 해제 시 함께 숨긴다.
	if (SelectedGlow)
	{
		SelectedGlow->SetVisibility(bTileSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		SelectedGlow->SetRenderOpacity(bTileSelected ? 1.0f : 0.0f);
	}
}

FLinearColor UPitchTileWidget::ResolvePlateColor() const
{
	// 선택은 SelectGlow 가 담당하므로 플레이트를 풀블루로 덮지 않는다(파랑 과다 방지) — 판정 워시를 그대로.
	if (CurrentVerdict == PitchBoardText::ETileVerdict::Locked)
	{
		return PlateMutedColor;
	}
	// 제품 타일의 판은 WBP 가 구운 밝은 표면 ― 틴트하지 않는다(상태는 띠가 말한다).
	if (ProductBandSwitcher)
	{
		return FLinearColor::White;
	}

	FLinearColor Wash = TileTintNeutral;
	if (CurrentVerdict == PitchBoardText::ETileVerdict::Pass)
	{
		Wash = TileTintPass;
	}
	else if (CurrentVerdict == PitchBoardText::ETileVerdict::Short)
	{
		Wash = TileTintShort;
	}
	// 워시는 낮은 알파 오버레이 — 출시 이력에 따른 크림색 위에 섞는다.
	const FLinearColor BaseColor = bTileHasShipped ? PlateBaseColor : PlateUnreleasedColor;
	return FMath::Lerp(BaseColor, FLinearColor(Wash.R, Wash.G, Wash.B, 1.f), Wash.A);
}

void UPitchTileWidget::ApplyReleaseHistory(const FPitchTileData& InData)
{
	const bool bShowStatus = InData.Verdict != PitchBoardText::ETileVerdict::Locked;
	const bool bShowHistory = bShowStatus && InData.bHasShipped;
	const bool bShowProjectStatus = bShowStatus && !InData.bManufacturing;
	const FLinearColor StampColor = InData.bHasShipped ? DevelopedStampColor : UndevelopedStampColor;
	if (Text_ReleaseState)
	{
		Text_ReleaseState->SetText(bShowProjectStatus
			? (InData.bHasShipped ? NSLOCTEXT("PitchBoard", "TileDeveloped", "개발 완료")
				: NSLOCTEXT("PitchBoard", "TileUndeveloped", "미개발"))
			: FText::GetEmpty());
		Text_ReleaseState->SetColorAndOpacity(FSlateColor(StampColor));
		Text_ReleaseState->SetVisibility(bShowProjectStatus ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ReleaseStampFrame)
	{
		FSlateBrush StampBrush = ReleaseStampFrame->Background;
		StampBrush.OutlineSettings.Color = FSlateColor(StampColor);
		ReleaseStampFrame->SetBrush(StampBrush);
		ReleaseStampFrame->SetVisibility(bShowProjectStatus ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	const bool bShowManufacturingStatus = bShowStatus && InData.bManufacturing;
	if (ManufacturingStatusFrame)
	{
		ManufacturingStatusFrame->SetVisibility(bShowManufacturingStatus
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ManufacturingStatusText)
	{
		ManufacturingStatusText->SetText(!bShowManufacturingStatus ? FText::GetEmpty()
			: InData.bManufacturingQuantitySecured ? NSLOCTEXT("PitchBoard", "ManufacturingQuantityReady", "수량 확보 완료")
			: InData.bHasShipped ? NSLOCTEXT("PitchBoard", "ManufacturingResearchComplete", "개발 완료")
			: InData.bManufacturingResearchInProgress ? NSLOCTEXT("PitchBoard", "ManufacturingResearchInProgress", "개발 진행 중")
			: NSLOCTEXT("PitchBoard", "ManufacturingResearchReady", "미개발"));
		ManufacturingStatusText->SetVisibility(bShowManufacturingStatus
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Text_Name)
		{
			ManufacturingStatusText->SetColorAndOpacity(InData.bHasShipped
				? FSlateColor(DevelopedStampColor) : Text_Name->GetColorAndOpacity());
		}
	}

	const bool bShowGrade = !InData.bManufacturing && bShowHistory && InData.ReleaseGrade != EQualityGrade::Max;
	if (bShowGrade && Badge && Text_Badge)
	{
		Text_Badge->SetText(FText::FromString(QualityGradeToAlphabetString(InData.ReleaseGrade)));
		CGQualityGradeMedal::Apply(GetGameInstance(), InData.CompanyType, InData.ReleaseGrade, Badge, Text_Badge);
		if (Badge->GetVisibility() != ESlateVisibility::Collapsed)
		{
			if (UMaterialInstanceDynamic* FrameMaterial = Badge->GetDynamicMaterial())
			{
				FrameMaterial->SetScalarParameterValue(TEXT("Strength"),
					FrameMaterial->K2_GetScalarParameterValue(TEXT("Strength")) * GradeReflectionScale);
			}
		}
		if (Text_Badge->GetVisibility() != ESlateVisibility::Collapsed)
		{
			if (UMaterialInstanceDynamic* LetterMaterial = Text_Badge->GetDynamicFontMaterial())
			{
				LetterMaterial->SetScalarParameterValue(TEXT("Strength"),
					LetterMaterial->K2_GetScalarParameterValue(TEXT("Strength")) * GradeReflectionScale);
			}
		}
	}
	else
	{
		if (Badge)
		{
			Badge->SetBrushFromMaterial(nullptr);
			Badge->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (Text_Badge)
		{
			Text_Badge->SetText(FText::GetEmpty());
			Text_Badge->SetVisibility(ESlateVisibility::Collapsed);
			FSlateFontInfo EmptyGradeFont = Text_Badge->GetFont();
			EmptyGradeFont.FontMaterial = nullptr;
			Text_Badge->SetFont(EmptyGradeFont);
		}
	}
	if (UWidget* TileBadgeSize = GetWidgetFromName(TEXT("TileBadgeSize")))
	{
		const bool bGradeReady = bShowGrade && Badge && Text_Badge
			&& Badge->GetVisibility() != ESlateVisibility::Collapsed
			&& Text_Badge->GetVisibility() != ESlateVisibility::Collapsed;
		TileBadgeSize->SetVisibility(bGradeReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (UWidget* ReleaseGroup = GetWidgetFromName(TEXT("ReleaseGroup")))
	{
		ReleaseGroup->SetVisibility(bShowProjectStatus ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UWidget* RecommendSize = GetWidgetFromName(TEXT("RecommendSize")))
	{
		RecommendSize->SetVisibility(bRecommended ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UWidget* VerdictRow = GetWidgetFromName(TEXT("VerdictRow")))
	{
		VerdictRow->SetVisibility(bShowProjectStatus ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

UMaterialInstanceDynamic* UPitchTileWidget::CreateProductCoverMID(UObject* Outer)
{
	UMaterialInterface* KeyMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/UI/Materials/M_UI_ProductCoverKey.M_UI_ProductCoverKey"))).LoadSynchronous();
	return KeyMaterial ? UMaterialInstanceDynamic::Create(KeyMaterial, Outer) : nullptr;
}

void UPitchTileWidget::ApplyCoverTexture(UTexture2D* CoverTex, bool bProductKey)
{
	if (!CoverImage || !CoverTex)
	{
		return;
	}
	if (bProductKey && !ProductCoverMID)
	{
		ProductCoverMID = CreateProductCoverMID(this);
	}
	if (bProductKey && ProductCoverMID)
	{
		ProductCoverMID->SetTextureParameterValue(TEXT("Cover"), CoverTex);
		CoverImage->SetBrushFromMaterial(ProductCoverMID);
		return;
	}
	CoverImage->SetBrushFromTexture(CoverTex);
}

void UPitchTileWidget::ApplyProductBand(const FPitchTileData& InData)
{
	if (!ProductBandSwitcher)
	{
		return;
	}
	if (!InData.bManufacturing || InData.Verdict == PitchBoardText::ETileVerdict::Locked)
	{
		ProductBandSwitcher->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const int32 BandIndex = InData.bManufacturingQuantitySecured ? 3
		: InData.bHasShipped ? 2
		: InData.bManufacturingResearchInProgress ? 1 : 0;
	ProductBandSwitcher->SetActiveWidgetIndex(BandIndex);
	ProductBandSwitcher->SetVisibility(ESlateVisibility::HitTestInvisible);

	const float Fraction = FMath::Clamp(InData.ManufacturingResearchFraction, 0.f, 1.f);
	if (ProductBandProgressBar)
	{
		ProductBandProgressBar->SetPercent(Fraction);
	}
	if (ProductBandProgressText)
	{
		ProductBandProgressText->SetText(FText::Format(NSLOCTEXT("PitchBoard", "ProductBandPercent", "{0}%"),
			FText::AsNumber(FMath::FloorToInt(Fraction * 100.f))));
	}
	const bool bCanReserve = InData.ManufacturingMaxReservable > 0;
	if (ProductBandReservableLabel)
	{
		ProductBandReservableLabel->SetVisibility(bCanReserve ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ProductBandReservableText)
	{
		ProductBandReservableText->SetText(bCanReserve
			? FText::Format(NSLOCTEXT("PitchBoard", "ProductBandCount", "{0}개"), FText::AsNumber(InData.ManufacturingMaxReservable))
			: NSLOCTEXT("PitchBoard", "ProductBandShort", "포인트 부족"));
	}
	if (ProductBandSecuredText)
	{
		ProductBandSecuredText->SetText(FText::Format(NSLOCTEXT("PitchBoard", "ProductBandCount", "{0}개"),
			FText::AsNumber(InData.ManufacturingSecuredQuantity)));
	}
}

void UPitchTileWidget::RefreshRecommendation(bool bRestartIntro)
{
	if (!bRecommended || !RecommendationRoot)
	{
		ResetRecommendationVisuals();
		return;
	}

	bRecommendationActive = true;
	RecommendationRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (RecommendShine)
	{
		RecommendShine->SetVisibility(bRecommendShineReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (RecommendSparkle)
	{
		RecommendSparkle->SetVisibility(bRecommendSparkleReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (RecommendGlow)
	{
		RecommendGlow->SetVisibility(bRecommendGlowReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bRestartIntro)
	{
		RecommendationIntroElapsed = 0.f;
		RecommendationCycleElapsed = 0.f;
	}
	UpdateRecommendationVisuals(0.f);
}

void UPitchTileWidget::ResetRecommendationVisuals()
{
	bRecommendationActive = false;
	RecommendationIntroElapsed = 0.f;
	RecommendationCycleElapsed = 0.f;
	if (RecommendationRoot)
	{
		RecommendationRoot->SetVisibility(ESlateVisibility::Collapsed);
		RecommendationRoot->SetRenderScale(FVector2D(1.f, 1.f));
	}
	if (RecommendShine)
	{
		RecommendShine->SetVisibility(ESlateVisibility::Collapsed);
		RecommendShine->SetRenderOpacity(0.f);
		RecommendShine->SetRenderTranslation(FVector2D::ZeroVector);
	}
	if (RecommendSparkle)
	{
		RecommendSparkle->SetVisibility(ESlateVisibility::Collapsed);
		RecommendSparkle->SetRenderOpacity(0.f);
		RecommendSparkle->SetRenderScale(FVector2D(1.f, 1.f));
		RecommendSparkle->SetRenderTransformAngle(0.f);
	}
	if (RecommendGlow)
	{
		RecommendGlow->SetVisibility(ESlateVisibility::Collapsed);
		RecommendGlow->SetRenderOpacity(0.f);
	}
}

void UPitchTileWidget::UpdateRecommendationVisuals(float DeltaTime)
{
	if (!RecommendationRoot)
	{
		return;
	}
	if (USettingsManagerSubsystem::IsReduceMotion(this))
	{
		RecommendationIntroElapsed = RecommendationIntroDuration;
		RecommendationRoot->SetRenderScale(FVector2D(1.f, 1.f));
		if (RecommendShine) RecommendShine->SetRenderOpacity(0.f);
		if (RecommendSparkle) RecommendSparkle->SetRenderOpacity(0.f);
		if (RecommendGlow) RecommendGlow->SetRenderOpacity(bRecommendGlowReady ? 0.24f : 0.f);
		return;
	}

	RecommendationIntroElapsed = FMath::Min(RecommendationIntroElapsed + DeltaTime, RecommendationIntroDuration);
	RecommendationCycleElapsed = FMath::Fmod(RecommendationCycleElapsed + DeltaTime, RecommendationCycleDuration);
	const float IntroT = RecommendationIntroElapsed / RecommendationIntroDuration;
	// R29: 스케일 1.0 초과 금지. 클립 부모에서 잘리고 확대 래스터가 깨진다.
	const float IntroScale = FMath::Lerp(0.94f, 1.f, FWidgetAnimationUtils::EaseOutQuad(IntroT));
	RecommendationRoot->SetRenderScale(FVector2D(IntroScale, IntroScale));

	// 스티커 안쪽 클립에서만 광택이 횡단한다. 고정 슬롯은 WBP가 소유한다.
	if (bRecommendShineReady)
	{
		const float ShineT = (RecommendationCycleElapsed - RecommendationShineStart) / RecommendationShineDuration;
		RecommendShine->SetRenderTranslation(FVector2D(FMath::Lerp(-52.f, 180.f, FMath::Clamp(ShineT, 0.f, 1.f)), 0.f));
		RecommendShine->SetRenderOpacity(ShineT >= 0.f && ShineT < 1.f ? 0.72f * FMath::Sin(ShineT * PI) : 0.f);
	}
	if (bRecommendSparkleReady)
	{
		FWidgetAnimationUtils::DriveSparklePop(RecommendSparkle, (RecommendationCycleElapsed - 0.85f) / 0.5f);
	}
	if (bRecommendGlowReady)
	{
		const float Breath = 0.5f + 0.5f * FMath::Sin(2.f * PI * RecommendationCycleElapsed / RecommendationCycleDuration);
		RecommendGlow->SetRenderOpacity(FMath::Lerp(0.16f, 0.32f, Breath));
	}
}
