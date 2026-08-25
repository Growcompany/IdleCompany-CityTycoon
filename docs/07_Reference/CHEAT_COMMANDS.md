# 치트 명령어 가이드

## 개요

`UCGCheatManager`를 통해 모든 맵에서 콘솔 명령어로 디버깅 및 테스트가 가능합니다.

> **참고**: Shipping 빌드에서는 자동으로 비활성화됩니다.
> **보기 편한 버전**: 같은 폴더의 `CHEAT_COMMANDS.html`을 브라우저로 열면 검색/복사 가능한 치트 시트로 볼 수 있습니다.

## 사용 방법

1. 에디터에서 플레이 (PIE)
2. `~` 키를 눌러 콘솔 열기
3. 명령어 입력 후 Enter
4. 결과는 Output Log 창에서 확인 (`Window > Developer Tools > Output Log`, `LogCGCheat` / `LogTemp` 카테고리)

> 치트 매니저가 살아있는지 확인하려면 `CheatPing` 입력 → 로그에 `PONG`, 화면에 노란 메시지가 뜨면 정상.

---

## 전체 명령어 요약

| 명령어 | 카테고리 | 한 줄 설명 | 동작 맥락 |
|--------|---------|-----------|-----------|
| `CheatPing` | 기본 | 콘솔 디스패치 동작 확인 (PONG) | 어디서나 |
| `Exp [Amount] [ID]` | 경험치 | 직원에게 경험치 추가 | 직원 선택 필요 |
| `DistExp [Amount] [Grade]` | 경험치 | 현재 빌딩 직원들에게 분배 | 관리 중 빌딩 필요 |
| `ExpStatus` | 경험치 | 전체 직원 경험치/레벨 표시 | 어디서나 |
| `Money [Amount]` | 자원 | 돈 추가 | 어디서나 |
| `ShowResources` | 자원 | 보유 자원 표시 | 어디서나 |
| `GrantRes [Row]` | 자원 | 테스트 자원 시나리오 일괄 적용 | 어디서나 |
| `ListEmp` | 직원 | 전체 직원 목록 표시 | 어디서나 |
| `SetRank [Level]` | 직원 | 선택 직원 강화 레벨(직급) 설정 | 직원 선택 필요 |
| `SetCrit [Pct]` | 직원 | 직원 크리티컬 확률 강제 설정(트레일러용, 기본 50%), `-1`=해제 | OfficeMap |
| `SetStat [Idx] [Val] [ID]` | 직원 | 선택 직원의 6스탯 중 1개 직접 설정 (검증용 — ★ 반복 없이 단일 스탯 격리 확인) | 직원 선택 필요 |
| `StatStatus [ID]` | 직원 | 선택 직원의 6스탯 저장값/강화보너스/유효값 + 종합 출력 | 직원 선택 필요 |
| `EmpName [Rarity] [Count]` | 직원 | 이름 생성기만 N번 돌려 로그 출력 (직원 생성 없음) | 어디서나 |
| `CompleteStep` | 스테이지 | 현재 Step 즉시 완료 | OfficeMap |
| `StageStatus` | 스테이지 | 현재 스테이지 상태 표시 | OfficeMap |
| `Stage_FillScores [Pct]` | 스테이지 | 활성 직능 점수를 목표의 Pct%로 일괄 세팅 (리뷰/실패 화면 검증) | OfficeMap, 개발 진행 중 |
| `SetOpTime [Sec]` | 운영 | 현재 빌딩 운영 남은 시간 설정 | 관리 중 빌딩 필요 |
| `OpStatus` | 운영 | 진행 중인 모든 운영 상태 표시 | 어디서나 |
| `RevenueMax [Amount]` | 운영 | 모든 건물 금고를 건물당 `Amount`원(기본 500만) 강제 충전 → 대량 코인 수거 연출 (트레일러 "방치 수익" 컷) | 어디서나 |
| `ShowOfflineReport [Hours] [Zero] [FakeRows]` | 운영 | 오프라인 정산 모달 즉시 표시(대기/앱 라이프사이클 없이). 기본=실제 지은 건물만(금고도 채워 [모두 수령] 검증). `Hours`≥12=캡 변주, `Zero`=1=적립0 변주, `FakeRows`>0=그 개수까지 가짜 행 패딩(레이아웃/스크롤 테스트) | MainMap |
| `Ticket [Amt] [Tier]` | 아이템 | 채용권 지급 | 어디서나 |
| `TraitTicket [Amt] [Tier]` | 아이템 | 건물 특성 뽑기권 지급 | 어디서나 |
| `SkinTicket [Amt] [Tier]` | 아이템 | 건물 스킨 뽑기권 지급 | 어디서나 |
| `Card [Amt] [Tier]` | 아이템 | 잠재 명함 지급 (직원창 잠재 리롤 테스트용) | 어디서나 |
| `LootRoll [Key] [Tier] [Count]` | 아이템 | 출시 전리품 드랍 롤 시뮬레이션 (지급 없음, 분포 로그만) | 어디서나 |
| `UnlockSkins [Count]` | 건물 | 스킨 랜덤 해금 | 어디서나 |
| `BldLightsOn [0/1]` | 건물 | 모든 건물 창문 조명 항상 켬(1)/정상 복원(0) — 운영 여부 무시 | MainMap |
| `FactoryMax [AmtLv] [SpeedSec]` | 벽돌공장 | 벽돌공장 개수+속도 만렙, SpeedSec로 버스트 간격 라이브 오버라이드 (트레일러 우수수) | 공장 배치된 맵 |
| `UnlockFactoryAuto` | 벽돌공장 | 자동 수집 즉시 해금 (HQ Lv.10 대기 없이) | 공장 배치된 맵 |
| `HQLevelUp` | 본사 | 조건 무시 강제 레벨업 | 어디서나 |
| `SetHQLv [Level]` | 본사 | 본사 레벨 직접 설정 | 어디서나 |
| `MineDebug` | 채광 | 활성 채광 국가 상태 출력 | WorldMap |
| `MineFill [Country]` | 채광 | 지정 국가 라인 한도까지 채움 | WorldMap |
| `MineSetLv [Country] [Type] [Lv]` | 채광 | 채광 강화 레벨 설정 (다운 불가) | WorldMap |
| `Prod_Country [Country]` | 생산 | 그 국가에서 제작 가능한 주문서 + 제작가능수/병목 | WorldMap |
| `Prod_Orders` | 생산 | 살아있는 주문서 전량 덤프 | WorldMap |
| `Prod_Seed [Count]` | 생산 | 테스트 주문서 생성 (생략=전체) | WorldMap |
| `Prod_MatFill [Amount]` | 생산 | 원자재 10종 일괄 수량 맞춤 | WorldMap |
| `Prod_MatSet [Res] [Amount]` | 생산 | 특정 원자재를 정확히 그 수량으로 | WorldMap |
| `Fac_Lines [Country]` | 생산 | 공장 라인 현황 (Active/Max + 각 라인) | WorldMap |
| `Fac_FillLines [Country]` | 생산 | 라인 가득 채우기 (더미 생산) | WorldMap |
| `Fac_ClearLines [Country]` | 생산 | 활성 라인 전부 즉시 완료+수령해 비움 (FillLines 되돌리기) | WorldMap |
| `SeedProducts [Slots] [Min] [Max]` | 판매 | 판매 모달용 랜덤 인벤 시드 | WorldMap |
| `SeedTraits [Kinds] [Min] [Max]` | 특성 | 건물 특성 인벤 랜덤 시드 | 어디서나 |
| `TraitStatus` | 특성 | 특성 보유 현황 출력 | 어디서나 |
| `TraitEffects [BuildingIndex]` | 특성 | 특성 효과 집계 덤프 (대상 20종) | 어디서나 |
| `ApplyStarterPreset [Row]` | 프리셋 | 사무실 스타터 프리셋 강제 적용 | OfficeMap |
| `ExportStarterPreset` | 프리셋 | 현재 배치를 프리셋 CSV 형식으로 출력 | OfficeMap |
| `CamPreset [Index]` | 카메라 | 오피스 카메라 각도 프리셋 (0코지~3탑다운, FOV고정) | OfficeMap |
| `CamBand [InP] [OutP] [InF] [OutF]` | 카메라 | 오피스 카메라 밴드 수동 지정 (피치/FOV) | OfficeMap |
| `WorkerBolt [Count]` | 이동속도 | 번아웃 배회(뛰는 상황) 즉시 발동 (생략=전원, N=앞 N명) | OfficeMap |
| `WorkerSpeed [Value]` | 이동속도 | 전 직원 MaxWalkSpeed 직접 지정 (-1 = 원래 값 복귀) | OfficeMap |
| `WorkerSpeedStatus` | 이동속도 | 직원별 모드/MaxWalkSpeed/실측 속도 출력 | OfficeMap |
| `BoltStatus [판초]` | 직원 | 폭주 확률 진단 — 직원별 실효 초당/판당 확률 + 판당 기대 인원 | OfficeMap |
| `Proj_Seed [Count] [Review]` | 프로젝트진척 | 개발 이력 + 출시 기록 임의 채움 (기본 5건, Review 0=랜덤) | OfficeMap |
| `Proj_SetTier [Tier]` | 프로젝트진척 | 현재 티어 강제 설정 (1~10) | OfficeMap |
| `Proj_ClearHistory` | 프로젝트진척 | 개발 이력 + 출시 기록 초기화 (빈 상태 확인) | OfficeMap |
| `Proj_State` | 프로젝트진척 | 티어/개발이력/출시기록 수 + 개발 인덱스 출력 | OfficeMap |
| `Preset_Apply [Row]` | 진행프리셋 | 중반 진행 상태 프리셋 인세션 재적용 (Early/Mid/Late) | MainMap |
| `Preset_Verify [Row]` | 진행프리셋 | 현재 상태가 프리셋과 정합한지 단언 + 실패 항목 나열 | 어디서나 |
| `SkipToMission [Row]` | 튜토리얼 | 임의 미션으로 즉시 점프 | 인세션 |
| `ResetTutorial` | 튜토리얼 | 세이브 삭제 (다음 Play M1부터) | 어디서나 |
| `UnlockGoalBoard` | 미션판 | 미션판 즉시 언락 + 전수 재평가 + 저장 (체인 완주 없이) | 인세션 |
| `CompleteGoal [Row]` | 미션판 | 지정 미션 강제 충족 (보상 지급 없음 — 수령은 보드에서) | 인세션 |
| `ResetGoals` | 미션판 | 미션판 초기화 (언락/충족/수령 전부 리셋) + 저장 | 인세션 |
| `ResetPanelIntro` | 미션판 | 패널 최초 진입 안내(코치마크)를 안 본 상태로 리셋 + 저장 + 제스처 힌트 졸업 카운트(졸음/폭주) 리셋 | 인세션 |
| `Acq_Buy [Key]` | 도시인수 | 지정 Key 회사 인수 시도 (자금 차감 + Milking 전환) | MainMap |
| `Acq_State [Key]` | 도시인수 | 지정 Key 인수 상태 출력 (0=미인수 1=수익중 2=고갈 3=완료) | MainMap |
| `Acq_Demolish [Key]` | 도시인수 | 지정 Key의 회사 철거 = 캐시아웃 (Milking/Depleted 에서 동작 -> Cleared) | MainMap |
| `Acq_ClearAll` | 도시인수 | 모든 도시 회사 즉시 인수 완료(Cleared) — 건물 사라지고 부지 구매 가능 (자금/상태 무시) | MainMap |
| `Acq_FillPot [Key] [Pct]` | 도시인수 | Milking 회사의 회수율을 Pct%(기본 90)까지 즉시 진행 (금고만 참, 지갑 무변동) | MainMap |
| `Plot_OwnAll` | 도시인수 | 모든 도시 부지 즉시 소유 (인접/자금/결제 게이트 우회) + 저장 | MainMap |
| `Day` | 시간대 | 낮으로 고정 (12:00 + 사이클 정지) | TimeCycle 액터 필요 |
| `Night` | 시간대 | 밤으로 고정 (00:00 + 사이클 정지) | TimeCycle 액터 필요 |
| `SetTime [H] [M]` | 시간대 | 임의 시각 고정 (+ 사이클 정지) | TimeCycle 액터 필요 |
| `TimeResume` | 시간대 | 시간 사이클 재개 (고정 해제) | TimeCycle 액터 필요 |
| `CG_TestBacklogMath` | 백로그 | 순익 지수감쇠 수학 자가검증 (4샘플 로그) | 어디서나 |
| `CG_DumpBacklog` | 백로그 | 현재 관리 빌딩 백로그 제품 목록 + 순익 합산 출력 | OfficeMap |
| `CG_DumpUI` | UI디버그 | UIBase 3스택 내용 + 현재 EInputMode 출력 (입력잠금 진단) | 어디서나 |
| `CG_UnlockInput` | UI디버그 | 입력모드 UI 잠김 시 강제 Normal 복원 (탈출용) | 어디서나 |
| `Balance_DumpDevScore` | 밸런스계측 | 개발 중 프로젝트 직능 스텝별 획득/목표 + 가중치 + q + 등급을 `[BALV]` 한 줄로 | OfficeMap |
| `Balance_DumpEconomy` | 밸런스계측 | 전 재화 잔액 + 빌딩별 수익률/안정율/금고잔량/금고용량 + 직원·방치 인원을 `[BALV]` 한 줄로 | 어디서나 |
| `Balance_SimOffline [Sec]` | 밸런스계측 | 오프라인 정산을 모의 경과초로 dry-run (세이브 무변경) — 적립/금고초과 손실/운영 경과 진행분 | 어디서나 |
| `Balance_GateRun [N] [Proj] [Dir]` | 밸런스계측 | 게이트 통과율 N판 자동 반복 (자동 착석 + 무탭 + 모달 자동 해소). 판마다 `[BALV] GateRun`, 끝나면 `GateRunDone` | OfficeMap (Game 산업) |
| `Balance_GateRunStop` | 밸런스계측 | 진행 중인 `Balance_GateRun` 즉시 중단 | OfficeMap |

