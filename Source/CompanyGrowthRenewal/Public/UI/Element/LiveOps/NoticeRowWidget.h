// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "CommonButtonBase.h"
#include "Table/LiveOpsTables.h"
#include "UI/Element/Common/ChipPlateSize.h"
#include "NoticeRowWidget.generated.h"

class UAlertMarkWidget;
class UCommonTextBlock;
class UImage;
class UWidget;

/**
 * ListView 아이템 (평구조 UObject — 우편 행 선례).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UNoticeItem : public UObject
{
	GENERATED_BODY()

public:
	FLiveOpsNoticeDef Def;

	bool bRead = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnNoticeRowClicked, UNoticeItem*);

/**
 * 공지 행 (WBP: UIE_NoticeRow) — 미읽음 도트 · 제목 · 등록일 · 고정 표시.
 * 선택 링은 소유 리스트의 선택 상태를 따른다(읽음 처리는 창이 한다).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UNoticeRowWidget : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	FOnNoticeRowClicked OnRowClicked;

	void SetNotice(const FLiveOpsNoticeDef& InDef, bool bInRead);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnClicked() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TitleText = nullptr;

	// "2026.09.13" (KST)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> DateText = nullptr;

	// "고정" — 문구는 WBP 가 소유하고 코드는 가시성만 정한다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> PinText = nullptr;

	// SDF 칩 2층 — 공유 MI 에 구워진 Wpx/Hpx 가 행 크기와 달라 그대로 두면 코너가 타원이 된다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> RowPlateBG = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> RowPlateLine = nullptr;

	// 가시성 토글 전용이라 Optional (수정안 G2)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UAlertMarkWidget> AlertMark = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectRing = nullptr;

private:
	void ApplySelectRing(bool bRingVisible);

	UPROPERTY()
	TObjectPtr<UNoticeItem> Item = nullptr;

	FChipPlateSizer PlateSizer;
};
