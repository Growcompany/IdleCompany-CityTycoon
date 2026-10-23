// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficManager.generated.h"

class USplineComponent;
class UStaticMesh;
class UMaterialInterface;
class UInstancedStaticMeshComponent;
class ATimeCycleManager;

// 레벨의 모든 ATrafficRoute(스플라인)를 모아 ~50대의 인스턴싱 차량을 주행시키는 매니저.
// - 렌더: 모델당 ISM 1개 + 라이트카드 ISM 2개(앞/뒤) → 대수와 무관하게 드로우콜 ≈ 모델수+2.
// - 주행: 룩어헤드 조향 + 각속도 제한(보트 무버 로직 이식). 매니저 1틱이 전 차량 인스턴스 갱신.
// - 카메라 풀: 화면에 보이는 차가 줌 비례 목표(MinActiveCars~MaxActiveCars)가 되도록 활성 대수를 조절, 포커스(플레이어 폰) 둘레 줌 비례 반경 안의 차만 표시(밖은 0스케일 숨김).
//   그래프 모드는 멀리 벗어난 숨김 차를 화면 밖 스폰 밴드로 재배치 → 같은 대수로 카메라 주변 밀도 유지.
// - 밤 라이트: 라이트 ISM 의 머티리얼(M_CarLight)이 MPC Night_Intensity 로 밤에만 발광(동적 광원 0).
UCLASS()
class COMPANYGROWTHRENEWAL_API ATrafficManager : public AActor
{
	GENERATED_BODY()

public:
	ATrafficManager();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 풀 크기 — 이 중 활성 차만 주행·표시하고 나머지는 정지·숨김으로 대기.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic", meta = (ClampMin = "0"))
	int32 TotalCars = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic", meta = (ClampMin = "0.0"))
	float MinSpeed = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic", meta = (ClampMin = "0.0"))
	float MaxSpeed = 1600.f;

	// 룩어헤드 거리(cm) — 클수록 코너를 미리 보고 완만하게 선회.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic", meta = (ClampMin = "0.0"))
	float LookAheadDistance = 1200.f;

	// 최대 선회 각속도(deg/s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic", meta = (ClampMin = "1.0"))
	float MaxYawRate = 120.f;

	// 경사 추종 최대 각속도(deg/s). 엣지 전환 순간 pitch 가 튀지 않게 제한.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic", meta = (ClampMin = "1.0"))
	float MaxPitchRate = 60.f;

	// 코너 둥글리기 반경(cm). 노드 앞뒤로 이 거리를 2차 베지어로 잇는다. 0 = 폴리라인(구 동작).
	// 엣지 파라미터로는 2R 을 가는데 90도 코너의 베지어 길이는 약 1.7R 이라 코너에서 저절로 느려진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Feel", meta = (ClampMin = "0.0"))
	float CornerRadius = 450.f;

	// 90도 선회에서 순항 속도의 몇 배까지 줄일지. 1 = 코너 감속 없음.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Feel", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float CornerSpeedFactor = 0.55f;

	// 코너 앞 몇 cm 부터 감속을 시작할지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Feel", meta = (ClampMin = "0.0"))
	float CornerBrakeDistance = 900.f;

	// 가속/감속 한계(cm/s^2). 속도를 목표로 서서히 옮겨 간격 변화의 멈칫거림을 없앤다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Feel", meta = (ClampMin = "1.0"))
	float MaxAccel = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Feel", meta = (ClampMin = "1.0"))
	float MaxBrake = 1800.f;

	// 앞차와 이 거리 안이면 간격에 비례해 감속(위치 클램프는 안전망으로 남는다).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Feel", meta = (ClampMin = "1.0"))
	float FollowSlowDistance = 1500.f;

	// 선회 시 차체 기울기(도). 바깥쪽으로 기운다 - 반대로 보이면 음수로.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Feel")
	float MaxBankAngle = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Feel", meta = (ClampMin = "1.0"))
	float MaxRollRate = 90.f;

	// 트래픽 시뮬 갱신 주기(초). 0=매 프레임. 기본 30Hz — 차속 1000~1600cm/s라 33ms 스텝(33~53cm)은 도시 줌에서 안 보이고, 매 프레임 정렬+인스턴스 업로드 비용을 절감(저사양).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic", meta = (ClampMin = "0.0"))
	float SimUpdateInterval = 0.0333f;

