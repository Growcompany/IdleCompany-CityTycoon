#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GachaPanelFrameWidget.generated.h"

class UCloseButtonWidget;
class UPanelHeaderWidget;
class UTexture2D;

// 전체화면 배경과 Body는 가챠가 소유하고, 제목·도움말·닫기는 공통 헤더에 전달한다.
UCLASS(Abstract, meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UGachaPanelFrameWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	virtual void SynchronizeProperties() override;
	UFUNCTION(BlueprintCallable, Category = "Frame")
	void SetTitle(const FText& InTitle);
	// 닫기는 패널이 처리한다(스택에서 내리는 건 패널의 일) ― 틀은 버튼만 넘긴다
	UCloseButtonWidget* GetCloseButton() const;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(EditAnywhere, Category = "Frame")
	FText Title;

	UPROPERTY(EditAnywhere, Category = "Frame")
	TObjectPtr<UTexture2D> HeaderIconTexture;

	// DT_Help 행 이름. 비우면 "?" 가 스스로 숨는다
	UPROPERTY(EditAnywhere, Category = "Frame")
	FName HelpKey;

	UPROPERTY(EditAnywhere, Category = "Frame|Appearance")
	FName ThemeKey;

	UPROPERTY(EditAnywhere, Category = "Frame|Appearance")
	FName HeaderPatternKey;

	UPROPERTY(EditAnywhere, Category = "Frame|Appearance")
	FName SideGradientKey;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelHeaderWidget> PanelHeader;

private:
	void ApplyHeader();
	// 풀링된 패널은 열 때마다 Construct 된다 ― 경고는 인스턴스당 한 번만 남긴다
	bool bWarnedEmptyTitle = false;
};