---

## 명령어 상세

### 기본

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `CheatPing` | 콘솔 디스패치가 작동하는지 확인 (`PONG` 로그 + 화면 메시지) | `CheatPing` |

---

### 경험치 관련

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `Exp [Amount]` | 선택된 직원에게 경험치 추가 | `Exp 100` |
| `Exp [Amount] [ID]` | 특정 직원(ID)에게 경험치 추가 | `Exp 100 3` |
| `DistExp [Amount]` | 현재 관리 중인 빌딩 직원들에게 분배 (B등급=1.0x) | `DistExp 50` |
| `DistExp [Amount] [Grade]` | 품질 등급 지정 분배 | `DistExp 100 5` |
| `ExpStatus` | 전체 직원 경험치/레벨/소속 빌딩 상태 표시 | `ExpStatus` |

> `Exp`는 ID 생략 시 **선택된 직원**, `DistExp`는 **현재 관리 중인 빌딩**(`GetCurrentManagedBuildingIndex`)이 대상. 선택/관리 중 빌딩이 없으면 경고 로그만 출력.

**품질 등급 값:**
| 값 | 등급 | 배율 |
|----|------|------|
| 0 | F | 0.3x |
| 1 | D | 0.5x |
| 2 | C | 0.8x |
| 3 | B | 1.0x |
| 4 | A | 1.3x |
| 5 | S | 1.5x |

---

### 자원 관련

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `Money [Amount]` | 돈 추가 (int64) | `Money 10000` |
| `ShowResources` | 현재 보유 자원 표시 | `ShowResources` |
| `GrantRes [RowName]` | 테스트 자원 시나리오 일괄 적용 (`DT_TestResourceScenarios`의 RowName) | `GrantRes Rich` |

**시나리오 RowName** (인자 생략 시 `Default`):
`Rich` / `Default` / `Poor` / `Empty`

> RowName이 DT에 없으면 `FAIL` 로그 + 빨간 화면 메시지. 자원 타입은 `EResourceType`(현재 Money) 기준.

---

### 직원 관련

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `ListEmp` | 전체 직원 목록 표시 (선택된 직원은 `>>>` 표시) | `ListEmp` |
| `SetRank [Level]` | 선택 직원의 강화 레벨 설정 (0-12) + 외모 갱신 | `SetRank 5` |
| `SetCrit [Percent]` | 직원 크리티컬 확률을 강제로 `Percent`%로 설정 (기본 50). `-1`이면 해제(정상 크리율 복귀) | `SetCrit` / `SetCrit 50` / `SetCrit -1` |
| `SetStat [StatIndex] [Value]` | 선택 직원의 지정 스탯(0~5)을 `Value`로 직접 설정 (음수는 0으로 클램프) | `SetStat 1 35` |
| `SetStat [StatIndex] [Value] [ID]` | 특정 직원(ID)의 지정 스탯 설정 | `SetStat 1 35 3` |
| `StatStatus` | 선택 직원의 6스탯 저장값 + 강화보너스 + 유효값(합산) + 종합 점수 출력 | `StatStatus` |
| `StatStatus [ID]` | 특정 직원(ID)의 스탯 상태 출력 | `StatStatus 3` |
| `EmpName [Rarity] [Count]` | 이름 생성기를 Count번 호출해 로그 출력. Rarity: 0 Common · 1 Unusual · 2 Rare · 3 Epic · 4 Legendary · 5 Mythic. 실제 직원 생성/세이브 없음 | `EmpName 5 100` |

> `SetStat`/`StatStatus`는 ID 생략 시 **선택된 직원**이 대상(`Exp`와 동일 규칙). 선택된 직원이 없고 ID도 생략하면 경고 로그만 출력. **StatIndex 규약**: `0=업무속도(WorkSpeed) 1=크리티컬 확률(CritChance) 2=침착성(Composure) 3=경험치획득(ExpGain) 4=체력(Stamina) 5=수익보너스(IncomeBonus)` — `EEmployeeStatIndex`/`UEmployeeStatsHelper`와 동일 인덱스. ★ 강화 15회를 반복하지 않고 스탯 1개의 효과만 격리 검증하기 위한 치트(Task 6~10 전제 도구).

