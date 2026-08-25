#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Enum/PrimitiveShapeType.h"
#include "Enum/FloatingTextType.h"
#include "GlobalAssetCache.generated.h"

class UCGGameInstance;

UCLASS()
class COMPANYGROWTHRENEWAL_API UGlobalAssetCache : public UObject
{
	GENERATED_BODY()

public:
	UGlobalAssetCache();

private:
	void Initialize();

	template <typename T>
	T* LoadObject(const FString& path)
	{
		static_assert(TIsDerivedFrom<T, UObject>::IsDerived, "T must be derived from UObject");

		ConstructorHelpers::FObjectFinder<T> loadedObj(*path);
		checkf(loadedObj.Succeeded(), TEXT("Failed to load object: %s"), *path);

		return Cast<T>(loadedObj.Object);
	}

	template <typename T>
	UClass* LoadClass(const FString& path)
	{
		static_assert(TIsDerivedFrom<T, UObject>::IsDerived, "T must be derived from UObject");

		ConstructorHelpers::FClassFinder<T> loadedObj(*path);
		checkf(loadedObj.Succeeded(), TEXT("Failed to load object: %s"), *path);

		UClass* loadedClass = loadedObj.Class;
		checkf(loadedClass != nullptr, TEXT("Failed to load class: %s"), *path);

		return loadedClass;
	}

	UPROPERTY()
	UCGGameInstance* GameInstance;

	UPROPERTY()
	UStaticMesh* PrimitiveShapeMeshs[static_cast<int>(EPrimitiveShapeType::Count)];

	UPROPERTY()
	UMaterialParameterCollection* CropoutMPC = nullptr;

	UPROPERTY()
	UMaterial* PlaceableMaterial = nullptr;

	UPROPERTY()
	TSubclassOf<AActor> FloatingTextClass = nullptr;

	// 활성 플로팅 텍스트 추적 + 전역 상한 (1초 드립 시 월드 액터 폭증 방지)
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> ActiveFloatingTexts;
	static constexpr int32 MaxFloatingTexts = 30;

public:
	void SetGameInstance(UCGGameInstance* instance);

	UStaticMesh* GetPrimitiveShapeMesh(EPrimitiveShapeType type);
	UMaterialParameterCollection* GetCropoutMPC();
	UMaterial* GetPlaceableMaterial();

	TSubclassOf<AActor> GetFloatingTextClass();

	// FloatingText 스폰 헬퍼 함수
	UFUNCTION(BlueprintCallable, Category = "FloatingText")
	AActor* SpawnFloatingText(UWorld* World, const FVector& Location, const FText& Value, EFloatingTextType TextType);
};
