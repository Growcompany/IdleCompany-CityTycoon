#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Manager/LaunchLootManagerSubsystem.h"
#include "LaunchLootOddsWidget.generated.h"

class UVerticalBox;
class UFont;
class UButton;
class UModalFrameWidget;
struct FButtonStyle;

/**
 * 출시 보상 확률표. 공용 ModalFrame 안에 데이터 행을 채우고 독립 뷰포트 창으로 표시한다.
 * 보드 레이아웃과 분리하며 닫기 버튼 또는 배경 클릭으로 돌아간다.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API ULaunchLootOddsWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void Show(const TArray<FLaunchLootPreviewEntry>& Entries, const FButtonStyle& DimButtonStyle, int32 ZOrder = 101);

	// 뭉치 지급 표기 "(2~3장)" — 상위 등급에서 슬롯 %가 낮아 보이는 오독 방지 (툴팁/확률표 공용)
	static FString GetBandAmountSuffix(const FLaunchLootPreviewEntry& Entry, int32 BandIndex);

	void Hide();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void EnsureTree();
	void BuildRows(const TArray<FLaunchLootPreviewEntry>& Entries);
	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<UButton> BackdropButton;

	UPROPERTY(Transient)
	TObjectPtr<UModalFrameWidget> ModalFrame;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RowsBox;

	UPROPERTY(Transient)
	TObjectPtr<UFont> BoldFont;

	UPROPERTY(Transient)
	TObjectPtr<UFont> RegularFont;
};
