// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Data/EmployeeTypes.h"
#include "Utils/FWidgetAnimationUtils.h"
#include "EmployeeRosterCardWidget.generated.h"

class UImage;
class UCommonTextBlock;
class UWidget;

// 로스터 카드 클릭 델리게이트 (EmployeeID 만 — 선택 확정/상세 표시는 호스트 직원창)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRosterCardClicked, int32, EmployeeID);

// 빈자리 타일 클릭 — 직원 ID 가 없으므로 선택 경로와 분리한다
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRosterHireSlotClicked);

/**
 * 직원창 좌측 로스터 레일 카드 (ListView Entry, UIE_EmployeeRosterCard)
 * - 얼굴 초상 + 이름 + Lv + 별 아이콘과 강화 수치(N, 0강 숨김) + 등급 젬(SpawnRarity → GetRarityColor)
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

	// 빈자리("채용") 타일 모드 — 직원 레이어를 걷고 채용 레이어만 남긴다. bAffordable = 브리딩 펄스 on/off
	// 기본값 false = fail closed: 채용 재화를 아는 건 창뿐이라, 모르는 호출부가 펄스를 켜 두면 거짓 신호가 된다
	UFUNCTION(BlueprintCallable, Category = "Employee")
	void SetHireSlot(bool bAffordable = false);

	UFUNCTION(BlueprintPure, Category = "Employee")
	bool IsHireSlot() const { return bHireSlot; }

	UPROPERTY(BlueprintAssignable, Category = "Employee|Event")
	FOnRosterCardClicked OnRosterCardClicked;

	UPROPERTY(BlueprintAssignable, Category = "Employee|Event")
	FOnRosterHireSlotClicked OnRosterHireSlotClicked;

	// Saved/Portraits/{ID}.png 로드 + 원본 비율을 유지하는 RGB/알파 분리 다운스케일 (실패 시 nullptr).
	// 기존 카드 5곳의 중복 루틴과 동일 — 신규 위젯은 이 static 공유, 구 카드 통합은 P4 레거시 정리에서.
	static UTexture2D* LoadPortraitTexture(int32 EmployeeID, const FVector2D& TargetSize);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

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

	// 초상 캡처 전까지 세우는 실루엣. 빈 슬롯도 타일로 보여야 한다(레퍼런스 §2.1-6).
	// Optional 이라 이 이미지를 안 둔 기존 카드 WBP 는 그대로 동작한다.
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UImage* PortraitFallback = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* NameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCommonTextBlock* LevelText;

	// 별 아이콘 옆 강화 수치(N) — 0성이면 StarsChip과 함께 Collapsed
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

	// 빈자리 타일 레이어 ([+] 글리프 + "채용") — 직원 카드에서는 Collapsed
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	UWidget* HireLayer;

	// 초상화 표시 크기 (소프트웨어 다운스케일)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait")
	FVector2D PortraitDisplaySize = FVector2D(88.f, 110.f);

private:
	UPROPERTY()
	FEmployeeInstance EmployeeData;

	bool bHireSlot = false;

	// 채용 가능(재화 충분) 빈자리만 브리딩 — 불가능하면 정적 (§9-3)
	bool bHirePulse = false;

	// 선택 펀치 (§9-2). 목록 재구성으로 복원되는 선택은 사용자 행위가 아니라 연출하지 않는다
	FScalePunchAnimation SelectPunch;
	bool bSelectedVisual = false;
	bool bApplyingListItem = false;

	float PulseElapsed = 0.f;

	// 스케일을 건드린 적이 있는지 — 연출이 끝난 뒤 딱 한 번만 1 로 되돌리기 위한 축
	bool bRenderScaleDirty = false;

	void OnButtonClicked();
};
