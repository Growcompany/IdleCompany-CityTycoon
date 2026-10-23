#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

// 임시 점검 — 기기에서 창의 그리기 층을 종류·이름별로 껐다 켜며 GPU 비용을 잰다(원인 확정 뒤 삭제)
// CGR.UIProbe.Dump <Root>                  : 클래스 이름에 <Root> 가 든 화면 위 위젯 아래 그려지는 층을 면적순으로 로그
// CGR.UIProbe.Hide <Root> <Mode> [Pattern] : all|image|border|button|text|progress|niagara|material|texture|rounded|big <px²>|name|res|class <Pattern>
// CGR.UIProbe.Restore                      : 끈 것을 전부 되돌린다

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/DynamicEntryBoxBase.h"
#include "Components/Image.h"
#include "Components/ListViewBase.h"
#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "UObject/UObjectIterator.h"

namespace CGRUIProbe
{
	enum class EUndoKind : uint8 { Visibility, BorderBrush, ButtonStyle };

	struct FUndo
	{
		TWeakObjectPtr<UWidget> Widget;
		EUndoKind Kind = EUndoKind::Visibility;
		ESlateVisibility Visibility = ESlateVisibility::Visible;
		FSlateBrush Brush;
		FButtonStyle Style;
	};

	struct FLayer
	{
		UWidget* Widget = nullptr;
		const FSlateBrush* Brush = nullptr;
		FString Kind;
		double Area = 0.0;
	};

	static TArray<FUndo> UndoStack;

