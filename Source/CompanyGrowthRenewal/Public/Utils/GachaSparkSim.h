#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

// 뽑기 상자 공개 효과의 순수 계산 ― 시안 GachaRevealJuice_2026-10-17/src.html 의 sparks()·ring()·glow()·frame()·doShake() 식 그대로.
// UObject·위젯 없음(자동화 시험·효과층 UGachaRevealFXLayer 공용). 좌표·길이 = 공개 화면 무대 px(세로 1440).

struct FGachaSparkParams
{
	int32 Count = 16;
	FLinearColor Color = FLinearColor::White;
	float AngleMin = 0.f;
	float AngleMax = UE_TWO_PI;
	float SpeedMin = 700.f;
	float SpeedMax = 1800.f;
	float LifeMin = .18f;
	float LifeMax = .42f;
	float WidthMin = 3.f;
	float WidthMax = 7.f;
	float Gravity = 1400.f;
	float Jitter = 0.f;
	float TailSec = .035f;
};

enum class EGachaFXParticleKind : uint8
{
	None,
	Spark,
	Ring,
	Glow,
};

struct FGachaFXParticle
{
	EGachaFXParticleKind Kind = EGachaFXParticleKind::None;
	FVector2D Pos = FVector2D::ZeroVector;
	FVector2D Vel = FVector2D::ZeroVector;
	float Life = 0.f;
	float MaxLife = 0.f;
	// 불꽃 = 기준 선 굵기 w, 고리 = 테 굵기 w
	float Width = 0.f;
	float Gravity = 0.f;
	float TailSec = 0.f;
	float R0 = 0.f;
	float R1 = 0.f;
	float Radius = 0.f;
	float Peak = 0.f;
	FLinearColor Color = FLinearColor::White;
	uint32 Serial = 0;

	bool IsActive() const { return Kind != EGachaFXParticleKind::None; }
	float Progress() const { return MaxLife > 0.f ? 1.f - Life / MaxLife : 1.f; }
};

namespace GachaFXMath
{
	inline float OneMinus(float T) { return FMath::Max(0.f, 1.f - T); }

	inline float SparkAlpha(float T) { return FMath::Pow(OneMinus(T), 1.2f); }
	inline float SparkLineWidth(float W, float T) { return W * (1.f - .7f * T); }
	inline float RingRadius(float R0, float R1, float T) { return FMath::Lerp(R0, R1, 1.f - FMath::Pow(OneMinus(T), 3.f)); }
	inline float RingAlpha(float T) { return FMath::Pow(OneMinus(T), 1.5f); }
	inline float RingWidth(float W, float T) { return W * (1.f - .6f * T); }
	inline float GlowAlpha(float Peak, float T) { return Peak * FMath::Pow(OneMinus(T), 1.6f); }

	// 시안 Math.round = floor(x + .5)
	inline int32 JsRound(float X) { return FMath::FloorToInt(X + .5f); }

	inline float LinearToSRGB(float L)
	{
		L = FMath::Clamp(L, 0.f, 1.f);
		return L <= .0031308f ? L * 12.92f : 1.055f * FMath::Pow(L, 1.f / 2.4f) - .055f;
	}

	inline float SRGBToLinear(float S)
	{
		S = FMath::Clamp(S, 0.f, 1.f);
		return S <= .04045f ? S / 12.92f : FMath::Pow((S + .055f) / 1.055f, 2.4f);
	}

	// 시안 캔버스는 sRGB 값끼리 곱한다(색 × globalAlpha). linear 색의 sRGB 값에 Mul 을 곱한 색을 linear 로 돌려준다 ―
	// 더하기 머티리얼(흰 심·칼끝·screen 더하기 장)이 정점색을 sRGB 로 되돌려 곱하므로 이미지 ColorAndOpacity 로 그대로 넘긴다(A = 1)
	inline FLinearColor ScaleInSRGB(const FLinearColor& Linear, float Mul)
	{
		const float M = FMath::Max(Mul, 0.f);
		return FLinearColor(SRGBToLinear(LinearToSRGB(Linear.R) * M), SRGBToLinear(LinearToSRGB(Linear.G) * M),
			SRGBToLinear(LinearToSRGB(Linear.B) * M), 1.f);
	}

	// 미리 곱한 겹치기 머티리얼(색 있는 획·빛·새는 빛) 정점색 = 색 그대로 + A = 세기. 셰이더가 sRGB 공간에서 색 × 세기를 곱하고 불투명도 = 모양 × 세기
	inline FLinearColor OverTint(const FLinearColor& Linear, float Intensity)
	{
		return FLinearColor(Linear.R, Linear.G, Linear.B, FMath::Clamp(Intensity, 0.f, 1.f));
	}

