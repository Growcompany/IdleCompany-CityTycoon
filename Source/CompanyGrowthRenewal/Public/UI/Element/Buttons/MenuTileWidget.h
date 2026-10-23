// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Buttons/IconWithButtonWidget.h"
#include "MenuTileWidget.generated.h"

class UCommonTextBlock;

/**
 * 잠금 · 레드닷을 갖는 타일 버튼 베이스 ― 메뉴 시트 타일(UMenuSheetTileWidget)과 사무실 기능 버튼(UOfficeFeatureButtonWidget)의 부모
 * - 잠금 레이어(LockLayer · LockReasonText) + 레드닷(부모 AlertMark)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMenuTileWidget : public UIconWithButtonWidget
{
	GENERATED_BODY()

public:
	void SetLocked(bool bInLocked, const FText& Reason);
	bool IsLocked() const { return bLocked; }

	// 메뉴 시트에 저작된 라벨·아이콘 — 본사 레벨업 보상 카드가 같은 타일을 그대로 보여준다
	const FText& GetTileLabel() const { return Text; }
	UTexture2D* GetTileIcon() const { return IconTexture; }

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> LockLayer = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> LockReasonText = nullptr;

private:
	bool bLocked = false;
};
