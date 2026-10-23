// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "AttendanceMilestoneNodeWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UWidget;

// 받은 단계 = 금 원판, 다음 단계 = 어두운 원판 + 금 테, 남은 단계 = 어두운 원판 + 흑백 선물 (스펙 ③)
UENUM()
enum class EAttendanceNodeState : uint8
{
	Got,
	Next,
	Pending
};

/**
 * 출석 진행 막대 위 특별 보상 원판 1개. 판 3벌 · 선물 2벌 · 「N일」 라벨은 WBP 가 저작하고
 * C++ 은 상태로 한 벌씩 켜고 라벨 글자·잉크만 고른다. 위치(막대 눈금)는 패널이 정한다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UAttendanceMilestoneNodeWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetNode(int32 Day, EAttendanceNodeState State);

protected:
	// 라벨 잉크는 WBP 가 저작한다 — 받은 단계만 금색, 나머지는 흐린 글자
	UPROPERTY(EditAnywhere, Category = "Attendance|Ink")
	FSlateColor LabelInkGot = FSlateColor::UseForeground();

	UPROPERTY(EditAnywhere, Category = "Attendance|Ink")
	FSlateColor LabelInkOther = FSlateColor::UseForeground();

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> PlateGot = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> PlateNext = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UWidget> PlatePending = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> GiftColor = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UImage> GiftGray = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> DayLabel = nullptr;

private:
	// 무명 네임스페이스 헬퍼는 유니티 TU 병합에서 재정의가 된다 — 클래스 안에 둔다
	static ESlateVisibility Show(bool bVisible);
};