	// 차 메시 forward축(+Y) 정렬 보정. 차가 뒤로 가면 +90.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic")
	FRotator MeshYawOffset = FRotator(0.f, -90.f, 0.f);

	// 차 메시 바닥 정렬 후 미세 Z 보정(cm). 스플라인/그래프 노드를 도로 위에 그리므로 기본 0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic")
	float VehicleZOffset = 0.f;

	// 표시 반경 = 카메라-포커스 거리 × 이 값. 1.2 = 피치 -40~-48°·hFOV 52~58° 지면 자취를 덮는 하한(19.5:9 근접 줌 화면 모서리가 약 1.2배).
	UPROPERTY(EditAnywhere, Category = "Traffic|Pool", meta = (ClampMin = "0.1"))
	float ViewRadiusScale = 1.2f;

	// 재배치 출발 노드는 표시 반경 바깥 이 폭(cm) 안에서만 고른다 — 화면 밖이되 곧 들어올 거리.
	UPROPERTY(EditAnywhere, Category = "Traffic|Pool", meta = (ClampMin = "0.0"))
	float SpawnBand = 2000.f;

	// 재활용 반경 = 표시 반경 + 이 값(cm). SpawnBand 보다 커야 막 재배치된 차가 곧바로 다시 걸리지 않는다.
	UPROPERTY(EditAnywhere, Category = "Traffic|Pool", meta = (ClampMin = "0.0"))
	float RecycleMargin = 3000.f;

	// 재활용 반경 밖에 이 시간(초) 이상 머문 차만 재배치 — 줌·이동으로 경계를 오가는 차가 사라지지 않게.
	UPROPERTY(EditAnywhere, Category = "Traffic|Pool", meta = (ClampMin = "0.0"))
	float RecycleDelay = 1.f;

	// 재배치 엣지의 같은 차선 출발점 이 거리(cm) 안에 차가 있으면 다른 후보 — 출발점 겹침 방지.
	UPROPERTY(EditAnywhere, Category = "Traffic|Pool", meta = (ClampMin = "0.0"))
	float MinRespawnGap = 1500.f;

	// 완전 줌인에서 화면에 보일 목표 대수(그래프 모드; 루트 모드는 활성 대수) — 근접 화면은 좁아 적어야 밀도가 맞는다.
	UPROPERTY(EditAnywhere, Category = "Traffic|Pool", meta = (ClampMin = "0"))
	int32 MinActiveCars = 5;

	// 완전 줌아웃에서 화면에 보일 목표 대수. 풀(TotalCars·cg.Traffic.MaxCars)보다 크면 풀 크기로 잘린다.
	UPROPERTY(EditAnywhere, Category = "Traffic|Pool", meta = (ClampMin = "0"))
	int32 MaxActiveCars = 20;

	// 활성 상한 = 목표 + 이 값 — 여유분은 띠에서 화면으로 들어오는 중인 차 몫. 배수로 두면 30Hz 스텝마다 투입돼 화면이 넘친다(2026-10-10 실측).
	UPROPERTY(EditAnywhere, Category = "Traffic|Pool", meta = (ClampMin = "0"))
	int32 TransitAllowance = 2;

	// 투입 최소 간격(초) — 한 번에 몰려 들어오지 않게.
	UPROPERTY(EditAnywhere, Category = "Traffic|Pool", meta = (ClampMin = "0.0"))
	float ActivateInterval = 1.f;

	// 이 카메라-포커스 거리(cm) 이하 = 완전 줌인. 리그 최근접(팔 5000 + 소켓 후퇴 ≈ 6208)에 맞춘 값.
	UPROPERTY(EditAnywhere, Category = "Traffic|Pool", meta = (ClampMin = "0.0"))
	float ZoomNearDistance = 6000.f;

	// 이 카메라-포커스 거리(cm) 이상 = 완전 줌아웃. 리그 최원(팔 20000 + 소켓 후퇴 ≈ 21202)에 맞춘 값.
	UPROPERTY(EditAnywhere, Category = "Traffic|Pool", meta = (ClampMin = "0.0"))
	float ZoomFarDistance = 21000.f;

