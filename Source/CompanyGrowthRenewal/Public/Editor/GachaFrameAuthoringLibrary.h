#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GachaFrameAuthoringLibrary.generated.h"

class UWidget;
class UUserWidget;

// UI 저작 전용. 위젯 복제, NamedSlot 내용 지정, 검증된 루트 교체를 파이썬에 제공한다.
UCLASS()
class COMPANYGROWTHRENEWAL_API UGachaFrameAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	// Sources 와 하위 전부를 TargetBlueprint 의 저작 트리로 복제한다. 반환은 Sources 순서의 복제 루트이며, 이름이 하나라도 어긋나면 복제본을 트리 밖으로 치우고 빈 배열이다.
	// 루트의 슬롯 배치(패딩·정렬·크기)는 따라오지 않으므로 호출자가 원본 슬롯 값으로 복원한다.
	UFUNCTION(BlueprintCallable, Category = "GachaFrameAuthoring")
	static TArray<UWidget*> CloneWidgetsInto(UObject* TargetBlueprint, const TArray<UWidget*>& Sources);

	UFUNCTION(BlueprintCallable, Category = "GachaFrameAuthoring")
	static bool SetNamedSlotContent(UUserWidget* Host, FName SlotName, UWidget* Content);

	UFUNCTION(BlueprintCallable, Category = "GachaFrameAuthoring")
	static UWidget* GetNamedSlotContent(UUserWidget* Host, FName SlotName);

	// 두 위젯은 같은 저작 트리에 있어야 하며, 교체될 루트가 호출자가 확인한 객체와 일치해야 한다.
	UFUNCTION(BlueprintCallable, Category = "GachaFrameAuthoring")
	static bool ReplaceRootWidget(UObject* TargetBlueprint, UWidget* ExpectedRoot, UWidget* ReplacementRoot);

	// 저작 트리의 표시 상태와 미리보기 크기를 디자이너에 저장한다. 런타임 레이아웃은 바꾸지 않는다.
	UFUNCTION(BlueprintCallable, Category = "GachaFrameAuthoring")
	static bool SetDesignerPreview(UObject* TargetBlueprint, FVector2D PreviewSize);

	UFUNCTION(BlueprintCallable, Category = "GachaFrameAuthoring")
	static void MarkStructurallyModified(UObject* Blueprint);
#endif
};
