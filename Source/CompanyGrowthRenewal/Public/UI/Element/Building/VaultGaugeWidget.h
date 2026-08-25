#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/GaugeHealth.h"
#include "VaultGaugeWidget.generated.h"

class UProgressBar;
class USizeBox;
class UImage;

/**
 * 건물 위 금고 채움 게이지.
 * 순수 표시 위젯 — 자기 위치와 생명주기를 모른다(InGameLayerWidget 이 풀로 관리).
 * 담당 구간은 0~99% 이고 100% 는 기존 VaultFull 버블이 인계받는다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UVaultGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 채움, 건강도, 표현 모드를 한 콜로 적용한다.
	 * Health == Idle 이면 FillPct 는 무시되며 Pearl 은 표시하지 않는다.
	 */
	void SetPresentation(float FillPct, EGaugeHealth Health, EVaultGaugePresentation Presentation);

	/** 원근 보정된 폭 적용. 같은 값이면 건너뛴다(레이아웃 재계산 회피). */
	void SetGaugeWidth(float Width);

protected:
	virtual void NativeConstruct() override;

	// 데이터 주입 대상이라 required — Optional 이면 WBP 배선 누락이 silent null 이 된다
	UPROPERTY(meta = (BindWidget))
	UProgressBar* FillBar = nullptr;

	// 원근에 따라 폭이 바뀌므로 SizeBox 를 직접 물린다.
	// ⚠ GetRootWidget() 캐스팅으로 찾지 말 것 — WBP 루트 구조가 바뀌면 조용히 nullptr 이 된다.
	UPROPERTY(meta = (BindWidget))
	USizeBox* GaugeBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* FillTip = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* EndingRim = nullptr;

	UPROPERTY(meta = (BindWidget))
	USizeBox* PearlBox = nullptr;

	UPROPERTY(EditAnywhere, Category = "VaultGauge|Color")
	FLinearColor ActiveFillColor = FLinearColor(0.114f, 0.573f, 0.253f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "VaultGauge|Color")
	FLinearColor EndingRimColor = FLinearColor(0.807f, 0.366f, 0.047f, 1.0f);

private:
	void ApplyBarOpacity(EGaugeHealth Health);
	void RefreshFillTipGeometry();

	// 같은 값 재대입 방어 — Slate invalidate 누적을 막는다.
	// BubbleContainerWidget 의 LastAppliedVisibility 와 같은 뿌리의 함정.
	EGaugeHealth LastHealth = EGaugeHealth::Idle;
	EVaultGaugePresentation LastPresentation = EVaultGaugePresentation::Bar;
	float LastFill = -1.0f;
	float LastWidth = 104.0f;
	float LastBarOpacity = -1.0f;
};
