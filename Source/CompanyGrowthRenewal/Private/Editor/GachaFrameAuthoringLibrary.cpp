#include "Editor/GachaFrameAuthoringLibrary.h"

#if WITH_EDITOR
#include "Algo/Find.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditorUtils.h"

TArray<UWidget*> UGachaFrameAuthoringLibrary::CloneWidgetsInto(UObject* TargetBlueprint, const TArray<UWidget*>& Sources)
{
	TArray<UWidget*> Result;
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(TargetBlueprint);
	if (!Blueprint || !Blueprint->WidgetTree || Sources.IsEmpty())
	{
		return Result;
	}
	// 디자이너 복사와 같다 ― 내보내기는 넘긴 위젯만 싣기 때문에 하위까지 모아야 슬롯 내용이 끊기지 않는다
	TSet<UWidget*> ToExport;
	for (UWidget* Source : Sources)
	{
		if (!Source) { return Result; }
		TArray<UWidget*> Subtree = { Source };
		UWidgetTree::GetChildWidgets(Source, Subtree);
		ToExport.Append(Subtree);
	}
	// 내보내기는 다른 트리 위젯이 하나라도 섞이면 check 로 에디터를 죽인다 ― 하위까지 전부 같은 트리 소속이어야 한다
	const UObject* const Scope = Sources[0]->GetOuter();
	for (const UWidget* Widget : ToExport)
	{
		if (Widget->GetOuter() != Scope) { return Result; }
	}
	FString Text;
	FWidgetBlueprintEditorUtils::ExportWidgetsToText(ToExport.Array(), Text);
	TSet<UWidget*> Imported;
	TMap<FName, UWidgetSlotPair*> SlotData;
	FWidgetBlueprintEditorUtils::ImportWidgetsFromText(Blueprint, Text, Imported, SlotData);

	TSet<FName> ExportedNames, ImportedNames;
	for (const UWidget* Widget : ToExport) { ExportedNames.Add(Widget->GetFName()); }
	for (const UWidget* Widget : Imported) { ImportedNames.Add(Widget->GetFName()); }
	TArray<FString> Mismatched;
	for (const FName& Missing : ExportedNames.Difference(ImportedNames)) { Mismatched.Add(TEXT("missing:") + Missing.ToString()); }
	for (const FName& Renamed : ImportedNames.Difference(ExportedNames)) { Mismatched.Add(TEXT("renamed:") + Renamed.ToString()); }
	for (UWidget* Source : Sources)
	{
		UWidget* const* Match = Algo::FindByPredicate(Imported, [Source](const UWidget* Candidate)
		{
			return Candidate && Candidate->GetFName() == Source->GetFName() && Candidate->GetParent() == nullptr;
		});
		if (!Match) { Mismatched.Add(TEXT("root:") + Source->GetName()); }
		Result.Add(Match ? *Match : nullptr);
	}
	if (Mismatched.IsEmpty())
	{
		return Result;
	}
	// 이름이 어긋난 복제본이 트리에 남으면 이어서 저장하는 호출자가 매달리지 않은 하위 트리를 에셋에 굳힌다 ― 트랜션트로 치운다
	UE_LOG(LogTemp, Warning, TEXT("CloneWidgetsInto(%s): imported names differ from exported, clones discarded: %s"),
		*Blueprint->GetName(), *FString::Join(Mismatched, TEXT(", ")));
	UPackage* const Discard = GetTransientPackage();
	for (UWidget* Widget : Imported)
	{
		FName FreeName = MakeUniqueObjectName(Discard, Widget->GetClass(), Widget->GetFName());
		while (StaticFindObjectFast(nullptr, Discard, FreeName)) { FreeName = MakeUniqueObjectName(Discard, Widget->GetClass(), Widget->GetFName()); }
		// 방금 가져온 객체라 링커 익스포트가 없다 ― 대상 WBP 패키지의 로더 리셋은 필요 없다
		Widget->Rename(*FreeName.ToString(), Discard, REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
	}
	return TArray<UWidget*>();
}

bool UGachaFrameAuthoringLibrary::SetNamedSlotContent(UUserWidget* Host, FName SlotName, UWidget* Content)
{
	// 부모가 있는 위젯을 넣으면 두 곳에 동시에 매달린다 ― 떼기는 호출자 몫이라 여기선 실패로 드러낸다
	if (!Host || (Content && Content->GetParent() != nullptr)) { return false; }
	// 클래스에 없는 칸 이름의 바인딩은 저장 때 조용히 지워진다
	TArray<FName> SlotNames;
	Host->GetSlotNames(SlotNames);
	if (!SlotNames.Contains(SlotName)) { return false; }
	Host->Modify();
	Host->SetContentForSlot(SlotName, Content);
	return Host->GetContentForSlot(SlotName) == Content;
}

UWidget* UGachaFrameAuthoringLibrary::GetNamedSlotContent(UUserWidget* Host, FName SlotName)
{
	return Host ? Host->GetContentForSlot(SlotName) : nullptr;
}

bool UGachaFrameAuthoringLibrary::ReplaceRootWidget(UObject* TargetBlueprint, UWidget* ExpectedRoot, UWidget* ReplacementRoot)
{
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(TargetBlueprint);
	UWidgetTree* TargetTree = Blueprint ? Blueprint->WidgetTree : nullptr;
	if (!GEditor || GEditor->PlayWorld || !TargetTree || !ExpectedRoot || !ReplacementRoot || ExpectedRoot == ReplacementRoot
		|| TargetTree->RootWidget != ExpectedRoot || ExpectedRoot->GetOuter() != TargetTree
		|| ReplacementRoot->GetOuter() != TargetTree || ReplacementRoot->GetParent() != nullptr)
	{
		return false;
	}

	Blueprint->Modify();
	TargetTree->Modify();
	TargetTree->RootWidget = ReplacementRoot;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

bool UGachaFrameAuthoringLibrary::SetDesignerPreview(UObject* TargetBlueprint, FVector2D PreviewSize)
{
	UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(TargetBlueprint);
	UUserWidget* Defaults = Blueprint && Blueprint->GeneratedClass
		? Cast<UUserWidget>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	if (!GEditor || GEditor->PlayWorld || !Blueprint || !Blueprint->WidgetTree || !Defaults
		|| !FMath::IsFinite(PreviewSize.X) || !FMath::IsFinite(PreviewSize.Y)
		|| PreviewSize.X <= 0.f || PreviewSize.Y <= 0.f)
	{
		return false;
	}
	Blueprint->Modify();
	Defaults->Modify();
	Defaults->DesignSizeMode = EDesignPreviewSizeMode::Custom;
	Defaults->DesignTimeSize = PreviewSize;
	Blueprint->WidgetTree->ForEachWidget([](UWidget* Widget)
	{
		Widget->Modify();
		const ESlateVisibility Visibility = Widget->GetVisibility();
		Widget->bHiddenInDesigner = Visibility == ESlateVisibility::Collapsed || Visibility == ESlateVisibility::Hidden;
	});
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return true;
}

void UGachaFrameAuthoringLibrary::MarkStructurallyModified(UObject* Blueprint)
{
	if (UBlueprint* Typed = Cast<UBlueprint>(Blueprint))
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Typed);
	}
}
#endif
