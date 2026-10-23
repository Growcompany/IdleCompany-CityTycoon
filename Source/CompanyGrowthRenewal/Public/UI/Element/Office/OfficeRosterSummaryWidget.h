#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/Element/Common/ChipPlateSize.h"
#include "OfficeRosterSummaryWidget.generated.h"

class UImage;
class UOfficeRosterRowWidget;
struct FOfficeRosterLine;

/**
 * 사무실 오른쪽 위 직무 구성 목록. 스스로 EmployeeManager 를 구독해 갱신하므로
 * UI_OfficeMapMain 은 트리에 품기만 하고 C++ 참조를 두지 않는다.
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeRosterSummaryWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 데이터 입구 ― Refresh 와 테스트가 같은 경로를 쓴다. MemberCount 0 이면 판을 접는다. */
	void ApplyLines(const TArray<FOfficeRosterLine>& Lines, int32 MemberCount);

	UOfficeRosterRowWidget* GetRow(int32 Index) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	void Refresh();
	void HandleRosterChanged() { Refresh(); }
	void HandleEmployeeChanged(int32 EmployeeID) { Refresh(); }

	// 직능 슬롯 순서 6줄 ― 데이터 주입 대상이라 전부 필수
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UOfficeRosterRowWidget> Row0 = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UOfficeRosterRowWidget> Row1 = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UOfficeRosterRowWidget> Row2 = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UOfficeRosterRowWidget> Row3 = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UOfficeRosterRowWidget> Row4 = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UOfficeRosterRowWidget> Row5 = nullptr;

	// 떠 있는 SDF 판 2층 ― 줄 수·숫자 폭에 따라 판 크기가 바뀌어 실치수를 매번 넣는다
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> PlateBG = nullptr;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> PlateLine = nullptr;

	FChipPlateSizer PlateSizer;
};
