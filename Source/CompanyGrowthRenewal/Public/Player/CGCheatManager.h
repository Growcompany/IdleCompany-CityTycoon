// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "Enum/QualityGrade.h"
#include "CGCheatManager.generated.h"

/**
 * 전역 치트 매니저
 *
 * 모든 맵에서 콘솔 명령어로 디버깅 가능
 * Shipping 빌드에서 자동 비활성화
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API UCGCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	virtual void InitCheatManager() override;

	UFUNCTION(Exec)
	void CheatPing();

	// ========== 경험치 관련 ==========

	// 선택된 직원에게 경험치 추가
	// 사용: Exp [Amount] 또는 Exp [Amount] [EmployeeID]
	UFUNCTION(Exec)
	void Exp(int32 Amount, int32 EmployeeID = -1);

	// 현재 빌딩 직원들에게 경험치 분배
	// 사용: DistExp [Amount] [QualityGrade(0=F ~ 5=S)]
	UFUNCTION(Exec)
	void DistExp(int32 Amount, int32 QualityGradeValue = 3);

	// 직원 경험치/레벨 상태 표시
	UFUNCTION(Exec)
	void ExpStatus();

	// ========== 자원 관련 ==========

	// 돈 추가
	// 사용: Money [Amount]
	UFUNCTION(Exec)
	void Money(int64 Amount);

	// 모든 자원 표시
	UFUNCTION(Exec)
	void ShowResources();

	// 테스트 자원 시나리오 일괄 적용 (DT_TestResourceScenarios 의 RowName)
	// 사용: GrantRes Rich  /  GrantRes Default  /  GrantRes Poor  /  GrantRes Empty
	UFUNCTION(Exec)
	void GrantRes(FString ScenarioRowName);

	// ========== 직원 관련 ==========

	// 직원 목록 표시
	UFUNCTION(Exec)
	void ListEmp();

	// 선택된 직원 강화 레벨 설정
	// 사용: SetRank [EnhancementLevel(0-12)]
	UFUNCTION(Exec)
	void SetRank(int32 EnhancementLevel);

	// 선택 직원의 지정 스탯을 직접 설정 (검증용 — ★ 반복 없이 단일 스탯 격리 확인)
	// 사용: SetStat [StatIndex 0~5] [Value] 또는 SetStat [Index] [Value] [EmployeeID]
	UFUNCTION(Exec)
	void SetStat(int32 StatIndex, int32 Value, int32 EmployeeID = -1);

	// 선택 직원의 6스탯 저장값/강화보너스/유효값 출력
	// 사용: StatStatus 또는 StatStatus [EmployeeID]
	UFUNCTION(Exec)
	void StatStatus(int32 EmployeeID = -1);

	// 이름 생성기만 N번 돌려 로그 출력 (직원 생성/세이브 없음)
	// 사용: EmpName [RarityValue 0=Common..5=Mythic] [Count]
	UFUNCTION(Exec)
	void EmpName(int32 RarityValue = 0, int32 Count = 20);

	// [OfficeMap] 직원 크리티컬 확률을 강제 설정(트레일러용). Percent=퍼센트(기본 50). Percent<0(-1) 이면 해제(정상 복귀).
	// 사용: SetCrit  /  SetCrit 50  /  SetCrit -1
	UFUNCTION(Exec) void SetCrit(float Percent = 50.f);

	// 데미지 숫자(직원 작업 점수) 시각 검수 — 게임플레이 진행 없이 여러 자릿수·직능을 한 화면에 띄운다.
	// 맵 무관(뷰포트에 직접 붙인다). bFreeze=1 이면 `cg.DamageNumber.Freeze` 를 켜 멈춘 채로 남긴다.
	// 사용: DamageNumberDemo  /  DamageNumberDemo 0  (해제는 DamageNumberDemoClear)
	UFUNCTION(Exec) void DamageNumberDemo(int32 bFreeze = 1);

	// 위 데모가 띄운 컨테이너를 걷고 Freeze 를 해제한다.
	UFUNCTION(Exec) void DamageNumberDemoClear();

	// ========== 스테이지 관련 ==========

	// 현재 Step 즉시 완료
	UFUNCTION(Exec)
	void CompleteStep();

	// 현재 스테이지 상태 표시
	UFUNCTION(Exec)
	void StageStatus();

	// 활성 프로젝트 직능 점수를 목표의 Percent% 로 일괄 세팅 (30=출시차단 재현 / 95=Best 컨텍스트 리뷰)
	UFUNCTION(Exec)
	void Stage_FillScores(int32 Percent = 100);

	// ========== 운영 관련 ==========

	// 현재 건물 운영 남은 시간 설정 (초)
	// 사용: SetOpTime [Seconds] (0 = 다음 틱에 즉시 완료)
	UFUNCTION(Exec)
	void SetOpTime(float Seconds);

	// 현재 건물 운영 상태 표시
	UFUNCTION(Exec)
	void OpStatus();

	// 모든 일반 건물 금고를 건물당 AmountPerBuilding(원, 기본 500만) 절대액으로 즉시 채워 돈 버블을 전부 띄운다
	// (트레일러 "방치 수익" 컷). 채우기만 하고 수거는 안 함 — 이후 "전체수거" 버튼으로 대량 코인 연출 트리거.
	// 사용: RevenueMax  /  RevenueMax 10000000
	UFUNCTION(Exec)
	void RevenueMax(int64 AmountPerBuilding = 5000000);

	// ========== 아이템 관련 ==========

	// 채용권 지급
	// 사용: Ticket [Amount] [Tier: 0=Normal, 1=Advanced, 2=Premium] (기본: 10장, Normal)
	UFUNCTION(Exec)
	void Ticket(int32 Amount = 10, int32 TierValue = 0);

	// 건물 특성 뽑기권 지급 (가챠 풀 플로우 테스트용 — SeedTraits 는 가챠 우회 직접 지급)
	// 사용: TraitTicket [Amount] [Tier: 0=Normal, 1=Advanced, -1=둘 다] (기본: 30장, 둘 다)
	UFUNCTION(Exec)
	void TraitTicket(int32 Amount = 30, int32 TierValue = -1);

	// 건물 스킨 뽑기권 지급 (스킨 가챠 풀 플로우 테스트용)
	// 사용: SkinTicket [Amount] [Tier: 0=Normal, 1=Advanced, -1=둘 다] (기본: 30장, 둘 다)
	UFUNCTION(Exec)
	void SkinTicket(int32 Amount = 30, int32 TierValue = -1);

	// 잠재 명함 지급 (직원창 잠재 리롤 테스트용 — 상점 구매를 우회한다)
	// 사용: Card [Amount] [Tier: 0=종이, 1=골드, 2=블랙, -1=전부] (기본: 20장, 전부)
	UFUNCTION(Exec)
	void Card(int32 Amount = 20, int32 TierValue = -1);

	// 출시 전리품 드랍 시뮬레이션 (지급 없음, 분포 로그만)
	// 사용: LootRoll [TableKey: Low/Mid/High] [Tier] [Count] (기본: High, 10, 10)
	UFUNCTION(Exec)
	void LootRoll(FString TableKey = TEXT("High"), int32 Tier = 10, int32 Count = 10);

	// ========== 건물 관련 ==========

	// 빌딩 티어는 Proj_SetTier 로 조정한다 (구 SetBldLv 는 빌딩 레벨 폐지와 함께 제거)

	// 스킨 랜덤 해금
	// 사용: UnlockSkins [Count] (기본 5개, 미보유 중 랜덤 선택)
	UFUNCTION(Exec)
	void UnlockSkins(int32 Count = 5);

	// 스킨 100인 완성 건물에 종류별 기본 스킨을 적용하고 저장한다.
	UFUNCTION(Exec)
	void BldDefaultSkins();

	// 모든 건물 창문 조명을 운영 여부와 무관하게 항상 켠다(bOn=1, 기본) / 원래 정책(운영 중만 점등)으로 되돌린다(bOn=0).
	// 사용: BldLightsOn  /  BldLightsOn 0
	UFUNCTION(Exec) void BldLightsOn(int32 bOn = 1);

	// [디버그] 오프라인 정산 모달을 즉시 띄운다(60초 대기/앱 라이프사이클 흉내 없이). 실제 표시 경로 그대로 태움.
	// 기본 = 실제 지은 건물만 내역 생성(금고도 채워 [모두 수령] 검증). OfflineHours>=12 면 12h 캡 변주,
	// bZeroGain=1 이면 적립 0(금고 만석) 변주. FakeRows>0 이면 실제 건물 + 그 개수까지 가짜 행 패딩
	// (레이아웃/스크롤 테스트용 — 존재 안 하는 인덱스라 "건물 N" 폴백).
	// 사용: ShowOfflineReport  /  ShowOfflineReport 14  /  ShowOfflineReport 3 1  /  ShowOfflineReport 3 0 8
	UFUNCTION(Exec) void ShowOfflineReport(int32 OfflineHours = 3, int32 bZeroGain = 0, int32 FakeRows = 0);

	// ========== 사이드 컨텐츠 (기술 연구소) — 시각 검수용 ==========

	/** 인덱스 음수면 세이브의 첫 보유 건물 — 사이드 컨텐츠 치트 3개가 공유한다. */
	static int32 ResolveSideBuilding(int32 BuildingIndex, class USaveLoadManager* SaveMgr);

	// [디버그] 그 건물의 기술 연구소를 띄운다. 인덱스를 안 주면 세이브의 첫 보유 건물.
	// 상태(연구·입장권) 는 세이브가 정한다 — 상태를 바꾸려면 Challenge/SweepTraining 을
	// 원격 파이썬으로 부를 것(둘 다 BlueprintCallable 이라 치트가 따로 필요 없다).
	// 사용: SideOpenTraining  /  SideOpenTraining 2
	UFUNCTION(Exec) void SideOpenTraining(int32 BuildingIndex = -1);

	// [디버그] 그 건물의 출장 창을 띄운다. 인덱스를 안 주면 세이브의 첫 보유 건물.
	// 카드 3장은 자정마다 뽑히고 슬롯 상태는 세이브가 정한다 — 진행/수령 화면을 보려면
	// Depart/ClaimDispatch 를 원격 파이썬으로 부를 것(둘 다 BlueprintCallable).
	// 사용: SideOpenTrip  /  SideOpenTrip 2
	UFUNCTION(Exec) void SideOpenTrip(int32 BuildingIndex = -1);

	// [디버그] 열려 있는 출장 창에서 카드를 고른다. 선발 좌석·판정 밴드는 카드가 있어야 나오는데
	// 클릭 말고는 그 상태로 갈 길이 없어 시각 검수가 막혔다(09-26). 카드의 공개 델리게이트를
	// 그대로 쏘므로 실제 클릭과 같은 경로다.
	// 사용: SideSelectTripCard  /  SideSelectTripCard 2
	UFUNCTION(Exec) void SideSelectTripCard(int32 CardSlot = 0);

	// [디버그] 그 건물로 직원을 옮겨 팀 파워를 올린다(다른 건물 소속에서 뺀다). 충족(그린) 화면을 만들 때.
	// 사용: SideFillCrew  /  SideFillCrew 2 5
	UFUNCTION(Exec) void SideFillCrew(int32 BuildingIndex = -1, int32 Count = 3);

	// [디버그] 연구·입장권을 직접 세운다(음수 = 그대로). 최종 연구 화면은 정상 플레이로 못 만든다.
	// 사용: SideSetTraining -1 50 3   (첫 건물을 최종 연구 + 입장권 3)
	UFUNCTION(Exec) void SideSetTraining(int32 BuildingIndex = -1, int32 CourseBest = -1, int32 Entries = -1);

	// [디버그] 스카이라인 최고 층·입장권을 직접 세운다(음수 = 그대로). 100층·보스 직전 화면은 정상 플레이로 못 만든다.
	// 최고 층을 올리면 그 아래 층 보상은 "이미 받음" 으로 처리된다 — 치트로 올렸다 내려도 두 번 받지 않는다.
	// 사용: SideSetSkyline 39 2   (최고 39층 + 입장권 2 = 40층 보스 앞)
	UFUNCTION(Exec) void SideSetSkyline(int32 BestFloor = -1, int32 Entries = -1);

	// [디버그] 스카이라인 전용 화면을 띄운다. 메뉴 시트를 거치지 않고 바로 로비로 — 시각 검수용.
	// 사무실 씬·결과 카드는 화면 안에서 [개발하기] 로 간다(상태는 SideSetSkyline 으로 먼저 만들 것).
	// 사용: SideOpenSkyline
	UFUNCTION(Exec) void SideOpenSkyline();

	// 콜라보 개발 창 표시. 메뉴 타일과 같은 PromptStack 경로를 사용한다.
	UFUNCTION(Exec) void SideOpenFusion();

	// Level/RemainingSeconds 음수=유지, 0초=수령 상태, FusionKey=None=진행 슬롯만 비움.
	// 사용: SideSetFusion game_platform 4 0
	UFUNCTION(Exec) void SideSetFusion(FName FusionKey, int32 Level = -1, int32 RemainingSeconds = -1);

	// ========== 벽돌공장 (ABrickFactory) — 트레일러 캡처용 ==========

	// 현재 맵의 모든 벽돌공장 강화를 만렙으로. 개수(HoldProductionAmount)+속도만 —
	// 자동수집은 손대지 않음(0.001초 틱의 "보관함 가득" 알림이 트레일러 컷을 망치므로). 공장 홀드 시 벽돌 우수수.
	// SpeedSeconds>0 이면 강화 곡선 대신 그 초를 버스트 간격으로 오버라이드 → 재빌드 없이 PIE에서 라이브 튜닝.
	// 사용: FactoryMax                (개수 만렙 + 속도=곡선 바닥 0.1초)
	//       FactoryMax 50             (개수만 50레벨로 낮춰 "+N"/카운터 폭주 억제)
	//       FactoryMax -1 0.3         (개수 만렙 유지, 버스트 간격만 0.3초로)
	//       FactoryMax 30 0.07        (개수 30, 간격 0.07초)
	UFUNCTION(Exec)
	void FactoryMax(int32 AmountLevel = -1, float SpeedSeconds = -1.0f);

	// 현재 맵의 모든 벽돌공장 자동 수집을 즉시 해금(중견기업 승격을 기다리지 않고 검증).
	// 자동/용량 강화 슬롯 잠금 해제 + 재고 슬롯 활성 + 해금 연출 pending. 이미 해금된 공장은 무시.
	// 사용: UnlockFactoryAuto
	UFUNCTION(Exec)
	void UnlockFactoryAuto();

	// ========== 본사 레벨 관련 ==========

	// 본사 레벨업 (조건 무시하고 강제 레벨업)
	// 사용: HQLevelUp
	UFUNCTION(Exec)
	void HQLevelUp();

	// 본사 레벨 직접 설정
	// 사용: SetHQLv [Level]
	UFUNCTION(Exec)
	void SetHQLv(int32 Level);

	// ========== 채광 (UMineManager) ==========

	// 모든 채광 가능 국가의 라인 상태/강화 레벨/저장 한도 출력
	// 사용: MineDebug
	UFUNCTION(Exec)
	void MineDebug();

	// 지정 국가의 모든 채광 라인을 한도까지 채움 (수령/saturated UI 테스트용)
	// 사용: MineFill Australia
	UFUNCTION(Exec)
	void MineFill(const FString& CountryName);

	// 지정 국가의 채광 강화 레벨 직접 설정
	// 사용: MineSetLv Australia Rate 5      (Rate / Storage)
	UFUNCTION(Exec)
	void MineSetLv(const FString& CountryName, const FString& UpgradeName, int32 Level);

	// ========== 생산 주문서 / 공장 라인 (UProductionOrderManager · UWorldFactoryManager) ==========

	// 지정 국가에서 제작 가능한 주문서 전량 + 각 주문의 제작가능수/병목 재료 + 라인 현황 출력.
	// 국가의 SupportedIndustries 로 걸러진 목록이라 "이 나라에서 뭘 만들 수 있나"를 그대로 보여준다.
	// 사용: Prod_Country Korea            (Korea / China / Japan / Germany / USA)
	UFUNCTION(Exec)
	void Prod_Country(const FString& CountryName);

	// 국가 필터 없이 살아있는 주문서 전량 덤프 (ID/산업/제품/등급/남은수량/제작가능수)
	// 사용: Prod_Orders
	UFUNCTION(Exec)
	void Prod_Orders();

	// DT_TestProductionOrders 로 테스트 주문서 생성 (-1 = 전체 행)
	// 사용: Prod_Seed          (전체)
	//       Prod_Seed 5        (무작위 5건)
	UFUNCTION(Exec)
	void Prod_Seed(int32 Count = -1);

	// 원자재 10종을 일괄 지급 (제작 가능 상태 만들기)
	// 사용: Prod_MatFill              (각 100,000)
	//       Prod_MatFill 500          (각 500 — 부족 상태 만들기)
	UFUNCTION(Exec)
	void Prod_MatFill(int64 Amount = 100000);

	// 특정 원자재를 정확히 그 수량으로 맞춤 (상한/병목 재료 테스트)
	// 사용: Prod_MatSet Lithium 62     (IronOre/Copper/Silicon/Lithium/Oil/RareEarth/Aluminum/Wood/Gold/DiamondOre)
	UFUNCTION(Exec)
	void Prod_MatSet(const FString& ResourceName, int64 Amount);

	// 지정 국가의 공장 라인 현황 (Active/Max + 각 라인 제품/진행/목표)
	// 사용: Fac_Lines Korea
	UFUNCTION(Exec)
	void Fac_Lines(const FString& CountryName);

	// 지정 국가의 라인이 꽉 찰 때까지 더미 생산 시작 ("라인 가득" UI 테스트)
	// 사용: Fac_FillLines Korea
	UFUNCTION(Exec)
	void Fac_FillLines(const FString& CountryName);

	// 지정 국가의 활성 라인을 전부 즉시 완료 후 수령해 비운다 (Fac_FillLines 되돌리기).
	// 더미 라인은 완주에 수일이 걸려 두지 않으면 그 국가를 다시 테스트할 수 없다.
	// 사용: Fac_ClearLines Korea
	UFUNCTION(Exec)
	void Fac_ClearLines(const FString& CountryName);

	// ========== 월드맵 판매 인벤 (UWorldMapManager) ==========

	// 판매 모달 테스트용 랜덤 인벤 시드 (산업 1~6 x 프로젝트 1~5, 등급 F~S 랜덤)
	// 사용: SeedProducts                  (24슬롯, 100~8000개)
	//       SeedProducts 50               (50슬롯, 기본 수량)
	//       SeedProducts 30 500 20000     (30슬롯, 500~20000개)
	UFUNCTION(Exec)
	void SeedProducts(int32 NumSlots = 24, int64 MinQty = 100, int64 MaxQty = 8000);

	// ========== 건물 특성 (UBuildingTraitManagerSubsystem) ==========

	// 특성 인벤 랜덤 시드 (DT_BuildingTrait 의 모든 Row 중 랜덤 N개에 1~MaxQty 부여)
	// 사용: SeedTraits                   (30종, 1~5장)
	//       SeedTraits 60                (60종, 기본 수량)
	//       SeedTraits 105 1 10          (모든 특성, 1~10장)
	UFUNCTION(Exec)
	void SeedTraits(int32 NumKinds = 30, int32 MinQty = 1, int32 MaxQty = 5);

	// 특성 보유 현황 출력
	UFUNCTION(Exec)
	void TraitStatus();

	// 특성 효과 집계 덤프 — 대상 20종의 현재 적용값. 효과축 v2 배선 검증용.
	// 사용: TraitEffects        (현재 관리 건물)
	//       TraitEffects 3      (해당 건물 인덱스)
	UFUNCTION(Exec)
	void TraitEffects(int32 BuildingIndex = -1);

	// ========== 사무실 스타터 프리셋 (OfficeMap 전용) ==========

	// 스타터 프리셋 강제 적용 (OfficeMap 전용). 예: ApplyStarterPreset Mythic_0
	UFUNCTION(Exec)
	void ApplyStarterPreset(const FString& PresetRowName);

	// 현재 사무실의 배치 데코를 프리셋 CSV 엔트리 형식(상대 좌표)으로 로그+파일 출력 (OfficeMap 전용)
	UFUNCTION(Exec)
	void ExportStarterPreset();

	// ========== 개발자 모드 ==========

	// 튜토리얼을 그 자리에서 끝내고 자원(Rich)+HQ20 을 얹어 시총 게이트(국가/티어/HQ)를 통째로 연다.
	// ★ 이 세션의 세이브가 봉인된다 — 껐다 켜면 치트도 그 뒤의 진행도 전부 사라진다(해제 불가, 앱 재시작이 해제).
	// 사용: DevMode
	UFUNCTION(Exec)
	void DevMode();

	// ========== 튜토리얼 (미션 점프) ==========

	// 임의 미션으로 즉시 점프 (인세션, 선행상태 시드 포함 — 자원 듬뿍 + 빌딩 체크).
	// 사용: SkipToMission M3_EnterOffice  /  SkipToMission M2_BuildFirstCompany
	// (드롭다운으로 미리 지정하려면 Project Settings > Game > CG Dev > 시작 미션)
	UFUNCTION(Exec)
	void SkipToMission(const FString& MissionRowName);

	// 튜토리얼 초기화 — 세이브 슬롯 삭제. Stop 후 다시 Play 하면 깨끗한 M1부터.
	// (SkipToMission 누적/체인완주로 CurrentMissionID=None 된 세이브 복구용)
	UFUNCTION(Exec)
	void ResetTutorial();

	// ========== 미션판 (튜토리얼 체인 종료 후) ==========

	// 미션판 즉시 언락 + 전수 재평가. 체인 완주 없이 보드 테스트용
	// 사용: UnlockGoalBoard
	UFUNCTION(Exec)
	void UnlockGoalBoard();

	// 미션 강제 충족(수령은 보드에서). 사용: CompleteGoal G4_AcquireCompany
	UFUNCTION(Exec)
	void CompleteGoal(const FString& GoalRowName);

	// 보드 상태 초기화 (언락/충족/수령 전부 리셋)
	// 사용: ResetGoals
	UFUNCTION(Exec)
	void ResetGoals();

	// 패널 최초 진입 안내(딤+구멍+말풍선)를 전부 안 본 상태로 되돌린다. 직원창/미션 목록 안내 재확인용
	// 사용: ResetPanelIntro
	UFUNCTION(Exec)
	void ResetPanelIntro();

	// ========== 라이브옵스 (공용 시계 / 퀘스트) ==========

	// 서버 시각 디버그 오프셋(초, 누적). 하루 넘김 재현: ClockOffset 86400
	UFUNCTION(Exec)
	void ClockOffset(int64 Seconds);

	// 시계 상태 로그 (동기 여부 / 서버 now / 하루·주 키 / 남은 초)
	// 사용: ClockStatus
	UFUNCTION(Exec)
	void ClockStatus();

	// 퀘스트 카운터 강제 보고. 사용: QuestReport RevenueCollect 5 / QuestReport ProjectLaunch
	UFUNCTION(Exec)
	void QuestReport(const FString& CounterName, int64 Amount = 1);

	// 퀘스트 원장 초기화(일일/주간/업적 전부) + 저장. 사용: QuestReset
	UFUNCTION(Exec)
	void QuestReset();

	// 일일/주간/업적 현재 상태 로그. 사용: QuestStatus
	UFUNCTION(Exec)
	void QuestStatus();

	// 오늘 출석 보상 수령 시도 (서버 시간 유효 + 오늘 미수령일 때만). 사용: AttendClaim
	UFUNCTION(Exec)
	void AttendClaim();

	// 출석 이력(순환 일차/마지막 수령 키/누계) 초기화 + 저장. 사용: AttendReset
	UFUNCTION(Exec)
	void AttendReset();

	// 출석 현황 로그 (수령 가능 여부 / 다음 일차 / 현재 일차 / 누계). 사용: AttendStatus
	UFUNCTION(Exec)
	void AttendStatus();

	// 테스트 우편 1통 발송 (다이아 N + 일반 채용권 5, 7일 만료). 사용: MailSend 100
	UFUNCTION(Exec)
	void MailSend(int32 DiamondAmount = 100);

	// 우편함 비우기(수신 이력 포함) + 저장. 사용: MailClear
	UFUNCTION(Exec)
	void MailClear();

	// 우편함 현황 로그 (통수 + 각 메일 제목/만료 시각). 사용: MailStatus
	UFUNCTION(Exec)
	void MailStatus();

	// 라이브옵스 콘텐츠(TitleData) 강제 재요청 (10분 스로틀 무시). 사용: LiveOpsRefresh
	UFUNCTION(Exec)
	void LiveOpsRefresh();

	// 활성 수집 이벤트 점수 가산. 사용: EventPoints EV_LaunchRush 5
	UFUNCTION(Exec)
	void EventPoints(const FString& EventId, int64 Amount = 1);

	// 활성 이벤트 현황 로그 (유형/진행/마일스톤 + 핫타임 배율 + 공지 미읽음 수). 사용: EventStatus
	UFUNCTION(Exec)
	void EventStatus();

	// ========== 시간대 / 낮밤 (ATimeCycleManager) ==========

	// 낮으로 고정 (12:00 세팅 + 사이클 정지 → 창문/차량 라이트 OFF)
	// 사용: Day
	UFUNCTION(Exec)
	void Day();

	// 밤으로 고정 (00:00 세팅 + 사이클 정지 → 창문/차량 라이트 ON)
	// 사용: Night
	UFUNCTION(Exec)
	void Night();

	// 석양(노을 정점)으로 고정 — 시각은 배치된 하늘의 황혼 노브에서 파생되므로 맵마다 다르다
	// 사용: Dusk
	UFUNCTION(Exec)
	void Dusk();

	// 여명(노을 정점)으로 고정 — Dusk 의 반대쪽
	// 사용: Dawn
	UFUNCTION(Exec)
	void Dawn();

	// 임의 시각으로 고정 (시 0~23, 분 0~59 + 사이클 정지)
	// 사용: SetTime 18  /  SetTime 6 30
	UFUNCTION(Exec)
	void SetTime(int32 Hour, int32 Minute = 0);

	// 시간 사이클 재개 (고정 해제 → 다시 시간이 흐름)
	// 사용: TimeResume
	UFUNCTION(Exec)
	void TimeResume();

	// ========== 도시 회사 인수 (UCityAcquisitionManager) ==========

	// 지정 Key(BuildingKey)의 회사를 인수 시도 (자금 차감 + Milking 상태 전환)
	// 사용: Acq_Buy [Key]
	UFUNCTION(Exec) void Acq_Buy(int32 Key);

	// 지정 Key의 인수 상태 출력 (0=NotAcquired 1=Milking 2=Depleted 3=Cleared)
	// 사용: Acq_State [Key]
	UFUNCTION(Exec) void Acq_State(int32 Key);
	// 지정 Key의 회사 철거 = 캐시아웃 (Milking/Depleted 에서 동작 -> Cleared)
	// 사용: Acq_Demolish [Key]
	UFUNCTION(Exec) void Acq_Demolish(int32 Key);

	// 모든 도시 회사를 즉시 인수 완료(Cleared)로 — 건물 사라지고 부지 구매 가능해짐 (자금/상태 무시).
	// 땅까지 접수하려면 이어서 Plot_OwnAll. 사용: Acq_ClearAll
	UFUNCTION(Exec) void Acq_ClearAll();

	// Milking 회사의 회수율을 Pct% 까지 즉시 진행 (금고만 참, 지갑 무변동. 기본 90)
	// 사용: Acq_FillPot [Key] [Pct]
	UFUNCTION(Exec) void Acq_FillPot(int32 Key, int32 Pct = 90);

	// ========== 도시 부지 (USpawnManager / ACityPlotActor) ==========

	// 스폰된 모든 도시 부지를 즉시 소유 상태로(인접/자금/결제 게이트 우회) + 저장.
	// 사용: Plot_OwnAll
	UFUNCTION(Exec) void Plot_OwnAll();

	// ========== 백로그 수학 검증 ==========

	// 백로그 순익 지수감쇠 수학 자가검증 (peak100 cost10 반감기600 4샘플 로그)
	// 사용: CG_TestBacklogMath
	UFUNCTION(Exec)
	void CG_TestBacklogMath();

	// 현재 관리 빌딩의 백로그 제품 목록 + 순익/빌딩 합산 출력 (백로그 자동화 검증용)
	// 사용: CG_DumpBacklog
	UFUNCTION(Exec)
	void CG_DumpBacklog();

	// ========== UI 디버그 (UIBase 스택 / 입력모드) ==========

	// UIBase 3스택(Main/Prompt/Bottom) 내용 + 현재 EInputMode 출력 (입력잠금/유령위젯 진단)
	// 사용: CG_DumpUI
	UFUNCTION(Exec)
	void CG_DumpUI();

	// 입력모드가 UI로 잠겼을 때 강제 Normal 복원 (유령 위젯 탈출용 — 위젯은 건드리지 않음)
	// 사용: CG_UnlockInput
	UFUNCTION(Exec)
	void CG_UnlockInput();

	// ========== 렌더 비용 귀속 (실기기 GPU 격리) ==========

	// 이름 부분일치(스태틱메시명·액터명·클래스명)로 프리미티브 컴포넌트를 숨긴다 — 쇼플래그로 못 가르는 개별 메시(하늘돔·바다판·지형·스플라인 도로)의 비용 귀속용
	// 사용: CG_HideMesh TimeCycleSky  /  CG_HideMesh Road_Double  /  CG_HideMesh IslandTerrain
	UFUNCTION(Exec)
	void CG_HideMesh(const FString& NameSubstring);

	// CG_HideMesh 로 숨긴 것을 같은 부분일치로 다시 표시
	// 사용: CG_ShowMesh TimeCycleSky
	UFUNCTION(Exec)
	void CG_ShowMesh(const FString& NameSubstring);

	// ========== 레벨 전환 오버레이 (SLevelTransitionOverlay) ==========

	// 레벨 이동 없이 전환 홀드 화면(+하단 팁 밴드)을 N초간 띄운다 — 전환은 순간이라 눈으로 볼 방법이 없다
	// 사용: CG_PreviewTransition  /  CG_PreviewTransition 10  /  CG_PreviewTransition 10 P_Desk
	UFUNCTION(Exec)
	void CG_PreviewTransition(float Seconds = 6.0f, FString TipKey = TEXT(""));

	// 현재 도시에서 첫 튜토리얼 오프닝만 재생한다. 시청 기록과 세이브는 변경하지 않는다.
	// 사용: CG_PreviewCityOpening
	UFUNCTION(Exec)
	void CG_PreviewCityOpening();

	// 로드된 DT_LoadingTip 행 전체를 출력 (키/묶음/컨텍스트/가중치/글자수) — 임포트 확인 + 미리보기 키 찾기
	// 사용: CG_ListTips
	UFUNCTION(Exec)
	void CG_ListTips();

	// ========== 카메라 각도 (OfficeMap 전용) ==========

	// 오피스 카메라 각도 프리셋 즉시 적용 (FOV 40/30 고정, 피치만 변화)
	// 사용: CamPreset 2  (0=코지 1=밸런스 2=탑다운 3=하드탑다운)
	UFUNCTION(Exec)
	void CamPreset(int32 Index = 1);

	// 오피스 카메라 밴드 수동 지정 (줌인/줌아웃 피치 + FOV)
	// 사용: CamBand -44 -58 40 30
	UFUNCTION(Exec)
	void CamBand(float InPitch, float OutPitch, float InFOV = 40.f, float OutFOV = 30.f);

	// 현재 오피스 카메라 포즈와 floor 기준 참조 프레임을 한 줄 로그로 출력 (상태 변경 없음)
	// 사용: CamPose
	UFUNCTION(Exec)
	void CamPose();

	// ========== 카메라 측정 (맵 무관) ==========

	// 현재 카메라가 **실제로 보는 범위**를 로그로 출력 (상태 변경 없음).
	// 줌을 손으로 움직이며 찍어 Min/MaxZoomDistance 를 정할 때 쓴다 —
	// 팔길이는 카메라 거리가 아니고(SocketOffset 후퇴), 수직 화각은 FOV 가 아니라서(MaintainYFOV)
	// 손계산이 어긋나기 쉽다. 사용: CamInfo
	UFUNCTION(Exec)
	void CamInfo();

	// ========== 직원 이동 속도 / 로코모션 (OfficeMap 전용) ==========

	// 번아웃 배회(직원이 뛰는 유일한 상황)를 즉시 발동 — 피로 누적을 기다리지 않고 러닝 모션 확인.
	// Count 생략(-1)이면 전원, 숫자를 주면 앞 N명만 → 걷는 직원과 나란히 비교 가능.
	// 사용: WorkerBolt  /  WorkerBolt 3
	UFUNCTION(Exec)
	void WorkerBolt(int32 Count = -1);

	// 모든 직원 MaxWalkSpeed 를 직접 덮어써 BS_StickLocomotion 축을 훑는다. -1 이면 원래 값 복귀.
	// 임시값 — 슬랙 페이즈가 바뀌면 되돌아감. 좋은 값을 찾았으면 MoveSpeedWalk/MoveSpeedBolt 에 옮겨 적을 것.
	// 사용: WorkerSpeed 400  /  WorkerSpeed -1
	UFUNCTION(Exec)
	void WorkerSpeed(float NewSpeed);

	// 직원별 모드 / MaxWalkSpeed / 실측 속도 출력 — ABP Speed 축에 값이 실제로 들어가는지 확인
	// 사용: WorkerSpeedStatus
	UFUNCTION(Exec)
	void WorkerSpeedStatus();

	// 폭주 확률 진단 — 직원별 유효침착성/실효 초당 확률/판당 기대 인원 + 사무실 합계 출력.
	// 강제 발동(WorkerBolt)으로는 확률 튜닝을 검증할 수 없어(표본 수십 회 필요) 계산값을 직접 보여준다.
	// StepSeconds 생략 시 15초(DefaultStepDuration) 기준. 사용: BoltStatus  /  BoltStatus 30
	UFUNCTION(Exec)
	void BoltStatus(float StepSeconds = 15.f);

	// ========== 직원 얼굴 (AFunnyOfficeworker 얼굴 판) ==========

	// 사무실 전 직원 표정을 EWorkerFaceExpression 번호로 고정 — 16표정 그림을 실제 화면에서 훑는다. -1 = 해제(상태 표정 복귀).
	// 사용: Face_Set 9  /  Face_Set -1
	UFUNCTION(Exec)
	void Face_Set(int32 ExpressionIndex);

	// ========== 프로젝트 진척 / 도감 (UOfficeStageProgressManager, OfficeMap 전용) ==========

	// 개발 이력 + 출시 기록을 임의로 채운다 — 기획 보드/도감을 실제 플레이 없이 검증.
	// 현재 티어까지의 프로젝트를 앞에서부터 Count 개 '개발 완료'로 기록한다.
	// Review 0 이면 12~34 랜덤(등급별 상한 안쪽), 값을 주면 그 평점으로 고정.
	// 사용: Proj_Seed  /  Proj_Seed 12  /  Proj_Seed 8 30
	UFUNCTION(Exec)
	void Proj_Seed(int32 Count = 5, int32 Review = 0);

	// 현재 티어를 강제 설정(1~10) — 지나온 티어를 만들어 첫 개발/재개발 분기를 함께 보려면 필요.
	// 사용: Proj_SetTier 3
	UFUNCTION(Exec)
	void Proj_SetTier(int32 Tier);

	// 개발 이력 + 출시 기록 초기화 (빈 상태 화면 확인용)
	// 사용: Proj_ClearHistory
	UFUNCTION(Exec)
	void Proj_ClearHistory();

	// 현재 티어 / 개발 이력 / 출시 기록 수 출력
	// 사용: Proj_State
	UFUNCTION(Exec)
	void Proj_State();

	// ========== 진행 상태 프리셋 (UDevPresetSeeder) ==========

	// 지정 프리셋을 인세션 재적용 (DT_DevProgressPreset RowName). 비우면 CG Dev 설정값.
	// 사용: Preset_Apply Mid
	UFUNCTION(Exec)
	void Preset_Apply(const FString& PresetRowName);

	// 현재 상태가 프리셋과 정합한지 단언하고 실패 항목을 나열. 비우면 CG Dev 설정값.
	// 사용: Preset_Verify  /  Preset_Verify Late
	UFUNCTION(Exec)
	void Preset_Verify(const FString& PresetRowName);

	// ========== 밸런스 검증 계측 (Tools/Balance 하네스가 로그를 파싱) ==========
	// 출력 계약: 한 줄 `[BALV] <Name> k1=v1 k2=v2 ...` (공백 구분 k=v, 값은 숫자 또는 got/target 슬래시쌍).
	// 계약이 곧 파서 스펙이라 키 이름/구분자를 바꾸면 하네스가 조용히 빈 측정치를 낸다.

	// 개발 중 프로젝트의 직능 스텝별 획득/목표 + 가중치 + 품질점수 q + 등급을 한 줄로 덤프
	// 사용: Balance_DumpDevScore
	UFUNCTION(Exec)
	void Balance_DumpDevScore();

	// ResourceBank 전 재화 잔액 + 빌딩별 (실수익률/안정율/금고잔량/금고용량) + 직원/방치 인원을 한 줄로 덤프
	// 사용: Balance_DumpEconomy
	UFUNCTION(Exec)
	void Balance_DumpEconomy();

	// 오프라인 정산을 모의 경과초로 dry-run (세이브/금고 무변경) — 적립합/금고초과 손실/운영 경과 진행분 출력
	// 사용: Balance_SimOffline 3600  /  Balance_SimOffline 21600
	UFUNCTION(Exec)
	void Balance_SimOffline(int32 Seconds);

	// 게이트 통과율(A2) 자동 반복 — 미착석 직원 자동 착석 → N판 착수/판정/종료를 무탭으로 돌린다.
	// 리플렉션 착수만으로는 직원이 Stage 로 못 넘어가(책상 미배정 → 텔레포트 실패 → Idle 복귀) 점수가 조용히 0 이 되고,
	// 개발 ~40% 지점 부스트 도박/트레이트 이벤트 모달이 뜨면 타이머가 영구 정지한다. 이 치트가 셋 다 처리한다.
	// 판마다 `[BALV] GateRun idx=.. pass=0|1 q=..`, 끝나면 `[BALV] GateRunDone n=..`.
	// DirectionValue = EProjectDirection (0=표준 1=속도 2=품질 3=연구). slomo 로 가속 가능.
	// 사용: Balance_GateRun 30  /  Balance_GateRun 30 1 0
	UFUNCTION(Exec)
	void Balance_GateRun(int32 Trials = 30, int32 ProjectIndex = 1, int32 DirectionValue = 0);

	// 진행 중인 Balance_GateRun 즉시 중단 (그때까지 나간 GateRun 라인 + GateRunDone n=실제완주수 는 유효)
	UFUNCTION(Exec)
	void Balance_GateRunStop();

