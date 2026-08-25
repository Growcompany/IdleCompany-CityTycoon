// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/ProductionOrderRowWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ProjectDataTable.h"
#include "Table/ProductRecipeTable.h"
#include "Table/ResourceInfo.h"
#include "Global/GlobalUtilFunctions.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

UProductionOrderRowWidget::UProductionOrderRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 골드→그레이 램프. 블루 계열을 쓰지 않는 이유 = 블루는 선택 상태 기능색 전용이라
	// 등급 배지에 쓰면 "선택됨"과 충돌한다.
	GradeColors.Add(EQualityGrade::S, FLinearColor(0.8632f, 0.5423f, 0.0782f, 1.0f)); // #F2C14E
	GradeColors.Add(EQualityGrade::A, FLinearColor(0.7379f, 0.3005f, 0.0645f, 1.0f)); // #E39A4B
	GradeColors.Add(EQualityGrade::B, FLinearColor(0.4793f, 0.3763f, 0.1946f, 1.0f)); // #B9A97E
	GradeColors.Add(EQualityGrade::C, FLinearColor(0.2462f, 0.2747f, 0.3372f, 1.0f)); // #8C93A0
	GradeColors.Add(EQualityGrade::D, FLinearColor(0.1444f, 0.1714f, 0.2158f, 1.0f)); // #6E7684
	GradeColors.Add(EQualityGrade::F, FLinearColor(0.0930f, 0.1119f, 0.1441f, 1.0f)); // #5B626E
}

void UProductionOrderRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	OnClicked().AddUObject(this, &UProductionOrderRowWidget::HandleClicked);
}

void UProductionOrderRowWidget::NativeDestruct()
{
	OnClicked().RemoveAll(this);
	Super::NativeDestruct();
}

void UProductionOrderRowWidget::HandleClicked()
{
	OnOrderRowClicked.ExecuteIfBound(CachedOrderID);
}

void UProductionOrderRowWidget::SetOrderData(const FProductionOrder& InOrder, int32 InMaxProducible,
	EResourceType InBottleneck, bool bInMaterialBound)
{
	CachedOrderID = InOrder.OrderID;

	// DT_Project_* 가 표시명 단일 진실 — Order 에 박제된 ProductName 은 stage 시점 stale 값.
	FText DisplayName = FText::FromString(InOrder.ProductName);
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
		{
			bool bOk = false;
			const FProjectData Data = TableMgr->GetProjectData(InOrder.CompanyType, InOrder.ProjectIndex, bOk);
			if (bOk && !Data.ProjectName.IsEmpty())
			{
				DisplayName = Data.ProjectName;
			}
		}
	}
	if (NameText)
	{
		NameText->SetText(DisplayName);
	}

	if (GradeText)
	{
		const UEnum* GradeEnum = StaticEnum<EQualityGrade>();
		GradeText->SetText(FText::FromString(
			GradeEnum ? GradeEnum->GetNameStringByValue(static_cast<int64>(InOrder.Grade)) : TEXT("?")));
	}
	if (GradeBorder)
	{
		GradeBorder->SetBrushColor(GetGradeColor(InOrder.Grade));
	}

	if (MetaText)
	{
		float PerUnitSec = 0.0f;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
			{
				bool bRecipeOk = false;
				const FProductRecipeTable Recipe = TableMgr->GetProductRecipe(InOrder.CompanyType, InOrder.ProjectIndex, bRecipeOk);
				if (bRecipeOk) PerUnitSec = Recipe.ProductionTimeSec;
			}
		}
		MetaText->SetText(FText::Format(
			NSLOCTEXT("ProductionOrderRow", "MetaFmt", "남은 {0}개 · {1} / 개"),
			FText::AsNumber(InOrder.RemainingQuantity),
			UGlobalUtilFunctions::FormatDurationKorean(PerUnitSec)));
	}

	UpdateCapacityHint(InMaxProducible, InBottleneck, bInMaterialBound);
	LoadCoverAsync(InOrder.CompanyType, InOrder.ProjectIndex);
}

void UProductionOrderRowWidget::UpdateCapacityHint(int32 InMaxProducible, EResourceType InBottleneck,
	bool bInMaterialBound)
{
	if (!HintText) return;

	// 조사 폴백 "이(가)" 금지 — 조사가 필요 없는 어순으로 쓴다
	FText Bottleneck = FText::GetEmpty();
	if (InBottleneck != EResourceType::None)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UTableManagerSubsystem* TableMgr = GI->GetSubsystem<UTableManagerSubsystem>())
			{
				bool bOk = false;
				const FResourceInfo Info = TableMgr->GetResourceInfo(InBottleneck, bOk);
				if (bOk) Bottleneck = Info.DisplayName;
			}
		}
	}

	if (InMaxProducible <= 0)
	{
		HintText->SetVisibility(ESlateVisibility::HitTestInvisible);
		HintText->SetColorAndOpacity(HintInkBlocked);
		HintText->SetText(Bottleneck.IsEmpty()
			? NSLOCTEXT("ProductionOrderRow", "BlockedPlain", "재료 부족 ― 1개도 못 만듭니다")
			: FText::Format(NSLOCTEXT("ProductionOrderRow", "BlockedFmt", "{0} 부족 ― 1개도 못 만듭니다"), Bottleneck));
	}
	else if (bInMaterialBound)
	{
		HintText->SetVisibility(ESlateVisibility::HitTestInvisible);
		HintText->SetColorAndOpacity(HintInkTight);
		HintText->SetText(FText::Format(
			NSLOCTEXT("ProductionOrderRow", "TightFmt", "재료로 {0}개까지 ― 상한 {1}"),
			FText::AsNumber(InMaxProducible), Bottleneck));
	}
	else
	{
		HintText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

FLinearColor UProductionOrderRowWidget::GetGradeColor(EQualityGrade Grade) const
{
	const FLinearColor* Found = GradeColors.Find(Grade);
	return Found ? *Found : FLinearColor(0.0930f, 0.1119f, 0.1441f, 1.0f);
}

void UProductionOrderRowWidget::SetRowSelected(bool bInSelected)
{
	if (SelectRing)
	{
		SelectRing->SetVisibility(bInSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UProductionOrderRowWidget::LoadCoverAsync(ECompanyType CompanyType, int32 ProjectIndex)
{
	if (!CoverImage || ProjectIndex <= 0) return;

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr) return;

	bool bOk = false;
	const FProjectData Data = TableMgr->GetProjectData(CompanyType, ProjectIndex, bOk);
	if (!bOk || Data.Icon.IsNull()) return;

	const TSoftObjectPtr<UTexture2D> SoftTex = Data.Icon;
	if (UTexture2D* Already = SoftTex.Get())
	{
		CoverImage->SetBrushFromTexture(Already);
		return;
	}

	TWeakObjectPtr<UProductionOrderRowWidget> WeakThis(this);
	UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftTex.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda([WeakThis, SoftTex]()
		{
			UProductionOrderRowWidget* Strong = WeakThis.Get();
			if (Strong && Strong->CoverImage)
			{
				if (UTexture2D* Tex = SoftTex.Get())
				{
					Strong->CoverImage->SetBrushFromTexture(Tex);
				}
			}
		}));
}
