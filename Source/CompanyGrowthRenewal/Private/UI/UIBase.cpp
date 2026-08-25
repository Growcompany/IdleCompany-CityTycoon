// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/UIBase.h"
#include "Widgets/CommonactivatableWidgetContainer.h"
#include "Manager/SoundManagerSubsystem.h"
#include "UI/UISoundTags.h"
#include "Engine/GameInstance.h"

void UUIBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (PromptStack)
	{
		PromptStack->OnDisplayedWidgetChanged().RemoveAll(this);
		PromptStack->OnDisplayedWidgetChanged().AddUObject(this, &UUIBase::HandlePromptStackDisplayChanged);
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
	LastPromptCount = Count;
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
	UCommonActivatableWidget* Widget = PromptStack->AddWidget(widgetClass);
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