> `SetCrit`은 `UOfficeStageProgressManager::GetCriticalChance()`(직원 작업 크리 판정의 단일 소스 — `EmployeeBehaviorComponent`가 매 산출 틱에 읽어 크리 스코어 오브/사운드 트리거)를 오버라이드한다. 기본 크리율(0.003)/Fortune 특성/피버 전부 무시하고 지정 확률을 반환하므로 **트레일러에서 크리가 자주 터진다**. 스테이지 리셋(크리율 0.003 초기화)에 영향받지 않아 프로젝트가 바뀌어도 유지 → `SetCrit -1`로 해제. `UOfficeStageProgressManager`는 월드 서브시스템이라 OfficeMap 세션 한정(맵 재로드 시 리셋). 참고: 직원별 버프/잠재큐브 크리 보너스는 이 값에 **가산**되므로 실제 확률은 지정값 이상이 될 수 있음(트레일러엔 유리).

**강화 레벨 → 직급:**
| 레벨 | 직급 | 레벨 | 직급 |
|------|------|------|------|
| 0 | 인턴 | 7 | 이사 |
| 1 | 사원 | 8 | 상무 |
| 2 | 주임 | 9 | 전무 |
| 3 | 대리 | 10 | 부사장 |
| 4 | 과장 | 11 | 사장 |
| 5 | 차장 | 12 | 회장 |
| 6 | 부장 | | |

---

### 스테이지 관련 (OfficeMap 전용)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `CompleteStep` | 3개 카테고리 모두 MinimumScore 이상 되도록 보너스 점수 부여 | `CompleteStep` |
| `StageStatus` | 현재 스테이지 상태 표시 (프로젝트/Step/점수/시간/품질/타이머) | `StageStatus` |
| `Stage_FillScores [Percent]` | 활성 직능 스텝 점수를 목표의 Percent%(기본 100, 0~200)로 일괄 세팅 — 30=최소선(50%) 미달로 출시 차단(실패 페이지) 재현 / 95=Best 컨텍스트 리뷰 재현 | `Stage_FillScores 30` |

> 스테이지가 진행 중이 아니면 `No stage in progress` 로그. OfficeMap이 아니면 매니저를 못 찾음.

---

### 운영 관련

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `SetOpTime [Seconds]` | 현재 관리 중인 빌딩의 운영 남은 시간 설정 (0 = 다음 틱에 즉시 완료) | `SetOpTime 0` |
| `OpStatus` | 진행 중인 모든 운영 상태 표시 (빌딩/품질/시간/수익) | `OpStatus` |
| `RevenueMax [Amount]` | 모든 빌딩 금고를 건물당 `Amount`원(기본 5,000,000) **절대액**으로 강제 충전 → 돈 버블 전부 표시. 수거는 안 함 | `RevenueMax` / `RevenueMax 10000000` |
| `ShowOfflineReport [OfflineHours] [bZeroGain] [FakeRows]` | 오프라인 정산 모달을 즉시 띄운다(60초 대기/앱 라이프사이클 흉내 없이). **실제 표시 경로**(`DebugSetPendingOfflineReport`→`TryShowOfflineReport`→Consume→모달) 그대로 태워 회귀 테스트 겸용. **기본 = 실제 지은 건물만** 내역 생성(절반은 금고 초과 손실, 일부는 운영 종료 태그 — 행 상태 확인) + **실제 금고도 같이 채워** [모두 수령] 코인 수거까지 검증. `OfflineHours`≥12면 12h 캡 변주, `bZeroGain`=1이면 적립 0(금고 만석) 변주. **`FakeRows`>0이면** 실제 건물 + 그 개수까지 가짜 인덱스 행 패딩(존재 안 하는 인덱스라 "건물 N" 폴백 — 레이아웃/스크롤 테스트용, 최대 30) | `ShowOfflineReport` / `ShowOfflineReport 14` / `ShowOfflineReport 3 1` / `ShowOfflineReport 3 0 8` |

> `SetOpTime`은 현재 관리 중인 빌딩에 **진행 중인 운영**이 있어야 동작. `ElapsedTime`도 함께 동기화됨.
> `RevenueMax`는 트레일러 "방치 수익" 컷 세팅용 — 각 건물 금고를 **금고 용량을 무시하고** 건물당 `Amount`원(기본 500만) 절대액으로 강제 세팅해 돈 버블을 동시에 띄운다. 이후 화면의 **전체수거** 버튼을 누르면 그 큰 금액이 그대로 회수되어(수거 시 용량 재클램프 없음) 코인이 상한(100개)까지 우수수 쏟아진다. 예전엔 금고 용량(≈수백 원)의 비율만 채워 수거액이 작았음 → 절대액 방식으로 변경(2026-07-13). 수거 로직은 기존 `CollectAllStoredRevenue` 그대로.

---

### 아이템 관련

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `Ticket [Amount] [Tier]` | 채용권 지급 (기본: 10장, Normal) | `Ticket 30 2` |
| `TraitTicket [Amount] [Tier]` | 건물 특성 뽑기권 지급 (가챠 풀 플로우 테스트용, 기본 30장) | `TraitTicket` / `TraitTicket 50 0` |
| `SkinTicket [Amount] [Tier]` | 건물 스킨 뽑기권 지급 (스킨 가챠 풀 플로우 테스트용, 기본 30장) | `SkinTicket` / `SkinTicket 50 1` |
| `Card [Amount] [Tier]` | 잠재 명함 지급 — 직원창 잠재 리롤 테스트용 (기본 20장씩 3종 전부) | `Card` / `Card 5 2` |
| `LootRoll [TableKey] [Tier] [Count]` | 출시 전리품 드랍(`DT_LaunchLoot`) 롤 시뮬레이션 — 지급 없이 분포만 로그 (기본 `High` / Tier 10 / 10회) | `LootRoll` / `LootRoll Mid 5 50` |

**채용권 등급(Tier) 값:**
| 값 | 등급 | EItemType |
|----|------|-----------|
| 0 | Normal | `RecruitTicketNormal` |
| 1 | Advanced | `RecruitTicketAdvanced` |
| 2 | Premium | `RecruitTicketPremium` |

**특성/스킨 뽑기권 Tier 값** (`TraitTicket` / `SkinTicket`):
| 값 | 지급 대상 |
|----|-----------|
| 0 | Normal만 |
| 1 | Advanced만 |
| -1 (기본) | Normal + Advanced 둘 다 |

> `TraitTicket`/`SkinTicket`은 **뽑기권을 지급**해 가챠 풀 플로우를 테스트하는 용도. 가챠를 우회해 특성을 **직접 인벤에 넣으려면** `SeedTraits` 사용.

**`Card` Tier 값** (잠재 명함 — 티어 = 리롤 등급 상한):
| 값 | 지급 대상 | 등급 상한 |
|----|-----------|-----------|
| 0 | 종이 명함 (`BusinessCardPaper`) | 레어 |
| 1 | 골드 명함 (`BusinessCardGold`) | 에픽 |
| 2 | 블랙 명함 (`BusinessCardBlack`) | 레전드리 |
| -1 (기본) | 3종 전부 | — |

> 명함의 정규 획득 경로는 **상점 전용**(종이=일일 Money, 골드=주간 Money·다이아, 블랙=다이아 전용)이라 `Card`는 그 구매를 우회하는 테스트 수단이다. `AddItem`이 `OnItemChanged`를 브로드캐스트하므로 **직원창을 열어둔 채로 쳐도** 명함 슬롯 수량과 리롤 버튼 활성이 즉시 갱신된다.

**`LootRoll` TableKey 값** (출시 평점 총점 4~40 기준):
| 값 | 조건 | 기본 롤 수 |
|----|------|-----------|
| `Low` | 평점 < 24 | 2 |
| `Mid` | 평점 24~31 | 2 |
| `High` | 평점 >= 32 | 3 |

> 실제 롤 수 = 기본 롤 수 + 티어 보너스(T1~3 `0` / T4~6 `+1` / T7~10 `+2`). `ScaleBasis`(개발비)는 100,000 고정이라 만분율 비례 엔트리도 함께 검증된다.
> **지급은 하지 않는다**(`bGrant=false`) — 재화/아이템 인벤은 그대로. 결과는 `[LaunchLoot] Roll ...` 로그로만 확인.

---

### 건물 관련

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `UnlockSkins [Count]` | 미보유 스킨 중 랜덤 해금 (기본 5개) | `UnlockSkins 10` |
| `BldLightsOn [bOn]` | 모든 건물 창문 조명을 운영 여부와 무관하게 항상 켬(`1`, 기본) / 원래 정책(운영 중만 점등)으로 복원(`0`) | `BldLightsOn` / `BldLightsOn 0` |

> `BldLightsOn`은 창문 발광(`Window_Emissive_Color`)을 운영 상태와 분리한다. 평소엔 `InGameLayer`가 운영 중인 건물만 점등하고 유휴 건물은 끄는데(`SetWindowLightActive(HasActiveOperation)`), 이 치트는 `ABuildingBaseActor`의 전역 강제 플래그를 켜 `SetWindowLightActive(false)` 요청을 무시하게 만들어 **모든 건물을 항상 켠 상태로 고정**한다(off 경로가 그 함수 단일 창구뿐이라 재평가와 무관하게 유지). `BldLightsOn 0`으로 플래그 해제 + 운영 여부 기준 정상 복원. MainMap의 `UEntityManager` 건물 목록 대상. (전역 static 플래그라 PIE 재시작 시에도 유지될 수 있음 — 끄려면 `BldLightsOn 0`.)

