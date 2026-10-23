#pragma once

#include "CoreMinimal.h"
#include "UI/Element/Buttons/MenuTileWidget.h"
#include "MenuSheetTileWidget.generated.h"

class UImage;
class UMaterialInterface;

/** 메뉴 시트 타일 ― 아이콘 뒤 받침(메달리온) 머티리얼을 묶음별로 인스턴스에서 지정한다. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UMenuSheetTileWidget : public UMenuTileWidget
{
	GENERATED_BODY()

public:
	UMaterialInterface* GetMedallionMaterial() const { return MedallionMaterial; }

protected:
	// 디자이너 프리뷰에도 받침이 보이도록 Construct 가 아니라 PreConstruct 에서 넣는다
	virtual void NativePreConstruct() override;

	UPROPERTY(EditAnywhere, Category = "MenuSheet")
	TObjectPtr<UMaterialInterface> MedallionMaterial = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> MedallionImage = nullptr;

	// 잠금 레이어(불투명 잠금 면)가 아이콘을 덮으므로 그 위에 같은 글리프를 다시 그린다 ― 회색 틴트·크기는 WBP 가 정한다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LockedIconImage = nullptr;
};
