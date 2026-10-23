// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Building/TraitSlotWidget.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "CommonTextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Element/Common/ResourceWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Manager/ResourceItemManager.h"
#include "Table/BuildingTraitTable.h"
#include "Enum/BuildingTraitTarget.h"

namespace TraitSlotColor
{
	// 등급색 유틸은 sRGB 분수를 돌려준다 ― 섞기는 sRGB 에서 하고 머티리얼에는 linear 로 넣는다
	static FLinearColor MixToLinear(const FLinearColor& BaseSRGB, const FLinearColor& RaritySRGB, float RarityMix)
	{
		return FLinearColor(FMath::Lerp(BaseSRGB, RaritySRGB, RarityMix).ToFColor(false));
	}

	// M_UIPanel_Rounded 계열 ― Outer = 가장자리 림, Inner = 림 안쪽 밝은 선
	static void SetPanelColors(UImage* Image, const FLinearColor& Top, const FLinearColor& Bottom,
		const FLinearColor& OuterRim, const FLinearColor& InnerRim)
	{
		if (!Image)
		{
			return;
		}
		if (UMaterialInstanceDynamic* MID = Image->GetDynamicMaterial())
		{
			MID->SetVectorParameterValue(TEXT("FillTop"), Top);
			MID->SetVectorParameterValue(TEXT("FillBottom"), Bottom);
			MID->SetVectorParameterValue(TEXT("OuterColor"), OuterRim);
			MID->SetVectorParameterValue(TEXT("InnerColor"), InnerRim);
		}
	}

	// M_UI_ChipLine 은 정점 색을 안 읽어 위젯 틴트가 무시된다 ― 색은 LineCol, 농도는 BaseA 로 넣는다
	static void SetLineColor(UImage* Image, const FColor& ColorSRGB)
	{
		if (!Image)
		{
			return;
		}
		if (UMaterialInstanceDynamic* MID = Image->GetDynamicMaterial())
		{
			MID->SetVectorParameterValue(TEXT("LineCol"), FLinearColor(FColor(ColorSRGB.R, ColorSRGB.G, ColorSRGB.B)));
			MID->SetScalarParameterValue(TEXT("BaseA"), ColorSRGB.A / 255.f);
		}
	}
}

void UTraitSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SlotButton)
	{
		SlotButton->OnClicked().AddUObject(this, &UTraitSlotWidget::HandleClicked);
	}
}

void UTraitSlotWidget::NativeDestruct()
{
	if (SlotButton)
	{
		SlotButton->OnClicked().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UTraitSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyDesignTimePreview();
}

void UTraitSlotWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ApplyDesignTimePreview();
}

void UTraitSlotWidget::ApplyDesignTimePreview()
{
	// 디자이너 프리뷰 전용. 런타임은 SetEmpty/SetEquipped/SetLocked 가 처리하므로 건드리지 않음.
	if (!IsDesignTime()) return;

	ApplyVisualForState(PreviewState);
	if (PreviewState == ETraitSlotVisualState::Equipped)
	{
		// 아이콘·효과 문구는 WBP 에 저작된 샘플 값이 보인다
		ApplyRarityColors(PreviewRarity);
	}
	if (SelectedBorder)
	{
		SelectedBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTraitSlotWidget::HandleClicked()
{
	OnSlotClicked.ExecuteIfBound(SlotIndex);
}

void UTraitSlotWidget::SetEmpty()
{
	State = ETraitSlotVisualState::Empty;
	EquippedTraitID = NAME_None;
	if (SlotCaptionText)
	{
		SlotCaptionText->SetText(FText::FromString(TEXT("장착 가능")));
	}
	ApplyStateVisibility();
}

void UTraitSlotWidget::SetEquipped(FName InTraitID)
{
	State = ETraitSlotVisualState::Equipped;
	EquippedTraitID = InTraitID;

	UTableManagerSubsystem* TableMgr = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	FBuildingTraitTableRow Row;
	if (TableMgr && TableMgr->GetBuildingTraitData(InTraitID, Row))
	{
		if (EquippedIcon)
		{
			UTexture2D* Tex = Row.Icon.IsNull() ? nullptr : Row.Icon.LoadSynchronous();
			EquippedIcon->SetBrushFromTexture(Tex, /*bMatchSize=*/false);
		}
		ApplyRarityColors(Row.Rarity);

		// 상세 창 라벨과 같은 (대상, 수치) 에서 나온 두 줄
		const EBuildingTraitTarget Target = GetTargetForCategory(Row.Category);
		if (EffectNameText)
		{
			EffectNameText->SetText(GetTraitTargetDisplayName(Target));
		}
		if (EffectValueText)
		{
			EffectValueText->SetText(FormatTraitEffectValueText(Target, Row.BaseEffect));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[TraitSlot] DT_BuildingTrait 에 '%s' 행이 없습니다 — 슬롯 %d 아이콘·효과 비움"),
			*InTraitID.ToString(), SlotIndex);
		if (EquippedIcon) EquippedIcon->SetBrushFromTexture(nullptr, false);
		if (EffectNameText) EffectNameText->SetText(FText::GetEmpty());
		if (EffectValueText) EffectValueText->SetText(FText::GetEmpty());
	}

	ApplyStateVisibility();
}

void UTraitSlotWidget::SetLocked(int32 InDiamondCost)
{
	State = ETraitSlotVisualState::Locked;
	EquippedTraitID = NAME_None;

	if (DiamondCostWidget)
	{
		// 비용 표기는 프로젝트 공용 ResourceWidget 경유 (아이콘+축약 숫자+afford 색). 비용은 올림(UCostActionButtonWidget::SetCost 동일 규칙).
		DiamondCostWidget->AmountRoundMode = ENumberRoundMode::Ceil;
		DiamondCostWidget->SetResourceType(EResourceType::Diamond, /*bUseTypeColor=*/true);
		DiamondCostWidget->SetValue(InDiamondCost);

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UResourceItemManager* RM = GI->GetSubsystem<UResourceItemManager>())
			{
				DiamondCostWidget->SetCanAfford(RM->GetResourceAmount(EResourceType::Diamond) >= InDiamondCost);
			}
		}
	}

	ApplyStateVisibility();
}

void UTraitSlotWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	PulseTime = 0.f;
	if (SelectedBorder)
	{
		SelectedBorder->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	ApplySelectedRingPulse();
	TraitSlotColor::SetLineColor(EmptyRing, bSelected ? RingTargetColor : RingColor);
	// FLinearColor(FColor) 가 sRGB→linear 변환을 한다
	if (EmptyPlus)
	{
		EmptyPlus->SetContentColorAndOpacity(FLinearColor(bSelected ? PlusTargetColor : PlusColor));
	}
	if (SlotCaptionText)
	{
		SlotCaptionText->SetColorAndOpacity(FSlateColor(FLinearColor(bSelected ? CaptionTargetColor : CaptionColor)));
	}
}

void UTraitSlotWidget::ApplyStateVisibility()
{
	ApplyVisualForState(State);
}

void UTraitSlotWidget::ApplyVisualForState(ETraitSlotVisualState InState)
{
	const bool bEquipped = (InState == ETraitSlotVisualState::Equipped);
	const bool bLocked = (InState == ETraitSlotVisualState::Locked);
	const bool bEmpty = (InState == ETraitSlotVisualState::Empty);

	auto Show = [](UWidget* W, bool bVisible)
	{
		if (W) W->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	};

	Show(RarityGlow, bEquipped);
	Show(EquippedFace, bEquipped);
	Show(EquippedIcon, bEquipped);
	Show(RarityGem, bEquipped);
	Show(EffectNameText, bEquipped);
	Show(EffectValueText, bEquipped);

	Show(EmptyRing, bEmpty);
	Show(EmptyPlus, bEmpty);
	Show(SlotCaptionText, bEmpty);

	Show(LockStripes, bLocked);
	Show(LockShadow, bLocked);
	Show(LockOverlay, bLocked);
	Show(DiamondCostWidget, bLocked);
}

void UTraitSlotWidget::ApplyRarityColors(ELootBoxRarity Rarity)
{
	using namespace TraitSlotColor;

	const FLinearColor RaritySRGB = FLootBoxRarityUtility::GetRarityColor(Rarity);
	const FLinearColor RarityLinear(RaritySRGB.ToFColor(false));

	const FLinearColor RarityHighlight = MixToLinear(FLinearColor::White, RaritySRGB, 0.65f);
	SetPanelColors(EquippedFace,
		MixToLinear(FaceTopBase.ReinterpretAsLinear(), RaritySRGB, FaceTopRarityMix),
		MixToLinear(FaceBottomBase.ReinterpretAsLinear(), RaritySRGB, FaceBottomRarityMix),
		RarityLinear, RarityHighlight);
	// 보석 = 위가 밝은 등급색 덩어리, 테두리는 WBP 의 어두운 림 그대로
	if (RarityGem)
	{
		if (UMaterialInstanceDynamic* MID = RarityGem->GetDynamicMaterial())
		{
			MID->SetVectorParameterValue(TEXT("FillTop"), RarityHighlight);
			MID->SetVectorParameterValue(TEXT("FillBottom"), RarityLinear);
		}
	}

	if (RarityGlow)
	{
		RarityGlow->SetColorAndOpacity(FLinearColor(RarityLinear.R, RarityLinear.G, RarityLinear.B, GlowAlpha));
	}
}

void UTraitSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 비선택 시 연산 0
	if (bSelected)
	{
		PulseTime += InDeltaTime;
		ApplySelectedRingPulse();
	}
}

void UTraitSlotWidget::ApplySelectedRingPulse()
{
	UMaterialInstanceDynamic* MID = SelectedBorder ? SelectedBorder->GetDynamicMaterial() : nullptr;
	if (!MID)
	{
		return;
	}
	// 링 MI 에 구운 농도가 최고점 ― 첫 주입 전에 한 번 읽어 둔다
	if (SelectedRingPeakAlpha < 0.f)
	{
		SelectedRingPeakAlpha = MID->K2_GetScalarParameterValue(TEXT("BaseA"));
	}
	// 최고점의 0.45~1.0 배(시안 opacity .45↔1), 주기 약 1.4초. 고른 순간이 가장 밝다
	MID->SetScalarParameterValue(TEXT("BaseA"), SelectedRingPeakAlpha * (0.725f + 0.275f * FMath::Cos(PulseTime * 4.5f)));
}
