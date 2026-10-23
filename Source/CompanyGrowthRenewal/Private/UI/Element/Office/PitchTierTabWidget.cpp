// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Office/PitchTierTabWidget.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

void UPitchTierTabWidget::NativeOnClicked()
{
	Super::NativeOnClicked();

	OnTierClicked.Broadcast(Tier);
}

void UPitchTierTabWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Plate/SelectBand는 표시 전용이라 히트 불가 — 클릭은 버튼(루트)이 받는다.
	if (Plate)
	{
		Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (SelectBand)
	{
		SelectBand->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	ApplySelectionVisual();
}

void UPitchTierTabWidget::Configure(const FPitchTierTabData& InData)
{
	Tier = InData.Tier;
	SetIsSelectable(!InData.bLocked);
	SetIsToggleable(false);
	SetIsInteractableWhenSelected(true);

	if (Text_Tier)
	{
		Text_Tier->SetText(FText::Format(NSLOCTEXT("PitchBoard", "TierTabName", "{0}단계"), InData.Tier));
		Text_Tier->SetColorAndOpacity(FSlateColor(TextColor));
	}

	if (TierCoverImage)
	{
		UTexture2D* CoverTexture = InData.Cover.IsNull() ? nullptr : InData.Cover.LoadSynchronous();
#if WITH_EDITOR
		if (CoverTexture)
		{
			// 비동기 컴파일의 임시 텍스처 대신 실제 표지 크기로 크롭을 계산한다.
			FTextureCompilingManager::Get().FinishCompilation({ CoverTexture });
		}
#endif
		TierCoverImage->SetBrushFromTexture(CoverTexture, false);
		FSlateBrush CoverBrush = TierCoverImage->GetBrush();
		CoverBrush.SetUVRegion(FBox2f(ForceInit));
		if (CoverTexture && CoverTexture->GetSizeX() > 0 && CoverTexture->GetSizeY() > 0
			&& (CoverSurfaceAspect > 0.f || (CoverBrush.ImageSize.X > 0.f && CoverBrush.ImageSize.Y > 0.f)))
		{
			// 최종 버튼 기하에서 이미지를 크롭해 둥근 모서리의 반경을 유지한다.
			const float SourceAspect = static_cast<float>(CoverTexture->GetSizeX()) / CoverTexture->GetSizeY();
			const float SurfaceAspect = CoverSurfaceAspect > 0.f ? CoverSurfaceAspect : CoverBrush.ImageSize.X / CoverBrush.ImageSize.Y;
			const FVector2f UVSize(FMath::Min(SurfaceAspect / SourceAspect, 1.f),
				FMath::Min(SourceAspect / SurfaceAspect, 1.f));
			const FVector2f UVMin = (FVector2f(1.f, 1.f) - UVSize) * 0.5f;
			CoverBrush.SetUVRegion(FBox2f(UVMin, UVMin + UVSize));
		}
		if (CoverTexture && InData.bLocked)
		{
			if (!CoverMID)
			{
				UMaterialInterface* MosaicMaterial = TSoftObjectPtr<UMaterialInterface>(
					FSoftObjectPath(TEXT("/Game/CompanyGrowth/UI/Materials/M_UI_CoverMosaic.M_UI_CoverMosaic"))).LoadSynchronous();
				if (MosaicMaterial)
				{
					CoverMID = UMaterialInstanceDynamic::Create(MosaicMaterial, this);
					CoverMID->SetScalarParameterValue(TEXT("CellPx"), 12.f);
					CoverMID->SetScalarParameterValue(TEXT("Bright"), 1.f);
				}
			}
			if (CoverMID)
			{
				CoverMID->SetTextureParameterValue(TEXT("Cover"), CoverTexture);
				// 크롭 UV와 RoundedBox를 유지한 채 표지 리소스만 교체한다.
				CoverBrush.SetResourceObject(CoverMID);
			}
		}
		TierCoverImage->SetBrush(CoverBrush);
		TierCoverImage->SetVisibility(CoverTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		TierCoverImage->SetRenderOpacity(InData.bLocked ? LockedCoverOpacity : 1.0f);
	}
	if (TierProgressBar)
	{
		TierProgressBar->SetPercent(FMath::Clamp(InData.ProgressFraction, 0.f, 1.f));
		TierProgressBar->SetVisibility(InData.bCurrent ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Plate)
	{
		Plate->SetRenderOpacity(1.0f);
	}

	if (LockIcon)
	{
		LockIcon->SetVisibility(InData.bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Check)
	{
		// 완료 체크는 지나온(클리어) 티어에만 — 잠긴 미래 티어엔 숨긴다
		Check->SetVisibility((InData.bDone && !InData.bLocked)
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Text_Progress)
	{
		Text_Progress->SetText(InData.ProgressText);
		Text_Progress->SetVisibility(bShowProgressInLabel && InData.bCurrent && !InData.ProgressText.IsEmpty()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 잠금 탭도 클릭을 받아 보드의 해금 안내를 띄운다.
	SetIsEnabled(true);
}

void UPitchTierTabWidget::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
	ApplySelectionVisual();
}

void UPitchTierTabWidget::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
	ApplySelectionVisual();
}

void UPitchTierTabWidget::ApplySelectionVisual()
{
	// 둥근 브러시의 외곽선은 틴트 알파와 독립적이므로 선택 해제 시 페인트 자체를 숨긴다.
	if (SelectBand)
	{
		SelectBand->SetRenderOpacity(GetSelected() ? 1.0f : 0.0f);
		SelectBand->SetVisibility(GetSelected() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
