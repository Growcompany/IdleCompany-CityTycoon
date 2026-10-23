// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Trade/CountryNameWidget.h"
#include "Manager/CountryMarketManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/CompanyInfoTable.h"
#include "Data/WorldMapTypes.h"
#include "Enum/CountryType.h"
#include "Entity/OverseasDiorama/OverseasDioramaTypes.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "UI/Element/WorldMap/MapActionBubbleWidget.h"
#include "Enum/WidgetType.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "CommonTextBlock.h"
#include "Engine/GameInstance.h"
#include "Rendering/DrawElements.h"
#include "Utils/FWidgetAnimationUtils.h"

namespace
{
	constexpr float SelectionRingGap = 3.0f;
	constexpr float SelectionRingThickness = 5.0f;
	constexpr float SelectionRingRadiusOffset = SelectionRingGap + SelectionRingThickness * 0.5f;
	constexpr float SelectionHaloOuterThickness = 20.0f;
	constexpr int32 SelectionRingSegments = 64;
}

void UCountryNameWidget::SetCountryType(ECountryType InType)
{
	if (CountryType == InType) return;
	CountryType = InType;

	if (CodeText)
	{
		const FString Code = ToCountryCode(InType);
		CodeText->SetText(FText::FromString(Code));
		CodeText->SetVisibility((Code.IsEmpty() || bHasMiniature) ? ESlateVisibility::Collapsed
		                                                          : ESlateVisibility::HitTestInvisible);
	}

	// CountryType 변경 시 닷 즉시 갱신 — SetCountryType 이 NativeConstruct 후 호출되거나
	// 동적 변경 시 stale 표시 방지. MarketMgr nullptr 시 RefreshDot이 자체적으로 Collapsed 처리.
	RefreshAllDots();
}

void UCountryNameWidget::SetLockState(bool bInLocked, const FText& InLockReason)
{
	bLocked = bInLocked;

	// 원반 면은 잠겨도 틴트하지 않는다 ― 그라데이션에 어두운 색을 곱하면 원반이 거의 검어져
	// 메달리온이 판 위에서 사라진다. 상태는 링 색·자물쇠·영역 덮개가 말한다.
	const ESlateVisibility LockVis = bInLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (LockImage) LockImage->SetVisibility(bHasMiniature ? ESlateVisibility::Collapsed : LockVis);
	if (LockText)
	{
		// 사유 문구가 없으면 접는다. 켜 두면 디자이너가 트리에 넣어 둔 기본 문자열이 그대로 보이는데,
		// 사유를 다른 곳(상태 줄)이 말하는 호출부에서는 그게 유령 텍스트가 된다.
		const bool bShowReason = bInLocked && !InLockReason.IsEmpty();
		LockText->SetVisibility(bShowReason ? LockVis : ESlateVisibility::Collapsed);
		if (bShowReason)
		{
			LockText->SetText(InLockReason);
		}
	}

	// 잠금이어도 입력은 받아 삼킨다(PitchTierTab 선례). 무반응이면 왜 못 여는지 알 길이 없다.
	SetIsInteractionEnabled(true);
}

FLinearColor UCountryNameWidget::ResolveRingColor(ECountryMapState State, const FLinearColor& SpecialtyAccent)
{
	// 잠긴 국가에 특산 색을 주면 아직 알면 안 되는 정보를 미리 흘린다.
	if (State == ECountryMapState::Locked)    { return FLinearColor::FromSRGBColor(FColor(0x3B, 0x43, 0x54)); }
	if (State == ECountryMapState::Available) { return FLinearColor::FromSRGBColor(FColor(0x7F, 0x8C, 0xA3)); }

	// 진출국만 산업색을 쓴다. 산업 hex 를 여기 두면 DT 의 복제본이 되고, 행이 없는 산업은 조용히 폴백으로 샌다.
	return SpecialtyAccent;
}

FLinearColor UCountryNameWidget::MiningRingColor()
{
	// 채광 전담국(브라질·사우디·호주·캐나다·남아공)은 특산 산업 행 자체가 없어 AccentColor 출처가 없다.
	return FLinearColor::FromSRGBColor(FColor(0xC9, 0xA5, 0x67));
}

FLinearColor UCountryNameWidget::TradeHubRingColor()
{
	// 무역 허브도 산업 행이 없다. teal-500 ― 링 팔레트(indigo/amber/red-500)와 같은 가족이면서
	// 항로 블루 #3D9BE0 과 구별된다.
	return FLinearColor::FromSRGBColor(FColor(0x14, 0xB8, 0xA6));
}

