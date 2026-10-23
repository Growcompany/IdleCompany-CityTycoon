// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Element/Fusion/FusionBuildingLabel.h"

#include "Components/Widget.h"
#include "Engine/World.h"
#include "Entity/Building/BuildingBaseActor.h"
#include "Manager/EntityManager.h"
#include "Manager/TableManagerSubsystem.h"
#include "Table/BuildableCardTable.h"

FText CGFusionLabel::BuildingDisplayName(const UWidget* Context, int32 BuildingIndex)
{
	if (BuildingIndex == INDEX_NONE || !Context)
	{
		return FText::GetEmpty();
	}

	// 카드 테이블 키는 FName BuildingID 라 액터를 경유해야 한다 — 인덱스로는 바로 못 찾는다.
	UWorld* World = Context->GetWorld();
	UEntityManager* EntityMgr = World ? World->GetSubsystem<UEntityManager>() : nullptr;
	ABuildingBaseActor* Building = EntityMgr ? EntityMgr->GetBuildingByIndex(BuildingIndex) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	const UTableManagerSubsystem* Tables = GI ? GI->GetSubsystem<UTableManagerSubsystem>() : nullptr;

	FBuildableCardTable CardData;
	if (Building && Tables && Tables->GetBuildableInfo(Building->GetBuildingID(), CardData))
	{
		return FText::FromName(CardData.Name);
	}
	return FText::Format(NSLOCTEXT("Fusion", "BuildingFallback", "건물 {0}"), FText::AsNumber(BuildingIndex + 1));
}
