// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/UIBase.h"
#include "Widgets/CommonactivatableWidgetContainer.h"
#include "Manager/SoundManagerSubsystem.h"
#include "Manager/MissionManagerSubsystem.h"
#include "Manager/UIManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "UI/Element/Buttons/CloseButtonWidget.h"
#include "Core/CGGameInstance.h"
#include "Engine/GameInstance.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "ICommonInputModule.h"
#include "CommonInputSettings.h"

namespace
{
	// X 가 없을 때 닫기로 치는 버튼 이름 ― 앞에 있을수록 먼저
	const TCHAR* const DismissButtonNames[] = { TEXT("CancelButton"), TEXT("BackButton"), TEXT("OutButton"), TEXT("ReceiveButton") };
	const TCHAR* const BackdropButtonName = TEXT("BackgroundBtn");

	bool IsPressable(const UWidget* Widget)
	{
		if (!Widget || !Widget->GetIsEnabled())
		{
			return false;
		}
		if (const UCloseButtonWidget* Close = Cast<UCloseButtonWidget>(Widget))
		{
			return Close->OnCloseClicked.IsBound();
		}
		if (const UButtonWidget* Common = Cast<UButtonWidget>(Widget))
		{
			return Common->IsInteractionEnabled();
		}
		const UButton* Plain = Cast<UButton>(Widget);
		return Plain && Plain->OnClicked.IsBound();
	}

	struct FDismissScan
	{
		UCloseButtonWidget* LastClose = nullptr;
		TMap<FName, UWidget*> Named;
	};

	// 보이는 가지만 내려간다 ― X 는 나중에 그려지는 것(창 안 작은 팝업)이 이긴다
	void ScanDismiss(UWidget* Widget, FDismissScan& Out, int32 Depth)
	{
		if (!Widget || Depth > 64 || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0.01f)
		{
			return;
		}
		if (UCloseButtonWidget* Close = Cast<UCloseButtonWidget>(Widget))
		{
			if (IsPressable(Close))
			{
				Out.LastClose = Close;
			}
			return;
		}
		if (!Out.Named.Contains(Widget->GetFName()) && (Widget->IsA<UButtonWidget>() || Widget->IsA<UButton>()) && IsPressable(Widget))
		{
			Out.Named.Add(Widget->GetFName(), Widget);
		}
		if (UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Widget))
		{
			ScanDismiss(Switcher->GetActiveWidget(), Out, Depth + 1);
			return;
		}
		if (UUserWidget* User = Cast<UUserWidget>(Widget))
		{
			ScanDismiss(User->GetRootWidget(), Out, Depth + 1);
			return;
		}
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				ScanDismiss(Panel->GetChildAt(Index), Out, Depth + 1);
			}
		}
	}

	// 슬롯 부모 → 트리를 가진 UserWidget 순으로 올라간다(UserWidget 경계를 넘는다)
	bool IsSameOrInside(const UWidget* Inner, const UWidget* Outer)
	{
		for (const UWidget* Cursor = Inner; Cursor && Outer;)
		{
			if (Cursor == Outer)
			{
				return true;
			}
			const UWidget* Parent = Cursor->GetParent();
			Cursor = Parent ? Parent : Cursor->GetTypedOuter<UUserWidget>();
		}
		return false;
	}
}

UWidget* CGBackNav::FindDismissControl(UWidget& Window, bool bAllowBackdrop)
{
	FDismissScan Scan;
	ScanDismiss(&Window, Scan, 0);
	if (Scan.LastClose)
	{
		return Scan.LastClose;
	}
	for (const TCHAR* Name : DismissButtonNames)
	{
		if (UWidget* const* Found = Scan.Named.Find(FName(Name)))
		{
			return *Found;
		}
	}
	UWidget* const* Backdrop = bAllowBackdrop ? Scan.Named.Find(FName(BackdropButtonName)) : nullptr;
	return Backdrop ? *Backdrop : nullptr;
}

