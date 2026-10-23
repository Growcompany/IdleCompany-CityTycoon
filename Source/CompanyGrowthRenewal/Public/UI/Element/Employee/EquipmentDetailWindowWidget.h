#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDialogWidget.h"
#include "EquipmentDetailWindowWidget.generated.h"

class UButtonWidget;
class UCloseButtonWidget;
class UCommonTextBlock;
class UImage;
class UPanelWidget;
class UWidget;

/** 상세 창에 그대로 꽂히는 표시용 내용 — 워크스페이스가 이미 계산한 값을 받는다(문자열 재파싱 없음). */
struct FEquipmentDetailContent
{
	FText Name;          // 러닝화 +12
	FText GradeName;     // SSS 등급
	FText PartName;      // 신발
	FText EnhanceText;   // 강화 +12
	FText State;         // 장착 중 / 미장착 / 김무난 착용 중
	bool bHeldByOther = false; // 다른 직원이 낀 장비 ― 상태를 경고색으로, 주 버튼은 「가져와 장착」
	FText BodyLabel;     // 장착 시 변화 / 장비 효과
	FText NoChangeText;  // 변화 행이 하나도 없을 때만 표시
	FText Notice;
	FText ItemNoEffectsText;
	FText ItemUniqueEffects;

	// 교체 스트립 두 칸 — 이 부위에 지금 낀 것(Current) 과 고른 것(After).
	FText CurrentLabel;  // 현재 신발
	FText CurrentName;   // 작업화 +2 (빈 슬롯이면 "빈 슬롯")
	FText AfterLabel;    // 변경 후
	FText AfterName;     // 러닝화 +12

	UTexture2D* Icon = nullptr;
	UTexture2D* CurrentIcon = nullptr;
	FLinearColor GradeColor = FLinearColor::Transparent;
	FLinearColor CurrentGradeColor = FLinearColor::Transparent;
	// 등급 테두리 재질은 행이 있어야 켜진다 — nullptr 을 넘기면 ApplyGradeFrame 이 통째로 접는다.
	const struct FEmployeeEquipmentGradeRow* GradeRow = nullptr;
	const FEmployeeEquipmentGradeRow* VisualGradeRow = nullptr;
	const FEmployeeEquipmentGradeRow* CurrentVisualGradeRow = nullptr;
	int32 Stars = 0;

	TArray<FEquipmentDeltaRow> Rows;
	TArray<FEquipmentUniqueChange> Uniques;
	TArray<FEquipmentDeltaRow> ItemEffectRows;

	FText Primary, Secondary;
	bool bPrimaryEnabled = true;
	bool bSecondaryEnabled = true;
	bool bShowSwap = true;
	bool bShowComparison = false;
};

/** 선택한 장비의 상세 — 드로어 옆 독립 창 또는 직원 화면의 상세 칼럼. */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UEquipmentDetailWindowWidget : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	void Configure(const FEquipmentDetailContent& Content);
	void TriggerSelectedIconPulse();
	// 드로어 폭이 뷰마다 달라서 호스트가 오른쪽 여백을 정한다.
	void SetDockOffset(float RightOffset);
	// 튜토리얼 앵커 — 주 행동 버튼(장착/해제). 라벨 판단은 호스트 몫이라 위젯만 돌려준다
	UWidget* GetPrimaryButtonWidget() const;

	FSimpleDelegate OnPrimary;
	FSimpleDelegate OnSecondary;
	FSimpleDelegate OnClose;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 데이터가 꽂히는 자리는 required — Optional 은 오타 한 글자를 조용한 빈칸으로 바꾼다.
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCloseButtonWidget> DetailCloseButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> SelectedIcon;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> SelectedNameText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> SelectedGradeText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> SelectedPartText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> SelectedEnhanceText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> SelectedStateText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> BodyLabelText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> NoChangeText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> NoticeText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> DeltaRowsBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> UniqueChipBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UPanelWidget> ItemEffectRowsBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> ItemNoEffectsText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> ItemUniqueEffectsText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> ItemEffectsSection;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> ComparisonSection;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> NoticePanel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> DetailPrimaryButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButtonWidget> DetailSecondaryButton;

	// 교체 스트립 두 칸
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> SwapRow;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> CurrentIcon;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> CurrentLabelText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> CurrentNameText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> AfterIcon;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> AfterLabelText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> AfterNameText;

	// 장식·배치 — 없어도 창은 성립한다.
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> SelectedGradeFrame;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> DetailDock;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> StarRow;

private:
	bool bStandalonePresentation = false;
	// 상태 줄의 저작 잉크 ― 다른 직원 장비일 때만 경고색으로 바꿨다가 되돌린다
	FSlateColor StateInkDefault;
	bool bStateInkCaptured = false;
	// 행·칩은 개수가 내용에 따라 변한다 — 매 Configure 마다 다시 찍는다.
	void RebuildRows(const FEquipmentDetailContent& Content);
	void HandlePrimary();
	void HandleSecondary();
	UFUNCTION() void HandleClose();
};
