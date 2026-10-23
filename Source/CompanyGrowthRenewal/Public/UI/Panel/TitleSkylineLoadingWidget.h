// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Panel/LoadingWidget.h"
#include "TitleSkylineLoadingWidget.generated.h"

class UImage;
class UWidget;

UENUM(BlueprintType)
enum class ETitleLogoVariant : uint8
{
	// 확정 로고 v7 — 연출이 끝나면 TitleLogo(UIE_TitleLogo) 로 바꿔 보여 준다
	Final UMETA(DisplayName = "Final (확정 v7)"),
	// D안 입체 빌딩 — 확정 텍스처가 없으므로 조각 묶음이 그대로 정지 로고가 된다
	Buildings3D UMETA(DisplayName = "Buildings3D (D안 입체 빌딩)"),
};

/**
 * 첫 부팅 로딩 화면의 스카이라인 변형.
 * 부팅 로직(비동기 로드·진행률·페이드·전환)은 ULoadingWidget 을 그대로 쓰고, 가운데 로고의 등장만 더한다.
 * 로고는 배지를 나눈 조각 이미지(판·회사·키우기·벽돌)가 위 화면 밖에서 차례로 떨어져 쌓이며 완성된다.
 * 조각은 착지마다 가로로 퍼지고 세로로 눌린 뒤 큰 반동과 작은 반동으로 멈춘다.
 * 글자가 붙은 뒤에는 뒷판도 충격에 반응하고, 마지막 벽돌 뒤에는 로고 전체가 반동한다.
 * 확정 로고는 다 붙으면 TitleLogo(UIE_TitleLogo)로 바꿔 두고, D안은 조각이 그대로 남는다.
 * 새·바람·돛단배·구름은 부모가 그린다 — 위치는 WBP 기본값(하늘 새 높이·바람 높이)으로 가운데 로고를 비켜 간다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UTitleSkylineLoadingWidget : public ULoadingWidget
{
	GENERATED_BODY()

public:
	// AddToViewport 전에 부르면 첫 등장에, 그 뒤에 부르면 처음부터 다시 재생한다.
	void SetLogoVariant(ETitleLogoVariant InVariant);
	void ReplayIntro();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 확정 로고의 연출이 끝난 뒤 남는 정지 로고 — 조각과 같은 상자에 놓인다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> TitleLogo;

	// 확정 로고 조각 묶음(CanvasPanel) — 로고 상자 하나에 판·글자·벽돌이 배지 좌표로 겹쳐 있다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> LogoClip;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LogoPlate;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LogoWordA;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LogoWordB;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LogoBrick;

	// D안 조각 묶음 — 같은 상자·같은 구조. 없으면 D안을 골라도 확정 로고로 보인다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> LogoClipD;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> LogoPlateD;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> LogoWordAD;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> LogoWordBD;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> LogoBrickD;

	UPROPERTY(EditAnywhere, Category = "Loading|Intro")
	ETitleLogoVariant LogoVariant = ETitleLogoVariant::Final;

	// 첫 화면이 한 번 그려진 뒤 시작하도록 두는 대기(초)
	UPROPERTY(EditAnywhere, Category = "Loading|Intro", meta = (ClampMin = "0"))
	float IntroDelay = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Loading|Layout")
	FVector2D BackgroundCenterUV = FVector2D(0.5f, 0.65f);

	UPROPERTY(EditDefaultsOnly, Category = "Loading|Layout")
	FVector2D LogoCenterUV = FVector2D(0.5f, 0.38f);

	UPROPERTY(EditDefaultsOnly, Category = "Loading|Layout", meta = (ClampMin = "0.01"))
	float LogoWidthFraction = 0.40f;

	UPROPERTY(EditDefaultsOnly, Category = "Loading|Layout", meta = (ClampMin = "0.01"))
	float LogoCanvasAspect = 16.f / 9.f;

	UPROPERTY(EditDefaultsOnly, Category = "Loading|Layout", meta = (ClampMin = "0.01"))
	float LogoBadgeAspect = 984.f / 2280.f;

	UPROPERTY(EditDefaultsOnly, Category = "Loading|Layout", meta = (ClampMin = "0.01"))
	float LogoBadgeAspectD = 820.f / 1960.f;

private:
	// 지금 움직이는 로고 묶음 — 확정 또는 D안 BindWidget 을 가리킨다(소유는 위젯 트리)
	struct FLogoParts
	{
		UWidget* Clip = nullptr;
		UImage* Plate = nullptr;
		UImage* WordA = nullptr;
		UImage* WordB = nullptr;
		UImage* Brick = nullptr;
	};

	// 조각 하나의 낙하 — 위 화면 밖에서 떨어져 Arrive 초에 제자리에 붙는다
	struct FPieceDrop
	{
		UImage* Piece = nullptr;
		float Arrive = 0.f;
		float Fall = 0.2f;
		float StartAngle = 0.f;
		// 글자 착지 뒤 뒷판이 아래로 밀리는 폭(화면 단위)
		float Jolt = 0.f;
	};

	void SelectLogoParts();
	void ResetIntro();
	void ApplyIntro(float T);
	void FinishIntro();
	void ApplyResponsiveLayout(const FVector2D& ViewportSize);
	void GetDrops(TArray<FPieceDrop, TInlineAllocator<4>>& OutDrops) const;
	float GetIntroDuration() const;
	void PoseFallingPiece(const FPieceDrop& Drop, float T);
	// 조각의 제자리 중심·반 크기(화면 단위)와 배지 → 화면 배율. 화면 크기를 아직 모르면 false.
	bool GetPieceLayout(const UImage* Piece, FVector2D& OutCenter, FVector2D& OutHalfSize, float& OutBadgeToScreen) const;

	FLogoParts Parts;
	// 확정 로고만 연출 뒤 TitleLogo 로 바꾼다 — D안은 조각이 그대로 남는다
	bool bRestOnBadge = true;
	float IntroClock = 0.f;
	bool bIntroFinished = true;
	bool bConstructed = false;
	// 지난 틱의 위젯 크기(= 화면). 조각의 화면 밖 출발점을 이 크기로 잰다.
	FVector2D ScreenSize = FVector2D::ZeroVector;
};
