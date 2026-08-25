#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Buttons/ButtonWidget.h"
#include "Enum/CompanyType.h"
#include "IndustryButtonWidget.generated.h"

class UImage;
class UBorder;

/**
 * 산업 선택 타일 버튼 (CommonButton 정식 변형).
 * - 배경/눌림 = 산업별 CUI_Style2_Btn_* 스타일 (DT ButtonStyle 컬럼에서 로드해 SetStyle)
 * - 라벨 = 상속 ButtonText (DT DisplayName)
 * - 글리프 = GlyphImage (DT GlyphIcon, 크림 틴트는 WBP 기본값)
 * - 선택 글로우 = SelectionBorder (MI_UI_Glow_Outline 브러시, NativeOnSelected/Deselected 로 토글)
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

	// 산업 단색 글리프 — DT GlyphIcon 로드. 크림 틴트(#F0E6D0)는 WBP ColorAndOpacity 기본값.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* GlyphImage;

	// 선택 글로우 링 — MI_UI_Glow_Outline 브러시. 표시는 선택 상태에 따라 토글 (기본 Collapsed).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UBorder* SelectionBorder;

	// 선택 체크 마크 — 우측 칸(레이아웃 흐름). 미선택 시 Hidden(자리 유지), 선택 시 Visible.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* CheckImage;

	// 잠금 자물쇠 오버레이 — 잠금 시에만 표시 (스킨 카드와 동일한 Locked_Gold 잠금 언어)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* LockImage;

private:
	// DT GetCompanyInfo → ButtonText/GlyphImage/SetStyle 주입. 디자인타임은 DT 직접 로드(IsDesignTime 가드).
	void ApplyCompanyInfo();

	// 파라미터명 bInSelected — CommonButtonBase::bSelected 멤버 셰도잉(C4458) 회피
	void UpdateSelectionBorder(bool bInSelected);

	// 선택 시 글리프에 입힐 산업 시그니처색(ApplyCompanyInfo 에서 캐시). 미선택 글리프는 중립 틴트(무지개 방지).
	FLinearColor GlyphAccentColor = FLinearColor::White;

	bool bIndustryLocked = false;
};
