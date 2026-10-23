#include "UI/Panel/OfficeMainWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/StaticMeshComponent.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Office/WorkstationActorBase.h"
#include "Player/Components/PlacementHandler.h"
#include "UI/Element/Effects/SelectionChevronWidget.h"

void UOfficeMainWidget::SetPlacementWorkstationMarker(
	AWorkstationActorBase* Workstation, UPlacementHandler* Handler)
{
	PlacementMarkerWorkstation = Workstation;
	PlacementMarkerHandler = Handler;
	PlacementMarkerElapsed = 0.f;
	if (PlacementChevron)
	{
		PlacementChevron->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UOfficeMainWidget::UpdatePlacementWorkstationMarker(float DeltaTime)
{
	AWorkstationActorBase* Workstation = PlacementMarkerWorkstation.Get();
	UPlacementHandler* Handler = PlacementMarkerHandler.Get();
	if (!Workstation || !Handler || Handler->GetCurrentPlacementMode() != EPlacementMode::Workstation)
	{
		SetPlacementWorkstationMarker(nullptr, nullptr);
		return;
	}

	if (!PlacementMarkerCanvas && WidgetTree)
	{
		if (UOverlay* RootOverlay = Cast<UOverlay>(GetRootWidget()))
		{
			PlacementMarkerCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(), TEXT("WorkstationPlacementMarkerCanvas"));
			PlacementMarkerCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
			UOverlaySlot* MarkerOverlaySlot = RootOverlay->AddChildToOverlay(PlacementMarkerCanvas);
			MarkerOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			MarkerOverlaySlot->SetVerticalAlignment(VAlign_Fill);
		}
		else
		{
			PlacementMarkerCanvas = Cast<UCanvasPanel>(GetRootWidget());
		}
	}
	UCanvasPanel* MarkerCanvas = PlacementMarkerCanvas;
	if (!MarkerCanvas)
	{
		return;
	}
	if (!PlacementChevron)
	{
		// 기존 자가 페인트 부품이므로 디자이너 에셋 없이 같은 실루엣을 재사용한다.
		PlacementChevron = CreateWidget<USelectionChevronWidget>(this, USelectionChevronWidget::StaticClass());
		if (!PlacementChevron) return;
		PlacementChevronSlot = MarkerCanvas->AddChildToCanvas(PlacementChevron);
		PlacementChevron->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlacementChevron->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		if (PlacementChevronSlot)
		{
			PlacementChevronSlot->SetAutoSize(true);
			PlacementChevronSlot->SetAnchors(FAnchors(0.f));
			PlacementChevronSlot->SetAlignment(FVector2D(0.5f, 1.f));
			PlacementChevronSlot->SetZOrder(10);
		}
	}
	if (!PlacementChevronSlot) return;

	FVector Anchor = Workstation->GetActorLocation();
	if (Workstation->DeskMesh)
	{
		const FBoxSphereBounds& DeskBounds = Workstation->DeskMesh->Bounds;
		Anchor = DeskBounds.Origin + FVector(0.f, 0.f, DeskBounds.BoxExtent.Z);
	}
	FVector2D ViewportPosition;
	APlayerController* Controller = GetOwningPlayer();
	if (!Controller || !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		Controller, Anchor, ViewportPosition, true))
	{
		PlacementChevron->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const FGeometry CanvasGeometry = MarkerCanvas->GetCachedGeometry();
	const FVector2D LocalPosition = CanvasGeometry.AbsoluteToLocal(
		UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).LocalToAbsolute(ViewportPosition));
	PlacementMarkerElapsed = FMath::Fmod(PlacementMarkerElapsed + DeltaTime, 1.f / 0.65f);
	const float Bob = FMath::Sin(PlacementMarkerElapsed * 2.f * PI * 0.65f) * 4.f;
	PlacementChevronSlot->SetPosition(LocalPosition - FVector2D(0.f, 48.f - Bob));
	PlacementChevron->SetRenderScale(FVector2D(0.55f));
	const FLinearColor MainColor = Handler->CanDropHere()
		? FLinearColor(0.08f, 0.78f, 0.18f, 1.f)
		: FLinearColor(0.95f, 0.16f, 0.10f, 1.f);
	FLinearColor EchoColor = MainColor;
	EchoColor.A = 0.45f;
	PlacementChevron->SetChevronColors(MainColor, EchoColor);
	PlacementChevron->SetVisibility(ESlateVisibility::HitTestInvisible);
}
