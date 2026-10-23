#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "OfficeRosterRowWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UTexture2D;

/** 사무실 직무 구성 목록의 한 줄(아이콘·인원·능력치). 글꼴·간격은 WBP 가 소유하고 C++ 는 값과 0명 상태만 넣는다. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeRosterRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetLine(UTexture2D* Icon, int32 Headcount, const FText& PowerLabel);

#if WITH_DEV_AUTOMATION_TESTS
	const UImage* GetIconForTest() const { return DisciplineIcon; }
	const UCommonTextBlock* GetCountTextForTest() const { return CountText; }
	const UCommonTextBlock* GetUnitTextForTest() const { return UnitText; }
	const UCommonTextBlock* GetPowerTextForTest() const { return PowerText; }
#endif

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> DisciplineIcon = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> CountText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> UnitText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> PowerText = nullptr;

	// 잉크 노브 ― WBP CDO 가 SOT(linear). 0명은 초반의 정상 상태라 경고색 대신 줄 전체를 흐리게 낮춘다
	UPROPERTY(EditAnywhere, Category = "Office Roster")
	FLinearColor CountInk = FLinearColor(0.8388f, 0.8550f, 0.8714f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Office Roster")
	FLinearColor UnitInk = FLinearColor(0.5457f, 0.5972f, 0.6724f, 1.f);

	// 0명 줄의 인원과 「명」이 함께 쓴다
	UPROPERTY(EditAnywhere, Category = "Office Roster")
	FLinearColor ZeroCountInk = FLinearColor(0.2122f, 0.2582f, 0.3325f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Office Roster")
	float ZeroIconOpacity = 0.38f;

	UPROPERTY(EditAnywhere, Category = "Office Roster")
	float ZeroPowerOpacity = 0.55f;
};