	// [디버그] 켜면 카메라 반경 게이팅 무시(모든 차 항상 표시 → 숨김 차만 옮기는 재활용도 멈춤) — 게이팅 vs 렌더링 문제 분리용.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Debug")
	bool bDebugIgnoreBuildingGating = false;

	// [디버그] 켜면 가시 차량 수·표시 반경·재배치 수를 주기적으로 로그.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Debug")
	bool bDebugLog = false;

	// [디버그] 켜면 도로 그래프를 PIE에 오버레이(노드=점, 엣지=선) — 그래프가 도로에 맞는지 시각 확인.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Debug")
	bool bDebugDrawGraph = false;

	// [디버그] 켜면 TotalCars 무시하고 '모든 엣지에 양방향 1대씩' 배치 → 전 도로 커버 테스트(빈 도로 식별).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Debug")
	bool bDebugFillAllEdges = false;

	// 라이트 글로우 카드 크기(차당 헤드2+테일2). 카드는 항상 카메라를 향함(빌보드)이라, 퍼지는 라디얼 글로우가 어느 각도서든 동그랗게 보임.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Light", meta = (ClampMin = "0.01"))
	float LightCardScale = 0.6f;

	// 멀리서도 라이트가 안 사라지게: 카메라 거리에 비례해 카드를 키워 '화면상 일정 크기' 유지. 이 거리(cm)에서 원본 LightCardScale.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Light", meta = (ClampMin = "100.0"))
	float LightRefDistance = 10000.f;

	// 거리 스케일 상한. 라이트 월드크기 = LightCardScale × clamp(카메라거리/LightRefDistance, LightMinFactor, 이 값). 15→3: 15는 ~9m additive 쿼드 ×200장 = 모바일 fill 주범이었음.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Light", meta = (ClampMin = "1.0"))
	float LightMaxGrow = 3.f;

	// 거리 스케일 하한 — 멀어도 카드가 서브픽셀로 안 작아지게(쿼드 커버리지 팝핑=원거리 차 라이트 깜빡임 원인). 가로등 LampMinFactor와 동일 원리.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Light", meta = (ClampMin = "1.0"))
	float LightMinFactor = 1.5f;

	// 라이트 카드를 카메라 쪽으로 미는 거리(cm) — 빌보드 절반이 차체 폴리곤에 파묻혀 z-테스트로 잘리며 깜빡이는 것 방지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Light", meta = (ClampMin = "0.0"))
	float LightCardCamPush = 25.f;

	// 발광 카드의 가로/세로 비. 1 = 정원, >1 = 가로로 납작한 타원(실제 전조등 모양).
	// 비균일 스케일이라 회전은 MakeFromZY(월드업) 로 수평축을 고정해야 타원이 안 돌아간다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Light", meta = (ClampMin = "0.1"))
	float LightCardAspect = 1.8f;

	// 카메라가 이 거리(cm) 이상 움직인 프레임에만 라이트 빌보드 재조준(시뮬 30Hz와 독립, 정지 시 비용 0). 가로등 LampMoveThreshold와 동일 원리.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Light", meta = (ClampMin = "0.0"))
	float LightMoveThreshold = 1.f;

	// 차 모델 메시(생성자에서 7종 기본 로드, 에디터 override 가능).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Assets")
	TArray<UStaticMesh*> CarModels;

	// 모델별 추가 yaw 보정(도). CarModels 와 같은 인덱스. 에셋 팩마다 메시의 앞 방향이 달라
	// MeshYawOffset 하나로는 일부 차가 뒤를 향한다 - 그 모델만 180 을 넣는다. 비면 전부 0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Assets")
	TArray<float> ModelYawOffsets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Assets")
	UStaticMesh* LightCardMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Assets")
	UMaterialInterface* HeadLightMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Assets")
	UMaterialInterface* TailLightMaterial;

	// --- 도로 그래프 모드 (Python 툴이 채우면 자동 사용; 비면 ATrafficRoute 스플라인 모드) ---
	// 교차로=노드(월드좌표). 도로구간=엣지. 차가 엣지를 달리다 교차로에서 다음 엣지를 랜덤 선택(U턴 금지).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Graph")
	TArray<FVector> GraphNodes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Graph")
	TArray<FIntPoint> GraphEdges;

