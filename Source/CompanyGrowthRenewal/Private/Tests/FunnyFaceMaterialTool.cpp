// 얼굴 정점색/피부 마스크를 기존 툰 명암에 연결하는 에셋 저작 도구.
#include "Misc/AutomationTest.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFunnyFaceMaterialBuild, "CGR.Tools.FunnyFaceMaterialBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRFunnyFaceMaterialBuild::RunTest(const FString& Parameters)
{
	UMaterial* SourceMaterial = LoadObject<UMaterial>(nullptr,
		TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/M_FunnyToon.M_FunnyToon"));
	if (!SourceMaterial) { AddError(TEXT("M_FunnyToon 로드 실패")); return false; }
	const TCHAR* PackagePath = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/M_FunnyFaceToon");
	UPackage* FacePackage = CreatePackage(PackagePath);
	FacePackage->FullyLoad();
	UMaterial* FaceMaterial = FindObject<UMaterial>(FacePackage, TEXT("M_FunnyFaceToon"));
	if (!FaceMaterial)
	{
		FaceMaterial = DuplicateObject<UMaterial>(SourceMaterial, FacePackage, TEXT("M_FunnyFaceToon"));
		if (!FaceMaterial) { AddError(TEXT("얼굴 툰 복제 실패")); return false; }
		FaceMaterial->SetFlags(RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(FaceMaterial);
	}

	UMaterialExpressionVectorParameter* SkinTint = nullptr;
	UMaterialExpressionMultiply* FaceColor = nullptr;
	TArray<FExpressionInput*> TintConsumers;
	for (UMaterialExpression* Expression : FaceMaterial->GetExpressions())
	{
		if (auto* Parameter = Cast<UMaterialExpressionVectorParameter>(Expression))
		{
			if (Parameter->ParameterName == TEXT("TintColor")) { SkinTint = Parameter; }
		}
		if (Expression && Expression->Desc == TEXT("FunnyFaceBaseColor"))
		{
			FaceColor = Cast<UMaterialExpressionMultiply>(Expression);
		}
	}
	if (!SkinTint) { AddError(TEXT("원본 툰 TintColor 계약 누락")); return false; }
	FaceMaterial->Modify();
	if (!FaceColor)
	{
		// 새 노드를 만들기 전에 소비자를 모아 SkinTint -> FaceColor -> SkinTint 순환을 피한다.
		for (UMaterialExpression* Expression : FaceMaterial->GetExpressions())
		{
			if (!Expression) { continue; }
			for (FExpressionInput* Input : Expression->GetInputsView())
			{
				if (Input && Input->Expression == SkinTint) { TintConsumers.Add(Input); }
			}
		}
		if (TintConsumers.IsEmpty()) { AddError(TEXT("TintColor 소비자 없음")); return false; }
		auto* VertexColor = Cast<UMaterialExpressionVertexColor>(UMaterialEditingLibrary::CreateMaterialExpression(
			FaceMaterial, UMaterialExpressionVertexColor::StaticClass(), -1900, -450));
		auto* SkinMask = Cast<UMaterialExpressionLinearInterpolate>(UMaterialEditingLibrary::CreateMaterialExpression(
			FaceMaterial, UMaterialExpressionLinearInterpolate::StaticClass(), -1700, -300));
		FaceColor = Cast<UMaterialExpressionMultiply>(UMaterialEditingLibrary::CreateMaterialExpression(
			FaceMaterial, UMaterialExpressionMultiply::StaticClass(), -1500, -350));
		if (!VertexColor || !SkinMask || !FaceColor) { AddError(TEXT("얼굴 그래프 생성 실패")); return false; }
		SkinMask->ConstA = 1.f;
		SkinMask->B.Connect(0, SkinTint);
		SkinMask->Alpha.Connect(4, VertexColor);
		FaceColor->A.Connect(0, VertexColor);
		FaceColor->B.Connect(0, SkinMask);
		FaceColor->Desc = TEXT("FunnyFaceBaseColor");
		for (FExpressionInput* Input : TintConsumers) { Input->Connect(0, FaceColor); }
	}
	// 색상은 정점 RGB, 피부색 적용 여부는 정점 A. 팔레트는 얼굴 파츠에 곱하지 않는다.
	for (UMaterialExpression* Expression : FaceMaterial->GetExpressions())
	{
		if (auto* Parameter = Cast<UMaterialExpressionScalarParameter>(Expression))
		{
			if (Parameter->ParameterName == TEXT("TexBlend")) { Parameter->DefaultValue = 0.f; }
		}
	}
	const auto* VertexColor = Cast<UMaterialExpressionVertexColor>(FaceColor->A.Expression);
	const auto* SkinMask = Cast<UMaterialExpressionLinearInterpolate>(FaceColor->B.Expression);
	if (!VertexColor || !SkinMask || SkinMask->B.Expression != SkinTint
		|| SkinMask->Alpha.Expression != VertexColor || SkinMask->Alpha.OutputIndex != 4 || SkinMask->ConstA != 1.f)
	{
		AddError(TEXT("얼굴 RGB * lerp(1, 피부색, A) 그래프 계약 불일치"));
		return false;
	}
	TArray<FName> ScalarNames;
	TArray<FName> VectorNames;
	UMaterialEditingLibrary::GetScalarParameterNames(FaceMaterial, ScalarNames);
	UMaterialEditingLibrary::GetVectorParameterNames(FaceMaterial, VectorNames);
	for (const FName Parameter : { FName(TEXT("Brightness")), FName(TEXT("ShadowThreshold")), FName(TEXT("ShadowSoftness")) })
	{
		if (!ScalarNames.Contains(Parameter)) { AddError(FString::Printf(TEXT("누락된 툰 파라미터: %s"), *Parameter.ToString())); return false; }
	}
	for (const FName Parameter : { FName(TEXT("LightDir")), FName(TEXT("ShadowTint")), FName(TEXT("TintColor")) })
	{
		if (!VectorNames.Contains(Parameter)) { AddError(FString::Printf(TEXT("누락된 툰 파라미터: %s"), *Parameter.ToString())); return false; }
	}
	FaceMaterial->bUsedWithSkeletalMesh = true;
	UMaterialEditingLibrary::RecompileMaterial(FaceMaterial);
	FaceMaterial->PostEditChange();
	FacePackage->MarkPackageDirty();
	if (!UEditorLoadingAndSavingUtils::SavePackages({ FacePackage }, false))
	{
		AddError(TEXT("M_FunnyFaceToon 저장 실패")); return false;
	}
	const FString Filename = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	TestTrue(TEXT("얼굴 머티리얼 디스크 저장"), IFileManager::Get().FileSize(*Filename) > 0);
	return true;
}
#endif
