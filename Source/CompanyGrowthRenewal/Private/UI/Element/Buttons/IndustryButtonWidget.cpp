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

// 미선택 글리프 = 다크 슬레이트(라이트 판 위 대비). 선택 시에만 산업 시그니처색으로 점화(무지개 방지 — 목업 규칙).
static const FLinearColor GlyphNeutralTint(0.32f, 0.37f, 0.44f, 1.0f);

void UIndustryButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너 프리뷰: 인스턴스의 CompanyType 으로 라벨/글리프/스타일을 미리 그림(런타임 무영향)
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

	// 산업 시그니처색은 캐시만 — 미선택 글리프는 중립, 선택 시에만 이 색으로 점화(무지개 방지).
	GlyphAccentColor = Info.AccentColor;
	if (GlyphImage && !Info.GlyphIcon.IsNull())
	{
		if (UTexture2D* GlyphTex = Info.GlyphIcon.LoadSynchronous())
		{
			GlyphImage->SetBrushFromTexture(GlyphTex);
		}
		GlyphImage->SetColorAndOpacity(GetSelected() ? GlyphAccentColor : GlyphNeutralTint);
	}

	// 선택 글로우 링을 그 산업색으로 — 고른 업종만 자기 색이 확 올라옴(미니 점화).
	if (SelectionBorder)
	{
		SelectionBorder->SetBrushColor(Info.AccentColor);
	}

	// 배경 = 플랫 칩 스타일(RoundedBox 다크글래스, 광택 마스터 미사용). 산업 구분은 글리프 색+선택 글로우가 담당.
	// 스타일 에셋 생성: docs/04_ArtDirection/UIChromePrompts (make_flatchip.py). 선택=밝은 중립, 색은 글리프.
	static const TSoftClassPtr<UCommonButtonStyle> NeutralButtonStyle(
		FSoftClassPath(TEXT("/Game/CompanyGrowth/UI/CommonStyle/Button/CUI_Style2_Btn_FlatChip.CUI_Style2_Btn_FlatChip_C")));
	if (UClass* StyleClass = NeutralButtonStyle.LoadSynchronous())
	{
		SetStyle(TSubclassOf<UCommonButtonStyle>(StyleClass));
	}
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
	SetRenderOpacity(bInLocked ? 0.45f : 1.0f);
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
	// 선택 = 산업 시그니처색 점화, 미선택 = 중립(무지개 방지 — 목업 규칙)
	if (GlyphImage)
	{
		GlyphImage->SetColorAndOpacity(bInSelected ? GlyphAccentColor : GlyphNeutralTint);
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