	// 우측통행 안쪽 차선 오프셋(cm) — 중앙선에서 진행방향 오른쪽 첫 차선까지.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Graph", meta = (ClampMin = "0.0"))
	float LaneOffset = 150.f;

	// 같은 방향 두 차선 간격(cm). 4차선 = 안쪽(LaneOffset) + 바깥쪽(LaneOffset+LaneWidth).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Graph", meta = (ClampMin = "0.0"))
	float LaneWidth = 300.f;

	// 같은 엣지·차선 앞차와 유지할 최소 간격(cm) — 차 겹침 방지(car-following). 차 길이+여유.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Graph", meta = (ClampMin = "0.0"))
	float MinCarGap = 650.f;

	UPROPERTY(VisibleAnywhere, Category = "Traffic")
	USceneComponent* Root;

private:

	// 모델당 1개 ISM (같은 모델 차량은 한 ISM 의 인스턴스로).
	UPROPERTY()
	TArray<UInstancedStaticMeshComponent*> BodyISM;

	UPROPERTY()
	UInstancedStaticMeshComponent* FrontLightISM;

	UPROPERTY()
	UInstancedStaticMeshComponent* RearLightISM;

	UPROPERTY()
	TArray<USplineComponent*> Routes;

	TArray<bool> RouteReverse;

	// 모델별 바닥 보정값(-BoundsMin.Z) — 스플라인 위에 바퀴가 닿도록 들어올림.
	TArray<float> ModelGroundLift;

	// 모델별 라이트 로컬좌표 — 차 메시 "Color Bloom" 발광면에서 추출(헤드=앞 흰색, 테일=뒤 빨강, 보통 각 2개).
	TArray<TArray<FVector>> ModelHeadlights;
	TArray<TArray<FVector>> ModelTaillights;

	struct FTrafficCar
	{
		int32 RouteIndex = 0;
		int32 ModelIndex = 0;
		int32 BodyInstance = 0;
		TArray<int32> HeadInst;   // 헤드라이트 인스턴스(FrontLightISM)
		TArray<int32> TailInst;   // 테일라이트 인스턴스(RearLightISM)
		float Distance = 0.f;
		float Speed = 0.f;
		float Yaw = 0.f;
		float Pitch = 0.f;
		float RouteLength = 0.f;
		bool bReverse = false;
		bool bVisible = false;
		bool bActive = false;   // 줌 비례 활성 대수 밖이면 정지·숨김
		FVector LastLoc = FVector::ZeroVector;   // 직전 스텝 위치 — 비활성화 대상(포커스에서 먼 차) 고르기용
	};
	TArray<FTrafficCar> Cars;

	FVector FocusLoc = FVector::ZeroVector;
	float ViewRadius = 0.f;
	FMatrix ViewProj = FMatrix::Identity;   // 직전 시뮬 스텝의 뷰·투영 — IsOnScreen 판정용
	bool bHasViewProj = false;
	bool bHasFocus = false;   // 카메라를 못 찾으면 전부 숨김(재활용도 정지)
	TArray<FIntPoint> SpawnCandidates;   // 재배치 후보 (출발 노드, 도착 노드) — 스텝 간 버퍼 재사용
	bool bSpawnCandidatesBuilt = false;   // 후보는 시뮬 스텝당 1회만 모은다(활성화·재활용 공용)
	float DesiredActiveCars = 0.f;   // 줌 비례 목표 대수(반올림 전; 그래프=화면 표시, 루트=활성) — UpdateFocus 가 갱신
	float ActivateCooldown = 0.f;   // 다음 투입까지 남은 시간(초)
	float DebugLogAccum = 0.f;
	int32 DebugRecycleCount = 0;
	float SimAccum = 0.f;   // 시뮬 스로틀 누적자(SimUpdateInterval 도달 시 1스텝).
	FVector CachedCameraLoc = FVector::ZeroVector;   // 라이트 빌보드용 — 매 틱 플레이어 카메라 위치 캐시.

