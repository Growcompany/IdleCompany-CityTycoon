// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Enum/CompanyType.h"
#include "Enum/ResourceType.h"
#include "MapIndustryTagWidget.generated.h"

class UHorizontalBox;
class UCommonTextBlock;
class UTexture2D;
class UTableManagerSubsystem;
struct FCountryInfoTable;

UENUM()
enum class EMapIndustryTagKind : uint8
{
	Factory,
	Mine,
	Oil,
	TradeHub,
};

/** 지도 이름표 한 칸이 무엇을 말하는지. 아이콘·글자·색은 위젯이 DT 에서 채운다. */
struct COMPANYGROWTHRENEWAL_API FMapIndustryTagSpec
{
	EMapIndustryTagKind Kind = EMapIndustryTagKind::Factory;
	ECompanyType Industry = ECompanyType::None;
	TArray<EResourceType> Resources;
	// 시설이 여럿인 나라는 자원 이름을 늘어놓지 않고 「광산」 한 단어로 줄인다.
	bool bMineWordOnly = false;
	bool bRefinery = false;

	bool operator==(const FMapIndustryTagSpec& Other) const
	{
		return Kind == Other.Kind && Industry == Other.Industry && Resources == Other.Resources
			&& bMineWordOnly == Other.bMineWordOnly && bRefinery == Other.bRefinery;
	}
	bool operator!=(const FMapIndustryTagSpec& Other) const { return !(*this == Other); }
};

namespace MapIndustryTags
{
	/**
	 * 국가 행 → 이름표 칸 목록(공장 → 광산/석유 → 무역항 순, 시안 `tagsOf`).
	 * bRefinery = 그 나라 광산이 디오라마에서 정유 외형(유전만 + 저장 3단계)인지 ― 호출부가 디오라마 입력에서 읽는다.
	 */
	COMPANYGROWTHRENEWAL_API TArray<FMapIndustryTagSpec> BuildSpecs(const FCountryInfoTable& Row, bool bRefinery);

	COMPANYGROWTHRENEWAL_API bool IsAllOil(const TArray<EResourceType>& Resources);

	// DT 에 시설 이름 행이 없어 표시 문자열을 여기 한 곳에만 둔다.
	COMPANYGROWTHRENEWAL_API FText MineWord();
	COMPANYGROWTHRENEWAL_API FText OilWord(bool bRefinery);
	COMPANYGROWTHRENEWAL_API FText TradeHubWord();
	COMPANYGROWTHRENEWAL_API FText JoinNames(const TArray<FText>& Names);
}

/**
 * 지도 이름표의 담당 산업 한 칸(아이콘 + 외곽선 글자). 판 없이 지도 위에 직접 얹힌다.
 * 글꼴·외곽선·크기는 WBP(UIE_MapIndustryTag)가, 산업 색·아이콘·이름은 DT 가 정한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMapIndustryTagWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** DT 에서 이 칸의 아이콘·글자·색을 찾아 싣는다. 못 찾은 값은 비우고 경고한다(코드 폴백 없음). */
	void ApplySpec(const FMapIndustryTagSpec& Spec, const UTableManagerSubsystem& TableMgr);

	// 아이콘 틴트 알파 0 = 원색 그대로(자원 아이콘은 이미 칠해져 있다).
	void SetTag(const TArray<UTexture2D*>& Icons, const FText& Label, const FLinearColor& TextColor, const FLinearColor& IconTint);

	FText GetLabel() const;
	int32 GetIconCount() const;

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> IconRow;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> LabelText;

	UPROPERTY(EditAnywhere, Category = "MapTag")
	float IconSize = 36.0f;

	// 자원 아이콘이 둘 이상이면 겹쳐 붙인다(시안 `.tag img+img{margin-left:-8px}`).
	UPROPERTY(EditAnywhere, Category = "MapTag")
	float IconOverlap = 8.0f;

	// 아이콘 뒤에 깔 어두운 테(시안 drop-shadow #0B1018) ― 같은 그림을 조금 키워 어둡게 칠한다.
	UPROPERTY(EditAnywhere, Category = "MapTag")
	FLinearColor IconHaloColor = FLinearColor::FromSRGBColor(FColor(0x0B, 0x10, 0x18, 0xE6));

	UPROPERTY(EditAnywhere, Category = "MapTag")
	float IconHaloScale = 1.14f;

	// 산업 행이 없는 칸의 색. 공장 칸은 DT_CompanyInfo.AccentColor 를 쓴다.
	UPROPERTY(EditAnywhere, Category = "MapTag")
	FLinearColor MineColor = FLinearColor::FromSRGBColor(FColor(0xEA, 0xDF, 0xC6));

	UPROPERTY(EditAnywhere, Category = "MapTag")
	FLinearColor OilColor = FLinearColor::FromSRGBColor(FColor(0xD3, 0xDA, 0xE4));

	UPROPERTY(EditAnywhere, Category = "MapTag")
	FLinearColor HubColor = FLinearColor::FromSRGBColor(FColor(0x73, 0xE6, 0xD6));

	// 무역항 칸 아이콘(흰 실루엣 배 ― 청록으로 칠한다). 산업·자원처럼 읽어올 DT 행이 없어 WBP 가 든다.
	UPROPERTY(EditAnywhere, Category = "MapTag")
	TObjectPtr<UTexture2D> HubIcon;

private:
	bool bMissingWarned = false;
};
