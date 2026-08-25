// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/EmployeeTypes.h"
#include "OccStatRowWidget.generated.h"

class UTextBlock;
class UButtonWidget;
class UProgressBar;

// 행의 [+] 클릭 — 자신의 StatIndex 를 부모(점유 뷰)로 올림 (네이티브, 위젯 내부 통신용)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnOccStatRowInvest, EEmployeeStatIndex /*StatIndex*/);

/**
 * 점유 직원 뷰의 스탯 1행 (스탯명 + 현재값 + [+])
 *
 * 역할 = 한 스탯의 표시 + [+] 의도 발신. 실제 투자/저장은 부모→호스트→EmployeeManager.
 * - SetStat(StatIndex, Value): 스탯명(enum DisplayName)/값 채우고 StatIndex 기억
 * - SetCanInvest(bCanInvest): 남은 포인트 0이면 [+] 숨김(비활성)
 * - [+] → OnRowInvestRequested.Broadcast(StatIndex) (부모가 구독해 재발신)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOccStatRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Occupant|StatRow")
	void SetStat(EEmployeeStatIndex InStatIndex, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Occupant|StatRow")
	void SetCanInvest(bool bCanInvest);

	// 행 [+] 의도 (부모 점유 뷰가 구독) — 위젯 내부 통신이라 네이티브 멀티캐스트
	FOnOccStatRowInvest OnRowInvestRequested;

	// M9 미션 가이드 — [+] 버튼 하이라이트 타겟 (본문은 .cpp, UButtonWidget→UWidget 업캐스트)
	UWidget* GetPlusButtonWidget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* StatNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* StatValueText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UButtonWidget* PlusButton;

	// 스탯 게이지 바 (선택 — 직원창 라이트 변형 UIE_EmployeeStatRow 용). SetStat 이 Value/StatBarMax 퍼센트 주입
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UProgressBar* StatBar;

	// 바 만점 기준값 (표시 스케일 — WBP 변형에서 튜닝)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occupant|StatRow")
	float StatBarMax = 40.f;

private:
	EEmployeeStatIndex StatIndex = EEmployeeStatIndex::WorkSpeed;

	void OnPlusClicked();
};