float UCountryNameWidget::ResolveRegionShadeAlpha(ECountryMapState State, bool bMaskValid)
{
	// bMaskValid 는 알파를 바꾸지 않는다. 마스크가 없으면 덮개를 못 그릴 뿐이고,
	// 그때 0 을 돌려주면 잠긴 국가가 진출한 것처럼 보인다 ― 호출부가 폴백을 걸 수 있게
	// 알파는 상태 그대로 주고, 마스크 유무는 호출부가 판단한다.
	if (State == ECountryMapState::Owned)     { return 0.0f;  }
	if (State == ECountryMapState::Available) { return 0.35f; }
	return 0.72f;
}

void UCountryNameWidget::SetCountryName(const FText& InName)
{
	if (!NameText) { return; }

	NameText->SetText(InName);
	NameText->SetVisibility(InName.IsEmpty() ? ESlateVisibility::Collapsed
	                                         : ESlateVisibility::HitTestInvisible);
}

void UCountryNameWidget::SetStatusLine(const FText& InStatus)
{
	if (!StatusText) { return; }

	StatusText->SetText(InStatus);
	ApplyLabelMode();
}

void UCountryNameWidget::SetRingColor(const FLinearColor& InColor)
{
	RingColor = InColor;
	ApplyRingColor();
}

void UCountryNameWidget::ApplyRingColor()
{
	FLinearColor DisplayColor = RingColor;
	if (GetSelected())
	{
		const float Brightness = FMotionGate::IsReduced() ? 1.45f
			: 1.45f - 0.25f * FMath::Cos(FocusHighlightElapsed * (2.0f * PI / 1.6f));
		DisplayColor.R *= Brightness;
		DisplayColor.G *= Brightness;
		DisplayColor.B *= Brightness;
	}
	if (RingImage) { RingImage->SetColorAndOpacity(DisplayColor); }

	// 코드 글자는 링과 한 기호다 ― 색이 갈리면 같은 상태를 두 번 읽어야 한다(목업 `.disc{color:var(--c)}`).
	if (CodeText) { CodeText->SetColorAndOpacity(FSlateColor(DisplayColor)); }
	Invalidate(EInvalidateWidget::Paint);
}

void UCountryNameWidget::SetFocusHighlight(bool bHighlight)
{
	bFocusHighlightRequested = bHighlight;
	if (bHighlight) { SetIsSelected(true, false); }
	else { ClearSelection(); }
}

void UCountryNameWidget::UpdateFocusHighlight(float DeltaTime)
{
	if (!GetSelected()) { return; }
	FocusHighlightElapsed = FMath::Fmod(FocusHighlightElapsed + FMath::Max(0.0f, DeltaTime), 1.6f);
	ApplyRingColor();
}

void UCountryNameWidget::NativeOnSelected(bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
	FocusHighlightElapsed = 0.0f;
	ApplyRingColor();
}

void UCountryNameWidget::NativeOnDeselected(bool bBroadcast)
{
	bFocusHighlightRequested = false;
	Super::NativeOnDeselected(bBroadcast);
	FocusHighlightElapsed = 0.0f;
	ApplyRingColor();
}

