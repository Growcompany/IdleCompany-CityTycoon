#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Enum/CompanyType.h"
#include "IndustryButtonWidget.generated.h"

class UImage;
class UBorder;

/**
 * 산업 선택 타일 버튼 (CommonButton 정식 변형).
 * - 버튼 = 투명 CardClear 스타일로 터치 영역을 제공
 * - 라벨 = 상속 ButtonText (DT DisplayName)
 * - 면 = FacePlate (DT 산업색으로 채우고 명도를 조절)
 * - 글리프 = GlyphImage (DT GlyphIcon의 전체 형태를 면 위 배경 문양으로 표시)
 * - 선택 표시 = SelectionBorder/CheckImage (NativeOnSelected/Deselected 로 토글)
 * - 배타선택은 UBuildModalWidget 의 UCommonButtonGroupBase 가 담당. 클릭은 CommonButtonBase 네이티브 OnClicked().
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UIndustryButtonWidget : public UButtonWidget
{
	GENERATED_BODY()

public:
	// 디자인타임 인스턴스에서 어떤 산업 타일인지 지정 (런타임 확정 소스는 모달의 SetCompanyType)
	UPROPERTY(EditAnywhere, Category = "Industry")
	ECompanyType CompanyType = ECompanyType::Game;

	UFUNCTION(BlueprintCallable, Category = "Industry")
	void SetCompanyType(ECompanyType InType);

	UFUNCTION(BlueprintPure, Category = "Industry")
	ECompanyType GetCompanyType() const { return CompanyType; }

	// 잠금 표현 — 상호작용은 유지(잠금 탭 안내는 모달 담당), 선택만 차단
	void SetIndustryLocked(bool bInLocked);
	bool IsIndustryLocked() const { return bIndustryLocked; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;

	// 눌림 표현은 FacePlate가 소유한다. 투명 버튼 스타일은 터치 영역만 제공한다.
	virtual void NativeOnPressed() override;
	virtual void NativeOnReleased() override;

	// 업종 배경 문양 — DT GlyphIcon의 전체 형태를 표시하고 선택 여부에 따라 흰색 알파를 조절한다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* GlyphImage;

	// 타일 면 — 산업 시그니처색을 받는 판. 버튼 자체 스타일은 CardClear(투명)라 이 판이 유일한 면이다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UBorder* FacePlate;

	// 선택 링 — WBP 브러시를 유지하고 선택 상태에 따라 토글한다 (기본 Collapsed).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UBorder* SelectionBorder;

	// 선택 체크 마크 — 우측 칸(레이아웃 흐름). 미선택 시 Hidden(자리 유지), 선택 시 Visible.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* CheckImage;

	// 잠금 자물쇠 오버레이 — 잠금 시에만 표시 (스킨 카드와 동일한 Locked_Gold 잠금 언어)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* LockImage;

private:
	// DT GetCompanyInfo → 라벨/배경 문양/면색 주입. 디자인타임은 DT 직접 로드(IsDesignTime 가드).
	void ApplyCompanyInfo();

	// 파라미터명 bInSelected — CommonButtonBase::bSelected 멤버 셰도잉(C4458) 회피
	void UpdateSelectionBorder(bool bInSelected);

	// 면색 재적용. 눌림이면 같은 색을 어둡게 + 살짝 가라앉힌다.
	void ApplyFaceColor(bool bPressed);

	// ApplyCompanyInfo 가 산출한 명도 적용 면색 — 눌림 복귀 기준값.
	FLinearColor CachedFaceColor = FLinearColor::White;

	FLinearColor GlyphAccentColor = FLinearColor::White;

	bool bIndustryLocked = false;
};
