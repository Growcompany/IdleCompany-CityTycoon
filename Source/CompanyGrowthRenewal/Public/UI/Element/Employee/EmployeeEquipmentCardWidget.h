#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Enum/EmployeeEquipmentTypes.h"
#include "UI/Element/Employee/EmployeeEquipmentIconVisual.h"
#include "EmployeeEquipmentCardWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UBorder;
class UScaleBox;
class UWidget;
struct FEmployeeEquipmentGradeRow;

/** 목록의 빈 칸 표식. 카드는 이 타입을 아이템으로 캐스팅하지 못해 빈 상태로 그리고,
 *  클릭 핸들러의 캐스팅에서도 걸러져 눌러도 아무 일이 없다.
 *  ⚠ UObject 를 그대로 쓰면 -game 에서 추상 클래스 어서션으로 죽는다(에디터만 통과). */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeEquipmentEmptySlotObject : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class COMPANYGROWTHRENEWAL_API UEmployeeEquipmentItemObject : public UObject
{
	GENERATED_BODY()
public:
	FGuid InstanceID;
	FName DefinitionID;
	EEmployeeEquipmentGrade Grade = EEmployeeEquipmentGrade::Count;
	EEmployeeEquipmentGrade VisualGrade = EEmployeeEquipmentGrade::Count;
	EEmployeeEquipmentIconContext IconContext = EEmployeeEquipmentIconContext::Idle;
	bool bDeferRevealPulse = false;
	FText GradeName;
	FText ChoiceLabel;
	int32 EnhancementLevel = 0;
	int32 SelectionValue = INDEX_NONE;
	bool bLocked = false;
	// 누구든 끼고 있으면 true. 그중 목록이 보는 직원이 아닌 다른 직원이 낀 것이면 bWornByOther ―
	// 카드는 앞은 파란 「장착」, 뒤는 아래 띠 「착용 중」으로 나눠 그린다
	bool bEquipped = false;
	bool bWornByOther = false;
	FLinearColor GradeColor = FLinearColor::White;
	UPROPERTY() TObjectPtr<UTexture2D> Icon;
};

UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UEmployeeEquipmentCardWidget : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()
public:
	void SetItemData(const UEmployeeEquipmentItemObject* Item);
	void TriggerIconPulse();
protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnEntryReleased() override;
	virtual void NativeDestruct() override;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> CardBackground;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> ItemImage;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> GradeFrame;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> SelectionMark;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> LockMark;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> GradeText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> GradeBadge;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> EnhanceText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> ChoiceImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> ChoiceText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> EquippedBadge;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> EquippedText;
	// 다른 직원이 낀 장비 ― 카드 아래를 가로지르는 「착용 중」 띠(문구·색은 WBP 저작)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> WearerBand;
	// 띠 글자는 강화 숫자·자물쇠와 한 줄의 채움 칸에 있다 ― 띠가 없을 땐 접지 않고 숨겨야 자물쇠가 제자리에 남는다
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> WearerText;
	// 강화 숫자를 감싼 배치 배율 상자 ― 그리기 배율(RenderScale)은 자리를 안 줄여 띠 글자가 가운데서 밀린다
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UScaleBox> EnhanceScaleBox;
	// 띠가 보이면 강화 숫자를 이만큼 줄여 띠 안에 맞춘다
	UPROPERTY(EditDefaultsOnly, Category = "Equipment", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float WornEnhanceScale = 0.85f;
private:
	void RefreshIcon(bool bEntrySelected, bool bTriggerPulse);
	void RefreshGradeText(const FEmployeeEquipmentGradeRow* Grade);
	UPROPERTY() TObjectPtr<UTexture2D> CurrentTexture;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> IconMaterial;
	EEmployeeEquipmentGrade CurrentGrade = EEmployeeEquipmentGrade::Common;
	EEmployeeEquipmentIconContext CurrentIconContext = EEmployeeEquipmentIconContext::Idle;
	FGuid CurrentInstanceID;
	FName CurrentDefinitionID;
	bool bHasItemData = false;
	bool bChoiceItem = false;
};
