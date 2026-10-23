#include "UI/Element/Employee/OfficeEmployeeStatRowWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Data/EmployeeStatsData.h"
#include "Manager/EmployeeManager.h"
#include "UI/Element/Buttons/ButtonWidget.h"

namespace OfficeEmployeeStatRow
{
	// 직원창 능력치 카운트업과 같은 길이
	constexpr float ValueCountDuration = 0.4f;
	constexpr float ValueCountStep = 1.f / 30.f;
}

void UOfficeEmployeeStatRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (InvestButton)
	{
		EnabledLabelColor = InvestButton->GetTextColor();
		CostText = Cast<UTextBlock>(InvestButton->GetWidgetFromName(TEXT("CostText")));
		if (!CostText)
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeeStatRow] 투자 버튼에 CostText 가 없습니다 — 비용 칩이 실제 비용을 못 보여 줍니다"));
		}
	}
}

void UOfficeEmployeeStatRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UGameInstance* GI = GetGameInstance())
	{
		EmployeeManager = GI->GetSubsystem<UEmployeeManager>();
	}
	if (InvestButton)
	{
		InvestButton->OnClicked().AddUObject(this, &ThisClass::HandleInvestClicked);
	}
	ApplyStaticContent();
	Refresh();
}

void UOfficeEmployeeStatRowWidget::NativeDestruct()
{
	if (InvestButton)
	{
		InvestButton->OnClicked().RemoveAll(this);
	}
	StopValueCount();
	Super::NativeDestruct();
}

void UOfficeEmployeeStatRowWidget::StopValueCount()
{
	ValueCount.Finish();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ValueCountTimer);
	}
}

void UOfficeEmployeeStatRowWidget::StepValueCount()
{
	const float StepSeconds = OfficeEmployeeStatRow::ValueCountStep;
	const int32 Shown = FMath::RoundToInt(ValueCount.Tick(StepSeconds));
	if (StatCurrentText)
	{
		// 포맷은 Refresh 와 같은 단일 출처(GetStatFeltText) — 소수·% 규칙을 복제하지 않는다
		StatCurrentText->SetText(UEmployeeStatsHelper::GetStatFeltText(static_cast<uint8>(Stat), Shown));
	}
	if (!ValueCount.IsPlaying())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ValueCountTimer);
		}
	}
}

void UOfficeEmployeeStatRowWidget::Configure(int32 InEmployeeId, EEmployeeStatIndex InStat)
{
	const bool bStatChanged = Stat != InStat;
	if (EmployeeId != InEmployeeId || bStatChanged)
	{
		LastEffectiveValue = INDEX_NONE;
		StopValueCount();
	}
	EmployeeId = InEmployeeId;
	Stat = InStat;
	ApplyStaticContent();
	Refresh();
}

void UOfficeEmployeeStatRowWidget::ApplyStaticContent()
{
	const int32 StatIndex = static_cast<int32>(Stat);
	if (StatNameText)
	{
		StatNameText->SetText(UEmployeeStatsHelper::GetStatDisplayName(static_cast<uint8>(StatIndex)));
	}
	if (StatDescText)
	{
		StatDescText->SetText(StatHints.IsValidIndex(StatIndex) ? StatHints[StatIndex] : FText::GetEmpty());
	}
	if (StatIcon)
	{
		UTexture2D* IconTexture = StatIcons.IsValidIndex(StatIndex) && !StatIcons[StatIndex].IsNull()
			? StatIcons[StatIndex].LoadSynchronous() : nullptr;
		if (IconTexture)
		{
			// WBP 의 글리프 크기를 유지한다
			StatIcon->SetBrushFromTexture(IconTexture, false);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[OfficeEmployeeStatRow] 능력치 %d 아이콘이 WBP 기본값에 없습니다"), StatIndex);
		}
	}
}

