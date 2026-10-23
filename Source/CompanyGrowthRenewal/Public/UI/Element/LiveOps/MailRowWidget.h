// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "CommonButtonBase.h"
#include "Data/MailboxSaveData.h"
#include "UI/Element/Common/ChipPlateSize.h"
#include "MailRowWidget.generated.h"

class UButtonWidget;
class UCommonButtonStyle;
class UCommonTextBlock;
class UHorizontalBox;
class UImage;
class UItemCardSlotWidget;

/**
 * ListView 아이템 (평구조 UObject — 퀘스트 행 선례).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMailRowItem : public UObject
{
	GENERATED_BODY()

public:
	FMailEntry Entry;

	// 만료 표기(D-N)의 기준 시각 — 행마다 시계를 묻지 않도록 창이 한 번 채워 넣는다
	int64 NowUnix = 0;

	// 서버 시각이 확인되기 전에는 모든 [받기]가 비활성이다 (스펙 §7.3)
	bool bServerTimeValid = true;

	// 아코디언 상태는 아이템이 소유한다 — 엔트리 위젯은 풀에서 재사용돼 스크롤하면 상태가 날아간다
	bool bExpanded = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMailRowClaim, UMailRowItem*);

/**
 * 우편 행 (WBP: UIE_MailRow) — 첫 보상 카드 · 제목/발신·만료 · 보상 칩 · [받기] + 아코디언 본문.
 * 행 자체가 버튼이라 탭하면 본문이 펼쳐지고, 수령은 행 끝 버튼이 따로 받는다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMailRowWidget : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	FOnMailRowClaim OnClaimRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnClicked() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TitleText = nullptr;

	// "시스템 · D-6"
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> SubText = nullptr;

	// 아코디언 본문 — 접힘이 기본이고 펼치면 행이 160 보다 높아진다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> BodyText = nullptr;

	// 첫 표시 항목만 얹는 카드 — 나머지는 RewardBox 칩이 맡는다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UItemCardSlotWidget> FirstRewardCard = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> RewardBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> ClaimButton = nullptr;

	// SDF 칩 2층 — 공유 MI 에 구워진 Wpx/Hpx 가 행 크기와 달라 그대로 두면 코너가 타원이 된다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> RowPlateBG = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> RowPlateLine = nullptr;

	// 수령 가능/불가 두 벌 — 스타일 에셋은 WBP CDO 가 소유하고 코드는 둘 중 하나를 고른다
	UPROPERTY(EditAnywhere, Category = "Style|Button")
	TSubclassOf<UCommonButtonStyle> ClaimStyle_Gold;

	UPROPERTY(EditAnywhere, Category = "Style|Button")
	TSubclassOf<UCommonButtonStyle> ClaimStyle_Gray;

private:
	void HandleClaimClicked();
	void ApplyBodyVisibility();

	UPROPERTY()
	TObjectPtr<UMailRowItem> Item = nullptr;

	FChipPlateSizer PlateSizer;
};
