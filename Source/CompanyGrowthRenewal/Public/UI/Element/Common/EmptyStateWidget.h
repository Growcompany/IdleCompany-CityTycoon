// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Layout/Margin.h"
#include "EmptyStateWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UTexture2D;

/**
 * 목록이 0건일 때 자리를 채우는 공용 부품 — 아이콘 한 장 + 안내 문구 한 줄.
 * 정적 스타일(면 색·폰트·정렬)은 WBP 가, 데이터(아이콘/문구)는 C++ 이 소유한다.
 * 다크 면은 UIE_EmptyState, 크림 면은 Child WBP UIE_EmptyState_Light 가 Class Defaults 로 틴트를 베이크한다.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UEmptyStateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 아이콘 + 문구 동시 주입. Icon 이 nullptr 이면 아이콘만 접힌다.
	UFUNCTION(BlueprintCallable, Category = "Empty State")
	void SetEmptyState(UTexture2D* Icon, const FText& Msg);

	// 문구만 교체 — 아이콘은 WBP 저작값 유지 (소비처가 상황별 문구를 갈아끼우는 경로)
	UFUNCTION(BlueprintCallable, Category = "Empty State")
	void SetMessage(const FText& Msg);

protected:
	virtual void NativeConstruct() override;
	virtual void SynchronizeProperties() override;

	void ApplyEmptyState();

	UPROPERTY(meta = (BindWidget))
	UImage* EmptyIcon;

	UPROPERTY(meta = (BindWidget))
	UCommonTextBlock* EmptyMessage;

	// 비우면 아이콘이 접힌다 — 문구만 쓰는 소비처를 위해
	UPROPERTY(EditAnywhere, Category = "EmptyState")
	TSoftObjectPtr<UTexture2D> IconTexture;

	// 좁은 레일 96 / 넓은 그리드 112 (원본 텍스처는 전부 정사각 256)
	UPROPERTY(EditAnywhere, Category = "EmptyState")
	FVector2D IconSize = FVector2D(96.0f, 96.0f);

	// 아이콘↔문구 간격. 슬롯 종류(VBox/HBox/Overlay/Border)는 ApplyWidgetIcon 이 흡수한다
	UPROPERTY(EditAnywhere, Category = "EmptyState")
	FMargin IconPadding = FMargin(0.0f, 0.0f, 0.0f, 12.0f);

	// 다크 면 #8A97A8. 알파 페이드가 아니라 색 자체를 낮춘다 — 반투명은 패턴 배경 위에서 얼룩진다
	UPROPERTY(EditAnywhere, Category = "EmptyState")
	FLinearColor IconTint = FLinearColor(0.254f, 0.310f, 0.392f, 1.0f);

	// 소비처가 상황별 문구를 주입하면 비워 둔다 — 빈 FText 면 문구가 접힌다
	UPROPERTY(EditAnywhere, Category = "EmptyState")
	FText Message;
};
