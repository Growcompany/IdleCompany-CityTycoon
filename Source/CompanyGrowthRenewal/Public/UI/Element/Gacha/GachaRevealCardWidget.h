#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Utils/GachaRevealModel.h"
#include "GachaRevealCardWidget.generated.h"

class UCanvasPanel;
class UImage;
class UMaterialInstanceDynamic;
class UOverlay;
class USizeBox;
class UTextBlock;
class UTexture2D;

enum class EGachaRevealKind : uint8 { Equipment, Trait, Skin };

// 시안 카드 크기 묶음. 앞 셋 = CSS sz1·sz10·sz30, Best(이번 최고 378)·Spot(단독 장면 560)은 sz1 규칙을 쓰는 다른 굽기 묶음
enum class EGachaCardSize : uint8 { Single, Ten, Thirty, Best, Spot };

struct FGachaRevealCardData
{
	EGachaRevealKind Kind = EGachaRevealKind::Equipment;
	int32 Grade = 0; // 0=C … 5=SSS (특성·스킨은 일반 … 신화)
	FText GradeText;
	FText Name;
	FText EffectLine;
	FText UniqueLine;
	TSoftObjectPtr<UTexture2D> Icon;
	TOptional<FBox2d> IconUVRegion;
	bool bNew = false;
	bool bRecommended = false; // 장비만 ▲
	int32 MileageGained = 0;   // 스킨 중복만 +nP
};

// 시안 .cd CSS 를 카드 묶음 크기(디자인 px, 카드 왼쪽 위 = 0)로 푼 값. 정답 = docs/05_UI/mockups/GachaRevealJuice_2026-10-17/src.html
namespace GachaRevealCardLayout
{
	enum class ECardPart : uint8 { Face, Back, BackHot, Badge, New, Up };

	// 구운 그림 = 묶음의 가장 큰 화면 크기. 같은 묶음 안 작은 카드는 위젯이 통째로 줄인다(굽기 규칙 _rules._card)
	FVector2D DesignSize(EGachaCardSize Size);
	const TCHAR* ClassSuffix(EGachaCardSize Size);
	bool UsesSingleRules(EGachaCardSize Size); // CSS .sz1

	// 비면 그 그림이 없는 조합(빛 테는 등급 3 이상만). 스킨은 특성 그림을 쓴다(시안 KINDS.skin.grades = trait)
	FSoftObjectPath SurfacePath(ECardPart Part, EGachaRevealKind Kind, int32 Grade, EGachaCardSize Size);
	FSoftObjectPath EmblemPath(EGachaRevealKind Kind);

	// 그림 원점 = 요소 원점 − 이 값(plate/slices.json _content 의 x,y)
	FVector2D SurfacePad(ECardPart Part, EGachaCardSize Size);
	// 꼬리표 요소 상자 크기(같은 _content 의 w,h)
	FVector2D TagSize(ECardPart Part, EGachaCardSize Size);

	FGachaRevealRect IconBox(EGachaCardSize Size);
	// object-fit: contain
	FGachaRevealRect FitContain(const FGachaRevealRect& Box, const FVector2D& TextureSize);

	// 글자 크기(px) 비율 = 시안 calc(var(--w)*k). UMG pt = px × PxToPt
	constexpr float PxToPt = 0.75f;
	constexpr float NameRatio = 0.1f;
	constexpr float ValueRatio = 0.085f;
	constexpr float ValueRatioThirty = 0.095f;
	constexpr float DupRatio = 0.085f;
	constexpr float FooterLineHeight = 1.25f;
	// 슬레이트 한 줄 상자 = hhea ascent+descent (메모리 reference_mockup_px_to_ue_pt)
	constexpr float NexonLineBox = 1.15f;
	constexpr float PretendardLineBox = 1.19f;

	// SetFlip 의 가로 배율 ― UMG 에 원근이 없어 rotateY 를 평면 배율로 근사(설계서의 유일한 근사)
	float FlipScaleX(float Alpha);
	// CSS breathe 0.9s ease-in-out: .45 → 1 → .45
	float BreatheOpacity(float Seconds);
	// CSS cubic-bezier(x1,y1,x2,y2) 의 진행도
	float CubicBezier(float X1, float Y1, float X2, float Y2, float T);
}

