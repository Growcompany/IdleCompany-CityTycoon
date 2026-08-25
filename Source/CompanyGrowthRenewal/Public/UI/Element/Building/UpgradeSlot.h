// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/ResourceType.h"
#include "UpgradeSlot.generated.h"

class UButton;
class UImage;
class UCommonTextBlock;
class UCommonBorder;
class UButtonWidget;
class UCostActionButtonWidget;
class UResourceWidget;
class UNiagaraSystemWidget;

/**
 * 재사용 가능한 업그레이드 슬롯 위젯
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UUpgradeSlot : public UUserWidget
{
	GENERATED_BODY()

public:
	// 편집 가능한 속성들
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade Config")
	TSoftObjectPtr<UTexture2D> Icon;

	// 슬롯 제목 (예: "매출 부스터")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade Config")
	FText Description = FText::FromString(TEXT("Upgrade"));

	// 슬롯 설명 (예: "돈을 더 많이 벌어요")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade Config")
	FText SubDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade Config")
	FText LockConditionText = FText::FromString(TEXT("Locked"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade Config")
	FString ValueUnit = TEXT("");

	// 현재/다음 값 앞에 붙는 기호(보너스 % 슬롯의 "+"). 델타 표기엔 안 붙는다 — 델타는 자기 부호를 따로 낸다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade Config")
	FString ValuePrefix = TEXT("");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade Config")
	bool bIsInteger = false;

	// 금액처럼 자릿수가 폭주하는 값 — 만/억/조로 축약해 슬롯 폭을 지킨다. 켜면 bIsInteger 는 무시된다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade Config")
	bool bAbbreviateValue = false;

	// 다음 레벨 증가분 표기. 값 두 개가 '진척'이 아닌 슬롯(재고 현재/최대 등)에선 끌 것
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade Config")
	bool bShowDelta = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade Config")
	EResourceType CostResourceType = EResourceType::Money;

	// 버튼에 표시될 텍스트 (비어있으면 기본값 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade Config")
	FText ButtonText;

	// 에디터 미리보기용 (디자인 타임에만 적용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrade Config")
	bool bPreviewLocked = false;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	// 바인딩된 위젯들
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* IconImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* DescriptionText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* SubDescriptionText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* LevelText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* CurrentValueText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* NextValueText;

	// 다음 레벨 증가분 (예: "(+0.05%)"). 델타 0 또는 MAX 면 Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* DeltaValueText;

	// 부가 병기 라인. 현재 쓰는 슬롯 없음(금고가 원 단위 주값으로 전환하며 병기 불필요) — 값 미설정이면 Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* SecondaryValueText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCostActionButtonWidget* UpgradeBtn;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonBorder* LockedBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* LockedOverlay;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* LockConditionTextBlock;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UNiagaraSystemWidget* UpgradeNiagaraEffect;

public:
	// UI 업데이트 함수 (단건 모드 — 벌크 오버로드에 Count=1 폴백 위임)
	void UpdateInfo(int32 Level, float CurrentValue, float NextValue, int64 Cost, bool bIsLocked, int32 MaxLevel = 0);

	// 벌크 모드 오버로드 — NextValue 는 +BulkCount 레벨 합산 후 값, 버튼 비용/캡션은 BulkTotalCost 기준.
	// BulkCount=0(최대 모드 자금 부족)이면 캡션에 부족액만 표기 (BulkTotalCost=1레벨 비용 전제)
	void UpdateInfo(int32 Level, float CurrentValue, float NextValue, int64 Cost, bool bIsLocked, int32 MaxLevel,
		bool bBulkMode, int32 BulkCount, int64 BulkTotalCost, int64 AvailableAmount);

	// 부가 병기 텍스트(예: 금고 원 환산). 빈 FText 면 Collapsed. SecondaryValueText 미바인딩이면 무동작.
	void SetSecondaryAnnotation(const FText& InText);

	// 레벨 뱃지 문구 교체(예: 빌드업의 "18층"). 빈 FText 면 기본 "Lv.N" 표기로 복귀.
	// UpdateInfo 가 매번 뱃지를 다시 쓰므로 값은 멤버로 남는다.
	void SetLevelBadgeOverride(const FText& InText);

	// 런타임 아이콘 텍스처 교체 (예: 선택된 책상 이미지) — NativePreConstruct 의 Icon 적용보다 나중이라 우선
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void SetIconTexture(UTexture2D* InTexture);

	// 데이터 행이 소유하는 제목/설명/비용 재화를 런타임에 주입한다.
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void SetRuntimePresentation(const FText& InTitle, const FText& InDescription, EResourceType InCostResourceType);

	// 업그레이드 성공 시 파티클 + 사운드 효과 재생. bPlaySound=false 면 시각만 (별도 사운드 사용 시 중복 방지)
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void PlayUpgradeEffect(bool bPlaySound = true);

	// 홀드 중 재구매 펀치 억제 패스스루. 패널의 Start/StopHold 에서 쌍으로 호출할 것
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void SetPunchSuppressed(bool bSuppressed);

	// 업그레이드 버튼 위젯 반환 (이벤트 바인딩용)
	UCostActionButtonWidget* GetUpgradeButton() const { return UpgradeBtn; }

	// 내부 UButton 직접 접근이 필요한 경우
	UButton* GetRawButton() const;

private:
	void ApplyDesignTimeSettings();

	// 홀드 연타 시 동일 문자열 SetText 스킵용
	FString CachedDeltaString;

	FText LevelBadgeOverride;
};
