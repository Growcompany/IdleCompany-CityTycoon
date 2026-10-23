// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Cards/ProductionOrderRowWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/ProjectDataTable.h"
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
	EResourceType InBottleneck, bool bInSupplyBound, const FText& InProductInputName,
	float InEffectiveTimePerUnitSec)
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

	// 1개 시간은 고른 뒤 상세에서 보여 준다 ― 레일 줄은 등급·남은 수량만(spec §2.10)
	(void)InEffectiveTimePerUnitSec;
	CachedGrade = InOrder.Grade;
	CachedRemaining = InOrder.RemainingQuantity;

	UpdateCapacityHint(InMaxProducible, InBottleneck, bInSupplyBound, InProductInputName);
	LoadCoverAsync(InOrder.CompanyType, InOrder.ProjectIndex);
}

void UProductionOrderRowWidget::ApplyMetaLine(int32 InMaxProducible, EResourceType InBottleneck, bool bInSupplyBound,
	const FText& InProductInputName)
{
	if (!MetaText) return;
	if (InMaxProducible <= 0)
	{
		// 칩이 막으면 「재료」라고 하지 않는다 ― 광석을 캐러 가게 된다
		const bool bPartBound = bInSupplyBound && InBottleneck == EResourceType::None && !InProductInputName.IsEmpty();
		MetaText->SetText(bPartBound
			? NSLOCTEXT("ProductionOrderRow", "PartShort", "부품 부족")
			: NSLOCTEXT("ProductionOrderRow", "MaterialShort", "재료 부족"));
		MetaText->SetColorAndOpacity(MetaInkBlocked);
		return;
	}
	const UEnum* GradeEnum = StaticEnum<EQualityGrade>();
	MetaText->SetText(FText::Format(NSLOCTEXT("ProductionOrderRow", "MetaGradeFmt", "{0} 등급 · 남은 {1}개"),
		FText::FromString(GradeEnum ? GradeEnum->GetNameStringByValue(static_cast<int64>(CachedGrade)) : TEXT("?")),
		FText::AsNumber(CachedRemaining)));
	MetaText->SetColorAndOpacity(MetaInk);
}

void UProductionOrderRowWidget::UpdateCapacityHint(int32 InMaxProducible, EResourceType InBottleneck,
	bool bInSupplyBound, const FText& InProductInputName)
{
	ApplyMetaLine(InMaxProducible, InBottleneck, bInSupplyBound, InProductInputName);
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

	// 자원 이름이 비는 것은 제품 입력(칩)이 상한을 정했다는 신호다 — 호출부가 그 때만 이름을 넘긴다.
	// 여기서 "재료"라고 말하면 칩이 모자란 플레이어가 광석을 캐러 간다.
	const bool bProductBound = bInSupplyBound
		&& (InBottleneck == EResourceType::None) && !InProductInputName.IsEmpty();

	if (InMaxProducible <= 0)
	{
		HintText->SetVisibility(ESlateVisibility::HitTestInvisible);
		HintText->SetColorAndOpacity(HintInkBlocked);
		FText Label = bProductBound ? InProductInputName : Bottleneck;
		if (Label.IsEmpty())
		{
			// 이름을 못 구한 자리는 무엇이 막았는지만 — 칩 병목에 "재료"를 쓰지 않는다
			Label = (bInSupplyBound && InBottleneck == EResourceType::None)
				? NSLOCTEXT("ProductionOrderRow", "PartWord", "부품")
				: NSLOCTEXT("ProductionOrderRow", "MaterialWord", "재료");
		}
		HintText->SetText(FText::Format(
			NSLOCTEXT("ProductionOrderRow", "BlockedFmt", "{0} 부족 ― 1개도 못 만듭니다"), Label));
	}
	else if (bInSupplyBound)
	{
		HintText->SetVisibility(ESlateVisibility::HitTestInvisible);
		HintText->SetColorAndOpacity(HintInkTight);
		const FText Label = bProductBound ? InProductInputName : Bottleneck;
		if (Label.IsEmpty())
		{
			// 빈 이름을 슬롯에 그대로 넣으면 "상한 " 으로 끝나는 매달린 문장이 나온다 — 수치만 쓴다
			HintText->SetText(FText::Format(
				NSLOCTEXT("ProductionOrderRow", "TightPlainFmt", "{0}개까지 만들 수 있습니다"),
				FText::AsNumber(InMaxProducible)));
		}
		else if (bProductBound)
		{
			HintText->SetText(FText::Format(
				NSLOCTEXT("ProductionOrderRow", "TightPartFmt", "부품으로 {0}개까지 ― 상한 {1}"),
				FText::AsNumber(InMaxProducible), Label));
		}
		else
		{
			HintText->SetText(FText::Format(
				NSLOCTEXT("ProductionOrderRow", "TightFmt", "재료로 {0}개까지 ― 상한 {1}"),
				FText::AsNumber(InMaxProducible), Label));
		}
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