// 뽑기 공개 카드 1장. 공개 화면·결과 화면이 동적으로 만들어 칸 크기대로 놓는다(WBP UIE_GachaRevealCard).
// 루트 캔버스를 채우는 구조라 desired 크기가 0×0 ― 놓는 쪽이 칸 크기를 줘야 한다(자동 크기 상자에 넣으면 안 보인다)
UCLASS(Abstract)
class COMPANYGROWTHRENEWAL_API UGachaRevealCardWidget : public UCommonUserWidget
{
	GENERATED_BODY()

	friend struct FGachaRevealCardTestAccessor;

public:
	void Configure(const FGachaRevealCardData& InData, EGachaCardSize InSize);

	// 즉시 앞/뒤
	void SetFaceUp(bool bFaceUp);
	// 0 = 뒷면, 1 = 앞면, 0.5 에서 면 교체
	void SetFlip(float Alpha);
	// 뒷면 등급 빛 숨쉬기(등급 3 이상·뒷면일 때만). 시안처럼 앞면이 되면 저절로 꺼진다
	void SetHoldGlow(bool bOn);
	// 숨쉬는 빛을 .6 → 1 로 끌어올림(시안 도장·단독 장면 직전), 끝나면 숨쉬기로 돌아간다
	void PlayHoldRise(float DurationMs);
	// 등급 색 덮개 PeakAlpha → 0, 380ms ease-out
	void PlayFlash(float PeakAlpha);
	// 이동·눌림·회전은 여기에 건다(뒤집기 배율은 안쪽 FlipRoot 가 따로 가진다)
	UWidget* GetVisualRoot() const;

	const FGachaRevealCardData& GetData() const { return Data; }
	EGachaCardSize GetCardSize() const { return CardSize; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> VisualRoot;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> DesignBox;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> FlipRoot;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> BackRoot;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> FaceRoot;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> BackImage;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> BackEmblem;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> BackHotImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> FaceImage;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconShadow;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> BadgeImage;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> NewTag;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> UpTag;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DupText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ValueText;
	// 30회 칸에서 효과가 없는 항목(스킨)의 이름 줄 ― 시안 .vl.n(NEXON Bold)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ValueNameText;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> OrbImage;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> FlashImage;

	// 시안 KINDS.*.grades[].fx (등급 0..5). 스킨은 특성 색
	UPROPERTY(EditDefaultsOnly, Category = "Card|Palette")
	TArray<FLinearColor> EquipmentFxColors;
	UPROPERTY(EditDefaultsOnly, Category = "Card|Palette")
	TArray<FLinearColor> TraitFxColors;
	// 시안 .vl 색 ― 30회 칸만 진하다
	UPROPERTY(EditDefaultsOnly, Category = "Card|Palette")
	FSlateColor ValueInkTen;
	UPROPERTY(EditDefaultsOnly, Category = "Card|Palette")
	FSlateColor ValueInkThirty;

private:
	void ApplySurfaces();
	void ApplyIcon();
	void ApplyTags();
	void ApplyFooter();
	void ApplyOrbAndFlashColor();
	void ShowSide(bool bFace);
	void SetTextPx(UTextBlock* Text, float Px);
	FLinearColor GradeFx() const;
	UMaterialInstanceDynamic* OrbMaterial();
	UMaterialInstanceDynamic* FlashMaterial();

	FGachaRevealCardData Data;
	EGachaCardSize CardSize = EGachaCardSize::Ten;
	bool bConfigured = false;
	bool bFaceShown = false;

	// 글자 줄의 WBP 저작 크기·외곽선 ― 재사용 카드라 처음 바꾸기 전에 한 번 기억해 둔다
	struct FAuthoredFont
	{
		float Size = 0.f;
		int32 Outline = 0;
	};
	TMap<const UTextBlock*, FAuthoredFont> AuthoredFonts;

	// 빛 테 그림을 실제로 불러왔을 때만 숨쉰다(빈 브러시 UImage = 흰 판)
	bool bHotSurfaceLoaded = false;
	bool bHold = false;
	float HoldSeconds = 0.f;
	float HoldRiseSeconds = 0.f;
	float HoldRiseDuration = 0.f;

	float FlashPeak = 0.f;
	float FlashSeconds = -1.f; // < 0 = 꺼짐

	float OrbSeconds = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OrbMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlashMID;
};