UWidget* CGBackNav::FindTopWindow(UWorld* World)
{
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	const UUIManagerSubsystem* UIMgr = GI ? GI->GetSubsystem<UUIManagerSubsystem>() : nullptr;
	UUIBase* Base = UIMgr ? UIMgr->GetUIBase() : nullptr;

	// 스택 밖에서 화면에 바로 올린 팝업(장비 창·확률 표·연출 등)이 스택 창보다 위에 그려진다
	UUserWidget* Overlay = nullptr;
	int32 OverlayZ = 0;
	if (UGameViewportSubsystem* Viewport = UGameViewportSubsystem::Get(World))
	{
		TArray<UUserWidget*> TopLevel;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, TopLevel, UUserWidget::StaticClass(), true);
		for (UUserWidget* Candidate : TopLevel)
		{
			if (!Candidate || Candidate == Base || !Viewport->IsWidgetAdded(Candidate) || !FindDismissControl(*Candidate))
			{
				continue;
			}
			const int32 Z = Viewport->GetWidgetSlot(Candidate).ZOrder;
			if (!Overlay || Z >= OverlayZ)
			{
				Overlay = Candidate;
				OverlayZ = Z;
			}
		}
	}
	if (Overlay)
	{
		return Overlay;
	}
	if (!Base)
	{
		return nullptr;
	}
	if (UCommonActivatableWidget* Prompt = Base->GetActivePromptWidget())
	{
		return Prompt;
	}
	// 아래 스택 맨 밑은 도시·사무실의 기본 도크다 ― 그 위에 쌓인 시트만 창이다
	return Base->GetBottomStackCount() > 1 ? Base->GetActiveBottomWidget() : nullptr;
}

bool CGBackNav::IsTopWindow(const UWidget& Widget)
{
	const UWidget* Top = FindTopWindow(Widget.GetWorld());
	// 맨 위가 이 창이 띄운 팝업이면 이 창이 자기 순서대로 닫는다
	return !Top || IsSameOrInside(&Widget, Top) || IsSameOrInside(Top, &Widget);
}

bool CGBackNav::IsBlockedByGuide(const UWidget& Context, const UWidget* Press)
{
	const UWorld* World = Context.GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	const UMissionManagerSubsystem* Missions = GI ? GI->GetSubsystem<UMissionManagerSubsystem>() : nullptr;
	if (!Missions || !Missions->IsInputGated())
	{
		return false;
	}
	const UWidget* Target = Missions->GetCurrentHighlightTarget();
	return !(Press && Target && (IsSameOrInside(Target, Press) || IsSameOrInside(Press, Target)));
}

void CGBackNav::Press(UWidget& Control)
{
	if (UCloseButtonWidget* Close = Cast<UCloseButtonWidget>(&Control))
	{
		Close->OnCloseClicked.Broadcast();
	}
	else if (UButtonWidget* Common = Cast<UButtonWidget>(&Control))
	{
		Common->SimulateClick();
	}
	else if (UButton* Plain = Cast<UButton>(&Control))
	{
		Plain->OnClicked.Broadcast();
	}
}

TOptional<FUIInputConfig> UUIBase::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::CaptureDuringMouseDown, false);
}

bool UUIBase::NativeOnHandleBackAction()
{
	const UCGGameInstance* Instance = Cast<UCGGameInstance>(GetGameInstance());
	if (Instance && Instance->IsLevelTransitionActive())
	{
		return true;
	}
	UWidget* Control = nullptr;
	if (UWidget* Top = CGBackNav::FindTopWindow(GetWorld()))
	{
		Control = CGBackNav::FindDismissControl(*Top);
	}
	// 열린 창이 없으면 바닥 화면 자체의 뒤로(사무실 → 도시, 배치 편집 취소). 바깥 딤은 바닥 화면에 없다
	else if (UCommonActivatableWidget* Main = MainStack ? MainStack->GetActiveWidget() : nullptr)
	{
		Control = CGBackNav::FindDismissControl(*Main, false);
	}
	if (Control && !CGBackNav::IsBlockedByGuide(*this, Control))
	{
		UE_LOG(LogTemp, Log, TEXT("[UIBase] 뒤로가기 → %s (%s)"), *Control->GetName(), *GetNameSafe(Control->GetTypedOuter<UUserWidget>()));
		CGBackNav::Press(*Control);
	}
	// 닫을 것이 없어도 소비한다 ― 선택을 강요하는 창(이벤트 선택 등)에서 뒤로가기가 아래로 새지 않게
	return true;
}

