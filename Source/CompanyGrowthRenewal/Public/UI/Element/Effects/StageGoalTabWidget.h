#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "StageGoalTabWidget.generated.h"

class UImage;

// 사무실 상단 바 분야 칸이 목표에 닿을 때 칸 아래에서 서랍처럼 빠져나왔다 들어가는 「목표 달성」 탭.
// 판·글자·체크 모양은 WBP(UIE_StageGoalTab)가 소유하고, 이 클래스는 1초짜리 움직임만 돌린다.
UCLASS()
class COMPANYGROWTHRENEWAL_API UStageGoalTabWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void Play(bool bReducedMotion);
	bool IsFinished() const { return Elapsed >= RetractEnd; }
	// 칸 아래 가장자리를 기준으로 탭 판이 차지하는 디자인 크기(그림자 여백 포함)
	FVector2D GetPanelPadding() const { return FVector2D(SideSpill, TuckIn); }
	float GetDesignHeight() const { return DesignHeight; }
	// 판 폭 = 칸 폭과 내용(체크+글자) 폭 중 큰 쪽 — 분야가 하나뿐인 좁은 칸에서 글자가 잘렸다. 캔버스에 붙인 뒤 부른다
	float GetBodyWidthFor(float CellWidth);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 미끄러지는 부분. 루트는 ClipToBounds 라 바 아래 가장자리 위로는 그려지지 않는다
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> TabRoot;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> CheckImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> SheenImage;

	// 판이 칸 좌우로 더 나가는 폭(위 모서리 곡선 + 그림자) — 텍스처 T_UI_StageGoalTab 의 여백과 같아야 한다
	UPROPERTY(EditAnywhere, Category = "Goal Tab") float SideSpill = 22.f;
	// 상단 바 아래 가장자리 밑으로 숨기는 높이 — 바 밑에서 나오는 것처럼 보이게 한다
	UPROPERTY(EditAnywhere, Category = "Goal Tab") float TuckIn = 7.f;
	UPROPERTY(EditAnywhere, Category = "Goal Tab") float DesignHeight = 98.f;
	// 내용 좌우로 남기는 판 여백 — 아래 둥근 모서리에 글자가 닿지 않게 한다
	UPROPERTY(EditAnywhere, Category = "Goal Tab") float ContentInset = 30.f;
	UPROPERTY(EditAnywhere, Category = "Goal Tab") float Overshoot = 6.75f;
	UPROPERTY(EditAnywhere, Category = "Goal Tab") float DropEnd = .10f;
	UPROPERTY(EditAnywhere, Category = "Goal Tab") float SettleEnd = .16f;
	UPROPERTY(EditAnywhere, Category = "Goal Tab") float HoldEnd = .90f;
	UPROPERTY(EditAnywhere, Category = "Goal Tab") float RetractEnd = 1.02f;

private:
	void Apply(float Time);

	float Elapsed = -1.f;
	bool bReduced = false;
};
