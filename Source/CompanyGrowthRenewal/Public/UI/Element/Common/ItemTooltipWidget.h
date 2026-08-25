// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemTooltipWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UTexture2D;
class UWidgetAnimation;

// 툴팁 배치 기준. RightOfAnchor=앵커(트리거 우측중앙)의 오른쪽 / BelowAnchor=앵커(트리거 하단중앙)의 아래.
UENUM(BlueprintType)
enum class ETooltipAnchor : uint8
{
	RightOfAnchor,
	BelowAnchor
};

/**
 * UIE_ItemTooltip
 * 아이템/재료 클릭 시 옆에 잠깐 떴다 사라지는 인라인 팝오버.
 * 모달 X — 게임 흐름 안 막음. viewport 직접 add, 자동 dismiss.
 *
 * 호출자는 인스턴스 1개 보유 후 ShowAt 으로 위치/데이터/지속시간 갱신.
 * BindWidgetAnimOptional 로 ShowAnim/HideAnim 있으면 자동 재생, 없으면 SetVisibility 토글.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UItemTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 위치 + 데이터 갱신 + auto dismiss 타이머 시작.
	 * @param AbsoluteScreenPos 화면 절대 좌표 (호출자가 카드 Geometry::LocalToAbsolute 로 계산)
	 * @param InName 아이템 이름
	 * @param InIcon 아이콘 (nullptr 가능 — EntityImage 안 변경)
	 * @param InDesc 설명 (Empty 면 DescText Collapsed)
	 * @param DurationSec auto dismiss 까지 시간. 0 이면 수동 Hide 까지 유지
	 * @param ZOrder viewport ZOrder. 기본 101(=ProductionStartPopup 100 짝). 더 높은 오버레이(보상/가챠 리빌) 위에서는 host+1 을 넘긴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemTooltip")
	void ShowAt(FVector2D AbsoluteScreenPos, const FText& InName, UTexture2D* InIcon, const FText& InDesc, int32 InBasePrice = 0, float DurationSec = 2.5f, ETooltipAnchor AnchorMode = ETooltipAnchor::RightOfAnchor, int32 ZOrder = 101);

	UFUNCTION(BlueprintCallable, Category = "ItemTooltip")
	void Hide();

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> EntityImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> NameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> DescText;

	// "기준가 50" 같은 텍스트 표시. WBP 에 없으면 단가 미표시 (옵셔널)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> PriceText;

	// 옵셔널 등장/퇴장 애니메이션 — 디자이너가 WBP 에서 만들 수 있음
	UPROPERTY(Transient, meta = (BindWidgetAnim, OptionalWidget = true))
	TObjectPtr<UWidgetAnimation> ShowAnim;

	UPROPERTY(Transient, meta = (BindWidgetAnim, OptionalWidget = true))
	TObjectPtr<UWidgetAnimation> HideAnim;

private:
	FTimerHandle DismissTimerHandle;

	void ApplyData(const FText& InName, UTexture2D* InIcon, const FText& InDesc, int32 InBasePrice);
	void HandleAutoDismiss();
};