void UOfficeEmployeeStatRowWidget::Refresh()
{
	const FEmployeeInstance* Employee = (EmployeeManager && EmployeeId != INDEX_NONE)
		? EmployeeManager->FindEmployee(EmployeeId) : nullptr;
	const uint8 StatIndex = static_cast<uint8>(Stat);

	if (!Employee)
	{
		if (StatCurrentText) StatCurrentText->SetText(FText::GetEmpty());
		if (StatNextText) StatNextText->SetText(FText::GetEmpty());
		if (StatBonusText) StatBonusText->SetVisibility(ESlateVisibility::Collapsed);
		if (StatLevelText) StatLevelText->SetText(FText::GetEmpty());
		if (CostText) CostText->SetText(FText::GetEmpty());
		if (InvestButton)
		{
			InvestButton->SetIsEnabled(false);
			InvestButton->SetTextColor(DisabledLabelColor);
		}
		LastEffectiveValue = INDEX_NONE;
		StopValueCount();
		return;
	}

	const int32 EnhanceBonus = UEmployeeTypeHelper::GetEnhanceStatBonus(Employee->EnhancementLevel);
	const int32 BaseValue = UEmployeeStatsHelper::GetStatValueByIndex(Employee->Stats, StatIndex);
	const int32 EffectiveValue = static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(BaseValue) + EnhanceBonus, MIN_int32, MAX_int32));
	const bool bCanGrow = BaseValue < MAX_int32;
	const int32 NextEffectiveValue = EffectiveValue < MAX_int32 ? EffectiveValue + 1 : EffectiveValue;

	if (StatCurrentText)
	{
		if (LastEffectiveValue != INDEX_NONE && LastEffectiveValue != EffectiveValue)
		{
			ValueCount.Start(static_cast<float>(LastEffectiveValue), static_cast<float>(EffectiveValue),
				OfficeEmployeeStatRow::ValueCountDuration);
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(ValueCountTimer, this, &ThisClass::StepValueCount,
					OfficeEmployeeStatRow::ValueCountStep, true);
			}
			else
			{
				ValueCount.Finish();
				StatCurrentText->SetText(UEmployeeStatsHelper::GetStatFeltText(StatIndex, EffectiveValue));
			}
		}
		else if (!ValueCount.IsPlaying())
		{
			StatCurrentText->SetText(UEmployeeStatsHelper::GetStatFeltText(StatIndex, EffectiveValue));
		}
	}
	LastEffectiveValue = EffectiveValue;

	const ESlateVisibility NextVisibility = bCanGrow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (StatNextText)
	{
		StatNextText->SetText(bCanGrow ? UEmployeeStatsHelper::GetStatFeltText(StatIndex, NextEffectiveValue) : FText::GetEmpty());
		StatNextText->SetVisibility(NextVisibility);
	}
	if (StatArrowText)
	{
		StatArrowText->SetVisibility(NextVisibility);
	}

	if (StatBonusText)
	{
		// 업무속도·이탈 확률은 감소가 이득이라 부호는 실제 방향 그대로
		const FText DeltaText = UEmployeeStatsHelper::GetStatFeltDeltaText(StatIndex, BaseValue, EffectiveValue);
		StatBonusText->SetText(FText::Format(NSLOCTEXT("OfficeEmployeePanel", "StarforceBonus", "★{0}"), DeltaText));
		// 표시 소수점에서 0 으로 접히는 보정("+0.00초")은 보정이 아니다 — Collapsed 라야 글자 묶음이 행 가운데에 선다
		const bool bDeltaReads = DeltaText.ToString().FindLastCharByPredicate(
			[](TCHAR Character) { return Character >= TEXT('1') && Character <= TEXT('9'); }) != INDEX_NONE;
		StatBonusText->SetVisibility(bDeltaReads ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (StatLevelText)
	{
		StatLevelText->SetText(FText::Format(NSLOCTEXT("OfficeEmployeePanel", "StatLevel", "Lv.{0}"),
			FText::AsNumber(EmployeeManager->GetStatLevel(EmployeeId, Stat))));
	}
	if (CostText)
	{
		CostText->SetText(FText::AsNumber(EmployeeManager->GetStatPointCost(EmployeeId, Stat)));
	}

	if (InvestButton)
	{
		const bool bCanInvest = EmployeeManager->CanAllocateStatPoint(EmployeeId, Stat);
		InvestButton->SetIsEnabled(bCanInvest);
		InvestButton->SetTextColor(bCanInvest ? EnabledLabelColor : DisabledLabelColor);
	}
}

void UOfficeEmployeeStatRowWidget::HandleInvestClicked()
{
	if (!EmployeeManager || EmployeeId == INDEX_NONE)
	{
		return;
	}
	// 화면 갱신은 AllocateStatPoint 의 OnEmployeeStatsChanged 를 받은 패널이 한다
	EmployeeManager->AllocateStatPoint(EmployeeId, Stat);
}
