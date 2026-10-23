#pragma once

#include "CoreMinimal.h"
#include "UI/AnimatedActivatableWidget.h"
#include "InstantFinishConfirmWidget.generated.h"

class UModalFrameWidget;
class UImage;
class UCommonTextBlock;
class UProgressBar;
class UButtonWidget;
class UCostActionButtonWidget;
class UTexture2D;

/**
 * UI_InstantFinishConfirm
 * 국가 서랍 공장·광산 라인의 다이아 즉시 완료 확인 창. 서랍이 AddToViewport 로 띄우고 결과 이벤트를 받아 닫는다.
 * 다이아 부족 판정·안내는 구매 경로(클라우드 경제) 한 곳이 한다 ― 확인 버튼의 비용 게이트는 꺼 둔다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UInstantFinishConfirmWidget : public UAnimatedActivatableWidget
{
	GENERATED_BODY()

public:
	void Configure(UTexture2D* Icon, const FText& Name, int64 Current, int64 Target, double Unit01,
		const FTimespan& Remaining, int32 Diamonds, bool bMine);

	DECLARE_EVENT(UInstantFinishConfirmWidget, FInstantFinishConfirmEvent);
	FInstantFinishConfirmEvent& OnConfirmed() { return ConfirmedEvent; }
	FInstantFinishConfirmEvent& OnCancelled() { return CancelledEvent; }

	// 결과 이벤트 없이 닫는다(서랍 닫힘·국가 변경·자연 완료)
	void Dismiss();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UModalFrameWidget> ModalFrame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> ProductIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ProductName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> StatusText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> BarText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> MessageText;

	// 이름이 CancelButton 이라 뒤로가기(CGBackNav)가 이 버튼을 누른 것으로 처리한다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButtonWidget> CancelButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCostActionButtonWidget> ConfirmButton;

private:
	FInstantFinishConfirmEvent ConfirmedEvent;
	FInstantFinishConfirmEvent CancelledEvent;

	// 한 번 고르면 끝 ― 닫히기 전 연타·뒤로가기가 두 번째 결과를 내지 않게
	bool bResolved = false;

	void HandleConfirmClicked();
	void HandleCancelClicked();

	UFUNCTION()
	void HandleFrameCloseClicked();
};
