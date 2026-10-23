#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Utils/GachaSparkSim.h"
#include "GachaRevealFXLayer.generated.h"

class UCanvasPanel;
class URetainerBox;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;
class UTextureRenderTarget2D;
class UWidget;

// 화면 막 모양 ― 시안 openLid(뚜껑 S 이상) / cutIn(단독 장면 SS 이상)
UENUM()
enum class EGachaCurtainShape : uint8
{
	Lid,
	Spot,
};

/**
 * 뽑기 상자 공개 화면의 효과층 ― 시안 효과 캔버스(#fx: 불꽃·충격 고리·빛 번짐·틈 빛), 화면 막(#curtain), 빛줄기(#beam), 단독 장면 빛 기둥(#pillar), 화면 흔들림(doShake).
 * 공개 화면 WBP 의 캔버스(BindWidget FXLayer)에 이미지 풀을 만들어 그린다. 위젯이 아니라 제어기라 WBP·EWidgetType 행이 없다.
 * 시간은 주인이 Tick 으로 준다(타이머·티커 없음 ― 일시정지·백그라운드는 주인 몫).
 * 좌표·길이 = FXCanvas 로컬(무대 px, 세로 1440). 색 = linear(시안 sRGB 를 변환한 값).
 * 합성: 시안 #fx 의 색과 coverage 를 두 누적 캔버스에서 따로 더한 뒤 색 리테이너의 효과 재질로 한 번 겹친다. 막·빛줄기·빛 기둥은 바깥에서 screen(곱하기 장 + 더하기 장).
 * 효과 캔버스·빛 기둥 캔버스의 조상은 RenderOpacity 1 이어야 한다(곱하기 장은 알파를 안 받아 흐리면 screen 이 어두워짐으로 바뀐다).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UGachaRevealFXLayer : public UObject
{
	GENERATED_BODY()

public:
	// FXCanvas = 무대 전체 캔버스(단독 장면 위·흔들 대상 밖·등급 도장 아래). UnderCanvas = 빛줄기 전용 빈 캔버스(상자 아래·흔들 대상 안 ― 시안 #shk 안 #beam),
	// 없으면 FXCanvas 맨 아래. PillarCanvas = 빛 기둥 전용 빈 캔버스(단독 장면 안·카드 뒤 ― 시안 #cutin 안 #pillar), 없으면 빛 기둥을 안 그린다(카드 위면 카드가 씻긴다).
	// 세 캔버스는 원점·배율이 같아야 한다. 다시 부르면 앞 이미지를 걷고 새로 만든다 ― 공개 화면 여는 연출 동안 부를 것(이미지 330장 생성·텍스처 13장 동기 로드)
	void Init(UCanvasPanel* InFXCanvas, UCanvasPanel* InUnderCanvas = nullptr, UCanvasPanel* InPillarCanvas = nullptr);
	// 이미지·머티리얼·텍스처를 놓는다(공개 화면을 닫을 때 ― 텍스처 메모리는 공개 화면이 열린 동안만)
	void Release();
	bool IsReady() const { return bReady; }

	// 프레임 간격(초). 입자·흔들림은 시안처럼 0.05 로 자르고, 막·빛 기둥은 그대로 흐른다(시안 WAAPI 실시간)
	void Tick(float DeltaSeconds);
	// 시안 resetAll/clearFx ― 입자·틈 빛·막·빛줄기·빛 기둥·흔들림 전부 끔
	void Clear();

	void Sparks(FVector2D Pos, const FGachaSparkParams& Params);
	void Ring(FVector2D Pos, float R0, float R1, float Life, FLinearColor Color, float Width = 10.f);
	void Glow(FVector2D Pos, float Radius, float Life, FLinearColor Color, float Peak = .9f);
	// 0 → Peak(Lid 22%·Spot 20% 지점) → 0, ease-out. DurationSec = 호출자가 시안 T(ms) 로 바꾼 값(속도·건너뛰기 반영), 0 이하면 안 그림
	void Curtain(FLinearColor Color, float Peak, float DurationSec, EGachaCurtainShape Shape = EGachaCurtainShape::Lid);
	// Base = 기둥 아래 끝 가운데(상자 #mkT), 높이 1200 고정. 세기 0 → Peak(25%) → 0, scaleY .2 → 1
	void Beam(FVector2D Base, float Width, FLinearColor Color, float Peak, float DurationSec);
	// 켜 둔 동안 매 Tick 그린다 ― 상자가 움직이면 이음매 L·R 을 매 프레임 다시 넘길 것. Intensity = .55·.85·1(시안 세 단계)
	void SetSlit(bool bOn, FVector2D L, FVector2D R, float Progress, FLinearColor Color, float Intensity, bool bBlade);
	// 단독 장면 SS 이상 카드 뒤 빛 기둥. Base = 아래 끝 가운데(카드 가운데 x, 무대 아래 1440), 760×1440 고정.
	// 켜기 = 불투명도 0→1·scaleY .3→1(RiseSec, ease-out), BreathDelaySec 부터 1→.65→1 1.8초 반복(주기는 속도·건너뛰기를 안 받음 ― 시안).
	// 시간 = 호출자가 시안 T(380)·T(400) 으로 바꾼 값, 0 이하 = 바로 다 켬
	void PillarOn(FVector2D Base, FLinearColor Color, float RiseSec, float BreathDelaySec);
	// 숨쉬기를 끊고 1 → 0(FadeSec = T(200), ease-out), 0 이하 = 바로 끔
	void PillarOff(float FadeSec);

	// 세기 = max(지금, Px × ShakeMul), e^(−9t) 감쇠. 흔들 대상의 렌더 이동을 덮어쓴다 ― 다른 렌더 변형이 없는 묶음 위젯을 줄 것
	void Shake(float Px);
	void SetShakeTarget(UWidget* Target);
	// 시안은 #shk(내용 묶음)와 #cutin(단독 장면) 두 곳을 같은 값으로 흔든다
	void AddShakeTarget(UWidget* Target);
	// SparkMul = 불꽃 개수 배수, ShakeMul = 흔들림 배수(연출 감소 = 0), Speed = 입자 수명 속도 배수(시안 S.speed)
	void SetIntensity(float InSparkMul, float InShakeMul, float InSpeed);
	void SetSeed(int32 Seed);

	int32 GetActiveParticleCount() const { return Sim.NumActive(); }
	const FGachaSparkSim& GetSim() const { return Sim; }

private:
	friend struct FGachaRevealFXLayerTestAccessor;

	struct FTimedScreen
	{
		bool bActive = false;
		float Elapsed = 0.f;
		float Duration = 0.f;
		float Peak = 0.f;
		float PeakAt = .22f;
		FLinearColor Color = FLinearColor::White;
		FVector2D Base = FVector2D::ZeroVector;
		float Width = 0.f;
	};

	struct FPillarState
	{
		bool bActive = false;
		bool bFading = false;
		float Elapsed = 0.f;
		float RiseSec = 0.f;
		float DelaySec = 0.f;
		float FadeSec = 0.f;
		FVector2D Base = FVector2D::ZeroVector;
		FLinearColor Color = FLinearColor::White;
	};

	struct FSlitState
	{
		bool bOn = false;
		FVector2D L = FVector2D::ZeroVector;
		FVector2D R = FVector2D::ZeroVector;
		float Progress = 0.f;
		FLinearColor Color = FLinearColor::White;
		float Intensity = 1.f;
		bool bBlade = false;
	};

	UImage* NewImage(UCanvasPanel* Canvas, int32 ZOrder);
	UMaterialInstanceDynamic* AddMaterialFor(int32 TextureId);
	UMaterialInstanceDynamic* OverMaterialFor(int32 TextureId);
	UMaterialInstanceDynamic* CoverageMaterialFor(int32 TextureId);
	void ConfigureCompositeMaterials();
	void MirrorCoverage(UImage* ColorImage, UImage* CoverageImage, UMaterialInterface* RingCoverage = nullptr);
	void ConnectCoverageTarget();
	bool BrushImage(UImage* Image, UMaterialInterface* Material, const FVector2D& ImageSize, bool bBox, const FMargin& Margin);
	void ShowParticle(int32 Index);
	void HideImage(UImage* Image);
	void UpdateParticles();
	void UpdateSlit();
	void UpdateScreen(FTimedScreen& State, int32 MulIndex, float DeltaSeconds, bool bBeam);
	void UpdatePillar(float DeltaSeconds);
	void ShowScreenPair(UImage* MulImage, UImage* AddImage, UMaterialInstanceDynamic* MulMID, const FLinearColor& Color, float Opacity);
	void ApplyShake(const FVector2D& Offset);

	FGachaSparkSim Sim;
	FGachaShake ShakeState;
	FSlitState Slit;
	FTimedScreen CurtainState;
	FTimedScreen BeamState;
	FPillarState PillarState;
	EGachaCurtainShape CurtainShape = EGachaCurtainShape::Lid;
	float SparkMul = 1.f;
	float ShakeMul = 1.f;
	float Speed = 1.f;
	bool bReady = false;
	int32 SlitOuterTexture = INDEX_NONE;

	TWeakObjectPtr<UCanvasPanel> FXCanvas;
	// 같은 좌표의 색·coverage RT를 별도 누적하고 효과 재질에서 합친다.
	UPROPERTY(Transient)
	TObjectPtr<URetainerBox> FXRetainer;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> FXAccum;
	UPROPERTY(Transient)
	TObjectPtr<URetainerBox> CoverageRetainer;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> CoverageAccum;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ColorCompositeMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CoverageCompositeMID;
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> CoverageTarget;
	TWeakObjectPtr<UCanvasPanel> UnderCanvas;
	TWeakObjectPtr<UCanvasPanel> PillarCanvas;
	TArray<TWeakObjectPtr<UWidget>> ShakeTargets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> Textures;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AddMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OverMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ScreenMulMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> RingMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CoverageMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CoverageRingMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ColorCompositeMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CoverageCompositeMaterial;

	// 텍스처마다 하나(정점색 = 색·세기라 같은 그림 이미지끼리 공유)
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> AddMIDs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> OverMIDs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> CoverageMIDs;

	// 입자 칸 i 의 바깥·심 이미지는 두 누적 캔버스에서 각각 더한다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> MainImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CoreImages;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CoverageMainImages;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CoverageCoreImages;

	// 고리는 반경·굵기가 고리마다 달라 칸마다 MID
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> RingMIDs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> CoverageRingMIDs;

	TArray<EGachaFXParticleKind> ShownKinds;
	TArray<uint32> ShownSerials;

	// 0 SlitOuter · 1 SlitCore · 2 SlitLeak · 3 Blade
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> SlitImages;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CoverageSlitImages;

	// 0 막 곱하기 · 1 막 더하기 · 2 빛줄기 곱하기 · 3 빛줄기 더하기 · 4 빛 기둥 곱하기 · 5 빛 기둥 더하기
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> ScreenImages;

	// 막·빛줄기·빛 기둥 곱하기 장 MID(색·세기가 매개변수)
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ScreenMulMIDs;
};
