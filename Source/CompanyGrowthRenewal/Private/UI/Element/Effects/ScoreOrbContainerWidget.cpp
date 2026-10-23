// Score Orb 비행 연출 컨테이너 위젯
// WBP_ScoreOrbContainer에서 머티리얼 참조, C++에서 WidgetTree 구성

#include "UI/Element/Effects/ScoreOrbContainerWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Manager/TableManagerSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Materials/MaterialInterface.h"
#include "Global/GlobalUtilFunctions.h"
#include "Components/TextBlock.h"
#include "CommonTextBlock.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/OfficeStageProgressManager.h"
#include "Manager/SettingsManagerSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

// 점수 숫자 튜닝 상수 — cpp-only 라 값 변경 시 Live Coding 주입 가능 (에디터 닫을 필요 X)
static constexpr int32 MaxScoreNumbers = 24;        // 전역 상한
static constexpr float NumberPopTime = 0.045f;
static constexpr float NumberSettleTime = 0.060f;
static constexpr float NumberHoldTime = 0.160f;
static constexpr float NumberDriftTime = 0.530f;
static constexpr float NumberFadeTime = 0.240f;
static constexpr float NumberLifetime = NumberPopTime + NumberSettleTime + NumberHoldTime + NumberDriftTime + NumberFadeTime;
static constexpr float NumberRise = 46.0f;          // 상승 픽셀
static constexpr int32 NumberFontSize = 88;
static constexpr float NumberShadowX = 0.0f;
static constexpr float NumberShadowY = 5.0f;
static constexpr float NumberShadowAlpha = 0.60f;
static constexpr float NumberPopStartScale = 0.90f;
static constexpr float NumberPopPeakScale = 1.045f;
static constexpr float CritNumberLifetime = 1.6f;
static constexpr float CritNumberRise = 120.0f;
static constexpr int32 CritNumberFontSize = 145;
static constexpr int32 CritRimAdd = 1;
// 타격 버스트 — 은은히 떠 있으면 후광이지 타격이 아니다. 확 터지고 글자보다 먼저 사라진다.
// 수명이 0.4초라 스크린샷으로는 못 잡는다. 체감 튜닝과 캡처를 위해 콘솔로 뺀다
// (유지 시간을 크게 주면 멈춰 세운 것처럼 찍을 수 있다).
static TAutoConsoleVariable<float> CVarBurstSize(
	TEXT("cg.DamageNumber.BurstSize"), 2.15f,
	TEXT("크리티컬 타격 버스트 크기 (글자 크기 배수)."), ECVF_Default);
static TAutoConsoleVariable<float> CVarBurstAlpha(
	TEXT("cg.DamageNumber.BurstAlpha"), 0.92f,
	TEXT("크리티컬 타격 버스트 최대 불투명도."), ECVF_Default);
static TAutoConsoleVariable<float> CVarBurstPop(
	TEXT("cg.DamageNumber.BurstPop"), 0.11f,
	TEXT("버스트가 다 자라는 시간(초)."), ECVF_Default);
static TAutoConsoleVariable<float> CVarBurstHold(
	TEXT("cg.DamageNumber.BurstHold"), 0.09f,
	TEXT("버스트가 꽉 찬 채 머무는 시간(초). 캡처할 때 크게 줄 것."), ECVF_Default);
static TAutoConsoleVariable<float> CVarBurstFade(
	TEXT("cg.DamageNumber.BurstFade"), 0.20f,
	TEXT("버스트가 사라지는 시간(초)."), ECVF_Default);
static TAutoConsoleVariable<int32> CVarNumberRim(
	TEXT("cg.DamageNumber.Rim"), 4,
	TEXT("숫자 바깥 검정 림 두께(px). 다음에 뜨는 숫자부터 적용."), ECVF_Default);
static TAutoConsoleVariable<int32> CVarNumberKeyline(
	TEXT("cg.DamageNumber.Keyline"), 2,
	TEXT("숫자 안쪽 흰 키라인 두께(px). 다음에 뜨는 숫자부터 적용."), ECVF_Default);
// === 글자 단위 조판 (미리 구운 스프라이트) ===
// 글자를 폰트로 그리지 않고 **작업 시점에 구운 이미지**로 조립한다.
// Slate 는 아웃라인을 FreeType 스트로커로 표시 크기에서 즉석에 만들어 아틀라스에 굽는데,
//  ① 화면 배율이 끼면 두께가 소수점이 되어 곡선마다 굵기가 흔들리고
//  ② 림/키라인/채움을 TextBlock 3장으로 쌓아야 해서 안티에일리어싱 경계가 세 번 겹친다.
// 구워 두면 목업에서 확정한 픽셀이 그대로 나가고 그릴 때는 이미지 한 장이다.
// 베이크 = Tools/DamageNumber/bake_glyph_sprites.py, 임포트 = import_glyph_sprites.py
static const TCHAR* const GlyphAssetNames[] = { TEXT("0"), TEXT("1"), TEXT("2"), TEXT("3"),
	TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"),
	TEXT("Man"), TEXT("Eok"), TEXT("Jo"), TEXT("Gyeong"), TEXT("Plus") };
static const TCHAR GlyphChars[] = TEXT("0123456789만억조경+");
static constexpr int32 GlyphCount = 15;
static constexpr int32 FirstUnitGlyph = 10;
static constexpr int32 UnitGlyphCount = 4;

// 베이크한 설계 크기 두 벌. 크리티컬이 일반의 1.66배라 한 벌로 덮으면 한쪽이 크게 리샘플된다.
static constexpr int32 GlyphBakeSizes[] = { 58, 96 };

// bake_glyph_sprites.py 가 출력한 값 — 전부 **설계 글자 크기 대비 비율**이다.
// 서체나 두께를 다시 구우면 advance·box·offset 표를 함께 갱신한다.
static constexpr float GlyphAdvanceRatio[UE_ARRAY_COUNT(GlyphBakeSizes)][GlyphCount] = {
	{ 0.7241f, 0.5862f, 0.7241f, 0.7069f, 0.7069f, 0.7241f, 0.7069f, 0.7069f, 0.7069f, 0.7069f, 0.8276f, 0.7931f, 0.7759f, 0.7759f, 0.6207f },
	{ 0.7083f, 0.5312f, 0.7083f, 0.7083f, 0.6979f, 0.7188f, 0.7083f, 0.7083f, 0.7083f, 0.7083f, 0.8021f, 0.7917f, 0.7708f, 0.7604f, 0.6250f }
};
static constexpr float GlyphBoxWRatio[UE_ARRAY_COUNT(GlyphBakeSizes)][GlyphCount] = {
	{ 1.2069f, 1.2586f, 1.2069f, 1.1897f, 1.1897f, 1.2069f, 1.1897f, 1.1897f, 1.1897f, 1.1897f, 1.3103f, 1.2759f, 1.2586f, 1.2586f, 1.1034f },
	{ 1.0000f, 1.0625f, 1.0000f, 1.0000f, 0.9896f, 1.0104f, 1.0000f, 1.0000f, 1.0000f, 1.0000f, 1.0938f, 1.0833f, 1.0625f, 1.0521f, 0.9167f }
};
static constexpr float GlyphBoxHRatio[UE_ARRAY_COUNT(GlyphBakeSizes)][GlyphCount] = {
	{ 1.2931f, 1.2586f, 1.2586f, 1.2586f, 1.2414f, 1.2586f, 1.2586f, 1.2414f, 1.2931f, 1.2586f, 1.2759f, 1.3103f, 1.2241f, 1.2586f, 1.0345f },
	{ 1.0938f, 1.0625f, 1.0625f, 1.0938f, 1.0625f, 1.0625f, 1.0833f, 1.0625f, 1.0833f, 1.0833f, 1.0521f, 1.0833f, 1.0104f, 1.0521f, 0.8333f }
};
static constexpr float GlyphOffsetXRatio[UE_ARRAY_COUNT(GlyphBakeSizes)][GlyphCount] = {
	{ 0.0000f, 0.0603f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f },
	{ 0.0000f, 0.0573f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f }
};

// 글자 높이 흔들기 — 오른쪽으로 갈수록 올라가면 줄 자체가 기울어 보인다(계단식 기각).
// 규칙이 안 보이도록 순서를 섞되 수열은 고정한다 — 같은 숫자는 늘 같은 모양이어야 한다.
static constexpr float GlyphJitter[] = { 0.15f, 0.85f, 0.35f, 1.00f, 0.50f,
	0.75f, 0.25f, 0.95f, 0.40f, 0.90f, 0.20f, 0.70f };
static constexpr float GlyphJitterAmp = 1.0f;       // 설계 픽셀 (1080p 에서 0.75px)

// 자간 — 길어질수록 좁힌다. 4자 이하는 그대로, 12자에서 16% 좁다.
static constexpr int32 TrackingStartLen = 4;
static constexpr int32 TrackingFullLen = 12;
static constexpr float TrackingMaxTighten = 0.16f;

// 단위(만/억) 좌우 여백 — 숫자와 붙어 있으면 한 덩어리로 읽힌다. 글자 크기 대비 비율.
static constexpr float UnitSideGapRatio = 0.10f;

// 채움 머티리얼이 쓰는 램프와 **같은 값이어야 한다** (Tools/DamageNumber/import_glyph_sprites.py).
static constexpr float RampSat = 2.30f;
static constexpr float RampValue = 0.49f;
static constexpr float RampTopBoost = 1.18f;
static constexpr float RampBottom = 0.14f;
static constexpr float RampGloss = 0.10f;

