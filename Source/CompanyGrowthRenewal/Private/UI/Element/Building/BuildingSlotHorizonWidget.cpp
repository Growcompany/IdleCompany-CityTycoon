// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Building/BuildingSlotHorizonWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "CommonTextBlock.h"
#include "Framework/Text/TextLayout.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Enum/CompanyType.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/EmployeeManager.h"
#include "Manager/ProjectOperationManager.h"
#include "Manager/SaveLoadManager.h"
#include "Core/CGGameInstance.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/BuildableCardTable.h"
#include "Table/CompanyInfoTable.h"
#include "UI/Element/HQ/HQPanelTypes.h"
#include "Engine/Texture2D.h"

void UBuildingSlotHorizonWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UCommonTextBlock* NameText = BuildingNameButton
		? Cast<UCommonTextBlock>(BuildingNameButton->GetWidgetFromName(TEXT("ButtonText"))) : nullptr)
	{
		NameText->SetJustification(ETextJustify::Left);
		NameText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		if (UOverlaySlot* NameSlot = Cast<UOverlaySlot>(NameText->Slot))
		{
			NameSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	if (MoveBtn)
	{
		MoveBtn->OnClicked().AddUObject(this, &UBuildingSlotHorizonWidget::HandleMoveClicked);
	}
	if (BuildingNameButton)
	{
		BuildingNameButton->OnClicked().AddUObject(this, &UBuildingSlotHorizonWidget::HandleMoveClicked);
	}
}

void UBuildingSlotHorizonWidget::NativeDestruct()
{
	if (MoveBtn)
	{
		MoveBtn->OnClicked().RemoveAll(this);
	}
	if (BuildingNameButton)
	{
		BuildingNameButton->OnClicked().RemoveAll(this);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UProjectOperationManager* OpMgr = GI->GetSubsystem<UProjectOperationManager>())
		{
			OpMgr->OnExpectedRevenueChanged.RemoveAll(this);
		}
	}

	Super::NativeDestruct();
}

