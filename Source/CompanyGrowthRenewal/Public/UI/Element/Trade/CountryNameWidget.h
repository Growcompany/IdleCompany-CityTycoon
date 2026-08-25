// UCommonButtonBase 상속으로 탭/클릭 감지 자동 처리

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Entity/Country/CountryActor.h"
#include "Enum/CompanyType.h"
#include "CountryNameWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UCountryMarketManager;

DECLARE_DELEGATE_OneParam(FOnCountryFlagClicked, ECountryType /*CountryType*/);

/**
 * CountryActor 위치에 표시되는 국기 버튼 위젯
 *
 * 잠금 상태 지원: SetLockState(true) 호출 시
 *  - BGImage / FlagIcon에 어두운 틴트 적용
 *  - LockImage / LockText 노출
 *  - 클릭 비활성화
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCountryNameWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Country")
	void SetFlagTexture(UTexture2D* FlagTexture);

	void SetCountryType(ECountryType InType);

	// 잠금 상태 토글 — 어두운 틴트 + 잠금 아이콘 + 비활성화
	UFUNCTION(BlueprintCallable, Category = "Country")
	void SetLockState(bool bInLocked, const FText& InLockReason = FText::GetEmpty());

	FOnCountryFlagClicked OnFlagClicked;

protected:
	virtual void NativeOnClicked() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> BGImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> FlagIcon;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LockImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> LockText;

	// 산업별 수요 닷 (3종 fixed). DT_CountryDemand 미정의 셀은 Collapsed.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> DotSemiconductor;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> DotElectronics;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> DotAutomobile;

	// 잠금 시 BGImage / FlagIcon 공통 색상 (기본 #414141FF)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Country|Lock")
	FLinearColor LockedTint = FLinearColor(FColor(0x41, 0x41, 0x41, 0xFF));

private:
	ECountryType CountryType = ECountryType::None;
	bool bLocked = false;

	UPROPERTY()
	TObjectPtr<UCountryMarketManager> MarketMgr;

	void RefreshAllDots();
	void RefreshDot(UImage* Dot, ECompanyType Industry);

	UFUNCTION()
	void HandleDemandChanged(ECountryType InCountry, ECompanyType InIndustry, float NewRatio);
};
