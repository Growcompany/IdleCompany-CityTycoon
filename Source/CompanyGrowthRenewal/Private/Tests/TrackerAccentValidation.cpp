#if WITH_EDITOR && !UE_BUILD_SHIPPING

#include "UI/Element/GoalTrackerRowWidget.h"
#include "UI/HUD/MissionTrackerWidget.h"
#include "Table/WidgetDataTable.h"
#include "Components/Image.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"

namespace TrackerAccentValidation
{
static bool CaptureWidget(UUserWidget& Widget, const FString& Filename, FVector2D Size)
{
	const TSharedRef<SWidget> Surface = Widget.TakeWidget();
	Widget.SetVisibility(ESlateVisibility::HitTestInvisible);
	FWidgetRenderer Renderer(true);
	TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, false));
	if (!Target.IsValid()) return false;
	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		Renderer.DrawWidget(Target.Get(), Surface, Size, 0.f);
		FlushRenderingCommands();
	}
	FImage RenderedImage;
	TArray64<uint8> Bytes;
	if (!FImageUtils::GetRenderTargetImage(Target.Get(), RenderedImage)) return false;
	RenderedImage.GammaSpace = EGammaSpace::sRGB;
	return FImageUtils::CompressImage(Bytes, TEXT("png"), RenderedImage)
		&& FFileHelper::SaveArrayToFile(Bytes, *Filename);
}

static void Capture()
{
	UWorld* CaptureWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UDataTable* WidgetTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass"))).LoadSynchronous();
	UDataTable* GoalTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/Table/Mission/DT_Goal.DT_Goal"))).LoadSynchronous();
	if (!CaptureWorld || !WidgetTable || !GoalTable) return;
	TArray<FWidgetDataTable*> WidgetRows;
	WidgetTable->GetAllRows(TEXT("TrackerAccentValidation"), WidgetRows);
	TSubclassOf<UUserWidget> GoalClass;
	TSubclassOf<UUserWidget> MissionClass;
	for (const FWidgetDataTable* Row : WidgetRows)
	{
		if (Row->WidgetType == EWidgetType::GoalTrackerRow) GoalClass = Row->WidgetClass;
		if (Row->WidgetType == EWidgetType::MissionTracker) MissionClass = Row->WidgetClass;
	}
	const TArray<FName> GoalNames = GoalTable->GetRowNames();
	if (!GoalClass || !MissionClass || GoalNames.IsEmpty()) return;
	const FGoalTable* GoalData = GoalTable->FindRow<FGoalTable>(GoalNames[0], TEXT("TrackerAccentValidation"));
	if (!GoalData) return;
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("TrackerAccentValidation"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	FString Report(TEXT("UI-only editor-world captures; no gameplay manager initialization or save mutation.\n"));
	bool bPassed = true;
	TStrongObjectPtr<UGoalTrackerRowWidget> Goal(CreateWidget<UGoalTrackerRowWidget>(CaptureWorld, GoalClass));
	if (!Goal.IsValid()) return;
	Goal->TakeWidget();
	const TCHAR* Labels[] = {TEXT("normal"), TEXT("tracked"), TEXT("practiced"), TEXT("expanded")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Labels); ++Index)
	{
		FGoalBoardEntry Entry;
		Entry.GoalID = GoalNames[0];
		Entry.Row = *GoalData;
		Entry.State = EGoalState::InProgress;
		Entry.bPracticed = Index == 2;
		Goal->Configure(Entry, Index == 1, Index == 3, FText::GetEmpty());
		UImage* Line = Cast<UImage>(Goal->GetWidgetFromName(TEXT("CardLine")));
		UMaterialInstanceDynamic* Material = Line ? Line->GetDynamicMaterial() : nullptr;
		FLinearColor Color;
		float FrameOpacity = 0.f;
		FLinearColor Expected(1.f, 0.723055f, 0.194618f, 0.95f);
		if (Index == 1) Expected = FLinearColor(0.046650f, 0.327780f, 0.745440f, 1.f);
		const bool bStatePassed = Material && Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("AccentColor")), Color)
			&& Color.Equals(Expected, 0.0001f) && !Goal->GetWidgetFromName(TEXT("AccentBar"))
			&& Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("FrameOpacity")), FrameOpacity)
			&& FMath::IsNearlyEqual(FrameOpacity, 0.15f)
			&& Line->GetColorAndOpacity().Equals(FLinearColor::White);
		const bool bSaved = CaptureWidget(*Goal, Directory / FString::Printf(TEXT("goal_%s.png"), Labels[Index]), FVector2D(620, Index == 3 ? 360 : 88));
		bPassed &= bStatePassed && bSaved;
		Report += FString::Printf(TEXT("goal_%s state=%d saved=%d frame=%f accent=%s\n"), Labels[Index], bStatePassed, bSaved, FrameOpacity, *Color.ToString());
	}
	TStrongObjectPtr<UMissionTrackerWidget> Mission(CreateWidget<UMissionTrackerWidget>(CaptureWorld, MissionClass));
	if (Mission.IsValid())
	{
		Mission->TakeWidget();
		UImage* Outline = Cast<UImage>(Mission->GetWidgetFromName(TEXT("CardOutline")));
		UMaterialInstanceDynamic* Material = Outline ? Outline->GetDynamicMaterial() : nullptr;
		bPassed &= Material && !Mission->GetWidgetFromName(TEXT("AccentBar"));
		bPassed &= CaptureWidget(*Mission, Directory / TEXT("mission_authored.png"), FVector2D(620, 300));
		if (Material)
		{
			Material->SetVectorParameterValue(TEXT("AccentColor"), FLinearColor::FromSRGBColor(FColor(0x22, 0xC5, 0x5E)));
			Material->SetScalarParameterValue(TEXT("FrameOpacity"), 0.7f);
			bPassed &= CaptureWidget(*Mission, Directory / TEXT("mission_green_visual.png"), FVector2D(620, 300));
		}
		Report += TEXT("Mission uses authored placeholder content and direct MID green preview; not a live claim-flow test.\n");
	}
	else bPassed = false;
	Report += FString::Printf(TEXT("passed=%d\n"), bPassed);
	FFileHelper::SaveStringToFile(Report, *(Directory / TEXT("report.txt")));
	UE_LOG(LogTemp, Display, TEXT("TrackerAccent.Capture passed=%d output=%s"), bPassed, *Directory);
}

static FAutoConsoleCommand Command(TEXT("TrackerAccent.Capture"),
	TEXT("Capture actual tracker WBP surfaces in editor world without changing gameplay state."),
	FConsoleCommandDelegate::CreateStatic(&Capture));
}
#endif
