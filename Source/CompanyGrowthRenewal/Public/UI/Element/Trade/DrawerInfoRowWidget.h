#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DrawerInfoRowWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UButtonWidget;
class USizeBox;
class UTexture2D;

UENUM(BlueprintType)
enum class EDrawerRowTone : uint8
{
	Normal,
	Good,
	Pending,
	Muted
};

/**
 * 국가 서랍의 한 줄 정보 행 (UIE_DrawerInfoRow) — 항구 주문 줄·창고 줄·빈 공장 라인 줄이 같은 행을 쓴다.
 * 비어 있는 칸(아이콘·번호·설명·값·버튼)은 접힌다. 색은 상태별 값만 런타임에 고르고 값 자체는 WBP 기본값이 갖는다.
 */
UCLASS(meta = (DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UDrawerInfoRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	DECLARE_EVENT_OneParam(UDrawerInfoRowWidget, FDrawerRowActionEvent, UDrawerInfoRowWidget*);
	FDrawerRowActionEvent& OnActionClicked() { return ActionClicked; }

	void SetRow(UTexture2D* Icon, const FText& Number, const FText& Name, bool bDimName,
		const FText& Detail, const FText& Value, EDrawerRowTone ValueTone);

	// 빈 FText 면 버튼을 접는다.
	void SetAction(const FText& Label, bool bEnabled = true);

	// 잠긴 다음 라인 = 낮은 줄 + 자물쇠 + 흐린 이름, 버튼 칸 접기. 풀면 SetRow/SetAction 이 고른 상태로 돌아간다.
	void SetLocked(bool bInLocked);

	// 빈 라인의 그림 칸(점선 + 「+」).
	void SetEmptyPlate(bool bShow);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NumberText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> DetailText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ValueText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> ActionButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ActionSize;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> RowHeight;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LockIcon;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EmptyPlate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DrawerInfoRow|Layout")
	float NormalRowMinHeight = 198.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DrawerInfoRow|Layout")
	float LockedRowMinHeight = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DrawerInfoRow|Ink")
	FSlateColor NameInk = FSlateColor(FLinearColor(0.888f, 0.905f, 0.930f));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DrawerInfoRow|Ink")
	FSlateColor NameDimInk = FSlateColor(FLinearColor(0.266f, 0.319f, 0.423f));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DrawerInfoRow|Ink")
	FSlateColor ToneNormalInk = FSlateColor(FLinearColor(0.539f, 0.610f, 0.708f));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DrawerInfoRow|Ink")
	FSlateColor ToneGoodInk = FSlateColor(FLinearColor(0.238f, 0.693f, 0.178f));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DrawerInfoRow|Ink")
	FSlateColor TonePendingInk = FSlateColor(FLinearColor(1.0f, 0.693f, 0.147f));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DrawerInfoRow|Ink")
	FSlateColor ToneMutedInk = FSlateColor(FLinearColor(0.266f, 0.319f, 0.423f));

private:
	void HandleActionClicked();
	const FSlateColor& InkForTone(EDrawerRowTone Tone) const;
	void ApplyNameInk();
	void ApplyActionVisibility();

	FDrawerRowActionEvent ActionClicked;
	bool bWarnedMissingBindings = false;
	bool bLocked = false;
	bool bNameDimmed = false;
	bool bHasAction = false;
};
