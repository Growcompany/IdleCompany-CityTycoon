#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/CGGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/Float16Color.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Office/OfficeDistantBackdrop.h"
#include "Office/OfficeInterior.h"
#include "Office/OfficeNeighborhoodSnapshot.h"
#include "ProceduralMeshComponent.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "TextureResource.h"
#include "TimeCycle/TimeCycleSky.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
// Builds a completed capture without a renderer so the transfer contract can be
// checked independently of the city scene and GPU availability.
struct FOfficeBackdropTransferTestAccess
{
	static UTextureRenderTarget2D* MakeFace()
	{
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
		Target->InitCustomFormat(4, 4, PF_FloatRGBA, true);
		Target->UpdateResourceImmediate(true);
		return Target;
	}

	static void PrimeCompleteCapture(UOfficeDistantBackdrop* Capture)
	{
		for (int32 Face = 0; Face < UOfficeDistantBackdrop::FaceCount; ++Face)
		{
			Capture->RenderTargets.Add(MakeFace());
		}
		Capture->CaptureFrameTransform = FTransform(FVector(0.f, 0.f, 300.f));
	}

	static void PrimePartialCapture(UOfficeDistantBackdrop* Capture)
	{
		Capture->RenderTargets.Add(MakeFace());
	}

	static UProceduralMeshComponent* GetDisplay(UOfficeDistantBackdrop* Capture)
	{
		return Capture->Display;
	}

	static void PrimeFilterPixels(UOfficeDistantBackdrop* Capture, const TArray<FLinearColor>& Colors)
	{
		for (const FLinearColor& Color : Colors)
		{
			UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
			Target->ClearColor = Color;
			Target->AddressX = TA_Clamp;
			Target->AddressY = TA_Clamp;
			Target->Filter = TF_Bilinear;
			Target->InitCustomFormat(32, 32, PF_FloatRGBA, true);
			Target->UpdateResourceImmediate(true);
			Capture->RenderTargets.Add(Target);
		}
	}

