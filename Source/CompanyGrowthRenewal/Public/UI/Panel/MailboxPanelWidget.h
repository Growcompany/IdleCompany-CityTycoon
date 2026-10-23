// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Panel/LiveOpsModalWidget.h"
#include "MailboxPanelWidget.generated.h"

class UButtonWidget;
class UCommonTextBlock;
class UListView;
class UMailboxSubsystem;
class UMailRowItem;
class UWidget;

/**
 * 우편함 창 — 미수령 개수 + 우편 행 리스트(행 탭 = 본문 아코디언) + 푸터 [모두 받기].
 * 탭이 없고, 수령 판정·보상 지급·만료 정리는 전부 UMailboxSubsystem 이 한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMailboxPanelWidget : public ULiveOpsModalWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void RefreshAll() override;

	// "받지 않은 우편" 라벨 — 값은 옆 숫자가 들고 있다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> MailCountText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> MailCountNumText = nullptr;

	// "· 받은 우편은 바로 사라집니다" — 문구는 WBP 가 소유(레이아웃/검증 계약으로 바인딩)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> MailCountNote = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UListView> MailList = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> ClaimAllButton = nullptr;

	// 빈 상태 글리프+문장 — 가시성 토글 전용이라 Optional (수정안 G2)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EmptyState = nullptr;

private:
	void HandleMailboxChanged();
	void HandleClaimAll();
	void HandleRowClaim(UMailRowItem* RowItem);
	void HandleEntryGenerated(UUserWidget& Entry);

	UMailboxSubsystem* GetMailbox() const;

	UPROPERTY()
	TArray<TObjectPtr<UMailRowItem>> Items;

	// 펼쳐 둔 우편 — 아이템 풀은 갱신마다 다른 우편으로 재사용되므로 상태를 인덱스가 아니라 Id 로 들고 있는다
	TSet<FGuid> ExpandedIds;
};
