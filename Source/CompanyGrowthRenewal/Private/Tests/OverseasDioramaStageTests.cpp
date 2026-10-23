#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Entity/OverseasDiorama/OverseasDioramaStage.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "TextureResource.h"
#include "Tests/OverseasDioramaTestSpecs.h"

namespace
{
	struct FDioStageFixture
	{
		UWorld* World = nullptr;

		FDioStageFixture()
		{
			World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
			if (World && GEngine)
			{
				GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
			}
		}

		~FDioStageFixture()
		{
			if (World)
			{
				if (GEngine) { GEngine->DestroyWorldContext(World); }
				World->DestroyWorld(false);
			}
		}

		AOverseasDioramaStage* Spawn() const
		{
			return World ? World->SpawnActor<AOverseasDioramaStage>(FVector(0.0, 0.0, 250000.0), FRotator::ZeroRotator) : nullptr;
		}
	};

	// RT 알파는 반전(형상=0)이라 PNG 로 남길 때 되돌린다.
	bool DioSaveTarget(UTextureRenderTarget2D* Target, const FString& Path, float& OutOpaqueRatio)
	{
		OutOpaqueRatio = 0.f;
		FTextureRenderTargetResource* Resource = Target ? Target->GameThread_GetRenderTargetResource() : nullptr;
		if (!Resource)
		{
			return false;
		}
		TArray<FLinearColor> Linear;
		if (!Resource->ReadLinearColorPixels(Linear) || Linear.Num() != Target->SizeX * Target->SizeY)
		{
			return false;
		}
		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(Linear.Num());
		int32 Opaque = 0;
		for (int32 i = 0; i < Linear.Num(); ++i)
		{
			const float Coverage = FMath::Clamp(1.f - Linear[i].A, 0.f, 1.f);
			FColor C = FLinearColor(Linear[i].R, Linear[i].G, Linear[i].B, 1.f).ToFColor(true);
			C.A = static_cast<uint8>(FMath::RoundToInt(Coverage * 255.f));
			Pixels[i] = C;
			Opaque += Coverage > 0.5f ? 1 : 0;
		}
		OutOpaqueRatio = static_cast<float>(Opaque) / static_cast<float>(Linear.Num());
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		return FFileHelper::SaveArrayToFile(Png, *Path);
	}

