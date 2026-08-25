// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FloatingNumberWidget.generated.h"

class UTextBlock;
class UCanvasPanel;

// 자원 획득 시 HUD 카운터 옆에 뜨는 "+N" 플로팅 숫자.
// Pop(오버슈트) → 위로 떠오름 → 페이드아웃 후 자가 제거. (BrickCollectWidget 과 동일한 NativeTick 수동 보간 패턴)
UCLASS()
class COMPANYGROWTHRENEWAL_API UFloatingNumberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// AnchorAbsPos(보통 카운터 아이콘 중앙의 절대 스크린 좌표) 위에 +N 을 띄워 Host 캔버스에 추가하고 즉시 재생.
	// 클래스 미등록/지오메트리 무효 시 nullptr 반환(no-op).
	static UFloatingNumberWidget* Spawn(UUserWidget* Owner, UCanvasPanel* Host, const FVector2D& AnchorAbsPos, const FText& Text, FLinearColor Color, int32 StackIndex = 0);

	void Play(const FText& Text, FLinearColor Color, int32 InStackIndex);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// WBP 에 동일 이름 TextBlock 필수(디자이너가 폰트/아웃라인 등 스타일 소유). 런타임엔 색/텍스트만 주입.
	UPROPERTY(meta = (BindWidget))
	UTextBlock* NumberText = nullptr;

private:
	float Elapsed = 0.f;
	int32 StackIndex = 0;
	bool bPlaying = false;

	// 카운터가 화면 최상단이라 상승거리를 짧게 (크면 화면 밖 이탈) + 완전 불투명 구간을 길게 잡아 가독성 확보
	static constexpr float Lifetime = 1.4f;
	static constexpr float PopDuration = 0.18f;
	static constexpr float RiseDistance = 36.f;
	static constexpr float FadeStart = 1.0f;

	static float EaseOutBack(float A);
	static float EaseOutCubic(float A);
};