private:
	// 현재 월드의 TimeCycleManager 반환 (없으면 경고 로그 후 nullptr)
	class ATimeCycleManager* GetTimeCycleManager() const;

	// ===== Balance_GateRun 상태 머신 =====
	// 개발 1판은 실시간 수십 초라 블로킹 대기가 불가능하다(게임스레드가 멈추면 개발 자체가 안 돈다).
	// 반복 타이머 폴링으로 "판 종료 감지 → 판정 emit → 다음 판 착수" 를 잇는다.
	void BalvGateStartTrial();
	void BalvGatePoll();
	void BalvGateFinish(const TCHAR* AbortReason);

	FTimerHandle BalvGatePollTimerHandle;
	int32 BalvGateTrialsTotal = 0;
	int32 BalvGateTrialIndex = 0;
	int32 BalvGateProjectIndex = 1;
	int32 BalvGateDirectionValue = 0;
	int32 BalvGateEmpCount = 0;
	// 판별 관측치 — 실제로 책상을 가진 인원 / 그중 Stage+Typing 인 인원의 판 내 최대치.
	// 착석이 안 됐는데 측정이 굴러가는 사고를 데이터로 드러내기 위한 필드다(조용한 0점 방지).
	int32 BalvGateDeskPeak = 0;
	int32 BalvGateTypingPeak = 0;
	float BalvGateTrialElapsed = 0.0f;
	// 착수 직후엔 스텝시작 이펙트 지연 때문에 타이머가 아직 안 돈다 — 한 번이라도 돈 걸 본 뒤에야 "종료"로 판정한다.
	bool bBalvGateTimerSeen = false;
	// 다음 판 착수 예약. 종료와 같은 프레임에 착수하면 Idle↔Stage 착석 전이가 겹치므로 한 틱 미룬다.
	bool bBalvGateNeedStart = false;
};