int32 UCountryNameWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	// 그림 모드에서는 링이 접혀 그 페인트 기하가 접히기 전 자리에 낡아 있다 ― 엉뚱한 자리에 원을 그리지 않는다.
	if (!GetSelected() || !RingImage || bHasMiniature) { return MaxLayer; }

	// NativePaint의 AllottedGeometry와 같은 창 내부 좌표계를 사용한다.
	const FGeometry& RingGeometry = RingImage->GetPaintSpaceGeometry();
	const FVector2D RingSize = RingGeometry.GetLocalSize();
	if (RingSize.X <= 0.0 || RingSize.Y <= 0.0) { return MaxLayer; }
	const FVector2D RingCenter = AllottedGeometry.AbsoluteToLocal(
		RingGeometry.LocalToAbsolute(RingSize * 0.5f));
	const FVector2D RingEdge = AllottedGeometry.AbsoluteToLocal(
		RingGeometry.LocalToAbsolute(FVector2D(RingSize.X, RingSize.Y * 0.5f)));
	const float Radius = FVector2D::Distance(RingCenter, RingEdge) + SelectionRingRadiusOffset;

	// 산업별 링 색과 독립된 금색 신호를 유지해 밝은 산업색 위에서도 선택을 읽을 수 있다.
	const FLinearColor SelectionColor = FLinearColor::FromSRGBColor(FColor(255, 224, 108))
		* InWidgetStyle.GetColorAndOpacityTint();
	if (SelectionColor.A <= 0.0f) { return MaxLayer; }
	const float HaloPulse = FMotionGate::IsReduced() ? 1.0f
		: 0.8f + 0.2f * FMath::Cos(FocusHighlightElapsed * (2.0f * PI / 1.6f));

	TArray<FVector2D> RingPoints;
	RingPoints.Reserve(SelectionRingSegments + 1);
	for (int32 PointIndex = 0; PointIndex <= SelectionRingSegments; ++PointIndex)
	{
		const float Angle = 2.0f * PI * PointIndex / SelectionRingSegments;
		RingPoints.Add(RingCenter + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}
	const ESlateDrawEffect DrawEffect = bParentEnabled && GetIsEnabled()
		? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const float HaloThicknesses[] = { SelectionHaloOuterThickness, 14.0f, 9.0f };
	const float HaloAlphas[] = { 0.10f, 0.17f, 0.25f };
	for (int32 HaloIndex = 0; HaloIndex < UE_ARRAY_COUNT(HaloThicknesses); ++HaloIndex)
	{
		FLinearColor HaloColor = SelectionColor;
		HaloColor.A *= HaloAlphas[HaloIndex] * HaloPulse;
		FSlateDrawElement::MakeLines(OutDrawElements, MaxLayer + 1, AllottedGeometry.ToPaintGeometry(),
			RingPoints, DrawEffect, HaloColor, true, HaloThicknesses[HaloIndex]);
	}
	FSlateDrawElement::MakeLines(OutDrawElements, MaxLayer + 1, AllottedGeometry.ToPaintGeometry(),
		RingPoints, DrawEffect, SelectionColor, true, SelectionRingThickness);
	return MaxLayer + 1;
}

bool UCountryNameWidget::GetFocusVisualBounds(FSlateRect& OutBounds) const
{
	// 그림 모드의 desired 는 상자 오버라이드 = 그림 크기라, 프리패스 전에도 같은 값을 쓴다.
	const FVector2D PinSize = bHasMiniature ? MiniatureDisplaySize : GetDesiredSize();
	if (PinSize.X <= 0.0 || PinSize.Y <= 0.0) { return false; }
	FVector2D BoundsMin = FVector2D::ZeroVector;
	FVector2D BoundsMax = PinSize;
	if (GetSelected() && !bHasMiniature)
	{
		// 슬롯 경계로 먼저 맞춰야 화면 밖 핀의 이전 Paint Geometry에 포커스가 끌리지 않는다.
		const FVector2D RingCenter = PinSize * 0.5f;
		const float RingExtent = FMath::Max(PinSize.X, PinSize.Y) * 0.5f
			+ SelectionRingRadiusOffset + SelectionHaloOuterThickness * 0.5f;
		BoundsMin = RingCenter - FVector2D(RingExtent, RingExtent);
		BoundsMax = RingCenter + FVector2D(RingExtent, RingExtent);
	}
	const UWidget* VisualParts[] = { bHasMiniature ? MiniatureImage.Get() : BGImage.Get(), StatusPlate.Get() };
	for (const UWidget* Part : VisualParts)
	{
		if (!Part) { continue; }
		const UCanvasPanelSlot* PartSlot = Cast<UCanvasPanelSlot>(Part->Slot);
		if (!PartSlot) { return false; }
		const FAnchors Anchors = PartSlot->GetAnchors();
		const FVector2D PartSize = PartSlot->GetAutoSize() ? Part->GetDesiredSize() : PartSlot->GetSize();
		if (!Anchors.Minimum.Equals(Anchors.Maximum) || PartSize.X <= 0.0 || PartSize.Y <= 0.0)
		{
			return false;
		}
		const FVector2D PartMin = PinSize * FVector2D(Anchors.Minimum)
			+ PartSlot->GetPosition() - PartSize * PartSlot->GetAlignment();
		const FVector2D PartMax = PartMin + PartSize;
		BoundsMin.X = FMath::Min(BoundsMin.X, PartMin.X);
		BoundsMin.Y = FMath::Min(BoundsMin.Y, PartMin.Y);
		BoundsMax.X = FMath::Max(BoundsMax.X, PartMax.X);
		BoundsMax.Y = FMath::Max(BoundsMax.Y, PartMax.Y);
	}
	OutBounds = FSlateRect(BoundsMin, BoundsMax);
	return true;
}

