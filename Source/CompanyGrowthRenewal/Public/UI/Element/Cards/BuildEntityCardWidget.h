// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Cards/EntityCardWidgetBase.h"
#include "Table/BuildableCardTable.h"
#include "BuildEntityCardWidget.generated.h"

// 카드 클릭 = "선택"만 (배치 시작은 부모 모달이 CTA로 처리). BuildModalWidget 이 수신
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildCardSelected, const FBuildableCardTable&, SelectedInfo);

/**
 * 건물 카드 위젯
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UBuildEntityCardWidget : public UEntityCardWidgetBase
{
	GENERATED_BODY()

public:
	// 카드 선택 이벤트 (BuildModalWidget 구독)
	UPROPERTY(BlueprintAssignable, Category = "Build")
	FOnBuildCardSelected OnCardSelected;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// IUserObjectListEntry 인터페이스 구현 (ListView Entry용)
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	// 카드 클릭 핸들러
	virtual void HandleCardClicked() override;

	// 건설 비용 체크
	void CheckHasConstructionCost();

private:
	// 건물 데이터
	UPROPERTY(BlueprintReadOnly, Category = "Build", meta = (AllowPrivateAccess = "true"))
	FBuildableCardTable BuildableInfo;

	// UIManagerSubsystem 델리게이트 핸들
	FDelegateHandle UIResourceChangedHandle;

	UFUNCTION()
	void HandleResourceChanged(EResourceType Type, int64 NewValue);

	// 에디터 미리보기용
	virtual void NativePreConstruct() override;

public:
	// 에디터 미리보기용 (디자인 타임에만 적용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
	bool bPreviewLocked = false;

	// 건물 정보 설정
	UFUNCTION(BlueprintCallable, Category = "Build")
	void SetBuildableInfo(const FBuildableCardTable& InBuildableInfo);
};
