#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "TipBandWidget.generated.h"

class UFont;
class UTexture2D;

/**
 * 로딩 팁 밴드의 외형 노브. **전환 오버레이와 WBP 가 같은 구조체를 쓴다** ― 한 곳에서 그리므로 갈라지지 않는다.
 * 비율은 전부 위젯 높이 기준이라 16:10~19.5:9 전 구간에서 같은 비례로 따라간다.
 */
USTRUCT(BlueprintType)
struct FLoadingTipBandStyle
{
	GENERATED_BODY()

	// 밴드 하단 위치(높이 비). 아래 7% 는 폰 제스처 바/라운드 코너용으로 비운다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Band")
	float BandBottom = 0.930f;

	// 밴드 세로 여백(높이 비). 1440 기준 36px ― 판 여백 기준
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Band")
	float BandPadV = 0.025f;

	// 밴드 좌우 인셋(폭 비)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Band")
	float BandInsetX = 0.090f;

	// 양끝 소멸 구간(밴드 폭 비). 카탈로그 밴드 문법 ― 선과 띠는 양끝에서 사라진다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Band")
	float FadeRatio = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Band")
	FLinearColor BandTint = FLinearColor(0.035f, 0.051f, 0.078f, 0.74f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Band")
	FLinearColor HairlineTint = FLinearColor(0.902f, 0.914f, 0.941f, 0.62f);

	// 본문 글꼴 크기(높이 비). 1440 기준 38 = Heading_L 사이즈 티어
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	float BodySizeRatio = 0.02639f;

	// 라벨 글꼴 크기(높이 비). 1440 기준 32 = Heading_M
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	float LabelSizeRatio = 0.02222f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	FLinearColor BodyTint = FLinearColor(0.925f, 0.933f, 0.941f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	FLinearColor LabelTint = FLinearColor(0.725f, 0.761f, 0.812f, 0.95f);

	// 본문 글꼴 대비 전구 크기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	float IconScale = 1.15f;

	// 와이프 패널의 주황 ― 이 화면이 이미 가진 색이라 새 색을 들이지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon")
	FLinearColor IconTint = FLinearColor(1.f, 0.714f, 0.361f, 1.f);
};

/**
 * 로딩 팁 밴드를 **에디터에서 보고 편집하기 위한** UMG 래퍼.
 *
 * 실제 전환 화면은 이 위젯을 쓰지 않는다 ― `OpenLevel` 이 게임 스레드를 막는 동안
 * MoviePlayer 스레드가 그려야 해서 UMG 가 존재할 수 없기 때문이다(`SLevelTransitionOverlay` 참조).
 * **대신 그리는 코드가 같다**: 둘 다 `PaintLoadingTipBand()` 한 함수를 부른다.
 * 그래서 여기서 숫자를 바꿔 보고 맞춘 값을 전환 쪽 기본값으로 옮기면 된다.
 */
UCLASS(meta = (DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UTipBandWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 디자이너 미리보기 문구. 런타임 전환은 DT_LoadingTip 에서 뽑은 문구를 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tip")
	FText PreviewTip = FText::FromString(TEXT("랭킹에 오른 다른 회사의 도시는 직접 방문할 수 있습니다."));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tip")
	FLoadingTipBandStyle Style;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tip")
	TObjectPtr<UFont> BodyFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tip")
	TObjectPtr<UFont> LabelFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tip")
	TObjectPtr<UTexture2D> IconTexture;

	/** 런타임에 문구만 바꿔 끼울 때 (전환 밖에서 이 위젯을 쓰는 경우) */
	UFUNCTION(BlueprintCallable, Category = "Tip")
	void SetTip(const FText& InTip);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	TSharedPtr<class SLoadingTipBand> Band;
};