void UCountryNameWidget::SetAnchorSide(EMedallionAnchor InSide)
{
	AnchorSide = InSide;
	ApplyAnchorSide();
}

void UCountryNameWidget::ApplyAnchorSide()
{
	if (!StatusPlate)
	{
		return;
	}

	// 라벨이 원반 캔버스 밖에 놓이면 방향을 실을 슬롯이 없다. 조용히 같은 쪽에 쌓이는 대신 한 줄 남긴다.
	UCanvasPanelSlot* PlateSlot = Cast<UCanvasPanelSlot>(StatusPlate->Slot);
	if (!PlateSlot)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CountryName] StatusPlate 가 캔버스 자식이 아니라 LabelSide 를 실을 수 없습니다. 라벨이 전부 같은 쪽에 겹칩니다."));
		return;
	}

	// 기본은 아래 ― EMedallionAnchor 의 서수 0 이라 DT 미기입 행도 여기로 떨어진다.
	FVector2D AnchorPoint(0.5, 1.0);
	FVector2D PlateAlignment(0.5, 0.0);
	FVector2D PlateOffset(0.0, LabelGap);

	// 그림 모드의 라벨판은 LabelSide 와 무관하게 그림 아래다(디오라마 설계 §1·§3.3).
	switch (bHasMiniature ? EMedallionAnchor::Bottom : AnchorSide)
	{
	case EMedallionAnchor::Top:
		AnchorPoint = FVector2D(0.5, 0.0);
		PlateAlignment = FVector2D(0.5, 1.0);
		PlateOffset = FVector2D(0.0, -LabelGap);
		break;
	case EMedallionAnchor::Left:
		AnchorPoint = FVector2D(0.0, 0.5);
		PlateAlignment = FVector2D(1.0, 0.5);
		PlateOffset = FVector2D(-LabelGap, 0.0);
		break;
	case EMedallionAnchor::Right:
		AnchorPoint = FVector2D(1.0, 0.5);
		PlateAlignment = FVector2D(0.0, 0.5);
		PlateOffset = FVector2D(LabelGap, 0.0);
		break;
	case EMedallionAnchor::Bottom:
	default:
		break;
	}

	// AutoSize 가 켜져 있어야 판 폭을 글자가 정한다. 꺼져 있으면 슬롯 크기(0)로 접혀 라벨이 사라진다.
	PlateSlot->SetAutoSize(true);
	PlateSlot->SetAnchors(FAnchors(AnchorPoint.X, AnchorPoint.Y));
	PlateSlot->SetAlignment(PlateAlignment);
	PlateSlot->SetPosition(PlateOffset);
}

void UCountryNameWidget::SetAlertVisible(bool bVisible)
{
	if (AlertDot)
	{
		AlertDot->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UCountryNameWidget::NativeOnClicked()
{
	Super::NativeOnClicked();
	// 부모가 확정한 표시는 재클릭에도 유지하고, 자동 선택만 콜백 전에 해제한다.
	if (bLocked || !bFocusHighlightRequested) { ClearSelection(); }

	if (bLocked)
	{
		OnFlagRejected.ExecuteIfBound(CountryType);
		return;
	}

	OnFlagClicked.ExecuteIfBound(CountryType);
}

void UCountryNameWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsSelectable(true);
	SetIsToggleable(false);
	SetIsInteractableWhenSelected(true);
	SetShouldSelectUponReceivingFocus(false);
	bTriggerClickedAfterSelection = true;

	// SetAnchorSide 는 CreateWidget 직후(슬레이트 생성 전)에 불리므로 여기서 한 번 더 싣는다 ―
	// 트리가 다시 만들어지면 슬롯이 WBP 기본값(아래)으로 돌아간다.
	ApplyAnchorSide();
	ApplyRingColor();
	if (!TagRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CountryName] TagRow 가 WBP 트리에 없어 담당 산업 이름표 대신 상태 줄을 씁니다."));
	}
	ApplyLabelMode();

	if (UGameInstance* GI = GetGameInstance())
	{
		MarketMgr = GI->GetSubsystem<UCountryMarketManager>();
		if (MarketMgr)
		{
			MarketMgr->OnDemandChanged.AddDynamic(this, &UCountryNameWidget::HandleDemandChanged);
		}
	}

	RefreshAllDots();
}

