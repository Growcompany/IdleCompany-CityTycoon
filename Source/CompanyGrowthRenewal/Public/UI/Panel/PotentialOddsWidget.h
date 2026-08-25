#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Enum/LootBoxRarity.h"
#include "PotentialOddsWidget.generated.h"

class UCloseButtonWidget;
class UCommonTextBlock;
class UVerticalBox;
class UWidget;

/**
 * 잠재 재설정 확률표 — 명함 3종 × 실효 등급 확률.
 *
 * 기본 추첨 확률만 보여주면 래칫이 달성 등급 아래를 한 칸에 뭉치므로 대부분의 직원에게 틀린 표가 된다.
 * 그래서 (선택 직원의 달성 등급 × 명함 상한)으로 계산한 실효 확률을 낸다 — GetEffectiveRarityOdds.
 * 스택 push 가 아니라 뷰포트 오버레이 (EmployeeGachaProbabilityWidget 선례 — 아래 직원창을 살려둔다).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UPotentialOddsWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	void Setup(const FText& InEmployeeName, ELootBoxRarity InMaxAchieved);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 닫기 버튼만 Optional — 빌더(build_trees.py)에 클래스 미발견 시 생략하는 폴백 경로가 있고,
	// 없어도 BackgroundBtn(바깥 클릭)으로 닫을 수 있다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCloseButtonWidget* UIE_CloseButton;

	// 아래는 전부 required — 데이터 주입/기능 대상이라 없으면 조용한 빈칸이 아니라 즉시 실패해야 한다.
	// (원격 WBP 편집으로 위젯이 유실된 사고 이력이 있어, Optional 은 그 사고를 침묵시킨다)

	// 바깥 클릭 닫기 (투명 풀스크린 — ProductSellModal 패턴)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<class UButton> BackgroundBtn;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* TargetNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* TargetGradeText;

	// ── 3열: 종이/골드/블랙 (CubeTypes 순서 고정) ──
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* CardName0;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* CardName1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* CardName2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* CardCap0;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* CardCap1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* CardCap2;

	// 행 주입 대상 — 없으면 표가 통째로 비므로 required
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVerticalBox* OddsBox0;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVerticalBox* OddsBox1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UVerticalBox* OddsBox2;

	// "등급이 오르지 않습니다" 칩 — 실효 확률이 한 등급 100% 일 때만 표시
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UWidget* LockChip0;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UWidget* LockChip1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UWidget* LockChip2;

	// 등급별 옵션 값 범위 5칸
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* RangeGrade0;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* RangeGrade1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* RangeGrade2;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* RangeGrade3;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* RangeGrade4;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* RangeValue0;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* RangeValue1;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* RangeValue2;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* RangeValue3;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* RangeValue4;

	// 기본 추첨 확률 각주
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* BaseOddsText;

private:
	ELootBoxRarity MaxAchieved = ELootBoxRarity::Common;

	// Setup 주입 전에는 표를 짓지 않는다 — 기본 등급으로 만든 틀린 표를 그리지 않기 위함
	bool bSetupDone = false;

	void Rebuild();
	void BuildColumn(int32 Index);
	void BuildValueRanges();

	UFUNCTION()
	void OnCloseClicked();

	UFUNCTION()
	void HandleDimClicked();
};