---

### 벽돌공장 (ABrickFactory — 트레일러 캡처용)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `FactoryMax` | 현재 맵의 모든 벽돌공장을 개수(HoldProductionAmount)+속도 만렙으로. 속도=곡선 바닥 0.2초. 공장을 홀드하면 벽돌 우수수 | `FactoryMax` |
| `FactoryMax [AmountLevel]` | 개수를 지정 레벨로 낮춰 "+N"/카운터 폭주 억제 (속도는 곡선 만렙) | `FactoryMax 50` |
| `FactoryMax [AmountLevel] [SpeedSeconds]` | 버스트 간격(초)을 강화 곡선 대신 직접 오버라이드 — **재빌드 없이 PIE에서 라이브 튜닝**. AmountLevel에 -1이면 개수는 만렙 유지 | `FactoryMax -1 0.3` / `FactoryMax 30 0.07` |
| `UnlockFactoryAuto` | 현재 맵의 모든 벽돌공장 **자동 수집을 즉시 해금**. 자동 수집/최대 용량 강화 슬롯의 잠금이 풀리고 벽돌 재고 슬롯이 활성화되며, 다음 공장 패널 오픈 시 해금 연출이 1회 재생된다. 이미 해금된 공장은 무시 | `UnlockFactoryAuto` |

> **버스트 간격 = 우수수 밀도의 핵심 레버.** 값이 작을수록 빠르게 쏟아짐: 0.045초=초당 22버스트(스트로브에 가까움), 0.1초=초당 10버스트(펄스), 0.07초 부근=끊김 없는 폭포, 0.3초=묵직한 청크 드롭. **곡선 만렙(baked) = 0.2초로 확정** (351레벨 수렴). `SpeedSeconds` 인자로 PIE 안에서 즉시 다른 값도 실험 가능.
> 개수(HoldProductionAmount) 만렙은 스프라이트 수가 아니라 **`+N` 팝업/브릭 카운터 숫자**만 키운다 (스프라이트는 `UI_InGameLayer`의 `BrickBurstMaxIcons`로 캡 → 프레임 안전).
> **자동수집(AutoCollection/Capacity)은 의도적으로 안 건드림** — 만렙 시 0.001초 틱으로 "벽돌 공장 보관함이 가득 찼습니다" 알림이 팝되어 트레일러 컷을 망치기 때문. 자동수집은 시각적으로 벽돌을 뿌리지도 않는다(조용히 누적).
> 간격 오버라이드는 공장 액터의 `TrailerSpawnIntervalOverride`(Transient) 필드에 세팅되고 `OnInteract`가 곡선값보다 우선 읽는다. **비용 차감 없음.** 벽돌 우수수 연출은 공장을 **홀드 프레스**할 때만 나온다(가만두면 안 나옴).
> `UnlockFactoryAuto`는 정규 해금 축(`USaveLoadManager::OnHQLevelUp` → `DT_FactoryUpgradeDefinition.RequiredHQLevel` 충족, 현재 출하값 10)을 우회해 `ABrickFactory::UnlockAutoCollection()`을 직접 호출한다. 해금 상태는 `FFactorySaveData.bAutoCollectionUnlocked`로 저장되므로 **되돌리려면 세이브를 지워야 한다**. 자동 수집 타이머 간격은 하한 0.25초로 클램프된다(강화 곡선 바닥이 0.001초라 그대로 두면 초당 1000틱).

---

### 본사 레벨 관련

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `HQLevelUp` | 조건 무시하고 본사 레벨 +1 (저장 + 델리게이트) | `HQLevelUp` |
| `SetHQLv [Level]` | 본사 레벨 직접 설정 (최소 1, 저장 + 델리게이트) | `SetHQLv 8` |

---

### 채광 관련 (UMineManager — WorldMap 채광 시스템)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `MineDebug` | 모든 채광 활성 국가의 라인 상태/강화 레벨/분당 채취량 출력 | `MineDebug` |
| `MineFill [CountryName]` | 지정 국가 모든 라인을 한도까지 채움 (수령/saturated UI 테스트) | `MineFill Australia` |
| `MineSetLv [CountryName] [Rate or Storage] [Level]` | 강화 레벨까지 반복 업그레이드 (다운 미지원) | `MineSetLv Australia Rate 5` |

**Country 이름** (DT_CountryInfo RowName 일치, 채광 가능 국가):
`Australia` / `Canada` / `Brazil` / `SouthAfrica` / `China` / `Saudi`

**강화 종류**:
- `Rate` (또는 `MiningRate`) — 분당 채취 속도 +15%/lv
- `Storage` (또는 `MiningStorage`) — 저장 한도 +100/lv (Lv0=100)

> **전제 조건**: CountryDetail 채광 탭에 한 번 진입했거나 `MineFill` 호출되어야 라인이 생성됨 (`EnsureCountryLines` 자동 호출).
> **레벨 다운 불가**: `MineSetLv`는 내부적으로 `UpgradeLevelUp`을 반복 호출하므로 현재보다 낮은 레벨로는 못 감 (낮추려면 새 세이브 필요).

---

### 생산 주문서 / 공장 라인 (UProductionOrderManager · UWorldFactoryManager)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `Prod_Country [CountryName]` | 그 국가에서 제작 가능한 주문서 전량 + 각 주문의 제작가능수/병목 재료 + 라인 현황 | `Prod_Country Korea` |
| `Prod_Orders` | 국가 필터 없이 살아있는 주문서 전량 덤프 | `Prod_Orders` |
| `Prod_Seed [Count]` | DT_TestProductionOrders 로 테스트 주문서 생성 (생략/-1 = 전체) | `Prod_Seed 5` |
| `Prod_MatFill [Amount]` | 원자재 10종을 일괄로 그 수량에 맞춤 (기본 100,000) | `Prod_MatFill 500` |
| `Prod_MatSet [Resource] [Amount]` | 특정 원자재를 정확히 그 수량으로 | `Prod_MatSet Lithium 62` |
| `Fac_Lines [CountryName]` | 공장 라인 현황 (Active/Max + 각 라인 제품/진행/목표) | `Fac_Lines Korea` |
| `Fac_FillLines [CountryName]` | 라인이 꽉 찰 때까지 더미 생산 시작 | `Fac_FillLines Korea` |
| `Fac_ClearLines [CountryName]` | 활성 라인 전부 즉시 완료 + 수령해 비움 (FillLines 되돌리기) | `Fac_ClearLines Korea` |

**Country 이름** (공장국): `Korea` / `China` / `Japan` / `Germany` / `USA`

**Resource 이름** (원자재 10종):
`IronOre` / `Copper` / `Silicon` / `Lithium` / `Oil` / `RareEarth` / `Aluminum` / `Wood` / `Gold` / `DiamondOre`

**출력 예시** (`Prod_Country Korea`):
```
===== [Korea] 제작 가능 목록 =====
  지원 산업: Electronics Semiconductor
  공장 라인: 2 / 3
  #12   Electronics  idx=24  제습기            등급A 남은 30  -> 20개까지(상한 Lithium)
  #13   Electronics  idx=37  블랙박스          등급C 남은  8  -> 6개까지(상한 RareEarth)
  #14   Electronics  idx=58  헤어 스타일러      등급S 남은 50  -> 제작불가(Gold 부족)
===== 3건 =====
```

> **제작 시작 팝업과 같은 계산을 씁니다** — `UProductionOrderManager::GetMaxProducible()` 을 그대로 호출하므로,
> 치트 출력과 팝업의 "제작 가능 N개 · 상한 X" 가 항상 일치한다. 팝업이 이상하면 이 치트로 데이터부터 확인할 것.

> **`Prod_MatFill` 은 낮추기도 된다** — Set API 가 없어 델타로 Store/Spend 한다.
> `Prod_MatFill 0` 이면 전부 0 (제작불가 상태), `Prod_MatFill 500` 이면 부분 부족 상태를 만들 수 있다.

> **`Fac_FillLines` 는 재료를 차감하지 않는 더미 라인** — "라인 가득" UI 상태만 만들기 위한 것.
> 더미는 `TargetQty=9999 · rate=0.01/s` 라 **완주에 수일**이 걸린다. 되돌릴 땐 **`Fac_ClearLines`** 를 쓸 것
> (`InstantFinishLine` + `ClaimLine`). ⚠ 더미가 아닌 진짜 라인도 함께 비워진다.

#### 제작 시작 팝업 테스트 시퀀스

월드맵 → 국가 클릭 → 공장 탭 → **[+ 추가 제작]** 이 대상. 상태별로 이렇게 만든다.

```
# 1) 기본 상태 — 전부 제작 가능
Prod_Seed                 주문서 24건 전량 생성 (숫자 주면 무작위 표본이라 특정 산업이 0 될 수 있음)
Prod_MatFill 100000       원자재 10종 충분  → CTA 초록, 부족 0
Prod_Country USA          팝업이 보여줄 목록을 로그로 먼저 확인 (전 산업)

# 2) 재료 상한 — "제작 가능 N개 · 상한 X" (노란 판독부)
Prod_MatSet Lithium 62    전자/자동차 레시피의 리튬을 병목으로
Prod_MatSet Silicon 500   반도체 레시피의 실리콘을 병목으로

# 3) 제작 불가 — 레일에 "X 부족 ― 1개도 못 만듭니다" (빨강)
Prod_MatSet Gold 2
Prod_MatFill 0            전 주문 제작불가

# 4) 라인 가득 — 헤더 칩 빨강 + CTA 회색, 클릭하면 토스트
Fac_FillLines Korea
Fac_Lines Korea           라인 현황 확인
```

