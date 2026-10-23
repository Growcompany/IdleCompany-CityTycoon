// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "CommonButtonBase.h"
#include "Table/LiveOpsTables.h"
#include "EventBannerWidget.generated.h"

class UAlertMarkWidget;
class UCommonTextBlock;
class UImage;
class UWidget;

/**
 * ListView 아이템 (평구조 UObject — 우편 행 선례).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEventBannerItem : public UObject
{
	GENERATED_BODY()

public:
	FLiveOpsEventDef Def;

	// 수령 가능한 마일스톤이 하나라도 있는지 — 배너마다 원장을 훑지 않도록 창이 계산해 넣는다
	bool bClaimable = false;

	// D-N 표기의 기준 시각 — 배너마다 시계를 묻지 않도록 창이 채운다
	int64 NowUnix = 0;

	// 권위 시각이 없으면 남은 기간을 말할 수 없다 (로컬 시계로 D-N 을 지어내지 않는다)
	bool bServerTimeValid = true;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnEventBannerClicked, UEventBannerItem*);

/**
 * 이벤트 배너 (WBP: UIE_EventBanner) — 유형 칩 · D-N · 제목. 탭하면 우측 상세로 이어진다.
 * 선택 링은 소유 리스트의 선택 상태를 따른다(버튼 자체의 선택 상태를 쓰지 않는다).
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UEventBannerWidget : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	FOnEventBannerClicked OnBannerClicked;

	void SetEvent(const FLiveOpsEventDef& InDef, bool bInClaimable, int64 InNowUnix, bool bInServerTimeValid);

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnClicked() override;

	// 배너 색은 DT 가 주는 런타임 데이터라 C++ 틴트가 정당하다 (정적 스타일 금지의 데이터 예외)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> BannerPlate = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TypeText = nullptr;

	// "D-6" / "상시" / "오늘 종료"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> DDayText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TitleText = nullptr;

	// 가시성 토글 전용이라 Optional (수정안 G2)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UAlertMarkWidget> AlertMark = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectRing = nullptr;

	// 유형 표시명은 WBP CDO 가 소유한다 — enum DisplayName 은 패키징 빌드에서 영어로 떨어진다
	UPROPERTY(EditAnywhere, Category = "LiveOps|Text")
	FText TypeNameAttendance;

	UPROPERTY(EditAnywhere, Category = "LiveOps|Text")
	FText TypeNameCollect;

	UPROPERTY(EditAnywhere, Category = "LiveOps|Text")
	FText TypeNameHotTime;

private:
	FText TypeDisplayName(ELiveOpsEventType Type) const;
	void ApplySelectRing(bool bRingVisible);

	UPROPERTY()
	TObjectPtr<UEventBannerItem> Item = nullptr;
};
