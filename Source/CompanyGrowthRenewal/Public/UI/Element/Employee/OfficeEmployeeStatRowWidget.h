#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Data/EmployeeTypes.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "OfficeEmployeeStatRowWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;
class UButtonWidget;
class UEmployeeManager;

/**
 * 사무실 직원 패널 성장 탭의 능력치 한 행 (UIE_OfficeEmployeeStatRow).
 * 아이콘 웰 · 이름 + 「Lv.N」 배지 → 한 줄 설명 → 「현재 > 다음」 → ★보너스(0이면 접힘) · 오른쪽 SP 투자 버튼(UIE_EmployeeStatInvestButton).
 * 표시 로직은 직원창 RefreshHeroStats / HandleStatPlusClicked 와 같은 헬퍼를 쓴다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeEmployeeStatRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void Configure(int32 InEmployeeId, EEmployeeStatIndex InStat);
	void Refresh();

	UButtonWidget* GetInvestButton() const { return InvestButton; }
	EEmployeeStatIndex GetStat() const { return Stat; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> StatIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatNameText;

	// 이름 오른쪽 「Lv.N」 배지 글자 — 그 능력치에 SP 를 넣은 횟수
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatLevelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatDescText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatCurrentText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatNextText;

	// 「현재 > 다음」 사이 기호 — 더 오를 수 없는 값이면 다음 값과 함께 접는다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatArrowText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatBonusText;

	// 비용 칩(SP 아이콘 + 비용 숫자 CostText)과 「강화」 라벨은 버튼 WBP 안에 있다
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButtonWidget> InvestButton;

	// 능력치별 글리프·한 줄 설명 (인덱스 = EEmployeeStatIndex). 값은 직원창 원본에서 옮겨 WBP 기본값에 둔다
	UPROPERTY(EditDefaultsOnly, Category = "Stat Row")
	TArray<TSoftObjectPtr<UTexture2D>> StatIcons;

	UPROPERTY(EditDefaultsOnly, Category = "Stat Row")
	TArray<FText> StatHints;

	// 꺼진 투자 버튼 라벨 — 흰 라벨은 비활성 효과로 물이 빠져 회색 판 위에서 안 읽힌다(설계 §3 차콜 #566273)
	UPROPERTY(EditDefaultsOnly, Category = "Stat Row")
	FSlateColor DisabledLabelColor = FSlateColor(FLinearColor(0.0931f, 0.1221f, 0.1714f));

private:
	UPROPERTY()
	TObjectPtr<UEmployeeManager> EmployeeManager;

	int32 EmployeeId = INDEX_NONE;
	EEmployeeStatIndex Stat = EEmployeeStatIndex::WorkSpeed;
	// 같은 직원의 값이 바뀐 순간에만 카운트업한다 — 직원 전환은 즉시 표시
	int32 LastEffectiveValue = INDEX_NONE;
	FNumberCountUpAnimation ValueCount;
	// UCommonUserWidget 은 DisableNativeTick 이라 카운트업은 짧은 타이머로 돌린다
	FTimerHandle ValueCountTimer;
	// 켜진 라벨 색 = WBP 인스턴스 값 (상태가 바뀔 때 되돌린다)
	FSlateColor EnabledLabelColor;
	// 투자 버튼 WBP 안의 비용 숫자 (UIE_EmployeeStatInvestButton.CostText)
	UPROPERTY()
	TObjectPtr<UTextBlock> CostText;

	void ApplyStaticContent();
	void StepValueCount();
	void StopValueCount();
	void HandleInvestClicked();
};
