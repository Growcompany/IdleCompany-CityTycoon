#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Engine/DataTable.h"
#include "LoadingTipTable.generated.h"

/** 팁을 띄울 도착지. 전환은 목적지를 알고 시작되므로 도착할 화면에 맞는 팁만 고를 수 있다 */
UENUM(BlueprintType)
enum class ELoadingTipContext : uint8
{
	Any			UMETA(DisplayName = "전체"),
	City		UMETA(DisplayName = "도시"),
	Office		UMETA(DisplayName = "사무실"),
	WorldMap	UMETA(DisplayName = "월드맵"),
	Recruitment	UMETA(DisplayName = "채용")
};

/**
 * 로딩/전환 화면 하단 팁 1줄 (DT_LoadingTip 행, RowName = TipKey).
 * 문구 SOT = `DataImport/DT_LoadingTip_Import.csv` ― 재임포트는 전체 교체이므로 CSV 를 먼저 고칠 것.
 */
USTRUCT(BlueprintType)
struct FLoadingTipTable : public FTableRowBase
{
	GENERATED_BODY()

	// RowName 미러. 직전에 띄운 팁을 피하는 키로도 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName TipKey;

	// 묶음 라벨(Basics/Employee/Project/…). 추첨에는 안 쓰고 저작·교체 단위로만 쓰므로 FName ― 새 묶음에 C++ 수정이 필요없다
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELoadingTipContext Context = ELoadingTipContext::Any;

	// 0 = 등장하지 않음. 문구를 지우지 않고 끄는 축
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Weight = 10;

	// 안내톤("~습니다"). 이모지 금지, em-dash 대신 한글 줄표. 한 줄 저작 상한 = FLoadingTipRules::MaxBodyChars
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Body;
};

/**
 * 추첨 규칙. 순수 함수라 DT 에셋·뷰포트 없이도 자동화가 컨텍스트/가중치/반복회피를 검증한다.
 */
struct FLoadingTipRules
{
	/**
	 * 한 줄 저작 상한(글자 수). 가장 좁은 화면비(가상 가로 2304)에서 NEXON Regular 28 한 줄이 들어가는 한계.
	 * 넘으면 오버레이가 2줄로 접지만 밴드가 두꺼워지므로 저작 단계에서 지킬 것.
	 */
	static constexpr int32 MaxBodyChars = 40;

	/** DT 미로드/전원 Weight 0 일 때의 최후 문구. 문안 수정은 CSV 에서 한다 */
	static FText LastResort()
	{
		return FText::FromString(TEXT("잠시만 기다려 주세요."));
	}

	static bool IsEligible(const FLoadingTipTable& Row, ELoadingTipContext Context)
	{
		if (Row.Weight <= 0 || Row.Body.IsEmpty())
		{
			return false;
		}
		return Row.Context == ELoadingTipContext::Any || Context == ELoadingTipContext::Any
			|| Row.Context == Context;
	}

	/**
	 * 가중 추첨. LastKey 는 연속 노출을 피하려 1순위에서 제외하되, 그래서 후보가 0이 되면 다시 넣는다
	 * ― 해당 컨텍스트에 팁이 하나뿐일 때 빈 밴드가 뜨는 것보다 반복이 낫다.
	 * Roll 은 호출자가 넘기는 난수(결정적 테스트용).
	 */
	static int32 Pick(TArrayView<const FLoadingTipTable> Rows, ELoadingTipContext Context,
		FName LastKey, uint32 Roll)
	{
		for (int32 Attempt = 0; Attempt < 2; ++Attempt)
		{
			const bool bAvoidLast = Attempt == 0 && !LastKey.IsNone();
			int64 Total = 0;
			for (const FLoadingTipTable& Row : Rows)
			{
				if (IsEligible(Row, Context) && !(bAvoidLast && Row.TipKey == LastKey))
				{
					Total += Row.Weight;
				}
			}
			if (Total <= 0)
			{
				continue;
			}
			int64 Cursor = static_cast<int64>(Roll % static_cast<uint32>(Total));
			for (int32 Index = 0; Index < Rows.Num(); ++Index)
			{
				const FLoadingTipTable& Row = Rows[Index];
				if (!IsEligible(Row, Context) || (bAvoidLast && Row.TipKey == LastKey))
				{
					continue;
				}
				Cursor -= Row.Weight;
				if (Cursor < 0)
				{
					return Index;
				}
			}
		}
		return INDEX_NONE;
	}
};
