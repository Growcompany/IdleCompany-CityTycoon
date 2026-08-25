// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Manager/SaveLoadManager.h"
#include "OfflineGainRowWidget.generated.h"

class UCommonTextBlock;
class UImage;

/**
 * 오프라인 정산 모달의 빌딩 1행.
 *
 * 금고 초과 손실(LossByVault)과 운영 수명 만료(bLifespanBound)는 서로 다른 사건이라 표시를 분리한다.
 * 수명 종료는 정상 종료이므로 손실 언어/레드와 접촉시키지 않는다 — 중립 태그만 붙인다.
 * 둘은 독립이라 한 행에 동시에 뜰 수 있고, 그게 데이터상 정확하다
 * (RawGain 이 이미 EffectiveSeconds 기준이라 LossByVault 에 수명 손실이 섞이지 않음).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfflineGainRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetRowData(const FOfflineGainEntry& Entry);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* BuildingNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* GainText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* BuildingIcon = nullptr;

	// 업종 글리프 — 건물 카드(FBuildableCardTable)엔 CompanyType 이 없어 액터에서 따로 끌어온다.
	// 텍스처는 화이트 1종 재사용하고 색만 DT 시그니처색으로 틴트 (카탈로그 §1 — 색 베이크 금지)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* IndustryGlyph = nullptr;

	// 업종명. ⚠ UMETA(DisplayName) 은 에디터 전용이라 패키징 빌드에서 영어로 떨어진다 —
	// 반드시 DT_CompanyInfo.DisplayName(FText) 경유
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* IndustryText = nullptr;

	// 손실 서브라인 전체 — 컨테이너째 접어야 빈 줄이 안 남는다
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* LossRow = nullptr;

	// 금액만 지출 레드. 주변 라벨("금고 가득 ―", "놓침")은 WBP 정적 텍스트가 담당
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* LossAmountText = nullptr;

	// 운영 수명 만료 중립 태그 ([운영 종료])
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* LifespanTag = nullptr;
};