static FLinearColor RampMidColor(const FLinearColor& Base)
{
	const float Lum = 0.2126f * Base.R + 0.7152f * Base.G + 0.0722f * Base.B;
	auto Push = [Lum](float C) { return FMath::Clamp((Lum + (C - Lum) * RampSat) * RampValue, 0.0f, 1.0f); };
	return FLinearColor(Push(Base.R), Push(Base.G), Push(Base.B), 1.0f);
}

// 한국식 전체 표기 — 4자리마다 만/억/조/경. 콤마는 안 쓴다(단위가 구분자 역할을 한다).
// 축약(AbbreviateNumber)과 달리 자릿수를 하나도 안 버린다 — 긴 숫자 자체가 보상감이다.
static FString FormatKoreanFull(int64 Value)
{
	if (Value < 10000) { return FString::Printf(TEXT("%lld"), Value); }

	static const TCHAR* const Units[] = { TEXT(""), TEXT("만"), TEXT("억"), TEXT("조"), TEXT("경") };
	int64 Groups[UE_ARRAY_COUNT(Units)] = {};
	int32 Count = 0;
	for (int64 Rest = Value; Rest > 0 && Count < static_cast<int32>(UE_ARRAY_COUNT(Units)); ++Count)
	{
		Groups[Count] = Rest % 10000;
		Rest /= 10000;
	}

	FString Out;
	for (int32 i = Count - 1; i >= 0; --i)
	{
		if (Groups[i] == 0) { continue; }
		// 맨 앞 묶음만 그대로, 나머지는 4자리로 채운다 — 9억1234만1234 처럼 자리가 맞아야 읽힌다
		Out += (i == Count - 1) ? FString::Printf(TEXT("%lld"), Groups[i])
			: FString::Printf(TEXT("%04lld"), Groups[i]);
		Out += Units[i];
	}
	return Out;
}

static int32 FindGlyphIndex(TCHAR Ch)
{
	for (int32 i = 0; i < GlyphCount; ++i)
	{
		if (GlyphChars[i] == Ch) { return i; }
	}
	return INDEX_NONE;
}

static float TrackingFor(int32 Len)
{
	const float T = FMath::Clamp(
		static_cast<float>(Len - TrackingStartLen) / static_cast<float>(TrackingFullLen - TrackingStartLen),
		0.0f, 1.0f);
	return 1.0f - TrackingMaxTighten * T;
}

static TAutoConsoleVariable<int32> CVarNumberPopScale(
	TEXT("cg.DamageNumber.PopScale"), 1,
	TEXT("1=숫자 전체가 짧게 팝하고 원래 크기로 정착(기본). 0=배율 고정, 이동과 페이드 유지."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarNumberFreeze(
	TEXT("cg.DamageNumber.Freeze"), 0,
	TEXT("1=떠 있는 숫자의 시간을 멈춘다(시각 검수용). 0=정상."), ECVF_Default);

// ⚠ 0 초에 멈추면 버스트가 가장 작고(0.25배) 글자도 팝인 전이라 "연출이 안 나온다" 로 보인다.
//   기본값은 버스트가 다 터지고 유지에 들어간 지점 — 한 컷으로 가장 잘 보이는 순간이다.
static TAutoConsoleVariable<float> CVarNumberFreezeAt(
	TEXT("cg.DamageNumber.FreezeAt"), 0.18f,
	TEXT("Freeze 중에 고정할 경과 시간(초)."), ECVF_Default);

struct FScoreNumberMotion
{
	float OffsetY = 0.0f;
	float Scale = 1.0f;
	float Opacity = 1.0f;
};

static FScoreNumberMotion EvaluateScoreNumberMotion(float Elapsed, float Lifetime, float RiseDistance,
	bool bPopScale, bool bReduceMotion)
{
	FScoreNumberMotion Motion;
	const float SettleEnd = NumberPopTime + NumberSettleTime;
	const float HoldEnd = SettleEnd + NumberHoldTime;
	const float FadeStart = Lifetime - NumberFadeTime;
	const float DriftRise = RiseDistance * (24.0f / 46.0f);
	Motion.Opacity = FMath::Clamp(Elapsed / 0.020f, 0.0f, 1.0f);

	if (Elapsed < NumberPopTime)
	{
		const float Ease = FMath::InterpEaseOut(0.0f, 1.0f,
			FMath::Clamp(Elapsed / NumberPopTime, 0.0f, 1.0f), 3.0f);
		Motion.OffsetY = FMath::Lerp(6.0f, -3.0f, Ease);
		Motion.Scale = FMath::Lerp(NumberPopStartScale, NumberPopPeakScale, Ease);
	}
	else if (Elapsed < SettleEnd)
	{
		const float Ease = FMath::InterpEaseOut(0.0f, 1.0f,
			(Elapsed - NumberPopTime) / NumberSettleTime, 3.0f);
		Motion.OffsetY = FMath::Lerp(-3.0f, 0.0f, Ease);
		Motion.Scale = FMath::Lerp(NumberPopPeakScale, 1.0f, Ease);
	}
	else if (Elapsed >= FadeStart)
	{
		const float Fade = FMath::Square(FMath::Clamp((Elapsed - FadeStart) / NumberFadeTime, 0.0f, 1.0f));
		Motion.OffsetY = -DriftRise - (RiseDistance - DriftRise) * Fade;
		Motion.Opacity = 1.0f - Fade;
	}
	else if (Elapsed >= HoldEnd)
	{
		const float Drift = (Elapsed - HoldEnd) / (FadeStart - HoldEnd);
		Motion.OffsetY = -DriftRise * Drift * Drift;
	}

	if (!bPopScale || bReduceMotion) { Motion.Scale = 1.0f; }
	if (bReduceMotion) { Motion.OffsetY = 0.0f; }
	return Motion;
}


static constexpr int32 CritSparkleCount = 2;
static const FLinearColor CritSparkTint(1.00f, 0.95f, 0.62f, 1.0f);
static const FLinearColor CritNumberColor(1.00f, 0.84f, 0.20f, 1.0f);
static constexpr int32 MaxConfettiPieces = 90;      // 모바일 상한
static constexpr float ConfettiGravity = 1800.0f;
static constexpr float ConfettiFadeTail = 0.4f;
// MedalBurst 튜닝 (cpp-only = Live Coding 주입 가능)
static constexpr float MedalBurstSpawnRadiusMin = 170.0f;   // 메달 뒤 원환 스폰 (중앙 숫자 가림 방지)
static constexpr float MedalBurstSpawnRadiusMax = 240.0f;
static constexpr float MedalBurstSpeedMin = 420.0f;
static constexpr float MedalBurstSpeedMax = 1150.0f;
static constexpr float MedalBurstStarRatio = 0.35f;         // 스타 조각 비율 (나머지 = 종이)
static constexpr float StampPopInTime = 0.22f;
static constexpr float StampHoldTime = 0.5f;
static constexpr float StampFadeTime = 0.3f;

// === Orb 산출음 (DT_GameSFX 행 이름 — 이웃 연출음 Event_ScoreSweep/Dev_Crit 과 같은 경로) ===
// 소리 교체는 이 배열만: 세라믹 톡으로 바꾸려면 { TEXT("ScoreOrb_Land_1"), TEXT("ScoreOrb_Land_2") }
static const FName ScoreOrbSoundTags[] = { TEXT("Event_ScoreSweep") };
// Event_ScoreSweep 행(0.55)은 크리/출시결과 리빌과 공유 — DT 를 낮추면 그쪽까지 죽으므로 여기서만 스케일
static constexpr float ScoreOrbSoundVolumeScale = 0.5f;
static constexpr float ScoreOrbSoundPitchJitter = 0.05f;   // 초당 반복이라 무피치변화면 기계음으로 들림
static constexpr float ScoreOrbSoundMinGap = 0.25f;        // 샘플이 0.55s — 간격이 짧으면 겹쳐서 뭉개짐

// 버스트당 1회, **스폰 시점**. 착지(비행 1.0s 뒤)에 걸면 "+N" 숫자 팝과 1초 어긋나 늦게 들림.
// 스로틀 상태를 멤버 대신 file-scope 로 둔 이유: 컨테이너가 Office 당 1개라 멤버와 동등하고,
// 헤더 불변 = 값 튜닝을 Live Coding 으로 주입 가능.
static void PlayScoreOrbSound(UWorld* World)
{
	if (!World) return;

	static float LastPlayTime = -1000.0f;
	const float Now = World->GetTimeSeconds();
	// PIE 재시작으로 월드시간이 되감기면 스로틀이 영구 잠기므로 리셋
	if (Now < LastPlayTime) { LastPlayTime = -1000.0f; }
	if (Now - LastPlayTime < ScoreOrbSoundMinGap) return;
	LastPlayTime = Now;

	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return;

	if (USoundManagerSubsystem* SoundMgr = GI->GetSubsystem<USoundManagerSubsystem>())
	{
		const int32 Idx = FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(ScoreOrbSoundTags)) - 1);
		SoundMgr->PlaySoundWithVolume(ScoreOrbSoundTags[Idx], ScoreOrbSoundVolumeScale,
			FMath::FRandRange(1.0f - ScoreOrbSoundPitchJitter, 1.0f + ScoreOrbSoundPitchJitter));
	}
}

