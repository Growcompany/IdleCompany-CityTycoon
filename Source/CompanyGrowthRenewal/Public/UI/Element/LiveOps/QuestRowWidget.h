// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "CommonUserWidget.h"
#include "Data/QuestRules.h"
#include "QuestRowWidget.generated.h"

class UAlertMarkWidget;
class UButtonWidget;
class UCommonButtonStyle;
class UCommonTextBlock;
class UHorizontalBox;
class UImage;
class UItemCardSlotWidget;
class UProgressBar;
class UWidget;

/**
 * ListView 아이템 (평구조 UObject — 장비 카드 선례).
 * 이벤트 마일스톤 행(Task 6)도 같은 아이템으로 표현한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UQuestRowItem : public UObject
{
	GENERATED_BODY()

public:
	FQuestEntry Entry;
	bool bMilestone = false;
	FString EventId;
	int32 MilestoneIndex = -1;

	// 서버 시각이 확인되기 전에는 모든 [받기]가 비활성이다 (스펙 §7.3) — 행이 보는 유일한 외부 게이트
	bool bServerTimeValid = true;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnQuestRowClaim, UQuestRowItem*);

/**
 * 퀘스트/마일스톤 행 (WBP: UIE_QuestRow) — 종류 아이콘 · 제목/단계 · 진행 막대 · 보상 카드 2장 · [받기]/[진행 중].
 * 행 자체는 눌리지 않는다(수령은 행 끝 버튼만). 정적 스타일은 WBP 소유, C++ 은 상태에 따른 값만 주입한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UQuestRowWidget : public UCommonUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	FOnQuestRowClaim OnClaimRequested;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TitleText = nullptr;

	// 업적 "N단계" — 일일/주간은 접는다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> TierText = nullptr;

	// 아이콘 칸 묶음 — DT 아이콘이 비면(소식 창 마일스톤 줄) 칸째 접는다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> IconWell = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> RowIcon = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ProgressCurrentText = nullptr;

	// " / 목표" — 현재값과 색을 따로 칠하려고 두 칸으로 나눈다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> ProgressTargetText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> ProgressBarBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UProgressBar> ProgressBar = nullptr;

	// 진행 막대 위 칸 나눔 눈금 — 목표가 작은 증분형일 때만 채운다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> SegmentBox = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UItemCardSlotWidget> RewardCard0 = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UItemCardSlotWidget> RewardCard1 = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UButtonWidget> ClaimButton = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UAlertMarkWidget> ClaimDot = nullptr;

	// 받은 줄의 체크 도장
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> DoneStamp = nullptr;

	UPROPERTY(EditAnywhere, Category = "Style|Button")
	TSubclassOf<UCommonButtonStyle> ClaimStyle_Claim;

	UPROPERTY(EditAnywhere, Category = "Style|Button")
	TSubclassOf<UCommonButtonStyle> ClaimStyle_Wait;

	// 버튼 라벨 잉크는 스타일이 아니라 ButtonWidget.TextColor 가 정한다 ― 밝은 대기 판 위에선 흰 글자가 사라진다
	UPROPERTY(EditAnywhere, Category = "Style|Button")
	FSlateColor ClaimLabelInk = FSlateColor(FLinearColor::White);

	UPROPERTY(EditAnywhere, Category = "Style|Button")
	FSlateColor WaitLabelInk = FSlateColor(FLinearColor::White);

	UPROPERTY(EditAnywhere, Category = "Style|Progress")
	FLinearColor BarFillProgress = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Style|Progress")
	FLinearColor BarFillFull = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Style|Progress")
	FSlateColor ProgressInkProgress;

	UPROPERTY(EditAnywhere, Category = "Style|Progress")
	FSlateColor ProgressInkFull;

	UPROPERTY(EditAnywhere, Category = "Style|Progress")
	FSlateBrush SegmentTickBrush;

	UPROPERTY(EditAnywhere, Category = "Style|Row", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ClaimedOpacity = 0.62f;

private:
	void ApplyIcon(const FQuestTable& Row);
	void ApplyProgress(const FQuestEntry& Entry);
	// 눈금 Count-1 개로 막대를 Count 칸으로 나눈다. Count 가 0 이면 비우고 접는다
	void ApplySegments(int32 Count);
	void ApplyRewards(const TArray<FMissionReward>& Rewards);
	void ApplyClaim(EQuestState State, bool bServerTimeValid);

	void HandleClaimClicked();

	UPROPERTY()
	TObjectPtr<UQuestRowItem> Item = nullptr;
};