	// CSS cubic-bezier(X1, Y1, X2, Y2) 의 y(x)
	inline float CubicBezier(float X, float X1, float Y1, float X2, float Y2)
	{
		X = FMath::Clamp(X, 0.f, 1.f);
		auto Bx = [X1, X2](float S) { return 3.f * (1.f - S) * (1.f - S) * S * X1 + 3.f * (1.f - S) * S * S * X2 + S * S * S; };
		auto By = [Y1, Y2](float S) { return 3.f * (1.f - S) * (1.f - S) * S * Y1 + 3.f * (1.f - S) * S * S * Y2 + S * S * S; };
		float Lo = 0.f, Hi = 1.f, S = X;
		for (int32 Iter = 0; Iter < 40; ++Iter)
		{
			const float Bv = Bx(S);
			if (FMath::Abs(Bv - X) < 1e-6f)
			{
				break;
			}
			(Bv < X ? Lo : Hi) = S;
			S = .5f * (Lo + Hi);
		}
		return By(S);
	}

	// WAAPI easing 'ease-out' 은 키프레임 사이가 아니라 전체 진행에 걸린다(시안 anim() 의 옵션)
	inline float EaseOut(float X) { return CubicBezier(X, 0.f, 0.f, .58f, 1.f); }

	// 시안 anim([{opacity 0}, {opacity Peak, offset At}, {opacity 0}], ms, 'ease-out') 의 세기 비율(0..1)
	inline float PeakEnvelope(float Elapsed, float Duration, float At)
	{
		if (Duration <= 0.f || Elapsed >= Duration)
		{
			return 0.f;
		}
		const float U = EaseOut(FMath::Max(Elapsed, 0.f) / Duration);
		return U < At ? U / At : FMath::Max(0.f, 1.f - (U - At) / (1.f - At));
	}

	// 시안 openLid 빛 기둥 scaleY: .2 → 1(offset .25) → 1
	inline float BeamScaleY(float Elapsed, float Duration)
	{
		if (Duration <= 0.f)
		{
			return 1.f;
		}
		const float U = EaseOut(FMath::Clamp(Elapsed / Duration, 0.f, 1.f));
		return U < .25f ? .2f + .8f * (U / .25f) : 1.f;
	}

	// 시안 cutIn #pillar: 켜기 anim(opacity 0→1·scaleY .3→1, ease-out) 뒤 delay 부터 1→.65→1 1.8초 반복(linear, 주기는 속도 배수를 안 받는다)
	constexpr float PillarBreathSec = 1.8f;
	constexpr float PillarBreathLow = .65f;
	constexpr float PillarRiseScaleY0 = .3f;

	inline float PillarOnOpacity(float Elapsed, float RiseSec, float DelaySec)
	{
		if (Elapsed >= DelaySec)
		{
			const float Phase = FMath::Fmod(Elapsed - DelaySec, PillarBreathSec) / PillarBreathSec;
			const float Dip = 1.f - PillarBreathLow;
			return Phase < .5f ? 1.f - Dip * (Phase / .5f) : PillarBreathLow + Dip * ((Phase - .5f) / .5f);
		}
		return RiseSec > 0.f ? EaseOut(FMath::Clamp(Elapsed / RiseSec, 0.f, 1.f)) : 1.f;
	}

	inline float PillarScaleY(float Elapsed, float RiseSec)
	{
		return RiseSec > 0.f ? PillarRiseScaleY0 + (1.f - PillarRiseScaleY0) * EaseOut(FMath::Clamp(Elapsed / RiseSec, 0.f, 1.f)) : 1.f;
	}

	// 끄기 = 숨쉬기를 취소하고 1 에서 0 으로(시안 anim 첫 키프레임이 1 ― 숨쉬기 중간 값에서 시작하지 않는다)
	inline float PillarOffOpacity(float Elapsed, float FadeSec)
	{
		return FadeSec > 0.f ? 1.f - EaseOut(FMath::Clamp(Elapsed / FadeSec, 0.f, 1.f)) : 0.f;
	}

	// 3-slice 로 늘이는 선 그림(굽기 fx.json 의 start·end·stretch·width). 그림 가로가 선 방향
	struct FStrokeSprite
	{
		float TexW;
		float TexH;
		float Start;
		float End;
		float LineWidth;
		float StretchL;
		float StretchR;
	};

