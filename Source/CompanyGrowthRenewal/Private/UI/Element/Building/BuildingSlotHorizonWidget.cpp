// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Building/BuildingSlotHorizonWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
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

void UBuildingSlotHorizonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MoveBtn)
	{
		MoveBtn->OnClicked().AddUObject(this, &UBuildingSlotHorizonWidget::HandleMoveClicked);
	}
}

void UBuildingSlotHorizonWidget::NativeDestruct()
{
	if (MoveBtn)
	{
		MoveBtn->OnClicked().RemoveAll(this);
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

	if (LevelText)
	{
		// 표기어는 "단계" — 밴드 뱃지/로드맵과 같은 어휘
		int32 Tier = 1;
		if (UCGGameInstance* SlotGI = UCGGameInstance::GetInstance())
		{
			if (USaveLoadManager* SlotSaveMgr = SlotGI->GetSubsystem<USaveLoadManager>())
			{
				Tier = SlotSaveMgr->GetBuildingTier(InBuilding->GetBuildingIndex());
			}
		}
		LevelText->SetText(FText::Format(NSLOCTEXT("Building", "SlotStage", "{0}단계"), FText::AsNumber(Tier)));
	}

	// 업종 시그니처색 — DT_CompanyInfo 단일 소스, IconPlate 테두리로 흡수 (업종 컬럼 대체)
	const ECompanyType Type = InBuilding->GetCompanyType();
	FLinearColor AccentColor = FLinearColor(0.35f, 0.39f, 0.45f);
	bool bHasIndustry = false;

	if (Type != ECompanyType::None && TableMgr)
	{
		bool bInfoFound = false;
		const FCompanyInfoTable Info = TableMgr->GetCompanyInfo(Type, bInfoFound);
		if (bInfoFound)
		{
			AccentColor = Info.AccentColor;
			bHasIndustry = true;
		}
	}

	if (IconPlate)
	{
		// 미지정 폴백 = WBP 기본 아웃라인과 동일 값 (통브러시 유지, 아웃라인만 주입)
		FSlateBrush PlateBrush = IconPlate->GetBrush();
		PlateBrush.OutlineSettings.Color = FSlateColor(bHasIndustry ? AccentColor : FLinearColor(0.09f, 0.11f, 0.15f, 0.18f));
		PlateBrush.OutlineSettings.Width = bHasIndustry ? 3.f : 1.f;
		IconPlate->SetBrush(PlateBrush);
	}

	// 직원 — 분모는 고용 게이트와 같은 수(인원 상한)여야 "더 뽑을 수 있나"가 카드에서 거짓 없이 읽힌다.
	UEmployeeManager* EmpMgr = GetGameInstance()->GetSubsystem<UEmployeeManager>();
	const int32 Current = EmpMgr ? EmpMgr->GetEmployeeCountInBuilding(CachedBuildingIndex) : 0;
	const int32 Capacity = EmpMgr ? EmpMgr->GetBuildingEmployeeCapacity(CachedBuildingIndex, /*bLogIfZero*/ false) : 0;
	if (EmployeeNumText)
	{
		EmployeeNumText->SetText(FText::FromString(
			Capacity > 0 ? FString::Printf(TEXT("%d / %d"), Current, Capacity)
			             : FString::Printf(TEXT("%d"), Current)));
	}
	if (CapacityBar)
	{
		CapacityBar->SetPercent(Capacity > 0 ? FMath::Clamp(static_cast<float>(Current) / Capacity, 0.f, 1.f) : 0.f);
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
	IncomeText->SetText(FText::FromString(
		UGlobalUtilFunctions::AbbreviateNumber(static_cast<int64>(RevPerSec)).ToString() + TEXT("/초")));
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
