#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Table/EmployeeEquipmentTable.h"
#include "Table/FunnyPartPoolTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "AssetCompilingManager.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "Rendering/SkeletalMeshModel.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"

namespace EmployeeEquipmentIconCapture
{
    FString ProbeDefinitions;
    int32 StudioProfile = 1;
    constexpr float StudioShadowSoftness = .32f;
    FAutoConsoleVariableRef ProbeDefinitionsVariable(TEXT("CGR.EquipmentIconCapture.Definitions"), ProbeDefinitions,
        TEXT("Comma-separated definition IDs for an isolated IconProbes capture; all or empty captures the full catalog."));
    FAutoConsoleVariableRef StudioProfileVariable(TEXT("CGR.EquipmentIconCapture.Studio"), StudioProfile,
        TEXT("1: supersampled camera-lit capture. 0: original capture for an isolated comparison."));

    template<typename AssetType>
    AssetType* Load(const TCHAR* Path)
    {
        TSoftObjectPtr<AssetType> Reference{FSoftObjectPath(Path)};
        return Reference.LoadSynchronous();
    }

    bool GetSingleHandBounds(const USkeletalMesh* Mesh, FBox& OutBounds)
    {
        const FSkeletalMeshModel* Model = Mesh ? Mesh->GetImportedModel() : nullptr;
        if (!Model || Model->LODModels.IsEmpty()) return false;
        TArray<FVector> Positions;
        FBox GeometryBounds(ForceInit);
        for (const FSkelMeshSection& Section : Model->LODModels[0].Sections)
        {
            for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
            {
                const FVector Position(Vertex.Position);
                Positions.Add(Position);
                GeometryBounds += Position;
            }
        }
        if (Positions.Num() < 2 || !GeometryBounds.IsValid) return false;
        const FVector Span = GeometryBounds.GetSize();
        int32 SplitAxis = Span.X >= Span.Y ? 0 : 1;
        if (Span.Z > Span[SplitAxis]) SplitAxis = 2;
        Positions.Sort([SplitAxis](const FVector& Left, const FVector& Right) { return Left[SplitAxis] < Right[SplitAxis]; });
        double LargestGap = 0.;
        int32 SplitIndex = INDEX_NONE;
        for (int32 Index = 1; Index < Positions.Num(); ++Index)
        {
            const double Gap = Positions[Index][SplitAxis] - Positions[Index - 1][SplitAxis];
            if (Gap > LargestGap) { LargestGap = Gap; SplitIndex = Index; }
        }
        // T 포즈 양손 사이 빈 구간으로 두 정점군을 분리한다. 상품명이나 특정 본 좌표에 의존하지 않는다.
        if (SplitIndex == INDEX_NONE || LargestGap < Span[SplitAxis] * 0.25f) return false;
        OutBounds = FBox(ForceInit);
        for (int32 Index = 0; Index < SplitIndex; ++Index) OutBounds += Positions[Index];
        return OutBounds.IsValid && OutBounds.GetExtent().GetMax() > KINDA_SMALL_NUMBER;
    }

