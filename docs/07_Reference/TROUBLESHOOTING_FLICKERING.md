# Building Flickering Troubleshooting

## 증상

- BuildingBaseActor의 건물 파츠(Base, Body, Top)가 카메라 각도/거리에 따라 깜빡거림
- 카메라가 위에서 비스듬히 내려다보는 각도에서 뒤로 빠지면 발생
- 건물이 카메라에 완전히 보이는 상태에서도 발생
- 각 파츠가 독립적으로 사라졌다 나타남

## 건물 컴포넌트 구조

```
ABuildingBaseActor
├── MainMeshComponent (UStaticMeshComponent) — 건물 베이스
├── Body_Module (UInstancedStaticMeshComponent) — 층 인스턴스들
├── Top_Module (UInstancedStaticMeshComponent) — 지붕
└── Top_Empty_Module (UInstancedStaticMeshComponent) — 빈 지붕 슬롯
```

## 시도했지만 효과 없었던 것들

### 1. BoundGap / ReCalcBoxExtent 수정

`ReCalcBoxExtent()`와 `BoundGap`은 **BoxComponent(콜리전용)**만 수정한다. 렌더링 프러스텀 컬링과는 무관.

```cpp
// 이건 콜리전 바운드 — 렌더링에 영향 없음
BoxComponent->SetBoxExtent(boundsMax);
```

### 2. SetBoundsScale 증가

각 메시 컴포넌트의 `SetBoundsScale()`을 5.0f~10.0f로 설정. 프러스텀 컬링 방지에는 효과가 있지만, 이번 문제의 원인은 프러스텀 컬링이 아니었음.

### 3. bUseAsOccluder = false

이 설정은 **"이 컴포넌트가 다른 것을 가리지 않겠다"**는 의미. **"이 컴포넌트가 다른 것에 의해 가려지지 않겠다"가 아님.** 즉 occluder(가리는 쪽)만 비활성화하지, occludee(가려지는 쪽)는 여전히 활성 상태.

### 4. bNeverDistanceCull = true

거리 기반 컬링 비활성화. 이번 문제와 무관.

## 원인: Hardware Occlusion Query

### 작동 원리

UE5 렌더러는 각 PrimitiveComponent의 바운딩 박스를 GPU에 그려서 "이 물체가 다른 오브젝트 뒤에 완전히 가려졌는가?"를 판정한다 (Hardware Occlusion Query).

### 왜 문제가 됐는가

- 카메라가 위에서 비스듬히 내려다보면, **지면 메시가 건물 컴포넌트 바운드 앞에 위치**하는 것으로 잘못 판정
- 각 컴포넌트(Base, Body, Top)가 **독립적으로** 오클루전 테스트를 받아서 일부만 사라짐
- 카메라가 멀어질수록 바운딩 박스가 작아져서 오판율 증가
- `bUseAsOccluder = false`는 occluder만 끄지, occludee는 안 끔

### 진단 방법

에디터 콘솔에서 `r.AllowOcclusionQueries 0` 입력 후 플리커링이 멈추면 Hardware Occlusion이 원인.

## 해결: r.AllowOcclusionQueries=0

### 적용 위치

`Config/DefaultEngine.ini` — `[/Script/Engine.RendererSettings]` 섹션:

```ini
r.AllowOcclusionQueries=0
```

### 왜 이 게임에서 안전한가

- 오클루전 쿼리는 **1인칭/3인칭 실내 환경**에서 벽 뒤 오브젝트를 컬링할 때 효과적
- **탑다운/아이소메트릭 게임에서는 대부분의 오브젝트가 항상 보임** → 쿼리 비용만 발생, 실제 컬링 효과 미미
- 오클루전 쿼리 자체가 GPU readback latency를 유발하므로, 끄면 오히려 **성능 향상**

### 추가 방어 설정 (C++ 코드)

```cpp
// 모든 건물 메시 컴포넌트에 적용
Component->bUseAsOccluder = false;           // 다른 것을 가리지 않음
Component->bNeverDistanceCull = true;        // 거리 컬링 비활성화
Component->bAllowCullDistanceVolume = false; // CullDistanceVolume 무시
Component->SetBoundsScale(5.0f);             // 프러스텀 컬링 바운드 확장
```

## UE5 컬링 시스템 정리

| 컬링 종류 | 제어 설정 | 설명 |
|-----------|-----------|------|
| Frustum Culling | `SetBoundsScale()` | 뷰 프러스텀 밖 오브젝트 제거. 바운드 확장으로 방지 |
| Distance Culling | `bNeverDistanceCull` | 거리 기반 제거. true로 비활성화 |
| Cull Distance Volume | `bAllowCullDistanceVolume` | 레벨에 배치된 볼륨 기반 컬링. false로 무시 |
| Hardware Occlusion | `r.AllowOcclusionQueries` | GPU 쿼리 기반 가림 판정. 0으로 비활성화 |
| Screen Size Culling | `MinScreenSize` | 화면상 크기 기반 제거. 0이면 비활성화 |

## 핵심 교훈

1. **BoxComponent 바운드 ≠ 렌더링 바운드**: `ReCalcBoxExtent()`는 콜리전용, 렌더링 컬링에 영향 없음
2. **bUseAsOccluder = false ≠ 오클루전 면역**: occluder만 끄지 occludee는 안 끔
3. **컬링은 Actor 단위가 아니라 PrimitiveComponent 단위**: 같은 Actor의 컴포넌트도 독립 컬링됨
4. **탑다운 게임에서 오클루전 쿼리는 불필요**: 끄는 것이 정석
