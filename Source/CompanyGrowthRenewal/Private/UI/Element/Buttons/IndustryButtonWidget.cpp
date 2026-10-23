#include "UI/Element/Buttons/IndustryButtonWidget.h"

#include "Components/Image.h"
#include "Components/Border.h"
#include "CommonButtonBase.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/CompanyInfoTable.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"

#if WITH_EDITOR
#include "Engine/DataTable.h"
#include "UObject/UObjectGlobals.h"
#endif

#if WITH_EDITOR
namespace
{
	// 디자인타임 전용: 서브시스템 없이 DT_CompanyInfo 행을 직접 찾아 채움.
	// 런타임은 TableManager 캐시 경유라 이 경로를 타지 않음(IsDesignTime 가드).
	bool LookupCompanyInfoDesignTime(ECompanyType InType, FCompanyInfoTable& OutInfo)
	{
		if (InType == ECompanyType::None)
		{
			return false;
		}

		UDataTable* DT = LoadObject<UDataTable>(
			nullptr, TEXT("/Game/CompanyGrowth/Table/Company/DT_CompanyInfo.DT_CompanyInfo"));
		if (!DT)
		{
			return false;
		}

		// 1차: 행명 = enum 식별자("Game" 등)로 직접 조회
		const FString FullName = StaticEnum<ECompanyType>()->GetNameStringByValue(static_cast<int64>(InType));
		FString RowKey = FullName;
		int32 ScopeIdx = INDEX_NONE;
		if (FullName.FindLastChar(TEXT(':'), ScopeIdx))
		{
			RowKey = FullName.RightChop(ScopeIdx + 1);
		}
		if (const FCompanyInfoTable* ByName = DT->FindRow<FCompanyInfoTable>(FName(*RowKey), TEXT("DesignTimePreview")))
		{
			OutInfo = *ByName;
			return true;
		}

		// 2차: 행명이 식별자와 다를 수 있으니 CompanyType 필드로 매칭(런타임 캐시와 동일 의미)
		for (const FName& RowName : DT->GetRowNames())
		{
			if (const FCompanyInfoTable* Row = DT->FindRow<FCompanyInfoTable>(RowName, TEXT("DesignTimePreview")))
			{
				if (Row->CompanyType == InType)
				{
					OutInfo = *Row;
					return true;
				}
			}
		}
		return false;
	}
}
#endif

// 배경 문양 = 면색을 OKLab 에서 밝힌 같은 계열 색. 흰색을 얹으면 면이 회색빛으로 바랜다.
static constexpr float IndustryGlyphAlpha = 0.40f;
static constexpr float IndustryGlyphAlphaSelected = 0.48f;
static constexpr float IndustryGlyphLift = 0.20f;
static constexpr float IndustryGlyphChromaScale = 0.85f;

namespace
{
	FVector3f LinearToOklab(const FLinearColor& C)
	{
		const float L = FMath::Pow(0.4122214708f * C.R + 0.5363325363f * C.G + 0.0514459929f * C.B, 1.f / 3.f);
		const float M = FMath::Pow(0.2119034982f * C.R + 0.6806995451f * C.G + 0.1073969566f * C.B, 1.f / 3.f);
		const float S = FMath::Pow(0.0883024619f * C.R + 0.2817188376f * C.G + 0.6299787005f * C.B, 1.f / 3.f);
		return FVector3f(
			0.2104542553f * L + 0.7936177850f * M - 0.0040720468f * S,
			1.9779984951f * L - 2.4285922050f * M + 0.4505937099f * S,
			0.0259040371f * L + 0.7827717662f * M - 0.8086757660f * S);
	}

