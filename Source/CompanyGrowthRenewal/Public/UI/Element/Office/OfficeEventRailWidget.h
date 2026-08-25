// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "OfficeEventRailWidget.generated.h"

class UVerticalBox;
class UNotificationElementWidget;

/**
 * Office 이벤트 레일 (UIE_OfficeEventRail). 개발 스트립(StripRoot) 바로 아래, 같은 세로 스택(StripRailColumn)에 레이아웃 배치.
 * OfficeMain(UOfficeMainWidget) 이 BindWidget(EventRail) 로 소유 — 좌표추적/OfficeLayer 절대앵커 호스팅은 2026-07-24 폐기.
 * 상태 토스트(자동 만료)와 이벤트 카드(자기 생명주기)가 한 피드에 쌓인다 — 새 엔트리가 맨 아래, 기존 것은 위로 밀림.
 *
 * 두 가지 비협상 제약:
 *  1) 자체 크롬 없음 — 배경/테두리 0. 엔트리가 0개면 화면에 아무것도 남지 않는다(사무실을 가리지 않는다는 목적).
 *  2) 트리 전체 SelfHitTestInvisible — 빈 영역/카드 사이 간격이 사무실 입력(빌딩·직원 클릭)을 막지 않는다. 입력은 카드 내부 버튼만.
 *
 * 전역 알림 컨테이너(UNotificationContainerWidget)와는 별개 시스템 — 엘리먼트(UNotificationElementWidget)만 재사용하고
 * 스택/최대개수/만료는 이 레일이 따로 소유한다(전역 경로 48곳 무영향).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeEventRailWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// 상태 토스트 — 전역 알림과 같은 엘리먼트(슬라이드 인/아웃 + 자동 만료). 만료 시 스택에서 자동 제거.
	// Key(>=0, 보통 EmployeeID)를 주면 같은 키의 기존 토스트를 교체하고 DismissStatusToast 로 조기 해제할 수 있다.
	// -1 = 키 없는 일회성(기존 동작). ⚠ UFUNCTION 기본인자에 INDEX_NONE 매크로 불가 — 리터럴 -1.
	UFUNCTION(BlueprintCallable, Category = "Office|EventRail")
	void AddStatusToast(const FText& Message, float Duration = 3.0f, FLinearColor Color = FLinearColor(1.0f, 0.867f, 0.478f, 1.0f), int32 Key = -1);

	// 키 토스트 조기 해제 — 해당 키가 없으면 무해한 no-op
	UFUNCTION(BlueprintCallable, Category = "Office|EventRail")
	void DismissStatusToast(int32 Key);

	// 이벤트 카드 — 자동 만료 없음. 해결/취소 시 카드가 RemoveEventCard 를 요청한다.
	UFUNCTION(BlueprintCallable, Category = "Office|EventRail")
	void AddEventCard(UUserWidget* Card);

	UFUNCTION(BlueprintCallable, Category = "Office|EventRail")
	void RemoveEventCard(UUserWidget* Card);

	// 자동 만료 카드(반응 토스트 등) — 레일에 붙이되 트림 우선순위는 토스트급(결정 대기 카드보다 먼저 밀려남).
	// 카드는 만료 시 스스로 RemoveEventCard 를 요청해야 한다(OnRailRemoveRequested 관용구).
	UFUNCTION(BlueprintCallable, Category = "Office|EventRail")
	void AddTransientCard(UUserWidget* Card);

	// 레일 전체 비우기 (오피스 이탈/레이어 파괴)
	UFUNCTION(BlueprintCallable, Category = "Office|EventRail")
	void ClearRail();

	int32 GetEntryCount() const { return Entries.Num(); }

protected:
	// 엔트리 스택 — AddChild 가 맨 아래(마지막 자식)에 붙으므로 새 엔트리가 아래에서 올라온다.
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* RailBox;

	// 동시에 표시할 엔트리 상한 (초과 시 가장 오래된 것 제거)
	UPROPERTY(EditAnywhere, Category = "Office|EventRail")
	int32 MaxEntries = 5;

	// 엔트리 사이 세로 간격
	UPROPERTY(EditAnywhere, Category = "Office|EventRail")
	float EntrySpacing = 12.0f;

	virtual void NativeDestruct() override;

private:
	// 표시 순서와 동일 (앞 = 위 = 가장 오래된 것)
	UPROPERTY()
	TArray<UUserWidget*> Entries;

	// Entries 의 보조 인덱스 — 키로 조기 해제하기 위한 것. 정리는 DetachEntry 한 곳에서만(모든 제거 경로가 거기를 지난다).
	UPROPERTY()
	TMap<int32, UNotificationElementWidget*> KeyedToasts;

	// AddTransientCard 로 들어온 엔트리 — TrimToMax 가 NotificationElement 와 같은 급으로 본다. 정리는 DetachEntry 한 곳.
	UPROPERTY()
	TSet<TObjectPtr<UUserWidget>> TransientEntries;

	void AppendEntry(UUserWidget* Entry);
	void DetachEntry(UUserWidget* Entry);
	void TrimToMax();

	void HandleToastFinished(UNotificationElementWidget* FinishedWidget);
};