bool UScoreOrbContainerWidget::Initialize()
{
	bool bSuccess = Super::Initialize();
	if (!bSuccess) return false;

	OrbCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("OrbCanvas"));
	if (OrbCanvas)
	{
		WidgetTree->RootWidget = OrbCanvas;
	}

	// WBP의 UPROPERTY에 지정된 머티리얼을 로드 (에디터 참조 → 쿠킹 자동 포함)
	for (int32 i = 0; i < FMath::Min(OrbMaterialRefs.Num(), 3); ++i)
	{
		if (!OrbMaterialRefs[i].IsNull())
		{
			CachedOrbMaterials[i] = OrbMaterialRefs[i].LoadSynchronous();
		}
	}

	// 글리프 스프라이트 합성 머티리얼 (R=채움 / G=흰 키라인 / B=검정 림).
	// /Game/CompanyGrowth/UI/Materials 는 쿠킹 등록 폴더다.
	GlyphMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/UI/Materials/DamageNumber/M_UI_DamageGlyph.M_UI_DamageGlyph"))).LoadSynchronous();
	NumberLabelStyle = TSoftClassPtr<UCommonTextStyle>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/UI/CommonStyle/Text/CUI_Style_Text_Heading_M.CUI_Style_Text_Heading_M_C"))).LoadSynchronous();
	CriticalNumberLabelStyle = TSoftClassPtr<UCommonTextStyle>(FSoftObjectPath(
		TEXT("/Game/CompanyGrowth/UI/CommonStyle/Text/CUI_Style_Text_Display_M.CUI_Style_Text_Display_M_C"))).LoadSynchronous();
	if (!NumberLabelStyle || !CriticalNumberLabelStyle)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ScoreOrb] 점수 직능 라벨의 공용 텍스트 스타일을 로드하지 못했습니다"));
	}

	return bSuccess;
}

UMaterialInstanceDynamic* UScoreOrbContainerWidget::GetGlyphMID(int32 GlyphIndex, int32 ColorIndex,
	int32 SizeIndex)
{
	if (!GlyphMaterial) { return nullptr; }

	const int32 Key = (SizeIndex * 8 + ColorIndex) * 64 + GlyphIndex;
	if (UMaterialInstanceDynamic** Found = GlyphMIDs.Find(Key))
	{
		return *Found;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(GlyphMaterial, this);
	if (!MID) { return nullptr; }

	const FString TexPath = FString::Printf(
		TEXT("/Game/CompanyGrowth/UI/Textures/Effect/DamageGlyph/%d/T_UI_DmgGlyph_%s.T_UI_DmgGlyph_%s"),
		GlyphBakeSizes[SizeIndex], GlyphAssetNames[GlyphIndex], GlyphAssetNames[GlyphIndex]);
	if (UTexture2D* Tex = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TexPath)).LoadSynchronous())
	{
		MID->SetTextureParameterValue(TEXT("Glyph"), Tex);
	}

	// 색은 스프라이트가 아니라 머티리얼이 준다 — 스프라이트는 마스크 3장뿐이다
	const bool bCrit = (ColorIndex == 6);
	const FLinearColor Base = bCrit ? CritNumberColor : UGlobalUtilFunctions::GetStepColor(ColorIndex + 1);
	const FLinearColor Mid = RampMidColor(Base);
	auto Up = [](float C) { return FMath::Min(C * RampTopBoost, 1.0f); };

	MID->SetVectorParameterValue(TEXT("TopColor"), FLinearColor(Up(Mid.R), Up(Mid.G), Up(Mid.B), 1.0f));
	MID->SetVectorParameterValue(TEXT("MidColor"), Mid);
	MID->SetVectorParameterValue(TEXT("BottomColor"),
		FLinearColor(Mid.R * RampBottom, Mid.G * RampBottom, Mid.B * RampBottom, 1.0f));
	MID->SetScalarParameterValue(TEXT("GlossStrength"), RampGloss);

	GlyphMIDs.Add(Key, MID);
	return MID;
}

UOverlay* UScoreOrbContainerWidget::BuildNumberRow(const FString& Text, int32 StepNumber,
	bool bCritical, int32 FontSize, int32 Id, FVector2D& OutSize, float& OutFirstGlyphX)
{
	if (!GlyphMaterial || Text.IsEmpty()) { return nullptr; }

	// 베이크한 두 벌 중 표시 크기에 맞는 쪽을 쓴다 — 리샘플 배율을 1 에 가깝게 유지한다
	const int32 SizeIndex = bCritical ? 1 : 0;
	const int32 ColorIndex = bCritical ? 6 : FMath::Clamp(StepNumber - 1, 0, 5);
	const float Scale = static_cast<float>(FontSize) / static_cast<float>(GlyphBakeSizes[SizeIndex]);

	// 1) 쓸 글자와 어드밴스를 먼저 모아 전체 폭을 구한다 (가운데 정렬 기준점이 필요하다).
	//    단위는 좌우로 여백을 더해 숫자와 떼어 놓는다 — 붙어 있으면 한 덩어리로 읽힌다.
	const bool bHasGainPrefix = Text.StartsWith(TEXT("+"));
	const float Tracking = TrackingFor(Text.Len() - (bHasGainPrefix ? 1 : 0));
	const float UnitGap = UnitSideGapRatio * FontSize;
	TArray<int32> Indices;
	TArray<float> Advances;
	TArray<float> Leads;
	Indices.Reserve(Text.Len());
	float TotalW = 0.0f;
	for (int32 i = 0; i < Text.Len(); ++i)
	{
		const int32 G = FindGlyphIndex(Text[i]);
		if (G == INDEX_NONE) { continue; }
		const bool bUnit = (G >= FirstUnitGlyph && G < FirstUnitGlyph + UnitGlyphCount);
		const float Glyph = GlyphAdvanceRatio[SizeIndex][G] * FontSize * Tracking;
		// 줄 양 끝에는 여백을 안 붙인다 — 가운데 정렬이 어긋난다
		const float Lead = (bUnit && i > 0) ? UnitGap : 0.0f;
		const float Trail = (bUnit && i < Text.Len() - 1) ? UnitGap : 0.0f;
		Indices.Add(G);
		Advances.Add(Glyph + Lead + Trail);
		Leads.Add(Lead + Glyph * 0.5f);
		TotalW += Advances.Last();
	}
	if (Indices.Num() == 0) { return nullptr; }

	UOverlay* Row = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), *FString::Printf(TEXT("ScoreNumRow_%d"), Id));
	if (!Row) { return nullptr; }

	// 2) 전부 가운데 정렬로 쌓고 렌더 트랜슬레이션으로만 벌린다.
	//    펜은 어드밴스만큼 전진하지만 스프라이트 박스는 좌우 여백만큼 더 넓어서, 이웃끼리
	//    그 여백에서 겹치고 검정 림이 끊기지 않고 이어진다.
	//    ⚠ HorizontalBox + 음수 패딩으로 겹치려 하면 안 된다 — 음수 패딩은 "겹침"이 아니라
	//      "할당 폭 축소"로 읽혀 Auto 크기 자식이 가로로 눌린다(2026-10-03 실측).
	float Pen = 0.0f;
	float MinY = 0.0f;
	float MaxY = 0.0f;
	float MaxBoxH = 0.0f;
	int32 Placed = 0;
	for (int32 i = 0; i < Indices.Num(); ++i)
	{
		const int32 G = Indices[i];
		const float BoxW = GlyphBoxWRatio[SizeIndex][G] * FontSize;
		const float BoxH = GlyphBoxHRatio[SizeIndex][G] * FontSize;
		MaxBoxH = FMath::Max(MaxBoxH, BoxH);

		UImage* Img = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), *FString::Printf(TEXT("ScoreGlyph_%d_%d"), Id, i));
		UMaterialInstanceDynamic* MID = GetGlyphMID(G, ColorIndex, SizeIndex);
		if (!Img || !MID)
		{
			Pen += Advances[i];
			continue;
		}

		Img->SetBrushFromMaterial(MID);
		// 브러시 ImageSize 를 직접 박는다 — SetDesiredSizeOverride 만으로는 내부 SImage 가
		// 아직 없을 때 조용히 버려진다 (CoinFlyout 이 쓰는 패턴)
		FSlateBrush Brush = Img->GetBrush();
		Brush.ImageSize = FVector2D(BoxW, BoxH);
		Img->SetBrush(Brush);
		Img->SetDesiredSizeOverride(Brush.ImageSize);
		Img->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		const int32 JitterIndex = FMath::Max(0, i - (bHasGainPrefix ? 1 : 0));
		const float Y = -GlyphJitterAmp * GlyphJitter[JitterIndex % static_cast<int32>(UE_ARRAY_COUNT(GlyphJitter))];
		const float X = Pen + Leads[i] - TotalW * 0.5f;
		Img->SetRenderTranslation(FVector2D(X + GlyphOffsetXRatio[SizeIndex][G] * FontSize, Y));
		if (UOverlaySlot* GlyphSlot = Row->AddChildToOverlay(Img))
		{
			GlyphSlot->SetHorizontalAlignment(HAlign_Center);
			GlyphSlot->SetVerticalAlignment(VAlign_Center);
		}

		// 타격 버스트는 부호 다음의 첫 숫자를 중심으로 터진다.
		if (Placed == 0 || (bHasGainPrefix && i == 1)) { OutFirstGlyphX = X; }
		MinY = FMath::Min(MinY, Y);
		MaxY = FMath::Max(MaxY, Y);
		Pen += Advances[i];
		++Placed;
	}

	if (Placed == 0)
	{
		Row->RemoveFromParent();
		return nullptr;
	}

	(void)Scale;
	OutSize = FVector2D(TotalW, MaxBoxH + (MaxY - MinY));
	return Row;
}

