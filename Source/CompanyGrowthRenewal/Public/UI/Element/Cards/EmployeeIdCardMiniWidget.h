// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Data/GachaRecruitmentData.h"
#include "EmployeeIdCardMiniWidget.generated.h"

class UTextBlock;
class UImage;
class UMaterialInstanceDynamic;
class UTexture2D;

/**
 * 가챠 멀티 리빌 사이드 사원증. 일반 뽑기는 라이브 RT, 오피스 입장은 저장 초상화를 표시한다.
 * 기본 트리는 센터 사원증(UI_EmployeeGachaPresentation IdCardBox)의 560x773 미러.
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
	bool SetPhotoTexture(UTexture2D* Texture);

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
