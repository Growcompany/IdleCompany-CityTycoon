#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "AssetCompilingManager.h"
#include "Entity/Officeworker/FunnyOfficeworker.h"
#include "Manager/EmployeeManager.h"
#include "Manager/SaveLoadManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Utils/EmployeeEquipmentVisuals.h"
#include "Data/EmployeeTypes.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"

namespace EmployeeEquipmentFXValidation
{
    struct FPreviewWorld
    {
        UWorld* Scene = nullptr;
        explicit FPreviewWorld(UGameInstance* Instance)
        {
            UWorld::InitializationValues Values;
            Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            Scene = UWorld::CreateWorld(EWorldType::EditorPreview, false, TEXT("EquipmentFXPreview"), nullptr, true, GMaxRHIFeatureLevel, &Values);
            if (Scene)
            {
                GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(Scene);
                Scene->SetGameInstance(Instance);
            }
        }
        ~FPreviewWorld()
        {
            if (!Scene) return;
            Scene->SetGameInstance(nullptr);
            GEngine->DestroyWorldContext(Scene);
            Scene->DestroyWorld(false);
        }
    };

    USkeletalMeshComponent* Part(AFunnyOfficeworker* Worker, const TCHAR* Name)
    {
        TInlineComponentArray<USkeletalMeshComponent*> Components;
        Worker->GetComponents(Components);
        for (USkeletalMeshComponent* Component : Components) if (Component->GetFName() == FName(Name)) return Component;
        return nullptr;
    }