	static bool Soften(UOfficeDistantBackdrop* Capture, AActor* SourceOwner)
	{
		return Capture->SoftenSourceCapture(SourceOwner);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeBackdropTransferTest,
	"CGR.Office.Neighborhood.BackdropTransfer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOfficeBackdropTransferTest::RunTest(const FString& Parameters)
{
	UWorld* SourceWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	UWorld* OfficeWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("source world"), SourceWorld)
		|| !TestNotNull(TEXT("office world"), OfficeWorld)) { return false; }
	SourceWorld->AddToRoot();
	OfficeWorld->AddToRoot();
	ON_SCOPE_EXIT
	{
		if (GEngine)
		{
			GEngine->DestroyWorldContext(SourceWorld);
			GEngine->DestroyWorldContext(OfficeWorld);
		}
		SourceWorld->RemoveFromRoot();
		OfficeWorld->RemoveFromRoot();
		SourceWorld->DestroyWorld(false);
		OfficeWorld->DestroyWorld(false);
	};
	if (GEngine)
	{
		GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(SourceWorld);
		GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(OfficeWorld);
	}
	AActor* SourceOwner = SourceWorld->SpawnActor<AActor>();
	AActor* DisplayOwner = OfficeWorld->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("source owner"), SourceOwner)
		|| !TestNotNull(TEXT("photo owner"), DisplayOwner)) { return false; }
	USceneComponent* Root = NewObject<USceneComponent>(DisplayOwner);
	DisplayOwner->SetRootComponent(Root);
	Root->RegisterComponent();
	DisplayOwner->SetActorRotation(FRotator(0.f, 90.f, 0.f));
	UMaterialInterface* Material = UMaterial::GetDefaultMaterial(MD_Surface);
	if (!TestNotNull(TEXT("display material"), Material)) { return false; }
	TStrongObjectPtr<UOfficeDistantBackdrop> SourceCapture(NewObject<UOfficeDistantBackdrop>(GetTransientPackage()));
	TStrongObjectPtr<UOfficeDistantBackdrop> OfficeCapture(NewObject<UOfficeDistantBackdrop>(DisplayOwner));
	const FVector ViewOrigin(320.f, -150.f, 1800.f);
	const FTransform DestinationFrame(DisplayOwner->GetActorQuat(), ViewOrigin);
	TestFalse(TEXT("empty capture cannot be adopted"), OfficeCapture->ApplyCapture(DisplayOwner, Material, SourceCapture.Get(), DestinationFrame, 0.35f));
	TestFalse(TEXT("empty capture is not reusable"), SourceCapture->HasCapture());
	TestFalse(TEXT("empty source list cannot start capture"), SourceCapture->CaptureSource(
		SourceOwner, nullptr, {}, FTransform::Identity));
	FOfficeBackdropTransferTestAccess::PrimePartialCapture(SourceCapture.Get());
	TestFalse(TEXT("partial capture cannot be adopted"), OfficeCapture->ApplyCapture(DisplayOwner, Material, SourceCapture.Get(), DestinationFrame, 0.35f));
	TestFalse(TEXT("partial capture is not reusable"), SourceCapture->HasCapture());
	SourceCapture->Reset();
	FOfficeBackdropTransferTestAccess::PrimeCompleteCapture(SourceCapture.Get());
	TestTrue(TEXT("complete capture is reusable"), SourceCapture->HasCapture());
	TestFalse(TEXT("capture cannot adopt without display owner"), OfficeCapture->ApplyCapture(nullptr, Material, SourceCapture.Get(), DestinationFrame, 0.35f));
	TestFalse(TEXT("capture cannot adopt without display material"), OfficeCapture->ApplyCapture(DisplayOwner, nullptr, SourceCapture.Get(), DestinationFrame, 0.35f));
	TestTrue(TEXT("complete source capture transfers into office world"), OfficeCapture->ApplyCapture(DisplayOwner, Material, SourceCapture.Get(), DestinationFrame, 0.35f));
	TestTrue(TEXT("adopted display is active"), OfficeCapture->IsActive());
	UProceduralMeshComponent* Display = FOfficeBackdropTransferTestAccess::GetDisplay(OfficeCapture.Get());
	if (!TestNotNull(TEXT("office display"), Display)) { return false; }
	TestTrue(TEXT("display belongs to office world"), Display->GetWorld() == OfficeWorld);
	TestEqual(TEXT("three display faces"), Display->GetNumSections(), UOfficeDistantBackdrop::FaceCount);
	TestTrue(TEXT("display follows photo owner rotation"), Display->GetComponentQuat().Equals(DisplayOwner->GetActorQuat()));
	const TArray<UTextureRenderTarget2D*> InitialTargets = OfficeCapture->GetRenderTargets();
	const FTransform MovedFrame(DisplayOwner->GetActorQuat(), ViewOrigin + FVector(700.f, -400.f, 500.f));
	TestTrue(TEXT("same capture reapplies at another frame"), OfficeCapture->ApplyCapture(
		DisplayOwner, Material, OfficeCapture.Get(), MovedFrame, 0.28f));
	TestTrue(TEXT("reapply keeps display component"), FOfficeBackdropTransferTestAccess::GetDisplay(OfficeCapture.Get()) == Display);
	TestTrue(TEXT("reapply keeps render targets"), OfficeCapture->GetRenderTargets() == InitialTargets);
	TestTrue(TEXT("reapply updates fixed photo frame"), Display->GetComponentTransform().Equals(MovedFrame));
	for (int32 Face = 0; Face < UOfficeDistantBackdrop::FaceCount; ++Face)
	{
		const FProcMeshSection* FaceSection = Display->GetProcMeshSection(Face);
		if (!TestNotNull(TEXT("flat photo face"), FaceSection)) { return false; }
		TestEqual(TEXT("flat photo face has four vertices"), FaceSection->ProcVertexBuffer.Num(), 4);
	}
	TArray<UTextureRenderTarget2D*> ExpectedDay = SourceCapture->GetRenderTargets();
	for (int32 Face = 0; Face < UOfficeDistantBackdrop::FaceCount; ++Face)
	{
		UMaterialInstanceDynamic* FaceMaterial = Cast<UMaterialInstanceDynamic>(Display->GetMaterial(Face));
		if (!TestNotNull(TEXT("face material"), FaceMaterial)) { return false; }
		UTexture* BoundDay = nullptr;
		FaceMaterial->GetTextureParameterValue(FMaterialParameterInfo(TEXT("DayCapture")), BoundDay);
		TestTrue(TEXT("source day face is bound to display"), BoundDay == ExpectedDay[Face]);
	}
	OfficeCapture->Reset();
	TestFalse(TEXT("reset removes office display"), OfficeCapture->IsActive());
	TestEqual(TEXT("reset releases adopted targets"), OfficeCapture->GetRenderTargets().Num(), 0);
	TestTrue(TEXT("office reset preserves source capture"), SourceCapture->HasCapture());
	TestTrue(TEXT("source capture can be adopted again"), OfficeCapture->ApplyCapture(DisplayOwner, Material, SourceCapture.Get(), DestinationFrame, 0.35f));
	TWeakObjectPtr<UOfficeDistantBackdrop> WeakSourceCapture(SourceCapture.Get());
	SourceCapture.Reset();
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("source capture object was collected"), WeakSourceCapture.IsValid());
	TestEqual(TEXT("adopted day faces survive source capture collection"), OfficeCapture->GetRenderTargets().Num(), UOfficeDistantBackdrop::FaceCount);
	for (int32 Face = 0; Face < UOfficeDistantBackdrop::FaceCount; ++Face)
	{
		TestTrue(TEXT("day face identity retained"), OfficeCapture->GetRenderTargets()[Face] == ExpectedDay[Face]);
	}
	OfficeCapture->Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeSourceFilterPixelsTest,
	"CGR.Office.Neighborhood.SourceFilterPixels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)
