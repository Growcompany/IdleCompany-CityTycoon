// 개별 버블 비주얼 위젯 (WBP 기반)
// BubbleContainerWidget이 CreateWidget으로 생성하고, Container가 외부에서 애니메이션 제어
// WBP_BubbleElement에서 크기/패딩/정렬을 에디터에서 조절
// UCommonButtonBase 상속으로 탭/클릭 감지 자동 처리

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Enum/BubbleType.h"
#include "BubbleElementWidget.generated.h"

class UImage;
class UOverlay;
class UTexture2D;
class UMaterialInterface;

DECLARE_DELEGATE_TwoParams(FOnBubbleElementClicked, int32 /*BuildingIndex*/, EBubbleType /*Type*/);

UCLASS()
class COMPANYGROWTHRENEWAL_API UBubbleElementWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	// 셸 머티리얼 설정 (null 시 텍스처 경로로 폴백)
	void SetBGMaterial(UMaterialInterface* InMaterial);

	// 배경 텍스처 설정 (null 시 폴백 브러시)
	void SetBGTexture(UTexture2D* InTexture);

	// 아이콘 텍스처 설정 (null 시 폴백 브러시)
	void SetIconTexture(UTexture2D* InTexture);

	// 셸 + 글리프 일괄 설정. InShell이 null이면 InBGTexture로 폴백
	void SetBubbleVisual(UMaterialInterface* InShell, UTexture2D* InBGTexture, UTexture2D* InIconTexture);

	// Idle 바운스 — 접지 그림자가 따라 뜨지 않도록 BounceRoot만 움직인다
	void ApplyIdleBounce(float YOffset);

	// Container가 생성 시 호출 — 건물 인덱스 + 버블 타입 저장
	void SetBubbleInfo(int32 InBuildingIndex, EBubbleType InType);

	// 클릭 시 외부에 통지하는 델리게이트
	FOnBubbleElementClicked OnBubbleClicked;

protected:
	virtual void NativeOnClicked() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* BGImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* IconImage;

	// 셸+글리프만 감싸는 바운스 대상. 그림자/글로우는 밖에 두어 바닥에 고정된다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UOverlay* BounceRoot;

private:
	int32 BuildingIndex = INDEX_NONE;
	EBubbleType BubbleType = EBubbleType::None;
};
