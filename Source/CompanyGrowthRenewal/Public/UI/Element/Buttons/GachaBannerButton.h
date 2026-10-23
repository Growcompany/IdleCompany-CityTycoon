// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "GachaBannerButton.generated.h"

class UImage;
class UTextBlock;
class UMaterialInterface;

/**
 * 가챠 배너 셀 버튼 — 티켓 아이콘 + 이름(ButtonText 상속) + 서브 한 줄.
 * 셀 = 버튼 자신(UCommonButtonBase 파생 유지 → 패널 BindWidget 계약 무변경).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UGachaBannerButton : public UButtonWidget
{
	GENERATED_BODY()

public:
	void SetBannerPresentation(const FText& InText, UObject* IconResource);

	// 배지의 보유 수량과 단위를 갱신한다. 종류 아이콘은 좌측 TierIcon이 표시한다.
	void SetOwnedCount(int32 Count, bool bBoxUnit = false);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* TierIcon;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* SubText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* CountText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UTextBlock* CountUnit;

	// 배지 전체(판+숫자+단위). 수량을 주입하지 않는 패널(특성/스킨)에서 "0장"이 붙지 않게 기본은 숨김
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* CountPlate;

	// 아래 넷은 레일 변형(UIE_GachaBannerButton_Rail) 전용 — 브러시는 WBP 소유, C++ 은 선택 상태와 바탕 텍스처만 싣는다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* FaceImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* SelectBand;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* SelectSheen;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* UnselectedVeil;

	// 배너별 정적 값 — 디자이너 인스턴스 지정 (아이콘 텍스처/표시 크기/서브 문구)
	UPROPERTY(EditAnywhere, Category = "Banner")
	UTexture2D* TierIconTexture = nullptr;

	// 머티리얼 아이콘(MI, 티어색 파라미터) — 설정되면 TierIconTexture 보다 우선
	UPROPERTY(EditAnywhere, Category = "Banner")
	UMaterialInterface* TierIconMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Banner")
	FVector2D TierIconSize = FVector2D(56.f, 56.f);

	UPROPERTY(EditAnywhere, Category = "Banner")
	FText SubTextValue;

	// 등급 색 바탕 — FaceImage 의 그리기 방식·반경은 그대로 두고 리소스만 바꾼다
	UPROPERTY(EditAnywhere, Category = "Banner")
	UTexture2D* FaceTexture = nullptr;

	// 기본값 = 다크 판 위 잉크. 등급 색 바탕을 쓰는 변형은 흰색 계열로 덮는다
	UPROPERTY(EditAnywhere, Category = "Banner")
	FLinearColor CountColor = FLinearColor(0.838f, 0.855f, 0.871f);

	UPROPERTY(EditAnywhere, Category = "Banner")
	FLinearColor CountZeroColor = FLinearColor(0.188f, 0.228f, 0.275f);

	// 0장일 때 배지 전체(보유·숫자·장)를 함께 흐리게
	UPROPERTY(EditAnywhere, Category = "Banner")
	float CountZeroOpacity = 1.f;

	virtual void NativePreConstruct() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;
	void RefreshTierIcon();
	void RefreshFace();
	void ApplySelectionVisual();
};