bool FOfficeSourceFilterPixelsTest::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI)
	{
		AddError(TEXT("SourceFilterPixels requires a graphics RHI to verify HDR and inverse-opacity pixels"));
		return false;
	}
	UWorld* FilterWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("filter world"), FilterWorld)) { return false; }
	if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(FilterWorld); }
	ON_SCOPE_EXIT
	{
		if (GEngine) { GEngine->DestroyWorldContext(FilterWorld); }
		FilterWorld->DestroyWorld(false);
	};
	AActor* FilterOwner = FilterWorld->SpawnActor<AActor>();
	TStrongObjectPtr<UOfficeDistantBackdrop> Capture(NewObject<UOfficeDistantBackdrop>(GetTransientPackage()));
	const TArray<FLinearColor> ExpectedColors = {
		FLinearColor(4.f, 0.5f, 0.25f, 0.25f), FLinearColor(0.f, 0.f, 0.f, 1.f), FLinearColor(0.1f, 0.3f, 0.7f, 0.f)};
	FOfficeBackdropTransferTestAccess::PrimeFilterPixels(Capture.Get(), ExpectedColors);
	if (!TestTrue(TEXT("source filter renders"), FOfficeBackdropTransferTestAccess::Soften(Capture.Get(), FilterOwner))) { return false; }
	FlushRenderingCommands();
	const TArray<UTextureRenderTarget2D*> Filtered = Capture->GetRenderTargets();
	for (int32 FaceIndex = 0; FaceIndex < Filtered.Num(); ++FaceIndex)
	{
		FTextureRenderTargetResource* Resource = Filtered[FaceIndex]->GameThread_GetRenderTargetResource();
		TArray<FFloat16Color> Pixels;
		if (!TestNotNull(TEXT("filtered resource"), Resource)
			|| !TestTrue(TEXT("filtered pixel readback"), Resource->ReadFloat16Pixels(Pixels))) { return false; }
		if (!TestTrue(TEXT("filtered pixels exist"), !Pixels.IsEmpty())) { return false; }
		const FLinearColor Actual = Pixels[Pixels.Num() / 2].GetFloats();
		TestTrue(FString::Printf(TEXT("face %d preserves HDR color and inverse opacity: %s"), FaceIndex, *Actual.ToString()),
			Actual.Equals(ExpectedColors[FaceIndex], 0.01f));
		TestTrue(TEXT("filter never stores more pixels than the source city capture"),
			Filtered[FaceIndex]->SizeX <= UOfficeDistantBackdrop::GetSourceFaceResolution());
		TestEqual(TEXT("filter stores at the backdrop display resolution"),
			Filtered[FaceIndex]->SizeX, UOfficeDistantBackdrop::SourceDisplayResolution);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeCityPhotoIncludesObjectsTest,
	"CGR.Office.Neighborhood.CityPhotoIncludesObjects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter | EAutomationTestFlags::NonNullRHI)
bool FOfficeCityPhotoIncludesObjectsTest::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI) { AddError(TEXT("City photo needs a graphics RHI")); return false; }
	UWorld* PhotoWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("photo world"), PhotoWorld)) { return false; }
	if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(PhotoWorld); }
	ON_SCOPE_EXIT
	{
		if (GEngine) { GEngine->DestroyWorldContext(PhotoWorld); }
		PhotoWorld->DestroyWorld(false);
	};
	ATimeCycleSky* Sky = PhotoWorld->SpawnActor<ATimeCycleSky>();
	AActor* PhotoOwner = PhotoWorld->SpawnActor<AActor>();
	UStaticMeshComponent* ObjectMesh = NewObject<UStaticMeshComponent>(PhotoOwner);
	PhotoOwner->SetRootComponent(ObjectMesh);
	ObjectMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	ObjectMesh->SetWorldScale3D(FVector(5.f));
	ObjectMesh->RegisterComponent();
	PhotoWorld->SendAllEndOfFrameUpdates();
	FlushRenderingCommands();
	TStrongObjectPtr<UOfficeDistantBackdrop> Photo(NewObject<UOfficeDistantBackdrop>());
	// 아래쪽 시야의 독립 물체도 지면 전용 목록 없이 사진에 남아야 한다.
	if (!TestTrue(TEXT("city photo includes downward objects"), Photo->CaptureSource(
		PhotoOwner, Sky, {ObjectMesh}, FTransform(FVector(0.f, 0.f, 1000.f))))) { return false; }
	TArray<FFloat16Color> Pixels;
	UTextureRenderTarget2D* Bottom = Photo->GetRenderTargets()[2];
	if (!TestTrue(TEXT("read downward photo"), Bottom->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(Pixels))) { return false; }
	const FLinearColor Center = Pixels[(Bottom->SizeY / 2) * Bottom->SizeX + Bottom->SizeX / 2].GetFloats();
	TestTrue(TEXT("non-ground object survives in downward photo alpha"), Center.A < 0.1f);
	AActor* DisplayOwner = PhotoWorld->SpawnActor<AActor>();
	USceneComponent* PhotoRoot = NewObject<USceneComponent>(DisplayOwner);
	DisplayOwner->SetRootComponent(PhotoRoot);
	PhotoRoot->RegisterComponent();
	TStrongObjectPtr<UOfficeDistantBackdrop> DisplayPhoto(NewObject<UOfficeDistantBackdrop>(DisplayOwner));
	TestTrue(TEXT("full city photo transfers without reconstructed ground"), DisplayPhoto->ApplyCapture(
		DisplayOwner, UMaterial::GetDefaultMaterial(MD_Surface), Photo.Get(), FTransform::Identity, 0.35f));
	UProceduralMeshComponent* Display = FOfficeBackdropTransferTestAccess::GetDisplay(DisplayPhoto.Get());
	if (!TestNotNull(TEXT("photo display"), Display)) { return false; }
	const FProcMeshSection* BottomSection = Display->GetProcMeshSection(2);
	if (!TestNotNull(TEXT("bottom photo surface"), BottomSection)) { return false; }
	TestEqual(TEXT("photo stays a flat image, not reconstructed object geometry"), BottomSection->ProcVertexBuffer.Num(), 4);
	TestEqual(TEXT("full city photo takes three captures only"), Photo->GetCaptureCount(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfficeInteriorCityPhotoTest,
	"CGR.Office.Neighborhood.InteriorOwnsPhotoDisplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOfficeInteriorCityPhotoTest::RunTest(const FString& Parameters)
{
	UWorld* OfficeWorld = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("office world"), OfficeWorld)) { return false; }
	if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(OfficeWorld); }
	ON_SCOPE_EXIT
	{
		if (GEngine) { GEngine->DestroyWorldContext(OfficeWorld); }
		OfficeWorld->DestroyWorld(false);
	};
	UCGGameInstance* GameInstance = NewObject<UCGGameInstance>();
	OfficeWorld->SetGameInstance(GameInstance);
	GameInstance->SetCurrentManagedBuildingIndex(50);
	FOfficeNeighborhoodSnapshot& Snapshot = const_cast<FOfficeNeighborhoodSnapshot&>(GameInstance->GetOfficeNeighborhoodSnapshot());
	Snapshot.bValid = true;
	Snapshot.ManagedBuildingIndex = 50;
	Snapshot.SourceAnchorWorldTransform = FTransform(FRotator(0.f, 90.f, 0.f), FVector(6000.f, -8000.f, 12320.f));
	Snapshot.SourceBackdrop = NewObject<UOfficeDistantBackdrop>(GameInstance);
	FOfficeBackdropTransferTestAccess::PrimeCompleteCapture(Snapshot.SourceBackdrop);
	AOfficeInterior* Interior = OfficeWorld->SpawnActor<AOfficeInterior>();
	if (!TestNotNull(TEXT("office interior"), Interior)) { return false; }
	Interior->SetActorTransform(FTransform(FRotator(0.f, 135.f, 0.f), FVector(500.f, -700.f, 80.f), FVector(1.2f)));
	if (!TestTrue(TEXT("completed city photo is applied without a shell"), Interior->SynchronizeCityBackdrop())) { return false; }
	FObjectPropertyBase* BackdropProperty = FindFProperty<FObjectPropertyBase>(AOfficeInterior::StaticClass(), TEXT("CityBackdrop"));
	if (!TestNotNull(TEXT("interior owns the photo"), BackdropProperty)) { return false; }
	UOfficeDistantBackdrop* Backdrop = Cast<UOfficeDistantBackdrop>(BackdropProperty->GetObjectPropertyValue_InContainer(Interior));
	if (!TestNotNull(TEXT("active photo"), Backdrop)) { return false; }
	TestTrue(TEXT("photo is visible"), Backdrop->IsActive());
	TestTrue(TEXT("existing targets are reused"), Backdrop->GetRenderTargets() == Snapshot.SourceBackdrop->GetRenderTargets());
	UProceduralMeshComponent* Display = FOfficeBackdropTransferTestAccess::GetDisplay(Backdrop);
	if (!TestNotNull(TEXT("image display"), Display)) { return false; }
	TestTrue(TEXT("photo belongs to existing interior"), Display->GetOwner() == Interior);
	TestTrue(TEXT("city photo keeps original cm scale"), Display->GetComponentScale().Equals(FVector::OneVector));
	for (int32 Face = 0; Face < UOfficeDistantBackdrop::FaceCount; ++Face)
	{
		TestEqual(TEXT("each photo is a flat quad"), Display->GetProcMeshSection(Face)->ProcVertexBuffer.Num(), 4);
	}
	const FTransform ExpectedFrame = Snapshot.SourceBackdrop->GetCaptureFrame().GetRelativeTransform(Snapshot.SourceAnchorWorldTransform)
		* FTransform(Interior->GetActorQuat(), Interior->GetActorTransform().TransformPosition(
			FVector(Interior->GetCurrentFloorBoundsLocal().GetCenter(), Interior->GetStructuralFloorZLocal())));
	TestTrue(TEXT("photo aligns with office floor and source building"), Display->GetComponentTransform().Equals(ExpectedFrame));
	TestTrue(TEXT("repeat synchronization succeeds"), Interior->SynchronizeCityBackdrop());
	TestTrue(TEXT("repeat synchronization reuses image display"), FOfficeBackdropTransferTestAccess::GetDisplay(Backdrop) == Display);
	Snapshot.SourceBackdrop = nullptr;
	TestFalse(TEXT("missing photo is reported"), Interior->SynchronizeCityBackdrop());
	TestFalse(TEXT("missing photo clears stale image"), Backdrop->IsActive());
	return true;
}
#endif
