// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Enum/CountryType.h"
#include "MapActionBubbleWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UTexture2D;
class UCommonButtonStyle;

UENUM()
enum class EMapActionBubbleKind : uint8
{
	Claim,
	Sell,
};

DECLARE_DELEGATE_TwoParams(FOnMapActionBubbleClicked, EMapActionBubbleKind /*Kind*/, ECountryType /*Country*/);

namespace MapBubbles
{
	// 시안 `.mbs{top:-oy-50}` + 말풍선 높이 56 → 줄 바닥이 미니어처 윗변에서 6px 아래에 걸친다.
	constexpr float kRowOverlapPx = 6.0f;

	/** 앵커 위로 솟은 그림 높이. 미니어처가 있으면 땅 조각 중심까지의 표시 높이, 없으면 원반 반지름. */
	COMPANYGROWTHRENEWAL_API float HeightAboveAnchor(bool bHasMiniature, float MiniaturePivotY, float DiscHeight);

	/** 앵커 기준 말풍선 줄 위치(정렬 (0.5, 1) = 줄 아래 가운데가 이 점). */
	COMPANYGROWTHRENEWAL_API FVector2D RowOffset(float InHeightAboveAnchor);

	COMPANYGROWTHRENEWAL_API FText ClaimLabel(int32 Pending);
	COMPANYGROWTHRENEWAL_API FText SellLabel(int64 Money);
	COMPANYGROWTHRENEWAL_API FText ClaimAllLabel(int32 Pending);
	COMPANYGROWTHRENEWAL_API FText ClaimAllButtonLabel();
	COMPANYGROWTHRENEWAL_API FText ClaimAllCountLabel(int32 Pending);
	// 서랍이 열려 있으면 지도 아래 모두 받기는 서랍 뒤에 묻히므로 접는다.
	COMPANYGROWTHRENEWAL_API bool ShouldShowClaimAll(int32 AllPending, bool bDrawerOpen);
}

/**
 * 지도 미니어처 위 할 일 말풍선 버튼. 금빛 = 그 나라 받기, 초록 = 가장 비싸게 사는 무역항에서 전부 팔기.
 * 면·림·눌림은 WBP 가 고른 공용 CTA 스타일(ClaimStyle/SellStyle)이 그리고, 코드는 종류·글자만 바꾼다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMapActionBubbleWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	void SetBubble(EMapActionBubbleKind InKind, ECountryType InCountry, const FText& Label);

	EMapActionBubbleKind GetKind() const { return Kind; }
	ECountryType GetCountry() const { return Country; }
	FText GetLabel() const;

	FOnMapActionBubbleClicked OnBubbleClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnClicked() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> LabelText;

	UPROPERTY(EditAnywhere, Category = "Bubble")
	TSubclassOf<UCommonButtonStyle> ClaimStyle;

	UPROPERTY(EditAnywhere, Category = "Bubble")
	TSubclassOf<UCommonButtonStyle> SellStyle;

	UPROPERTY(EditAnywhere, Category = "Bubble")
	TObjectPtr<UTexture2D> ClaimIcon;

	UPROPERTY(EditAnywhere, Category = "Bubble")
	TObjectPtr<UTexture2D> SellIcon;

	// 골드·그린 면 위 잉크(시안 #3A2A00 / #0E2A08). 라벨 외곽선은 쓰지 않는다(카탈로그 §6).
	UPROPERTY(EditAnywhere, Category = "Bubble")
	FLinearColor ClaimInk = FLinearColor::FromSRGBColor(FColor(0x3A, 0x2A, 0x00));

	UPROPERTY(EditAnywhere, Category = "Bubble")
	FLinearColor SellInk = FLinearColor::FromSRGBColor(FColor(0x0E, 0x2A, 0x08));

	// 상자 아이콘은 흰 면 + 검은 테라 원색 그대로, 배 아이콘은 흰 실루엣이라 잉크로 칠한다. 알파 0 = 원색.
	UPROPERTY(EditAnywhere, Category = "Bubble")
	FLinearColor ClaimIconTint = FLinearColor::Transparent;

	UPROPERTY(EditAnywhere, Category = "Bubble")
	FLinearColor SellIconTint = FLinearColor::FromSRGBColor(FColor(0x0E, 0x2A, 0x08));

private:
	void ApplyKindLook();

	EMapActionBubbleKind Kind = EMapActionBubbleKind::Claim;
	ECountryType Country = ECountryType::None;
	bool bLookApplied = false;
	EMapActionBubbleKind AppliedKind = EMapActionBubbleKind::Claim;
};
