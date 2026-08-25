#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "DisciplineCardWidget.generated.h"

class UTextBlock;
class UEmployeeManager;
class UDisciplineRadarWidget;
class UDisciplineCellButtonWidget;
class UCostActionButtonWidget;

/**
 * 직능 투자 카드 (UIE_DisciplineCard) — 직원 1명의 6직능(EProductionDiscipline)에 스킬포인트(SP)를 투자/리셋.
 *
 * 셀 전체 = 강화 버튼(UIE_DisciplineCellBtn ×6): 탭 1포인트, 홀드 연속 투자(Factory 홀드 패턴 미러).
 * 재사용 컴포넌트 — 정적 스타일/레이아웃은 WBP, C++는 데이터 주입·상호작용만.
 * 상위 직원창(EmployeeWindowWidget)이 매 오픈 Configure(EmployeeID) 로 구동하는 유일한 진입점.
 *
 * 갱신 모델 = UEmployeeManager::OnEmployeeDisciplineChanged(네이티브 멀티캐스트) 구독.
 *   Configure 마다 재바인딩(RemoveAll→AddUObject), NativeDestruct 에서 해제(재사용 위젯 누적 방지).
 * 재분배 = 스톡 UIE_CostActionButton — 다이아 50 자동 게이팅(부족 시 입력 삼킴+토스트는 버튼 내장).
 * 직능 표시명 = DT_DisciplineDisplay 데이터 주도(슬롯 의미는 enum 고정, 표시만 산업별 재해석).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UDisciplineCardWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 상위 직원창이 매 오픈 호출 — EmployeeID 주입 + 델리게이트 재바인딩 + 표시 갱신
	UFUNCTION(BlueprintCallable, Category = "Employee|Discipline")
	void Configure(int32 InEmployeeID);

	// M9 튜토리얼 가이드 링 타겟 — 첫 직능 셀 버튼 (SP=0 판정은 호출부가 GetIsEnabled 로)
	UWidget* GetFirstInvestButtonWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ===== 헤더 =====

	// 남은 스킬포인트 배지 (AvailableSkillPoints)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* SkillPointText;

	// ===== 레이더 (v7 폴리시 — 특화 실루엣, 등급색 fill) =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UDisciplineRadarWidget* RadarWidget;

	// ===== 6직능 셀 (인덱스 = EProductionDiscipline 0~5, 셀 전체가 강화 버튼) =====

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UDisciplineCellButtonWidget* DiscCell0;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UDisciplineCellButtonWidget* DiscCell1;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UDisciplineCellButtonWidget* DiscCell2;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UDisciplineCellButtonWidget* DiscCell3;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UDisciplineCellButtonWidget* DiscCell4;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UDisciplineCellButtonWidget* DiscCell5;

	// 레이더 정규화 상한 하한값 — 실제 상한 = max(이 값, 6직능 최대값)
	UPROPERTY(EditAnywhere, Category = "Discipline")
	int32 GaugeDisplayMin = 15;

	// 홀드 연속 투자 노브 (스펙 §3 확정값)
	UPROPERTY(EditAnywhere, Category = "Discipline|Hold")
	float HoldStartDelay = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Discipline|Hold")
	float HoldRepeatInterval = 0.11f;

	// ===== 하단 =====

	// 재분배(리셋) — 스톡 CostActionButton. 비용/게이팅은 SetCost(50, Diamond), 투자분 0 = SetLockedState
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCostActionButtonWidget* ResetButton;

private:
	// 현재 표시 직원 (-1 = 없음, 델리게이트 키 필터)
	int32 CurrentEmployeeID = -1;

	UPROPERTY()
	UEmployeeManager* EmployeeManager;

	// BindWidget 6셀 포인터 모음 (인덱스 = EProductionDiscipline) — 채움/바인딩 루프용
	TArray<UDisciplineCellButtonWidget*> DiscCells;

	// 홀드 상태
	FTimerHandle InvestHoldTimerHandle;
	int32 CurrentHoldDiscipline = 0;

	// SP 배지 + 6셀 + 재분배 상태 일괄 갱신
	void RefreshFromData();

	// 1포인트 투자 시도 — 성공 시 강화음 재생, 실패(SP 소진) false
	bool TryInvest(int32 DisciplineIndex);

	// 셀 Pressed = 즉시 1투자 + 홀드 타이머 시작 / Released = 정지 (OnClicked 미사용 — 이중 투자 방지)
	void HandleCellPressed(int32 DisciplineIndex);
	void HandleCellReleased();
	void OnInvestHoldTick();
	void StopInvestHold();

	// 재분배 — 버튼이 다이아 게이팅을 하므로 여기 도달 = 원칙상 지불 가능 (stale afford 레이스만 폴백 토스트)
	void HandleResetClicked();

	// 매니저 브로드캐스트 — 내 직원이면 갱신
	void HandleDisciplineChanged(int32 ChangedEmployeeID);
};
