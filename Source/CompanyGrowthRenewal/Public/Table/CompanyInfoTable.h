// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Enum/CompanyType.h"
#include "CompanyInfoTable.generated.h"

class UCommonButtonStyle;

/**
 * 산업(회사 타입)별 메타 정보 (DT 단일 진실 원천).
 * 아이콘/표시명/강조색 등 산업 표현 관련 모든 값.
 *
 * 사용처:
 *  - CountrySellRowWidget — 자유판매 모달의 산업 이름 + 강조색
 *  - IndustryButtonWidget — 빌드 모달 산업 타일의 글리프 + 이름 + 시그니처색 + 버튼 스타일
 *  - 향후 산업별 표현 필요한 모든 위젯
 *
 * Row Name = ECompanyType enum 이름 ("Game", "Electronics", "Semiconductor" 등)
 */
USTRUCT(BlueprintType)
struct COMPANYGROWTHRENEWAL_API FCompanyInfoTable : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	ECompanyType CompanyType = ECompanyType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	FText DisplayName;

	// 타일용 단색 글리프 (시그니처색 타일 위에 크림 틴트로 올림)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	TSoftObjectPtr<UTexture2D> GlyphIcon;

	// 산업 타일 버튼 스타일 (CUI_Style2_Btn_* — 시그니처색 배경/눌림). IndustryButtonWidget 이 SetStyle 로 적용.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	TSoftClassPtr<UCommonButtonStyle> ButtonStyle;

	/** 산업별 강조색 (제목/배지/하이라이트 등). 미지정 시 흰색. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	FLinearColor AccentColor = FLinearColor::White;

	/** 카테고리 라벨 ("프로젝트형", "제조업형" 등) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	FText CategoryLabel;

	/** 산업 한 줄 설명 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	FText Description;

	/** 특징 1 (불릿 포인트) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	FText Feature1;

	/** 특징 2 (불릿 포인트) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	FText Feature2;

	// 해금에 필요한 본사 레벨 (0 = 시작부터). 산업 게이트 = HQ 단일 스파인 (DECISION_RECORDS §1.3)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Company")
	int32 RequiredHQLevel = 0;
};
