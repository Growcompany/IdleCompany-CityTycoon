// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GachaCaptureStage.generated.h"

class USceneCaptureComponent2D;
class USceneComponent;
class USpotLightComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UMaterialInterface;
class UTextureRenderTarget2D;
class AStickOfficeworker;
struct FEmployeeInstance;

// 무대 위 애니 모드 — 가챠 리빌=랜덤 댄스, 직원창 히어로 라이브 뷰=제자리 걷기
enum class EStageAnimMode : uint8
{
	Dance,
	Walk,
	// 오버라이드 없음 — MOVE_None 에서 ABP 기본 아이들(숨쉬기). 직원창 히어로 뷰 기본(제자리 걷기는 러닝머신처럼 어색)
	Idle
};

/**
 * 직원 라이브 뷰용 격리 캡처 무대 (가챠 리빌 + 직원창 히어로 공용).
 * 먼 곳(Z=100000)에 스폰 + SceneCapture 의 PRM_UseShowOnlyList 로 라이브 맵을 제외하고
 * 대상 직원(코스메틱 적용)만 찍어 RenderTarget 으로 만든다. RT 알파는 반전(캐릭터=투명) —
 * 소비 위젯이 M_GachaRevealRT(OneMinus a)로 보정해 "캐릭터만 불투명 + 배경 투명" 컷아웃 표시.
 * 캡처는 소비 위젯이 열려있는 동안만 ON(ReleaseShared/EndReveal 에서 정리) — 모바일 성능.
 * 라이브 캡처 무대는 월드 동시 1개 원칙 — AcquireShared/ReleaseShared 로 소비처가 공유(새 홀더가 인계).
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API AGachaCaptureStage : public AActor
{
	GENERATED_BODY()

public:
	AGachaCaptureStage();

	// 결과로 워커 스폰 + 코스메틱 + 애니(댄스/걷기) 시작 + 캡처 ON. 표시용 RT 반환.
	UTextureRenderTarget2D* BeginReveal(const FEmployeeInstance& Employee, EStageAnimMode AnimMode = EStageAnimMode::Dance);

	// 멀티 뽑기: 슬롯 순서(C,R1,L1,R2,L2) 직원 N명을 한 무대에 세우고 와이드 RT 1장 반환.
	// 카메라 1대 + 카드별 UV 윈도우 — 매프레임 SceneCapture 는 월드 1개 원칙(모바일).
	UTextureRenderTarget2D* BeginRevealBatch(const TArray<FEmployeeInstance>& EmployeesInSlotOrder,
		EStageAnimMode AnimMode = EStageAnimMode::Dance);

	// 슬롯 k(0=C,1=R1,2=L1,3=R2,4=L2)의 RT UV 윈도우 (OffU,OffV,ScaleU,ScaleV). NumSlots=1이면 풀 윈도우.
	// 기하 산식 소유자 — InGroundLineV 는 반드시 라이브 리그에서 잰 값을 넘겨야 한다(기본 인자 없음).
	// 인스턴스 메서드인 이유 = 열 피치가 BP 편집 가능한 인스턴스 값이라 static 이면 볼 수 없다.
	// 카드/위젯 등 소비처는 이걸 직접 호출하지 말고 GetLiveSlotUVWindow 로 무대가 실제로 배치에 쓴 창을 받을 것.
	FVector4 GetSlotUVWindow(int32 SlotIndex, int32 NumSlots, float InGroundLineV) const;

	// BeginRevealBatch 가 워커를 실제로 세울 때 산출한 슬롯 창 — 무대와 카드의 프레이밍 권위를 하나로 묶는다.
	// 배치 미구성(단발/미호출)이면 풀 윈도우.
	FVector4 GetLiveSlotUVWindow(int32 SlotIndex) const;

	// [다시뽑기]/선택 변경: 기존 워커 destroy 후 새 대상으로 재구성(RT 재사용).
	UTextureRenderTarget2D* Rebuild(const FEmployeeInstance& Employee, EStageAnimMode AnimMode = EStageAnimMode::Dance);

	// 위젯 닫힐 때: 캡처 OFF + 워커/무대 정리.
	void EndReveal();

	// 살아있는 공유 무대를 인계받거나 스폰 — 매프레임 캡처 중복 방지(동시 1개)
	static AGachaCaptureStage* AcquireShared(UObject* Holder, UWorld* World, TSubclassOf<AGachaCaptureStage> StageClass);

	// Holder 가 현재 소유자일 때만 EndReveal — 다른 소비처가 인계했으면 no-op(그쪽이 반납)
	void ReleaseShared(const UObject* Holder);

	bool IsHeldBy(const UObject* Holder) const { return HolderWeak.Get() == Holder; }

	// 에디터/배치 인스턴스에서 ShowOnly(this) + 매프레임 캡처 → BP 뷰포트에서 프리뷰 직원이 RT 에 실시간으로 보임(프레이밍 튜닝용)
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Stage") USceneCaptureComponent2D* Capture;
	UPROPERTY(VisibleAnywhere, Category = "Stage") USceneComponent* WorkerAnchor;   // 워커 스폰 위치
	UPROPERTY(VisibleAnywhere, Category = "Stage") USpotLightComponent* KeyLight;   // 격리 무대 키라이트
	UPROPERTY(VisibleAnywhere, Category = "Stage") UPointLightComponent* FillLight; // 전면 필 — 그늘부 컷아웃 임계값 확보(균일 밝힘)
	// 에디터 전용 프리뷰 직원 — 뷰포트에서 카메라/라이트 프레이밍 잡는 기준(런타임엔 숨김, 진짜 직원이 들어감)
	UPROPERTY(VisibleAnywhere, Category = "Stage") USkeletalMeshComponent* PreviewWorker;

	// 스폰할 스틱워커 BP. 생성자에서 BP_StickOfficeworker 자동 지정(수동 설정 불필요, BP 만들 필요 없음).
	UPROPERTY(EditDefaultsOnly, Category = "Stage") TSubclassOf<AStickOfficeworker> WorkerClass;

	UPROPERTY(EditDefaultsOnly, Category = "Stage") int32 RTWidth = 512;
	UPROPERTY(EditDefaultsOnly, Category = "Stage") int32 RTHeight = 640;

	// 멀티 리빌의 사이드 열 피치(RT px) — 크롭 창 폭(=512x0.58=297)과 별개 축이다. 창은 카드 안 프레이밍을,
	// 피치는 무대 위 댄서 간 실제 거리를 정한다. 창 폭이 불변이라 카드 그림(구도/크기)은 그대로고 이웃 댄서가
	// 크롭 창에 비치는 것만 막힌다 → BP 에서 재빌드 없이 조정하는 안전 마진 노브.
	// ⚠ 유일한 커플링 = 외곽 열이 FillLight 에서 멀어져 어두워진다(800 기준 5장 최외곽 ≈−22%, 감쇠반경 안).
	// 하한 297 = 크롭 창 폭(창보다 좁은 피치는 U0 음수/창 겹침), 상한 1200 = 5장 RT 폭 5312 < 8192(모바일 한계).
	UPROPERTY(EditDefaultsOnly, Category = "Stage", meta = (ClampMin = "297", ClampMax = "1200"))
	int32 SideColumnPitchPx = 800;

private:
	UPROPERTY() TArray<AStickOfficeworker*> CurrentWorkers; // 단발=1개, 멀티=N개(슬롯 순서)
	UPROPERTY() UTextureRenderTarget2D* RT = nullptr;
	UPROPERTY() UTextureRenderTarget2D* WideRT = nullptr;   // 멀티 전용 transient (BP 지정 RT 미오염) + GC 보호
	TArray<FVector4> SlotUVWindows;                        // BeginRevealBatch 산출 창(슬롯 순서) — 카드가 소비

	// 현재 소유 소비처 (직원창/가챠 위젯) — ReleaseShared 판정용
	TWeakObjectPtr<const UObject> HolderWeak;

	AStickOfficeworker* SpawnAndDress(const FEmployeeInstance& Employee, EStageAnimMode AnimMode,
		const FVector& LocalOffset, float ActorScale);
	void RefreshShowOnly();
	void DestroyCurrentWorkers();
	// BeginReveal 의 노출/CaptureSource/ShowFlags 불변식 — 단발/멀티 공유(결정성 계약, 룩 노브 아님)
	void ApplyCaptureInvariants();
	// 단발 카메라 기준 위치/회전 = CDO(BP 프레이밍 튜닝 반영) 값. 카메라 자세는 불변식이 아니라 룩 노브다.
	// 라이브가 아니라 CDO 를 보는 이유 = 멀티가 라이브 값을 후퇴/평탄화로 덮으므로, 라이브를 읽으면 후퇴가 누적된다.
	FVector GetSingleCamLocation() const;
	FRotator GetSingleCamRotation() const;

	// 유효 열 피치 — RT 폭과 UV 창이 반드시 같은 값에서 유도되도록 클램프를 한 곳에 둔다.
	// 메타 ClampMin/Max 는 에디터 입력만 막으므로(기존 에셋/코드 대입은 통과) 산식 쪽에서도 같은 범위로 가둔다.
	int32 GetColumnPitchPx() const { return FMath::Clamp(SideColumnPitchPx, SideWindowPx, PitchMaxPx); }

	static constexpr float SingleCamX = -520.f;   // 생성자 Capture 상대 X — CDO 미확보 시 폴백
	static constexpr int32 CenterSlotPx = 512;
	static constexpr float SideActorScale = 0.58f; // 스펙 §5.1 SideScale 과 동기
	// 사이드 크롭 창 폭 = ceil(512 x 0.58) = 297. 피치 하한이기도 하다 — 창보다 좁은 피치는 최좌열 U0 가 음수가 되어
	// 엣지 스미어 + 이웃 창 겹침을 동시에 일으킨다.
	static constexpr int32 SideWindowPx = 297;
	// 피치 상한 — 5장 RT 폭 = 512 + 4x1200 = 5312 < 8192(모바일 텍스처 한계). RHI 상한(16384) 초과 크래시 차단.
	static constexpr int32 PitchMaxPx = 1200;
	// 창 폭 산식과 정수 사본이 갈라지면 하한이 무의미해진다 — SideActorScale 튜닝 시 여기서 걸린다
	static_assert(SideWindowPx >= CenterSlotPx * SideActorScale
		&& SideWindowPx < CenterSlotPx * SideActorScale + 1.f, "SideWindowPx must be ceil(CenterSlotPx * SideActorScale)");
	static_assert(PitchMaxPx > SideWindowPx, "PitchMaxPx must exceed the crop window width");
	static constexpr float FeetZ = 88.f;           // 발끝의 액터 원점 대비 오프셋 = ACharacter 캡슐 half-height(리그 아닌 메시 속성)
	static constexpr float SideZDrop = FeetZ * (1.f - SideActorScale); // 캡슐 중심 스케일이라 뜨는 만큼 내림(발 정렬)
	// 무대 지면선(발끝 Z=−FeetZ)의 프레임 내 v 좌표 — 기본 리그(카메라 X=−520·Z=5·FOV 40·RT 512×640) 값.
	// 발끝은 프레임 바닥(v=1)이 아니다. BeginRevealBatch 가 실제 리그로 재계산해 주입하고, 이 값은 퇴화 케이스
	// 폴백 + 리그 드리프트 경고의 기준선으로만 쓴다(기본 인자는 제거됨 — 라이브 값 주입 강제).
	static constexpr float GroundLineV = 0.6966f;
	// 화면 우측 = 앵커 로컬 -Y 추정 — PIE 검증 항목(반전되면 이 상수 부호만 뒤집으면 됨)
	static constexpr float ScreenRightToAnchorY = -1.f;
};
