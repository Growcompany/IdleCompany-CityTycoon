#include "Global/GlobalAssetCache.h"
#include "Enum/PrimitiveShapeType.h"

#include "Materials/MaterialParameterCollection.h"

UGlobalAssetCache::UGlobalAssetCache()
{
	Initialize();
}

void UGlobalAssetCache::Initialize()
{
	// PrimitiveShape 메쉬 로드
	for(int i = 0; i < static_cast<int>(EPrimitiveShapeType::Count); i++)
	{
		PrimitiveShapeMeshs[i] = LoadObject<UStaticMesh>(PrimitiveShapeTypeToPath(static_cast<EPrimitiveShapeType>(i)));
	}

	// Material Parameter Collection 로드
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> CropoutMPCObj(
		TEXT("/Script/Engine.MaterialParameterCollection'/Game/CompanyGrowth/Resources/Materials/MPC_Cropout.MPC_Cropout'")
	);
	if (CropoutMPCObj.Succeeded())
		CropoutMPC = CropoutMPCObj.Object;

	// Placeable Material 로드
	static ConstructorHelpers::FObjectFinder<UMaterial> PlaceableMatObj(
		TEXT("/Script/Engine.Material'/Game/CompanyGrowth/Resources/Materials/M_Placeable.M_Placeable'")
	);
	if (PlaceableMatObj.Succeeded())
		PlaceableMaterial = PlaceableMatObj.Object;

	// FloatingText 블루프린트 클래스 로드
	static ConstructorHelpers::FClassFinder<AActor> FloatingTextBPClass(
		TEXT("/Game/DamageText/Blueprints/BP_DamageText")
	);
	if (FloatingTextBPClass.Succeeded())
		FloatingTextClass = FloatingTextBPClass.Class;
}

void UGlobalAssetCache::SetGameInstance(UCGGameInstance* instance)
{
	GameInstance = instance;
}

UStaticMesh* UGlobalAssetCache::GetPrimitiveShapeMesh(EPrimitiveShapeType type)
{
	return PrimitiveShapeMeshs[static_cast<int>(type)];
}

UMaterialParameterCollection* UGlobalAssetCache::GetCropoutMPC()
{
	return CropoutMPC;
}

UMaterial* UGlobalAssetCache::GetPlaceableMaterial()
{
	return PlaceableMaterial;
}

TSubclassOf<AActor> UGlobalAssetCache::GetFloatingTextClass()
{
	return FloatingTextClass;
}

AActor* UGlobalAssetCache::SpawnFloatingText(UWorld* World, const FVector& Location, const FText& Value, EFloatingTextType TextType)
{
	if (!World || !FloatingTextClass)
	{
		return nullptr;
	}

	// 전역 상한 — 죽은 참조 정리 후 초과 시 오래된 것부터 제거 (1초 드립 월드 텍스트 폭증 방지)
	ActiveFloatingTexts.RemoveAll([](const TWeakObjectPtr<AActor>& A) { return !A.IsValid(); });
	while (ActiveFloatingTexts.Num() >= MaxFloatingTexts)
	{
		TWeakObjectPtr<AActor> Oldest = ActiveFloatingTexts[0];
		ActiveFloatingTexts.RemoveAt(0);
		if (Oldest.IsValid())
		{
			Oldest->Destroy();
		}
	}

	AActor* TextActor = World->SpawnActor<AActor>(FloatingTextClass, Location, FRotator::ZeroRotator);

	if (TextActor)
	{
		// BP의 Init 함수 호출
		UFunction* InitFunc = TextActor->FindFunction(FName("Init"));
		if (InitFunc)
		{
			struct FInitParams
			{
				FText Value;
				uint8 Type;
			};

			FInitParams Params;
			Params.Value = Value;
			Params.Type = static_cast<uint8>(TextType);

			TextActor->ProcessEvent(InitFunc, &Params);
		}

		ActiveFloatingTexts.Add(TextActor);
	}

	return TextActor;
}
