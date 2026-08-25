// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "UIBase.generated.h"

class UOverlay;
class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;

/**
 * 
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UUIBase : public UCommonActivatableWidget
{
	GENERATED_BODY()
	
private:
	bool OnlyUIInput = false;

	// 닫힘 사운드 단일 hook — 표시 위젯 변경 시 스택 카운트가 줄었으면 닫힘으로 판정.
	// X버튼/Back/Esc/프로그래매틱 어느 경로로 닫혀도 1회만 재생 (열림 사운드는 Push* 에서 재생)
	void HandlePromptStackDisplayChanged(UCommonActivatableWidget* NewDisplayedWidget);
	void HandleBottomStackDisplayChanged(UCommonActivatableWidget* NewDisplayedWidget);

	int32 LastPromptCount = 0;
	int32 LastBottomCount = 0;

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonActivatableWidgetStack* MainStack;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonActivatableWidgetStack* PromptStack;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonActivatableWidgetStack* BottomStack;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	UCommonActivatableWidget* PushMenuClass(TSubclassOf<UCommonActivatableWidget> widgetClass);
	UCommonActivatableWidget* PushPromptClass(TSubclassOf<UCommonActivatableWidget> widgetClass);
	UCommonActivatableWidget* PushBottomClass(TSubclassOf<UCommonActivatableWidget> widgetClass);

	int32 GetPromptStackCount() const;

	// 최상단(활성) 프롬프트 위젯 — 가이드 오버레이가 "타겟이 이 모달 안에 있는가"를 판정하는 데 사용
	UCommonActivatableWidget* GetActivePromptWidget() const;

	int32 GetBottomStackCount() const;
	UCommonActivatableWidget* GetActiveBottomWidget() const;
	bool PopBottomWidget();
	int32 GetMainStackCount() const;
	bool PopMainWidget();

	void DebugPrintStackContents();
};
