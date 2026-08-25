// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Office/DecorationTypes.h"
#include "WallSelectionWidget.generated.h"

// 벽 선택 완료 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWallSelected, EWallSide, SelectedWall);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWallSelectionCancelled);

/**
 * 벽 장식 배치 시 왼쪽/오른쪽 벽을 선택하는 UI
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UWallSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// ========== 델리게이트 ==========

	// 벽이 선택되었을 때 (왼쪽 또는 오른쪽)
	UPROPERTY(BlueprintAssignable, Category = "Wall Selection")
	FOnWallSelected OnWallSelected;

	// 선택이 취소되었을 때
	UPROPERTY(BlueprintAssignable, Category = "Wall Selection")
	FOnWallSelectionCancelled OnSelectionCancelled;

	// ========== 버튼 바인딩 ==========

	/**
	 * 왼쪽 벽 버튼 (블루프린트에서 바인딩)
	 */
	UPROPERTY(meta = (BindWidget))
	class UButton* LeftWallButton;

	/**
	 * 오른쪽 벽 버튼 (블루프린트에서 바인딩)
	 */
	UPROPERTY(meta = (BindWidget))
	class UButton* RightWallButton;

	/**
	 * 취소 버튼 (블루프린트에서 바인딩)
	 */
	UPROPERTY(meta = (BindWidget))
	class UButton* CancelButton;

	// ========== 텍스트 바인딩 (선택 사항) ==========

	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* LeftWallText;

	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* RightWallText;

	UPROPERTY(meta = (BindWidgetOptional))
	class UTextBlock* CancelText;

public:
	// ========== 공개 함수 ==========

	/**
	 * 위젯 표시 (화면에 추가)
	 */
	UFUNCTION(BlueprintCallable, Category = "Wall Selection")
	void ShowWidget();

	/**
	 * 위젯 숨김 (화면에서 제거)
	 */
	UFUNCTION(BlueprintCallable, Category = "Wall Selection")
	void HideWidget();

protected:
	// ========== 버튼 클릭 핸들러 ==========

	UFUNCTION()
	void OnLeftWallButtonClicked();

	UFUNCTION()
	void OnRightWallButtonClicked();

	UFUNCTION()
	void OnCancelButtonClicked();
};
