#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Element/Common/HelpTableProvider.h"
#include "HelpTableRowWidget.generated.h"

class UCommonTextBlock;

/**
 * 도움말 표의 한 행 ― 항목 · 수치 · 비고 3열 고정.
 * 열이 더 필요하면 늘리지 말고 축으로 나눈다 (설계 R2 §6.2).
 * 스타일은 전부 WBP 소유. 여기서는 텍스트만 주입한다.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UHelpTableRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRow(const FHelpTableRow& InRow);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> LabelText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ValueText = nullptr;

	// 비고는 비어 있을 수 있다 ― 그때는 칸째 접는다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> NoteText = nullptr;
};
