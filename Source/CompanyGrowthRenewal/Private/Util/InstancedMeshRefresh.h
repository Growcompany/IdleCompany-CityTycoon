#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"

// GPUScene 를 끈 모바일(UE5.4)에서는 인스턴스를 개별 갱신하면 렌더 쪽 인스턴스 대응이 틀어져 엉뚱한 칸이 움직이거나 사라진다.
// 같은 데이터로 전부 지우고 순서대로 다시 넣으면 대응이 새로 만들어진다 — 인덱스와 변환은 그대로 유지된다.
namespace CGInstancedMesh
{
	inline void RewriteAllInstances(UInstancedStaticMeshComponent* Instances)
	{
		if (!IsValid(Instances) || Instances->GetInstanceCount() == 0)
		{
			return;
		}
		// 인스턴스별 커스텀 데이터는 재추가 직후 다시 쓰는 것 자체가 같은 문제를 부르므로 다루지 않는다.
		if (Instances->NumCustomDataFloats > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[InstancedMeshRefresh] skipped %s: per-instance custom data"), *Instances->GetName());
			return;
		}
		const int32 Count = Instances->GetInstanceCount();
		TArray<FTransform> Transforms;
		Transforms.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FTransform Local;
			Instances->GetInstanceTransform(Index, Local, false);
			Transforms.Add(Local);
		}
		Instances->ClearInstances();
		Instances->AddInstances(Transforms, false, false);
	}
}