	// 위젯은 그림 px 단위 크기(LocalSize)로 놓고 렌더 배율 Scale(≤1, 그림 = 시안 최대 굵기)로 줄인다 ― Box 여백(둥근 끝)이 굵기와 같은 배로 준다
	struct FStrokeLayout
	{
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D LocalSize = FVector2D::ZeroVector;
		float Scale = 1.f;
		// 고정 끝 조각보다 짧은 선은 고정 폭으로 놓고 가로로 더 줄인다(Slate Box 는 여백이 겹치면 가운데를 잘라 먹는다)
		float ScaleX = 1.f;
		float AngleDeg = 0.f;
	};

	inline FStrokeLayout LayoutStroke(const FStrokeSprite& Sprite, const FVector2D& A, const FVector2D& B, float Width)
	{
		FStrokeLayout Out;
		Out.Scale = FMath::Max(Width, 0.f) / Sprite.LineWidth;
		const FVector2D Delta = B - A;
		const float Len = static_cast<float>(Delta.Size());
		const FVector2D Dir = Len > KINDA_SMALL_NUMBER ? Delta / Len : FVector2D(1.0, 0.0);
		const float Lead = Sprite.Start;
		const float LocalLen = Out.Scale > KINDA_SMALL_NUMBER ? Len / Out.Scale : 0.f;
		const float Needed = LocalLen + Lead + (Sprite.TexW - Sprite.End);
		const float MinW = Sprite.StretchL + Sprite.StretchR;
		Out.LocalSize = FVector2D(FMath::Max(Needed, MinW), Sprite.TexH);
		Out.ScaleX = Needed < MinW ? Needed / MinW : 1.f;
		Out.Center = A + Dir * ((Needed * .5f - Lead) * Out.Scale);
		Out.AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Dir.Y), static_cast<float>(Dir.X)));
		return Out;
	}

	// 굽기 bake/out/fx/fx.json 값(apply_pending check 가 대조)
	constexpr FStrokeSprite SparkOuter{ 176.f, 31.f, 16.f, 160.f, 26.4f, 16.f, 16.f };
	constexpr FStrokeSprite SparkCore{ 115.f, 13.f, 7.2f, 108.f, 8.4f, 7.f, 7.f };
	constexpr FStrokeSprite SlitCore{ 680.f, 160.f, 80.f, 600.f, 4.f, 80.f, 80.f };
	constexpr FStrokeSprite SlitOuter055{ 680.f, 160.f, 80.f, 600.f, 16.f, 100.f, 100.f };
	constexpr FStrokeSprite SlitOuter085{ 680.f, 160.f, 80.f, 600.f, 16.f, 116.f, 116.f };
	constexpr FStrokeSprite SlitOuter100{ 680.f, 160.f, 80.f, 600.f, 16.f, 121.f, 121.f };
	constexpr float SparkOuterWidthMul = 2.2f;
	constexpr float SparkCoreWidthMul = .7f;
	constexpr float SparkCoreLengthMul = .7f;
	constexpr float SlitLeakHeight = 240.f;
	constexpr float BladeSize = 100.f;
	// glow.png 512² 의 반경 256 = 그림 전체
	constexpr float GlowTexRadiusRatio = 1.f;
}

// 시안 doShake/frame(): 세기 = max(지금, px×배수), 0.3 넘는 동안 e^(−9dt) 로 줄며 매 프레임 ±세기 무작위 이동
struct FGachaShake
{
	float Amp = 0.f;

	void Kick(float Px, float Mul) { Amp = FMath::Max(Amp, Px * Mul); }

	// 돌려준 값 = 이번 프레임 이동, bOutActive = 흔들 대상에 이동을 줘야 하는지(끝난 프레임은 0 으로 되돌림)
	FVector2D Step(float Dt, FRandomStream& Rng, bool& bOutActive)
	{
		if (Amp > .3f)
		{
			Amp *= FMath::Exp(-Dt * 9.f);
			bOutActive = true;
			return FVector2D((Rng.FRand() - .5f) * 2.f * Amp, (Rng.FRand() - .5f) * 2.f * Amp);
		}
		bOutActive = Amp != 0.f;
		Amp = 0.f;
		return FVector2D::ZeroVector;
	}
};

struct FGachaSparkSim
{
	static constexpr int32 Capacity = 160;

	TArray<FGachaFXParticle> Particles;
	FRandomStream Rng;
	uint32 NextSerial = 1;