void UBuildingSlotHorizonWidget::SetBuildingData(ABuildingBaseActor* InBuilding)
{
	if (!InBuilding) return;

	CachedBuildingIndex = InBuilding->GetBuildingIndex();

	// DataTable에서 건물 정보 조회 (이름, 아이콘)
	UTableManagerSubsystem* TableMgr = GetGameInstance()->GetSubsystem<UTableManagerSubsystem>();
	FBuildableCardTable CardData;
	bool bFound = TableMgr && TableMgr->GetBuildableInfo(InBuilding->GetBuildingID(), CardData);

	if (BuildingNameButton)
	{
		FText DisplayName = bFound
			? FText::FromName(CardData.Name)
			: FText::FromString(FString::Printf(TEXT("건물 %d"), CachedBuildingIndex + 1));
		BuildingNameButton->SetButtonText(DisplayName);
	}

	// 건물 아이콘
	if (IconImage && bFound && !CardData.UIIcon.IsNull())
	{
		if (UTexture2D* Tex = CardData.UIIcon.LoadSynchronous())
		{
			IconImage->SetBrushFromTexture(Tex);
		}
	}

	// 업종 이름·테두리 색·배지 아이콘은 같은 DT_CompanyInfo 행에서 읽는다.
	const ECompanyType Type = InBuilding->GetCompanyType();
	FText IndustryName;
	FLinearColor AccentColor = FLinearColor(0.35f, 0.39f, 0.45f);
	UTexture2D* IndustryIcon = nullptr;
	bool bHasIndustry = false;
	if (Type != ECompanyType::None && TableMgr)
	{
		bool bInfoFound = false;
		const FCompanyInfoTable Info = TableMgr->GetCompanyInfo(Type, bInfoFound);
		if (bInfoFound)
		{
			IndustryName = Info.DisplayName;
			AccentColor = HQIndustryPalette::RibbonTop(Info.AccentColor);
			IndustryIcon = Info.PanelIcon.IsNull() ? nullptr : Info.PanelIcon.LoadSynchronous();
			if (!IndustryIcon)
			{
				UE_LOG(LogTemp, Warning, TEXT("[BuildingSlot] DT_CompanyInfo.PanelIcon missing for company type %d"), static_cast<int32>(Type));
			}
			bHasIndustry = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BuildingSlot] DT_CompanyInfo row missing for company type %d"), static_cast<int32>(Type));
		}
	}

	if (LevelText)
	{
		// 산업명이 비었으면 단계만 표시한다.
		int32 Tier = 1;
		if (UCGGameInstance* SlotGI = UCGGameInstance::GetInstance())
		{
			if (USaveLoadManager* SlotSaveMgr = SlotGI->GetSubsystem<USaveLoadManager>())
			{
				Tier = SlotSaveMgr->GetBuildingTier(InBuilding->GetBuildingIndex());
			}
		}
		const FText StageText = FText::Format(NSLOCTEXT("Building", "SlotStage", "{0}단계"), FText::AsNumber(Tier));
		LevelText->SetText(IndustryName.IsEmpty()
			? StageText
			: FText::Format(NSLOCTEXT("Building", "SlotIndustryStage", "{0} · {1}"), IndustryName, StageText));
	}

	// 업종 시그니처색을 IconPlate 테두리에 표시한다.
	if (IconPlate)
	{
		// 미지정 폴백 = WBP 기본 아웃라인과 동일 값 (통브러시 유지, 아웃라인만 주입)
		FSlateBrush PlateBrush = IconPlate->GetBrush();
		PlateBrush.OutlineSettings.Color = FSlateColor(bHasIndustry ? AccentColor : FLinearColor(0.09f, 0.11f, 0.15f, 0.18f));
		PlateBrush.OutlineSettings.Width = bHasIndustry ? 5.f : 1.f;
		IconPlate->SetBrush(PlateBrush);
	}

	const bool bShowBadge = bHasIndustry && IndustryIcon;
	if (IndustryBadgeRing)
	{
		IndustryBadgeRing->SetVisibility(bShowBadge ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		FSlateBrush RingBrush = IndustryBadgeRing->GetBrush();
		RingBrush.OutlineSettings.Color = FSlateColor(AccentColor);
		IndustryBadgeRing->SetBrush(RingBrush);
	}
	if (IndustryBadgeIcon)
	{
		IndustryBadgeIcon->SetVisibility(bShowBadge ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bShowBadge) IndustryBadgeIcon->SetBrushFromTexture(IndustryIcon, false);
	}

	// 직원 — 분모는 고용 게이트와 같은 수(인원 상한)여야 "더 뽑을 수 있나"가 카드에서 거짓 없이 읽힌다.
	UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	const int32 Current = EmpMgr ? EmpMgr->GetEmployeeCountInBuilding(CachedBuildingIndex) : 0;
	const int32 Capacity = EmpMgr ? EmpMgr->GetBuildingEmployeeCapacity(CachedBuildingIndex, /*bLogIfZero*/ false) : 0;
	if (EmployeeCapText)
	{
		if (EmployeeNumText) EmployeeNumText->SetText(FText::AsNumber(Current));
		EmployeeCapText->SetVisibility(Capacity > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		EmployeeCapText->SetText(FText::FromString(FString::Printf(TEXT("/ %d"), Capacity)));
	}
	else if (EmployeeNumText)
	{
		EmployeeNumText->SetText(FText::FromString(
			Capacity > 0 ? FString::Printf(TEXT("%d / %d"), Current, Capacity)
			             : FString::Printf(TEXT("%d"), Current)));
	}
	// 실수익(초당) — 표시 정의(감쇠O/진동X) 단일 경로. 운영 없으면 0. 브로드캐스트 구독으로 열려 있는 동안 갱신
	if (UProjectOperationManager* OpMgr = GetGameInstance()->GetSubsystem<UProjectOperationManager>())
	{
		SetIncomePerSec(OpMgr->GetBuildingDisplayNetPerSec(CachedBuildingIndex));

		OpMgr->OnExpectedRevenueChanged.RemoveAll(this);  // 재사용/재오픈 중복구독 가드
		OpMgr->OnExpectedRevenueChanged.AddUObject(this, &UBuildingSlotHorizonWidget::HandleExpectedRevenueChanged);
	}
	else
	{
		SetIncomePerSec(0.f);
	}
}

void UBuildingSlotHorizonWidget::SetIncomePerSec(float RevPerSec)
{
	if (!IncomeText) return;
	const FText Amount = UGlobalUtilFunctions::AbbreviateNumber(static_cast<int64>(RevPerSec));
	if (IncomeUnitText)
	{
		IncomeText->SetText(Amount);
		IncomeUnitText->SetText(NSLOCTEXT("Building", "IncomePerSecUnit", "/초"));
		return;
	}
	IncomeText->SetText(FText::FromString(Amount.ToString() + TEXT("/초")));
}

void UBuildingSlotHorizonWidget::HandleExpectedRevenueChanged(int32 InBuildingID, float NewRate)
{
	if (InBuildingID != CachedBuildingIndex) return;
	SetIncomePerSec(NewRate);
}

void UBuildingSlotHorizonWidget::HandleMoveClicked()
{
	OnMoveClicked.ExecuteIfBound(CachedBuildingIndex);
}
