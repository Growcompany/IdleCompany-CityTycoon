// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Input/CGCommonUIInputData.h"
#include "Engine/DataTable.h"
#include "UObject/ConstructorHelpers.h"

UCGCommonUIInputData::UCGCommonUIInputData()
{
	// 프로젝트 관행대로 생성자에서 직접 로드한다 (CLAUDE.md 「DataTable 추가 시 필수 패턴」).
	static ConstructorHelpers::FObjectFinder<UDataTable> ActionTable(
		TEXT("DataTable'/Game/CompanyGrowth/Table/UI/DT_CommonInputActions.DT_CommonInputActions'"));

	if (!ActionTable.Succeeded())
	{
		// 조용히 비어 있으면 증상이 "ESC 가 안 먹는다" 로만 나타나 원인 추적이 길어진다.
		UE_LOG(LogTemp, Error,
			TEXT("[CGCommonUIInputData] DT_CommonInputActions 를 못 찾았습니다 ― ESC·안드로이드 뒤로가기가 위젯에 도달하지 않습니다."));
		return;
	}

	DefaultClickAction.DataTable = ActionTable.Object;
	DefaultClickAction.RowName = TEXT("DefaultClick");

	DefaultBackAction.DataTable = ActionTable.Object;
	DefaultBackAction.RowName = TEXT("DefaultBack");
}