> **국가별로 보이는 게 다른 건 정상** — `DT_CountryInfo.SupportedIndustries` 로 걸러진다.
> `Korea`=Semiconductor / `Japan`=Electronics / `Germany`=Automobile / `China`·`USA`=전 산업.
> 한국에서 전자제품 주문서가 안 보이는 건 버그가 아니다.

> **팝업과 같은 계산을 쓴다** — `Prod_Country` 는 `UProductionOrderManager::GetMaxProducible()` 을 그대로 호출하므로
> 로그의 "20개까지(상한 Lithium)" 와 팝업의 "제작 가능 20개 · 상한 리튬" 이 어긋날 수 없다.
> 화면이 이상하면 이 치트로 **데이터 문제인지 UI 배선 문제인지** 먼저 가른다.

---

### 월드맵 판매 인벤 (UWorldMapManager — WorldMap 전용)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `SeedProducts` | 판매 모달 테스트용 랜덤 인벤 시드 (24슬롯, 100~8000개) | `SeedProducts` |
| `SeedProducts [Slots]` | 슬롯 수 지정 | `SeedProducts 50` |
| `SeedProducts [Slots] [Min] [Max]` | 슬롯 수 + 수량 범위 지정 | `SeedProducts 30 500 20000` |

> 산업 1~6 x 프로젝트 1~5, 등급 F~S 랜덤. WorldMap 레벨에서만 동작.

---

### 건물 특성 (UBuildingTraitManagerSubsystem)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `SeedTraits` | 특성 인벤 랜덤 시드 (30종, 1~5장) | `SeedTraits` |
| `SeedTraits [Kinds]` | 종류 수 지정 | `SeedTraits 60` |
| `SeedTraits [Kinds] [Min] [Max]` | 종류 수 + 장수 범위 지정 | `SeedTraits 105 1 10` |
| `TraitStatus` | 특성 보유 현황 출력 (종류 수 / 총 장수 / 등급별) | `TraitStatus` |
| `TraitEffects` | 현재 관리 건물의 특성 효과 집계 (0 아닌 대상만 + 전사 무역) | `TraitEffects` |
| `TraitEffects [BuildingIndex]` | 지정 건물의 집계 | `TraitEffects 3` |

> `DT_BuildingTrait`의 모든 Row 중 랜덤 N개를 셔플해 부여. DT가 비어있으면 경고 로그.

---

### 사무실 스타터 프리셋 (OfficeMap 전용)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `ApplyStarterPreset [RowName]` | 스타터 프리셋 강제 적용 (타일 크기 + 데코 배치). 기존 확장 상태 덮어씀 | `ApplyStarterPreset Mythic_0` |
| `ExportStarterPreset` | 현재 사무실 배치 데코를 프리셋 CSV 엔트리 형식(상대 좌표)으로 로그 + 파일 출력 | `ExportStarterPreset` |

> `ApplyStarterPreset`의 RowName은 `DT_OfficeStarterPreset` 기준. 적용 후 `OfficeInterior`/`OfficeManager` 복원기를 즉시 재실행해 반영. `ExportStarterPreset`은 `Saved/StarterPresetExport.txt`에 저장하며 스케일 미반영 상대 좌표를 쓴다. 둘 다 OfficeMap에서만 동작.

---

### 카메라 각도 (OfficeMap 전용)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `CamPreset [Index]` | 오피스 카메라 각도 프리셋 즉시 적용 (FOV 40/30 고정, 피치만 변화). 0=코지(~-38) 1=밸런스(~-45) 2=탑다운(~-53) 3=하드탑다운(~-60) | `CamPreset 2` |
| `CamBand [InP] [OutP] [InF] [OutF]` | 오피스 카메라 밴드 수동 지정 — 줌인/줌아웃 피치 + FOV (InF/OutF 생략 시 40/30) | `CamBand -44 -58 40 30` |

> 둘 다 OfficeMap에서만 동작 (possess한 Pawn이 `AOfficeCameraPawn`일 때). 런타임 즉시 반영 — 각도 비교용 임시 도구이며, 확정 값은 `AOfficeCameraPawn` 생성자에 베이크한다. PIE 재시작 시 생성자 기본값으로 리셋됨. 피치는 줌에 연동되므로 표기한 "~각도"는 기본 줌 기준 근사치.

---

### 프로젝트 진척 / 도감 (OfficeMap 전용)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `Proj_Seed [Count] [Review]` | 현재 티어까지의 프로젝트를 앞에서부터 `Count`개(기본 5) **개발 완료**로 기록하고 출시 기록도 함께 생성. `Review` 0(기본)이면 12~34 랜덤, 값을 주면 그 평점 고정 | `Proj_Seed 12` |
| `Proj_SetTier [Tier]` | 현재 티어 강제 설정 (1~10) | `Proj_SetTier 3` |
| `Proj_ClearHistory` | 개발 이력 + 출시 기록 초기화. 도감 마일스톤 지급 기록도 비움 | `Proj_ClearHistory` |
| `Proj_State` | 산업 / 티어 / 개발이력 수 / 출시기록 수 + 개발한 인덱스 목록 출력 | `Proj_State` |

**용도.** `[이전 프로젝트]` 패널과 포트폴리오(도감)를 **실제 플레이 없이** 검증한다. 그냥 두면 프로젝트를 여러 개 실제로 굴려야 리스트에 뭐가 뜨는지 볼 수 있다.

**권장 순서 — 두 종류가 섞인 화면을 보려면 티어를 먼저 올린다.**
```
Proj_SetTier 3     // 티어 1~2 가 '지나온 티어'가 됨
Proj_Seed 12       // 앞 12개를 개발 완료로 → 재개발 행 12개
Proj_State         // 확인
```
이러면 `[이전 프로젝트]` 리스트에 **재개발**(개발함) 행과 **첫 개발**(지나온 티어 미개발) 행이 함께 뜬다.
빈 상태를 보려면 `Proj_ClearHistory` 후 `Proj_SetTier 1`.

**주의.** `Proj_Seed` 는 **DT 미정의 조합을 건너뛴다** — 패널이 `HasAffinityCombo` 로 그런 행을 걸러내므로, 시드에 포함하면 화면에 안 보이는 이력만 쌓여 "치트가 안 먹는다"로 오인하게 된다. 그래서 요청한 `Count` 보다 적게 기록될 수 있고, 실제 기록 건수를 로그로 찍는다.

### 진행 상태 프리셋 (UDevPresetSeeder)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `Preset_Apply [Row]` | 중반 진행 상태 프리셋을 인세션 재적용. 부지·빌딩·직원·이력·운영중 프로젝트·월드맵 생산·자원을 정해진 순서로 시드한다. 비우면 CG Dev 설정값 | `Preset_Apply Late` |
| `Preset_Verify [Row]` | 현재 상태가 프리셋과 정합한지 단언하고 실패 항목을 나열. 실패 0건이면 시드 성공. 비우면 CG Dev 설정값 | `Preset_Verify` |

**용도.** `Project Settings > Game > CG Dev > 시작 모드 = 진행 상태 프리셋` 으로 시작하면 자동 적용되지만,
값을 바꿔가며 확인할 때 매번 PIE 를 재시작하지 않으려면 `Preset_Apply` 로 인세션 재적용한다.
`Preset_Verify` 는 **시드가 반쯤 실패한 상태를 잡아내는 용도** — 부지 미스폰이나 DT 행 누락은
전부 무음 부분 실패라 로그를 안 보면 모르고 지나간다.

**프리셋 3종.** `Early`(티어2) / `Mid`(티어5) / `Late`(티어8·대기업).
`TargetTier` 가 앵커라서 이력 개수와 빌딩 레벨 하한이 거기서 역산된다.

**주의.** 회사 등급은 프리셋이 직접 대입하지 않는다 — 시총을 마지막에 넣어 실제 승격 조건을 밟게 한다.
그래서 `Preset_Verify` 에서 **등급 항목이 FAIL 이면 프리셋 값 자체가 부정합**하다는 뜻이다
(빌딩 수·업종 수·이력 건수 중 뭔가 모자란다). `Late` 위 등급은 `ManagerCount` 조건에
도달 경로가 없어 대기업이 상한이다.