	FLinearColor OklabToLinear(float L, float A, float B)
	{
		const float L3 = FMath::Cube(L + 0.3963377774f * A + 0.2158037573f * B);
		const float M3 = FMath::Cube(L - 0.1055613458f * A - 0.0638541728f * B);
		const float S3 = FMath::Cube(L - 0.0894841775f * A - 1.2914855480f * B);
		return FLinearColor(
			4.0767416621f * L3 - 3.3077115913f * M3 + 0.2309699292f * S3,
			-1.2684380046f * L3 + 2.6097574011f * M3 - 0.3413193965f * S3,
			-0.0041960863f * L3 - 0.7034186147f * M3 + 1.7076147010f * S3);
	}

	// 화면 밖 색은 채널별로 자르면 색상이 틀어지므로, 색상·명도를 지킨 채 채도만 줄여 맞춘다.
	FLinearColor OklchToLinear(float L, float Chroma, float HueRad)
	{
		const float CosH = FMath::Cos(HueRad);
		const float SinH = FMath::Sin(HueRad);
		auto InGamut = [](const FLinearColor& X)
		{
			return FMath::Min3(X.R, X.G, X.B) >= -1e-5f && FMath::Max3(X.R, X.G, X.B) <= 1.f + 1e-5f;
		};
		FLinearColor Out = OklabToLinear(L, Chroma * CosH, Chroma * SinH);
		if (!InGamut(Out))
		{
			float Lo = 0.f;
			float Hi = Chroma;
			for (int32 Step = 0; Step < 24; ++Step)
			{
				const float Mid = 0.5f * (Lo + Hi);
				if (InGamut(OklabToLinear(L, Mid * CosH, Mid * SinH)))
				{
					Lo = Mid;
				}
				else
				{
					Hi = Mid;
				}
			}
			Out = OklabToLinear(L, Lo * CosH, Lo * SinH);
		}
		return FLinearColor(FMath::Clamp(Out.R, 0.f, 1.f), FMath::Clamp(Out.G, 0.f, 1.f), FMath::Clamp(Out.B, 0.f, 1.f), 1.f);
	}

	FLinearColor IndustryGlyphColorFromFace(const FLinearColor& Face, float Alpha)
	{
		const FVector3f Lab = LinearToOklab(Face);
		FLinearColor Glyph = OklchToLinear(
			FMath::Min(Lab.X + IndustryGlyphLift, 0.97f),
			FMath::Sqrt(Lab.Y * Lab.Y + Lab.Z * Lab.Z) * IndustryGlyphChromaScale,
			FMath::Atan2(Lab.Z, Lab.Y));
		Glyph.A = Alpha;
		return Glyph;
	}
}

// 눌림 = 같은 면을 어둡게 + 2px 가라앉힘. 크기가 안 변하므로 면 밖으로 번지는 잔상이 없다.
static constexpr float FacePressedScale = 0.82f;
static constexpr float FacePressSink = 2.0f;

// 산업색 면 위 라벨은 크림색으로 읽히게 한다.
static const FLinearColor LabelCream(1.0f, 0.9387f, 0.8714f, 1.0f);

void UIndustryButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너 프리뷰: 인스턴스의 CompanyType 으로 라벨/배경 문양/면색을 미리 그림(런타임 무영향)
	if (IsDesignTime())
	{
		ApplyCompanyInfo();
	}

	UpdateSelectionBorder(GetSelected());
}

void UIndustryButtonWidget::SetCompanyType(ECompanyType InType)
{
	CompanyType = InType;
	ApplyCompanyInfo();
}

void UIndustryButtonWidget::ApplyCompanyInfo()
{
	FCompanyInfoTable Info;
	bool bFound = false;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			Info = TableMgr->GetCompanyInfo(CompanyType, bFound);
		}
	}
#if WITH_EDITOR
	// 디자인타임 프리뷰 전용: 서브시스템 부재라 위 경로가 비어, DT를 직접 로드해 동일하게 채움.
	if (!bFound && IsDesignTime())
	{
		bFound = LookupCompanyInfoDesignTime(CompanyType, Info);
	}
