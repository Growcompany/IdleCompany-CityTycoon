#include "UI/Panel/PotentialOddsWidget.h"
#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/EmployeePotentialData.h"
#include "Enum/ItemType.h"
#include "Manager/TableManagerSubsystem.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Employee/PotentialOddsRowWidget.h"

namespace PotentialOddsUI
{
	// 직원창 명함 슬롯과 같은 순서 (종이/골드/블랙) — 열 인덱스가 곧 이 배열 인덱스
	static const EItemType CardTypes[3] = {
		EItemType::BusinessCardPaper,
		EItemType::BusinessCardGold,
		EItemType::BusinessCardBlack,
	};

	static const ELootBoxRarity RangeGrades[5] = {
		ELootBoxRarity::Common, ELootBoxRarity::Unusual, ELootBoxRarity::Rare,
		ELootBoxRarity::Epic,   ELootBoxRarity::Legendary,
	};
}

void UPotentialOddsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.AddDynamic(this, &UPotentialOddsWidget::OnCloseClicked);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.AddDynamic(this, &UPotentialOddsWidget::HandleDimClicked);
	}

	// Setup 이 먼저 온 경우(CreateWidget 직후 주입)만 다시 짓는다.
	// 아직이면 짓지 않는다 — 기본 등급(Common)으로 만든 틀린 표를 한 번 그리게 되고, 곧 Setup 이 올바른 값으로 다시 짓는다.
	if (bSetupDone)
	{
		Rebuild();
	}
}

void UPotentialOddsWidget::NativeDestruct()
{
	if (UIE_CloseButton)
	{
		UIE_CloseButton->OnCloseClicked.RemoveAll(this);
	}
	if (BackgroundBtn)
	{
		BackgroundBtn->OnClicked.RemoveDynamic(this, &UPotentialOddsWidget::HandleDimClicked);
	}
	Super::NativeDestruct();
}

void UPotentialOddsWidget::Setup(const FText& InEmployeeName, ELootBoxRarity InMaxAchieved)
{
	MaxAchieved = InMaxAchieved;
	bSetupDone = true;

	if (TargetNameText)
	{
		TargetNameText->SetText(InEmployeeName);
	}
	if (TargetGradeText)
	{
		TargetGradeText->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(MaxAchieved)));
		FLinearColor Col = FLootBoxRarityUtility::GetRarityColor(MaxAchieved);
		Col.A = 1.f;
		TargetGradeText->SetColorAndOpacity(FSlateColor(Col));
	}

	Rebuild();
}

void UPotentialOddsWidget::Rebuild()
{
	if (!OddsBox0 || !OddsBox1 || !OddsBox2)
	{
		return;
	}

	for (int32 i = 0; i < 3; ++i)
	{
		BuildColumn(i);
	}
	BuildValueRanges();

	if (BaseOddsText)
	{
		FString Line;
		for (const FPotentialRarityOdds& Row : UEmployeePotentialHelper::GetBaseRarityOdds())
		{
			if (!Line.IsEmpty()) { Line += TEXT(" · "); }
			Line += FString::Printf(TEXT("%s %g%%"), *FLootBoxRarityUtility::GetKoreanName(Row.Rarity), Row.Percent);
		}
		// U+2015 전각 줄표 — NEXON 에 em-dash(U+2014) 글리프가 없어 박스로 뜬다
		BaseOddsText->SetText(FText::FromString(FString::Printf(TEXT("기본 추첨 확률 ― %s"), *Line)));
	}
}

void UPotentialOddsWidget::BuildColumn(int32 Index)
{
	UVerticalBox* Boxes[3] = { OddsBox0, OddsBox1, OddsBox2 };
	UCommonTextBlock* Names[3] = { CardName0, CardName1, CardName2 };
	UCommonTextBlock* Caps[3] = { CardCap0, CardCap1, CardCap2 };
	UWidget* Locks[3] = { LockChip0, LockChip1, LockChip2 };

	UVerticalBox* Box = Boxes[Index];
	if (!Box) { return; }
	Box->ClearChildren();

	const EItemType CardType = PotentialOddsUI::CardTypes[Index];
	const ELootBoxRarity Ceiling = UEmployeePotentialHelper::GetCubeCeiling(CardType);

	if (Names[Index])
	{
		Names[Index]->SetText(GetItemTypeDisplayName(CardType));
	}
	if (Caps[Index])
	{
		Caps[Index]->SetText(FText::FromString(
			FLootBoxRarityUtility::GetKoreanName(Ceiling) + TEXT("까지")));
	}

	const TArray<FPotentialRarityOdds> Odds = UEmployeePotentialHelper::GetEffectiveRarityOdds(MaxAchieved, Ceiling);

	// 한 등급이 100% = 이 명함으로는 등급이 안 오른다. 알려주지 않으면 오르지 않는 등급에 계속 돈을 쓴다.
	// 행 생성 성공 여부와 무관하게 데이터로만 결정한다 — 아래 조기 반환에 걸려도 칩이 방치되지 않도록.
	if (Locks[Index])
	{
		Locks[Index]->SetVisibility(Odds.Num() <= 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	TSubclassOf<UUserWidget> RowClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::PotentialOddsRow) : nullptr;
	if (!RowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PotentialOdds] DT_WidgetClass 에 PotentialOddsRow 행이 없습니다 — 확률표가 빈 채로 뜹니다"));
		return;
	}

	for (const FPotentialRarityOdds& Row : Odds)
	{
		// CreateWidget<T> 는 타입 불일치 시 assert 로 즉사한다(Class.h IsA 체크) — Live Coding 패치 후
		// 클래스 포인터가 갈리면 그대로 크래시(2026-08-06 실측). 베이스로 만들고 Cast 로 조용히 걸러낸다.
		UPotentialOddsRowWidget* RowWidget = Cast<UPotentialOddsRowWidget>(CreateWidget(this, RowClass));
		if (!RowWidget) { continue; }
		RowWidget->SetOdds(Row.Rarity, Row.Percent);
		if (UVerticalBoxSlot* BoxSlot = Box->AddChildToVerticalBox(RowWidget))
		{
			BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}
	}
}

void UPotentialOddsWidget::BuildValueRanges()
{
	UCommonTextBlock* Grades[5] = { RangeGrade0, RangeGrade1, RangeGrade2, RangeGrade3, RangeGrade4 };
	UCommonTextBlock* Values[5] = { RangeValue0, RangeValue1, RangeValue2, RangeValue3, RangeValue4 };

	for (int32 i = 0; i < 5; ++i)
	{
		const ELootBoxRarity Grade = PotentialOddsUI::RangeGrades[i];
		if (Grades[i])
		{
			Grades[i]->SetText(FText::FromString(FLootBoxRarityUtility::GetKoreanName(Grade)));
			FLinearColor Ink = FLootBoxRarityUtility::GetRarityColor(Grade);   // 다크 면 = 원색 그대로
			Ink.A = 1.f;
			Grades[i]->SetColorAndOpacity(FSlateColor(Ink));
		}
		if (Values[i])
		{
			float Min = 0.f, Max = 0.f;
			UEmployeePotentialHelper::GetValueRangeForRarity(Grade, Min, Max);
			Values[i]->SetText(FText::FromString(FString::Printf(TEXT("%g~%g%%"), Min, Max)));
		}
	}
}

void UPotentialOddsWidget::OnCloseClicked()
{
	// 스택 push 가 아니라 뷰포트 오버레이 — 아래 직원창을 살려둔 채 자기만 걷는다
	RemoveFromParent();
}

void UPotentialOddsWidget::HandleDimClicked()
{
	RemoveFromParent();
}