FVector2D UScoreOrbContainerWidget::ResolveNumberOverlap(FVector2D Desired, int64 Amount, float Width, float Height)
{
	// 숫자들은 같은 속도로 떠오르므로 시작 위치만 떨어뜨려 두면 수명 내내 간격이 유지된다.
	// 한 번에 확정한다 — 겹칠 때마다 조금씩 미는 반복법은 셋 이상이 동시에 뜨면 수렴하지 못해
	// 결국 두 개가 붙은 채 남는다 (2026-10-01 실캡처로 확인).
	const float GapY = Height * 1.06f;

	float TopY = Desired.Y;
	bool bColumnBusy = false;
	for (const FScoreNumberAnimData& Other : ActiveNumbers)
	{
		// 가로로 안 겹치면 같은 열이 아니다
		if (FMath::Abs(Other.StartPos.X - Desired.X) >= (Width + Other.Width) * 0.5f) { continue; }
		// 내 자리보다 한참 위에 있는 것은 이미 비켜난 것이라 셈에서 뺀다
		if (Other.StartPos.Y > Desired.Y + GapY) { continue; }
		bColumnBusy = true;
		TopY = FMath::Min(TopY, Other.StartPos.Y);
	}
	if (bColumnBusy)
	{
		// 그 열의 가장 위 숫자보다 한 칸 더 위 — 몇 개가 동시에 와도 겹칠 수 없다
		Desired.Y = TopY - GapY;
	}
	return Desired;
}


FGeometry UScoreOrbContainerWidget::GetCanvasGeometry() const
{
	if (OrbCanvas)
	{
		return OrbCanvas->GetCachedGeometry();
	}
	return FGeometry();
}

void UScoreOrbContainerWidget::SpawnOrbBurst(FVector2D StartPos, FVector2D TargetPos,
	int32 StepNumber, bool bIsCritical, int32 OrbCount)
{
	if (!OrbCanvas) return;

	// 크리는 EmployeeBehaviorComponent 가 같은 시점에 Event_ScoreSweep 을 울림 (한 액션 한 사운드)
	if (!bIsCritical)
	{
		PlayScoreOrbSound(GetWorld());
	}

	// 수익 패킷이 나눠져도 호출자가 배정한 전체 시각 예산을 유지한다.
	for (int32 i = 0; i < OrbCount; ++i)
	{
		// 시작 위치를 약간씩 랜덤 오프셋 (퍼지는 느낌)
		FVector2D OffsetStart = StartPos + FVector2D(
			FMath::FRandRange(-BurstSpreadRadius, BurstSpreadRadius),
			FMath::FRandRange(-BurstSpreadRadius, BurstSpreadRadius));

		// 시차: 앞쪽 구슬부터 순서대로
		float Delay = i * BurstDelayPerOrb;

		SpawnSingleOrb(OffsetStart, TargetPos, StepNumber, bIsCritical, Delay, /*bNotifyArrival*/ i == 0);
	}
}

void UScoreOrbContainerWidget::SpawnEventOrbBurst(FVector2D ScreenCenter, FVector2D TargetPos,
	int32 StepNumber, int32 OrbCount)
{
	if (!OrbCanvas || OrbCount <= 0) return;

	int32 MatIndex = FMath::Clamp(StepNumber - 1, 0, 2);
	UMaterialInterface* OrbMat = CachedOrbMaterials[MatIndex];

	for (int32 i = 0; i < OrbCount; ++i)
	{
		// Phase 1 종료 위치: 화면 중앙에서 EventPopOutRadius 반경의 랜덤 방향
		float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		float Radius = EventPopOutRadius * FMath::FRandRange(0.5f, 1.0f);
		FVector2D PopOutEnd = ScreenCenter + FVector2D(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);

		// === 글로우 후광 ===
		int32 Id = OrbIdCounter++;
		UImage* GlowImg = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("EventOrbGlow_%d"), Id));
		if (GlowImg)
		{
			if (OrbMat) GlowImg->SetBrushFromMaterial(OrbMat);
			float GlowSize = OrbSize * EventOrbScale * GlowSizeMultiplier;
			GlowImg->SetDesiredSizeOverride(FVector2D(GlowSize, GlowSize));
			GlowImg->SetRenderScale(FVector2D(0.0f, 0.0f));
			GlowImg->SetRenderOpacity(0.0f);

			UCanvasPanelSlot* GlowSlot = OrbCanvas->AddChildToCanvas(GlowImg);
			if (GlowSlot)
			{
				GlowSlot->SetAutoSize(true);
				GlowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				GlowSlot->SetPosition(ScreenCenter);
			}
		}

		// === 메인 구슬 ===
		UImage* OrbImg = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("EventOrb_%d"), Id));
		if (!OrbImg) continue;

		if (OrbMat)
		{
			OrbImg->SetBrushFromMaterial(OrbMat);
		}
		else
		{
			FSlateBrush FallbackBrush;
			FallbackBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			FallbackBrush.TintColor = FSlateColor(UGlobalUtilFunctions::GetStepColor(StepNumber));
			FallbackBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
			OrbImg->SetBrush(FallbackBrush);
		}

		float ActualSize = OrbSize * EventOrbScale;
		OrbImg->SetDesiredSizeOverride(FVector2D(ActualSize, ActualSize));
		OrbImg->SetRenderScale(FVector2D(0.0f, 0.0f));
		OrbImg->SetRenderOpacity(0.0f);

		UCanvasPanelSlot* CanvasSlot = OrbCanvas->AddChildToCanvas(OrbImg);
		if (CanvasSlot)
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetPosition(ScreenCenter);
		}

		FOrbAnimData AnimData;
		AnimData.OrbImage = OrbImg;
		AnimData.GlowImage = GlowImg;
		AnimData.OrbSlot = CanvasSlot;
		AnimData.GlowSlot = GlowImg ? Cast<UCanvasPanelSlot>(GlowImg->Slot) : nullptr;
		AnimData.StartPos = ScreenCenter;
		AnimData.FlightStartPos = ScreenCenter;
		AnimData.TargetPos = TargetPos;
		AnimData.PopOutTargetPos = PopOutEnd;
		AnimData.Delay = i * EventStaggerDelay;
		AnimData.PopOutDuration = EventPopOutDuration;
		AnimData.Duration = EventFlightDuration + FMath::FRandRange(-0.1f, 0.1f);
		AnimData.ArcHeight = EventArcHeight + FMath::FRandRange(-15.f, 15.f);
		AnimData.Scale = EventOrbScale;
		AnimData.WobbleOffset = WobbleBase * 0.6f + FMath::FRandRange(-5.f, 5.f);
		AnimData.bEventStyle = true;
		AnimData.bPopOutDone = false;
		AnimData.StepNumber = StepNumber;
		AnimData.bNotifyArrival = (i == 0);   // 이벤트 orb 착지도 동일 정산 경로
		ActiveOrbs.Add(AnimData);
	}
}

void UScoreOrbContainerWidget::SpawnSingleOrb(FVector2D StartPos, FVector2D TargetPos,
	int32 StepNumber, bool bIsCritical, float Delay, bool bNotifyArrival)
{
	if (!OrbCanvas) return;

	int32 MatIndex = FMath::Clamp(StepNumber - 1, 0, 2);
	UMaterialInterface* OrbMat = CachedOrbMaterials[MatIndex];
	int32 Id = OrbIdCounter++;

	// === 글로우 후광 (먼저 추가 = 뒤에 그려짐, 같은 머티리얼 사용) ===
	UImage* GlowImg = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		*FString::Printf(TEXT("OrbGlow_%d"), Id));

	if (GlowImg)
	{
		if (OrbMat)
		{
			GlowImg->SetBrushFromMaterial(OrbMat);
		}

		float GlowSize = OrbSize * GlowSizeMultiplier;
		GlowImg->SetDesiredSizeOverride(FVector2D(GlowSize, GlowSize));
		GlowImg->SetRenderScale(FVector2D(0.0f, 0.0f));
		GlowImg->SetRenderOpacity(0.0f);

		UCanvasPanelSlot* GlowSlot = OrbCanvas->AddChildToCanvas(GlowImg);
		if (GlowSlot)
		{
			GlowSlot->SetAutoSize(true);
			GlowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			GlowSlot->SetPosition(StartPos);
		}
	}

	// === 메인 구슬 ===
	UImage* OrbImg = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		*FString::Printf(TEXT("Orb_%d"), Id));
	if (!OrbImg) return;

	if (OrbMat)
	{
		OrbImg->SetBrushFromMaterial(OrbMat);
	}
	else
	{
		// 머티리얼 미로드 시 단색 원형 폴백
		FSlateBrush FallbackBrush;
		FallbackBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		FallbackBrush.TintColor = FSlateColor(UGlobalUtilFunctions::GetStepColor(StepNumber));
		FallbackBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		OrbImg->SetBrush(FallbackBrush);
	}

	float ActualSize = bIsCritical ? OrbSize * CriticalScaleMultiplier : OrbSize;
	OrbImg->SetDesiredSizeOverride(FVector2D(ActualSize, ActualSize));
	OrbImg->SetRenderScale(FVector2D(0.0f, 0.0f));
	OrbImg->SetRenderOpacity(0.0f);

	UCanvasPanelSlot* CanvasSlot = OrbCanvas->AddChildToCanvas(OrbImg);
	if (CanvasSlot)
	{
		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetPosition(StartPos);
	}

	FOrbAnimData AnimData;
	AnimData.OrbImage = OrbImg;
	AnimData.GlowImage = GlowImg;
	AnimData.OrbSlot = CanvasSlot;
	AnimData.GlowSlot = GlowImg ? Cast<UCanvasPanelSlot>(GlowImg->Slot) : nullptr;
	AnimData.StartPos = StartPos;
	AnimData.FlightStartPos = StartPos;
	AnimData.TargetPos = TargetPos;
	AnimData.Delay = Delay;
	AnimData.Duration = FlightDuration + FMath::FRandRange(-0.08f, 0.08f);
	AnimData.ArcHeight = BaseArcHeight + FMath::FRandRange(-20.f, 20.f);
	AnimData.Scale = bIsCritical ? CriticalScaleMultiplier : 1.0f;
	AnimData.WobbleOffset = WobbleBase + FMath::FRandRange(-8.f, 8.f);
	AnimData.StepNumber = StepNumber;
	AnimData.bIsCritical = bIsCritical;
	AnimData.bNotifyArrival = bNotifyArrival;

	ActiveOrbs.Add(AnimData);
}

