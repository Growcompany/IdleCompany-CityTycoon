// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "EntityCardWidgetBase.generated.h"

class UImage;
class UTexture2D;
class UEntityCardFrameWidget;
class UCardInfoWidget;

/**
 * 엔티티 카드 위젯 베이스 클래스
 * - 빌딩, 장식, 업무공간 등 배치 가능한 엔티티 카드의 공통 부모
 * - 프레임 UI, 아이콘 로드, 잠금 처리 등 공통 로직 제공
 */
UCLASS(Abstract)
class COMPANYGROWTHRENEWAL_API UEntityCardWidgetBase : public UCommonButtonBase, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 촉감 피드백 — 색이 아니라 크기/위치로. 색 변화는 카드가 튀어서 기각된 이력이 있다.
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnPressed() override;
	virtual void NativeOnReleased() override;

	// 타일 재활용 시 촉감 상태 초기화 — 안 하면 눌린 트랜스폼이 다음 항목에 딸려간다.
	virtual void NativeOnEntryReleased() override;

	// 버튼 클릭 시 호출 (내부용)
	void OnButtonClicked();

	// 자식 클래스에서 구현할 클릭 핸들러
	virtual void HandleCardClicked() PURE_VIRTUAL(UEntityCardWidgetBase::HandleCardClicked, );

	// 카드 프레임 (EntityImage, LockBorder 등 포함)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UEntityCardFrameWidget* UIE_EntityCardFrame;

	// 카드 정보 위젯 (ContentSlot에 배치)
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	UCardInfoWidget* CardInfoWidget;

	// 현재 잠금 상태
	bool bIsCurrentlyLocked = false;

	// 현재 선택 상태 (FloorTile 등에서 사용)
	bool bIsSelected = false;

	bool bIsHovered = false;
	bool bIsPressed = false;

	// 호버/눌림 상태를 렌더 트랜스폼으로 반영
	void RefreshTactileTransform();

	// 잠금 UI 업데이트
	void UpdateLockUI(bool bIsLocked, const FText& LockReason = FText::GetEmpty());

public:
	// 아이콘 로드 완료 콜백
	UFUNCTION()
	void OnIconLoaded(FSoftObjectPath LoadedPath);

	// 이미지 위젯 접근
	UImage* GetEntityImage() const;

	// 잠금 상태 확인
	bool IsLocked() const { return bIsCurrentlyLocked; }

	// 선택 상태 설정
	void SetSelected(bool bInSelected);

	// 선택 상태 확인
	bool IsSelected() const { return bIsSelected; }
};
