// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "UIBase.generated.h"

class UOverlay;
class UCommonActivatableWidget;
class UCommonActivatableWidgetContainerBase;
class UCommonActivatableWidgetStack;
class UWorld;

/**
 * 
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UUIBase : public UCommonActivatableWidget
{
	GENERATED_BODY()
	
private:
	bool OnlyUIInput = false;

	// 닫힘 사운드 단일 hook — 표시 위젯 변경 시 스택 카운트가 줄었으면 닫힘으로 판정.
	// X버튼/Back/Esc/프로그래매틱 어느 경로로 닫혀도 1회만 재생 (열림 사운드는 Push* 에서 재생)
	void HandlePromptStackDisplayChanged(UCommonActivatableWidget* NewDisplayedWidget);
	void HandlePromptStackTransitionChanged(UCommonActivatableWidgetContainerBase* Container, bool bIsTransitioning);
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
	// 어떤 창도 스스로 받지 않은 뒤로가기(ESC·안드로이드)가 여기로 올라온다 ― 가장 위 창을 그 창의 X 처럼 닫는다
	virtual bool NativeOnHandleBackAction() override;

public:
	/** 자식 패널이 닫히면 게임 입력과 CommonUI 액션을 함께 받는 루트 설정으로 복원한다. */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UCommonActivatableWidget* PushMenuClass(TSubclassOf<UCommonActivatableWidget> widgetClass);
	UCommonActivatableWidget* PushPromptClass(TSubclassOf<UCommonActivatableWidget> widgetClass);
	UCommonActivatableWidget* PushPromptClass(TSubclassOf<UCommonActivatableWidget> widgetClass, TFunctionRef<void(UCommonActivatableWidget&)> InitializeInstance);
	UCommonActivatableWidget* PushBottomClass(TSubclassOf<UCommonActivatableWidget> widgetClass);

	int32 GetPromptStackCount() const;

	// 가운데 창(프롬프트) 개수가 바뀔 때 ― 창 옆에 걸쳐 보이는 HUD 가 잠시 숨을 때 쓴다
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPromptCountChanged, int32 /*PromptCount*/);
	FOnPromptCountChanged OnPromptCountChanged;

	// 최상단(활성) 프롬프트 위젯 — 가이드 오버레이가 "타겟이 이 모달 안에 있는가"를 판정하는 데 사용
	UCommonActivatableWidget* GetActivePromptWidget() const;
	/** 전환 대기 중이거나 활성인 지정 프롬프트만 스택에서 제거한다. */
	bool RemovePromptWidget(UCommonActivatableWidget* Widget);

	int32 GetBottomStackCount() const;
	UCommonActivatableWidget* GetActiveBottomWidget() const;
	bool PopBottomWidget();
	int32 GetMainStackCount() const;
	bool PopMainWidget();

	void DebugPrintStackContents();
};

// 뒤로가기(ESC·안드로이드) 공용 규칙 ― 「가장 위에 열린 창의 X 를 누른 것」과 같게 닫는다
namespace CGBackNav
{
	// 가장 위에 열린 창: 화면에 바로 올린 팝업(닫을 수단이 보이는 것 중 Z 최상) → 가운데 창 → 아래 시트 창(기본 도크 제외). 없으면 nullptr
	COMPANYGROWTHRENEWAL_API UWidget* FindTopWindow(UWorld* World);

	// 자기 뒤로가기 처리기를 가진 창은 이게 true 일 때만 받는다 ― 동시에 열린 형제 창의 처리 순서가 정해져 있지 않아 아래 창이 먼저 닫힐 수 있다
	COMPANYGROWTHRENEWAL_API bool IsTopWindow(const UWidget& Widget);

	// 창 안에서 지금 누를 수 있는 닫기 수단: 공용 X(나중에 그려지는 것) → 취소·뒤로·나가기·받기 버튼 → 바깥 딤(bAllowBackdrop)
	COMPANYGROWTHRENEWAL_API UWidget* FindDismissControl(UWidget& Window, bool bAllowBackdrop = true);

	// 튜토리얼이 화면 입력을 막는 동안에는 짚는 대상이 Press 일 때만 허용 ― 구멍 밖 X 를 못 누르는 것과 같다
	COMPANYGROWTHRENEWAL_API bool IsBlockedByGuide(const UWidget& Context, const UWidget* Press);

	// 사람이 그 버튼을 누른 것과 같은 처리기를 부른다
	COMPANYGROWTHRENEWAL_API void Press(UWidget& Control);
}
