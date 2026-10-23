// 도구 — 캐릭터 전용 2톤 툰 머티리얼 M_FunnyToon 생성/재생성 (헤드리스). 그래프 SOT 는 이 파일.
// Unlit: Emissive = lerp(Base*ShadowTint, Base, smoothstep(NdotL)) + Base*Band*Metallic + Base*Emissive
// Base 는 GarmentZones(의상 존) 를 거친다 — PreSkinnedPosition 으로 카라/앞단/소매/밴드를 나눠 칠한다(UV 불필요).
// Base = T_Textures * TintColor * Brightness, 채도는 Desaturation(1-Saturation) 로 외삽. LightDir 기본값 = OfficeMap 태양.
#include "Misc/AutomationTest.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "MaterialEditingLibrary.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionSmoothStep.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionPreSkinnedPosition.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Engine/Texture2D.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGRFunnyToonMaterialBuild, "CGR.Tools.FunnyToonMaterialBuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCGRFunnyToonMaterialBuild::RunTest(const FString& Parameters)
{
	static const TCHAR* PkgPath = TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/M_FunnyToon");
	UTexture2D* Palette = LoadObject<UTexture2D>(nullptr, TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/T_Textures.T_Textures"));
	if (!Palette) { AddError(TEXT("T_Textures 로드 실패")); return false; }

	// 오피스 고정 태양 방향 — 못 읽으면 위-앞-왼쪽 기본값
	FVector LightDir(-0.5f, 0.35f, 0.79f);
	if (UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/CompanyGrowth/Level/OfficeMap")))
	{
		for (TActorIterator<ADirectionalLight> It(GWorld); It; ++It)
		{
			// 라이트 forward = 빛이 진행하는 방향. 셰이더는 표면→광원 벡터가 필요하므로 반전.
			LightDir = -It->GetActorForwardVector();
			UE_LOG(LogTemp, Display, TEXT("[Toon] OfficeMap 태양 %s forward=%s → LightDir=%s"), *It->GetActorLabel(), *It->GetActorForwardVector().ToString(), *LightDir.ToString());
			break;
		}
	}
	else { UE_LOG(LogTemp, Warning, TEXT("[Toon] OfficeMap 로드 실패 — 기본 LightDir 사용")); }

	UPackage* Pkg = CreatePackage(PkgPath);
	Pkg->FullyLoad();
	UMaterial* M = FindObject<UMaterial>(Pkg, TEXT("M_FunnyToon"));
	if (!M)
	{
		UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
		M = Cast<UMaterial>(Factory->FactoryCreateNew(UMaterial::StaticClass(), Pkg, TEXT("M_FunnyToon"), RF_Public | RF_Standalone, nullptr, GWarn));
		FAssetRegistryModule::AssetCreated(M);
	}
	if (!M) { AddError(TEXT("머티리얼 생성 실패")); return false; }
	M->Modify();
	while (const int32 ExpressionCount = UMaterialEditingLibrary::GetNumMaterialExpressions(M))
	{
		UMaterialEditingLibrary::DeleteAllMaterialExpressions(M);
		if (UMaterialEditingLibrary::GetNumMaterialExpressions(M) >= ExpressionCount)
		{ AddError(TEXT("Toon graph cleanup stalled")); return false; }
	}
	M->SetShadingModel(MSM_Unlit);
	M->BlendMode = BLEND_Opaque;
	M->bUsedWithSkeletalMesh = true;
	M->bUsedWithMorphTargets = true;

	auto New = [&](TSubclassOf<UMaterialExpression> C, int32 X, int32 Y) { return UMaterialEditingLibrary::CreateMaterialExpression(M, C, X, Y); };
	auto Scalar = [&](const TCHAR* N, float V, int32 X, int32 Y) { auto* E = Cast<UMaterialExpressionScalarParameter>(New(UMaterialExpressionScalarParameter::StaticClass(), X, Y)); E->ParameterName = N; E->DefaultValue = V; return E; };
	auto Vector = [&](const TCHAR* N, FLinearColor V, int32 X, int32 Y) { auto* E = Cast<UMaterialExpressionVectorParameter>(New(UMaterialExpressionVectorParameter::StaticClass(), X, Y)); E->ParameterName = N; E->DefaultValue = V; return E; };
	auto Link = [&](UMaterialExpression* From, UMaterialExpression* To, const TCHAR* In) { if (!UMaterialEditingLibrary::ConnectMaterialExpressions(From, TEXT(""), To, In)) { AddError(FString::Printf(TEXT("연결 실패 %s→%s.%s"), *From->GetName(), *To->GetName(), In)); } };

	// --- Base color: Tex * Tint * Brightness, 채도 외삽 ---
	auto* Tex = Cast<UMaterialExpressionTextureSample>(New(UMaterialExpressionTextureSample::StaticClass(), -1400, -200)); Tex->Texture = Palette; Tex->SamplerType = SAMPLERTYPE_Color;
	auto* Tint = Vector(TEXT("TintColor"), FLinearColor::White, -1400, 0);
	auto* Bright = Scalar(TEXT("Brightness"), 1.25f, -1400, 150);
	auto* Sat = Scalar(TEXT("Saturation"), 1.1f, -1400, 250);
	auto* Gain = Scalar(TEXT("TexGain"), 1.f, -1400, 80);   // 팔레트 셀 보정. Body 셀은 linear ≈0.185 라 C++ 이 5.4 를 넣는다(눈=0 은 그대로 0)
	auto* TexG = New(UMaterialExpressionMultiply::StaticClass(), -1250, -150); Link(Tex, TexG, TEXT("A")); Link(Gain, TexG, TEXT("B"));
	auto* M1 = New(UMaterialExpressionMultiply::StaticClass(), -1150, -100); Link(TexG, M1, TEXT("A")); Link(Tint, M1, TEXT("B"));
	auto* TexB = Scalar(TEXT("TexBlend"), 1.f, -1400, 350);   // 0=틴트만(Body/Hair), 1=텍스처x틴트
	auto* M1b = New(UMaterialExpressionLinearInterpolate::StaticClass(), -1100, 50); Link(Tint, M1b, TEXT("A")); Link(M1, M1b, TEXT("B")); Link(TexB, M1b, TEXT("Alpha"));
	auto* LocalPosition = New(UMaterialExpressionPreSkinnedPosition::StaticClass(), -900, 1500);
	auto* PositionInterpolator = Cast<UMaterialExpressionVertexInterpolator>(New(UMaterialExpressionVertexInterpolator::StaticClass(), -750, 1500));
	PositionInterpolator->Input.Connect(0, LocalPosition);
	// 몸통에 밀착된 라운드넥 이너. 기준 자세 좌표로 목·팔·다리의 피부를 보존한다.
	auto* InnerShirt = Cast<UMaterialExpressionCustom>(New(UMaterialExpressionCustom::StaticClass(), -750, -450));
	InnerShirt->Description = TEXT("Body inner shirt in reference pose");
	InnerShirt->OutputType = CMOT_Float3;
	InnerShirt->Inputs.Reset();
	InnerShirt->Code = TEXT(R"HLSL(
float x = abs(LocalPos.x);
float neckCurve = sqrt(saturate(1.0 - x * x / max(NeckWidth * NeckWidth, 1e-4)));
float neckline = ShoulderHeight - NeckDepth * neckCurve;
float distance = min(min(LocalPos.z - HemHeight, neckline - LocalPos.z), HalfWidth - x);
float coverage = saturate(distance / max(abs(ddx(distance)) + abs(ddy(distance)), 1e-4) + 0.5);
return lerp(SkinColor, ShirtColor, coverage);
)HLSL");
	auto BindInner = [&](const TCHAR* Name, UMaterialExpression* Expression)
	{
		FCustomInput& Input = InnerShirt->Inputs.AddDefaulted_GetRef();
		Input.InputName = Name;
		Input.Input.Connect(0, Expression);
	};
	BindInner(TEXT("LocalPos"), PositionInterpolator);
	BindInner(TEXT("SkinColor"), M1b);
	BindInner(TEXT("ShirtColor"), Vector(TEXT("InnerShirtColor"), FLinearColor(0.665f, 0.708f, 0.761f), -1400, -450));
	BindInner(TEXT("HemHeight"), Scalar(TEXT("InnerShirtHemHeight"), 66.f, -1400, -550));
	BindInner(TEXT("ShoulderHeight"), Scalar(TEXT("InnerShirtShoulderHeight"), 127.f, -1400, -650));
	BindInner(TEXT("HalfWidth"), Scalar(TEXT("InnerShirtHalfWidth"), 23.f, -1400, -750));
	BindInner(TEXT("NeckWidth"), Scalar(TEXT("InnerShirtNeckWidth"), 9.f, -1400, -850));
	BindInner(TEXT("NeckDepth"), Scalar(TEXT("InnerShirtNeckDepth"), 7.f, -1400, -950));
	auto* InnerSwitch = Cast<UMaterialExpressionStaticSwitchParameter>(New(UMaterialExpressionStaticSwitchParameter::StaticClass(), -550, -450));
	InnerSwitch->ParameterName = TEXT("InnerShirtEnabled");
	InnerSwitch->DefaultValue = false;
	InnerSwitch->A.Connect(0, InnerShirt);
	InnerSwitch->B.Connect(0, M1b);
	auto* M2 = New(UMaterialExpressionMultiply::StaticClass(), -1000, -100); Link(InnerSwitch, M2, TEXT("A")); Link(Bright, M2, TEXT("B"));
	auto* SatInv = New(UMaterialExpressionOneMinus::StaticClass(), -1150, 250); Link(Sat, SatInv, TEXT(""));
	auto* Desat = New(UMaterialExpressionDesaturation::StaticClass(), -850, -100); Link(M2, Desat, TEXT("")); Link(SatInv, Desat, TEXT("Fraction"));
	// 의상 존 — 스킨 전 로컬 좌표라 걷거나 앉아도 카라가 목에 붙어 있는다. 화면/월드 좌표면 옷 위에서 미끄러진다.
	auto* Zones = Cast<UMaterialExpressionCustom>(New(UMaterialExpressionCustom::StaticClass(), -600, 1500));
	Zones->Description = TEXT("Garment zones: collar / placket / raglan sleeve / band");
	Zones->OutputType = CMOT_Float3;
	Zones->Inputs.Reset();
	Zones->Code = TEXT(R"HLSL(
float  t     = (LocalPos.z - ZoneMin) / max(ZoneMax - ZoneMin, 1e-4);
float3 c     = BaseColor;
int    m     = (int)(Mode + 0.5);
float  xWidth = max(abs(ddx(LocalPos.x)) + abs(ddy(LocalPos.x)), 1e-4);
float  yWidth = max(abs(ddx(LocalPos.y)) + abs(ddy(LocalPos.y)), 1e-4);
float  tWidth = max(abs(ddx(t)) + abs(ddy(t)), 1e-4);
float  xLo = LocalPos.x - 0.5 * xWidth;
float  xHi = LocalPos.x + 0.5 * xWidth;
float  tLo = t - 0.5 * tWidth;
float  tHi = t + 0.5 * tWidth;
float  front = saturate((2.0 - LocalPos.y) / yWidth + 0.5);
if (m == 1) {
    if (CuffStart > 0.01 && XMax > 45.0) {
        float cuff = 1.0 - saturate((min(xHi, XMax - CuffStart) - max(xLo, CuffStart - XMax)) / xWidth);
        c = lerp(c, Accent, cuff);
    }
    float placketX = saturate((min(xHi, PlacketWidth) - max(xLo, -PlacketWidth)) / xWidth);
    float placketT = saturate((min(tHi, 0.93) - max(tLo, 0.70)) / tWidth);
    c = lerp(c, Accent, front * placketX * placketT);
    float collar = saturate((tHi - CollarTop) / tWidth);
    float notchX = saturate((min(xHi, CollarNotch) - max(xLo, -CollarNotch)) / xWidth);
    float notchT = saturate((min(tHi, CollarTop + 0.045) - max(tLo, CollarTop)) / tWidth);
    c = lerp(c, Accent, saturate(collar - front * notchX * notchT));
    float buttonX = saturate((min(xHi, PlacketWidth * 0.42) - max(xLo, -PlacketWidth * 0.42)) / xWidth);
    float buttons = saturate((min(tHi, 0.815) - max(tLo, 0.800)) / tWidth)
                  + saturate((min(tHi, 0.750) - max(tLo, 0.735)) / tWidth);
    c = lerp(c, BaseColor * 0.45, front * buttonX * saturate(buttons));
} else if (m == 2) {
    float sleeve = 1.0 - saturate((min(xHi, SleeveSplit) - max(xLo, -SleeveSplit)) / xWidth);
    float pipeOuter = saturate((min(xHi, SleeveSplit - PipingWidth) - max(xLo, PipingWidth - SleeveSplit)) / xWidth);
    float piping = saturate(1.0 - sleeve - pipeOuter);
    c = c * (1.0 - sleeve - piping) + BaseColor * 0.55 * sleeve + Accent * 0.92 * piping;
    c = lerp(c, Accent * 0.92, saturate((tHi - CollarTop) / tWidth));
} else if (m == 3) {
    float bands = max(BandCount, 1.0);
    float qLo = max(tLo * bands, 0.0);
    float qHi = max(tHi * bands, 0.0);
    float loPairs = floor(qLo * 0.5);
    float hiPairs = floor(qHi * 0.5);
    float loArea = loPairs + saturate(qLo - 2.0 * loPairs - 1.0);
    float hiArea = hiPairs + saturate(qHi - 2.0 * hiPairs - 1.0);
    float stripe = saturate((hiArea - loArea) / (tWidth * bands));
    c = lerp(c, BaseColor * 0.38 + Accent * 0.62, stripe);
}
return c;
	)HLSL");
	auto BindZone = [&](const TCHAR* Name, UMaterialExpression* Expression)
	{
		FCustomInput& Input = Zones->Inputs.AddDefaulted_GetRef();
		Input.InputName = Name;
		Input.Input.Connect(0, Expression);
	};
	BindZone(TEXT("BaseColor"), Desat);
	BindZone(TEXT("LocalPos"), PositionInterpolator);
	BindZone(TEXT("Mode"), Scalar(TEXT("GarmentMode"), 0.f, -900, 1600));
	BindZone(TEXT("ZoneMin"), Scalar(TEXT("GarmentZoneMin"), 0.f, -900, 1660));
	BindZone(TEXT("ZoneMax"), Scalar(TEXT("GarmentZoneMax"), 1.f, -900, 1720));
	BindZone(TEXT("XMax"), Scalar(TEXT("GarmentXMax"), 0.f, -900, 1780));
	BindZone(TEXT("Accent"), Vector(TEXT("GarmentAccent"), FLinearColor(0.93f, 0.94f, 0.96f), -900, 1840));
	BindZone(TEXT("CollarTop"), Scalar(TEXT("GarmentCollarTop"), 0.905f, -900, 1900));
	BindZone(TEXT("CollarNotch"), Scalar(TEXT("GarmentCollarNotch"), 4.4f, -900, 1960));
	BindZone(TEXT("PlacketWidth"), Scalar(TEXT("GarmentPlacketWidth"), 2.6f, -900, 2020));
	BindZone(TEXT("SleeveSplit"), Scalar(TEXT("GarmentSleeveSplit"), 11.5f, -900, 2080));
	BindZone(TEXT("PipingWidth"), Scalar(TEXT("GarmentPipingWidth"), 1.5f, -900, 2140));
	BindZone(TEXT("BandCount"), Scalar(TEXT("GarmentBandCount"), 7.f, -900, 2200));
	BindZone(TEXT("CuffStart"), Scalar(TEXT("GarmentCuffStart"), 6.f, -900, 2260));

	// 의상 슬롯만 켠다 — 바디/헤어/안경은 기존 permutation 을 그대로 쓴다.
	auto* ZoneSwitch = Cast<UMaterialExpressionStaticSwitchParameter>(New(UMaterialExpressionStaticSwitchParameter::StaticClass(), -420, 1500));
	ZoneSwitch->ParameterName = TEXT("GarmentZonesEnabled");
	ZoneSwitch->DefaultValue = false;
	ZoneSwitch->A.Connect(0, Zones);
	ZoneSwitch->B.Connect(0, Desat);

	UMaterialExpression* Base = ZoneSwitch;

	// --- 2톤: NdotL smoothstep ---
	auto* LDir = Vector(TEXT("LightDir"), FLinearColor(LightDir.X, LightDir.Y, LightDir.Z, 0.f), -1400, 450);
	auto* LNorm = New(UMaterialExpressionNormalize::StaticClass(), -1200, 450); Link(LDir, LNorm, TEXT("VectorInput"));
	auto* Nrm = New(UMaterialExpressionVertexNormalWS::StaticClass(), -1200, 550);
	auto* NdL = New(UMaterialExpressionDotProduct::StaticClass(), -1000, 480); Link(Nrm, NdL, TEXT("A")); Link(LNorm, NdL, TEXT("B"));
	auto* Thr = Scalar(TEXT("ShadowThreshold"), 0.05f, -1200, 650);
	auto* Soft = Scalar(TEXT("ShadowSoftness"), 0.12f, -1200, 750);
	auto* Lo = New(UMaterialExpressionSubtract::StaticClass(), -1000, 650); Link(Thr, Lo, TEXT("A")); Link(Soft, Lo, TEXT("B"));
	auto* Hi = New(UMaterialExpressionAdd::StaticClass(), -1000, 750); Link(Thr, Hi, TEXT("A")); Link(Soft, Hi, TEXT("B"));
	auto* Shade = New(UMaterialExpressionSmoothStep::StaticClass(), -800, 550); Link(Lo, Shade, TEXT("Min")); Link(Hi, Shade, TEXT("Max")); Link(NdL, Shade, TEXT("Value"));
	auto* STint = Vector(TEXT("ShadowTint"), FLinearColor(0.72f, 0.74f, 0.80f), -850, 100);
	auto* Dark = New(UMaterialExpressionMultiply::StaticClass(), -650, 0); Link(Base, Dark, TEXT("A")); Link(STint, Dark, TEXT("B"));
	auto* Lit = New(UMaterialExpressionLinearInterpolate::StaticClass(), -450, 100); Link(Dark, Lit, TEXT("A")); Link(Base, Lit, TEXT("B")); Link(Shade, Lit, TEXT("Alpha"));

	// --- Premium 골드 하이라이트 밴드: Metallic 이 강도 (0=없음) ---
	auto* Cam = New(UMaterialExpressionCameraVectorWS::StaticClass(), -1200, 900);
	auto* HSum = New(UMaterialExpressionAdd::StaticClass(), -1000, 900); Link(LNorm, HSum, TEXT("A")); Link(Cam, HSum, TEXT("B"));
	auto* HNorm = New(UMaterialExpressionNormalize::StaticClass(), -850, 900); Link(HSum, HNorm, TEXT("VectorInput"));
	auto* NdH = New(UMaterialExpressionDotProduct::StaticClass(), -700, 900); Link(Nrm, NdH, TEXT("A")); Link(HNorm, NdH, TEXT("B"));
	auto* BLo = Cast<UMaterialExpressionConstant>(New(UMaterialExpressionConstant::StaticClass(), -700, 1000)); BLo->R = 0.90f;
	auto* BHi = Cast<UMaterialExpressionConstant>(New(UMaterialExpressionConstant::StaticClass(), -700, 1080)); BHi->R = 0.94f;
	auto* Band = New(UMaterialExpressionSmoothStep::StaticClass(), -500, 950); Link(BLo, Band, TEXT("Min")); Link(BHi, Band, TEXT("Max")); Link(NdH, Band, TEXT("Value"));
	auto* Metal = Scalar(TEXT("Metallic"), 0.f, -500, 1100);
	auto* BandW = New(UMaterialExpressionMultiply::StaticClass(), -350, 1000); Link(Band, BandW, TEXT("A")); Link(Metal, BandW, TEXT("B"));
	auto* BandC = New(UMaterialExpressionMultiply::StaticClass(), -200, 900); Link(Base, BandC, TEXT("A")); Link(BandW, BandC, TEXT("B"));

	// --- Emissive 가산 ---
	auto* Emis = Scalar(TEXT("Emissive"), 0.f, -500, 1250);
	auto* EmisC = New(UMaterialExpressionMultiply::StaticClass(), -200, 1200); Link(Base, EmisC, TEXT("A")); Link(Emis, EmisC, TEXT("B"));
	auto* Out1 = New(UMaterialExpressionAdd::StaticClass(), -50, 300); Link(Lit, Out1, TEXT("A")); Link(BandC, Out1, TEXT("B"));
	auto* Out2 = New(UMaterialExpressionAdd::StaticClass(), 100, 300); Link(Out1, Out2, TEXT("A")); Link(EmisC, Out2, TEXT("B"));
	// 일반 직원 permutation은 기존 Out2만 컴파일한다. FX 입력은 장비 전용 MIC에서만 활성화한다.
	auto* FX = Cast<UMaterialExpressionCustom>(New(UMaterialExpressionCustom::StaticClass(), 400, 700));
	FX->Description = TEXT("Equipment subtle static rim");
	FX->OutputType = CMOT_Float3;
	FX->Inputs.Reset();
	FX->Code = TEXT(R"HLSL(
float facing = saturate(dot(normalize(Normal), normalize(View)));
float rim = pow(1.0 - facing, 8.0);
return max(Strength, 0.0) * max(Shine, 0.0) * rim * FXTint;
)HLSL");
	auto BindFX = [&](const TCHAR* Name, UMaterialExpression* Expression)
	{
		FCustomInput& Input = FX->Inputs.AddDefaulted_GetRef();
		Input.InputName = Name;
		Input.Input.Connect(0, Expression);
	};
	BindFX(TEXT("Normal"), Nrm);
	BindFX(TEXT("View"), Cam);
	BindFX(TEXT("FXTint"), Vector(TEXT("EquipmentFXTint"), FLinearColor::White, 0, 700));
	BindFX(TEXT("Shine"), Scalar(TEXT("EquipmentFXShine"), 0.f, 0, 800));
	BindFX(TEXT("Strength"), Scalar(TEXT("EquipmentFXStrength"), 0.f, 0, 1300));
	auto* WithFX = New(UMaterialExpressionAdd::StaticClass(), 650, 500);
	Link(Out2, WithFX, TEXT("A")); Link(FX, WithFX, TEXT("B"));
	auto* FXSwitch = Cast<UMaterialExpressionStaticSwitchParameter>(New(UMaterialExpressionStaticSwitchParameter::StaticClass(), 850, 300));
	FXSwitch->ParameterName = TEXT("EquipmentFXEnabled");
	FXSwitch->DefaultValue = false;
	FXSwitch->A.Connect(0, WithFX);
	FXSwitch->B.Connect(0, Out2);
	if (!UMaterialEditingLibrary::ConnectMaterialProperty(FXSwitch, TEXT(""), MP_EmissiveColor)) { AddError(TEXT("EmissiveColor 연결 실패")); }

	UMaterialEditingLibrary::RecompileMaterial(M);
	M->PostEditChange();
	if (!UEditorLoadingAndSavingUtils::SavePackages({ Pkg }, false)) { AddError(TEXT("Toon save failed")); return false; }
	for (const TCHAR* Grade : { TEXT("Legendary"), TEXT("Ascendant") })
	{
		const FString InstanceName = FString(TEXT("MI_FunnyEquipment_")) + Grade;
		const FString InstancePath = FString(TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/")) + InstanceName;
		UPackage* InstancePackage = CreatePackage(*InstancePath);
		InstancePackage->FullyLoad();
		UMaterialInstanceConstant* Instance = FindObject<UMaterialInstanceConstant>(InstancePackage, *InstanceName);
		if (!Instance)
		{
			UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
			Instance = Cast<UMaterialInstanceConstant>(Factory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(), InstancePackage, FName(*InstanceName), RF_Public | RF_Standalone, nullptr, GWarn));
			if (Instance) { FAssetRegistryModule::AssetCreated(Instance); }
		}
		if (!Instance) { AddError(TEXT("Equipment MIC creation failed")); return false; }
		Instance->Modify();
		Instance->SetParentEditorOnly(M);
		UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(Instance, TEXT("EquipmentFXEnabled"), true);
		UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
		bool bEquipmentFXEnabled = false;
		FGuid ParameterGuid;
		if (!Instance->GetStaticSwitchParameterValue(FMaterialParameterInfo(TEXT("EquipmentFXEnabled")), bEquipmentFXEnabled, ParameterGuid) || !bEquipmentFXEnabled)
		{ AddError(TEXT("Equipment static switch readback failed")); return false; }
		Instance->PostEditChange();
		if (!UEditorLoadingAndSavingUtils::SavePackages({ InstancePackage }, false)) { AddError(TEXT("Equipment MIC save failed")); return false; }
	}
	// StaticSwitch 는 컴파일 시 결정되므로 몸과 겉옷에 각각 필요한 분기만 켠다.
	for (const bool bBodyMaterial : { false, true })
	{
		const TCHAR* InstanceName = bBodyMaterial ? TEXT("MI_FunnyBody") : TEXT("MI_FunnyGarment");
		const FString InstancePath = FString(TEXT("/Game/CompanyGrowth/Characters/FunnyWorker/Shared/")) + InstanceName;
		UPackage* InstancePackage = CreatePackage(*InstancePath);
		InstancePackage->FullyLoad();
		UMaterialInstanceConstant* Instance = FindObject<UMaterialInstanceConstant>(InstancePackage, InstanceName);
		if (!Instance)
		{
			UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
			Instance = Cast<UMaterialInstanceConstant>(Factory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(), InstancePackage, FName(InstanceName), RF_Public | RF_Standalone, nullptr, GWarn));
			if (Instance) { FAssetRegistryModule::AssetCreated(Instance); }
		}
		if (!Instance) { AddError(TEXT("Body/garment MIC creation failed")); return false; }
		Instance->Modify();
		Instance->SetParentEditorOnly(M);
		UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(Instance, TEXT("GarmentZonesEnabled"), !bBodyMaterial);
		UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(Instance, TEXT("InnerShirtEnabled"), bBodyMaterial);
		UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
		bool bZonesEnabled = false;
		FGuid ZoneGuid;
		if (!Instance->GetStaticSwitchParameterValue(FMaterialParameterInfo(TEXT("GarmentZonesEnabled")), bZonesEnabled, ZoneGuid) || bZonesEnabled != !bBodyMaterial)
		{ AddError(TEXT("Garment static switch readback failed")); return false; }
		bool bInnerEnabled = false;
		if (!Instance->GetStaticSwitchParameterValue(FMaterialParameterInfo(TEXT("InnerShirtEnabled")), bInnerEnabled, ZoneGuid) || bInnerEnabled != bBodyMaterial)
		{ AddError(TEXT("Body inner shirt static switch readback failed")); return false; }
		Instance->PostEditChange();
		if (!UEditorLoadingAndSavingUtils::SavePackages({ InstancePackage }, false)) { AddError(TEXT("Garment MIC save failed")); return false; }
	}

	TArray<FName> Sc; UMaterialEditingLibrary::GetScalarParameterNames(M, Sc);
	TArray<FName> Vc; UMaterialEditingLibrary::GetVectorParameterNames(M, Vc);
	UE_LOG(LogTemp, Display, TEXT("[Toon] ★ M_FunnyToon 저장 — scalar=%d vector=%d LightDir=%s"), Sc.Num(), Vc.Num(), *LightDir.ToString());
	TestTrue(TEXT("기본 Toon 파라미터 유지"), Sc.Contains(TEXT("Metallic")) && Sc.Contains(TEXT("TexGain")) && Vc.Contains(TEXT("TintColor")));
	TestTrue(TEXT("장비 파라미터 존재"), Sc.Contains(TEXT("EquipmentFXShine")) && Sc.Contains(TEXT("EquipmentFXStrength")) && Vc.Contains(TEXT("EquipmentFXTint")));
	TestTrue(TEXT("의상 존 파라미터 존재"), Sc.Contains(TEXT("GarmentMode")) && Sc.Contains(TEXT("GarmentZoneMin")) && Sc.Contains(TEXT("GarmentCollarTop")) && Vc.Contains(TEXT("GarmentAccent")));
	TestTrue(TEXT("이너 셔츠 파라미터 존재"), Sc.Contains(TEXT("InnerShirtHemHeight")) && Sc.Contains(TEXT("InnerShirtNeckDepth")) && Vc.Contains(TEXT("InnerShirtColor")));
	return true;
}
#endif