	explicit FGachaSparkSim(int32 Seed = 0x6D2B79F5)
	{
		Particles.SetNum(Capacity);
		Rng.Initialize(Seed);
	}

	void Clear()
	{
		for (FGachaFXParticle& P : Particles)
		{
			P = FGachaFXParticle();
		}
	}

	// 빈 칸, 없으면 가장 먼저 만든 입자 칸
	int32 Allocate() const
	{
		int32 Oldest = 0;
		for (int32 Index = 0; Index < Particles.Num(); ++Index)
		{
			if (!Particles[Index].IsActive())
			{
				return Index;
			}
			if (Particles[Index].Serial < Particles[Oldest].Serial)
			{
				Oldest = Index;
			}
		}
		return Oldest;
	}

	int32 Emit(const FGachaFXParticle& P)
	{
		const int32 Index = Allocate();
		Particles[Index] = P;
		Particles[Index].MaxLife = P.MaxLife > 0.f ? P.MaxLife : P.Life;
		Particles[Index].Serial = NextSerial++;
		return Index;
	}

	// 시안 sparks(): 개수 = round(n × 배수), 속도 = lerp(s0, s1, rand^.7), 흩뿌림 = (rand − .5) × jit
	int32 Sparks(const FVector2D& Pos, const FGachaSparkParams& Params, float CountMul)
	{
		const int32 N = JsRoundCount(Params.Count, CountMul);
		for (int32 I = 0; I < N; ++I)
		{
			const float Ang = FMath::Lerp(Params.AngleMin, Params.AngleMax, Rng.FRand());
			const float Speed = FMath::Lerp(Params.SpeedMin, Params.SpeedMax, FMath::Pow(Rng.FRand(), .7f));
			FGachaFXParticle P;
			P.Kind = EGachaFXParticleKind::Spark;
			P.Life = FMath::Lerp(Params.LifeMin, Params.LifeMax, Rng.FRand());
			P.Pos.X = Pos.X + (Rng.FRand() - .5f) * Params.Jitter;
			P.Pos.Y = Pos.Y + (Rng.FRand() - .5f) * Params.Jitter;
			P.Vel = FVector2D(FMath::Cos(Ang) * Speed, FMath::Sin(Ang) * Speed);
			P.Width = FMath::Lerp(Params.WidthMin, Params.WidthMax, Rng.FRand());
			P.Color = Params.Color;
			P.Gravity = Params.Gravity;
			P.TailSec = Params.TailSec;
			Emit(P);
		}
		return N;
	}

	int32 Ring(const FVector2D& Pos, float R0, float R1, float Life, const FLinearColor& Color, float Width)
	{
		FGachaFXParticle P;
		P.Kind = EGachaFXParticleKind::Ring;
		P.Pos = Pos;
		P.R0 = R0;
		P.R1 = R1;
		P.Life = Life;
		P.Color = Color;
		P.Width = Width;
		return Emit(P);
	}

	int32 Glow(const FVector2D& Pos, float Radius, float Life, const FLinearColor& Color, float Peak)
	{
		FGachaFXParticle P;
		P.Kind = EGachaFXParticleKind::Glow;
		P.Pos = Pos;
		P.Radius = Radius;
		P.Life = Life;
		P.Color = Color;
		P.Peak = Peak;
		return Emit(P);
	}

	// 시안 frame(): 수명만 속도 배수로 줄고(life −= dt·speed), 움직임은 dt 그대로
	void Step(float Dt, float Speed)
	{
		const float Decay = FMath::Exp(-3.2f * Dt);
		for (FGachaFXParticle& P : Particles)
		{
			if (!P.IsActive())
			{
				continue;
			}
			P.Life -= Dt * Speed;
			if (P.Life <= 0.f)
			{
				P.Kind = EGachaFXParticleKind::None;
				continue;
			}
			if (P.Kind == EGachaFXParticleKind::Spark)
			{
				P.Vel.X *= Decay;
				P.Vel.Y = P.Vel.Y * Decay + P.Gravity * Dt;
				P.Pos += P.Vel * Dt;
			}
		}
	}

	int32 NumActive() const
	{
		int32 Count = 0;
		for (const FGachaFXParticle& P : Particles)
		{
			Count += P.IsActive() ? 1 : 0;
		}
		return Count;
	}

	static int32 JsRoundCount(int32 Count, float Mul) { return FMath::Max(0, GachaFXMath::JsRound(static_cast<float>(Count) * Mul)); }
};
