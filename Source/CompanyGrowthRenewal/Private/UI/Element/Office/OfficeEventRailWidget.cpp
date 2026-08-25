// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Office/OfficeEventRailWidget.h"
#include "UI/Element/Notifications/NotificationElementWidget.h"
#include "Manager/TableManagerSubsystem.h"
#include "Enum/WidgetType.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UOfficeEventRailWidget::AddStatusToast(const FText& Message, float Duration, FLinearColor Color, int32 Key)
{
	if (!RailBox)
	{
		return;
	}

	// 같은 직원의 후속 상태(졸음 → 놓침 → 폭주)는 쌓지 않고 갈아끼운다 — 한 직원이 레일을 다 먹으면 안 된다
	if (Key >= 0)
	{
		DismissStatusToast(Key);
	}

	UGameInstance* GI = GetGameInstance();
	UTableManagerSubsystem* TableMgr = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;
	if (!TableMgr)
	{
		return;
	}

	// 레일 전용 컴팩트 토스트(UIE_OfficeStatusToast) — 전역 배너(UIE_Notification)와 분리, 폭/텍스트를 레일에 맞춤.
	// C++ 로직(UNotificationElementWidget)은 공유하고 WBP 만 컴팩트 다크 칩으로 리스타일한 것이라 캐스팅 타입은 동일하다.
	TSubclassOf<UUserWidget> ElementClass = TableMgr->GetWidgetClass(EWidgetType::OfficeStatusToast);
	if (!ElementClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OfficeEventRail] OfficeStatusToast 미등록(DT_WidgetClass) — 토스트 생략"));
		return;
	}

	UNotificationElementWidget* Toast = CreateWidget<UNotificationElementWidget>(this, ElementClass);
	if (!Toast)
	{
		return;
	}

	Toast->OnNotificationFinished.BindUObject(this, &UOfficeEventRailWidget::HandleToastFinished);

	// 우측 레일 토스트는 우측 가장자리에서 슬라이드-인(전역 배너의 상단 드롭 대신) — ShowMessage 애니 시작 전에 설정.
	Toast->SetSlideFromRight(true);

	// 폭은 레일(RailWidthBox=480)이 소유하고 칩은 fill — 긴 문구가 칩 밖으로 새어나가지 않도록 안전하게 클리핑
	Toast->SetClipping(EWidgetClipping::ClipToBounds);

	if (Key >= 0)
	{
		KeyedToasts.Add(Key, Toast);
	}

	AppendEntry(Toast);

	Toast->ShowMessage(Message, Duration, Color);
}

void UOfficeEventRailWidget::DismissStatusToast(int32 Key)
{
	if (UNotificationElementWidget** Found = KeyedToasts.Find(Key))
	{
		if (UNotificationElementWidget* Toast = *Found)
		{
			// DetachEntry 가 맵에서도 빼므로 여기서 별도 Remove 불필요 (경로 하나로 유지)
			DetachEntry(Toast);
			return;
		}
		KeyedToasts.Remove(Key);
	}
}

void UOfficeEventRailWidget::AddEventCard(UUserWidget* Card)
{
	if (!RailBox || !Card || Entries.Contains(Card))
	{
		return;
	}

	AppendEntry(Card);
}

void UOfficeEventRailWidget::AddTransientCard(UUserWidget* Card)
{
	if (!RailBox || !Card || Entries.Contains(Card))
	{
		return;
	}

	TransientEntries.Add(Card);
	AppendEntry(Card);
}

void UOfficeEventRailWidget::RemoveEventCard(UUserWidget* Card)
{
	if (!Card)
	{
		return;
	}

	// 이미 레일에서 빠진 카드도(트림/재진입) 안전하게 정리되도록 idempotent
	if (!Entries.Contains(Card))
	{
		Card->RemoveFromParent();
		return;
	}

	DetachEntry(Card);
}

void UOfficeEventRailWidget::ClearRail()
{
	TArray<UUserWidget*> Snapshot = Entries;
	for (UUserWidget* Entry : Snapshot)
	{
		DetachEntry(Entry);
	}
	Entries.Empty();
	KeyedToasts.Empty();
}

