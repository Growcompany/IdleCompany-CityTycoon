// 도구 — 라이브 무대 RT 소비 머티리얼 M_GachaRevealRT / _Window 생성·재생성 (헤드리스). 그래프 SOT 는 이 파일.
// MD_UI/Translucent: c = OneMinus(RT.a) 가 캐릭터 커버리지. 알파 에지 8탭(축 4 + 대각 4×0.5)으로 외곽선을 굽는다.
// Emissive = lerp(RT.rgb, OutlineColor, edge * lerp(1, OutlineOpacity, c)), Opacity = max(c, edge * OutlineOpacity).
// 구 인게임 후처리 외곽선(4탭 CustomStencil, 2026-10-10 폐기)과 같은 구조 + 대각 소프트. 루마키 시절 Threshold/MaskBoost 는 제거됨.
#include "Misc/AutomationTest.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MaterialEditingLibrary.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSmoothStep.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureProperty.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "FileHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace GachaRevealRTMaterialTool
{
	// GetShortenPinName 이 Coordinates→UVs, TextureObject→Tex 로 줄여서 매칭하므로 짧은 쪽 이름으로 찾아야 한다.
	static const TCHAR* PinUVs = TEXT("UVs");
	static const TCHAR* PinTex = TEXT("Tex");

	bool Build(FAutomationTestBase& Test, const TCHAR* PackagePath, const TCHAR* AssetName)
	{
		UPackage* Pkg = CreatePackage(PackagePath);
		Pkg->FullyLoad();
		UMaterial* M = FindObject<UMaterial>(Pkg, AssetName);
		if (!M)
		{
			UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
			M = Cast<UMaterial>(Factory->FactoryCreateNew(UMaterial::StaticClass(), Pkg, FName(AssetName), RF_Public | RF_Standalone, nullptr, GWarn));
			if (M) { FAssetRegistryModule::AssetCreated(M); }
		}
		if (!M) { Test.AddError(FString::Printf(TEXT("%s 생성 실패"), AssetName)); return false; }
		M->Modify();
		// 엔진 DeleteAllMaterialExpressions 는 순회 중 제거라 노드를 건너뛴다(잔존 Threshold 실측) — 빌 때까지 반복
		for (int32 Pass = 0; Pass < 8 && M->GetExpressions().Num() > 0; ++Pass)
		{
			UMaterialEditingLibrary::DeleteAllMaterialExpressions(M);
		}
		if (M->GetExpressions().Num() > 0) { Test.AddError(FString::Printf(TEXT("%s 기존 노드 잔존 %d"), AssetName, M->GetExpressions().Num())); return false; }
		M->MaterialDomain = MD_UI;
		M->BlendMode = BLEND_Translucent;

		auto New = [&](TSubclassOf<UMaterialExpression> C, int32 X, int32 Y) { return UMaterialEditingLibrary::CreateMaterialExpression(M, C, X, Y); };
		auto Link = [&](UMaterialExpression* From, const TCHAR* Out, UMaterialExpression* To, const TCHAR* In)
		{
			if (!UMaterialEditingLibrary::ConnectMaterialExpressions(From, Out, To, In))
			{
				Test.AddError(FString::Printf(TEXT("%s 연결 실패 %s.%s→%s.%s"), AssetName, *From->GetName(), Out, *To->GetName(), In));
			}
		};
		auto Scalar = [&](const TCHAR* N, float V, int32 X, int32 Y)
		{
			auto* E = Cast<UMaterialExpressionScalarParameter>(New(UMaterialExpressionScalarParameter::StaticClass(), X, Y));
			E->ParameterName = N; E->DefaultValue = V; return E;
		};
		auto Const = [&](float V, int32 X, int32 Y)
		{
			auto* E = Cast<UMaterialExpressionConstant>(New(UMaterialExpressionConstant::StaticClass(), X, Y));
			E->R = V; return E;
		};

		// --- UV 창: TexCoord0 * UVWindow.zw + UVWindow.xy ---
		// ⚠ VectorParameter 기본 출력은 float3 라 .ba ComponentMask 가 컴파일 실패("Not enough components") → 채널 핀을 AppendVector 로 조립 (2026-08-12 실측)
		auto* TexCoord = New(UMaterialExpressionTextureCoordinate::StaticClass(), -2200, 0);
		auto* Window = Cast<UMaterialExpressionVectorParameter>(New(UMaterialExpressionVectorParameter::StaticClass(), -2200, 160));
		Window->ParameterName = TEXT("UVWindow"); Window->DefaultValue = FLinearColor(0.f, 0.f, 1.f, 1.f);
		auto* WinOffset = New(UMaterialExpressionAppendVector::StaticClass(), -2000, 120);
		Link(Window, TEXT("R"), WinOffset, TEXT("A")); Link(Window, TEXT("G"), WinOffset, TEXT("B"));
		auto* WinScale = New(UMaterialExpressionAppendVector::StaticClass(), -2000, 260);
		Link(Window, TEXT("B"), WinScale, TEXT("A")); Link(Window, TEXT("A"), WinScale, TEXT("B"));
		auto* UVScaled = New(UMaterialExpressionMultiply::StaticClass(), -1820, 60);
		Link(TexCoord, TEXT(""), UVScaled, TEXT("A")); Link(WinScale, TEXT(""), UVScaled, TEXT("B"));
		auto* UV = New(UMaterialExpressionAdd::StaticClass(), -1660, 60);
		Link(UVScaled, TEXT(""), UV, TEXT("A")); Link(WinOffset, TEXT(""), UV, TEXT("B"));

		// --- RT 텍스처 오브젝트 파라미터 + 텍셀 스텝 ---
		auto* RTParam = Cast<UMaterialExpressionTextureObjectParameter>(New(UMaterialExpressionTextureObjectParameter::StaticClass(), -2200, 460));
		RTParam->ParameterName = TEXT("RT"); RTParam->SamplerType = SAMPLERTYPE_Color;
		auto* Texel = Cast<UMaterialExpressionTextureProperty>(New(UMaterialExpressionTextureProperty::StaticClass(), -2000, 460));
		Texel->Property = TMTM_TexelSize;
		Link(RTParam, TEXT(""), Texel, PinTex);
		auto* Thickness = Scalar(TEXT("OutlineThickness"), 1.9f, -2000, 600);
		auto* Step = New(UMaterialExpressionMultiply::StaticClass(), -1820, 520);
		Link(Texel, TEXT(""), Step, TEXT("A")); Link(Thickness, TEXT(""), Step, TEXT("B"));

		// --- 중심 샘플 ---
		auto* Center = Cast<UMaterialExpressionTextureSample>(New(UMaterialExpressionTextureSample::StaticClass(), -1460, 0));
		Center->SamplerType = SAMPLERTYPE_Color;
		Link(RTParam, TEXT(""), Center, PinTex); Link(UV, TEXT(""), Center, PinUVs);

		// --- 8탭 알파 차이 누적 (축 4 + 대각 4, 대각은 같은 반경이 되도록 0.7071 + 가중 0.5) ---
		const float Dirs[8][2] = { { 1.f, 0.f }, { -1.f, 0.f }, { 0.f, 1.f }, { 0.f, -1.f },
			{ 0.7071f, 0.7071f }, { 0.7071f, -0.7071f }, { -0.7071f, 0.7071f }, { -0.7071f, -0.7071f } };
		auto* Half = Const(0.5f, -1820, 700);
		UMaterialExpression* Accum = nullptr;
		for (int32 TapIndex = 0; TapIndex < 8; ++TapIndex)
		{
			const int32 RowY = 900 + TapIndex * 220;
			auto* Dir = Cast<UMaterialExpressionConstant2Vector>(New(UMaterialExpressionConstant2Vector::StaticClass(), -1820, RowY));
			Dir->R = Dirs[TapIndex][0]; Dir->G = Dirs[TapIndex][1];
			auto* Offset = New(UMaterialExpressionMultiply::StaticClass(), -1640, RowY);
			Link(Step, TEXT(""), Offset, TEXT("A")); Link(Dir, TEXT(""), Offset, TEXT("B"));
			auto* TapUV = New(UMaterialExpressionAdd::StaticClass(), -1460, RowY);
			Link(UV, TEXT(""), TapUV, TEXT("A")); Link(Offset, TEXT(""), TapUV, TEXT("B"));
			auto* Tap = Cast<UMaterialExpressionTextureSample>(New(UMaterialExpressionTextureSample::StaticClass(), -1280, RowY));
			Tap->SamplerType = SAMPLERTYPE_Color;
			Link(RTParam, TEXT(""), Tap, PinTex); Link(TapUV, TEXT(""), Tap, PinUVs);
			auto* Diff = New(UMaterialExpressionSubtract::StaticClass(), -1060, RowY);
			Link(Tap, TEXT("A"), Diff, TEXT("A")); Link(Center, TEXT("A"), Diff, TEXT("B"));
			UMaterialExpression* Term = New(UMaterialExpressionAbs::StaticClass(), -900, RowY);
			Link(Diff, TEXT(""), Term, TEXT(""));
			if (TapIndex >= 4)
			{
				auto* Weighted = New(UMaterialExpressionMultiply::StaticClass(), -760, RowY);
				Link(Term, TEXT(""), Weighted, TEXT("A")); Link(Half, TEXT(""), Weighted, TEXT("B"));
				Term = Weighted;
			}
			if (!Accum) { Accum = Term; }
			else
			{
				auto* Sum = New(UMaterialExpressionAdd::StaticClass(), -600, RowY);
				Link(Accum, TEXT(""), Sum, TEXT("A")); Link(Term, TEXT(""), Sum, TEXT("B"));
				Accum = Sum;
			}
		}

		// --- edge = smoothstep(0, 1, saturate(합)) ---
		auto* EdgeRaw = New(UMaterialExpressionSaturate::StaticClass(), -420, 1600);
		Link(Accum, TEXT(""), EdgeRaw, TEXT(""));
		auto* Zero = Const(0.f, -420, 1740);
		auto* One = Const(1.f, -420, 1840);
		auto* Edge = New(UMaterialExpressionSmoothStep::StaticClass(), -260, 1660);
		Link(Zero, TEXT(""), Edge, TEXT("Min")); Link(One, TEXT(""), Edge, TEXT("Max")); Link(EdgeRaw, TEXT(""), Edge, TEXT("Value"));

		// --- 커버리지 c = OneMinus(RT.a) (기존 컷아웃 계약) ---
		auto* Coverage = New(UMaterialExpressionOneMinus::StaticClass(), -1060, 120);
		Link(Center, TEXT("A"), Coverage, TEXT(""));
		auto* Opacity = Scalar(TEXT("OutlineOpacity"), 0.8f, -1060, 260);

		// --- k = edge * lerp(1, OutlineOpacity, c) : 캐릭터 내부에선 선을 약하게 ---
		auto* InkMix = New(UMaterialExpressionLinearInterpolate::StaticClass(), -100, 200);
		Link(One, TEXT(""), InkMix, TEXT("A")); Link(Opacity, TEXT(""), InkMix, TEXT("B")); Link(Coverage, TEXT(""), InkMix, TEXT("Alpha"));
		auto* Ink = New(UMaterialExpressionMultiply::StaticClass(), 60, 300);
		Link(Edge, TEXT(""), Ink, TEXT("A")); Link(InkMix, TEXT(""), Ink, TEXT("B"));

		auto* OutlineColor = Cast<UMaterialExpressionVectorParameter>(New(UMaterialExpressionVectorParameter::StaticClass(), -100, 0));
		OutlineColor->ParameterName = TEXT("OutlineColor"); OutlineColor->DefaultValue = FLinearColor(0.12f, 0.10f, 0.10f, 1.f);
		auto* Emissive = New(UMaterialExpressionLinearInterpolate::StaticClass(), 240, 60);
		Link(Center, TEXT(""), Emissive, TEXT("A")); Link(OutlineColor, TEXT(""), Emissive, TEXT("B")); Link(Ink, TEXT(""), Emissive, TEXT("Alpha"));

		auto* EdgeAlpha = New(UMaterialExpressionMultiply::StaticClass(), 60, 500);
		Link(Edge, TEXT(""), EdgeAlpha, TEXT("A")); Link(Opacity, TEXT(""), EdgeAlpha, TEXT("B"));
		auto* FinalOpacity = New(UMaterialExpressionMax::StaticClass(), 240, 460);
		Link(Coverage, TEXT(""), FinalOpacity, TEXT("A")); Link(EdgeAlpha, TEXT(""), FinalOpacity, TEXT("B"));

		if (!UMaterialEditingLibrary::ConnectMaterialProperty(Emissive, TEXT(""), MP_EmissiveColor)) { Test.AddError(FString::Printf(TEXT("%s EmissiveColor 연결 실패"), AssetName)); }
		if (!UMaterialEditingLibrary::ConnectMaterialProperty(FinalOpacity, TEXT(""), MP_Opacity)) { Test.AddError(FString::Printf(TEXT("%s Opacity 연결 실패"), AssetName)); }

		UMaterialEditingLibrary::RecompileMaterial(M);
		M->PostEditChange();
		if (!UEditorLoadingAndSavingUtils::SavePackages({ Pkg }, false)) { Test.AddError(FString::Printf(TEXT("%s 저장 실패"), AssetName)); return false; }

		TArray<FName> ScalarNames; UMaterialEditingLibrary::GetScalarParameterNames(M, ScalarNames);
		TArray<FName> VectorNames; UMaterialEditingLibrary::GetVectorParameterNames(M, VectorNames);
		TArray<FName> TextureNames; UMaterialEditingLibrary::GetTextureParameterNames(M, TextureNames);
		UE_LOG(LogTemp, Display, TEXT("[RevealRT] ★ %s 저장 — scalar=%d vector=%d texture=%d"), AssetName, ScalarNames.Num(), VectorNames.Num(), TextureNames.Num());

		Test.TestEqual(*FString::Printf(TEXT("%s 스칼라 파라미터 2"), AssetName), ScalarNames.Num(), 2);
		Test.TestEqual(*FString::Printf(TEXT("%s 벡터 파라미터 2"), AssetName), VectorNames.Num(), 2);
		Test.TestEqual(*FString::Printf(TEXT("%s 텍스처 파라미터 1"), AssetName), TextureNames.Num(), 1);
		Test.TestTrue(*FString::Printf(TEXT("%s 파라미터 이름 계약(소비처 무변경)"), AssetName),
			ScalarNames.Contains(TEXT("OutlineThickness")) && ScalarNames.Contains(TEXT("OutlineOpacity"))
			&& VectorNames.Contains(TEXT("UVWindow")) && VectorNames.Contains(TEXT("OutlineColor"))
			&& TextureNames.Contains(TEXT("RT")));
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRGachaRevealRTMaterialBuild, "CGR.Tools.GachaRevealRTMaterialBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRGachaRevealRTMaterialBuild::RunTest(const FString& Parameters)
{
	// 두 에셋은 같은 그래프 — 소비처 경로(단발 / 멀티 슬롯)를 유지하려고 자산만 둘로 둔다
	const bool bSingle = GachaRevealRTMaterialTool::Build(*this, TEXT("/Game/CompanyGrowth/UI/Materials/M_GachaRevealRT"), TEXT("M_GachaRevealRT"));
	const bool bWindow = GachaRevealRTMaterialTool::Build(*this, TEXT("/Game/CompanyGrowth/UI/Materials/M_GachaRevealRT_Window"), TEXT("M_GachaRevealRT_Window"));
	return bSingle && bWindow;
}
#endif
