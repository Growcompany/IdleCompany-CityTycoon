#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Overlay.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Slate/WidgetRenderer.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "UI/Element/Common/HelpButtonWidget.h"
#include "UI/Element/Common/ModalFrameWidget.h"
#include "UI/Element/Common/PanelHeaderWidget.h"
#include "UI/Element/Gacha/GachaPanelFrameWidget.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

// 에디터 저작/측정 전용이며 유니티 빌드의 다른 검증 파일과 심볼을 공유하지 않는다.
namespace CGRPanelLayoutAuthoring
{
	struct FPreviewErrorCounter final : FOutputDevice
	{
		int32 ErrorCount = 0;
		FPreviewErrorCounter() { GLog->AddOutputDevice(this); }
		~FPreviewErrorCounter() { GLog->RemoveOutputDevice(this); }
		virtual void Serialize(const TCHAR*, ELogVerbosity::Type Verbosity, const FName&) override
		{
			if (Verbosity == ELogVerbosity::Error || Verbosity == ELogVerbosity::Fatal) { ++ErrorCount; }
		}
	};

	static bool CheckEditor()
	{
		if (!IsInGameThread() || !GEditor || GEditor->PlayWorld || GEditor->bIsSimulatingInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] Requires the editor game thread with PIE/SIE stopped."));
			return false;
		}
		return true;
	}

	static UWidgetBlueprint* LoadPanel(const FString& AssetPath)
	{
		const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
		if (!PackageName.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(PackageName))
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] Invalid /Game package path: %s"), *AssetPath);
			return nullptr;
		}
		const FString ObjectPath = PackageName + TEXT(".") + FPackageName::GetLongPackageAssetName(PackageName);
		if (AssetPath != PackageName && AssetPath != ObjectPath)
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] Expected a package or its asset object path: %s"), *AssetPath);
			return nullptr;
		}
		TSoftObjectPtr<UWidgetBlueprint> BlueprintPath{FSoftObjectPath(ObjectPath)};
		UWidgetBlueprint* Blueprint = BlueprintPath.LoadSynchronous();
		if (!Blueprint || !Blueprint->WidgetTree || Blueprint->WidgetTree->GetOuter() != Blueprint)
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] No authored WidgetBlueprint tree: %s"), *AssetPath);
			return nullptr;
		}
		return Blueprint;
	}

	static bool IsSimpleName(const FString& Name)
	{
		if (Name.IsEmpty() || Name.Len() >= NAME_SIZE || Name.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			return false;
		}
		for (TCHAR Character : Name)
		{
			if (!((Character >= TEXT('a') && Character <= TEXT('z'))
				|| (Character >= TEXT('A') && Character <= TEXT('Z'))
				|| (Character >= TEXT('0') && Character <= TEXT('9')) || Character == TEXT('_')))
			{
				return false;
			}
		}
		return true;
	}

	static void ReplaceRoot(const TArray<FString>& Arguments)
	{
		if (!CheckEditor()) { return; }
		if (Arguments.Num() != 2 || !IsSimpleName(Arguments[1]))
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] Usage: CGR.PanelLayout.ReplaceRoot <asset path> <detached Overlay name>"));
			return;
		}
		UWidgetBlueprint* Blueprint = LoadPanel(Arguments[0]);
		if (!Blueprint) { return; }
		UWidgetTree* Tree = Blueprint->WidgetTree;
		UCanvasPanel* OldRoot = Cast<UCanvasPanel>(Tree->RootWidget);
		// FindWidget는 루트에서 순회하므로 아직 매달리지 않은 새 루트를 찾을 수 없다.
		UOverlay* NewRoot = Cast<UOverlay>(StaticFindObjectFast(nullptr, Tree, FName(*Arguments[1])));
		if (!OldRoot || OldRoot->GetOuter() != Tree || OldRoot->GetChildrenCount() != 0
			|| OldRoot->GetParent() || OldRoot->Slot || !NewRoot || NewRoot->GetClass() != UOverlay::StaticClass()
			|| NewRoot->GetOuter() != Tree || NewRoot->GetParent() || NewRoot->Slot
			|| NewRoot->GetChildrenCount() == 0 || NewRoot->GetFName() == OldRoot->GetFName())
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] %s: expected an empty authored Canvas root and a detached, nonempty Overlay in the same tree."), *Blueprint->GetName());
			return;
		}
		// 루트 소유의 instanced 객체는 바깥(Canvas)을 트랜션트로 옮길 때 참조가 끊어진다.
		for (FName PropertyName : {FName(TEXT("Navigation")), FName(TEXT("AccessibleWidgetData")), FName(TEXT("ToolTipWidget"))})
		{
			const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(UWidget::StaticClass(), PropertyName);
			if (!Property || Property->GetObjectPropertyValue_InContainer(OldRoot) || Property->GetObjectPropertyValue_InContainer(NewRoot))
			{
				UE_LOG(LogTemp, Error, TEXT("[PanelLayout] %s: configured root object property is unsupported: %s."), *Blueprint->GetName(), *PropertyName.ToString());
				return;
			}
		}
		TArray<FProperty*> RootMetadata;
		for (FName PropertyName : {FName(TEXT("bIsVariable")), FName(TEXT("bHiddenInDesigner")), FName(TEXT("bExpandedInDesigner")),
			FName(TEXT("bLockedInDesigner")), FName(TEXT("DesignerFlags")), FName(TEXT("DisplayLabel"))})
		{
			FProperty* Property = FindFProperty<FProperty>(UWidget::StaticClass(), PropertyName);
			if (!Property)
			{
				UE_LOG(LogTemp, Error, TEXT("[PanelLayout] Missing root metadata property: %s."), *PropertyName.ToString());
				return;
			}
			RootMetadata.Add(Property);
		}
		TArray<UWidget*> NewWidgets{NewRoot};
		UWidgetTree::GetChildWidgets(NewRoot, NewWidgets);
		TSet<UWidget*> UniqueWidgets;
		for (UWidget* Widget : NewWidgets)
		{
			if (!Widget || Widget == OldRoot || Widget->GetOuter() != Tree || UniqueWidgets.Contains(Widget)
				|| StaticFindObjectFast(nullptr, Tree, Widget->GetFName()) != Widget)
			{
				UE_LOG(LogTemp, Error, TEXT("[PanelLayout] %s: invalid ownership, duplicate child or name collision in replacement subtree."), *Blueprint->GetName());
				return;
			}
			UniqueWidgets.Add(Widget);
		}
		const FString OriginalName = OldRoot->GetName();
		if (StaticFindObjectFast(nullptr, Tree, OldRoot->GetFName()) != OldRoot
			|| Tree->FindWidget(NewRoot->GetFName()) != nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] %s: root name collision or replacement already reachable from the tree."), *Blueprint->GetName());
			return;
		}
		UPackage* DiscardPackage = GetTransientPackage();
		const FString DiscardName = MakeUniqueObjectName(DiscardPackage, OldRoot->GetClass(), TEXT("PanelLayoutDiscardedCanvas")).ToString();
		const ERenameFlags RenameFlags = REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_DoNotDirty;
		if (!OldRoot->Rename(*DiscardName, DiscardPackage, RenameFlags | REN_Test))
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] %s: cannot move the empty Canvas out of the package."), *Blueprint->GetName());
			return;
		}

		const bool bPackageWasDirty = Blueprint->GetOutermost()->IsDirty();
		FScopedTransaction Transaction(NSLOCTEXT("PanelLayoutAuthoring", "ReplaceRoot", "Replace panel Canvas root with Overlay"));
		Blueprint->Modify(false);
		Tree->Modify(false);
		OldRoot->SetFlags(RF_Transactional);
		NewRoot->SetFlags(RF_Transactional);
		OldRoot->Modify(false);
		NewRoot->Modify(false);
		if (!OldRoot->Rename(*DiscardName, DiscardPackage, RenameFlags))
		{
			Transaction.Cancel();
			Blueprint->GetOutermost()->SetDirtyFlag(bPackageWasDirty);
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] %s: Canvas rename failed; root unchanged."), *Blueprint->GetName());
			return;
		}
		if (!NewRoot->Rename(*OriginalName, Tree, RenameFlags | REN_Test)
			|| !NewRoot->Rename(*OriginalName, Tree, RenameFlags))
		{
			const bool bRestored = OldRoot->Rename(*OriginalName, Tree, RenameFlags | REN_Test)
				&& OldRoot->Rename(*OriginalName, Tree, RenameFlags);
			Transaction.Cancel();
			Blueprint->GetOutermost()->SetDirtyFlag(bPackageWasDirty);
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] %s: Overlay rename failed; Canvas rollback=%s. Do not save unless rollback succeeded."),
				*Blueprint->GetName(), bRestored ? TEXT("succeeded") : TEXT("FAILED"));
			return;
		}
		for (FProperty* Property : RootMetadata)
		{
			Property->CopyCompleteValue_InContainer(NewRoot, OldRoot);
		}
		Tree->RootWidget = NewRoot;
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		UE_LOG(LogTemp, Display, TEXT("[PanelLayout] ReplaceRoot SUCCESS %s root=%s children=%d; compile and save are caller responsibilities."),
			*Blueprint->GetPathName(), *OriginalName, NewRoot->GetChildrenCount());
	}

	static void GatherPreviewWidgets(UUserWidget* Panel, TArray<UWidget*>& OutWidgets)
	{
		if (!Panel->WidgetTree) { return; }
		TArray<UWidget*> TreeWidgets;
		Panel->WidgetTree->GetAllWidgets(TreeWidgets);
		for (UWidget* Widget : TreeWidgets)
		{
			if (!Widget || OutWidgets.Contains(Widget)) { continue; }
			OutWidgets.Add(Widget);
			if (UUserWidget* NestedPanel = Cast<UUserWidget>(Widget))
			{
				GatherPreviewWidgets(NestedPanel, OutWidgets);
			}
		}
	}

	static bool IsEffectivelyCollapsed(const UWidget* Widget, const UUserWidget* Panel)
	{
		for (const UWidget* Current = Widget; Current;)
		{
			if (Current->GetVisibility() == ESlateVisibility::Collapsed) { return true; }
			if (Current == Panel) { break; }
			const UWidget* ParentWidget = Current->GetParent();
			Current = ParentWidget ? ParentWidget : Current->GetTypedOuter<UUserWidget>();
		}
		return false;
	}

	static bool CaptureWidth(UWidgetBlueprint* Blueprint, UWorld* EditorWorld, const FString& Stage, int32 Width)
	{
		const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("PanelLayoutOverlay/render") / Stage);
		const FString OutputBase = Directory / FString::Printf(TEXT("%s_%d"), *Blueprint->GetName(), Width);
		const FString JsonPath = OutputBase + TEXT(".json");
		const FString PngPath = OutputBase + TEXT(".png");
		if (!IFileManager::Get().MakeDirectory(*Directory, true)
			|| !IFileManager::Get().Delete(*JsonPath, false, true)
			|| !IFileManager::Get().Delete(*PngPath, false, true))
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] Cannot prepare capture output: %s"), *OutputBase);
			return false;
		}
		FPreviewErrorCounter PreviewErrors;
		// Initialize 전 디자인 플래그를 세워 중첩 WBP의 NativeOnInitialized까지 차단한다.
		TStrongObjectPtr<UUserWidget> Panel(NewObject<UUserWidget>(EditorWorld, Blueprint->GeneratedClass, NAME_None, RF_Transient));
		if (!Panel.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] %s: preview creation failed."), *Blueprint->GetName());
			return false;
		}
		// Designing은 Construct/Destruct/활성화/게임 Tick을 막는다. PreConstruct도 실행하지 않는다.
		Panel->SetDesignerFlags(EWidgetDesignFlags::Designing);
		if (!Panel->Initialize() || !Panel->WidgetTree || !Panel->GetRootWidget())
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] %s: preview initialization failed."), *Blueprint->GetName());
			return false;
		}
		TArray<UWidget*> PreviewWidgets{Panel.Get()};
		GatherPreviewWidgets(Panel.Get(), PreviewWidgets);
		TArray<ESlateVisibility> AuthoredVisibility;
		TArray<bool> AuthoredEnabled;
		for (UWidget* Widget : PreviewWidgets)
		{
			Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
			AuthoredVisibility.Add(Widget->GetVisibility());
			AuthoredEnabled.Add(Widget->GetIsEnabled());
		}
		TSharedPtr<SWidget> PanelSlate = Panel->TakeWidget();
		ON_SCOPE_EXIT
		{
			Panel->ReleaseSlateResources(true);
			PanelSlate.Reset();
		};
		// 디자이너는 숨김을 Visible로 덮으므로, 인스턴스에만 원래 저작 Visibility를 돌려준다.
		for (int32 Index = 0; Index < PreviewWidgets.Num(); ++Index)
		{
			PreviewWidgets[Index]->SetVisibility(AuthoredVisibility[Index]);
			PreviewWidgets[Index]->SetIsEnabled(AuthoredEnabled[Index]);
		}
		// GatherPreviewWidgets는 부모를 먼저 담는다. Frame이 전달한 값을 자식 Header가 이어 적용한다.
		// 게임 생명주기는 계속 차단하고, 공용 크롬의 DT 팔레트·제목·선택 장식만 복원한다.
		for (UWidget* Widget : PreviewWidgets)
		{
			if (UModalFrameWidget* Frame = Cast<UModalFrameWidget>(Widget))
			{
				Frame->SynchronizeProperties();
			}
			else if (UGachaPanelFrameWidget* GachaFrame = Cast<UGachaPanelFrameWidget>(Widget))
			{
				GachaFrame->SynchronizeProperties();
			}
			else if (UPanelHeaderWidget* Header = Cast<UPanelHeaderWidget>(Widget))
			{
				Header->SynchronizeProperties();
			}
		}
		for (UWidget* Widget : PreviewWidgets)
		{
			if (UCloseButtonWidget* CloseButton = Cast<UCloseButtonWidget>(Widget))
			{
				CloseButton->SynchronizeProperties();
			}
			else if (UHelpButtonWidget* HelpButton = Cast<UHelpButtonWidget>(Widget))
			{
				HelpButton->SynchronizeProperties();
			}
			// Super의 디자이너 표시 강제를 걷어내고 Header가 결정한 현재 숨김/활성 상태를 유지한다.
			Widget->SetVisibility(Widget->GetVisibility());
			Widget->SetIsEnabled(Widget->GetIsEnabled());
		}

		const FVector2D DrawSize(Width, 1440);
		FWidgetRenderer Renderer(true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false));
		if (!Target.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] %s: render target creation failed."), *Blueprint->GetName());
			return false;
		}
		ON_SCOPE_EXIT
		{
			Target->ReleaseResource();
			FlushRenderingCommands();
		};
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			Renderer.DrawWidget(Target.Get(), PanelSlate.ToSharedRef(), 1.f, DrawSize, 0.f);
			FlushRenderingCommands();
		}
		const FGeometry RootGeometry = Panel->GetRootWidget()->GetCachedGeometry();
		const FVector2D RootSize = RootGeometry.GetLocalSize();
		if (!FMath::IsFinite(RootSize.X) || !FMath::IsFinite(RootSize.Y) || RootSize.X <= 0.0 || RootSize.Y <= 0.0)
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] %s: root has no usable geometry at width %d."), *Blueprint->GetName(), Width);
			return false;
		}
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetStringField(TEXT("mode"), TEXT("authored_editor_preview"));
		Report->SetStringField(TEXT("asset"), Blueprint->GetPathName());
		Report->SetStringField(TEXT("stage"), Stage);
		Report->SetNumberField(TEXT("viewport_width"), Width);
		Report->SetNumberField(TEXT("viewport_height"), 1440);
		Report->SetStringField(TEXT("root_name"), Panel->GetRootWidget()->GetName());
		Report->SetStringField(TEXT("root_class"), Panel->GetRootWidget()->GetClass()->GetName());
		Report->SetNumberField(TEXT("root_width"), RootSize.X);
		Report->SetNumberField(TEXT("root_height"), RootSize.Y);
		Report->SetStringField(TEXT("limitations"), TEXT("Authored layout with shared chrome property synchronization; no gameplay lifecycle, PreConstruct, activation or data fixtures. FWidgetRenderer material brushes may differ from in-game rendering."));
		TArray<TSharedPtr<FJsonValue>> Rows;
		for (UWidget* Widget : PreviewWidgets)
		{
			if (Widget == Panel.Get()) { continue; }
			const FGeometry& Geometry = Widget->GetCachedGeometry();
			const FVector2D Position = RootGeometry.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
			const FVector2D Extent = Geometry.GetLocalSize();
			const bool bGeometryValid = FMath::IsFinite(Position.X) && FMath::IsFinite(Position.Y)
				&& FMath::IsFinite(Extent.X) && FMath::IsFinite(Extent.Y) && Extent.X > 0.0 && Extent.Y > 0.0;
			const bool bCollapsed = IsEffectivelyCollapsed(Widget, Panel.Get());
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("key"), Widget->GetPathName(Panel.Get()));
			Row->SetStringField(TEXT("name"), Widget->GetName());
			Row->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
			Row->SetStringField(TEXT("visibility"), StaticEnum<ESlateVisibility>()->GetNameStringByValue(static_cast<int64>(Widget->GetVisibility())));
			Row->SetNumberField(TEXT("clipping"), static_cast<int32>(Widget->GetClipping()));
			Row->SetBoolField(TEXT("enabled"), Widget->GetIsEnabled());
			Row->SetBoolField(TEXT("geometry_valid"), bGeometryValid);
			Row->SetBoolField(TEXT("effectively_collapsed"), bCollapsed);
			Row->SetBoolField(TEXT("compare_geometry"), bGeometryValid && !bCollapsed);
			Row->SetNumberField(TEXT("x"), FMath::IsFinite(Position.X) ? Position.X : 0.0);
			Row->SetNumberField(TEXT("y"), FMath::IsFinite(Position.Y) ? Position.Y : 0.0);
			Row->SetNumberField(TEXT("width"), FMath::IsFinite(Extent.X) ? Extent.X : 0.0);
			Row->SetNumberField(TEXT("height"), FMath::IsFinite(Extent.Y) ? Extent.Y : 0.0);
			Rows.Add(MakeShared<FJsonValueObject>(Row));
		}
		Report->SetArrayField(TEXT("widgets"), Rows);
		FImage CapturedImage;
		TArray64<uint8> PngBytes;
		if (!FImageUtils::GetRenderTargetImage(Target.Get(), CapturedImage) || CapturedImage.Format != ERawImageFormat::BGRA8)
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] Render readback failed: %s"), *OutputBase);
			return false;
		}
		CapturedImage.GammaSpace = EGammaSpace::sRGB;
		if (!FImageUtils::CompressImage(PngBytes, TEXT("png"), CapturedImage)
			|| !FFileHelper::SaveArrayToFile(PngBytes, *PngPath))
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] PNG write failed: %s"), *PngPath);
			return false;
		}
		if (PreviewErrors.ErrorCount != 0)
		{
			IFileManager::Get().Delete(*PngPath, false, true);
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] %s: capture emitted %d errors; no successful report written."), *Blueprint->GetName(), PreviewErrors.ErrorCount);
			return false;
		}
		Report->SetNumberField(TEXT("error_count"), PreviewErrors.ErrorCount);
		Report->SetBoolField(TEXT("success"), true);
		FString Json;
		if (!FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json))
			|| !FFileHelper::SaveStringToFile(Json, *JsonPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			IFileManager::Get().Delete(*JsonPath, false, true);
			IFileManager::Get().Delete(*PngPath, false, true);
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] JSON write failed: %s"), *JsonPath);
			return false;
		}
		UE_LOG(LogTemp, Display, TEXT("[PanelLayout] Capture SUCCESS %s stage=%s size=%dx1440 widgets=%d output=%s"),
			*Blueprint->GetName(), *Stage, Width, Rows.Num(), *OutputBase);
		return true;
	}

	static void Capture(const TArray<FString>& Arguments)
	{
		if (!CheckEditor()) { return; }
		if (Arguments.Num() != 2 || !IsSimpleName(Arguments[1]))
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] Usage: CGR.PanelLayout.Capture <asset path> <filename-safe stage label>"));
			return;
		}
		UWidgetBlueprint* Blueprint = LoadPanel(Arguments[0]);
		if (!Blueprint) { return; }
		static const TSet<FName> SupportedPanels{
			TEXT("UI_HelpTablePanel"), TEXT("UI_RewardReveal"), TEXT("UI_AttendancePanel"), TEXT("UI_BusinessTripPanel"),
			TEXT("UI_MailboxPanel"), TEXT("UI_MenuPanel"), TEXT("UI_NewsPanel"), TEXT("UI_QuestPanel"),
			TEXT("UI_SettingsPanel"), TEXT("UI_TrainingCenterPanel"), TEXT("UI_EmployeeGachaPresentation"),
			TEXT("UI_PotentialOdds"),
			TEXT("UI_HQManagePanel"), TEXT("UI_PitchBoardPanel"), TEXT("UI_BuildModal"), TEXT("UI_CodexPanel"),
			TEXT("UI_ProfileImagePanel"), TEXT("UI_ProductSellModal"), TEXT("UI_ProductionStartPopup"), TEXT("UIE_GachaPanelFrame"),
			TEXT("UI_BuildingManagePanel"), TEXT("UI_CityCompanyManage"), TEXT("UI_FactoryPanel"),
			TEXT("UI_EmployeeEquipmentPanel"), TEXT("UI_CountryDetail"), TEXT("UI_WorkstationInfo"), TEXT("UI_ShopPanel"),
			TEXT("UI_EmployeeWindow"), TEXT("UI_RankingPanel"), TEXT("UI_CountryRouterPanel"), TEXT("UI_WorldMapPanel"),
			TEXT("UI_FusionProjectPanel"), TEXT("UIE_PanelHeader"), TEXT("UIE_ModalFrame"),
			// 빌드 카드는 TileView 엔트리라 UI_BuildModal 프리뷰에는 안 나온다(저작 프리뷰에 데이터 픽스처가 없음).
			// 카드 자체를 찍어야 정보 행이 카드 높이를 넘기는지 실측된다 — 행 수를 늘릴 때마다 필요하다.
			TEXT("UIE_BuildEntityCard"), TEXT("UIE_BuildCardInfo"), TEXT("UI_CityOpeningOverlay")};
		UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
		if (!SupportedPanels.Contains(Blueprint->GetFName()) || !EditorWorld || EditorWorld->WorldType != EWorldType::Editor
			|| EditorWorld->GetGameInstance() || !FSlateApplication::IsInitialized()
			|| !Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(UUserWidget::StaticClass())
			|| Blueprint->Status != BS_UpToDate)
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] %s: unsupported panel, unavailable editor-only world, or Blueprint not successfully compiled."), *Blueprint->GetName());
			return;
		}
		for (int32 Width : {2304, 3120})
		{
			if (!CaptureWidth(Blueprint, EditorWorld, Arguments[1], Width))
			{
				UE_LOG(LogTemp, Error, TEXT("[PanelLayout] Capture FAILED %s stage=%s width=%d."), *Blueprint->GetName(), *Arguments[1], Width);
				return;
			}
		}
	}

	static void Validate(const TArray<FString>& Arguments)
	{
		if (Arguments.Num() != 1)
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] Usage: CGR.PanelLayout.Validate <asset path>"));
			return;
		}
		const FString PackageName = FPackageName::ObjectPathToPackageName(Arguments[0]);
		if (!PackageName.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(PackageName))
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] Invalid validation package path: %s"), *Arguments[0]);
			return;
		}
		const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("PanelLayoutOverlay/compile"));
		const FString JsonPath = Directory / (FPackageName::GetLongPackageAssetName(PackageName) + TEXT(".json"));
		// 뒤의 에디터/로드 가드에서 중단돼도 예전 성공 결과가 남지 않는다.
		if (!IFileManager::Get().MakeDirectory(*Directory, true) || !IFileManager::Get().Delete(*JsonPath, false, true)
			|| !FFileHelper::SaveStringToFile(TEXT("{\"success\":false,\"status\":\"validation_not_completed\"}"), *JsonPath,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] Cannot prepare validation output: %s"), *JsonPath);
			return;
		}
		if (!CheckEditor()) { return; }
		UWidgetBlueprint* Blueprint = LoadPanel(Arguments[0]);
		if (!Blueprint) { return; }
		const UWidgetBlueprintGeneratedClass* CompiledClass = Cast<UWidgetBlueprintGeneratedClass>(Blueprint->GeneratedClass);
		const UWidgetTree* CompiledTree = CompiledClass ? CompiledClass->GetWidgetTreeArchetype() : nullptr;
		const UWidget* AuthoredRoot = Blueprint->WidgetTree->RootWidget;
		const UWidget* CompiledRoot = CompiledTree ? CompiledTree->RootWidget.Get() : nullptr;
		const bool bRootMatches = AuthoredRoot && CompiledRoot && AuthoredRoot->GetFName() == CompiledRoot->GetFName()
			&& AuthoredRoot->GetClass() == CompiledRoot->GetClass();
		const bool bCleanCompile = Blueprint->Status == BS_UpToDate;
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetStringField(TEXT("asset"), Blueprint->GetPathName());
		Report->SetStringField(TEXT("status"), StaticEnum<EBlueprintStatus>()->GetNameStringByValue(static_cast<int64>(Blueprint->Status.GetValue())));
		Report->SetBoolField(TEXT("clean_compile"), bCleanCompile);
		Report->SetBoolField(TEXT("root_matches"), bRootMatches);
		Report->SetStringField(TEXT("authored_root_name"), GetNameSafe(AuthoredRoot));
		Report->SetStringField(TEXT("authored_root_class"), AuthoredRoot ? AuthoredRoot->GetClass()->GetName() : TEXT("None"));
		Report->SetStringField(TEXT("compiled_root_name"), GetNameSafe(CompiledRoot));
		Report->SetStringField(TEXT("compiled_root_class"), CompiledRoot ? CompiledRoot->GetClass()->GetName() : TEXT("None"));
		Report->SetBoolField(TEXT("success"), bCleanCompile && bRootMatches);
		FString Json;
		if (!FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json))
			|| !FFileHelper::SaveStringToFile(Json, *JsonPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			IFileManager::Get().Delete(*JsonPath, false, true);
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] Validation report write failed: %s"), *JsonPath);
			return;
		}
		if (bCleanCompile && bRootMatches)
		{
			UE_LOG(LogTemp, Display, TEXT("[PanelLayout] Validate SUCCESS %s; clean compiled root matches authored root."), *Blueprint->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[PanelLayout] Validate FAILED %s; clean_compile=%d root_matches=%d."), *Blueprint->GetName(), bCleanCompile, bRootMatches);
		}
	}

	static FAutoConsoleCommand ReplaceRootCommand(TEXT("CGR.PanelLayout.ReplaceRoot"),
		TEXT("Replace an empty authored Canvas root with an existing detached Overlay. Does not save or compile the Blueprint."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ReplaceRoot));
	static FAutoConsoleCommand CaptureCommand(TEXT("CGR.PanelLayout.Capture"),
		TEXT("Capture an authored editor preview at 2304/3120 x 1440, with PNG and root-local geometry JSON. Does not activate gameplay."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Capture));
	static FAutoConsoleCommand ValidateCommand(TEXT("CGR.PanelLayout.Validate"),
		TEXT("Report clean Blueprint compile status and compiled/authored root agreement. Does not compile or save."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Validate));
}

#endif
