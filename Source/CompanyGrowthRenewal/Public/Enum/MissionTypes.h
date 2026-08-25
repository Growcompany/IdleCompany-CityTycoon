#pragma once

#include "CoreMinimal.h"
#include "MissionTypes.generated.h"

// 미션 달성 조건 (DT_Mission.ConditionType)
UENUM(BlueprintType)
enum class EMissionConditionType : uint8
{
	None UMETA(DisplayName = "None"),
	// 벽돌 보유량 ConditionAmount 도달 — 오프닝 M1 (Brick Factory 탭 노가다 유도)
	CollectBricks UMETA(DisplayName = "CollectBricks"),
	// 건물 1채 배치 확정 — 오프닝 M2. [건설]→빌드모달(게임 타일)→배치 가이드 페이즈 동반
	BuildFirstBuilding UMETA(DisplayName = "BuildFirstBuilding"),
	// 사무실(OfficeMap) 첫 진입 — 오프닝 M3. 빌딩 스포트라이트→ManagePanel [입장] 가이드, OfficeMap 진입으로 완료
	EnterOffice UMETA(DisplayName = "EnterOffice"),
	// 직원 ConditionAmount 명 채용 — M5. 도달형(빌딩 인원수 절대값)이라 ×N 한번에든 ×1 반복이든 인정된다
	RecruitEmployees UMETA(DisplayName = "RecruitEmployees"),
	// 책상 ConditionAmount 개 배치 — M4. 카운트형, 한 개마다 배치 버튼으로 되돌아가 다음을 유도
	PlaceDesks UMETA(DisplayName = "PlaceDesks"),
	// 첫 그림(벽장식) 배치 — 미션판 G7. [꾸미기]→벽 그림 배치 확정으로 완료
	PlaceFirstPainting UMETA(DisplayName = "PlaceFirstPainting"),
	// 직원 성장 — 미션판 G9. 완료는 강화 "시도"(성공/유지 무관) — 스타포스는 게임에서 여기서만 소개된다.
	// 신호원은 HandleEmployeeEnhanced (SP 투자 시점 아님). 구 M9 는 2026-08-08 체인에서 이관됨.
	InvestFirstStatPoint UMETA(DisplayName = "InvestFirstStatPoint"),
	// 첫 자체개발 출시 — M6. [새 프로젝트] 기획 보드→기획안 선택→[출시] 즉시 완료(운영은 백그라운드)
	LaunchFirstInHouseProject UMETA(DisplayName = "LaunchFirstInHouseProject"),
	// 첫 수익 수집 — M7. 사무실 나가기→MainMap 빌딩 돈 버블 클릭으로 수익 수령 완료 (cross-map)
	CollectFirstRevenue UMETA(DisplayName = "CollectFirstRevenue"),
	// 첫 특성 장착 — 미션판 G10. 신호원은 NotifyTraitEquipped. 구 M11 은 2026-08-08 체인에서 이관됨
	EquipFirstTrait UMETA(DisplayName = "EquipFirstTrait"),
	// 첫 스킨 적용 — 미션판 G8. 관리패널 스킨탭→적용으로 완료
	EquipFirstSkin UMETA(DisplayName = "EquipFirstSkin"),
	// 빌드업(층수) 강화 N회 — 미션판 G1. ConditionAmount=목표 횟수
	RaiseBuildingFloor UMETA(DisplayName = "RaiseBuildingFloor"),
	// 미션판 G2 — 본사 레벨 N 도달 (ConditionAmount = 목표 레벨). 현재 레벨 기준 판정이라 이미 도달했으면 즉시 완료.
	ReachHQLevel UMETA(DisplayName = "본사 레벨 도달"),
	// 미션판 G3 — 보유 빌딩 2채 이상 도달 (M2 는 0->1, 이건 1->2)
	BuildSecondBuilding UMETA(DisplayName = "두 번째 회사 건설"),
	// 미션판 G4 — 도시 입주 회사 1곳 인수(도박형). 완료 = Acquire 성공
	AcquireFirstCompany UMETA(DisplayName = "회사 인수"),
	// 미션판 G5 — 인수한 회사가 고갈된 뒤 수동 철거로 부지 비우기. 완료 = OnCompanyCleared
	DemolishFirstCompany UMETA(DisplayName = "회사 철거"),
	// 미션판 G6 — 비워진 부지를 사서 건설 (보유 빌딩 3채 도달형)
	BuildOnClearedPlot UMETA(DisplayName = "비운 땅에 건설"),
	// 미션판 G11 — 도시 인접 빈 부지 1곳 Money 인수. 완료 = ACityPlotActor 결제 성공
	AcquireFirstPlot UMETA(DisplayName = "부지 인수"),
};