	void DioWarmUpRendering()
	{
		FlushRenderingCommands();
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
		FlushRenderingCommands();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasStageBuildSectionsTest, "CGR.Overseas.Stage.BuildSections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasStageBuildSectionsTest::RunTest(const FString& Parameters)
{
	FDioStageFixture Fixture;
	AOverseasDioramaStage* Stage = Fixture.Spawn();
	if (!TestNotNull(TEXT("stage"), Stage)) { return false; }
	Stage->BuildFor(OverseasDioramaTestSpecs::Korea4(), false);
	TestTrue(TEXT("has build"), Stage->HasBuild());
	TestTrue(TEXT("tile sections"), Stage->GetTileMesh()->GetNumSections() > 0);
	TestEqual(TEXT("one module"), Stage->GetModuleMeshes().Num(), 1);
	for (UProceduralMeshComponent* Mesh : Stage->GetModuleMeshes())
	{
		TestTrue(TEXT("module sections"), Mesh && Mesh->GetNumSections() > 0);
		for (int32 i = 0; Mesh && i < Mesh->GetNumSections(); ++i)
		{
			const UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(i));
			TestTrue(TEXT("toon parent"), MID && MID->Parent && MID->Parent->GetName() == TEXT("M_FunnyToon"));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasStageRebuildTest, "CGR.Overseas.Stage.RebuildClearsOld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasStageRebuildTest::RunTest(const FString& Parameters)
{
	FDioStageFixture Fixture;
	AOverseasDioramaStage* Reused = Fixture.Spawn();
	AOverseasDioramaStage* Fresh = Fixture.Spawn();
	if (!TestNotNull(TEXT("stages"), Reused) || !TestNotNull(TEXT("fresh"), Fresh)) { return false; }
	Reused->BuildFor(OverseasDioramaTestSpecs::Japan6(), false);
	Reused->BuildFor(OverseasDioramaTestSpecs::SingaporePort(), false);
	Fresh->BuildFor(OverseasDioramaTestSpecs::SingaporePort(), false);
	TestEqual(TEXT("rig count matches fresh build"), Reused->GetRigComponentCount(), Fresh->GetRigComponentCount());
	TestEqual(TEXT("module count"), Reused->GetModuleMeshes().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasStageOilPumpAnimationTest, "CGR.Overseas.Stage.OilPumpAnimation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasStageOilPumpAnimationTest::RunTest(const FString& Parameters)
{
	using S = EDioramaSlotState;
	FDioStageFixture Fixture;
	AOverseasDioramaStage* Stage = Fixture.Spawn();
	if (!TestNotNull(TEXT("stage"), Stage)) { return false; }
	FOverseasDioramaSpec Spec;
	Spec.Country = ECountryType::Saudi;
	OverseasDioramaTestSpecs::AddMine(Spec, {
		OverseasDioramaTestSpecs::Unit(EResourceType::Oil, S::Running, 0.3f),
		OverseasDioramaTestSpecs::Unit(EResourceType::Oil, S::Empty, 0.f),
		OverseasDioramaTestSpecs::Unit(EResourceType::Oil, S::Completed, 1.f) }, 3, 1, 0, false);
	Stage->BuildFor(Spec, false);
	TArray<USkeletalMeshComponent*> Pumps;
	Stage->GetComponents(Pumps);
	Pumps.Sort([](const USkeletalMeshComponent& A, const USkeletalMeshComponent& B)
	{
		return A.GetRelativeLocation().Y < B.GetRelativeLocation().Y;
	});
	if (!TestEqual(TEXT("one skeletal pump per oil unit"), Pumps.Num(), 3)) { return false; }
	for (USkeletalMeshComponent* Pump : Pumps)
	{
		TestTrue(TEXT("original oil drilling skeletal mesh"), Pump->GetSkeletalMeshAsset() &&
			Pump->GetSkeletalMeshAsset()->GetPathName() == TEXT("/Game/CompanyGrowth/Factory/OilDrilling/SM_OilDrilling.SM_OilDrilling"));
		const UAnimSingleNodeInstance* Animation = Pump->GetSingleNodeInstance();
		TestTrue(TEXT("original oil drilling animation"), Animation && Animation->GetAnimationAsset() &&
			Animation->GetAnimationAsset()->GetPathName() == TEXT("/Game/CompanyGrowth/Factory/OilDrilling/uploads_files_4730463_Oil_Drilling_Animated_for_Blender_Anim.uploads_files_4730463_Oil_Drilling_Animated_for_Blender_Anim"));
		TestFalse(TEXT("component does not tick on its own"), Pump->IsComponentTickEnabled());
	}

	UTextureRenderTarget2D* Live = NewObject<UTextureRenderTarget2D>(Stage);
	const float RunningStart = Pumps[0]->GetPosition();
	const float EmptyStart = Pumps[1]->GetPosition();
	const float CompletedStart = Pumps[2]->GetPosition();
	Stage->StartLive(Live);
	Stage->Tick(0.25f);
	TestTrue(TEXT("running pump advances in live mode"), Pumps[0]->GetPosition() > RunningStart + 0.1f);
	TestEqual(TEXT("empty pump stays still"), Pumps[1]->GetPosition(), EmptyStart);
	TestEqual(TEXT("completed pump stays still"), Pumps[2]->GetPosition(), CompletedStart);
	Stage->StopLive();
	const float StoppedTime = Pumps[0]->GetPosition();
	Stage->Tick(0.25f);
	TestEqual(TEXT("stopped live pump stays still"), Pumps[0]->GetPosition(), StoppedTime);

	Stage->BuildFor(Spec, true);
	TArray<USkeletalMeshComponent*> StillPumps;
	Stage->GetComponents(StillPumps);
	TestEqual(TEXT("still build has one pump per unit"), StillPumps.Num(), 3);
	TestFalse(TEXT("old live rig was destroyed"), IsValid(Pumps[0]));
	if (StillPumps.Num() > 0)
	{
		const float StillTime = StillPumps[0]->GetPosition();
		Stage->Tick(0.25f);
		TestEqual(TEXT("still pump stays still"), StillPumps[0]->GetPosition(), StillTime);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasStageFocusMissingTest, "CGR.Overseas.Stage.FocusMissingModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasStageFocusMissingTest::RunTest(const FString& Parameters)
{
	FDioStageFixture Fixture;
	AOverseasDioramaStage* Stage = Fixture.Spawn();
	if (!TestNotNull(TEXT("stage"), Stage)) { return false; }
	Stage->BuildFor(OverseasDioramaTestSpecs::SingaporePort(), false);
	Stage->FocusModule(EDioramaModule::Factory, true);
	const FDioramaLiveCamera Goal = Stage->GetLiveCameraGoal();
	TestTrue(TEXT("port rect center"), Goal.TargetM.Equals(FVector(0.0, 0.0, 1.5), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasStageCameraDistanceTest, "CGR.Overseas.Stage.CameraDistance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasStageCameraDistanceTest::RunTest(const FString& Parameters)
{
	using S = EDioramaSlotState;
	FDioStageFixture Fixture;
	AOverseasDioramaStage* Stage = Fixture.Spawn();
	if (!TestNotNull(TEXT("stage"), Stage)) { return false; }

	Stage->BuildFor(OverseasDioramaTestSpecs::Korea4(), false);
	Stage->FocusModule(EDioramaModule::Factory, true);
	TestEqual(TEXT("factory 110"), Stage->GetLiveCameraGoal().DistM, 110.0, 0.001);

	FOverseasDioramaSpec One;
	One.Country = ECountryType::Australia;
	OverseasDioramaTestSpecs::AddMine(One, { OverseasDioramaTestSpecs::Unit(EResourceType::IronOre, S::Empty, 0.f) }, 0, 0, 0, false);
	Stage->BuildFor(One, false);
	Stage->FocusModule(EDioramaModule::Mine, true);
	TestEqual(TEXT("mine 1 unit"), Stage->GetLiveCameraGoal().DistM, 66.0, 0.001);

	Stage->BuildFor(OverseasDioramaTestSpecs::SaudiRefinery(), false);
	Stage->FocusModule(EDioramaModule::Mine, true);
	TestEqual(TEXT("mine 5 units"), Stage->GetLiveCameraGoal().DistM, 103.04, 0.001);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverseasRenderProbeTest, "CGR.Overseas.Render.Probe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverseasRenderProbeTest::RunTest(const FString& Parameters)
{
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("렌더 없는 실행이라 탐침 생략(-d3d12 -RenderOffscreen 으로 실행)"));
		return true;
	}
	FDioStageFixture Fixture;
	AOverseasDioramaStage* Stage = Fixture.Spawn();
	if (!TestNotNull(TEXT("stage"), Stage)) { return false; }

	const FString Dir = FPaths::ProjectSavedDir() / TEXT("OverseasDiorama/Probe");
	IFileManager::Get().MakeDirectory(*Dir, true);

	const TPair<const TCHAR*, FOverseasDioramaSpec> Cases[] = {
		{ TEXT("KR4"), OverseasDioramaTestSpecs::Korea4() },
		{ TEXT("JP6"), OverseasDioramaTestSpecs::Japan6() },
		{ TEXT("US3"), OverseasDioramaTestSpecs::Usa3() },
		{ TEXT("SA_refinery"), OverseasDioramaTestSpecs::SaudiRefinery() },
		{ TEXT("SG_port"), OverseasDioramaTestSpecs::SingaporePort() },
		{ TEXT("BR_mine"), OverseasDioramaTestSpecs::BrazilMine() },
	};

	UTextureRenderTarget2D* Live = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	Live->RenderTargetFormat = RTF_RGBA16f;
	Live->ClearColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
	Live->InitAutoFormat(1200, 900);
	Live->UpdateResourceImmediate(true);

	for (const TPair<const TCHAR*, FOverseasDioramaSpec>& Case : Cases)
	{
		Stage->BuildFor(Case.Value, true);
		UTextureRenderTarget2D* Still = nullptr;
		FDioramaStillInfo Info;
		DioWarmUpRendering();
		if (!TestTrue(FString::Printf(TEXT("%s still capture"), Case.Key), Stage->CaptureStill(Still, Info)))
		{
			continue;
		}
		DioWarmUpRendering();
		Stage->CaptureStill(Still, Info);
		FlushRenderingCommands();
		float StillRatio = 0.f;
		TestTrue(FString::Printf(TEXT("%s still png"), Case.Key), DioSaveTarget(Still, Dir / FString::Printf(TEXT("%s_still.png"), Case.Key), StillRatio));
		TestTrue(FString::Printf(TEXT("%s still coverage %.2f"), Case.Key, StillRatio), StillRatio > 0.15f && StillRatio < 0.85f);
		AddInfo(FString::Printf(TEXT("%s still %dx%d pivot(%.1f,%.1f) coverage %.2f"), Case.Key, Info.SizePx.X, Info.SizePx.Y, Info.PivotPx.X, Info.PivotPx.Y, StillRatio));

		Stage->BuildFor(Case.Value, false);
		Stage->StartLive(Live);
		Stage->FocusModule(Case.Value.PrimaryModule(), true);
		DioWarmUpRendering();
		Stage->CaptureLiveFrameNow();
		FlushRenderingCommands();
		float LiveRatio = 0.f;
		TestTrue(FString::Printf(TEXT("%s live png"), Case.Key), DioSaveTarget(Live, Dir / FString::Printf(TEXT("%s_live.png"), Case.Key), LiveRatio));
		TestTrue(FString::Printf(TEXT("%s live coverage %.2f"), Case.Key, LiveRatio), LiveRatio > 0.10f);
		Stage->StopLive();
	}
	return true;
}

#endif
