#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RevenueRateChipWidget.generated.h"

class UCommonTextBlock;
class UImage;

/**
 * HUD 상단 실시간 수익 칩. Money 칩 우측에 인접해 "보유액 + 유량" 을 한 묶음으로 만든다.
 *
 * ⚠ 라벨 텍스트를 두지 않는다. GetCompanyNetPerSec 의 계산에는 차감 항목이 하나도 없어
 * (Base × TotalMult × IncomeMult × StatBonus × Decay) 회계적으로 순수익이 아니라 총 유입액이고,
 * 코드의 NetPerSec 은 내부 계산 용어다. "+" 와 "/s" 가 이미 늘어나는 속도를 말하므로
 * 단어를 고를 필요가 없다. 장래에 라벨이 꼭 필요해지면 "순수익" 이 아니라 "수입".
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API URevenueRateChipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 회사 전체 유량을 주입한다. 1.0/s 미만은 칩을 유지한 채 뮤트된 0/s로 표시한다. */
	void SetRate(double PerSec);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 데이터 주입 대상 — required
	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* RateText = nullptr;

	// 펄스 대상 — required (없으면 펄스가 조용히 사라져 "왜 안 움직이나" 오진을 부른다)
	UPROPERTY(meta = (BindWidget))
	UImage* TrendGlyph = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* ChipBG = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* ChipLine = nullptr;

	// ===== 사건형 펄스 =====
	// 첫 활성 수입이 생기거나 직전 활성 샘플에서 의미 있게 변할 때만 짧게 강조한다.
	UPROPERTY(EditAnywhere, Category = "RateChip|Pulse", meta = (ClampMin = "0.05"))
	float PulseDuration = 0.45f;

	UPROPERTY(EditAnywhere, Category = "RateChip|Pulse", meta = (ClampMin = "0.0"))
	float PulseScaleGain = 0.08f;

	UPROPERTY(EditAnywhere, Category = "RateChip|Pulse", meta = (ClampMin = "0.0"))
	double PulseRelativeThreshold = 0.05;

	UPROPERTY(EditAnywhere, Category = "RateChip|Color")
	FLinearColor RateColor = FLinearColor(0.107023102051626f, 0.584078415397575f, 0.254152092796134f, 1.0f);   // #5CC98A

	UPROPERTY(EditAnywhere, Category = "RateChip|Color")
	FLinearColor RatePulseColor = FLinearColor(0.780f, 1.000f, 0.855f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "RateChip|Color")
	FLinearColor MutedRateColor = FLinearColor(0.309f, 0.366f, 0.468f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "RateChip|Color", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MutedGlyphOpacity = 0.45f;

private:
	void BeginPulse();
	void ResetPulseVisual();
	void UpdateChipMaterialSize(const FGeometry& MyGeometry);

	FVector2D LastChipMatSize = FVector2D::ZeroVector;

	bool bPulseActive = false;
	bool bHasActiveRate = false;
	float PulseEpochSeconds = 0.0f;

	double LastObservedRate = -1.0;
	int64 LastDisplayedRate = INDEX_NONE;
};
