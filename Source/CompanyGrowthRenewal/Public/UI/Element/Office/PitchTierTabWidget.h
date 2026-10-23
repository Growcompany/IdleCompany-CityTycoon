// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "PitchTierTabWidget.generated.h"

class UBorder;
class UCommonTextBlock;
class UImage;
class UMaterialInstanceDynamic;
class UProgressBar;
class UTexture2D;

// 기획 보드 좌측 세로 레일 탭 1칸에 보드가 밀어 넣는 주입 묶음.
// 한 티어는 done(지나옴) / current(진행중) / locked(미도달) 중 정확히 하나다.
USTRUCT()
struct FPitchTierTabData
{
	GENERATED_BODY()

	int32 Tier = 1;
	bool bLocked = false;    // 미도달 티어 (Tier > 현재 티어)
	bool bDone = false;      // 지나온 티어 (Tier < 현재 티어) — 완료 체크
	bool bCurrent = false;   // 진행 중 티어 (Tier == 현재 티어) — 진행 카운트 노출
	FText ProgressText;      // 현재 티어일 때 "4/10"
	float ProgressFraction = 0.f;
	TSoftObjectPtr<UTexture2D> Cover;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPitchTierClicked, int32, Tier);

/**
 * 기획 보드 좌측 세로 티어 레일의 탭 1칸 (UIE_PitchTierTab).
 * 색과 표면은 WBP가 소유하고, 선택 표시는 CommonUI의 GetSelected()를 따른다.
 * 선택 = SelectBand, 완료 = 체크, 잠금 = LockIcon.
 * 전체가 버튼이라 클릭 = OnTierClicked. 타일(UPitchTileWidget) 패턴 미러 — 런타임 생성.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UPitchTierTabWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	void Configure(const FPitchTierTabData& InData);

	int32 GetTier() const { return Tier; }

	UPROPERTY(BlueprintAssignable, Category = "PitchBoard")
	FOnPitchTierClicked OnTierClicked;

protected:
	virtual void NativeOnClicked() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;

	// 이미지 배경의 표면과 테두리는 WBP가 소유한다. 루트 버튼은 CardClear(투명) 스타일을 쓴다.
	UPROPERTY(meta = (BindWidget))
	UBorder* Plate;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* Text_Tier;

	UPROPERTY(meta = (BindWidgetOptional))
	UImage* TierCoverImage;

	// 잠금 아이콘 — 브러시는 WBP 소유, C++는 Visibility만 토글
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* LockIcon;

	// 완료 체크 — 지나온 티어에만
	UPROPERTY(meta = (BindWidgetOptional))
	UImage* Check;

	// 진행 카운트 "4/10" — 현재 티어에만
	UPROPERTY(meta = (BindWidgetOptional))
	UCommonTextBlock* Text_Progress;

	UPROPERTY(meta = (BindWidgetOptional))
	UProgressBar* TierProgressBar;

	// 선택선의 색/모양은 WBP 소유, C++는 GetSelected()에 따라 Visibility를 적용한다.
	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* SelectBand;

	// 티어명 잉크 (크롬 잉크 #E6E9F0)
	UPROPERTY(EditAnywhere, Category = "Style|Color")
	FLinearColor TextColor = FLinearColor(0.791f, 0.815f, 0.871f, 1.f);

	// 제조 전용 변형(UIE_PitchTierTabCompact)만 켠다 ― 현재 단계의 진행 글자를 이름 띠 오른쪽에 보인다.
	UPROPERTY(EditAnywhere, Category = "Style")
	bool bShowProgressInLabel = false;

	// 잠긴 단계 표지의 불투명도. 변형 WBP 가 더 흐리게 덮어쓴다.
	UPROPERTY(EditAnywhere, Category = "Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LockedCoverOpacity = 0.72f;

	// 표지 크롭 기준 가로세로비. 0 이면 브러시 ImageSize 비율을 쓴다(변형 WBP 는 그림 칸 비율을 직접 준다).
	UPROPERTY(EditAnywhere, Category = "Style", meta = (ClampMin = "0.0"))
	float CoverSurfaceAspect = 0.f;

private:
	void ApplySelectionVisual();
	int32 Tier = 1;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* CoverMID = nullptr;
};