	static bool IsShown(const UWidget* Widget)
	{
		for (const UWidget* Current = Widget; Current;)
		{
			if (!Current->IsVisible()) return false;
			if (const UPanelWidget* Parent = Current->GetParent())
			{
				// 스위처는 활성 자식만 그린다
				if (const UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Parent))
				{
					if (Switcher->GetActiveWidget() != Current) return false;
				}
				Current = Parent;
				continue;
			}
			const UUserWidget* Owner = Current->GetTypedOuter<UUserWidget>();
			Current = Owner != Current ? Owner : nullptr;
		}
		return true;
	}

	static void Collect(UUserWidget* UserWidget, TArray<UWidget*>& Out, TSet<UObject*>& Seen)
	{
		if (!UserWidget || !UserWidget->WidgetTree || Seen.Contains(UserWidget)) return;
		Seen.Add(UserWidget);
		UserWidget->WidgetTree->ForEachWidget([&Out, &Seen](UWidget* Widget)
		{
			if (!Widget || Seen.Contains(Widget)) return;
			if (UUserWidget* Child = Cast<UUserWidget>(Widget))
			{
				Out.Add(Child);
				Collect(Child, Out, Seen);
				return;
			}
			Seen.Add(Widget);
			Out.Add(Widget);
			if (UListViewBase* List = Cast<UListViewBase>(Widget))
			{
				for (UUserWidget* Entry : List->GetDisplayedEntryWidgets())
				{
					Out.Add(Entry);
					Collect(Entry, Out, Seen);
				}
			}
			else if (UDynamicEntryBoxBase* EntryBox = Cast<UDynamicEntryBoxBase>(Widget))
			{
				for (UUserWidget* Entry : EntryBox->GetAllEntries())
				{
					Out.Add(Entry);
					Collect(Entry, Out, Seen);
				}
			}
		});
	}

	static TArray<UWidget*> GatherWidgets(const FString& RootPattern, TArray<UUserWidget*>& OutRoots)
	{
		TArray<UWidget*> Widgets;
		TSet<UObject*> Seen;
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			UUserWidget* Candidate = *It;
			UWorld* World = Candidate ? Candidate->GetWorld() : nullptr;
			if (!World || !World->IsGameWorld() || Candidate->HasAnyFlags(RF_ClassDefaultObject)) continue;
			if (!Candidate->GetClass()->GetName().Contains(RootPattern) || !Candidate->GetCachedWidget().IsValid()) continue;
			OutRoots.Add(Candidate);
			Collect(Candidate, Widgets, Seen);
		}
		return Widgets;
	}

	static FString ResourceName(const FSlateBrush* Brush)
	{
		const UObject* Resource = Brush ? Brush->GetResourceObject() : nullptr;
		if (!Resource) return TEXT("-");
		if (const UMaterialInterface* Material = Cast<UMaterialInterface>(Resource))
		{
			const UMaterial* Base = Material->GetMaterial();
			return FString::Printf(TEXT("MAT:%s(%s)"), *Material->GetName(), Base ? *Base->GetName() : TEXT("?"));
		}
		return FString::Printf(TEXT("%s:%s"), Resource->IsA<UTexture>() ? TEXT("TEX") : TEXT("OBJ"), *Resource->GetName());
	}

	static const TCHAR* DrawAsName(const FSlateBrush* Brush)
	{
		if (!Brush) return TEXT("-");
		switch (Brush->GetDrawType())
		{
		case ESlateBrushDrawType::NoDrawType: return TEXT("None");
		case ESlateBrushDrawType::Box: return TEXT("Box");
		case ESlateBrushDrawType::Border: return TEXT("Border");
		case ESlateBrushDrawType::Image: return TEXT("Image");
		case ESlateBrushDrawType::RoundedBox: return TEXT("Rounded");
		default: return TEXT("?");
		}
	}

	// 그려지는 층만 — 컨테이너는 자기 바탕이 있을 때만 센다
	static TArray<FLayer> GatherLayers(const TArray<UWidget*>& Widgets)
	{
		TArray<FLayer> Layers;
		for (UWidget* Widget : Widgets)
		{
			if (!Widget || !IsShown(Widget)) continue;
			const FVector2D Size = Widget->GetCachedGeometry().GetAbsoluteSize();
			FLayer Layer;
			Layer.Widget = Widget;
			Layer.Area = FMath::Max(0.0, Size.X) * FMath::Max(0.0, Size.Y);
			if (UImage* Image = Cast<UImage>(Widget))
			{
				Layer.Kind = TEXT("image");
				Layer.Brush = &Image->GetBrush();
				if (Image->GetColorAndOpacity().A <= 0.f) continue;
			}
			else if (UBorder* Border = Cast<UBorder>(Widget))
			{
				Layer.Kind = TEXT("border");
				Layer.Brush = &Border->Background;
				if (Border->Background.GetDrawType() == ESlateBrushDrawType::NoDrawType || Border->GetBrushColor().A <= 0.f) continue;
			}
			else if (UButton* Button = Cast<UButton>(Widget))
			{
				Layer.Kind = TEXT("button");
				Layer.Brush = &Button->GetStyle().Normal;
				if (Layer.Brush->GetDrawType() == ESlateBrushDrawType::NoDrawType || Button->GetBackgroundColor().A <= 0.f) continue;
			}
			else if (Widget->IsA<UTextBlock>() || Widget->IsA<URichTextBlock>())
			{
				Layer.Kind = TEXT("text");
			}
			else if (Widget->IsA<UProgressBar>())
			{
				Layer.Kind = TEXT("progress");
			}
			else if (Widget->GetClass()->GetName().Contains(TEXT("Niagara")))
			{
				Layer.Kind = TEXT("niagara");
			}
			else
			{
				continue;
			}
			Layers.Add(Layer);
		}
		return Layers;
	}

	static void PushVisibility(UWidget* Widget)
	{
		FUndo& Undo = UndoStack.AddDefaulted_GetRef();
		Undo.Widget = Widget;
		Undo.Kind = EUndoKind::Visibility;
		Undo.Visibility = Widget->GetVisibility();
		Widget->SetVisibility(ESlateVisibility::Hidden);
	}

	// 바탕만 끄고 안의 자식은 그대로 그리게 한다
	static void HideOwnDraw(const FLayer& Layer)
	{
		if (UBorder* Border = Cast<UBorder>(Layer.Widget))
		{
			FUndo& Undo = UndoStack.AddDefaulted_GetRef();
			Undo.Widget = Border;
			Undo.Kind = EUndoKind::BorderBrush;
			Undo.Brush = Border->Background;
			FSlateBrush Empty = Border->Background;
			Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
			Border->SetBrush(Empty);
		}
		else if (UButton* Button = Cast<UButton>(Layer.Widget))
		{
			FUndo& Undo = UndoStack.AddDefaulted_GetRef();
			Undo.Widget = Button;
			Undo.Kind = EUndoKind::ButtonStyle;
			Undo.Style = Button->GetStyle();
			FButtonStyle Empty = Button->GetStyle();
			for (FSlateBrush* StateBrush : {&Empty.Normal, &Empty.Hovered, &Empty.Pressed, &Empty.Disabled})
			{
				StateBrush->DrawAs = ESlateBrushDrawType::NoDrawType;
			}
			Button->SetStyle(Empty);
		}
		else
		{
			PushVisibility(Layer.Widget);
		}
	}

	static bool Matches(const FLayer& Layer, const FString& Mode, const FString& Pattern)
	{
		const UObject* Resource = Layer.Brush ? Layer.Brush->GetResourceObject() : nullptr;
		if (Mode == TEXT("all")) return true;
		if (Mode == TEXT("material")) return Resource && Resource->IsA<UMaterialInterface>();
		if (Mode == TEXT("texture")) return Resource && Resource->IsA<UTexture>();
		if (Mode == TEXT("rounded")) return Layer.Brush && Layer.Brush->GetDrawType() == ESlateBrushDrawType::RoundedBox;
		if (Mode == TEXT("big")) return Layer.Brush && Layer.Area >= FCString::Atod(*Pattern);
		if (Mode == TEXT("res")) return ResourceName(Layer.Brush).Contains(Pattern);
		return Layer.Kind == Mode;
	}

	static void Dump(const TArray<FString>& Arguments)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UIProbe_Dump);
		if (Arguments.Num() < 1)
		{
			UE_LOG(LogTemp, Error, TEXT("[UIProbe] Usage: CGR.UIProbe.Dump <RootClassPattern>"));
			return;
		}
		TArray<UUserWidget*> Roots;
		TArray<FLayer> Layers = GatherLayers(GatherWidgets(Arguments[0], Roots));
		Layers.Sort([](const FLayer& A, const FLayer& B) { return A.Area > B.Area; });
		double Total = 0.0;
		TMap<FString, double> ByResource;
		TMap<FString, int32> CountByResource;
		for (const FLayer& Layer : Layers)
		{
			Total += Layer.Area;
			const FString Key = Layer.Brush ? FString::Printf(TEXT("%s %s"), DrawAsName(Layer.Brush), *ResourceName(Layer.Brush)) : Layer.Kind;
			ByResource.FindOrAdd(Key) += Layer.Area;
			++CountByResource.FindOrAdd(Key);
		}
		UE_LOG(LogTemp, Display, TEXT("[UIProbe] roots=%d layers=%d total=%.2f screens"), Roots.Num(), Layers.Num(), Total / (2220.0 * 1080.0));
		for (int32 Index = 0; Index < FMath::Min(70, Layers.Num()); ++Index)
		{
			const FLayer& Layer = Layers[Index];
			const UUserWidget* Owner = Layer.Widget->GetTypedOuter<UUserWidget>();
			UE_LOG(LogTemp, Display, TEXT("[UIProbe] L %9.0f %-8s %-34s in %-28s %-8s %s"),
				Layer.Area, *Layer.Kind, *Layer.Widget->GetName(), Owner ? *Owner->GetClass()->GetName() : TEXT("-"),
				DrawAsName(Layer.Brush), *ResourceName(Layer.Brush));
		}
		ByResource.ValueSort([](double A, double B) { return A > B; });
		for (const TPair<FString, double>& Pair : ByResource)
		{
			UE_LOG(LogTemp, Display, TEXT("[UIProbe] R %6.2f screens x%-3d %s"), Pair.Value / (2220.0 * 1080.0), CountByResource[Pair.Key], *Pair.Key);
		}
	}

	static void Hide(const TArray<FString>& Arguments)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UIProbe_Hide);
		if (Arguments.Num() < 2)
		{
			UE_LOG(LogTemp, Error, TEXT("[UIProbe] Usage: CGR.UIProbe.Hide <RootClassPattern> <Mode> [Pattern]"));
			return;
		}
		const FString& Mode = Arguments[1];
		const FString Pattern = Arguments.Num() > 2 ? Arguments[2] : FString();
		TArray<UUserWidget*> Roots;
		const TArray<UWidget*> Widgets = GatherWidgets(Arguments[0], Roots);
		int32 Hidden = 0;
		double Area = 0.0;
		if (Mode == TEXT("name") || Mode == TEXT("class"))
		{
			// 컨테이너·하위 위젯 통째로 — 이름·클래스로 고른 묶음의 전체 비용
			for (UWidget* Widget : Widgets)
			{
				if (!Widget || !IsShown(Widget) || Roots.Contains(Widget)) continue;
				const FString Key = Mode == TEXT("name") ? Widget->GetName() : Widget->GetClass()->GetName();
				if (!Key.Contains(Pattern)) continue;
				const FVector2D Size = Widget->GetCachedGeometry().GetAbsoluteSize();
				Area += Size.X * Size.Y;
				PushVisibility(Widget);
				++Hidden;
			}
		}
		else
		{
			for (const FLayer& Layer : GatherLayers(Widgets))
			{
				if (Roots.Contains(Layer.Widget) || !Matches(Layer, Mode, Pattern)) continue;
				Area += Layer.Area;
				HideOwnDraw(Layer);
				++Hidden;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("[UIProbe] hide %s %s %s -> %d layers, %.2f screens"), *Arguments[0], *Mode, *Pattern, Hidden, Area / (2220.0 * 1080.0));
	}

	static void Restore(const TArray<FString>& Arguments)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UIProbe_Restore);
		const int32 Count = UndoStack.Num();
		for (int32 Index = UndoStack.Num() - 1; Index >= 0; --Index)
		{
			const FUndo& Undo = UndoStack[Index];
			UWidget* Widget = Undo.Widget.Get();
			if (!Widget) continue;
			switch (Undo.Kind)
			{
			case EUndoKind::Visibility: Widget->SetVisibility(Undo.Visibility); break;
			case EUndoKind::BorderBrush: CastChecked<UBorder>(Widget)->SetBrush(Undo.Brush); break;
			case EUndoKind::ButtonStyle: CastChecked<UButton>(Widget)->SetStyle(Undo.Style); break;
			}
		}
		UndoStack.Reset();
		UE_LOG(LogTemp, Display, TEXT("[UIProbe] restored %d"), Count);
	}

	static FAutoConsoleCommand DumpCommand(TEXT("CGR.UIProbe.Dump"),
		TEXT("Log drawn UI layers under on-screen widgets whose class contains the pattern"), FConsoleCommandWithArgsDelegate::CreateStatic(&Dump));
	static FAutoConsoleCommand HideCommand(TEXT("CGR.UIProbe.Hide"),
		TEXT("Hide UI layers by kind, brush resource, name or class for GPU A/B"), FConsoleCommandWithArgsDelegate::CreateStatic(&Hide));
	static FAutoConsoleCommand RestoreCommand(TEXT("CGR.UIProbe.Restore"),
		TEXT("Restore everything hidden by CGR.UIProbe.Hide"), FConsoleCommandWithArgsDelegate::CreateStatic(&Restore));
}

#endif