void UUIBase::NativeConstruct()
{
	// 뒤로가기 바인딩은 부모 NativeConstruct 가 건다 ― 그 전에 켜고, 게임 밖(시험)에서도 입력 데이터가 읽혀 있게 한다
	bIsBackHandler = true;
	ICommonInputModule::GetSettings().LoadData();
	Super::NativeConstruct();

	if (PromptStack)
	{
		PromptStack->OnDisplayedWidgetChanged().RemoveAll(this);
		PromptStack->OnDisplayedWidgetChanged().AddUObject(this, &UUIBase::HandlePromptStackDisplayChanged);
		PromptStack->OnTransitioningChanged.RemoveAll(this);
		PromptStack->OnTransitioningChanged.AddUObject(this, &UUIBase::HandlePromptStackTransitionChanged);
		LastPromptCount = PromptStack->GetNumWidgets();
	}
	if (BottomStack)
	{
		BottomStack->OnDisplayedWidgetChanged().RemoveAll(this);
		BottomStack->OnDisplayedWidgetChanged().AddUObject(this, &UUIBase::HandleBottomStackDisplayChanged);
		LastBottomCount = BottomStack->GetNumWidgets();
	}
}

void UUIBase::NativeDestruct()
{
	if (PromptStack)
	{
		PromptStack->OnDisplayedWidgetChanged().RemoveAll(this);
		PromptStack->OnTransitioningChanged.RemoveAll(this);
	}
	if (BottomStack)
	{
		BottomStack->OnDisplayedWidgetChanged().RemoveAll(this);
	}
	Super::NativeDestruct();
}

void UUIBase::HandlePromptStackDisplayChanged(UCommonActivatableWidget* NewDisplayedWidget)
{
	const int32 Count = PromptStack ? PromptStack->GetNumWidgets() : 0;
	if (Count < LastPromptCount)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
			{
				SM->PlayUISound(CGUISoundTags::ModalClose);
			}
		}
	}
	if (Count != LastPromptCount)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UMissionManagerSubsystem* Missions = GI->GetSubsystem<UMissionManagerSubsystem>())
			{
				Missions->NotifyPromptStackChanged();
			}
		}
		OnPromptCountChanged.Broadcast(Count);
	}
	LastPromptCount = Count;
}

void UUIBase::HandlePromptStackTransitionChanged(UCommonActivatableWidgetContainerBase* Container, bool bIsTransitioning)
{
	if (bIsTransitioning || !PromptStack || Container != PromptStack) { return; }

	// CommonUI가 화면 전환을 끝내고 WidgetList를 정리한 뒤의 최종 상태로 HUD 숨김을 재판정한다.
	if (PromptStack->GetNumWidgets() != LastPromptCount)
	{
		HandlePromptStackDisplayChanged(PromptStack->GetActiveWidget());
		return;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMissionManagerSubsystem* Missions = GI->GetSubsystem<UMissionManagerSubsystem>())
		{
			Missions->NotifyPromptStackChanged();
		}
	}
}

void UUIBase::HandleBottomStackDisplayChanged(UCommonActivatableWidget* NewDisplayedWidget)
{
	const int32 Count = BottomStack ? BottomStack->GetNumWidgets() : 0;
	if (Count < LastBottomCount)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
			{
				SM->PlayUISound(CGUISoundTags::BottomSheetClose);
			}
		}
	}
	LastBottomCount = Count;
}

UCommonActivatableWidget* UUIBase::PushMenuClass(TSubclassOf<UCommonActivatableWidget> widgetClass)
{
	return MainStack->AddWidget(widgetClass);
}

UCommonActivatableWidget* UUIBase::PushPromptClass(TSubclassOf<UCommonActivatableWidget> widgetClass)
{
	return PushPromptClass(widgetClass, [](UCommonActivatableWidget&) {});
}

UCommonActivatableWidget* UUIBase::PushPromptClass(TSubclassOf<UCommonActivatableWidget> widgetClass, TFunctionRef<void(UCommonActivatableWidget&)> InitializeInstance)
{
	UCommonActivatableWidget* Widget = PromptStack ? PromptStack->AddWidget(widgetClass, InitializeInstance) : nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::ModalOpen);
		}
	}
	return Widget;
}

UCommonActivatableWidget* UUIBase::PushBottomClass(TSubclassOf<UCommonActivatableWidget> widgetClass)
{
	UCommonActivatableWidget* Widget = BottomStack->AddWidget(widgetClass);
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USoundManagerSubsystem* SM = GI->GetSubsystem<USoundManagerSubsystem>())
		{
			SM->PlayUISound(CGUISoundTags::BottomSheetOpen);
		}
	}
	return Widget;
}

