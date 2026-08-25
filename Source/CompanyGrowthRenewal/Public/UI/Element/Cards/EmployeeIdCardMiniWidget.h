// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Data/GachaRecruitmentData.h"
#include "EmployeeIdCardMiniWidget.generated.h"

class UTextBlock;
class UImage;
class UMaterialInstanceDynamic;

/**
 * 가챠 멀티 리빌 사이드 사원증 (인쇄 완료 상태 — 도장/텍스트 정적, 사진만 라이브 RT UV 크롭).
 * 트리는 센터 사원증(UI_EmployeeGachaPresentation IdCardBox)의 560x773 미러 — 표시는 RenderScale 로 축소.
 * 등급 연출: 레어+ 시트 광택 스윕 / 에픽+ 배후광 브리딩 (자체 NativeTick — 호스트는 배치/딜링만).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeIdCardMiniWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetCardData(const FGachaResultData& Result, int32 IdOrdinal,
		const FText& DeptDisplayName, const FString& RankDisplayName);
	void SetPhotoMaterial(UMaterialInstanceDynamic* MID);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// WBP 트리는 Task 9 에서 주입 — 전부 Optional + 호출부 null 가드
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* BandLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* DeptText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* RarityText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* PhotoImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* PhotoRing;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* CardShine;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* BackGlow;

private:
	int32 RarityTier = 0;
	float FxTime = 0.f;
};
