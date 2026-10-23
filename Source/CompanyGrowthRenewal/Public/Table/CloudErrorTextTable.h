#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CloudErrorTextTable.generated.h"

/**
 * 서버 에러코드 → 화면 안내 문구 (DT_CloudErrorText 행, RowName = 코드).
 * 서버 `message` 는 개발자용 영문이고 `INTERNAL` 은 항상 "internal error" 고정이라 화면에 그대로 쓸 수 없다(설계서 §6.0).
 * 코드 문자열은 설계서 부록 A 가 SOT ― 여기서 코드를 만들지 말 것.
 */
USTRUCT(BlueprintType)
struct FCloudErrorTextTable : public FTableRowBase
{
	GENERATED_BODY()

	// 부록 A 코드. 공통 폴백 행은 DEFAULT
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Code;

	// 안내톤("~습니다"). 이모지 금지, em-dash 대신 한글 줄표
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Message;
};

/**
 * 코드 → 문구 조회 규칙. 순수 함수라 DT 에셋 없이도 자동화가 폴백 경로를 검증한다.
 */
struct FCloudErrorTextRules
{
	/** 공통 폴백 행의 RowName/Code */
	static const FName& DefaultCode()
	{
		static const FName Default(TEXT("DEFAULT"));
		return Default;
	}

	/**
	 * 등록 코드 → 그 문구, 미등록·빈 코드 → DEFAULT 행, DEFAULT 행도 없으면 최후 문구.
	 * 빈 FText 를 돌려주지 않는다 ― 실패 안내가 사라지면 사용자는 버튼이 죽은 줄 안다.
	 */
	static FText Resolve(const TMap<FName, FText>& Texts, FName Code)
	{
		if (!Code.IsNone())
		{
			if (const FText* Found = Texts.Find(Code))
			{
				if (!Found->IsEmpty())
				{
					return *Found;
				}
			}
		}

		if (const FText* Fallback = Texts.Find(DefaultCode()))
		{
			if (!Fallback->IsEmpty())
			{
				return *Fallback;
			}
		}

		return LastResort();
	}

	/** DT 미로드/미등록 시의 최후 시스템 문구. 문안 수정은 DT_CloudErrorText 의 DEFAULT 행에서 한다 */
	static FText LastResort()
	{
		return FText::FromString(TEXT("잠시 후 다시 시도해 주세요."));
	}
};