bool UUIBase::RemovePromptWidget(UCommonActivatableWidget* Widget)
{
	if (!Widget || !PromptStack || !PromptStack->GetWidgetList().Contains(Widget)) return false;
	PromptStack->RemoveWidget(*Widget);
	return true;
}

int32 UUIBase::GetPromptStackCount() const
{
    if (!PromptStack)
        return 0;

    return PromptStack->GetNumWidgets();
}

UCommonActivatableWidget* UUIBase::GetActivePromptWidget() const
{
    return PromptStack ? PromptStack->GetActiveWidget() : nullptr;
}

int32 UUIBase::GetBottomStackCount() const
{
    if (!BottomStack)
        return 0;

    return BottomStack->GetNumWidgets();
}

UCommonActivatableWidget* UUIBase::GetActiveBottomWidget() const
{
    return BottomStack ? BottomStack->GetActiveWidget() : nullptr;
}

bool UUIBase::PopBottomWidget()
{
    if (!BottomStack || BottomStack->GetNumWidgets() <= 0)
        return false;

    // �� �� ���� ����
    UCommonActivatableWidget* TopWidget = BottomStack->GetActiveWidget();
    if (TopWidget)
    {
        // 닫힘 사운드는 BottomStack 표시 변경 hook 이 재생 (여기서 또 재생하면 더블플레이)
        BottomStack->RemoveWidget(*TopWidget);
        UE_LOG(LogTemp, Log, TEXT("[UIBase] PopBottomWidget - Widget removed"));
        return true;
    }

    return false;
}

int32 UUIBase::GetMainStackCount() const
{
    if (!MainStack)
        return 0;

    return MainStack->GetNumWidgets();
}

bool UUIBase::PopMainWidget()
{
    if (!MainStack || MainStack->GetNumWidgets() <= 0)
        return false;

    UCommonActivatableWidget* TopWidget = MainStack->GetActiveWidget();
    if (TopWidget)
    {
        MainStack->RemoveWidget(*TopWidget);
        UE_LOG(LogTemp, Log, TEXT("[UIBase] PopMainWidget - Widget removed"));
        return true;
    }

    return false;
}

void UUIBase::DebugPrintStackContents()
{
    UE_LOG(LogTemp, Warning, TEXT("=== UI Stack Debug ==="));

    // MainStack�� ��� ���� �̸� Ȯ��
    if (MainStack)
    {
        int32 MainCount = MainStack->GetNumWidgets();
        UE_LOG(LogTemp, Warning, TEXT("MainStack count: %d"), MainCount);

        UCommonActivatableWidget* ActiveMain = MainStack->GetActiveWidget();
        if (ActiveMain)
        {
            UE_LOG(LogTemp, Warning, TEXT("  MainStack Active: %s"), *ActiveMain->GetClass()->GetName());
        }

        // ��Ȱ�� �����鵵 Ȯ���ϴ� ��� �ʿ�
        // MainStack�� ��� ������ ��ȸ�� �� �ִ� �ٸ� ��� ã��
    }

    // BottomStack�� ��� ���� �̸� Ȯ��
    if (BottomStack)
    {
        int32 BottomCount = BottomStack->GetNumWidgets();
        UE_LOG(LogTemp, Warning, TEXT("BottomStack count: %d"), BottomCount);

        UCommonActivatableWidget* ActiveBottom = BottomStack->GetActiveWidget();
        if (ActiveBottom)
        {
            UE_LOG(LogTemp, Warning, TEXT("  BottomStack Active: %s"), *ActiveBottom->GetClass()->GetName());
        }

        // ���Ⱑ �ٽ�! �ٸ� �����鵵 Ȯ���ؾ� ��
    }

    // PromptStack�� Ȯ��
    if (PromptStack)
    {
        int32 PromptCount = PromptStack->GetNumWidgets();
        UE_LOG(LogTemp, Warning, TEXT("PromptStack count: %d"), PromptCount);

        UCommonActivatableWidget* ActivePrompt = PromptStack->GetActiveWidget();
        if (ActivePrompt)
        {
            UE_LOG(LogTemp, Warning, TEXT("  PromptStack Active: %s"), *ActivePrompt->GetClass()->GetName());
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("====================="));
}
