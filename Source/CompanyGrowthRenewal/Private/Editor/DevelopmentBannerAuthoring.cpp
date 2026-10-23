#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Animation/MovieScene2DTransformSection.h"
#include "Animation/MovieScene2DTransformTrack.h"
#include "Animation/WidgetAnimation.h"
#include "Animation/WidgetAnimationBinding.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "MovieScene.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Table/WidgetDataTable.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "UI/Element/Effects/DevelopmentBannerWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPtr.h"
#include "WidgetBlueprint.h"

#include <initializer_list>

namespace CGRDevelopmentBannerAuthoring
{
	struct FBannerAsset
	{
		const TCHAR* ObjectPath;
		bool bCompletion;
	};

	static constexpr FBannerAsset BannerAssets[] = {
		{ TEXT("/Game/CompanyGrowth/UI/Elements/Effects/UIE_DevelopmentBanner.UIE_DevelopmentBanner"), true }
	};

	static bool RowsEqual(const FWidgetDataTable& A, const FWidgetDataTable& B);

	static bool CanAuthor(const TArray<FString>& Arguments, const TCHAR* Usage)
	{
		if (!IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] Requires the editor game thread with PIE/SIE stopped."));
			return false;
		}
		if (!Arguments.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] Usage: %s"), Usage);
			return false;
		}
		return true;
	}

	static bool SaveAsset(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
	}

	static float BannerAlpha(float Time, bool bCompletion)
	{
		const float VisualAge = bCompletion ? FMath::Max(.18f, Time) : Time;
		const float Enter = FMath::Clamp(VisualAge / .16f, 0.f, 1.f);
		return bCompletion
			? Enter * FMath::Clamp((2.2f - VisualAge) / .5f, 0.f, 1.f)
			: FMath::Min(Enter, FMath::Clamp((1.34f - VisualAge) * 4.f, 0.f, 1.f));
	}

	static float BannerScale(float Time, bool bCompletion)
	{
		const float VisualAge = bCompletion ? FMath::Max(.18f, Time) : Time;
		return 1.f + .10f * FMath::Sin(FMath::Clamp(VisualAge / .34f, 0.f, 1.f) * PI) * FMath::Exp(-VisualAge * 2.f);
	}

	static bool WriteAnimation(UWidgetBlueprint* Blueprint, bool bCompletion)
	{
		UWidgetAnimation* Animation = nullptr;
		for (UWidgetAnimation* Candidate : Blueprint->Animations)
		{
			if (Candidate && Candidate->GetFName() == TEXT("Presentation"))
			{
				if (Animation) return false;
				Animation = Candidate;
			}
		}
		if (!Animation)
		{
			Animation = FindObject<UWidgetAnimation>(Blueprint, TEXT("Presentation"));
			if (!Animation) Animation = NewObject<UWidgetAnimation>(Blueprint, TEXT("Presentation"), RF_Transactional);
			Blueprint->Animations.Add(Animation);
		}
		if (!Animation) return false;
		Blueprint->Modify();
		Animation->Modify();
		Animation->SetDisplayLabel(TEXT("Presentation"));
		Animation->AnimationBindings.Reset();
		if (UMovieScene* PreviousScene = Animation->MovieScene)
		{
			const FName DiscardName = MakeUniqueObjectName(GetTransientPackage(), UMovieScene::StaticClass(), TEXT("DiscardedDevelopmentBannerScene"));
			if (!PreviousScene->Rename(*DiscardName.ToString(), GetTransientPackage(),
				REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders)) return false;
			Animation->MovieScene = nullptr;
		}
		if (UMovieScene* OrphanedScene = FindObject<UMovieScene>(Animation, TEXT("Presentation")))
		{
			const FName DiscardName = MakeUniqueObjectName(GetTransientPackage(), UMovieScene::StaticClass(), TEXT("DiscardedDevelopmentBannerScene"));
			if (!OrphanedScene->Rename(*DiscardName.ToString(), GetTransientPackage(),
				REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders)) return false;
		}
		UMovieScene* Scene = NewObject<UMovieScene>(Animation, TEXT("Presentation"), RF_Transactional);
		if (!Scene) return false;
		Animation->MovieScene = Scene;
		Scene->SetDisplayRate(FFrameRate(60, 1));
		Scene->SetTickResolutionDirectly(FFrameRate(60000, 1));
		const float Duration = bCompletion ? 2.2f : 1.34f;
		const int32 EndFrame = FMath::RoundToInt(Duration * 60000.f);
		Scene->SetPlaybackRange(FFrameNumber(0), EndFrame + 1);
		Scene->GetEditorData().WorkStart = 0.0;
		Scene->GetEditorData().WorkEnd = Duration;

		const FGuid RootGuid = Scene->AddPossessable(TEXT("AnimationRoot"), UOverlay::StaticClass());
		if (!RootGuid.IsValid()) return false;
		FWidgetAnimationBinding Binding;
		Binding.AnimationGuid = RootGuid;
		Binding.WidgetName = TEXT("AnimationRoot");
		Binding.bIsRootWidget = false;
		Animation->AnimationBindings.Add(Binding);

		UMovieSceneFloatTrack* OpacityTrack = Scene->AddTrack<UMovieSceneFloatTrack>(RootGuid);
		UMovieScene2DTransformTrack* TransformTrack = Scene->AddTrack<UMovieScene2DTransformTrack>(RootGuid);
		if (!OpacityTrack || !TransformTrack) return false;
		OpacityTrack->SetPropertyNameAndPath(TEXT("RenderOpacity"), TEXT("RenderOpacity"));
		TransformTrack->SetPropertyNameAndPath(TEXT("RenderTransform"), TEXT("RenderTransform"));
		UMovieSceneFloatSection* Opacity = Cast<UMovieSceneFloatSection>(OpacityTrack->CreateNewSection());
		UMovieScene2DTransformSection* Transform = Cast<UMovieScene2DTransformSection>(TransformTrack->CreateNewSection());
		if (!Opacity || !Transform) return false;
		OpacityTrack->AddSection(*Opacity);
		TransformTrack->AddSection(*Transform);
		const TRange<FFrameNumber> Range(FFrameNumber(0), FFrameNumber(EndFrame + 1));
		Opacity->SetRange(Range);
		Transform->SetRange(Range);
		Opacity->SetCompletionMode(EMovieSceneCompletionMode::KeepState);
		Transform->SetCompletionMode(EMovieSceneCompletionMode::KeepState);
		Transform->SetMask(FMovieScene2DTransformMask(EMovieScene2DTransformChannel::Scale));

		TArray<int32> KeyFrames;
		for (int32 Frame = 0; Frame <= 20; ++Frame) KeyFrames.Add(Frame * 1000);
		KeyFrames.AddUnique(FMath::RoundToInt(.16f * 60000.f));
		if (bCompletion) KeyFrames.AddUnique(FMath::RoundToInt(.18f * 60000.f));
		KeyFrames.AddUnique(FMath::RoundToInt(.34f * 60000.f));
		KeyFrames.AddUnique(FMath::RoundToInt((bCompletion ? 1.7f : 1.09f) * 60000.f));
		KeyFrames.AddUnique(EndFrame);
		KeyFrames.Sort();
		for (int32 Frame : KeyFrames)
		{
			const float Time = static_cast<float>(Frame) / 60000.f;
			Opacity->GetChannel().AddLinearKey(FFrameNumber(Frame), BannerAlpha(Time, bCompletion));
			const float Scale = BannerScale(Time, bCompletion);
			Transform->Scale[0].AddLinearKey(FFrameNumber(Frame), Scale);
			Transform->Scale[1].AddLinearKey(FFrameNumber(Frame), Scale);
		}
		return true;
	}

	static void AuthorAnimations(const TArray<FString>& Arguments)
	{
		if (!CanAuthor(Arguments, TEXT("CGR.DevelopmentBanner.AuthorAnimations"))) return;
		UWidgetBlueprint* Blueprints[UE_ARRAY_COUNT(BannerAssets)] = {};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(BannerAssets); ++Index)
		{
			Blueprints[Index] = TSoftObjectPtr<UWidgetBlueprint>(FSoftObjectPath(BannerAssets[Index].ObjectPath)).LoadSynchronous();
			UWidgetBlueprint* Blueprint = Blueprints[Index];
			if (!Blueprint || Blueprint->ParentClass != UDevelopmentBannerWidget::StaticClass()
				|| !Blueprint->WidgetTree || !Blueprint->WidgetTree->FindWidget<UOverlay>(TEXT("AnimationRoot")))
			{
				UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] Missing direct-parent WBP or AnimationRoot: %s"), BannerAssets[Index].ObjectPath);
				return;
			}
			if (Blueprint->GetOutermost()->IsDirty())
			{
				UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] Save unsaved WBP changes before authoring: %s"), BannerAssets[Index].ObjectPath);
				return;
			}
		}
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(BannerAssets); ++Index)
		{
			UWidgetBlueprint* Blueprint = Blueprints[Index];
			if (!WriteAnimation(Blueprint, BannerAssets[Index].bCompletion))
			{
				UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] Animation write failed: %s"), BannerAssets[Index].ObjectPath);
				return;
			}
			FCompilerResultsLog CompileLog;
			FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog);
			if (CompileLog.NumErrors != 0 || CompileLog.NumWarnings != 0 || Blueprint->Status == BS_Error)
			{
				UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] WBP compile not clean: %s (errors=%d warnings=%d)"),
					BannerAssets[Index].ObjectPath, CompileLog.NumErrors, CompileLog.NumWarnings);
				return;
			}
			if (!SaveAsset(Blueprint))
			{
				UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] WBP save failed: %s"), BannerAssets[Index].ObjectPath);
				return;
			}
			UE_LOG(LogTemp, Display, TEXT("[DevelopmentBannerAuthoring] Presentation saved: %s"), BannerAssets[Index].ObjectPath);
		}
	}

	static bool WriteStartAnimation(UWidgetBlueprint* Blueprint)
	{
		UWidgetAnimation* Animation = nullptr;
		for (UWidgetAnimation* Candidate : Blueprint->Animations)
		{
			if (Candidate && Candidate->GetFName() == TEXT("Presentation"))
			{
				if (Animation) return false;
				Animation = Candidate;
			}
		}
		if (!Animation)
		{
			Animation = FindObject<UWidgetAnimation>(Blueprint, TEXT("Presentation"));
			if (!Animation) Animation = NewObject<UWidgetAnimation>(Blueprint, TEXT("Presentation"), RF_Transactional);
			Blueprint->Animations.Add(Animation);
		}
		if (!Animation) return false;
		Blueprint->Modify();
		Animation->Modify();
		Animation->SetDisplayLabel(TEXT("Presentation"));
		Animation->AnimationBindings.Reset();
		if (UMovieScene* PreviousScene = Animation->MovieScene)
		{
			const FName DiscardName = MakeUniqueObjectName(GetTransientPackage(), UMovieScene::StaticClass(), TEXT("DiscardedDevelopmentStartScene"));
			if (!PreviousScene->Rename(*DiscardName.ToString(), GetTransientPackage(),
				REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders)) return false;
			Animation->MovieScene = nullptr;
		}
		if (UMovieScene* OrphanedScene = FindObject<UMovieScene>(Animation, TEXT("Presentation")))
		{
			const FName DiscardName = MakeUniqueObjectName(GetTransientPackage(), UMovieScene::StaticClass(), TEXT("DiscardedDevelopmentStartScene"));
			if (!OrphanedScene->Rename(*DiscardName.ToString(), GetTransientPackage(),
				REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders)) return false;
		}
		UMovieScene* Scene = NewObject<UMovieScene>(Animation, TEXT("Presentation"), RF_Transactional);
		if (!Scene) return false;
		Animation->MovieScene = Scene;
		Scene->SetDisplayRate(FFrameRate(60, 1));
		Scene->SetTickResolutionDirectly(FFrameRate(60000, 1));
		constexpr float Duration = .95f;
		const int32 EndFrame = FMath::RoundToInt(Duration * 60000.f);
		Scene->SetPlaybackRange(FFrameNumber(0), EndFrame + 1);
		Scene->GetEditorData().WorkStart = 0.0;
		Scene->GetEditorData().WorkEnd = Duration;
		const TRange<FFrameNumber> Range(FFrameNumber(0), FFrameNumber(EndFrame + 1));

		auto Bind = [&](const TCHAR* WidgetName, UClass* WidgetClass) -> FGuid
		{
			const FGuid Guid = Scene->AddPossessable(WidgetName, WidgetClass);
			if (Guid.IsValid())
			{
				FWidgetAnimationBinding Binding;
				Binding.AnimationGuid = Guid;
				Binding.WidgetName = WidgetName;
				Binding.bIsRootWidget = false;
				Animation->AnimationBindings.Add(Binding);
			}
			return Guid;
		};
		auto AddOpacity = [&](const FGuid& Guid, std::initializer_list<FVector2D> Keys) -> bool
		{
			UMovieSceneFloatTrack* Track = Scene->AddTrack<UMovieSceneFloatTrack>(Guid);
			if (!Track) return false;
			Track->SetPropertyNameAndPath(TEXT("RenderOpacity"), TEXT("RenderOpacity"));
			UMovieSceneFloatSection* Section = Cast<UMovieSceneFloatSection>(Track->CreateNewSection());
			if (!Section) return false;
			Track->AddSection(*Section);
			Section->SetRange(Range);
			Section->SetCompletionMode(EMovieSceneCompletionMode::KeepState);
			for (const FVector2D& Key : Keys)
			{
				Section->GetChannel().AddLinearKey(FFrameNumber(FMath::RoundToInt32(Key.X * 60000.f)), static_cast<float>(Key.Y));
			}
			return true;
		};
		auto AddTransform = [&](const FGuid& Guid, EMovieScene2DTransformChannel Mask,
			std::initializer_list<FVector2D> XKeys, std::initializer_list<FVector2D> YKeys,
			std::initializer_list<FVector2D> ScaleKeys) -> bool
		{
			UMovieScene2DTransformTrack* Track = Scene->AddTrack<UMovieScene2DTransformTrack>(Guid);
			if (!Track) return false;
			Track->SetPropertyNameAndPath(TEXT("RenderTransform"), TEXT("RenderTransform"));
			UMovieScene2DTransformSection* Section = Cast<UMovieScene2DTransformSection>(Track->CreateNewSection());
			if (!Section) return false;
			Track->AddSection(*Section);
			Section->SetRange(Range);
			Section->SetCompletionMode(EMovieSceneCompletionMode::KeepState);
			Section->SetMask(FMovieScene2DTransformMask(Mask));
			for (const FVector2D& Key : XKeys)
			{
				Section->Translation[0].AddLinearKey(FFrameNumber(FMath::RoundToInt32(Key.X * 60000.f)), static_cast<float>(Key.Y));
			}
			for (const FVector2D& Key : YKeys)
			{
				Section->Translation[1].AddLinearKey(FFrameNumber(FMath::RoundToInt32(Key.X * 60000.f)), static_cast<float>(Key.Y));
			}
			for (const FVector2D& Key : ScaleKeys)
			{
				const FFrameNumber Frame(FMath::RoundToInt32(Key.X * 60000.f));
				Section->Scale[0].AddLinearKey(Frame, static_cast<float>(Key.Y));
				Section->Scale[1].AddLinearKey(Frame, static_cast<float>(Key.Y));
			}
			return true;
		};

		const FGuid Root = Bind(TEXT("AnimationRoot"), UOverlay::StaticClass());
		const FGuid Dim = Bind(TEXT("DimImage"), UImage::StaticClass());
		const FGuid Ribbon = Bind(TEXT("RibbonRoot"), UCanvasPanel::StaticClass());
		const FGuid Title = Bind(TEXT("TitleText"), UCommonTextBlock::StaticClass());
		const FGuid Sheen = Bind(TEXT("SheenImage"), UImage::StaticClass());
		if (!Root.IsValid() || !Dim.IsValid() || !Ribbon.IsValid() || !Title.IsValid() || !Sheen.IsValid()) return false;
		return AddOpacity(Root, {{0.f, 0.f}, {.045f, 1.f}, {.74f, 1.f}, {.95f, 0.f}})
			&& AddTransform(Root, EMovieScene2DTransformChannel::TranslationX,
				{{0.f, 0.f}, {.74f, 0.f}, {.95f, 95.f}}, {}, {})
			&& AddOpacity(Dim, {{0.f, 0.f}, {.045f, 1.f}, {.74f, 1.f}, {.95f, 0.f}})
			&& AddOpacity(Ribbon, {{0.f, 0.f}, {.055f, 1.f}, {.74f, 1.f}, {.95f, 1.f}})
			&& AddTransform(Ribbon, EMovieScene2DTransformChannel::ScaleX,
				{}, {}, {{0.f, 0.f}, {.18f, 1.f}, {.95f, 1.f}})
			&& AddOpacity(Title, {{0.f, 0.f}, {.04f, 0.f}, {.12f, 1.f}, {.95f, 1.f}})
			&& AddTransform(Title, EMovieScene2DTransformChannel::TranslationY | EMovieScene2DTransformChannel::Scale,
				{}, {{.035f, 15.f}, {.22f, 0.f}, {.95f, 0.f}},
				{{.035f, 1.025f}, {.22f, 1.f}, {.95f, 1.f}})
			&& AddOpacity(Sheen, {{0.f, 0.f}, {.34f, 0.f}, {.455f, .28f}, {.595f, 0.f}, {.95f, 0.f}})
			&& AddTransform(Sheen, EMovieScene2DTransformChannel::TranslationX,
				{{.34f, -280.f}, {.595f, 280.f}, {.95f, 280.f}}, {}, {});
	}

	static void AuthorStartAnimations(const TArray<FString>& Arguments)
	{
		if (!CanAuthor(Arguments, TEXT("CGR.DevelopmentBanner.AuthorStartAnimations"))) return;
		const TCHAR* Path = TEXT("/Game/CompanyGrowth/UI/Elements/Effects/UIE_DevelopmentStart.UIE_DevelopmentStart");
		UWidgetBlueprint* Blueprint = TSoftObjectPtr<UWidgetBlueprint>(FSoftObjectPath(Path)).LoadSynchronous();
		if (!Blueprint || Blueprint->ParentClass != UDevelopmentBannerWidget::StaticClass() || !Blueprint->WidgetTree
			|| Blueprint->GetOutermost()->IsDirty()
			|| !Blueprint->WidgetTree->FindWidget<UOverlay>(TEXT("AnimationRoot"))
			|| !Blueprint->WidgetTree->FindWidget<UCanvasPanel>(TEXT("RibbonRoot"))
			|| !Blueprint->WidgetTree->FindWidget<UCommonTextBlock>(TEXT("TitleText"))
			|| !Blueprint->WidgetTree->FindWidget<UImage>(TEXT("DimImage"))
			|| !Blueprint->WidgetTree->FindWidget<UImage>(TEXT("SheenImage")))
		{
			UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] Save a clean direct-parent DevelopmentStart WBP with required layers first."));
			return;
		}
		if (!WriteStartAnimation(Blueprint))
		{
			UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] Start animation write failed."));
			return;
		}
		FCompilerResultsLog CompileLog;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog);
		if (CompileLog.NumErrors != 0 || CompileLog.NumWarnings != 0 || Blueprint->Status == BS_Error || !SaveAsset(Blueprint))
		{
			UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] Start WBP compile/save failed (errors=%d warnings=%d)."),
				CompileLog.NumErrors, CompileLog.NumWarnings);
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[DevelopmentBannerAuthoring] DevelopmentStart Presentation saved at 0.95 seconds."));
	}

	static void RegisterStartWidget(const TArray<FString>& Arguments)
	{
		if (!CanAuthor(Arguments, TEXT("CGR.DevelopmentBanner.RegisterStartWidget"))) return;
		const FName RowName(TEXT("DevelopmentStart"));
		if (StaticEnum<EWidgetType>()->GetNameStringByValue(static_cast<int64>(EWidgetType::DevelopmentStart)) != RowName.ToString())
		{
			UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] EWidgetType::DevelopmentStart missing."));
			return;
		}
		UClass* WidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/UI/Elements/Effects/UIE_DevelopmentStart.UIE_DevelopmentStart_C"))).LoadSynchronous();
		if (!WidgetClass || !WidgetClass->IsChildOf(UDevelopmentBannerWidget::StaticClass()) || WidgetClass->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] DevelopmentStart WBP class missing/incompatible."));
			return;
		}
		UDataTable* Table = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/Table/UI/DT_WidgetClass.DT_WidgetClass"))).LoadSynchronous();
		if (!Table || Table->GetRowStruct() != FWidgetDataTable::StaticStruct() || Table->GetOutermost()->IsDirty())
		{
			UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] Save clean DT_WidgetClass before registering DevelopmentStart."));
			return;
		}
		TMap<FName, FWidgetDataTable> BeforeRows;
		TMap<FName, TArray<uint8>> OtherRowBytes;
		const int32 RowSize = FWidgetDataTable::StaticStruct()->GetStructureSize();
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			if (!Pair.Value) return;
			const FWidgetDataTable& Existing = *reinterpret_cast<const FWidgetDataTable*>(Pair.Value);
			BeforeRows.Add(Pair.Key, Existing);
			if (Existing.WidgetType == EWidgetType::DevelopmentStart && Pair.Key != RowName)
			{
				UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] DevelopmentStart enum already used by %s."), *Pair.Key.ToString());
				return;
			}
			if (Pair.Key == RowName && Existing.WidgetType != EWidgetType::DevelopmentStart)
			{
				UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] DevelopmentStart row name occupied."));
				return;
			}
			if (Pair.Key != RowName) OtherRowBytes.FindOrAdd(Pair.Key).Append(Pair.Value, RowSize);
		}
		FWidgetDataTable Desired;
		Desired.WidgetType = EWidgetType::DevelopmentStart;
		Desired.WidgetClass = WidgetClass;
		if (const FWidgetDataTable* Current = BeforeRows.Find(RowName))
		{
			if (RowsEqual(*Current, Desired))
			{
				UE_LOG(LogTemp, Display, TEXT("[DevelopmentBannerAuthoring] DevelopmentStart row already registered."));
				return;
			}
		}
		UPackage* Package = Table->GetOutermost();
		const bool bWasDirty = Package->IsDirty();
		Table->Modify(false);
		Table->AddRow(RowName, Desired);
		const FWidgetDataTable* Updated = Table->FindRow<FWidgetDataTable>(RowName, TEXT("DevelopmentStart target"), false);
		bool bValid = Table->GetRowMap().Num() == BeforeRows.Num() + (BeforeRows.Contains(RowName) ? 0 : 1)
			&& Updated && RowsEqual(*Updated, Desired);
		for (const TPair<FName, TArray<uint8>>& Pair : OtherRowBytes)
		{
			const FWidgetDataTable* Current = Table->FindRow<FWidgetDataTable>(Pair.Key, TEXT("DevelopmentStart preserved"), false);
			bValid &= Current && RowsEqual(*Current, BeforeRows.FindChecked(Pair.Key))
				&& FMemory::Memcmp(Pair.Value.GetData(), Current, Pair.Value.Num()) == 0;
		}
		if (!bValid || !SaveAsset(Table))
		{
			if (const FWidgetDataTable* Previous = BeforeRows.Find(RowName)) Table->AddRow(RowName, *Previous);
			else Table->RemoveRow(RowName);
			Package->SetDirtyFlag(bWasDirty);
			UE_LOG(LogTemp, Error, TEXT("[DevelopmentBannerAuthoring] DevelopmentStart merge/save failed; target row restored in memory."));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[DevelopmentBannerAuthoring] DevelopmentStart registered; %d -> %d rows, all other row bytes unchanged."),
			BeforeRows.Num(), Table->GetRowMap().Num());
	}

	static bool RowsEqual(const FWidgetDataTable& A, const FWidgetDataTable& B)
	{
		return static_cast<uint8>(A.WidgetType) == static_cast<uint8>(B.WidgetType)
			&& FWidgetDataTable::StaticStruct()->CompareScriptStruct(&A, &B, 0);
	}

	static FAutoConsoleCommand AuthorAnimationsCommand(TEXT("CGR.DevelopmentBanner.AuthorAnimations"),
		TEXT("Create or update editable Presentation animations on all three DevelopmentBanner WBP assets."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&AuthorAnimations));
	static FAutoConsoleCommand AuthorStartAnimationsCommand(TEXT("CGR.DevelopmentBanner.AuthorStartAnimations"),
		TEXT("Author the 0.95 second Presentation animation on UIE_DevelopmentStart only."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&AuthorStartAnimations));
	static FAutoConsoleCommand RegisterStartWidgetCommand(TEXT("CGR.DevelopmentBanner.RegisterStartWidget"),
		TEXT("Upsert the DevelopmentStart DT_WidgetClass row while preserving all other rows."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RegisterStartWidget));
}

#endif
