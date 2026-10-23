#pragma once

class AActor;
class UStaticMeshComponent;

namespace BuildingGroundContact
{
	// Base 메시의 실제 로컬 바닥을 기준으로 그림자를 구성한다. 실패하면 컴포넌트를 숨긴다.
	bool Configure(AActor* Owner, UStaticMeshComponent* BaseMesh, UStaticMeshComponent* ContactMesh);

	// BP_MB는 C++ 컴포넌트가 없으므로 런타임에 한 번 생성한다.
	void AddToCityBuilding(AActor* Owner);
}