UCanvasPanelSlot* UScoreOrbContainerWidget::AddNumberProbeSwatch(UWidget* Probe)
{
	return (OrbCanvas && Probe) ? OrbCanvas->AddChildToCanvas(Probe) : nullptr;
}

void UScoreOrbContainerWidget::SpawnScoreNumber(FVector2D StartPos, int64 Amount, int32 StepNumber, bool bIsCritical)
{
	if (!OrbCanvas || Amount <= 0) return;

	// 전역 상한 — 오래된 것부터 제거
	while (ActiveNumbers.Num() >= MaxScoreNumbers)
	{
		if (ActiveNumbers[0].Root)
		{
			ActiveNumbers[0].Root->RemoveFromParent();
		}
		ActiveNumbers.RemoveAt(0);
	}

	const int32 Id = OrbIdCounter++;
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), *FString::Printf(TEXT("ScoreNum_%d"), Id));
	if (!Root) return;

	// 증가량을 +로 구분하고 만/억 단위 안의 자릿수는 생략하지 않는다.
	const FString AmountText = TEXT("+") + FormatKoreanFull(Amount);

	const int32 FontSize = bIsCritical ? CritNumberFontSize : NumberFontSize;

	FScoreNumberAnimData Data;
	Data.bCritical = bIsCritical;

	// === 크리티컬 배후광 + 반짝이 — 글자보다 먼저 넣어야 뒤로 깔린다 ===
	if (bIsCritical)
	{
		UGameInstance* GI = GetGameInstance();
		UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
		if (TableMgr)
		{
			UTexture2D* BurstTex = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
				TEXT("/Game/CompanyGrowth/UI/Textures/Effect/Impact/T_UI_ImpactBurst.T_UI_ImpactBurst"))).LoadSynchronous();

			// 텍스처가 흰 속 + 노란 테두리를 이미 갖고 있다. 그 흰 속이 숫자 받침이 되므로
			// 뒤에 테두리를 따로 깔지 않고, 틴트도 주지 않는다(흰 속이 직능색으로 물든다).
			const float BurstSize = FontSize * CVarBurstSize.GetValueOnGameThread();
			auto MakeBurstLayer = [&](const TCHAR* NamePrefix, float SizeMul, const FLinearColor& Tint) -> UImage*
			{
				if (!BurstTex) return nullptr;
				UImage* Img = WidgetTree->ConstructWidget<UImage>(
					UImage::StaticClass(), *FString::Printf(TEXT("%s_%d"), NamePrefix, Id));
				if (!Img) return nullptr;

				Img->SetBrushFromTexture(BurstTex);
				// 브러시 ImageSize 를 직접 박아야 한다 — SetDesiredSizeOverride 는 내부 SImage 가
				// 아직 없으면 조용히 버려지고, SetBrushFromTexture 는 bMatchSize 기본값이 false 라
				// ImageSize 가 UImage 기본 32x32 로 남는다 (CoinFlyout 이 쓰는 패턴).
				FSlateBrush Brush = Img->GetBrush();
				Brush.ImageSize = FVector2D(BurstSize * SizeMul, BurstSize * SizeMul);
				Img->SetBrush(Brush);
				Img->SetDesiredSizeOverride(Brush.ImageSize);
				Img->SetColorAndOpacity(Tint);
				Img->SetRenderOpacity(0.0f);
				Img->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				if (UOverlaySlot* BurstSlot = Root->AddChildToOverlay(Img))
				{
					BurstSlot->SetHorizontalAlignment(HAlign_Center);
					BurstSlot->SetVerticalAlignment(VAlign_Center);
				}
				return Img;
			};

			Data.Burst = MakeBurstLayer(TEXT("ScoreBurst"), 1.0f, FLinearColor::White);
			// 매번 같은 각도면 반복이 눈에 띈다 — 갈래가 많아 30도 안에서만 흔들어도 충분히 달라 보인다
			Data.BurstAngle = FMath::FRandRange(-15.0f, 15.0f);

			for (int32 s = 0; s < CritSparkleCount; ++s)
			{
				UImage* Spark = WidgetTree->ConstructWidget<UImage>(
					UImage::StaticClass(), *FString::Printf(TEXT("ScoreSpark_%d_%d"), Id, s));
				if (!Spark || !TableMgr->ApplyUIVFXTexture(TEXT("SparkleMain"), Spark))
				{
					continue;
				}
				const float SparkSize = FontSize * ((s % 2) ? 0.44f : 0.62f);
				FSlateBrush SparkBrush = Spark->GetBrush();
				SparkBrush.ImageSize = FVector2D(SparkSize, SparkSize);
				Spark->SetBrush(SparkBrush);
				Spark->SetDesiredSizeOverride(FVector2D(SparkSize, SparkSize));
				Spark->SetColorAndOpacity(CritSparkTint);
				Spark->SetRenderOpacity(0.0f);
				Spark->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				// Overlay 는 정렬만 주므로 자리 이동은 렌더 트랜슬레이션으로 — 레이아웃을 다시 잡지 않는다
				const float Angle = FMath::DegreesToRadians(-42.0f + s * 128.0f);
				Spark->SetRenderTranslation(
					FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * FontSize * 0.78f);
				if (UOverlaySlot* SparkSlot = Root->AddChildToOverlay(Spark))
				{
					SparkSlot->SetHorizontalAlignment(HAlign_Center);
					SparkSlot->SetVerticalAlignment(VAlign_Center);
				}
				Data.Sparks.Add(Spark);
			}
		}
	}

	// === 숫자 본체 ===
	// 글자마다 따로 세운다 — 높이를 글자별로 흔들고(들쭉날쭉) 한글 단위에만 볼드를 걸려면
	// 한 TextBlock 으로는 안 된다. 림은 수익 숫자(36px/림4)와 같은 절대 두께로 맞췄다:
	// 메이플 룩을 내려고 6~9 로 밀었더니 모서리가 뭉개고 8·0 의 구멍이 메워졌다.
	FVector2D RowSize = FVector2D(FontSize, FontSize);
	float FirstGlyphX = 0.0f;
	UOverlay* NumberRow = BuildNumberRow(AmountText, StepNumber, bIsCritical, FontSize, Id,
		RowSize, FirstGlyphX);
	if (!NumberRow)
	{
		Root->RemoveFromParent();
		return;
	}
	if (UOverlaySlot* RowSlot = Root->AddChildToOverlay(NumberRow))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Center);
		RowSlot->SetVerticalAlignment(VAlign_Center);
	}

	// 활성 스텝은 weight>0 직능만 들어가므로 StepNumber를 직능 슬롯으로 간주하면 안 된다.
	FText DisciplineName;
	const UOfficeStageProgressManager* ProgressMgr = GetWorld()
		? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
	const UTableManagerSubsystem* DisplayTables = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (ProgressMgr && DisplayTables)
	{
		const FStageProgressData& Progress = ProgressMgr->GetProgressData();
		if (Progress.Steps.IsValidIndex(StepNumber - 1))
		{
			const int32 DisciplineSlot = Progress.Steps[StepNumber - 1].DisciplineSlot;
			if (DisciplineSlot != INDEX_NONE)
			{
				DisciplineName = DisplayTables->GetDisciplineDisplayName(Progress.CompanyType, DisciplineSlot);
			}
		}
	}
	float NumberOffsetX = 0.0f;
	const TSubclassOf<UCommonTextStyle> LabelStyle = bIsCritical ? CriticalNumberLabelStyle : NumberLabelStyle;
	if (!DisciplineName.IsEmpty() && LabelStyle)
	{
		UCommonTextBlock* Label = WidgetTree->ConstructWidget<UCommonTextBlock>(
			UCommonTextBlock::StaticClass(), *FString::Printf(TEXT("ScoreDiscipline_%d"), Id));
		Label->SetStyle(LabelStyle);
		// 공용 폰트를 복사한 뒤 연결을 풀어 로컬 림/그림자가 UpdateFromStyle에 덮이지 않게 한다.
		Label->SetStyle(nullptr);
		Label->SetText(DisciplineName);
		Label->SetAutoWrapText(false);
		Label->SetMargin(FMargin(0.0f));
		FSlateFontInfo LabelFont = Label->GetFont();
		const float LabelScale = static_cast<float>(FontSize) / GlyphBakeSizes[bIsCritical ? 1 : 0];
		LabelFont.Size = FMath::RoundToInt(LabelFont.Size * LabelScale);
		LabelFont.OutlineSettings.OutlineSize = 2;
		LabelFont.OutlineSettings.OutlineColor = FLinearColor(0.025f, 0.03f, 0.04f, 1.0f);
		Label->SetFont(LabelFont);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.95f, 1.0f, 1.0f)));
		Label->SetShadowOffset(FVector2D(0.0f, 2.0f));
		Label->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* LabelSlot = Root->AddChildToOverlay(Label))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		// 실제 Slate 레이아웃을 재서 한 덩어리의 가운데를 맞춘다. 글자 수로 폭을 추정하지 않는다.
		Label->TakeWidget();
		Label->ForceLayoutPrepass();
		const FVector2D LabelSize = Label->GetDesiredSize();
		const float LabelGap = 8.0f * LabelScale;
		NumberOffsetX = (LabelSize.X + LabelGap) * 0.5f;
		Label->SetRenderTranslation(FVector2D(-(RowSize.X + LabelGap) * 0.5f, 0.0f));
		NumberRow->SetRenderTranslation(FVector2D(NumberOffsetX, 0.0f));
		RowSize.X += LabelSize.X + LabelGap;
		RowSize.Y = FMath::Max(RowSize.Y, LabelSize.Y);
	}

	// 버스트는 맨 왼쪽 글자 위에 — 숫자 전체 뒤 중앙에 깔면 터진 지점이 안 읽힌다.
	// 숫자보다 먼저 추가돼 뒤에 깔려 있으므로 자리만 옮긴다.
	if (Data.Burst)
	{
		Data.Burst->SetRenderTranslation(FVector2D(FirstGlyphX + NumberOffsetX, 0.0f));
	}
	for (UImage* Spark : Data.Sparks)
	{
		Spark->SetRenderTranslation(Spark->GetRenderTransform().Translation + FVector2D(NumberOffsetX, 0.0f));
	}

	UCanvasPanelSlot* NumSlot = OrbCanvas->AddChildToCanvas(Root);
	if (!NumSlot)
	{
		Root->RemoveFromParent();
		return;
	}
	// 매 틱 같은 자리 스트로브 방지 — 시작점 소량 지터
	FVector2D Start = StartPos + FVector2D(
		FMath::FRandRange(-22.0f, 22.0f), FMath::FRandRange(-10.0f, 6.0f));

	// 조립하면서 잰 실측 크기를 쓴다 — 자릿수로 추정하면 한글 단위가 섞인 줄에서 어긋난다
	Start = ResolveNumberOverlap(Start, Amount, RowSize.X, RowSize.Y);

	NumSlot->SetAutoSize(true);
	NumSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	NumSlot->SetPosition(Start);

	Data.Root = Root;
	Data.Slot = NumSlot;
	Data.StartPos = Start;
	Data.Amount = Amount;
	Data.Width = RowSize.X;
	Data.Lifetime = bIsCritical ? CritNumberLifetime : NumberLifetime;
	Data.RiseDistance = bIsCritical ? CritNumberRise : NumberRise;
	const FScoreNumberMotion InitialMotion = EvaluateScoreNumberMotion(0.0f, Data.Lifetime, Data.RiseDistance,
		CVarNumberPopScale.GetValueOnGameThread() > 0, USettingsManagerSubsystem::IsReduceMotion(this));
	Root->SetRenderTranslation(FVector2D(0.0f, InitialMotion.OffsetY));
	Root->SetRenderScale(FVector2D(InitialMotion.Scale, InitialMotion.Scale));
	Root->SetRenderOpacity(InitialMotion.Opacity);
	ActiveNumbers.Add(Data);
}

