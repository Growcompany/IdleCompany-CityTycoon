#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Buttons/CostActionButtonWidget.h"
#include "DisciplineCellButtonWidget.generated.h"

class UTextBlock;
class UImage;
class UOverlay;
class UBorder;
class UTexture2D;

/**
 * 직능 투자 셀 버튼 (UIE_DisciplineCellBtn) — 셀 전체가 강화 버튼.
 * UCostActionButtonWidget 파생: 버튼 배선/OnPressed·OnReleased/SetEnabled 재사용.
 * 비용 게이트(SetCost/afford)는 사용하지 않는다 — SP는 직원별 자원이라 카드가 SetInvestEnabled 로 게이팅.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UDisciplineCellButtonWidget : public UCostActionButtonWidget
{
	GENERATED_BODY()

public:
	// 카드가 매 refresh 주입. Invested==0 이면 (+N) Collapsed
	UFUNCTION(BlueprintCallable, Category = "DisciplineCell")
	void SetDiscipline(const FText& Name, int32 Total, int32 Invested);

	// SP>0 게이팅 — 내부 Btn(base)과 셀 자신(GetIsEnabled, M9 가이드 게이트가 읽는 축)을 함께 전환
	UFUNCTION(BlueprintCallable, Category = "DisciplineCell")
	void SetInvestEnabled(bool bEnabled);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 직능 글리프 — 인스턴스별 지정(셀 WBP 내부 IconImg 는 카드 디자이너에서 직접 오버라이드 불가)
	UPROPERTY(EditAnywhere, Category = "DisciplineCell")
	TSoftObjectPtr<UTexture2D> IconTexture;

	// 눌림 트랜스폼 깊이(px)
	UPROPERTY(EditAnywhere, Category = "DisciplineCell")
	float PressDepth = 3.f;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* NameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* ValueText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* BonusText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* IconImg;

	// 눌림 시 Y+PressDepth 로 내려앉는 면 — 립(LipImg)은 루트에 남아 "눌림"이 성립
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UOverlay* FaceBox;

	// 상태 틴트 대상: 골드 스트립 / 흰 비용 칩 / 칩 숫자
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* StripBG;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UBorder* CostPill;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* CostText;

private:
	void HandleCellPressed();
	void HandleCellReleased();
	// 활성/비활성 팔레트 일괄 적용 (런타임 상태 주입 — 정적 기본값은 WBP 소유)
	void ApplyInvestVisuals(bool bEnabled);

	bool bInvestEnabled = true;
};
