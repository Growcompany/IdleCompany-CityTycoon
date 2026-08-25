// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Cards/EntityCardWidgetBase.h"
#include "Table/DecorationCardTable.h"
#include "OfficeDecorationCardWidget.generated.h"

class UBorder;
class UImage;
class UCommonTextBlock;
class UOfficeManager;
class AOfficeInterior;

// 장식 카드 선택 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDecorationCardSelected, const FDecorationCardTable&, CardData);

/**
 * 장식품 카드 위젯
 * - 벽 장식, 바닥 장식, 가구, 바닥 타일 공통 사용
 * - 클릭 시 카테고리에 따라 다른 배치 플로우 시작
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeDecorationCardWidget : public UEntityCardWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 카드 클릭 핸들러 (부모의 OnButtonClicked에서 호출됨)
	virtual void HandleCardClicked() override;

	// IUserObjectListEntry 인터페이스 구현 (ListView Entry용)
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

private:
	// 장식품 데이터
	UPROPERTY(BlueprintReadOnly, Category = "Decoration", meta = (AllowPrivateAccess = "true"))
	FDecorationCardTable DecorationInfo;

	// 캐시된 참조 (WorldSubsystem이므로 UPROPERTY 불필요)
	UOfficeManager* CachedOfficeManager = nullptr;

	UPROPERTY()
	AOfficeInterior* CachedOfficeInterior;

	// 참조 찾기
	void CacheReferences();

	// 잠금 조건 체크 (레벨, 프리미엄 등)
	void CheckUnlockCondition();

public:
	// 장식품 정보 설정
	UFUNCTION(BlueprintCallable, Category = "Decoration")
	void SetDecorationInfo(const FDecorationCardTable& InDecorationInfo);

	// 장식품 정보 가져오기
	UFUNCTION(BlueprintPure, Category = "Decoration")
	const FDecorationCardTable& GetDecorationInfo() const { return DecorationInfo; }

	// 카드 선택 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Decoration|Event")
	FOnDecorationCardSelected OnDecorationCardSelected;
};