void UScoreOrbContainerWidget::PlayConfettiBurst(int32 PieceCount, float GoldRatio,
	EConfettiStyle Style, FVector2D CenterNorm)
{
	if (!OrbCanvas || PieceCount <= 0) return;

	// 대포(착지 정산) 팔레트 — 액센트 블루/스텝 그린/골드/크림 (스타일 카탈로그 톤)
	static const FLinearColor CannonPalette[] = {
		FLinearColor(0.046665f, 0.327778f, 0.745404f, 1.0f),
		FLinearColor(0.104616f, 0.630757f, 0.254152f, 1.0f),
		FLinearColor(0.791298f, 0.421407f, 0.086500f, 1.0f),
		FLinearColor(0.904661f, 0.838799f, 0.693872f, 1.0f),
	};
	// 메달 버스트(레벨업) 팔레트 — 골드/샴페인/앰버/크림 (다크+골드 장면 전용 웜톤)
	static const FLinearColor WarmPalette[] = {
		FLinearColor(0.791298f, 0.421407f, 0.086500f, 1.0f),
		FLinearColor(0.937000f, 0.610000f, 0.212000f, 1.0f),
		FLinearColor(0.745000f, 0.254000f, 0.027000f, 1.0f),
		FLinearColor(0.904661f, 0.838799f, 0.693872f, 1.0f),
	};
	static const FLinearColor Gold(0.791298f, 0.421407f, 0.086500f, 1.0f);

	const bool bMedal = (Style == EConfettiStyle::MedalBurst);

	// 스타 조각 텍스처 lazy load — 흰색 온 투명이라 틴트 안전 (2026-07-20 실측)
	if (bMedal && CachedConfettiStarTextures.Num() == 0)
	{
		for (int32 i = 1; i <= 6; ++i)
		{
			const FString Path = FString::Printf(
				TEXT("/Game/CompanyGrowth/UI/CasualPack/VFX/Polygon_%02d.Polygon_%02d"), i, i);
			if (UTexture2D* Tex = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path)).LoadSynchronous())
			{
				CachedConfettiStarTextures.Add(Tex);
			}
		}
	}

	const FVector2D CanvasSize = OrbCanvas->GetCachedGeometry().GetLocalSize();
	if (CanvasSize.X < 100.0f || CanvasSize.Y < 100.0f) return;   // 지오메트리 미확정 프레임 가드

	const FVector2D Center(CanvasSize.X * CenterNorm.X, CanvasSize.Y * CenterNorm.Y);
	const int32 Count = FMath::Min(PieceCount, MaxConfettiPieces - ActiveConfetti.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		FVector2D Origin;
		FVector2D Vel;
		if (bMedal)
		{
			// 메달 뒤 원환에서 바깥+위 바이어스로 방사 분출
			const float SpawnAng = FMath::FRandRange(0.0f, 2.0f * PI);
			const float SpawnR = FMath::FRandRange(MedalBurstSpawnRadiusMin, MedalBurstSpawnRadiusMax);
			Origin = Center + FVector2D(FMath::Cos(SpawnAng), FMath::Sin(SpawnAng)) * SpawnR;
			FVector2D Dir = (Origin - Center).GetSafeNormal();
			Dir.Y -= FMath::FRandRange(0.35f, 0.95f);
			Dir.Normalize();
			Vel = Dir * FMath::FRandRange(MedalBurstSpeedMin, MedalBurstSpeedMax);
		}
		else
		{
			const bool bLeft = (i % 2 == 0);
			Origin = FVector2D(CanvasSize.X * (bLeft ? 0.08f : 0.92f), CanvasSize.Y * 1.05f);

			// 수평 기준 60~85° 위-안쪽 분출
			const float AngleDeg = FMath::FRandRange(60.0f, 85.0f);
			const float Speed = FMath::FRandRange(1100.0f, 1600.0f);
			const float Rad = FMath::DegreesToRadians(AngleDeg);
			Vel = FVector2D(FMath::Cos(Rad) * Speed * (bLeft ? 1.0f : -1.0f), -FMath::Sin(Rad) * Speed);
		}

		UImage* Piece = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), *FString::Printf(TEXT("Confetti_%d"), OrbIdCounter++));
		if (!Piece) continue;

		const bool bStar = bMedal && CachedConfettiStarTextures.Num() > 0
			&& FMath::FRand() < MedalBurstStarRatio;
		const FLinearColor Color = (FMath::FRand() < GoldRatio) ? Gold
			: (bMedal ? WarmPalette[FMath::RandRange(0, UE_ARRAY_COUNT(WarmPalette) - 1)]
			          : CannonPalette[FMath::RandRange(0, UE_ARRAY_COUNT(CannonPalette) - 1)]);

		FSlateBrush Brush;
		Brush.TintColor = FSlateColor(Color);
		if (bStar)
		{
			Brush.SetResourceObject(CachedConfettiStarTextures[
				FMath::RandRange(0, CachedConfettiStarTextures.Num() - 1)]);
			const float StarSize = FMath::FRandRange(20.0f, 34.0f);
			Piece->SetDesiredSizeOverride(FVector2D(StarSize, StarSize));
		}
		else
		{
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			Brush.OutlineSettings.CornerRadii = FVector4(3.0, 3.0, 3.0, 3.0);
			Piece->SetDesiredSizeOverride(FVector2D(
				FMath::FRandRange(11.0f, 17.0f), FMath::FRandRange(17.0f, 26.0f)));
		}
		Piece->SetBrush(Brush);
		Piece->SetVisibility(ESlateVisibility::HitTestInvisible);

		UCanvasPanelSlot* PieceSlot = OrbCanvas->AddChildToCanvas(Piece);
		if (!PieceSlot) { Piece->RemoveFromParent(); continue; }
		PieceSlot->SetAutoSize(true);
		PieceSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PieceSlot->SetPosition(Origin);

		FConfettiAnimData Data;
		Data.Image = Piece;
		Data.Slot = PieceSlot;
		Data.Pos = Origin;
		Data.StartPos = Origin;
		Data.Velocity = Vel;
		Data.SpinRate = FMath::FRandRange(180.0f, 540.0f)
			* (FMath::RandBool() ? 1.0f : -1.0f) * (bStar ? 0.4f : 1.0f);
		Data.Angle = FMath::FRandRange(0.0f, 360.0f);
		Data.Lifetime = bMedal ? FMath::FRandRange(1.9f, 2.6f) : FMath::FRandRange(1.6f, 2.2f);
		Data.bStar = bStar;
		Data.FlutterRate = FMath::FRandRange(bStar ? 6.0f : 7.0f, bStar ? 10.0f : 12.0f);
		Data.FlutterPhase = FMath::FRandRange(0.0f, 2.0f * PI);
		if (bMedal)
		{
			// 종이 공기저항 — 초반 분출 후 팔랑이며 천천히 낙하
			Data.TerminalFall = FMath::FRandRange(300.0f, 520.0f);
			Data.HorizontalDrag = FMath::FRandRange(0.6f, 1.2f);
		}
		ActiveConfetti.Add(Data);
	}
}

