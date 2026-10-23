#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enum/WidgetType.h"
#include "HelpButtonWidget.generated.h"

class UButton;
class UGuideTooltipWidget;
class USizeBox;

UENUM()
enum class EHelpMode : uint8
{
	None,   // 내용 없음 ― 버튼을 숨긴다
	Prose,  // DT_Help.Body 말풍선 한 장 (규칙만 있는 화면)
	Table,  // 소유 패널이 IHelpTableProvider 로 준 표 (숫자 화면)
	Panel,  // 숫자 화면이 이미 따로 있다 ― 그걸 연다 (가챠 확률 패널 등)
};

/**
 * 숫자가 있으면 숫자가 답이다 ― 글과 표 중에 고를 일이 생기면 표를 보여준다.
 * Panel 이 Table 보다 뒤인 이유는, 패널이 직접 표를 만들어 줄 수 있다면 그게 그 화면의
 * 문맥에 맞는 표이기 때문이다.
 *
 * 코치마크는 여기 없다 ― 순차 투어는 탭을 5번 만든다. DT_PanelIntro 는 최초 진입
 * 자동 재생 전용으로 되돌렸다 (설계 §2.4).
 */
inline EHelpMode ResolveHelpMode(bool bHasProviderTable, bool bHasLinkWidget, bool bHasBody)
{
	if (bHasProviderTable) { return EHelpMode::Table; }
	if (bHasLinkWidget)    { return EHelpMode::Panel; }
	if (bHasBody)          { return EHelpMode::Prose; }
	return EHelpMode::None;
}

/**
 * 패널 헤더 도움말 "?" ― UIE_HelpButton / UIE_HelpButton_Light 의 부모.
 * 색·웰은 CDO 가 소유하고, 인스턴스가 지정하는 것은 HelpKey 와 크기 두 축
 * (GlyphExtent = 보이는 크기 / TouchExtent = 히트 영역) 이다 ― UCloseButtonWidget 과 같은 문법.
 * 설계 = docs/superpowers/specs/2026-09-23-help-button-design.md
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UHelpButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void SynchronizeProperties() override;

	// DT_Help.HelpKey. 이 키로 답을 못 찾으면 버튼이 스스로 숨는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Help")
	FName HelpKey;

	// 공용 헤더 부품이 패널마다 다른 키를 넘긴다 ― 중첩 위젯은 품은 위젯보다 먼저 Construct 되므로 여기서 다시 푼다
	void SetHelpKey(FName InKey);

	// 탭과 같은 일을 한다. 보조 CTA(UIE_HelpLinkButton)와 검증 하네스가 같은 문으로 들어온다
	void OpenHelp();

	EHelpMode GetHelpMode() const { return Mode; }
	bool IsHelpOpen() const { return Popover != nullptr; }

	// 팝오버는 뷰포트에 붙어 패널 트리 밖에 있다 ― 패널만 렌더해선 위치를 확인할 수 없어
	// 검증 하네스가 좌표를 직접 묻는다. 없으면 false
	bool GetPopoverViewportRect(FVector2D& OutPosition, FVector2D& OutSize, FVector2D& OutViewport) const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 데이터/입력 주입 대상이라 required ― Optional 이면 배선 누락이 silent null 이 된다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> HelpBtn = nullptr;

	// 두 크기 축의 주입 대상 ― 이 부품의 WBP 는 둘 다 이 박스를 가진다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> RootSizeBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> GlyphSize = nullptr;

	// 보이는 "?" 한 변 = 레이아웃 점유 크기. question-mark 캔버스가 96 이라 초과하면 확대되어 흐려진다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Help", meta = (ClampMin = "32", ClampMax = "96"))
	float GlyphExtent = 96.0f;

	// 히트 영역 한 변. 차액은 음수 패딩이 흡수하므로 부모가 보는 크기는 GlyphExtent 그대로다 (Material 3 하한 154)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Help", meta = (ClampMin = "96"))
	float TouchExtent = 154.0f;

private:
	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleDismissClicked();

	void ShowPopover();
	void HidePopover();

	// 소유 패널의 데이터는 패널이 만들어진 뒤에 들어온다(SetTargetBuilding 등).
	// 그래서 구성 시점은 "내보낼지"만 정하고, 무엇을 보여줄지는 탭 때 다시 정한다
	void ResolveMode();

	// 키가 정해진 뒤의 마감 ― 모드 재해석 + 탭 배선 + 가시성
	void ApplyHelpKey();

	void ShowHelpTable();
	void OpenLinkedPanel();

	EHelpMode Mode = EHelpMode::None;

	// 키가 바뀌어 다시 보일 때 되돌릴 WBP 저작 가시성. 접힌 뒤에 읽으면 Collapsed 가 굳는다
	ESlateVisibility AuthoredVisibility = ESlateVisibility::Visible;
	bool bAuthoredVisibilityCaptured = false;

	// 팝오버는 뷰포트에 직접 붙는다 ― 패널 트리 안에 두면 헤더 클리핑에 잘린다
	UPROPERTY(Transient)
	TObjectPtr<UGuideTooltipWidget> Popover = nullptr;

	// 바깥 탭 수신기. 딤은 없다 ― 패널이 계속 보여야 "이 화면의 설명"으로 읽힌다
	UPROPERTY(Transient)
	TObjectPtr<UButton> DismissCatcher = nullptr;

	// Panel 모드 목적지. DT_Help.LinkWidget 을 NativeConstruct 에서 한 번만 읽어 둔다
	EWidgetType LinkWidget = EWidgetType::None;

	// 표를 주는 소유 패널. IHelpTableProvider 구현체이며, 아우터 체인에서 한 번만 찾는다
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> Provider = nullptr;

	// 표 위에 같이 얹는 규칙 한 줄. 규칙 보려고 한 번, 숫자 보려고 또 한 번 누르게 하지 않는다
	FText Caption;

	static constexpr int32 PopoverZOrder = 9200;   // 최초 진입 코치마크 오버레이(9100) 위
	static constexpr float PopoverGap = 18.f;
	static constexpr float PopoverSafeMargin = 24.f;
	// UIE_GuideTooltip 트리 계약 = TailBox 정사각 30×30. 재생기의 TailSize/TailCornerInset 과 같은 값이어야
	// 같은 말풍선이 두 경로에서 똑같이 보인다
	static constexpr float PopoverTailSize = 30.f;
	static constexpr float PopoverTailCornerInset = 20.f;
};
