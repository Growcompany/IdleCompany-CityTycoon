// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Employee/EmployeeRosterCardWidget.h"
#include "Global/CGDevSettings.h"
#include "Data/EntityCardData.h"
#include "Data/EmployeeStatsData.h"
#include "Enum/LootBoxRarity.h"
#include "UI/Element/Employee/EmployeeRarityDisplay.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "CommonTextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"
#include "Global/GlobalUtilFunctions.h"

namespace
{
	// 빈자리와 직원이 같은 다크 타일 판을 공유한다 — 판 위의 직원 전용 레이어는 이름으로 걷어야 비침이 없다
	const TCHAR* const RosterEmployeeOnlyLayers[] = {
		TEXT("NameBand"), TEXT("LvBadgeBox"), TEXT("StageGlow"), TEXT("StarsChip"), TEXT("DiscChip") };
	// 아래 SetEmployeeInfo 가 따로 가시성을 정하지 않는 것만 되살린다.
	constexpr int32 RosterRestoreLayerCount = 3;

	// 빈자리 브리딩 — 타일을 키우면 엔트리 셀을 넘으므로 링/＋ 의 불투명도만 움직인다
	constexpr float HirePulsePeriodSeconds = 1.2f;
	// ㉔ 펄스 골에서 잉크가 배경과 섞여 대비비가 3.26 까지 내려갔다(0.57×잉크 + 0.43×타일면).
	// 저작 잉크(#A7B1BE)는 정확한데 레이어 불투명도가 계약을 무력화한 것 — 골을 0.82 로 올려 골에서도 CR 4.5 를 넘긴다.
	constexpr float HirePulseFloorOpacity = 0.82f;
}

void UEmployeeRosterCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	OnClicked().AddUObject(this, &UEmployeeRosterCardWidget::OnButtonClicked);
}

void UEmployeeRosterCardWidget::NativeDestruct()
{
	OnClicked().RemoveAll(this);

	Super::NativeDestruct();
}

void UEmployeeRosterCardWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	// 이 구간의 선택 복원은 목록 재구성일 뿐 사용자의 선택이 아니다 — 펀치를 막는다
	bApplyingListItem = true;

	if (UEntityCardData* CardData = Cast<UEntityCardData>(ListItemObject))
	{
		if (CardData->bIsHireSlot)
		{
			SetHireSlot();
		}
		else
		{
			bHireSlot = false;
			bHirePulse = false;   // 재활용 엔트리가 앞 빈자리의 펄스를 물고 오지 않도록
			PulseElapsed = 0.f;
			if (HireLayer)
			{
				HireLayer->SetVisibility(ESlateVisibility::Collapsed);
			}
			SetEmployeeInfo(CardData->EmployeeInfo);
		}
	}

	// 재활용 엔트리는 OnItemSelectionChanged 를 못 받을 수 있어 선택 상태를 직접 재질의
	SetSelectedVisual(IsListItemSelected());

	bApplyingListItem = false;
}

void UEmployeeRosterCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (SelectPunch.IsPlaying())
	{
		SetRenderScale(FVector2D(SelectPunch.Tick(InDeltaTime)));
		bRenderScaleDirty = true;
		return;
	}

	if (bRenderScaleDirty)
	{
		SetRenderScale(FVector2D::UnitVector);
		bRenderScaleDirty = false;
	}

	if (bHireSlot && bHirePulse && HireLayer)
	{
		// 스케일 펄스는 141 타일을 엔트리 셀 밖으로 밀어 TileView 클립에 잘렸다 — 크기 대신 불투명도로 호흡한다.
		// 올린 코사인이 곧 EaseInOut 곡선이라 별도 이징을 겹치지 않는다 (1.2s 주기, 0.82 ~ 1.0)
		PulseElapsed += InDeltaTime;
		const float Phase = 0.5f * (1.f - FMath::Cos(2.f * PI * PulseElapsed / HirePulsePeriodSeconds));
		HireLayer->SetRenderOpacity(FMath::Lerp(HirePulseFloorOpacity, 1.f, Phase));
	}
}

void UEmployeeRosterCardWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	SetSelectedVisual(bIsSelected);
}

void UEmployeeRosterCardWidget::SetHireSlot(bool bAffordable)
{
	bHireSlot = true;
	if (bHirePulse != bAffordable)
	{
		PulseElapsed = 0.f;   // 재화가 생긴 순간 호흡을 처음부터 (중간 위상에서 튀어나오지 않게)
		bHirePulse = bAffordable;
	}
	EmployeeData = FEmployeeInstance();

	// 직원 레이어 전부 숨기고 빈자리 레이어만 — 재활용 엔트리가 앞 직원의 칩/글린트를 남기지 않도록
	UWidget* EmployeeLayers[] = { PortraitImage, PortraitFallback, NameText, LevelText, RarityGem, StarsText, OverallText, PotentialGlint };
	for (UWidget* Layer : EmployeeLayers)
	{
		if (Layer)
		{
			Layer->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	for (const TCHAR* LayerName : RosterEmployeeOnlyLayers)
	{
		if (UWidget* Layer = GetWidgetFromName(LayerName))
		{
			Layer->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (HireLayer)
	{
		HireLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
		// 펄스가 꺼진 채로 재활용되면 앞 타일의 호흡 중간값(투명한 ＋)이 그대로 남는다
		HireLayer->SetRenderOpacity(1.f);
	}
}

void UEmployeeRosterCardWidget::SetEmployeeInfo(const FEmployeeInstance& InEmployeeData)
{
	EmployeeData = InEmployeeData;

	// SetHireSlot 이 걷은 레이어 중 아래에서 가시성을 다시 지정하지 않는 것만 복원 (재활용 엔트리 대비)
	if (NameText)
	{
		NameText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (LevelText)
	{
		LevelText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	const bool bHasEnhancement = EmployeeData.EnhancementLevel > 0;
	if (StarsText)
	{
		StarsText->SetText(bHasEnhancement
			? FText::FromString(FString::Printf(TEXT("%d"), EmployeeData.EnhancementLevel)) : FText::GetEmpty());
		StarsText->SetVisibility(bHasEnhancement ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UWidget* StarsChip = GetWidgetFromName(TEXT("StarsChip")))
	{
		StarsChip->SetVisibility(bHasEnhancement ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	for (int32 LayerIndex = 0; LayerIndex < RosterRestoreLayerCount; ++LayerIndex)
	{
		if (UWidget* Layer = GetWidgetFromName(RosterEmployeeOnlyLayers[LayerIndex]))
		{
			Layer->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	// 초상은 런타임 캡처라 아직 없을 수 있다 — 그때 빈 판만 두면 슬롯이 고장난 것처럼 보인다
	bool bHasPortrait = false;
	if (PortraitImage)
	{
		if (UTexture2D* Portrait = LoadPortraitTexture(EmployeeData.EmployeeID, PortraitDisplaySize))
		{
			PortraitImage->SetBrushFromTexture(Portrait, true);
			PortraitImage->SetVisibility(ESlateVisibility::Visible);
			bHasPortrait = true;
		}
		else
		{
			PortraitImage->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	if (PortraitFallback)
	{
		PortraitFallback->SetVisibility(bHasPortrait ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (NameText)
	{
		NameText->SetText(FText::FromString(EmployeeData.EmployeeName));
	}

	if (LevelText)
	{
		// 강화 수치와 구분하도록 레벨 접두사를 유지한다.
		LevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv%d"), EmployeeData.Level)));
	}

	// 등급 = 타일을 두르는 2px 프레임(구 상단 밴드). 위젯 이름은 유지한다 — 주입 경로가 여기 하나뿐이라
	// 이름을 바꾸면 저작과 조용히 갈린다. 원색은 형광으로 떠서 x0.62 다크 파생 (시뮬 실측)
	if (RarityGem)
	{
		const FLinearColor RC = EmployeeRarityDisplay::Color(EmployeeData.SpawnRarity);
		RarityGem->SetVisibility(ESlateVisibility::HitTestInvisible);
		// 이 위젯은 면이 아니라 타일을 두르는 2px 프레임이다 — 라운드박스의 아웃라인 색에는 위젯
		// ColorAndOpacity 가 곱해지지 않는다(틴트는 채움/이미지 축). 채움이 투명이라 틴트만 쓰면
		// 저작된 흰 테두리가 그대로 남는다. 등급색은 브러시 아웃라인에 직접 쓴다.
		FSlateBrush GemBrush = RarityGem->GetBrush();
		GemBrush.OutlineSettings.Color = FSlateColor(FLinearColor(RC.R * 0.62f, RC.G * 0.62f, RC.B * 0.62f, 1.f));
		RarityGem->SetBrush(GemBrush);
		RarityGem->SetColorAndOpacity(FLinearColor::White);
	}

	// 좌상단 레이팅 = 최고 직능 포인트 (좌하단 직능 아이콘과 한 세트 — "개발 33짜리 개발자". 2026-07-25 사용자 확정)
	// FC 카드 재질 — 면/림/프레임/무대광을 등급색 믹스로 (GetRarityColor 값=sRGB 분수 → sRGB 혼합 후 linear 변환)
	{
		const FLinearColor RaritySrgb = FLootBoxRarityUtility::GetRarityColor(EmployeeData.SpawnRarity);
		auto SrgbToLinear = [](float C) { return FMath::Pow((C + 0.055f) / 1.055f, 2.4f); };
		auto MixToLinear = [&](const FLinearColor& BaseSrgb, float RarityRatio, float Alpha)
		{
			const FLinearColor M = BaseSrgb * (1.f - RarityRatio) + RaritySrgb * RarityRatio;
			return FLinearColor(SrgbToLinear(M.R), SrgbToLinear(M.G), SrgbToLinear(M.B), Alpha);
		};
		if (UImage* Base = Cast<UImage>(GetWidgetFromName(TEXT("BaseBG"))))
		{
			FSlateBrush B = Base->GetBrush();
			B.TintColor = FSlateColor(MixToLinear(FLinearColor(0.941f, 0.929f, 0.894f), 0.13f, 1.f));            // #F0EDE4 재질면
			B.OutlineSettings.Color = FSlateColor(MixToLinear(FLinearColor(0.290f, 0.255f, 0.200f), 0.46f, 1.f)); // #4A4133 림
			Base->SetBrush(B);
		}
		if (UImage* Frame = Cast<UImage>(GetWidgetFromName(TEXT("InnerFrame"))))
		{
			FSlateBrush B = Frame->GetBrush();
			B.OutlineSettings.Color = FSlateColor(MixToLinear(FLinearColor(0.227f, 0.188f, 0.118f), 0.42f, 0.55f)); // #3A301E 헤어라인
			Frame->SetBrush(B);
		}
		if (UImage* Glow = Cast<UImage>(GetWidgetFromName(TEXT("StageGlow"))))
		{
			// 글로우는 밝을수록 좋아 sRGB 분수를 linear로 그대로 사용 (기존 등급색 관행)
			Glow->SetColorAndOpacity(FLinearColor(RaritySrgb.R, RaritySrgb.G, RaritySrgb.B, 0.34f));
		}
		if (UImage* Shade = Cast<UImage>(GetWidgetFromName(TEXT("FaceShade"))))
		{
			// 하단 음영 = 등급색 심연 (FC 금속 깊이) — 플립 그라데이션에 어두운 등급 틴트
			Shade->SetColorAndOpacity(FLinearColor(RaritySrgb.R * 0.15f, RaritySrgb.G * 0.15f, RaritySrgb.B * 0.15f, 0.30f));
		}
		if (UImage* GlintImg = Cast<UImage>(GetWidgetFromName(TEXT("CardGlint"))))
		{
			// 키라인 궤도 등급색 글린트 — 광택은 밝을수록 좋아 sRGB 분수 그대로 (PotentialGlint 선례)
			if (UMaterialInstanceDynamic* GlintMID = GlintImg->GetDynamicMaterial())
			{
				GlintMID->SetVectorParameterValue(TEXT("LineCol"), RaritySrgb);
			}
		}
	}

	// 좌하단 직능 아이콘 칩 = DisciplinePoints 최고값 (GetDerivedDepartment와 동일 argmax)
	{
		int32 BestIdx = -1, BestVal = 0;
		for (int32 i = 0; i < EmployeeData.DisciplinePoints.Num() && i < 6; ++i)
		{
			if (EmployeeData.DisciplinePoints[i] > BestVal) { BestVal = EmployeeData.DisciplinePoints[i]; BestIdx = i; }
		}
		UTexture2D* DiscTex = UGlobalUtilFunctions::GetDisciplineIcon(
			GetDisciplineIconSlot(BestIdx, EmployeeData.DisciplinePoints.Num()));
		if (OverallText)
		{
			OverallText->SetText(FText::AsNumber(BestVal));
			OverallText->SetVisibility(BestIdx >= 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (UImage* Icon = Cast<UImage>(GetWidgetFromName(TEXT("DiscIcon"))))
		{
			if (DiscTex) { Icon->SetBrushFromTexture(DiscTex); }
		}
		if (UWidget* Chip = GetWidgetFromName(TEXT("DiscChip")))
		{
			Chip->SetVisibility((BestIdx >= 0 && DiscTex) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	if (PotentialGlint)
	{
		if (UMaterialInstanceDynamic* GlintMID = PotentialGlint->GetDynamicMaterial())
		{
			const ELootBoxRarity PotentialRarity = EmployeeData.PotentialAbility.CurrentRarity;
			GlintMID->SetVectorParameterValue(TEXT("LineCol"), EmployeeRarityDisplay::Color(PotentialRarity));

			// Epic+ 만 반짝임 — 낮은 등급까지 반짝이면 신호가 무의미해짐
			const bool bHighPotential = static_cast<uint8>(PotentialRarity) >= static_cast<uint8>(ELootBoxRarity::Epic);
			GlintMID->SetScalarParameterValue(TEXT("GlintOrbit"), 1.0f);
			GlintMID->SetScalarParameterValue(TEXT("GlintGain"), bHighPotential ? 0.35f : 0.0f);
			GlintMID->SetScalarParameterValue(TEXT("GlintSpeed"), 0.15f);
			GlintMID->SetScalarParameterValue(TEXT("BaseA"), 0.55f);
		}
		PotentialGlint->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UEmployeeRosterCardWidget::SetSelectedVisual(bool bNowSelected)
{
	const bool bTurnedOn = bNowSelected && !bSelectedVisual;
	bSelectedVisual = bNowSelected;

	if (SelectionBG)
	{
		SelectionBG->SetVisibility(bNowSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}

	if (bTurnedOn && !bApplyingListItem)
	{
		SelectPunch.Start(1.06f, 0.25f);
	}
}

void UEmployeeRosterCardWidget::OnButtonClicked()
{
	// 빈자리 타일은 직원 ID 가 없다 — 선택 경로로 보내면 없는 직원을 선택하게 된다
	if (bHireSlot)
	{
		OnRosterHireSlotClicked.Broadcast();
		return;
	}

	OnRosterCardClicked.Broadcast(EmployeeData.EmployeeID);
}

UTexture2D* UEmployeeRosterCardWidget::LoadPortraitTexture(int32 EmployeeID, const FVector2D& TargetSize)
{
	FString BasePath;
#if PLATFORM_ANDROID
	extern FString GExternalFilePath;
	BasePath = GExternalFilePath;
#else
	BasePath = FPaths::ProjectSavedDir();
#endif

	const FString FilePath = BasePath / UCGDevSettings::GetPortraitFolderName() / (FString::FromInt(EmployeeID) + TEXT(".png"));
	if (!FPaths::FileExists(FilePath))
	{
		return nullptr;
	}

	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
	{
		return nullptr;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
	{
		return nullptr;
	}

	TArray<uint8> RawData;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
	{
		return nullptr;
	}

	const int32 SrcWidth = ImageWrapper->GetWidth();
	const int32 SrcHeight = ImageWrapper->GetHeight();
	if (SrcWidth <= 0 || SrcHeight <= 0)
	{
		return nullptr;
	}
	// TargetSize는 최대 예산이다. 두 축을 따로 늘리면 컷아웃 얼굴 비율이 바뀐다.
	const double Scale = TargetSize.X > 0.f && TargetSize.Y > 0.f
		? FMath::Min(1.0, FMath::Min(TargetSize.X / SrcWidth, TargetSize.Y / SrcHeight)) : 1.0;
	const int32 DstWidth = FMath::Max(1, FMath::RoundToInt32(SrcWidth * Scale));
	const int32 DstHeight = FMath::Max(1, FMath::RoundToInt32(SrcHeight * Scale));

	int32 FinalWidth = SrcWidth;
	int32 FinalHeight = SrcHeight;
	TArray<uint8> FinalData;

	if (DstWidth > 0 && DstHeight > 0 && (SrcWidth > DstWidth || SrcHeight > DstHeight))
	{
		TArray<FColor> SrcColors;
		SrcColors.SetNum(SrcWidth * SrcHeight);
		FMemory::Memcpy(SrcColors.GetData(), RawData.GetData(), RawData.Num());

		// RGB/알파 분리 리사이즈 (알파에 감마 보정 미적용 — 컷아웃 가장자리 헤일로 방지)
		TArray<FColor> RGBOnly;
		TArray<FColor> AlphaAsColor;
		RGBOnly.SetNum(SrcWidth * SrcHeight);
		AlphaAsColor.SetNum(SrcWidth * SrcHeight);
		for (int32 i = 0; i < SrcColors.Num(); ++i)
		{
			const FColor& Src = SrcColors[i];
			RGBOnly[i] = FColor(Src.R, Src.G, Src.B, 255);
			AlphaAsColor[i] = FColor(Src.A, Src.A, Src.A, 255);
		}

		TArray<FColor> ResizedRGB;
		FImageUtils::ImageResize(SrcWidth, SrcHeight, RGBOnly, DstWidth, DstHeight, ResizedRGB, false);
		TArray<FColor> ResizedAlpha;
		FImageUtils::ImageResize(SrcWidth, SrcHeight, AlphaAsColor, DstWidth, DstHeight, ResizedAlpha, false);

		TArray<FColor> ResizedPixels;
		ResizedPixels.SetNum(DstWidth * DstHeight);
		for (int32 i = 0; i < ResizedPixels.Num(); ++i)
		{
			ResizedPixels[i] = FColor(ResizedRGB[i].R, ResizedRGB[i].G, ResizedRGB[i].B, ResizedAlpha[i].R);
		}

		FinalWidth = DstWidth;
		FinalHeight = DstHeight;
		FinalData.SetNum(FinalWidth * FinalHeight * 4);
		FMemory::Memcpy(FinalData.GetData(), ResizedPixels.GetData(), FinalData.Num());
	}
	else
	{
		FinalData = MoveTemp(RawData);
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(FinalWidth, FinalHeight, PF_B8G8R8A8);
	if (!Texture)
	{
		return nullptr;
	}

	void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, FinalData.GetData(), FinalData.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();

	return Texture;
}
