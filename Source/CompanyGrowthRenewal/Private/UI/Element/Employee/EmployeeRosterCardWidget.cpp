// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Employee/EmployeeRosterCardWidget.h"
#include "Data/EntityCardData.h"
#include "Data/EmployeeStatsData.h"
#include "Enum/LootBoxRarity.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "CommonTextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"

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
	if (UEntityCardData* CardData = Cast<UEntityCardData>(ListItemObject))
	{
		SetEmployeeInfo(CardData->EmployeeInfo);
	}

	// 재활용 엔트리는 OnItemSelectionChanged 를 못 받을 수 있어 선택 상태를 직접 재질의
	SetSelectedVisual(IsListItemSelected());
}

void UEmployeeRosterCardWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	SetSelectedVisual(bIsSelected);
}

void UEmployeeRosterCardWidget::SetEmployeeInfo(const FEmployeeInstance& InEmployeeData)
{
	EmployeeData = InEmployeeData;

	if (PortraitImage)
	{
		if (UTexture2D* Portrait = LoadPortraitTexture(EmployeeData.EmployeeID, PortraitDisplaySize))
		{
			PortraitImage->SetBrushFromTexture(Portrait);
			PortraitImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			PortraitImage->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (NameText)
	{
		NameText->SetText(FText::FromString(EmployeeData.EmployeeName));
	}

	if (LevelText)
	{
		// 다이아 뱃지 안 숫자만 (다이아 = 레벨 기호, "Lv." 라벨 폐기 — v8x)
		LevelText->SetText(FText::AsNumber(EmployeeData.Level));
	}

	if (StarsText)
	{
		// FC 강화 표기 "+N" (별 글리프 대신 — 2026-07-24 사용자 확정, 카드 한정) — 0강이면 칩째 숨김
		const int32 Enh = EmployeeData.EnhancementLevel;
		StarsText->SetText(FText::FromString(FString::Printf(TEXT("+%d"), Enh)));
		UWidget* ChipOrText = GetWidgetFromName(TEXT("StarsChip"));
		if (!ChipOrText) { ChipOrText = StarsText; }
		ChipOrText->SetVisibility(Enh > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		// 강화 등급색 — 1~5 회색 / 6~10 은색 / 11+ 금색 (표시값=sRGB → linear 변환)
		auto S2L = [](float c) { return FMath::Pow((c + 0.055f) / 1.055f, 2.4f); };
		auto Metal = [&](float r, float g, float b) { return FLinearColor(S2L(r), S2L(g), S2L(b), 1.f); };
		FLinearColor TxtCol, OutCol;
		if (Enh >= 11)     { TxtCol = Metal(0.878f, 0.627f, 0.125f); OutCol = Metal(0.541f, 0.353f, 0.078f); }
		else if (Enh >= 6) { TxtCol = Metal(0.831f, 0.867f, 0.902f); OutCol = Metal(0.420f, 0.470f, 0.530f); }
		else               { TxtCol = Metal(0.660f, 0.700f, 0.760f); OutCol = Metal(0.300f, 0.340f, 0.400f); }
		StarsText->SetColorAndOpacity(FSlateColor(TxtCol));
		if (UBorder* Chip = Cast<UBorder>(ChipOrText))
		{
			FSlateBrush B = Chip->Background;
			B.OutlineSettings.Color = FSlateColor(OutCol);
			Chip->SetBrush(B);
		}
	}

	// 등급 이음새 라인 — 원색은 형광으로 떠서 x0.62 다크 파생 (시뮬 실측)
	if (RarityGem)
	{
		const FLinearColor RC = FLootBoxRarityUtility::GetRarityColor(EmployeeData.SpawnRarity);
		RarityGem->SetVisibility(ESlateVisibility::HitTestInvisible);
		RarityGem->SetColorAndOpacity(FLinearColor(RC.R * 0.62f, RC.G * 0.62f, RC.B * 0.62f, 1.f));
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
	// 인덱스 = EProductionDiscipline 순서. UIIcon 폴더는 DirectoriesToAlwaysCook 등록됨
	{
		static const TCHAR* DiscIconPaths[6] = {
			TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Plan.T_UIIcon_Disc_Plan"),
			TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Dev.T_UIIcon_Disc_Dev"),
			TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Art.T_UIIcon_Disc_Art"),
			TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Sound.T_UIIcon_Disc_Sound"),
			TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_Server.T_UIIcon_Disc_Server"),
			TEXT("/Game/CompanyGrowth/UI/Textures/UIIcon/T_UIIcon_Disc_QA.T_UIIcon_Disc_QA"),
		};
		int32 BestIdx = -1, BestVal = 0;
		for (int32 i = 0; i < EmployeeData.DisciplinePoints.Num() && i < 6; ++i)
		{
			if (EmployeeData.DisciplinePoints[i] > BestVal) { BestVal = EmployeeData.DisciplinePoints[i]; BestIdx = i; }
		}
		UTexture2D* DiscTex = nullptr;
		if (BestIdx >= 0)
		{
			DiscTex = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(DiscIconPaths[BestIdx])).LoadSynchronous();
		}
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
			GlintMID->SetVectorParameterValue(TEXT("LineCol"), FLootBoxRarityUtility::GetRarityColor(PotentialRarity));

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
	if (SelectionBG)
	{
		SelectionBG->SetVisibility(bNowSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UEmployeeRosterCardWidget::OnButtonClicked()
{
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

	const FString FilePath = BasePath / TEXT("Portraits") / (FString::FromInt(EmployeeID) + TEXT(".png"));
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
	const int32 DstWidth = FMath::RoundToInt32(TargetSize.X);
	const int32 DstHeight = FMath::RoundToInt32(TargetSize.Y);

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