void UCountryNameWidget::NativeDestruct()
{
	ClearSelection();
	if (MarketMgr)
	{
		MarketMgr->OnDemandChanged.RemoveDynamic(this, &UCountryNameWidget::HandleDemandChanged);
		MarketMgr = nullptr;
	}

	Super::NativeDestruct();
}

void UCountryNameWidget::RefreshAllDots()
{
	RefreshDot(DotSemiconductor, ECompanyType::Semiconductor);
	RefreshDot(DotElectronics, ECompanyType::Electronics);
	RefreshDot(DotAutomobile, ECompanyType::Automobile);
}

void UCountryNameWidget::RefreshDot(UImage* Dot, ECompanyType Industry)
{
	if (!Dot) return;

	if (!MarketMgr || CountryType == ECountryType::None)
	{
		Dot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UGameInstance* GI = GetGameInstance();

	// 미가용 업종(이번 출시 범위 밖)은 닷을 접는다. 매니저를 못 찾으면 노출을 유지한다 ―
	// 초기화 순서 문제로 수요 표시가 조용히 비는 쪽이 더 나쁘다.
	USaveLoadManager* SaveMgr = GI ? GI->GetSubsystem<USaveLoadManager>() : nullptr;
	if (SaveMgr && !SaveMgr->IsIndustryAvailable(Industry))
	{
		Dot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	bool bFound = false;
	MarketMgr->GetMarketState(CountryType, Industry, bFound);
	if (!bFound)
	{
		// DT_CountryDemand 미정의 셀 — 닷 자체 숨김 (이 나라엔 이 산업 수요 정의 없음)
		Dot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	bool bCompanyOk = false;
	const FCompanyInfoTable CompanyInfo = TableMgr
		? TableMgr->GetCompanyInfo(Industry, bCompanyOk) : FCompanyInfoTable();
	if (!bCompanyOk || CompanyInfo.AccentColor.A < KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CountryNameWidget] %s 산업의 DT_CompanyInfo.AccentColor 가 없어 %s 수요 아이콘을 숨깁니다."),
			*UEnum::GetValueAsString(Industry), ToCountryCode(CountryType));
		Dot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Dot->SetColorAndOpacity(CompanyInfo.AccentColor);
	Dot->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCountryNameWidget::HandleDemandChanged(ECountryType InCountry, ECompanyType InIndustry, float NewRatio)
{
	if (InCountry != CountryType) return;

	switch (InIndustry)
	{
	case ECompanyType::Semiconductor: RefreshDot(DotSemiconductor, InIndustry); break;
	case ECompanyType::Electronics:   RefreshDot(DotElectronics, InIndustry);   break;
	case ECompanyType::Automobile:    RefreshDot(DotAutomobile, InIndustry);    break;
	default: break;
	}
}

void UCountryNameWidget::SetMiniature(UTexture* Texture, const FDioramaStillInfo& Info, UMaterialInterface* RTMaterial)
{
	if (!Texture || !RTMaterial || !MiniatureImage || !MedallionBox || Info.SizePx.X <= 0 || Info.SizePx.Y <= 0)
	{
		ClearMiniature();
		return;
	}

	const FVector2D DisplaySize = FVector2D(Info.SizePx.X, Info.SizePx.Y) * kDioMiniatureDisplayScale;
	const FVector2D DisplayPivot = Info.PivotPx * kDioMiniatureDisplayScale;
	// 시총이 오를 때마다 전 국가가 다시 칠해진다 ― 같은 그림이면 상자·슬롯을 다시 무효화하지 않는다.
	if (bHasMiniature && MiniatureTexture == Texture && MiniatureMID && MiniatureMID->Parent == RTMaterial
		&& MiniatureDisplaySize.Equals(DisplaySize) && MiniaturePivot.Equals(DisplayPivot))
	{
		return;
	}

	if (!MiniatureMID || MiniatureMID->Parent != RTMaterial)
	{
		MiniatureMID = UMaterialInstanceDynamic::Create(RTMaterial, this);
		if (!MiniatureMID)
		{
			ClearMiniature();
			return;
		}
		MiniatureMID->SetVectorParameterValue(TEXT("UVWindow"), FLinearColor(0.0f, 0.0f, 1.0f, 1.0f));
		MiniatureImage->SetBrushFromMaterial(MiniatureMID);
	}
	MiniatureMID->SetTextureParameterValue(TEXT("RT"), Texture);
	MiniatureTexture = Texture;

	if (!bHasMiniature)
	{
		SaveDiscLayout();
	}
	bHasMiniature = true;
	MiniatureDisplaySize = DisplaySize;
	MiniaturePivot = DisplayPivot;
	ApplyMiniatureLayout();
}

void UCountryNameWidget::ClearMiniature()
{
	if (!bHasMiniature)
	{
		return;
	}
	bHasMiniature = false;
	MiniatureTexture = nullptr;
	MiniatureDisplaySize = FVector2D::ZeroVector;
	MiniaturePivot = FVector2D::ZeroVector;

	if (MiniatureImage)
	{
		MiniatureImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (MedallionBox)
	{
		if (SavedDiscLayout.bBoxWidthOverride) { MedallionBox->SetWidthOverride(SavedDiscLayout.BoxWidth); }
		else { MedallionBox->ClearWidthOverride(); }
		if (SavedDiscLayout.bBoxHeightOverride) { MedallionBox->SetHeightOverride(SavedDiscLayout.BoxHeight); }
		else { MedallionBox->ClearHeightOverride(); }
	}
	if (BGImage) { BGImage->SetVisibility(SavedDiscLayout.BGVisibility); }
	if (RingImage) { RingImage->SetVisibility(SavedDiscLayout.RingVisibility); }
	if (CodeText)
	{
		CodeText->SetVisibility(CodeText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (LockImage)
	{
		LockImage->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UCanvasPanelSlot* DotSlot = AlertDot ? Cast<UCanvasPanelSlot>(AlertDot->Slot) : nullptr)
	{
		FAnchors DotAnchors;
		DotAnchors.Minimum = SavedDiscLayout.AlertAnchorMin;
		DotAnchors.Maximum = SavedDiscLayout.AlertAnchorMax;
		DotSlot->SetAnchors(DotAnchors);
		DotSlot->SetAlignment(SavedDiscLayout.AlertAlignment);
		DotSlot->SetPosition(SavedDiscLayout.AlertPosition);
	}
	ApplyAnchorSide();
}

FVector2D UCountryNameWidget::GetAnchorAlignment() const
{
	if (!bHasMiniature || MiniatureDisplaySize.X <= 0.0 || MiniatureDisplaySize.Y <= 0.0)
	{
		return FVector2D(0.5, 0.5);
	}
	return FVector2D(
		FMath::Clamp(MiniaturePivot.X / MiniatureDisplaySize.X, 0.0, 1.0),
		FMath::Clamp(MiniaturePivot.Y / MiniatureDisplaySize.Y, 0.0, 1.0));
}

void UCountryNameWidget::SaveDiscLayout()
{
	if (BGImage) { SavedDiscLayout.BGVisibility = BGImage->GetVisibility(); }
	if (RingImage) { SavedDiscLayout.RingVisibility = RingImage->GetVisibility(); }
	if (MedallionBox)
	{
		SavedDiscLayout.bBoxWidthOverride = MedallionBox->IsWidthOverride();
		SavedDiscLayout.bBoxHeightOverride = MedallionBox->IsHeightOverride();
		SavedDiscLayout.BoxWidth = MedallionBox->GetWidthOverride();
		SavedDiscLayout.BoxHeight = MedallionBox->GetHeightOverride();
	}
	if (const UCanvasPanelSlot* DotSlot = AlertDot ? Cast<UCanvasPanelSlot>(AlertDot->Slot) : nullptr)
	{
		const FAnchors DotAnchors = DotSlot->GetAnchors();
		SavedDiscLayout.AlertAnchorMin = DotAnchors.Minimum;
		SavedDiscLayout.AlertAnchorMax = DotAnchors.Maximum;
		SavedDiscLayout.AlertAlignment = DotSlot->GetAlignment();
		SavedDiscLayout.AlertPosition = DotSlot->GetPosition();
	}
}

void UCountryNameWidget::ApplyMiniatureLayout()
{
	// 버튼 히트 영역 = 루트 상자(WBP 88px 고정)라, 그림 모서리까지 눌리려면 그림 모드에서만 상자를 그림 크기로 바꾼다.
	MedallionBox->SetWidthOverride(static_cast<float>(MiniatureDisplaySize.X));
	MedallionBox->SetHeightOverride(static_cast<float>(MiniatureDisplaySize.Y));

	if (UCanvasPanelSlot* MiniatureSlot = Cast<UCanvasPanelSlot>(MiniatureImage->Slot))
	{
		MiniatureSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		MiniatureSlot->SetAlignment(FVector2D(0.5, 0.5));
		MiniatureSlot->SetAutoSize(false);
		MiniatureSlot->SetPosition(FVector2D::ZeroVector);
		MiniatureSlot->SetSize(MiniatureDisplaySize);
	}
	MiniatureImage->SetVisibility(ESlateVisibility::Visible);

	UWidget* DiscParts[] = { BGImage, RingImage, CodeText, LockImage };
	for (UWidget* DiscPart : DiscParts)
	{
		if (DiscPart)
		{
			DiscPart->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (UCanvasPanelSlot* DotSlot = AlertDot ? Cast<UCanvasPanelSlot>(AlertDot->Slot) : nullptr)
	{
		DotSlot->SetAnchors(FAnchors(1.0f, 0.0f));
		DotSlot->SetAlignment(FVector2D(1.0, 0.0));
		DotSlot->SetPosition(FVector2D::ZeroVector);
	}
	ApplyAnchorSide();
}

void UCountryNameWidget::SetIndustryTags(const TArray<FMapIndustryTagSpec>& Specs)
{
	if (!TagRow)
	{
		bTagMode = false;
		AppliedTagSpecs.Reset();
		ApplyLabelMode();
		return;
	}
	// 시총이 오를 때마다 전 국가가 다시 칠해진다 ― 같은 목록이면 칸을 다시 만들지 않는다.
	if (Specs == AppliedTagSpecs && TagWidgets.Num() == Specs.Num())
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	const UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	const TSubclassOf<UUserWidget> TagClass = TableMgr ? TableMgr->GetWidgetClass(EWidgetType::MapIndustryTag) : nullptr;

	TagRow->ClearChildren();
	TagWidgets.Reset();
	if (TagClass && TagClass->IsChildOf(UMapIndustryTagWidget::StaticClass()))
	{
		for (const FMapIndustryTagSpec& Spec : Specs)
		{
			UMapIndustryTagWidget* Tag = CreateWidget<UMapIndustryTagWidget>(this, TagClass);
			if (!Tag)
			{
				continue;
			}
			Tag->ApplySpec(Spec, *TableMgr);
			if (UHorizontalBoxSlot* TagSlot = TagRow->AddChildToHorizontalBox(Tag))
			{
				TagSlot->SetVerticalAlignment(VAlign_Center);
				TagSlot->SetPadding(FMargin(TagWidgets.Num() > 0 ? TagGap : 0.0f, 0.0f, 0.0f, 0.0f));
			}
			TagWidgets.Add(Tag);
		}
	}
	else if (Specs.Num() > 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CountryName] EWidgetType::MapIndustryTag 위젯 클래스를 못 찾아 %s 이름표를 상태 줄로 표시합니다. DT_WidgetClass 를 확인하세요."),
			ToCountryCode(CountryType));
	}

	AppliedTagSpecs = Specs;
	bTagMode = TagWidgets.Num() > 0;
	ApplyLabelMode();
}

void UCountryNameWidget::ApplyLabelMode()
{
	if (TagRow)
	{
		TagRow->SetVisibility(bTagMode ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (StatusText)
	{
		StatusText->SetVisibility((bTagMode || StatusText->GetText().IsEmpty())
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (DotRow)
	{
		if (!bDotRowVisibilitySaved)
		{
			DotRowAuthoredVisibility = DotRow->GetVisibility();
			bDotRowVisibilitySaved = true;
		}
		// 수요는 서랍이 말한다 ― 이름표 모드에서는 수요 행을 접는다.
		DotRow->SetVisibility(bTagMode ? ESlateVisibility::Collapsed : DotRowAuthoredVisibility);
	}
	Invalidate(EInvalidateWidget::Layout);
}

float UCountryNameWidget::GetHeightAboveAnchor() const
{
	const float DiscHeight = (MedallionBox && MedallionBox->IsHeightOverride())
		? MedallionBox->GetHeightOverride()
		: static_cast<float>(GetDesiredSize().Y);
	return MapBubbles::HeightAboveAnchor(bHasMiniature, static_cast<float>(MiniaturePivot.Y), DiscHeight);
}