    struct FScopedPreviewWorld
    {
        UWorld* PreviewWorld = nullptr;
        FScopedPreviewWorld()
        {
            UWorld::InitializationValues Values;
            Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            PreviewWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false, TEXT("EmployeeEquipmentIconPreview"), nullptr, true, GMaxRHIFeatureLevel, &Values);
            if (PreviewWorld && GEngine) GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(PreviewWorld);
        }
        ~FScopedPreviewWorld()
        {
            if (!PreviewWorld) return;
            if (GEngine) GEngine->DestroyWorldContext(PreviewWorld);
            PreviewWorld->DestroyWorld(false);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEmployeeEquipmentIconCapture, "CGR.Tools.EmployeeEquipmentIconCapture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEmployeeEquipmentIconCapture::RunTest(const FString& Parameters)
{
    using namespace EmployeeEquipmentIconCapture;
    FString RequestedDefinitions = ProbeDefinitions;
    int32 RequestedProfile = StudioProfile;
    FParse::Value(*Parameters, TEXT("Definitions="), RequestedDefinitions);
    FParse::Value(*Parameters, TEXT("Studio="), RequestedProfile);
    RequestedDefinitions.TrimStartAndEndInline();
    if (RequestedDefinitions.Equals(TEXT("all"), ESearchCase::IgnoreCase)) RequestedDefinitions.Empty();
    const bool bStudio = RequestedProfile != 0;
    const bool bProbe = !RequestedDefinitions.IsEmpty();
    UDataTable* Definitions = Load<UDataTable>(TEXT("/Game/CompanyGrowth/Table/Employee/DT_EmployeeEquipmentDefinition.DT_EmployeeEquipmentDefinition"));
    UDataTable* Appearances = Load<UDataTable>(TEXT("/Game/CompanyGrowth/Table/Employee/DT_EmployeeEquipmentAppearance.DT_EmployeeEquipmentAppearance"));
    UDataTable* Parts = Load<UDataTable>(TEXT("/Game/CompanyGrowth/Table/Employee/DT_FunnyPartPool.DT_FunnyPartPool"));
    UMaterialInterface* Toon = Load<UMaterialInterface>(TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/M_FunnyToon.M_FunnyToon"));
    if (!Definitions || !Appearances || !Parts || !Toon) { AddError(TEXT("Equipment icon catalog/material is missing")); return false; }
    // Asset compilation waits for meshes/textures, but material shader maps have their own compiler.
    FMaterialResource* ToonResource = Toon->GetMaterialResource(GMaxRHIFeatureLevel);
    if (!ToonResource) { AddError(TEXT("Equipment icon Toon material has no resource for the current feature level")); return false; }
    ToonResource->FinishCompilation();
    const bool bRequestedShaderCompilation = !ToonResource->IsGameThreadShaderMapComplete();
    if (bRequestedShaderCompilation)
    {
        {
            FMaterialUpdateContext UpdateContext;
            UpdateContext.AddMaterial(Toon->GetMaterial());
            Toon->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
        }
        ToonResource = Toon->GetMaterialResource(GMaxRHIFeatureLevel);
        if (!ToonResource) { AddError(TEXT("Equipment icon Toon material resource was not prepared")); return false; }
        ToonResource->FinishCompilation();
    }
    if (!ToonResource->IsGameThreadShaderMapComplete()) { AddError(TEXT("Equipment icon Toon shader map is incomplete")); return false; }
    FlushRenderingCommands();
    TArray<FName> Names = Definitions->GetRowNames();
    if (bProbe)
    {
        TArray<FString> RequestedNames;
        RequestedDefinitions.ParseIntoArray(RequestedNames, TEXT(","), true);
        TArray<FName> SelectedNames;
        for (FString& RequestedName : RequestedNames)
        {
            RequestedName.TrimStartAndEndInline();
            const FName RowName(*RequestedName);
            if (!Names.Contains(RowName)) { AddError(TEXT("Unknown equipment icon definition: ") + RequestedName); return false; }
            SelectedNames.AddUnique(RowName);
        }
        if (SelectedNames.IsEmpty()) { AddError(TEXT("Icon probe requires at least one definition")); return false; }
        Names = MoveTemp(SelectedNames);
    }
    Names.Sort(FNameLexicalLess());
    FScopedPreviewWorld Scene;
    if (!Scene.PreviewWorld) { AddError(TEXT("Cannot create icon preview world")); return false; }
    constexpr int32 Size = 512;
    const int32 RenderSize = bStudio ? 2048 : Size;
    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(Scene.PreviewWorld);
    Target->ClearColor = FLinearColor(0, 0, 0, 1);
    Target->InitCustomFormat(RenderSize, RenderSize, PF_FloatRGBA, true);
    Target->UpdateResourceImmediate(true);
    AActor* CaptureActor = Scene.PreviewWorld->SpawnActor<AActor>();
    USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(CaptureActor);
    CaptureActor->SetRootComponent(Capture);
    Capture->RegisterComponent();
    Capture->TextureTarget = Target;
    Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
    Capture->ProjectionType = ECameraProjectionMode::Orthographic;
    Capture->bAutoCalculateOrthoPlanes = true;
    Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->ShowFlags.SetAtmosphere(false);
    Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetVolumetricFog(false);
    Capture->ShowFlags.SetMotionBlur(false);
    Capture->ShowFlags.SetTemporalAA(false);
    if (bStudio)
    {
        Capture->ShowFlags.SetEyeAdaptation(false);
        Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
        Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
        Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
        Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    }
    const FString ProfileName = bStudio ? TEXT("Studio") : TEXT("Original");
    const FString OutputDir = bProbe ? FPaths::ProjectSavedDir() / TEXT("EmployeeEquipment/IconProbes") / ProfileName
        : FPaths::ProjectSavedDir() / TEXT("EmployeeEquipment/Icons");
    IFileManager::Get().MakeDirectory(*OutputDir, true);
    IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    TArray<TSharedPtr<FJsonValue>> Records;
    int32 Written = 0;
    for (FName DefinitionId : Names)
    {
        const FEmployeeEquipmentDefinitionRow* Definition = Definitions->FindRow<FEmployeeEquipmentDefinitionRow>(DefinitionId, TEXT("IconCapture"));
        const FEmployeeEquipmentAppearanceRow* Appearance = Definition ? Appearances->FindRow<FEmployeeEquipmentAppearanceRow>(Definition->AppearanceId, TEXT("IconCapture")) : nullptr;
        if (!Appearance || Appearance->PartOverrides.IsEmpty()) { AddError(DefinitionId.ToString() + TEXT(": appearance missing")); return false; }
        const float CameraHeight = bStudio ? 0.30f : 0.45f;
        const FVector CameraDirection = (Definition->EquipmentSlot == EEmployeeEquipmentSlot::Back ? FVector(1.f, -1.f, CameraHeight) : FVector(-1.f, -1.f, CameraHeight)).GetSafeNormal();
        const FRotator ViewRotation = (-CameraDirection).Rotation();
        const FVector Right = ViewRotation.RotateVector(FVector::RightVector);
        const FVector Up = ViewRotation.RotateVector(FVector::UpVector);
        const FVector KeyDirection = (CameraDirection * .65f - Right * .45f + Up * .75f).GetSafeNormal();
        AActor* ItemActor = Scene.PreviewWorld->SpawnActor<AActor>();
        USceneComponent* ItemRoot = NewObject<USceneComponent>(ItemActor);
        ItemActor->SetRootComponent(ItemRoot);
        ItemRoot->RegisterComponent();
        FBox ItemBounds(ForceInit);
        for (const FEquipmentPartOverride& Part : Appearance->PartOverrides)
        {
            const FFunnyPartPoolTable* PartRow = Parts->FindRow<FFunnyPartPoolTable>(Part.PartRowId, TEXT("IconCapture"));
            USkeletalMesh* Mesh = PartRow && !PartRow->Mesh.IsNull() ? PartRow->Mesh.LoadSynchronous() : nullptr;
            if (!Mesh) { AddError(DefinitionId.ToString() + TEXT(": part mesh missing")); return false; }
            USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(ItemActor);
            Component->SetupAttachment(ItemRoot);
            Component->SetSkeletalMeshAsset(Mesh);
            Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Component->SetForcedLOD(1);
            Component->RegisterComponent();
            Component->RefreshBoneTransforms();
            Component->UpdateBounds();
            UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Toon, Component);
            Material->SetVectorParameterValue(TEXT("TintColor"), Part.bOverrideTint ? Part.Tint : FLinearColor::White);
            Material->SetScalarParameterValue(TEXT("Metallic"), 0.f);
            Material->SetScalarParameterValue(TEXT("Emissive"), 0.f);
            Material->SetScalarParameterValue(TEXT("TexBlend"), 1.f);
            Material->SetScalarParameterValue(TEXT("TexGain"), 1.f);
            if (bStudio)
            {
                // The shared Toon is unlit; its light vector must follow this product camera.
                Material->SetVectorParameterValue(TEXT("LightDir"), FLinearColor(KeyDirection.X, KeyDirection.Y, KeyDirection.Z, 0.f));
                Material->SetVectorParameterValue(TEXT("ShadowTint"), FLinearColor(.46f, .52f, .62f));
                Material->SetScalarParameterValue(TEXT("ShadowThreshold"), .25f);
                Material->SetScalarParameterValue(TEXT("ShadowSoftness"), StudioShadowSoftness);
                Material->SetScalarParameterValue(TEXT("Brightness"), 1.25f);
                Material->SetScalarParameterValue(TEXT("TexBlend"), .80f);
            }
            for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetMaterials().Num(); ++MaterialIndex) Component->SetMaterial(MaterialIndex, Material);
            if (Definition->EquipmentSlot == EEmployeeEquipmentSlot::Hands)
            {
                FAssetCompilingManager::Get().FinishAllCompilation();
                FBox HandBounds(ForceInit);
                if (!GetSingleHandBounds(Mesh, HandBounds))
                { AddError(DefinitionId.ToString() + TEXT(": cannot isolate one hand from imported LOD0 vertices")); return false; }
                ItemBounds += HandBounds;
            }
            else ItemBounds += Mesh->GetBounds().GetBox();
        }
        FAssetCompilingManager::Get().FinishAllCompilation();
        if (!ItemBounds.IsValid || ItemBounds.GetExtent().GetMax() < KINDA_SMALL_NUMBER) { AddError(TEXT("Invalid icon bounds")); return false; }
        const FVector Center = ItemBounds.GetCenter();
        float HalfWidth = 0.f;
        float HalfHeight = 0.f;
        for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
        {
            const FVector Corner((CornerIndex & 1) ? ItemBounds.Max.X : ItemBounds.Min.X, (CornerIndex & 2) ? ItemBounds.Max.Y : ItemBounds.Min.Y, (CornerIndex & 4) ? ItemBounds.Max.Z : ItemBounds.Min.Z);
            HalfWidth = FMath::Max(HalfWidth, FMath::Abs(FVector::DotProduct(Corner - Center, Right)));
            HalfHeight = FMath::Max(HalfHeight, FMath::Abs(FVector::DotProduct(Corner - Center, Up)));
        }
        Capture->OrthoWidth = FMath::Max(HalfWidth, HalfHeight) * 2.3f;
        CaptureActor->SetActorLocationAndRotation(Center + CameraDirection * ItemBounds.GetExtent().Size() * 4.f, ViewRotation);
        Capture->ClearShowOnlyComponents();
        Capture->ShowOnlyActorComponents(ItemActor);
        Scene.PreviewWorld->SendAllEndOfFrameUpdates();
        Capture->CaptureScene();
        TArray<FLinearColor> LinearPixels;
        if (!Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(LinearPixels) || LinearPixels.Num() != RenderSize * RenderSize)
        { AddError(TEXT("Icon GPU readback failed")); return false; }
        for (FLinearColor& Pixel : LinearPixels)
        {
            const float Coverage = FMath::Clamp(1.f - Pixel.A, 0.f, 1.f);
            Pixel = FLinearColor(Pixel.R * Coverage, Pixel.G * Coverage, Pixel.B * Coverage, Coverage);
        }
        if (RenderSize != Size)
        {
            TArray<FLinearColor> Downsampled;
            Downsampled.SetNumUninitialized(Size * Size);
            FImageCore::ResizeImage(FImageView(LinearPixels.GetData(), RenderSize, RenderSize),
                FImageView(Downsampled.GetData(), Size, Size), FImageCore::EResizeImageFilter::Box);
            LinearPixels = MoveTemp(Downsampled);
        }
        TArray<FColor> Pixels;
        Pixels.SetNumUninitialized(LinearPixels.Num());
        int32 OpaqueCount = 0, PartialAlphaCount = 0, MaximumVisibleRGB = 0;
        bool bHandsTouchMargin = false;
        for (int32 PixelIndex = 0; PixelIndex < LinearPixels.Num(); ++PixelIndex)
        {
            const FLinearColor& Filtered = LinearPixels[PixelIndex];
            // Filter coverage-weighted RGB in linear light, then store straight alpha for UI blending.
            const FLinearColor Straight = Filtered.A > SMALL_NUMBER
                ? FLinearColor(Filtered.R / Filtered.A, Filtered.G / Filtered.A, Filtered.B / Filtered.A, Filtered.A)
                : FLinearColor::Transparent;
            Pixels[PixelIndex] = Straight.ToFColor(true);
            Pixels[PixelIndex].A = uint8(FMath::Clamp(FMath::RoundToInt(Filtered.A * 255.f), 0, 255));
            OpaqueCount += Pixels[PixelIndex].A > 16 ? 1 : 0;
            if (Pixels[PixelIndex].A > 16)
                MaximumVisibleRGB = FMath::Max(MaximumVisibleRGB, int32(FMath::Max3(Pixels[PixelIndex].R, Pixels[PixelIndex].G, Pixels[PixelIndex].B)));
            PartialAlphaCount += Pixels[PixelIndex].A > 0 && Pixels[PixelIndex].A < 255 ? 1 : 0;
            if (Definition->EquipmentSlot == EEmployeeEquipmentSlot::Hands && Pixels[PixelIndex].A > 16)
            {
                const int32 PixelX = PixelIndex % Size;
                const int32 PixelY = PixelIndex / Size;
                bHandsTouchMargin |= PixelX < 16 || PixelX >= Size - 16 || PixelY < 16 || PixelY >= Size - 16;
            }
        }
        if (OpaqueCount < 100 || OpaqueCount > Size * Size * 0.9f) { AddError(DefinitionId.ToString() + TEXT(": empty/opaque alpha capture")); return false; }
        if (MaximumVisibleRGB == 0) { AddError(DefinitionId.ToString() + TEXT(": capture has valid alpha but no visible RGB; output was not saved")); return false; }
        if (bHandsTouchMargin) { AddError(DefinitionId.ToString() + TEXT(": hand icon overlaps the transparent 16px safety margin")); return false; }
        TSharedPtr<IImageWrapper> Png = Images.CreateImageWrapper(EImageFormat::PNG);
        if (!Png->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Size, Size, ERGBFormat::BGRA, 8)) return false;
        const TArray64<uint8>& Compressed = Png->GetCompressed();
        const FString OutputFile = OutputDir / (TEXT("T_") + DefinitionId.ToString() + TEXT(".png"));
        if (!FFileHelper::SaveArrayToFile(Compressed, *OutputFile)) { AddError(TEXT("Icon PNG write failed")); return false; }
        auto Record = MakeShared<FJsonObject>();
        Record->SetStringField(TEXT("definition"), DefinitionId.ToString());
        Record->SetStringField(TEXT("output"), OutputFile);
        Record->SetNumberField(TEXT("visible_pixels"), OpaqueCount);
        Record->SetNumberField(TEXT("partial_alpha_pixels"), PartialAlphaCount);
        Record->SetNumberField(TEXT("maximum_visible_rgb"), MaximumVisibleRGB);
        Record->SetBoolField(TEXT("hand_margin_passed"), !bHandsTouchMargin);
        Records.Add(MakeShared<FJsonValueObject>(Record));
        ++Written;
        UE_LOG(LogTemp, Display, TEXT("[EquipmentIcon] %s pixels=%d output=%s"), *DefinitionId.ToString(), OpaqueCount, *OutputFile);
        ItemActor->Destroy();
    }
    auto Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("profile"), ProfileName);
    Report->SetBoolField(TEXT("isolated_probe"), bProbe);
    if (bStudio) Report->SetNumberField(TEXT("shadow_softness"), StudioShadowSoftness);
    Report->SetNumberField(TEXT("render_size"), RenderSize);
    Report->SetNumberField(TEXT("output_size"), Size);
    Report->SetBoolField(TEXT("requested_shader_compilation"), bRequestedShaderCompilation);
    Report->SetBoolField(TEXT("toon_shader_map_complete"), ToonResource->IsGameThreadShaderMapComplete());
    Report->SetArrayField(TEXT("icons"), Records);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    if (!FFileHelper::SaveStringToFile(Json, *(OutputDir / TEXT("capture-manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    { AddError(TEXT("Icon capture manifest write failed")); return false; }
    const int32 ExpectedCount = Names.Num();
    TestEqual(TEXT("Rendered every requested equipment definition"), Written, ExpectedCount);
    return Written == ExpectedCount;
}
#endif