	// 라이트 카드 매프레임 재조준 캐시 — 시뮬 스텝(30Hz)이 위치/기본스케일을 쓰고, 매 프레임 패스는 회전·거리스케일만 재계산.
	TArray<FVector> FrontCardPos, RearCardPos;   // ISM 인스턴스 인덱스로 정렬된 카드 월드 위치(카메라 push 적용 전)
	TArray<float> FrontCardBase, RearCardBase;   // 기본 스케일(숨김 카드 = 0 → 매프레임 패스가 스킵)
	FVector LastLightCamLoc = FVector(TNumericLimits<float>::Max());
	void RefreshLightBillboards(bool bRefreshHidden = false);

#if WITH_DEV_AUTOMATION_TESTS
	friend struct FTrafficLightUpdateFixture;
	friend struct FTrafficTurnFixture;
#endif

	// day/night 전환 시 라이트 카드 ISM(헤드/테일)만 가시성 토글 — 낮엔 숨겨 fill 절감. 차체(BodyISM)는 토글 안 함.
	UFUNCTION()
	void HandleVisualTimeChanged();
	void ApplyNightVisibility(bool bNight);

	TWeakObjectPtr<ATimeCycleManager> CachedTimeCycle;
	bool bGlowVisible = true; // 시계 못 찾으면 표시 유지(밤 라이트 회귀 방지)

	void BuildTraffic();
	void UpdateFocus();
	bool IsInView(const FVector& WorldLoc, bool bWasVisible) const;
	bool IsOnScreen(const FVector& WorldLoc) const;
	void UpdateCar(FTrafficCar& Car, float DeltaTime);
	void AdjustActiveCars(float DeltaTime);

	// --- 공유 헬퍼 (스플라인/그래프 모드 공통) ---
	void CreateInstanceComponents();
	void ApplyCarInstances(int32 ModelIdx, int32 BodyInst, const TArray<int32>& HeadInsts, const TArray<int32>& TailInsts, const FVector& WorldLoc, float WorldYaw, bool bVisible, bool bWasVisible, float WorldPitch = 0.f, float WorldRoll = 0.f);

	// --- 그래프 모드 ---
	bool bGraphMode = false;
	TArray<TArray<int32>> Adjacency;   // 노드 → 이웃 노드 인덱스
	struct FGraphCar
	{
		int32 FromNode = 0;
		int32 ToNode = 0;
		int32 NextNode = 0;     // 다음다음 노드(미리 선택 → 교차로 전 회전 예측)
		int32 ModelIndex = 0;
		int32 BodyInstance = 0;
		TArray<int32> HeadInst;   // 헤드라이트 인스턴스(FrontLightISM)
		TArray<int32> TailInst;   // 테일라이트 인스턴스(RearLightISM)
		float Dist = 0.f;       // 현재 엣지 진행 거리
		float EdgeLen = 1.f;
		float Speed = 0.f;
		float CruiseSpeed = 0.f;  // 이 차의 순항 속도 - Speed 는 가/감속으로 이 값을 따라간다
		float Yaw = 0.f;
		float Pitch = 0.f;
		float Roll = 0.f;
		int32 PrevNode = 0;      // 코너 진입 반쪽을 그리려면 노드 하나 전이 필요
		float LaneOff = 0.f;    // 이 차의 차선 오프셋(안쪽/바깥쪽)
		uint32 Rng = 0;         // 차별 결정적 회전 RNG
		float SpacingGap = 1e9f; // 같은 엣지·차선 앞차와의 간격(car-following) — 매 틱 계산
		bool bVisible = false;   // 직전 스텝 가시 여부 — 숨김→숨김이면 인스턴스 쓰기 스킵.
		float OutOfRangeTime = 0.f;   // 재활용 반경 밖에 연속으로 머문 시간(초)
		bool bActive = false;   // 줌 비례 활성 대수 밖이면 정지·숨김(정렬·간격·LOD 계산에서도 제외)
		FVector LastLoc = FVector::ZeroVector;   // 직전 스텝 위치 — 비활성화 대상 고르기용
	};
	TArray<FGraphCar> GCars;
	void BuildGraphTraffic();
	void UpdateGraphCar(FGraphCar& Car, float DeltaTime);
	int32 PickNextNode(int32 FromNode, int32 AtNode, uint32& Rng) const;
	bool TryRespawnIntoBand(int32 CarIdx);
	void RecycleGraphCars();
	void RespawnGraphCar(FGraphCar& Car, int32 FromNode, int32 ToNode);
};
