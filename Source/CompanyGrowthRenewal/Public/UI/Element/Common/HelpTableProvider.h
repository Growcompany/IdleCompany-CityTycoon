#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HelpTableProvider.generated.h"

/** 표 한 행. 항목 · 수치 · 비고 3열 고정 ― 더 필요하면 열이 아니라 축으로 나눈다. */
USTRUCT()
struct FHelpTableRow
{
	GENERATED_BODY()

	// 명사구. "최소 회수" ― 문장을 쓰면 표가 다시 글이 된다
	UPROPERTY()
	FText Label;

	UPROPERTY()
	FText Value;

	// 기준을 밝히는 짧은 꼬리표. "인수가 대비"
	UPROPERTY()
	FText Note;
};

/** 표의 축 구성. 행은 여기 없다 ― 보이는 칸만 그때그때 만든다. */
USTRUCT()
struct FHelpTableInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FText Title;

	// 페이저로 넘기는 축 (티어 · 등급 등). 1개 이하면 페이저를 숨긴다
	UPROPERTY()
	TArray<FText> PageLabels;

	// 탭으로 넘기는 축 (보는 항목). 1개 이하면 탭을 숨긴다
	UPROPERTY()
	TArray<FText> AxisLabels;

	// 출처 한 줄. "수치는 DT_CityCompany 에서 읽어옵니다"
	UPROPERTY()
	FText Footnote;
};

/** 축이 하나뿐이면 전환기를 내보내지 않는다 ― 고를 게 없는 컨트롤은 자리만 먹는다. */
inline bool ShouldShowPager(int32 PageCount) { return PageCount > 1; }
inline bool ShouldShowAxisTabs(int32 AxisCount) { return AxisCount > 1; }

UINTERFACE(MinimalAPI)
class UHelpTableProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * 도움말 "?" 가 숫자를 보여줘야 하는 화면에서, 그 숫자를 만들어 넘기는 쪽.
 *
 * 도움말은 숫자를 소유하지 않는다 ― 확률·비용·배율을 DT_Help 에 복사해 적으면
 * 밸런스를 한 번 고칠 때마다 도움말만 조용히 틀린다. 소유 패널이 자기 DT 에서 읽어 준다.
 *
 * 행을 미리 다 만들지 않는 이유: 10티어 x 6산업 x 2항목이면 섹션이 120개다.
 * 페이저/탭을 누를 때마다 GetHelpTableRows 를 다시 부른다.
 *
 * 설계 = docs/superpowers/specs/2026-09-23-help-button-design.md §9.4
 */
class IHelpTableProvider
{
	GENERATED_BODY()

public:
	// false 면 이 키에 줄 표가 없다는 뜻 ― "?" 는 글이나 연결 패널로 내려간다
	virtual bool GetHelpTableInfo(FName HelpKey, FHelpTableInfo& Out) const = 0;

	virtual void GetHelpTableRows(FName HelpKey, int32 PageIndex, int32 AxisIndex,
		TArray<FHelpTableRow>& Out) const = 0;
};
