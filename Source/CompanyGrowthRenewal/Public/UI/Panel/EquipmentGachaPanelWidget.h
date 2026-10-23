#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "EquipmentGachaPanelWidget.generated.h"

class UEmployeeEquipmentWorkspaceWidget;
class UGachaPanelFrameWidget;
class UOverlay;

/**
 * 장비 뽑기 전용 창 (오피스 도크 [장비], 장비 관리 [뽑기]).
 * 본문은 장비 Workspace 를 뽑기 모드로 끼워 쓰고, 배경·제목·닫기는 가챠 공용 틀이 맡는다.
 */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UEquipmentGachaPanelWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// PushPromptClass 초기화 람다에서 부른다. EmployeeId = 뽑은 뒤 상세·장착으로 이어갈 직원(없으면 INDEX_NONE)
	void Open(int32 EmployeeId = INDEX_NONE);

	UWidget* GetCloseButtonWidget() const;

	// 튜토리얼 M18 — 일반권 1회 [뽑기]. 풀링된 본문이 마지막 권종을 기억하므로 일반권 탭을 먼저 맞춘다
	UWidget* GetPullOneButtonWidget() const;
	class UGachaRevealPresentationWidget* GetActiveReveal() const;
	UWidget* GetNormalTicketTabWidget() const;
	bool IsNormalTicketSelected() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UGachaPanelFrameWidget> GachaFrame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> EquipmentGachaHost;

private:
	friend struct FEquipmentGachaPanelValidation;

	UPROPERTY()
	TObjectPtr<UEmployeeEquipmentWorkspaceWidget> EquipmentWorkspace;

	int32 EquipmentEmployeeId = INDEX_NONE;

	void ConfigureWorkspace();

	UFUNCTION()
	void HandleCloseClicked();
};