### 직원 이동 속도 / 로코모션 (OfficeMap 전용)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `WorkerBolt [Count]` | 번아웃 배회를 즉시 발동 — 피로 누적을 기다리지 않고 러닝 모션 확인. 생략 시 전원, 숫자를 주면 앞 N명만 (걷는 직원과 나란히 비교). **Stage 모드 직원만 대상** (아니면 발동 0명 경고) | `WorkerBolt 3` |
| `WorkerSpeed [Value]` | 전 직원 `MaxWalkSpeed` 직접 지정. 블렌드스페이스 축을 훑어 전환 지점을 찾는 용도. `-1` 이면 원래 값 복귀 | `WorkerSpeed 320` |
| `WorkerSpeedStatus` | 직원별 모드 / `MaxWalkSpeed` / 실측 속도 출력. ABP 의 `Speed` 축에 값이 실제로 들어가는지 확인 | `WorkerSpeedStatus` |
| `BoltStatus [판초]` | 폭주 확률 진단. 직원별 **유효침착성 / 실효 초당 확률 / 판당 확률 / 모드** + 사무실 **판당 기대 폭주 인원** 출력. 판초 생략 시 15초(`DefaultStepDuration`) 기준. **강제 발동(`WorkerBolt`)으로는 확률 튜닝을 검증할 수 없어**(표본 수십 회 필요) 계산값을 직접 보여주는 진단 치트다. 확률은 `GetEffectiveBoltChance()` 를 그대로 읽으므로 본체 공식과 어긋나지 않는다 | `BoltStatus` / `BoltStatus 30` |

**동작 구조.** `UEmployeeAnimInstance::Speed` = 실측 속도(`GetVelocity().Size2D()`)이고, 이 값이 `ABP_StickWorker` → `EmployeeLocomotion` → `Walking` 스테이트의 `BS_StickLocomotion` 축에 들어간다. 블렌드스페이스 샘플은 걷기 125 / 느린런 250 / 보통런 400 / 빠른런 550.

**직원이 뛰는 상황은 하나뿐이다 — 번아웃 배회(`EFatigueSlackPhase::Bolting`).** 피로가 Slacking 에 도달 → 텔레그래프(캐치 윈도우) → 늘어짐 → 미캐치 시 드물게(`BoltChance`) 자리를 박차고 뛰쳐나가는 구간이며 **Stage 중에만** 발생한다. 행동 모드(Idle/Operation/Stage) 자체로는 속도가 바뀌지 않는다.

속도 값은 `UEmployeeBehaviorComponent` 의 `MoveSpeedWalk`(평상시) / `MoveSpeedBolt`(뛰쳐나감) 두 개(`EditDefaultsOnly`)이고, `ApplyMoveSpeed()` 가 `EnterSlackBolt()` / `ExitSlackToWork()` 에서 전환한다. `AOfficeworker` 생성자가 `MaxWalkSpeed=125` 로 고정해두므로 이 런타임 덮어쓰기가 없으면 러닝 구간에 도달하지 못한다.

> **`WorkerSpeed` 는 임시값이다.** 슬랙 페이즈가 바뀌면 `ApplyMoveSpeed()` 가 되돌린다. 좋은 값을 찾았으면 `MoveSpeedWalk` / `MoveSpeedBolt`(BP Class Defaults)에 옮겨 적을 것.
>
> 클립이 전부 in-place(루트 모션 없음)라 발 미끄러짐은 **속도와 샘플 값이 맞을 때만** 잡힌다. `MoveSpeedWalk` 를 125 에서 떼면 걷기 샘플과 어긋나 미끄러지고, `MoveSpeedBolt` 를 250 아래로 내리면 러닝 샘플에 못 닿아 걷기로 보인다.

---

### 튜토리얼 / 미션 점프

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `SkipToMission [RowName]` | 임의 미션으로 즉시 점프 (선행 상태 시드 + 자원 듬뿍 + 빌딩 체크 포함) | `SkipToMission M3_EnterOffice` |
| `ResetTutorial` | 세이브 슬롯 삭제 — Stop 후 다시 Play 하면 깨끗한 M1부터 | `ResetTutorial` |

> `SkipToMission`의 RowName은 `DT_Mission` 기준. 시작 미션을 드롭다운으로 미리 지정하려면 `Project Settings > Game > CG Dev > 시작 미션`. `ResetTutorial`은 `SkipToMission` 누적/체인 완주로 `CurrentMissionID=None`이 된 세이브 복구용 (자원 시드 없음 — 신규 게임과 동일).

---

### 미션판 (UGoalBoardSubsystem)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `UnlockGoalBoard` | 미션판 즉시 언락 + 전수 재평가 + 저장. 튜토리얼 체인을 완주하지 않고 보드를 열어본다 | `UnlockGoalBoard` |
| `CompleteGoal [GoalRowName]` | 지정 미션을 강제 충족 처리 (`DT_Goal` RowName). **보상은 지급하지 않는다** — 수령은 보드 카드의 `[수령]` | `CompleteGoal G4_AcquireCompany` |
| `ResetGoals` | 미션판 상태 초기화 — 언락/충족/수령 전부 리셋 + 저장 | `ResetGoals` |
| `ResetPanelIntro` | 패널 최초 진입 안내(딤+구멍+말풍선)를 전부 안 본 상태로 되돌린다 + 저장. 직원창(`EmployeeWindow`) / 미션 목록(`MissionTracker`) 안내 재확인용. 캐치 힌트 졸업 카운트(`CatchDoze`/`CatchBolt`)도 함께 0. | `ResetPanelIntro` |

**미션 RowName** (`DT_Goal`): `G1_RaiseFloor` / `G2_HQLevel3` / `G3_SecondBuilding` / `G4_AcquireCompany` / `G5_DemolishCompany` / `G11_AcquirePlot` / `G6_BuildOnClearedPlot` / `G7_PlacePainting` / `G8_EquipSkin` / `G9_EnhanceStat` / `G10_EquipTrait`. 없는 이름을 주면 유효 목록과 함께 FAIL 로그가 뜬다.

> 정규 언락 지점은 튜토리얼 체인 마지막 미션 완료다. `UnlockGoalBoard`는 그 지점만 건너뛰며, 언락 직후 전수 재평가가 돌아 **이미 조건을 넘긴 도달형 미션(본사 레벨 / 건물 수)은 곧바로 수령 가능 상태**가 된다. 단발 이벤트형은 실제 행동을 하거나 `CompleteGoal`로 밀어야 하며, `G1_RaiseFloor`은 성공한 빌드업을 5회 누적한다.
> `CompleteGoal`은 충족 래치까지만 — 보상 지급/수령 플로우(토스트 포함)는 보드에서 눌러야 탄다. 선행 미션(`PrereqGoalID`)가 미수령이면 카드가 잠금 상태라 `[수령]`이 안 뜬다 (`G5`는 `G4` 수령 후, `G6`은 `G5` 수령 후).
> 세 치트 모두 결과를 즉시 저장하므로 PIE Stop 후 재시작해도 상태가 유지된다. `ResetGoals`는 이미 지급된 보상 자원까지 회수하지는 않는다.
> 미션 목록 코치마크를 다시 보려면 `ResetPanelIntro` → `ResetGoals` → `UnlockGoalBoard` 순. 트래커는 언락 시 새로 생성되면서 안내를 다시 재생한다(안내는 트래커가 접혀 있으면 재생하지 않는다).

---

### 도시 회사 인수 (UCityAcquisitionManager — MainMap)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `Acq_Buy [Key]` | 지정 BuildingKey의 회사를 인수 시도. 자금 충분하면 차감 + Milking 상태 전환 + 저장 | `Acq_Buy 5` |
| `Acq_State [Key]` | 지정 Key의 인수 상태 출력 (0=NotAcquired 1=Milking 2=Depleted 3=Cleared) | `Acq_State 5` |
| `Acq_Demolish [Key]` | 지정 Key의 회사 철거 = **캐시아웃**. `Milking`/`Depleted` 어디서든 동작 → 금고(판돈)를 Money 로 지급하고 즉시 `Cleared` | `Acq_Demolish 5` |
| `Acq_ClearAll` | 모든 도시 회사를 즉시 Cleared 로 (자금/현재 상태 무시). 건물 즉시 숨김(데모 연출 없음) + 옥상 비콘 제거 + 부지 구매 게이트 개방 + 저장 | `Acq_ClearAll` |
| `Acq_FillPot [Key] [Pct]` | `Milking` 회사의 회수율을 `Pct`%(기본 90)까지 즉시 진행. 금고만 차고 지갑은 무변동. 100 이상이면 `Depleted` 전환 | `Acq_FillPot 5` / `Acq_FillPot 5 100` |
| `Plot_OwnAll` | 스폰된 모든 도시 부지를 즉시 소유 (인접/자금/결제 게이트 우회) + 저장 + 가격 배지 갱신 | `Plot_OwnAll` |

