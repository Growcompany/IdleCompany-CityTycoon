#pragma once

#include "CoreMinimal.h"

// 가이드 페이즈 의미 (DT MentorLines 인덱스와 1:1). 매니저 switch 와 GuideGestureRules 가 공유한다.
namespace BuildGuide
{
	constexpr int32 PressBuild = 0;    // [건설] 버튼 잔상
	constexpr int32 PickGameCard = 1;  // 빌드 모달 — '게임' 타일 잔상
	constexpr int32 PlaceBuilding = 2; // 배치 확정 — 배치 바 [배치] 링 타겟 + 프리뷰 위 드래그 손(제스처 채널), 딤 0
}
namespace BrickGuide
{
	// 시작부터 스포트라이트 — 오프닝 2 카메라가 이미 공장 근접이라 버튼 단계 불필요 (2026-06-07 사용자 확정)
	constexpr int32 TapFactory = 0;   // 공장 액터 스포트라이트 (딤+컷아웃+라벨) — 탭해서 벽돌 생산
	constexpr int32 OpenFactory = 1;  // [공장] 버튼 — 공장 패널 열기 유도
	constexpr int32 NudgeUpgrade = 2;  // 첫 강화 슬롯(생산속도) 잔상
	constexpr int32 NudgeUpgrade2 = 3; // 아래(둘째) 강화 슬롯(생산량) 잔상
	constexpr int32 Grind = 4;         // 목표까지 모으기 (가이드 없음, 트래커 진행도)

	constexpr int64 UpgradeNudgeAtBricks = 3; // 이 보유량부터 공장 패널 → 강화 유도
	// 강화 요구 횟수 = 2회 (첫 슬롯 → 아래 슬롯까지 유도, 루프 학습 강화)
}
namespace EnterOfficeGuide
{
	constexpr int32 ClickBuilding = 0; // 지은 회사 빌딩 스포트라이트 — 눌러서 관리 패널 열기 (MainMap)
	constexpr int32 PressEnter = 1;    // ManagePanel [입장] 버튼 하이라이트 (MainMap) → OfficeMap 진입으로 완료
}
// M5 RecruitEmployees — 오피스 [채용] 도크 버튼 → 채용 패널 → ×N 뽑기 → 확인(자동착석) → 닫기.
// 완료는 페이즈가 아니라 빌딩 인원수 도달로 판정한다 — ×1 반복 경로로도 통과해 소프트락이 안 생긴다.
namespace RecruitGuide
{
	constexpr int32 OpenRecruit       = 0;  // 오피스 [채용] 도크 버튼 → 채용 패널 열기
	constexpr int32 PressPull         = 1;  // 채용 패널 [×N 한번에 채용]
	constexpr int32 PressConfirm      = 2;  // 사원증 리빌 [확인] (자동 착석)
	constexpr int32 PressCloseRecruit = 3;  // 채용 패널 [닫기]
}
// M4 PlaceDesks — 카운트형. 책상은 확정 후에도 배치 모드가 유지되는 연속 배치라,
// 한 개 놓았다고 버튼 유도(OpenPlacement)로 되돌리면 이미 열려 있는 버튼을 다시 누르라는 문구가 된다.
// 모드가 살아 있는 동안은 PlaceMore 로 남은 개수를 안내하고, 모드를 나가야 PollGuideProgress 가 버튼 유도로 되돌린다.
namespace DeskGuide  { constexpr int32 OpenPlacement = 0; constexpr int32 Confirm = 1; constexpr int32 PlaceMore = 2; }
// M6 LaunchFirstInHouseProject — [새 프로젝트] 기획 보드 → 기획안 선택 → 개발 스트립 관찰 → [출시](즉시 완료, 운영은 3분 백그라운드)
namespace InHouseGuide
{
	constexpr int32 OpenBoard   = 0;
	constexpr int32 PickInHouse = 1;
	constexpr int32 WatchStrip  = 2;  // 스트립 관찰 유도 (누를 대상 없음 — 시간 기반 이탈)
	constexpr int32 PressLaunch = 3;

	// 관찰 노출 시간. 개발 15초 내내 딤을 깔면 답답하고 부스트 도박 모달과 겹치므로 짧게 끊는다.
	constexpr float StripWatchSeconds = 4.0f;
}
// M7 CollectFirstRevenue — 사무실 [뒤로] → 수입/운영 설명 → 하단 액션바 [수집] (cross-map)
namespace CollectGuide
{
	constexpr int32 ExitOffice   = 0;  // 사무실 [뒤로]
	// 유입(수입 칩)과 운영 진행 게이지를 순서대로 설명한다.
	constexpr int32 Explain      = 1;
	// 하단 액션바 [수집] — 버블을 하나씩 누르는 대신 전 빌딩 수익을 한 번에 거두는 상시 수단을 가르친다.
	constexpr int32 PressCollect = 2;
}