#endif

	// 행 없음(None 등): WBP 기본값 보존하고 종료
	if (!bFound)
	{
		return;
	}

	if (!Info.DisplayName.IsEmpty())
	{
		SetButtonText(Info.DisplayName);
	}

	GlyphAccentColor = Info.AccentColor;
	if (GlyphImage && !Info.GlyphIcon.IsNull())
	{
		if (UTexture2D* GlyphTex = Info.GlyphIcon.LoadSynchronous())
		{
			GlyphImage->SetBrushFromTexture(GlyphTex);
		}
	}

	// 선택 링은 공통 블루 고정이라 WBP 저작값이 전부다 — 여기서 틴트를 건드리면
	// RoundedBox 의 TintColor(= 면 채움)가 덮여 타일이 통째로 파래진다.
	SetTextColor(FSlateColor(LabelCream));

	if (FacePlate)
	{
		if (Info.ButtonFaceColor.A <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[IndustryButton] DT_CompanyInfo '%s' ButtonFaceColor not set (alpha 0); keeping WBP default face"),
				*StaticEnum<ECompanyType>()->GetNameStringByValue(static_cast<int64>(CompanyType)));
		}
		else
		{
			CachedFaceColor = Info.ButtonFaceColor;
			ApplyFaceColor(false);
		}
	}
	if (GlyphImage)
	{
		GlyphImage->SetColorAndOpacity(IndustryGlyphColorFromFace(CachedFaceColor, GetSelected() ? IndustryGlyphAlphaSelected : IndustryGlyphAlpha));
	}
}

void UIndustryButtonWidget::ApplyFaceColor(bool bPressed)
{
	if (!FacePlate)
	{
		return;
	}
	// 알파는 건드리지 않는다 — 스칼라 곱을 그대로 쓰면 눌릴 때 면이 반투명해진다.
	FLinearColor Face = CachedFaceColor;
	if (bPressed)
	{
		Face.R *= FacePressedScale;
		Face.G *= FacePressedScale;
		Face.B *= FacePressedScale;
	}
	FacePlate->SetBrushColor(Face);
	FacePlate->SetRenderTranslation(FVector2D(0.f, bPressed ? FacePressSink : 0.f));
}

void UIndustryButtonWidget::NativeOnPressed()
{
	Super::NativeOnPressed();
	ApplyFaceColor(true);
}

void UIndustryButtonWidget::NativeOnReleased()
{
	Super::NativeOnReleased();
	ApplyFaceColor(false);
}

void UIndustryButtonWidget::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
	UpdateSelectionBorder(true);
}

void UIndustryButtonWidget::NativeOnDeselected(bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
	UpdateSelectionBorder(false);
}

void UIndustryButtonWidget::SetIndustryLocked(bool bInLocked)
{
	bIndustryLocked = bInLocked;
	SetIsSelectable(!bInLocked);
	// 잠금 시 면과 전경을 함께 낮추되 업종색과 글리프는 식별 가능하게 유지한다.
	SetRenderOpacity(bInLocked ? 0.72f : 1.0f);
	if (bInLocked && CheckImage)
	{
		// 비선택 기본선(Hidden)과 통일 — 레이아웃 칸 유지로 잠금↔해금 전환 시 글리프/라벨 안 밀림 (UpdateSelectionBorder 규약과 동일)
		CheckImage->SetVisibility(ESlateVisibility::Hidden);
	}
	if (LockImage)
	{
		LockImage->SetVisibility(bInLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UIndustryButtonWidget::UpdateSelectionBorder(bool bInSelected)
{
	if (GlyphImage)
	{
		GlyphImage->SetColorAndOpacity(IndustryGlyphColorFromFace(CachedFaceColor, bInSelected ? IndustryGlyphAlphaSelected : IndustryGlyphAlpha));
	}
	if (SelectionBorder)
	{
		SelectionBorder->SetVisibility(bInSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	// 체크는 레이아웃 칸을 유지(Hidden)해 미선택↔선택 전환 시 글리프/라벨이 안 밀리게
	if (CheckImage)
	{
		CheckImage->SetVisibility(bInSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