void UOfficeEventRailWidget::NativeDestruct()
{
	// 엘리먼트가 만료 콜백으로 파괴 중인 레일을 되부르지 않도록 해제 (AddStatusToast 의 BindUObject 쌍)
	for (UUserWidget* Entry : Entries)
	{
		if (UNotificationElementWidget* Toast = Cast<UNotificationElementWidget>(Entry))
		{
			Toast->OnNotificationFinished.Unbind();
		}
	}
	Entries.Empty();

	Super::NativeDestruct();
}

void UOfficeEventRailWidget::AppendEntry(UUserWidget* Entry)
{
	if (!RailBox || !Entry)
	{
		return;
	}

	const bool bFirstEntry = RailBox->GetChildrenCount() == 0;

	if (UVerticalBoxSlot* BoxSlot = RailBox->AddChildToVerticalBox(Entry))
	{
		BoxSlot->SetHorizontalAlignment(HAlign_Fill);
		BoxSlot->SetPadding(FMargin(0.0f, bFirstEntry ? 0.0f : EntrySpacing, 0.0f, 0.0f));
	}

	Entries.Add(Entry);

	TrimToMax();
}

void UOfficeEventRailWidget::DetachEntry(UUserWidget* Entry)
{
	if (!Entry)
	{
		return;
	}

	if (UNotificationElementWidget* Toast = Cast<UNotificationElementWidget>(Entry))
	{
		Toast->OnNotificationFinished.Unbind();

		// 키 인덱스도 여기서만 정리 — 자동만료/트림/명시해제/ClearRail 이 전부 이 함수를 지나므로 스테일이 안 남는다.
		// FindKey 반환 포인터는 맵 내부를 가리키므로 값을 복사한 뒤 지운다.
		if (const int32* FoundKey = KeyedToasts.FindKey(Toast))
		{
			const int32 KeyCopy = *FoundKey;
			KeyedToasts.Remove(KeyCopy);
		}
	}

	// 목록에서 먼저 뺀다 — RemoveFromParent 가 카드의 NativeDestruct(미해결 정산 → 다시 RemoveEventCard)를
	// 되부를 수 있어, 순서를 바꾸면 같은 엔트리를 두 번 타고 들어간다.
	TransientEntries.Remove(Entry);
	Entries.Remove(Entry);
	Entry->RemoveFromParent();

	// 첫 엔트리 상단 간격 재정렬 — 위쪽이 빠지면 남은 맨 위가 간격을 물고 있으면 안 된다
	if (RailBox && RailBox->GetChildrenCount() > 0)
	{
		if (UWidget* TopChild = RailBox->GetChildAt(0))
		{
			if (UVerticalBoxSlot* TopSlot = Cast<UVerticalBoxSlot>(TopChild->Slot))
			{
				TopSlot->SetPadding(FMargin(0.0f));
			}
		}
	}
}

void UOfficeEventRailWidget::TrimToMax()
{
	while (Entries.Num() > FMath::Max(1, MaxEntries))
	{
		// 오래된 순으로 토스트부터 밀어낸다 — 이벤트 카드는 결정 대기 상태라 마지막 희생자.
		int32 VictimIndex = INDEX_NONE;
		for (int32 i = 0; i < Entries.Num(); ++i)
		{
			if (Entries[i] && (Entries[i]->IsA<UNotificationElementWidget>() || TransientEntries.Contains(Entries[i])))
			{
				VictimIndex = i;
				break;
			}
		}
		if (VictimIndex == INDEX_NONE)
		{
			VictimIndex = 0;
		}

		UUserWidget* Victim = Entries[VictimIndex];
		if (!Victim)
		{
			Entries.RemoveAt(VictimIndex);
			continue;
		}
		DetachEntry(Victim);
	}
}

void UOfficeEventRailWidget::HandleToastFinished(UNotificationElementWidget* FinishedWidget)
{
	DetachEntry(FinishedWidget);
}