> `Acq_Buy`는 `CanAcquire` 검사(상태+자금)를 통과해야 성공 → 성공 시 `1`, 실패 시 `0` 로그. BuildingKey는 스카이라인 빌딩 `BP_MB###` 번호 (1~40). 자금 부족 시 `GrantRes Rich` 또는 `Money` 로 충전 후 재시도.
> **금고(판돈) 모델** — 드립은 Money 로 직행하지 않고 회사 금고에만 쌓인다. 지갑 지급 지점은 `Demolish`(캐시아웃) 하나뿐이라 `Acq_FillPot` 은 회수율만 밀어 올리고 잔액을 안 건드린다. 총 회수액은 인수 시점에 `YieldMin~YieldMax` 로 굴려져 확정되며, 그 뒤로는 시간이 지나면 반드시 100% 까지 찬다 (파산 판정은 2026-08-07 폐지).
> **회수 완주 검증 레시피** = `Acq_Buy K` → `Acq_FillPot K 100`(즉시 `Depleted` 전환) → `Acq_Demolish K`(금고 전액 지급 확인). 티어가 높으면 자연 회수는 수 시간~24시간이 걸리므로 `Acq_FillPot` 이 사실상 필수다.
> **도시 통째 접수** = `Acq_ClearAll` (회사 다 정리 → 건물 사라짐) 후 `Plot_OwnAll` (땅 다 인수). `Acq_ClearAll`은 `Demolish`(층별 붕괴 연출 + 금고 캐시아웃)와 달리 연출/지급 없이 상태 무관하게 전 회사를 한 번에 즉시 숨김 처리하고, 회사별 `OnCompanyCleared` 를 브로드캐스트해 비콘/부지 게이트를 갱신한다. `Plot_OwnAll`은 `SpawnManager`가 캐시한 모든 부지를 `SetOwnedState(true)` 로 전환하며, 세이브는 `GatherOwnedPlotIds`(현재 `IsOwned()` 부지 수집) 단일 소스를 그대로 탄다.

---

### 백로그 수학 검증

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `CG_TestBacklogMath` | 백로그 순익 지수감쇠 수학 자가검증 — peak100 cost10 반감기600 기준 4샘플(t=0/60/600/3600) 로그 출력 | `CG_TestBacklogMath` |
| `CG_DumpBacklog` | 현재 관리 빌딩의 백로그 제품 전체(PID/경과시간/peak/cost/순익) + 빌딩 합산 순익 출력 | `CG_DumpBacklog` |

> 기댓값: t=0 ~90 / t=60 ~78.8 / t=600 ~40 / t=3600 ~0. `ComputeProductNetPerSec` 단위 검증용.
> `CG_DumpBacklog`는 OfficeMap에서 현재 관리 빌딩(`GetCurrentManagedBuildingIndex`)의 백로그 제품 전체를 순회 — 백로그 자동화 배선 검증용.

---

### UI 디버그 (UIBase 스택 / 입력모드)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `CG_DumpUI` | UIBase 3스택(Main/Prompt/Bottom) 내용 + 현재 `EInputMode` 출력 — 화면에 카운트 요약, 상세는 Output Log | `CG_DumpUI` |
| `CG_UnlockInput` | 입력모드가 UI로 잠겼을 때 강제 `GoToNormalMode()` 복원 (위젯은 건드리지 않음) | `CG_UnlockInput` |

> "창을 닫았는데(또는 안 보이는데) 카메라 이동/월드 탭이 안 먹는" 상황 = 위젯이 안 보이는 채 스택에 active로 남아 `EInputMode::UI`가 잔류한 것. `CG_DumpUI`로 어떤 위젯이 남았는지 확인 후 `CG_UnlockInput`으로 탈출 (2026-07-24 오피스 입력잠금 사고 진단용으로 추가).

---

### 시간대 / 낮밤 (ATimeCycleManager)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `Day` | 낮으로 고정 (12:00 세팅 + 사이클 정지 → 창문/차량 라이트 OFF) | `Day` |
| `Night` | 밤으로 고정 (00:00 세팅 + 사이클 정지 → 창문/차량 라이트 ON) | `Night` |
| `SetTime [Hour] [Minute]` | 임의 시각으로 고정 (시 0~23, 분 0~59 + 사이클 정지) | `SetTime 18` / `SetTime 6 30` |
| `TimeResume` | 시간 사이클 재개 (고정 해제 → 다시 시간이 흐름) | `TimeResume` |

> 시간만 세팅하면 `UWindowLightManagerSubsystem`이 매 틱 그 시간을 읽어 `Night_Intensity`(0~1) MPC를 갱신 → 창문 발광/차량 라이트 등 **MPC 기반 룩만** 낮/밤으로 바뀐다 (씬 전체 라이팅/태양 각도는 변경 안 함).
> 밤/낮 기준은 WindowLightManager 자체 값(**일출 6시 / 일몰 21시**): 완전 낮 = 7~18시, 완전 밤 = 22~5시. `Day`=12시, `Night`=0시는 각 구간의 안전한 중앙값.
> 맵에 `ATimeCycleManager`가 배치되어 있어야 동작 (없으면 `[Cheat:Time] TimeCycleManager 없음` 경고). 내부적으로 `SetCurrentTime` + `PauseCycle`(고정) / `StartCycle`(재개).

---

### 밸런스 검증 계측 (`[BALV]` — Tools/Balance 하네스 전용)

| 명령어 | 설명 | 예시 |
|--------|------|------|
| `Balance_DumpDevScore` | 개발 중 프로젝트의 활성 직능 스텝별 `획득/목표` + 가중치 + 직능슬롯 + 품질점수 `q` + 등급 + 통과여부를 한 줄 덤프 | `Balance_DumpDevScore` |
| `Balance_DumpEconomy` | `ResourceBank` 전 재화 잔액 + 빌딩별 `실수익률/안정율/금고잔량/금고용량/남은운영시간` + 직원·방치 인원을 한 줄 덤프 | `Balance_DumpEconomy` |
| `Balance_SimOffline [Sec]` | 오프라인 정산을 모의 경과초로 **dry-run** — 세이브·금고·운영 경과를 전혀 바꾸지 않고 계산 결과만 출력 | `Balance_SimOffline 3600` / `Balance_SimOffline 21600` |
| `Balance_GateRun [N] [Proj] [Dir]` | 게이트 통과율(A2) **N판 자동 반복**. 미착석 직원 자동 착석 → 착수 → 무탭 진행 → 판정 emit → 종료 → 다음 판. 기본 N=30, Proj=1, Dir=0(표준) | `Balance_GateRun 30` / `Balance_GateRun 30 1 0` |
| `Balance_GateRunStop` | 진행 중인 `Balance_GateRun` 즉시 중단 (그때까지의 `GateRun` 라인 + `GateRunDone n=실제완주수` 는 유효) | `Balance_GateRunStop` |

**출력 계약 (파서 스펙 — 바꾸면 하네스가 조용히 빈 측정치를 낸다):**

```
[BALV] <Name> k1=v1 k2=v2 ...
```

한 줄, 공백 구분 `키=값`, 값은 숫자이거나 `획득/목표` 슬래시쌍. `UE_LOG(LogTemp, Display, ...)` 로 나가므로
Output Log 에서 `BALV` 로 필터하거나 `Saved/Logs/CompanyGrowthRenewal.log` 를 파싱한다.

출력 예시:

```
[BALV] DevScore proj=3 tier=1 steps=3 emp=4 s0=48.2100/120.0000 w0=5 d0=0 s1=31.5000/72.0000 w1=3 d1=1 s2=19.8000/48.0000 w2=2 d2=5 q=0.7431 grade=C pass=0 running=1 remain=8.30 total=15.00 elapsed=6.70 retry=0
[BALV] Economy Brick=120 Money=4820000 Diamond=310 ... DiamondOre=0 ops=2 blds=3 b0_op=1 b0_rate=812.4000 b0_stable=903.0000 b0_stored=45200.00 b0_cap=2168400.00 b0_remain=430.0 b0_earned=93400.00 b1_op=0 b1_rate=0.0000 b1_stable=0.0000 b1_stored=0.00 b1_cap=200.00 b1_remain=0.0 b1_earned=0.00 emp=12 empUnassigned=3 empOffice=4 empIdle=1
[BALV] SimOffline sec=3600 gain=1462320.0000 clipped=0.0000 elapsedAdvance=720.0000 blds=2 rows=2 skipped=0 liveOps=2 b0_raw=1462320.0000 b0_gain=1462320.0000 b0_loss=0.0000 b0_life=0 b0_vaultLv=4 ...
[BALV] GateRun idx=0 pass=1 q=1.2043 grade=B emp=5 typing=5 desk=5 timeout=0
[BALV] GateRunDone n=30 req=30 emp=5 proj=1 dir=0
```

- `s<i>` / `w<i>` / `d<i>` — i 번째 **활성 직능** 스텝(출시 스텝 제외, 인덱스 재부여)의 획득/목표, 가중치, `EProductionDiscipline` 슬롯
- `emp` (DevScore) — 스폰된 오피스 직원 수 = per-employee 산출 분모
- `b<Idx>_*` — 빌딩 인덱스별. `_cap` 은 `CalculateWarehouseCapacity`(운영 없으면 200 하한 폴백), `_stable` 은 감쇠 제외 안정율, `_earned` 은 운영 누적 매출(직원 드립 포함, 단조증가)
- 직원 드립 수익은 `Money` 로 직접 들어가고 금고(`_stored`)를 거치지 않는다 — A3(방치수익)는 `Money` 차분, A4(금고)는 `_stored` 차분으로 각각 봐야 귀속이 안 섞인다
- `empUnassigned` = 배치 안 된 직원(세이브 기준), `empIdle` = 오피스에서 `EEmployeeBehaviorMode::Idle` 로 도는 직원
- `clipped` = 금고 초과로 잘린 합, `elapsedAdvance` = 오프라인 동안 앞당겨지는 운영 경과(초, `OfflineSeconds × 0.2`)
- `skipped=1` = 실지급 경로였다면 60초 미만이라 통째로 스킵됐을 구간 (dry-run 은 그래도 계산값을 낸다)
- `liveOps` = 메모리상 활성 운영 수. **`liveOps>0` 인데 `blds=0` 이면 세이브가 스테일** — 정산은 세이브의 `CurrentOperation` 스냅샷을 읽는데 그건 `SaveGameData()` 때만 갱신되므로, 착수 직후엔 저장을 한 번 태우고 다시 호출할 것 (조용한 0 측정 방지용 신호)

