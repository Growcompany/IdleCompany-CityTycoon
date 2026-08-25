// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Element/Building/UpgradeSlot.h"

#include "CommonTextBlock.h"
#include "CommonBorder.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "Global/GlobalUtilFunctions.h"
#include "Manager/ResourceItemManager.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "NiagaraSystemWidget.h"
#include "NiagaraUIComponent.h"

void UUpgradeSlot::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyDesignTimeSettings();
}

void UUpgradeSlot::NativeConstruct()
{
	Super::NativeConstruct();
}

void UUpgradeSlot::ApplyDesignTimeSettings()
{
	// 아이콘 설정
	if (IconImage && Icon.ToSoftObjectPath().IsValid())
	{
		UTexture2D* IconTexture = Icon.LoadSynchronous();
		if (IconTexture)
		{
			IconImage->SetBrushFromTexture(IconTexture);
		}
	}

	// 제목 텍스트 설정
	if (DescriptionText)
	{
		DescriptionText->SetText(Description);
	}

	// 설명 텍스트 설정
	if (SubDescriptionText)
	{
		if (SubDescription.IsEmpty())
		{
			SubDescriptionText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			SubDescriptionText->SetText(SubDescription);
			SubDescriptionText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}

	// 잠금 조건 텍스트 설정
	if (LockConditionTextBlock)
	{
		LockConditionTextBlock->SetText(LockConditionText);
	}

	// 잠금 상태 미리보기 (에디터용)
	if (LockedBorder)
	{
		LockedBorder->SetVisibility(bPreviewLocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (LockedOverlay)
	{
		LockedOverlay->SetVisibility(bPreviewLocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 버튼 텍스트 설정
	if (UpgradeBtn && !ButtonText.IsEmpty())
	{
		UpgradeBtn->SetButtonText(ButtonText);
	}

	// 부가 병기는 값 주입 전까지 숨김 (금고 외 슬롯은 계속 숨김 유지)
	if (SecondaryValueText)
	{
		SecondaryValueText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UUpgradeSlot::SetSecondaryAnnotation(const FText& InText)
{
	if (!SecondaryValueText)
	{
		return;
	}
	if (InText.IsEmpty())
	{
		SecondaryValueText->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		SecondaryValueText->SetText(InText);
		SecondaryValueText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UUpgradeSlot::SetLevelBadgeOverride(const FText& InText)
{
	LevelBadgeOverride = InText;
}

void UUpgradeSlot::UpdateInfo(int32 Level, float CurrentValue, float NextValue, int64 Cost, bool bIsLocked, int32 MaxLevel)
{
	UpdateInfo(Level, CurrentValue, NextValue, Cost, bIsLocked, MaxLevel,
		/*bBulkMode=*/false, /*BulkCount=*/1, /*BulkTotalCost=*/Cost, /*AvailableAmount=*/0);
}

void UUpgradeSlot::UpdateInfo(int32 Level, float CurrentValue, float NextValue, int64 Cost, bool bIsLocked, int32 MaxLevel,
	bool bBulkMode, int32 BulkCount, int64 BulkTotalCost, int64 AvailableAmount)
{
	const bool bIsMaxLevel = (MaxLevel > 0 && Level >= MaxLevel);

	// 레벨 텍스트 업데이트
	if (LevelText)
	{
		if (!LevelBadgeOverride.IsEmpty())
		{
			LevelText->SetText(LevelBadgeOverride);
		}
		else if (MaxLevel > 0)
		{
			// 최대 레벨 정보가 있으면 "Lv.현재레벨 / 최대레벨" 형식으로 표시
			LevelText->SetText(FText::Format(
				FText::FromString(TEXT("Lv.{0} / {1}")), Level, MaxLevel));
		}
		else
		{
			// 최대 레벨 정보가 없으면 기존 형식 유지
			LevelText->SetText(FText::Format(
				FText::FromString(TEXT("Lv.{0}")), Level));
		}
	}

	auto FormatNumber = [this](float Value) -> FText
	{
		if (bAbbreviateValue)
		{
			return UGlobalUtilFunctions::AbbreviateNumberFloat(Value);
		}
		return bIsInteger
			? FText::AsNumber(FMath::RoundToInt(Value))
			: FText::AsNumber(Value, &FNumberFormattingOptions::DefaultNoGrouping());
	};

	auto FormatValue = [this, &FormatNumber](float Value, const TCHAR* Format)
	{
		return FText::Format(FText::FromString(Format),
			FText::FromString(ValuePrefix), FormatNumber(Value), FText::FromString(ValueUnit));
	};

	// 현재 값 업데이트
	if (CurrentValueText)
	{
		CurrentValueText->SetText(FormatValue(CurrentValue, TEXT("{0}{1}{2} ")));
	}

	// 다음 레벨 값 업데이트
	if (NextValueText)
	{
		NextValueText->SetText(FormatValue(NextValue, TEXT("{0}{1}{2}")));
	}

	// 증가분 표기 — 원시값 두 개만으론 "나아지고 있는지" 가 안 읽힌다
	if (DeltaValueText)
	{
		const float Delta = NextValue - CurrentValue;
		// 임계값 = AsNumber 기본 소수 3자리의 반올림 경계. 이보다 작으면 "(+0)" 이라는 거짓말이 찍힌다
		if (!bShowDelta || bIsMaxLevel || FMath::IsNearlyZero(Delta, 5.e-4f))
		{
			DeltaValueText->SetVisibility(ESlateVisibility::Collapsed);
			CachedDeltaString.Reset();
		}
		else
		{
			const float AbsDelta = FMath::Abs(Delta);
			const FString NumberPart = FormatNumber(AbsDelta).ToString();

			const FString NewDelta = FString::Printf(TEXT("(%s%s%s)"),
				Delta > 0.f ? TEXT("+") : TEXT("-"), *NumberPart, *ValueUnit);

			if (NewDelta != CachedDeltaString)
			{
				CachedDeltaString = NewDelta;
				DeltaValueText->SetText(FText::FromString(NewDelta));
			}
			DeltaValueText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}

	// 잠금 상태 UI 업데이트
	if (LockedBorder)
	{
		LockedBorder->SetVisibility(bIsLocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (LockedOverlay)
	{
		LockedOverlay->SetVisibility(bIsLocked ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 비용 업데이트 및 활성화/비활성화 (UpgradeBtnWidget 사용)
	if (UpgradeBtn)
	{
		UpgradeBtn->SetMaxLevelState(bIsMaxLevel);

		// 벌크 모드는 버튼 비용/부족 판정/부족 토스트 전부 합산 비용 기준
		const int64 ButtonCost = bBulkMode ? BulkTotalCost : Cost;

		if (bIsMaxLevel || bIsLocked)
		{
			// MAX/잠금은 무조건 비활성화, 비용은 체크 안함 (MAX 는 비용 행 자체가 Collapsed)
			UpgradeBtn->SetLockedState(true);
			UpgradeBtn->SetCost(ButtonCost, CostResourceType, false);
		}
		else
		{
			// 자금 부족은 잠금 축을 건드리지 않으므로, 잠금 해제 복귀는 여기서 명시적으로
			UpgradeBtn->SetLockedState(false);
			UpgradeBtn->SetCost(ButtonCost, CostResourceType, true);
		}
	}
}

void UUpgradeSlot::SetPunchSuppressed(bool bSuppressed)
{
	if (UpgradeBtn)
	{
		UpgradeBtn->SetPunchSuppressed(bSuppressed);
	}
}

void UUpgradeSlot::SetIconTexture(UTexture2D* InTexture)
{
	if (IconImage && InTexture)
	{
		IconImage->SetBrushFromTexture(InTexture);
		IconImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UUpgradeSlot::SetRuntimePresentation(const FText& InTitle, const FText& InDescription,
	EResourceType InCostResourceType)
{
	Description = InTitle;
	SubDescription = InDescription;
	CostResourceType = InCostResourceType;

	if (DescriptionText)
	{
		DescriptionText->SetText(Description);
	}

	if (SubDescriptionText)
	{
		SubDescriptionText->SetText(SubDescription);
		SubDescriptionText->SetVisibility(SubDescription.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible);
	}
}

void UUpgradeSlot::PlayUpgradeEffect(bool bPlaySound)
{
	if (UpgradeNiagaraEffect)
	{
		UpgradeNiagaraEffect->DeactivateSystem();
		UpgradeNiagaraEffect->ActivateSystem(true);
	}

	if (!bPlaySound)
	{
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::UpgradeSuccess);
		}
	}
}

UButton* UUpgradeSlot::GetRawButton() const
{
	return UpgradeBtn ? UpgradeBtn->GetButton() : nullptr;
}
