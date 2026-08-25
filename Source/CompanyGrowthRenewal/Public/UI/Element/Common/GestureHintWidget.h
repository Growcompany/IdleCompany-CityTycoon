#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Element/Common/GestureHintTypes.h"
#include "GestureHintWidget.generated.h"

class UImage;
class UTextBlock;
class URadialProgressWidget;
class UGestureHintFxWidget;

/**
 * 제스처 힌트 — 손 글리프(탭/홀드/드래그) + 홀드 라디얼 링 + 라벨. 순수 표시 위젯(위치/수명은 소유자 책임).
 * 루트 SizeBox 320×320 의 중심이 검지 손끝 = 앵커. 소유자는 캔버스 슬롯 Alignment(0.5,0.5) 로 손끝을 대상에 둔다.
 * 탭 점 펄스/드래그 점선은 MotionFx 자식이 그린다 — MotionFx 는 WBP 에서 HandImage 보다 먼저(아래) 배치 + 슬롯
 * 320×320 + Alignment(0.5,0.5) 명시. 먼저여야 손 글리프가 이펙트 위에 오고(자식 z-order 가 곧 페인트 순서), AutoSize 는 금지 — Fx 원점이
 * 자기 지오메트리 중심이라 desired 0 이면 좌상단에 그린다.
 * 설계 = docs/superpowers/specs/2026-08-22-tutorial-gesture-hints-design.md §3
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UGestureHintWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr float BoxSize = 320.f;

	void SetGesture(EGestureHintKind Kind);
	void SetLabel(const FText& Label);
	EGestureHintKind GetGesture() const { return Gesture; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 데이터 주입 대상이라 required — Optional 이면 WBP 배선 누락이 silent null 이 된다
	UPROPERTY(meta = (BindWidget))
	UImage* HandImage = nullptr;
	UPROPERTY(meta = (BindWidget))
	URadialProgressWidget* HoldRing = nullptr;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* LabelText = nullptr;
	UPROPERTY(meta = (BindWidget))
	UGestureHintFxWidget* MotionFx = nullptr;

private:
	EGestureHintKind Gesture = EGestureHintKind::None;
	// 모션 위상 누적 (Tick 갱신, MotionFx 는 주입받아 그리기만). 제스처가 바뀔 때만 0 — 같은 제스처를 다시
	// 보여주면 루프가 이어진다(SetGesture 의 early-return). 소유자는 위상-0 재시작을 전제하지 말 것
	float Elapsed = 0.f;
	void ApplyGestureVisibility();
};