**`Balance_GateRun` 만의 주의점 (A2 자동 반복):**

- **Game 산업 빌딩 전용.** `DT_Project_{IT,Finance,Electronics,Automobile,Semiconductor}` 는 `Weight_*` 6컬럼이 전부 0이라 활성 직능 스텝이 안 생긴다(`steps=0`) → 개발점수 축 자체가 없어 통과율이 무의미하다.
- **자동 착석이 전제.** 책상 미배정 직원은 Stage 전환에서 텔레포트에 실패해 Idle 로 되돌아가고 기여 루프가 통째로 안 돈다(점수가 "조용한 0"). 치트가 시작 시 `UOfficeManager::AutoSeatEmployee` 로 미착석 직원만 앉힌다. 배치된 책상이 하나도 없으면 **아무 `[BALV]` 도 안 내고 경고만** 남기고 중단하고, 좌석이 모자라 일부만 앉으면 `⚠ 좌석 부족` 경고를 남긴다.
- ⚠ **착석 판정은 좌석 역조회(`FindWorkstationByEmployeeID`)로만 한다.** `FEmployeeInstance::bIsAssigned` 는 "이 빌딩에 배치됨" 플래그지 "책상에 앉음" 이 아니다 — 그걸로 판정하면 전원이 '기존 착석' 으로 스킵돼 AutoSeat 가 한 번도 안 돌고, 좌석 없이 30판이 조용히 0점을 낸다(2026-08-02 실측 사고).
- **`typing=` / `desk=` 가 표본 유효성 지표다.** 각 판에서 (책상 보유 && Stage && Typing) 인 인원의 최대치. `typing=0` 이면 아무도 기여하지 않은 판이라 `pass=0` 은 밸런스가 아니라 셋업 실패다 — 통과율 분모에 넣으면 안 된다(파서가 자동 제외).
- **모달 자동 해소.** 개발 ~40% 지점의 부스트 도박은 `ResolveBoostGamble(false)`(항상 안전), 트레이트 이벤트는 `HandleEventChoice(0)`(항상 첫 선택지)로 고정 처리한다. 이게 없으면 `SimultaneousTimerTick` 이 카운트다운을 건너뛰어 타이머가 영구 정지한다. **정책을 바꾸면 무탭 기준선이 흔들리므로 고정.**
- **실행 중 화면을 건드리지 말 것** — 탭 입력이 섞이면 무탭 측정이 아니게 된다. `slomo 10` 으로 가속 가능(폴링이 월드 타임 기준이라 판정 지연이 안 늘어난다).
- **개발비가 판마다 나간다.** Project001(Game) 기준 700 × N. 부족해지면 착수가 조용히 실패하는데, 치트가 `IsStageInProgress()` 로 잡아 중단 + 사유 로그를 남긴다. 미리 `Money 1000000` 을 권장.
- `timeout=1` 인 판은 예산(듀레이션×3+30초) 초과로 강제 종료된 판 = **신뢰할 수 없는 표본**. `GateRunDone` 의 `n` 과 `req` 가 다르면 부분 실행이다.

> `Balance_SimOffline` 은 `USaveLoadManager::ComputeOfflineGains(Sec, bApplyToSave=false, ...)` 를 호출한다.
> 실지급 경로(`CalculateOfflineGains`)가 같은 함수를 `bApplyToSave=true` 로 부르므로 **계산식이 갈릴 수 없다** — 계측이 곧 실경로 검증.
> 재화 키 순서는 `EResourceType` enum 순회라 세션 간 고정 (TMap 순회 순서는 비보장이므로 쓰지 않는다).

---

## 테스트 시나리오 예시

### 1. 경험치 시스템 테스트

```
ExpStatus                  // 현재 상태 확인
Exp 50                     // 선택 직원에게 50 EXP
ExpStatus                  // 변경 확인
Exp 500                    // 대량 추가 → 레벨업 가능 확인
```

### 2. Step/Operation 경험치 확인

```
ExpStatus                  // 초기 상태
CompleteStep               // Step 완료 → 경험치 분배
ExpStatus                  // Step 완료 후 경험치 증가 확인
```

### 3. 품질 등급별 배율 테스트

```
ExpStatus                  // 초기 상태
DistExp 100 0              // F등급 (0.3x = 30 EXP)
ExpStatus                  // 확인
DistExp 100 5              // S등급 (1.5x = 150 EXP)
ExpStatus                  // 확인
```

### 4. 직원 직급 테스트

```
ListEmp                    // 직원 선택 (3D 클릭 또는 UI)
SetRank 6                  // 부장으로 설정
ListEmp                    // 변경 확인
```

### 5. 운영 즉시 완료 테스트

```
OpStatus                   // 진행 중 운영 확인
SetOpTime 0                // 현재 빌딩 운영 남은 시간 0 → 다음 틱 완료
OpStatus                   // 완료 처리 확인
```

### 6. 건물/본사 레벨 점프

```
SetHQLv 8                  // 본사 8레벨
HQLevelUp                  // 본사 +1 (조건 무시)
```

### 7. 가챠/판매/특성 인벤 시드

```
Ticket 30 2                // 프리미엄 채용권 30장
SeedProducts 50            // 판매 인벤 50슬롯 시드 (WorldMap)
SeedTraits 60              // 특성 60종 시드
TraitStatus                // 특성 보유 현황 확인
```

### 8. 채광 시스템 동작 검증

```
// 호주 채광 라인 강제 생성 (CountryDetail 진입 안 해도 OK)
MineFill Australia         // 라인 ensure + 모두 한도까지 채움
MineDebug                  // [Mine Australia] IronOre: 200/200 (100%) Sat=true 확인

// 강화 효과 검증
MineSetLv Australia Storage 5    // 한도 100 -> 600
MineSetLv Australia Rate 10      // 분당 채취량 +150%
MineDebug                  // 새 한도/배율 반영 확인

// SaveLoad + 오프라인 catchup
MineDebug                  // 현재 상태 기록
// (게임 종료 후 30분 대기 후 재시작)
MineDebug                  // 자동으로 누적된 양 확인 (8h 캡 내)
```

### 9. 낮/밤 룩 고정 (창문·차량 라이트 확인)

```
Night                      // 00:00 고정 → 창문 발광/차량 라이트 ON
Day                        // 12:00 고정 → OFF
SetTime 20                 // 황혼 구간 그라데이션 확인
TimeResume                 // 고정 해제, 다시 시간 흐름
```

### 10. 뽑기권 풀 플로우 + 튜토리얼 점프

```
TraitTicket 50 -1          // 특성 뽑기권 Normal+Advanced 50장씩 → 가챠 패널 테스트
SkinTicket 50 -1           // 스킨 뽑기권 50장씩
SkipToMission M6_FirstDesk // 특정 미션 상태로 점프 (선행 시드 포함)
```

---

## 로그 확인

Output Log 창에서 결과 확인:
- `Window > Developer Tools > Output Log`
- `LogCGCheat` 또는 `LogTemp` 카테고리로 필터링

예시 출력:
```
[Cheat:Exp] 김철수 (ID:1): 0.0 -> 100.0 / 150 (Lv.1)
[Cheat:Exp] >>> LEVEL UP AVAILABLE!
```

---

## 파일 위치

- 헤더: `Public/Player/CGCheatManager.h`  ← **명령어의 단일 진실 소스** (`UFUNCTION(Exec)` 목록)
- 구현: `Private/Player/CGCheatManager.cpp`
- 본 문서: `docs/07_Reference/CHEAT_COMMANDS.md`
- 치트 시트(HTML): `docs/07_Reference/CHEAT_COMMANDS.html`

## 새 명령어 추가 방법

1. `CGCheatManager.h`에 함수 선언:
```cpp
UFUNCTION(Exec)
void MyNewCheat(int32 Param);
```

2. `CGCheatManager.cpp`에 구현:
```cpp
void UCGCheatManager::MyNewCheat(int32 Param)
{
    UE_LOG(LogCGCheat, Warning, TEXT("[Cheat:MyNewCheat] Param: %d"), Param);
    // 로직 구현
}
```

3. 빌드 후 콘솔에서 `MyNewCheat 123` 사용
4. **본 문서와 `CHEAT_COMMANDS.html`에도 새 명령어 한 줄 추가** (헤더와 문서 동기화)

> **자동 동기화 가드**: `CGCheatManager.h`/`.cpp`를 편집하면 PostToolUse 훅이 `Tools/CheatDoc/verify_cheat_docs.py`를 자동 실행해, 헤더의 `UFUNCTION(Exec)` 명령 중 본 문서 또는 HTML에 빠진 게 있으면 경고를 띄운다. 경고가 뜨면 위 4번을 수행할 것. 수동으로 점검하려면:
> ```
> py -3 Tools/CheatDoc/verify_cheat_docs.py
> ```