    void Run()
    {
        auto Report = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> Checks;
        bool bPassed = true;
        auto Check = [&](bool Condition, const FString& Message)
        {
            auto Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("check"), Message);
            Entry->SetBoolField(TEXT("passed"), Condition);
            Checks.Add(MakeShared<FJsonValueObject>(Entry));
            bPassed &= Condition;
            return Condition;
        };
        const FString OutputDirectory = FPaths::ProjectSavedDir() / TEXT("EmployeeEquipmentFX");
        IFileManager::Get().MakeDirectory(*OutputDirectory, true);
        UWorld* PlayWorld = nullptr;
        int32 WorldCount = 0;
        if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::PIE) { PlayWorld = Context.World(); ++WorldCount; }
        UGameInstance* Instance = PlayWorld ? PlayWorld->GetGameInstance() : nullptr;
        auto* Employees = Instance ? Instance->GetSubsystem<UEmployeeManager>() : nullptr;
        auto* Tables = Instance ? Instance->GetSubsystem<UTableManagerSubsystem>() : nullptr;
        auto* Save = Instance ? Instance->GetSubsystem<USaveLoadManager>() : nullptr;
        FGameSaveData Before;
        TArray<uint8> DiskBefore;
        const bool bHadDisk = UGameplayStatics::DoesSaveGameExist(TEXT("GameSlot"), 0);
        const FDateTime BeforeCaptureStart = FDateTime::UtcNow();
        const bool bReady = WorldCount == 1 && Employees && Tables && Save && !Employees->GetAllEmployees().IsEmpty()
            && Save->GetCurrentSaveData() && Save->BuildCurrentSnapshot(Before) && (!bHadDisk || UGameplayStatics::LoadDataFromSlot(DiskBefore, TEXT("GameSlot"), 0));
        const FDateTime BeforeCaptureEnd = FDateTime::UtcNow();
        if (Check(bReady, TEXT("One loaded PIE instance and an existing employee; snapshots captured read-only")))
        {
            const FGameSaveData CachedBefore = Save->GetCurrentSaveData()->GameData;
            const FEmployeeInstance Employee = Employees->GetAllEmployees()[0];
            Report->SetNumberField(TEXT("employee_id"), Employee.EmployeeID);
            auto Validate = [&]() -> bool
            {
                FPreviewWorld Preview(Instance);
                if (!Check(Preview.Scene != nullptr, TEXT("Isolated preview world created"))) return false;
                const FTransform Transform = FTransform::Identity;
                AFunnyOfficeworker* Worker = Preview.Scene->SpawnActorDeferred<AFunnyOfficeworker>(AFunnyOfficeworker::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
                if (!Check(Worker != nullptr, TEXT("Separate preview worker created"))) return false;
                Worker->bEditorPreview = false;
                Worker->AutoPossessAI = EAutoPossessAI::Disabled;
                Worker->FinishSpawning(Transform);
                const EEmployeeRank Rank = UEmployeeTypeHelper::GetRankFromEnhancementLevel(Employee.EnhancementLevel);
                const EGachaTier Tier = AFunnyOfficeworker::ResolveGachaTier(Employee);
                Worker->ApplyWorkerCosmetics(Employee.Department, Rank, Tier, Employee.EmployeeID, Employee.Gender);
                auto* Hat = Part(Worker, TEXT("Headwear"));
                auto* Hair = Part(Worker, TEXT("CosmeticHair"));
                auto* BodyMID = Cast<UMaterialInstanceDynamic>(Worker->GetMesh()->GetMaterial(0));
                if (!Check(Hat && Hair && BodyMID, TEXT("Real Funny components and base MID available"))) return false;
                UMaterialInterface* Base = BodyMID->Parent;
                UMaterialInterface* HairMaterial = Hair->GetMaterial(0);
                const FLinearColor Skin = BodyMID->K2_GetVectorParameterValue(TEXT("TintColor"));
                const float BodyMetal = BodyMID->K2_GetScalarParameterValue(TEXT("Metallic"));
                const float BodyEmissive = BodyMID->K2_GetScalarParameterValue(TEXT("Emissive"));
                bool bBaseFX = true; FGuid SwitchGuid;
                Check(Base->GetStaticSwitchParameterValue(FMaterialParameterInfo(TEXT("EquipmentFXEnabled")), bBaseFX, SwitchGuid) && !bBaseFX, TEXT("Default Toon static FX branch disabled"));

                AActor* CameraActor = Preview.Scene->SpawnActor<AActor>();
                auto* Capture = NewObject<USceneCaptureComponent2D>(CameraActor);
                CameraActor->SetRootComponent(Capture); Capture->RegisterComponent();
                auto* Target = NewObject<UTextureRenderTarget2D>(CameraActor);
                Target->ClearColor = FLinearColor(0, 0, 0, 1);
                Target->InitCustomFormat(512, 512, PF_FloatRGBA, true); Target->UpdateResourceImmediate(true);
                Capture->TextureTarget = Target;
                Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
                Capture->ProjectionType = ECameraProjectionMode::Orthographic;
                Capture->bAutoCalculateOrthoPlanes = true;
                Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false;
                Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
                Capture->ShowOnlyComponent(Hat);
                Capture->ShowFlags.SetAtmosphere(false); Capture->ShowFlags.SetFog(false);
                Capture->ShowFlags.SetVolumetricFog(false); Capture->ShowFlags.SetMotionBlur(false);
                Capture->ShowFlags.SetTemporalAA(false);
                USkeletalMesh* ReferenceHat = nullptr;
                auto CaptureFrame = [&](const FString& Name, double Time, TArray<FColor>& Pixels) -> bool
                {
                    // Time uniform만 바뀌는 고립된 촬영 월드. 실제 PIE 시간/틱/경제를 진행시키지 않는다.
                    Preview.Scene->TimeSeconds = Time; Preview.Scene->RealTimeSeconds = Time; Preview.Scene->UnpausedTimeSeconds = Time;
                    FAssetCompilingManager::Get().FinishAllCompilation();
                    Preview.Scene->SendAllEndOfFrameUpdates(); Capture->CaptureScene();
                    TArray<FLinearColor> Linear;
                    if (!Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Linear) || Linear.Num() != 512 * 512) return false;
                    Pixels.SetNumUninitialized(Linear.Num()); int32 Visible = 0; int32 Colored = 0;
                    for (int32 Index = 0; Index < Linear.Num(); ++Index)
                    {
                        Pixels[Index] = Linear[Index].ToFColor(true);
                        Pixels[Index].A = uint8(FMath::Clamp(FMath::RoundToInt((1.f - Linear[Index].A) * 255.f), 0, 255));
                        Visible += Pixels[Index].A > 16 ? 1 : 0;
                        Colored += Pixels[Index].A > 16 && (Pixels[Index].R > 4 || Pixels[Index].G > 4 || Pixels[Index].B > 4) ? 1 : 0;
                    }
                    if (Visible < 100 || Colored < 100) return false;
                    auto Image = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper")).CreateImageWrapper(EImageFormat::PNG);
                    return Image->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), 512, 512, ERGBFormat::BGRA, 8)
                        && FFileHelper::SaveArrayToFile(Image->GetCompressed(), *(OutputDirectory / (Name + TEXT(".png"))));
                };
                auto Difference = [](const TArray<FColor>& Left, const TArray<FColor>& Right)
                {
                    int32 Changed = 0;
                    for (int32 Index = 0; Index < Left.Num() && Index < Right.Num(); ++Index)
                        Changed += Left[Index].A > 16 && (FMath::Abs(int32(Left[Index].R) - int32(Right[Index].R)) + FMath::Abs(int32(Left[Index].G) - int32(Right[Index].G)) + FMath::Abs(int32(Left[Index].B) - int32(Right[Index].B)) > 6) ? 1 : 0;
                    return Changed;
                };
                TArray<FColor> CommonPixels;
                for (EEmployeeEquipmentGrade Grade : {EEmployeeEquipmentGrade::Common, EEmployeeEquipmentGrade::Legendary, EEmployeeEquipmentGrade::Ascendant})
                {
                    const FEmployeeEquipmentAppearanceRow* Appearance = nullptr;
                    for (const auto& Pair : Tables->GetEmployeeEquipmentAppearances())
                        if (Pair.Value.EquipmentSlot == EEmployeeEquipmentSlot::Head && Pair.Value.VisualGrade == Grade && (!Appearance || Pair.Key.LexicalLess(Appearance->AppearanceId))) Appearance = &Pair.Value;
                    const auto* Style = Tables->FindEmployeeEquipmentGrade(Grade);
                    if (!Check(Appearance && Style && Worker->PreviewEquipmentAppearance(EEmployeeEquipmentSlot::Head, EEquipmentAppearanceMode::Override, Appearance->AppearanceId), TEXT("Runtime appearance preview applies"))) return false;
                    auto* MID = Cast<UMaterialInstanceDynamic>(Hat->GetMaterial(0));
                    UMaterialInterface* ExpectedParent = Style->MeshMaterial.IsNull() ? Base : Style->MeshMaterial.LoadSynchronous();
                    if (!Check(MID && MID->Parent == ExpectedParent && Hat->IsVisible(), TEXT("Visible appearance grade selects its expected MID parent"))) return false;
                    const auto* PartRow = Tables->FindFunnyPartRow(Appearance->PartOverrides[0].PartRowId);
                    Check(PartRow && Hat->GetSkeletalMeshAsset() == PartRow->Mesh.LoadSynchronous(), TEXT("Runtime preview uses catalog mesh"));
                    Check(Style->RainbowStrength == 0.f && Style->SparkleStrength == 0.f && Style->IconIdleStrength == 0.f,
                        TEXT("Grade policy disables rainbow stars and idle icon animation"));
                    bool bEnabled = false;
                    Check(ExpectedParent->GetStaticSwitchParameterValue(FMaterialParameterInfo(TEXT("EquipmentFXEnabled")), bEnabled, SwitchGuid) && bEnabled == (Grade >= EEmployeeEquipmentGrade::Legendary), TEXT("Only high-grade MIC enables shader branch"));
                    if (!ReferenceHat) ReferenceHat = Hat->GetSkeletalMeshAsset();
                    // 같은 메시/틴트로 등급 효과만 A/B 비교한다. 런타임 메시 정합은 위에서 별도 검증한다.
                    Hat->SetSkeletalMeshAsset(ReferenceHat); Hat->SetLeaderPoseComponent(Worker->GetMesh());
                    MID->SetVectorParameterValue(TEXT("TintColor"), FLinearColor(0.17f, 0.20f, 0.29f));
                    const FBox Bounds = ReferenceHat->GetBounds().GetBox().TransformBy(Hat->GetComponentTransform());
                    const FVector Direction = FVector(-1, -1, 0.45).GetSafeNormal();
                    CameraActor->SetActorLocationAndRotation(Bounds.GetCenter() + Direction * 300.f, (-Direction).Rotation());
                    Capture->OrthoWidth = Bounds.GetSize().GetMax() * 1.6f;
                    const FString Label = UEnum::GetValueAsString(Grade).Replace(TEXT("EEmployeeEquipmentGrade::"), TEXT(""));
                    TArray<FColor> First, Second;
                    // 새 MID의 렌더 프록시가 준비된 뒤 두 시점의 유효 RGB를 측정한다.
                    TArray<FColor> Prepared;
                    CaptureFrame(Label + TEXT("_prepare"), 0., Prepared);
                    if (!Check(CaptureFrame(Label + TEXT("_0"), 0., First) && CaptureFrame(Label + TEXT("_16"), 1.6, Second), TEXT("Nonempty GPU captures saved for same hat"))) return false;
                    const int32 Changed = Difference(First, Second);
                    Report->SetNumberField(Label + TEXT("_changed_pixels"), Changed);
                    Check(Changed == 0, TEXT("Every world grade is static across shader Time"));
                    if (Grade == EEmployeeEquipmentGrade::Common)
                    {
                        CommonPixels = First;
                    }
                    else
                    {
                        const int32 RimPixels = Difference(CommonPixels, First);
                        int32 VisiblePixels = 0;
                        int32 AlphaChanges = 0;
                        for (int32 Index = 0; Index < First.Num(); ++Index)
                        {
                            VisiblePixels += CommonPixels[Index].A > 16 ? 1 : 0;
                            AlphaChanges += CommonPixels[Index].A != First[Index].A ? 1 : 0;
                        }
                        Report->SetNumberField(Label + TEXT("_rim_pixels"), RimPixels);
                        Report->SetNumberField(Label + TEXT("_visible_pixels"), VisiblePixels);
                        Check(RimPixels > 20 && RimPixels < VisiblePixels * 0.3f,
                            TEXT("High-grade rim is visible while most original-color pixels remain unchanged"));
                        Check(AlphaChanges == 0, TEXT("High-grade rim preserves mesh silhouette alpha"));
                    }
                    Worker->ClearEquipmentAppearancePreview();
                }
                Check(Worker->GetMesh()->GetMaterial(0) == BodyMID && Hair->GetMaterial(0) == HairMaterial
                    && BodyMID->K2_GetVectorParameterValue(TEXT("TintColor")) == Skin
                    && BodyMID->K2_GetScalarParameterValue(TEXT("Metallic")) == BodyMetal
                    && BodyMID->K2_GetScalarParameterValue(TEXT("Emissive")) == BodyEmissive, TEXT("Skin hair and Premium body parameters preserved"));
                const FEmployeeEquipmentAppearanceRow* Ascendant = nullptr;
                for (const auto& Pair : Tables->GetEmployeeEquipmentAppearances()) if (Pair.Value.EquipmentSlot == EEmployeeEquipmentSlot::Head && Pair.Value.VisualGrade == EEmployeeEquipmentGrade::Ascendant) { Ascendant = &Pair.Value; break; }
                if (!Check(Ascendant != nullptr, TEXT("Ascendant portrait fixture exists"))) return false;
                Hat->SetOverlayMaterial(Base);
                Worker->SetSelected(true);
                UMaterialInterface* Selection = Hat->GetOverlayMaterial();
                Worker->PreviewEquipmentAppearance(EEmployeeEquipmentSlot::Head, EEquipmentAppearanceMode::Override, Ascendant->AppearanceId);
                Check(Hat->GetOverlayMaterial() == Selection, TEXT("Existing overlay preserved while selected during FX preview"));
                Worker->SetSelected(false);
                Worker->ApplyCosmeticsForPortrait(Employee.Department, Rank, Tier, Employee.EmployeeID, Employee.Gender);
                auto* PortraitMID = Cast<UMaterialInstanceDynamic>(Hat->GetMaterial(0));
                Check(PortraitMID != nullptr, TEXT("Portrait uses a valid equipment material"));
                TArray<FColor> PortraitA, PortraitB;
                TArray<FColor> PortraitPrepared;
                CaptureFrame(TEXT("Portrait_prepare"), 0., PortraitPrepared);
                Check(CaptureFrame(TEXT("Portrait_0"), 0., PortraitA) && CaptureFrame(TEXT("Portrait_16"), 1.6, PortraitB) && Difference(PortraitA, PortraitB) == 0, TEXT("Portrait pixels remain stable across shader Time"));
                Worker->PreviewEquipmentAppearance(EEmployeeEquipmentSlot::Head, EEquipmentAppearanceMode::Hidden, NAME_None);
                auto* HiddenMID = Cast<UMaterialInstanceDynamic>(Hat->GetMaterial(0));
                Check(!Hat->IsVisible() && HiddenMID && HiddenMID->Parent == Base && HiddenMID->K2_GetScalarParameterValue(TEXT("EquipmentFXStrength")) == 0.f, TEXT("Hidden high-grade part restores default parent without FX residue"));
                Worker->ClearEquipmentAppearancePreview();
                return bPassed;
            };
            bPassed &= Validate();
            FGameSaveData After; TArray<uint8> DiskAfter;
            const FDateTime AfterCaptureStart = FDateTime::UtcNow();
            const bool bAfterCaptured = Save->BuildCurrentSnapshot(After);
            const FDateTime AfterCaptureEnd = FDateTime::UtcNow();
            Check(bAfterCaptured, TEXT("Final live snapshot collected"));
            if (bAfterCaptured)
            {
                TArray<TSharedPtr<FJsonValue>> ChangedFields;
                for (TFieldIterator<FProperty> Property(FGameSaveData::StaticStruct()); Property; ++Property)
                    if (!Property->Identical_InContainer(&Before, &After, 0)) ChangedFields.Add(MakeShared<FJsonValueString>(Property->GetName()));
                Report->SetArrayField(TEXT("snapshot_changed_fields"), ChangedFields);
                Report->SetStringField(TEXT("snapshot_worldmap_last_save_utc_before"), Before.WorldMap_LastSaveUtc.ToIso8601());
                Report->SetStringField(TEXT("snapshot_worldmap_last_save_utc_after"), After.WorldMap_LastSaveUtc.ToIso8601());
                const bool bOnlyGeneratedTimestamp = ChangedFields.Num() == 1 && ChangedFields[0]->AsString() == TEXT("WorldMap_LastSaveUtc")
                    && Before.WorldMap_LastSaveUtc >= BeforeCaptureStart && Before.WorldMap_LastSaveUtc <= BeforeCaptureEnd
                    && After.WorldMap_LastSaveUtc >= AfterCaptureStart && After.WorldMap_LastSaveUtc <= AfterCaptureEnd;
                Report->SetBoolField(TEXT("only_generated_worldmap_timestamp_differs"), bOnlyGeneratedTimestamp);
                // WorldMapManager::SerializeForSave는 상태와 무관하게 이 출력 필드에 UtcNow를 쓴다.
                // 실제 diff가 그 한 필드이며 두 수집 호출 시각 안에 생성됐을 때만 비교값을 정규화한다.
                FGameSaveData ComparableAfter = After;
                if (bOnlyGeneratedTimestamp) ComparableAfter.WorldMap_LastSaveUtc = Before.WorldMap_LastSaveUtc;
                Check(FGameSaveData::StaticStruct()->CompareScriptStruct(&Before, &ComparableAfter, 0), TEXT("Full live state unchanged apart from verified snapshot-generation timestamp"));
            }
            const USaveGame_GameData* CachedAfter = Save->GetCurrentSaveData();
            Check(CachedAfter && FGameSaveData::StaticStruct()->CompareScriptStruct(&CachedBefore, &CachedAfter->GameData, 0), TEXT("Cached save data unchanged including every timestamp"));
            const bool bDiskIdentical = bHadDisk ? UGameplayStatics::LoadDataFromSlot(DiskAfter, TEXT("GameSlot"), 0) && DiskBefore == DiskAfter : !UGameplayStatics::DoesSaveGameExist(TEXT("GameSlot"), 0);
            Check(bDiskIdentical, TEXT("GameSlot bytes unchanged; no rewards or purchases"));
            Report->SetBoolField(TEXT("no_save"), bDiskIdentical);
        }
        Report->SetBoolField(TEXT("passed"), bPassed); Report->SetArrayField(TEXT("checks"), Checks);
        FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json, *(OutputDirectory / TEXT("3d-validation.json")));
        UE_LOG(LogTemp, Display, TEXT("[EquipmentFXQA] passed=%d report=%s"), bPassed, *(OutputDirectory / TEXT("3d-validation.json")));
    }
    FAutoConsoleCommand ValidateCommand(TEXT("CGR.EquipmentQA.ValidateFX"), TEXT("Read-only isolated employee FX preview and GPU readback; requires one loaded PIE instance."), FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