void UScoreOrbContainerWidget::SpawnStampText(FVector2D CenterPos, const FText& Text,
	const FLinearColor& Color, const FSlateFontInfo& Font)
{
	if (!OrbCanvas) return;

	UTextBlock* Stamp = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), *FString::Printf(TEXT("Stamp_%d"), OrbIdCounter++));
	if (!Stamp) return;

	Stamp->SetText(Text);
	Stamp->SetJustification(ETextJustify::Center);
	FSlateFontInfo StampFont = Font;
	StampFont.OutlineSettings.OutlineSize = 2;
	StampFont.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	Stamp->SetFont(StampFont);
	Stamp->SetColorAndOpacity(FSlateColor(Color));
	Stamp->SetVisibility(ESlateVisibility::HitTestInvisible);
	Stamp->SetShadowOffset(FVector2D(1.5f, 2.5f));
	Stamp->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	Stamp->SetRenderTransformAngle(-8.0f);
	Stamp->SetRenderScale(FVector2D(1.6f, 1.6f));
	Stamp->SetRenderOpacity(0.0f);

	UCanvasPanelSlot* StampSlot = OrbCanvas->AddChildToCanvas(Stamp);
	if (!StampSlot) { Stamp->RemoveFromParent(); return; }
	StampSlot->SetAutoSize(true);
	StampSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	StampSlot->SetPosition(CenterPos);

	FStampAnimData Data;
	Data.Text = Stamp;
	ActiveStamps.Add(Data);
}

void UScoreOrbContainerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Slate 가 넘기는 InDeltaTime 은 실시간이라 배속(TimeDilation)을 안 탄다 — 연출을 개발 배속에 묶으려면 월드 델타.
	const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : InDeltaTime;
	if (bDevelopmentPresentation)
	{
		const UOfficeStageProgressManager* ProgressMgr = GetWorld() ? GetWorld()->GetSubsystem<UOfficeStageProgressManager>() : nullptr;
		bDevelopmentPaused = ProgressMgr && (ProgressMgr->IsEventPaused() || ProgressMgr->IsBoostGamblePending());
		if (bDevelopmentPaused) return;
		TickDevelopmentEffects(DeltaTime);
	}

	// === 점수 숫자 애니메이션 (팝 + 정착 + 읽히는 쉼 + 상승/페이드) ===
	const bool bReduceNumberMotion = USettingsManagerSubsystem::IsReduceMotion(this);
	for (int32 i = ActiveNumbers.Num() - 1; i >= 0; --i)
	{
		FScoreNumberAnimData& N = ActiveNumbers[i];
		if (CVarNumberFreeze.GetValueOnGameThread() == 0)
		{
			N.Elapsed += DeltaTime;
		}
		else
		{
			N.Elapsed = CVarNumberFreezeAt.GetValueOnGameThread();
		}
		const float T = FMath::Clamp(N.Elapsed / N.Lifetime, 0.0f, 1.0f);

		// 슬롯을 옮기면 Slate 가 매 프레임 레이아웃을 다시 잡는다 — 그리기만 옮긴다(출발점은 스폰 때 고정)
		if (N.Root)
		{
			const FScoreNumberMotion Motion = EvaluateScoreNumberMotion(N.Elapsed, N.Lifetime, N.RiseDistance,
				CVarNumberPopScale.GetValueOnGameThread() > 0, bReduceNumberMotion);
			N.Root->SetRenderTranslation(FVector2D(0.0f, Motion.OffsetY));
			N.Root->SetRenderScale(FVector2D(Motion.Scale, Motion.Scale));
			N.Root->SetRenderOpacity(Motion.Opacity);
		}

		// 타격 버스트 — 터지고(0.11s) 머물고(0.09s) 빠진다(0.20s). 숫자 수명(1.6s)의 1/4만 산다.
		// 오래 남으면 그건 타격이 아니라 배후광이 되고, 숫자를 읽는 데도 방해가 된다.
		if (N.Burst)
		{
			// 매 틱 읽는다 — 그래야 콘솔로 값을 바꾸면 이미 떠 있는 버스트에도 바로 먹는다
			const float PopTime = FMath::Max(0.01f, CVarBurstPop.GetValueOnGameThread());
			const float HoldTime = FMath::Max(0.0f, CVarBurstHold.GetValueOnGameThread());
			const float FadeTime = FMath::Max(0.01f, CVarBurstFade.GetValueOnGameThread());
			const float MaxAlpha = CVarBurstAlpha.GetValueOnGameThread();
			const float Grow = FMath::Clamp(N.Elapsed / PopTime, 0.0f, 1.0f);
			const float C1 = 1.70158f;
			const float C3 = C1 + 1.0f;
			const float TB = Grow - 1.0f;
			const float EaseOutBack = 1.0f + C3 * TB * TB * TB + C1 * TB * TB;   // 0 → ~1.1 → 1
			const float Scale = FMath::Lerp(0.25f, 1.0f, EaseOutBack);

			const float FadeStart = PopTime + HoldTime;
			const float Alpha = (N.Elapsed <= FadeStart)
				? MaxAlpha
				: MaxAlpha * FMath::Max(0.0f, 1.0f - (N.Elapsed - FadeStart) / FadeTime);

			// 스케일은 자기 중심을 기준으로 돈다 — 맨 왼쪽 글자 위로 옮겨둔 자리는 안 흐트러진다
			N.Burst->SetRenderScale(FVector2D(Scale, Scale));
			N.Burst->SetRenderTransformAngle(N.BurstAngle);
			N.Burst->SetRenderOpacity(Alpha);
		}

		for (int32 s = 0; s < N.Sparks.Num(); ++s)
		{
			UImage* Spark = N.Sparks[s];
			if (!Spark) continue;

			// 버스트와 같은 창에서 같이 튄다 — 혼자 오래 남으면 타격이 아니라 장식이 된다
			const float Phase = FMath::Clamp((N.Elapsed - 0.03f * s) / 0.10f, 0.0f, 1.0f);
			const float SparkScale = FMath::Lerp(0.3f, 1.0f, Phase);
			Spark->SetRenderScale(FVector2D(SparkScale, SparkScale));
			const float SparkFadeStart = FMath::Max(0.01f, CVarBurstPop.GetValueOnGameThread())
				+ FMath::Max(0.0f, CVarBurstHold.GetValueOnGameThread());
			const float SparkAlpha = (N.Elapsed <= SparkFadeStart)
				? 1.0f
				: FMath::Max(0.0f, 1.0f - (N.Elapsed - SparkFadeStart)
					/ (FMath::Max(0.01f, CVarBurstFade.GetValueOnGameThread()) * 1.4f));
			Spark->SetRenderOpacity(Phase * SparkAlpha);
		}

		if (T >= 1.0f)
		{
			if (N.Root)
			{
				N.Root->RemoveFromParent();
			}
			ActiveNumbers.RemoveAt(i);
		}
	}

	// === 컨페티 (분출→중력 낙하→팔랑임/트윙클→페이드) ===
	for (int32 i = ActiveConfetti.Num() - 1; i >= 0; --i)
	{
		FConfettiAnimData& C = ActiveConfetti[i];
		C.Elapsed += DeltaTime;
		C.Velocity.Y = FMath::Min(C.Velocity.Y + ConfettiGravity * DeltaTime, C.TerminalFall);
		C.Velocity.X *= FMath::Max(0.0f, 1.0f - C.HorizontalDrag * DeltaTime);
		C.Pos += C.Velocity * DeltaTime;
		C.Angle += C.SpinRate * DeltaTime;

		if (C.Image) C.Image->SetRenderTranslation(C.Pos - C.StartPos);
		if (C.Image)
		{
			C.Image->SetRenderTransformAngle(C.Angle);
			if (C.bStar)
			{
				// 스타 트윙클 — 스케일 펄스
				const float Pulse = 0.9f + 0.25f * FMath::Sin(C.Elapsed * C.FlutterRate + C.FlutterPhase);
				C.Image->SetRenderScale(FVector2D(Pulse, Pulse));
			}
			else if (C.FlutterRate > 0.0f)
			{
				// 종이 뒤집힘 — X폭이 |cos|로 얇아졌다 넓어지는 가짜 3D 팔랑임
				const float Flip = FMath::Abs(FMath::Cos(C.Elapsed * C.FlutterRate + C.FlutterPhase));
				C.Image->SetRenderScale(FVector2D(FMath::Max(0.12f, Flip), 1.0f));
			}
			const float Remain = C.Lifetime - C.Elapsed;
			C.Image->SetRenderOpacity(Remain < ConfettiFadeTail ? FMath::Max(0.0f, Remain / ConfettiFadeTail) : 1.0f);
		}

		if (C.Elapsed >= C.Lifetime)
		{
			if (C.Image) C.Image->RemoveFromParent();
			ActiveConfetti.RemoveAt(i);
		}
	}

	// === 도장 텍스트 (쾅 스케일인→유지→페이드) ===
	for (int32 i = ActiveStamps.Num() - 1; i >= 0; --i)
	{
		FStampAnimData& S = ActiveStamps[i];
		S.Elapsed += DeltaTime;
		if (!S.Text) { ActiveStamps.RemoveAt(i); continue; }

		if (S.Elapsed < StampPopInTime)
		{
			// EaseOutBack — 1.6→1.0 으로 눌러 찍히는 도장
			const float a = S.Elapsed / StampPopInTime;
			const float c1 = 1.70158f;
			const float c3 = c1 + 1.0f;
			const float tb = a - 1.0f;
			const float Eased = 1.0f + c3 * tb * tb * tb + c1 * tb * tb;
			const float Sc = FMath::Lerp(1.6f, 1.0f, Eased);
			S.Text->SetRenderScale(FVector2D(Sc, Sc));
			S.Text->SetRenderOpacity(FMath::Min(1.0f, a * 3.0f));
		}
		else
		{
			S.Text->SetRenderScale(FVector2D(1.0f, 1.0f));
			const float AfterHold = S.Elapsed - StampPopInTime - StampHoldTime;
			S.Text->SetRenderOpacity(AfterHold <= 0.0f ? 1.0f : FMath::Max(0.0f, 1.0f - AfterHold / StampFadeTime));
		}

		if (S.Elapsed >= StampPopInTime + StampHoldTime + StampFadeTime)
		{
			S.Text->RemoveFromParent();
			ActiveStamps.RemoveAt(i);
		}
	}

	for (int32 i = ActiveOrbs.Num() - 1; i >= 0; --i)
	{
		FOrbAnimData& Orb = ActiveOrbs[i];
		if (Orb.bCompleted) continue;

		// 시차 대기 처리
		if (Orb.Delay > 0.0f)
		{
			Orb.Delay -= DeltaTime;
			continue;
		}

		Orb.Elapsed += DeltaTime;

		// === 이벤트 스타일: Phase 1 (팝아웃) 처리 ===
		if (Orb.bEventStyle && !Orb.bPopOutDone)
		{
			float PopAlpha = FMath::Clamp(Orb.Elapsed / Orb.PopOutDuration, 0.0f, 1.0f);

			// EaseOutBack: 살짝 오버슈트하며 튀어나감
			float EasedT;
			{
				const float c1 = 1.70158f;
				const float c3 = c1 + 1.0f;
				float t = PopAlpha - 1.0f;
				EasedT = 1.0f + c3 * t * t * t + c1 * t * t;
			}

			FVector2D PopPos = FMath::Lerp(Orb.StartPos, Orb.PopOutTargetPos, EasedT);
			float PopScale = FMath::Lerp(0.5f * Orb.Scale, 1.4f * Orb.Scale, PopAlpha);
			float PopOpacity = FMath::Min(1.0f, PopAlpha * 2.0f);

			if (Orb.OrbImage)
			{
				Orb.OrbImage->SetRenderTranslation(PopPos - Orb.StartPos);
				Orb.OrbImage->SetRenderScale(FVector2D(PopScale, PopScale));
				Orb.OrbImage->SetRenderOpacity(PopOpacity);
			}
			if (Orb.GlowImage)
			{
				Orb.GlowImage->SetRenderTranslation(PopPos - Orb.StartPos);
				float GlowScale = PopScale * GlowSizeMultiplier * 0.5f;
				Orb.GlowImage->SetRenderScale(FVector2D(GlowScale, GlowScale));
				Orb.GlowImage->SetRenderOpacity(PopOpacity * 0.6f);
			}

			if (PopAlpha >= 1.0f)
			{
				// Phase 1 종료 → Phase 2 비행 시작점으로 StartPos 갱신
				Orb.FlightStartPos = Orb.PopOutTargetPos;
				Orb.Elapsed = 0.0f;
				Orb.bPopOutDone = true;
			}
			continue;
		}

		float Alpha = FMath::Clamp(Orb.Elapsed / Orb.Duration, 0.0f, 1.0f);

		// === 이동: 이차 베지어 + EaseOut ===
		float MoveAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.5f);

		FVector2D MidPoint = (Orb.FlightStartPos + Orb.TargetPos) * 0.5f;
		FVector2D ControlPoint = MidPoint + FVector2D(0.f, -Orb.ArcHeight);

		float t = MoveAlpha;
		float OneMinusT = 1.0f - t;
		FVector2D CurrentPos =
			OneMinusT * OneMinusT * Orb.FlightStartPos +
			2.0f * OneMinusT * t * ControlPoint +
			t * t * Orb.TargetPos;

		// === 좌우 흔들림 (감쇄 사인파, 비행 경로에 수직) ===
		FVector2D FlightDir = Orb.TargetPos - Orb.FlightStartPos;
		float FlightLen = FlightDir.Size();
		if (FlightLen > 1.0f)
		{
			FVector2D Perpendicular(-FlightDir.Y / FlightLen, FlightDir.X / FlightLen);
			float Damping = 1.0f - Alpha;
			float Wobble = FMath::Sin(Alpha * PI * 3.0f) * Damping * Orb.WobbleOffset;
			CurrentPos += Perpendicular * Wobble;
		}

		// === 스케일: 팝 인 → 유지 → 축소 ===
		if (bDevelopmentPresentation && !USettingsManagerSubsystem::IsReduceMotion(this))
		{
			Orb.DevelopmentTrail.Add(CurrentPos);
			if (Orb.DevelopmentTrail.Num() > 9) Orb.DevelopmentTrail.RemoveAt(0, 1, EAllowShrinking::No);
		}

		float ScaleFactor;
		if (Alpha < 0.10f)
		{
			// 팝 인: 0 → 1.4 (탄성 느낌)
			float PopAlpha = Alpha / 0.10f;
			ScaleFactor = FMath::Lerp(0.0f, 1.4f * Orb.Scale, PopAlpha);
		}
		else if (Alpha < 0.22f)
		{
			// 오버슈트 복귀: 1.4 → 1.0
			float SettleAlpha = (Alpha - 0.10f) / 0.12f;
			ScaleFactor = FMath::Lerp(1.4f * Orb.Scale, Orb.Scale, SettleAlpha);
		}
		else if (Alpha < 0.88f)
		{
			// 비행 중 숨쉬기 (미세 스케일 진동)
			float Breathe = 1.0f + FMath::Sin(Alpha * PI * 4.0f) * 0.06f;
			ScaleFactor = Orb.Scale * Breathe;
		}
		else
		{
			// 도착 직전: 1.0 → 0.0 (짧고 빠르게 축소)
			float ShrinkAlpha = (Alpha - 0.88f) / 0.12f;
			float EasedShrink = ShrinkAlpha * ShrinkAlpha;
			ScaleFactor = FMath::Lerp(Orb.Scale, 0.0f, EasedShrink);
		}

		// === 오파시티 ===
		float Opacity = 1.0f;
		if (Alpha < 0.08f)
		{
			// 페이드 인
			Opacity = Alpha / 0.08f;
		}
		else if (Alpha > 0.90f)
		{
			// 도착 직전 페이드 아웃
			Opacity = FMath::Lerp(1.0f, 0.0f, (Alpha - 0.90f) / 0.10f);
		}

		// === 글로우 후광: 메인보다 크게 (맥동은 머티리얼 PulseSpeed에서 처리) ===
		float GlowScale = ScaleFactor * GlowSizeMultiplier * 0.5f;
		float GlowOpacity = Opacity * 0.5f;

		// 메인 구슬 업데이트
		if (Orb.OrbImage)
		{
			Orb.OrbImage->SetRenderTranslation(CurrentPos - Orb.StartPos);
			Orb.OrbImage->SetRenderScale(FVector2D(ScaleFactor, ScaleFactor));
			Orb.OrbImage->SetRenderOpacity(Opacity);
		}

		// 글로우 후광 업데이트
		if (Orb.GlowImage)
		{
			Orb.GlowImage->SetRenderTranslation(CurrentPos - Orb.StartPos);
			Orb.GlowImage->SetRenderScale(FVector2D(GlowScale, GlowScale));
			Orb.GlowImage->SetRenderOpacity(GlowOpacity);
		}

		// 도착 완료 → 제거
		if (Alpha >= 1.0f)
		{
			Orb.bCompleted = true;
			if (Orb.bNotifyArrival)
			{
				OnScoreOrbArrived.Broadcast(Orb.StepNumber, Orb.bIsCritical);
			}
			if (Orb.GlowImage)
			{
				OrbCanvas->RemoveChild(Orb.GlowImage);
			}
			if (Orb.OrbImage)
			{
				OrbCanvas->RemoveChild(Orb.OrbImage);
			}
			ActiveOrbs.RemoveAt(i);
		}
	}
}
