// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Data/EmployeeTypes.h"
#include "EmployeeRosterCardWidget.generated.h"

class UImage;
class UCommonTextBlock;

// 로스터 카드 클릭 델리게이트 (EmployeeID 만 — 선택 확정/상세 표시는 호스트 직원창)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRosterCardClicked, int32, EmployeeID);

/**
 * 직원창 좌측 로스터 레일 카드 (ListView Entry, UIE_EmployeeRosterCard)
 * - 얼굴 초상 + 이름 + Lv + 채워진 별(★×EnhancementLevel, 0성 숨김) + 등급 젬(SpawnRarity → GetRarityColor)
 * - 선택 상태 = ListView 아이템 선택 단일 진실 (재활용 엔트리도 NativeOnListItemObjectSet 에서 재질의)
 * - 스타일 에셋 미사용, RoundedBox 데이터 주도 (등급색·선택 상태를 C++ 가 브러시에 직접 주입)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeRosterCardWidget : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Employee")
	void SetEmployeeInfo(const FEmployeeInstance& InEmployeeData);

	UFUNCTION(BlueprintPure, Category = "Employee")
	int32 GetEmployeeID() const { return EmployeeData.EmployeeID; }

	// 선택 하이라이트 토글 (호스트/ListView 선택 상태 반영)
	void SetSelectedVisual(bool bNowSelected);

	UPROPERTY(BlueprintAssignable, Category = "Employee|Event")
	FOnRosterCardClicked OnRosterCardClicked;

	// Saved/Portraits/{ID}.png 로드 + RGB/알파 분리 다운스케일 → transient 텍스처 (실패 시 nullptr).
	// 기존 카드 5곳의 중복 루틴과 동일 — 신규 위젯은 이 static 공유, 구 카드 통합은 P4 레거시 정리에서.
	static UTexture2D* LoadPortraitTexture(int32 EmployeeID, const FVector2D& TargetSize);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// IUserObjectListEntry
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;

	// 선택 하이라이트 (블루 틴트 RoundedBox — 비선택 시 Hidden)
	// ※ 초상화 4코너 브래킷은 2026-07-22 시도 후 기각 — 카드 키라인과 선택 언어 중복(HQ 헤드와 동일 원칙).
	//   브래킷은 카드 크롬 없는 정사각 대상(도감 셀/가챠 픽/튜토리얼 지시) 전용.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* SelectionBG;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UImage* PortraitImage;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* NameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* LevelText;

	// 채워진 별 (골드) — 0성이면 Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* StarsText;

	// 등급 젬 — SpawnRarity(입사 가챠 등급, 불변) 색. 별(강화)과 역할 분리
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* RarityGem;

	// 잠재능력 등급 오빗 글린트 — PotentialAbility.CurrentRarity 색(WBP 미배선 상태에서도 안전하도록 Optional)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* PotentialGlint;

	// 우측 종합 수치 (CalculateOverall — 전투력식 요약)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UCommonTextBlock* OverallText;

	// 초상화 표시 크기 (소프트웨어 다운스케일)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait")
	FVector2D PortraitDisplaySize = FVector2D(88.f, 110.f);

private:
	UPROPERTY()
	FEmployeeInstance EmployeeData;

	void OnButtonClicked();
};
