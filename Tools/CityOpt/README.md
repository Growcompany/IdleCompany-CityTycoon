# CityOpt StaticMesh LOD authoring

## 도시 오토바이 제거·차량 LOD 전환 조정 — 2026-10-10

사용자 요청으로 `MainMap_City`의 고정 오토바이12대를 제거했다. 오토바이만 담은 HISM3개(6/2/2대)와 개별 StaticMeshActor2개를 삭제하고 외부 액터 패키지5개의 디스크 삭제·전체 WP descriptor 부재를 확인했다. 주행 `TrafficManager`의 실제 `CarModels`는 버스·승용차·택시·소형/중형/대형 트럭6종이며 원래 오토바이를 포함하지 않았다. 공급 에셋 원본은 게임 배치와 구분한다.

차량6종의 기존4 LOD geometry를 유지하며 LOD2/3 화면 크기만 앞당겼다. LOD0/1 화면 크기, 재질·소켓·충돌·배치·주행 코드는 보존했다. `R`은 원본 바운드 구체 반경(cm)이며 신규 LOD2/3 화면 크기는 각각 `3.8*R/3000`, `3.8*R/4500`(소수6자리)이다. 이 분모를 실제 고정 전환 거리로 안내하지 않는다. 투영·FOV/DefaultFOV·거리 배율·인스턴스 배율에 따라 실제 선택이 달라진다.

| 메시 | LOD0 | LOD1 | LOD2 | LOD3 | 새 LOD2/3 화면 크기 |
|---|---:|---:|---:|---:|---|
| SM_Car_01 | 2840 | 1419 | 710 | 354 | .327216 / .218144 |
| SM_Taxi | 3019 | 1508 | 754 | 376 | .327217 / .218145 |
| SM_Truck_Small | 3268 | 1634 | 817 | 407 | .348977 / .232652 |
| SM_Truck_Medium | 3086 | 1543 | 770 | 385 | .488419 / .325613 |
| SM_Truck_Large | 3192 | 1595 | 798 | 399 | .642546 / .428364 |
| SM_Bus | 1996 | 998 | 498 | 249 | .610082 / .406721 |

`Saved/CityOpt/VehicleLOD20261010/verification.json`에서6개 저장 패키지를 디스크 재로드하여 저작값 및 geometry·재질·소켓·충돌 보존을 대조했다. 재로드 전후537개 액터의 메시 컴포넌트·인스턴스 변환·재질·가시성·충돌 및 실제 차량 모델/yaw 배열은 같고 dirty content/map은0이다. `motorbike_removal.json`에는12대와 삭제 패키지5개의 백업 해시가 있다. `capture.json` 및 `vehicle_lod0/2/3.png`는 같은 Win64 D3D12 미리보기에서 차체·바퀴·실루엣의 단계별 차이를 확인한 근거다. 임시 미리보기 액터는 전부 제거했다.

일반 게임 카메라의 실제 LOD 선택과 A9 FPS는 **미검증**이다. SIE는 SpectatorPawn/원점 카메라여서 주행 차 표시가0이었고 정상 플레이 검수에서 제외했다(`runtime_baseline.json`). 정상 PIE 창 제어는 `failed to activate captured window`로 실패했고 사용자에게 플레이 시작을 요청했다. `selection_prediction.json`은 화면 투영 수식에 따른 예상값이며 실측이 아니다. C++는 변경하지 않았고 Android 패키징·기기 조작을 실행하지 않았다. 폰 반영에는 이 변경을 포함한 새 쿠킹·APK 설치가 필요하다.

진단 SIE가 만든 자동 저장은 정확한 결과 해시를 확인하고 별도 보존한 뒤 검사 직전 원본으로 복원했다(`editor_final.json`). 원본과 결과 백업·로그·이미지는 유지하며 이번 일회성 Python/실행 설정은 정리한다(`cleanup.json`). 새 회귀 테스트는 추가하지 않았다.

## Future Island Partition

`MainMap_City` has `/Game/CompanyGrowth/Environment/CityRegions/DL_FutureIsland`, a Runtime Data Layer initially `Unloaded`. It contains 102 verified unused-region actors. The 2026-10-08 far-fog revision restored 19 structural bridge actors from the initial 121 members so the two bridges continue toward the distant bank. Shared mainland terrain, landscape splines, waterfront instances and all gameplay anchors remain outside the layer. Future edits use Unreal's Data Layers editor; the one-off authoring and inspection scripts were removed after application.

Two always-loaded HISM actors, `FutureIsland_CloudCore` and `FutureIsland_CloudFringe`, now use one `/Engine/BasicShapes/Plane` instance each instead of 30 spheres. They form a long soft bank only toward world +X, the top of the default city view. Left/right/bottom remain open sea. The two-sided unlit translucent materials use world-position smoothstep fades, DepthFade (1500), and existing `MPC_WindowLight.Night_Intensity`; no global fog, collision, shadows or actor/component ticks were added. `CityPlayableArea_Mainland` authors the camera XY rectangle from `(-80000, -114000)` to `(11000, 46000)`.

Current evidence is in `Saved/CityIslands/NorthFog20261008/`: `authored.json`, `camera_authored.json`, `editor_membership_verified.json`, original-package `Backup/`, and `final_mobile_verified.json`. A fresh Vulkan ES3.1 game verified all 102 excluded actors absent, 19 restored bridges and 19 protected mainland actors present, two fog planes, and both opposite camera-boundary corners with Z unchanged. `final_mobile_*.png` contains ten normal-rig views and two explicitly diagnostic aerial layout views. The latter show the continuous upper bank; at normal zoom the distant bank can lie outside the frame, so these images do not establish a visible fog barrier at every allowed camera pose. No physical-device FPS, memory or Android cooking result is claimed. Initial sphere-version evidence remains separately in `Saved/CityIslands/20261008/`.

Recheck saved membership, runtime presence and visual coverage after changes. The maintained `CGR.Camera.CityPlayableArea.RotatedBoundaryAndFocus` regression checks rotated bounds, Z preservation and completion of out-of-bounds focus. Full parameters, scope and verification commands: `docs/superpowers/plans/2026-10-08-city-island-streaming.md`.

## Static Mesh LODs

`hism_convert.py`는 일반 스케일과 반전 스케일(월드 스케일 XYZ 곱이 음수인 액터)을 별도 HISM으로 묶는다. 반전 그룹은 컴포넌트 자체에 X=-1을 적용하고 원래 월드 변환으로 인스턴스를 추가한다. 로컬 인스턴스는 양의 방향성을 가지며, UE5.4의 컴포넌트 단위 면 컬링이 반전 보도를 올바르게 그린다. 위치·경사·재질·충돌과 WP 셀 분류는 보존된다. 저장 후 새 게임에서 대상 면이 보이는지 확인한다.

`static_mesh_lods.py`는 manifest에 적힌 StaticMesh만 읽고, 현재 LOD가 하나인 메시만 LOD0와 축소 LOD 3개로 만든다. 이미 4 LOD인 가로등·차량·건물 등은 프로필을 검사하고 재생성하지 않는다. `inspect`와 `verify`는 에셋을 변경하지 않는다.

Manifest는 프로젝트 `Saved/` 아래 JSON으로 저장한다. `report_path`와 `backup_dir`도 `Saved/` 아래 절대경로이며, 각 메시의 감축률과 화면 크기는 반드시 명시한다.

```json
{
  "report_path": "${PROJECT_ROOT}/Saved/CityOpt/lod_report.json",
  "backup_dir": "${PROJECT_ROOT}/Saved/CityOpt/LODBackup",
  "assets": [
    {
      "path": "/Game/City_Pack/Meshes/SM_Park_Bench",
      "percent_triangles": [1.0, 0.5, 0.25, 0.125],
      "screen_sizes": [1.0, 0.3, 0.12, 0.05]
    }
  ]
}
```

경로와 수치는 예시다. 실제 manifest에는 먼저 감사한 대상의 정확한 패키지 경로와 승인된 자산별 값을 넣는다. 4 LOD 기존 메시를 포함할 경우 그 메시의 실제 화면 크기 프로필을 적어야 검증이 통과한다. 실행 전에 `inspect` 보고서에서 현재 LOD 수를 확인한다. 모든 모드는 PIE 중 실행을 거부하고, `apply`·`verify`는 미저장 대상 패키지를 거부한다.

에디터 Output Log의 Cmd 드롭다운을 Python으로 바꾼 뒤, 프로젝트에 이미 등록된 Python 경로에서 `import static_mesh_lods; static_mesh_lods.run(r"C:/.../Saved/CityOpt/lod_manifest.json", "inspect")` 형태로 실행한다. 모듈 경로가 등록되지 않은 세션은 짧은 임시 `C:/tmp/citylod.py`에 `sys.path` 추가와 이 호출만 적고 `exec(open(r"C:/tmp/citylod.py").read())` 한 줄로 실행한다. 임시 런처는 작업 후 삭제한다. 같은 manifest로 `apply`, `verify` 순서로 실행한다. `verify`는 성공한 `apply` 또는 이전 `verify` 보고서의 보존 데이터와 저장 해시를 요구한다.

`apply`는 각 원본 `.uasset`을 `backup_dir`에 같은 상대경로로 복사하고 SHA-256을 비교한 뒤 수정한다. LOD0 원본 좌표·삼각형 연결의 SHA-256과 삼각형·정점·섹션별 재질, 모든 재질 슬롯, 충돌 BodySetup, 소켓이 전후 동일한지 검사한다. 전체 렌더 바운드는 별도 기록한다. UE5.4 StaticMeshBuilder.cpp는 모든 LOD 바운드의 합집합을 사용하므로 축소 메시 생성 후 전체 바운드는 변할 수 있다. 충돌 형상과 소켓 회전은 UE 구조체의 `export_text()`로 직렬화하고, 단순 충돌 개수는 엔진 조회값과 BodySetup의 구·상자·캡슐 개수를 대조한다. LOD0의 기존 감축 설정도 보존하며, 4단계의 삼각형·정점 수가 양수이며 점감하는지, 모든 LOD의 렌더 섹션 재질 인덱스와 화면 크기·자동 계산 상태를 읽어 확인한다. 에셋 저장 반환값과 저장된 `.uasset` 해시도 확인한다. 오류는 `report_path`에 기록하고 예외를 다시 발생시킨다. 이미 목표 4 LOD인 메시에는 `SetLods`를 다시 호출하지 않는다.

이 도구는 맵을 전환하거나 PIE를 종료하지 않는다. UE5.4의 `StaticMeshEditorSubsystem` 조회 API는 PIE 중 값을 제공하지 않으므로 PIE 종료 후 재실행한다. `verify`는 현재 에디터 메모리에 로드된 에셋을 검사한다. 저장 파일의 독립 재로드 검증이 필요하면 에디터를 재시작한 뒤 `verify`를 다시 실행한다.

### 벤치·야외 의자 적용 (2026-10-10)

- `SM_Park_Bench`: LOD0/1/2/3 = **790 / 395 / 196 / 97**. `SM_Outdoor_Chair`: **1278 / 638 / 318 / 159**. 화면 크기는 두 메시 모두 **1 / 0.3 / 0.12 / 0.05**, 자동 계산은 끈다. 도시의 벤치99개·의자10개 배치, ForcedLodModel=0(자동 선택), 기존 컬링 거리는 유지했다.
- 두 원본의 source geometry·재질·충돌·소켓은 보존했다. 벤치는 LOD0 렌더 좌표와 삼각형도 동일하다. 의자는 Nanite fallback 재빌드로 기존1282개가1278개로 달라졌다. 원본 백업을 별도 임시 에셋으로 읽어 source geometry를 대조하고 실제 렌더 geometry·동일 시점 원본/생성 LOD0 및 LOD1–3를 확인했다. 의자의 렌더 geometry가 비트 단위로 같다고 주장하지 않는다. 일반 도구의 엄격한 렌더 수 보존 검사 실패는 중간 결과로 남겼고 검사를 완화하지 않았다.
- `get_triangle_vertices()`는 UE5.4 `MeshDescriptionBase.cpp`에서 `SetNumUninitialized(3)` 뒤 `Algo::Copy`로3개를 추가해 앞3개에 미초기화 값이 포함된다. LOD0 topology 해시는 `get_triangle_vertex_instance(triangle, corner)` → `get_vertex_instance_vertex()`로 실제 세 꼭짓점을 읽는다. 실에셋 반복 조회와 저장 후 재로드로 검증했으며 일회성 진단 코드는 제거했다.
- 최종 근거는 `Saved/CityOpt/SeatingLOD20261010/verification.json`, 원본 비교는 `authored.json`·`geometry_comparison.json`, 복구용 원본은 `before/`에 있다. `manifest.json`으로 `verify`를 실행하면 현재 저장된4 LOD 프로필과 저장 해시를 검증한다. 원본 source와의 비교 근거는 `authored.json`이며 `report.json`의 최종 저장 프로필과 구분한다.
- Win64 편집기에서 non-Nanite LOD를 강제한 검수이며 A9 FPS·Android 렌더 검증은 아니다. 폰 반영에는 재패키징·설치가 필요하다. 에셋 재로드가 수정 상태로 표시한 맵과 두 HISM 참조 패키지의 추가 저장은 자동 승인 검토에서 거절되어 수행하지 않았다. LOD 메시 두 패키지는 저장·재로드 검증됐으며 다른 미저장 내용을 저장하거나 폐기하지 않았다.

## 지형 구역 분할·가로등·작은 장식 (2026-10-10)

- 사용자는 지형·가로등 추가 최적화와 실제 배치에서 가장 긴 변이 **100cm 이하인 순수 장식** 제거를 승인했다. 휴지통은 크기에 관계없이 전부 보존한다. 도로·난간·기능성 물체·벤치·야외 의자는 이 제거 범위에 포함하지 않는다.
- `MainMap_City` 지형은 원본45000삼각형을 유지하며 로컬 XY200m 격자47구역으로 나눴다. 경계 삼각형은 자르지 않고 중심점으로 한 구역에만 배정하므로 좌표·UV·해안선을 바꾸지 않는다. 자산은 `/Game/CompanyGrowth/Environment/Terrain/CityChunks/`이고 맵의47개 StaticMeshActor가 하드 참조한다. 원본 `SM_IslandTerrain` 액터는 렌더·그림자를 끄고 기존 충돌을 담당한다. 신규 구역은 `NoCollision` 및 navigation off이며 원본 위치·회전·스케일·재질·거리 컬링 설정을 따른다.
- `terrain_chunks.py`의 `prepare(source_path, destination_folder, report_path, cell_size_cm=20000, update_existing=False)`는 맵을 수정하지 않는다. 기본값은 새 구역 자산만 만들고 기존 목적지 자산이 있으면 중단한다. `update_existing=True`는 이번 계획에 들어간 기존 StaticMesh의 LOD0만 제자리 갱신하고 새 셀은 생성한다. 시작 전에 갱신 대상 전부의 미저장 상태·1 LOD·재질 슬롯을 검사한다. **이 함수에는 백업이나 이전 계획에서 빠진 구역 자산·액터의 삭제가 없다.** 실행 전에 대상 자산·맵을 별도로 백업하고, 빠진 셀과 액터는 결과를 확인한 뒤 별도 작업으로 처리한다. 원본 전체에서 빌드 설정의 노멀·탄젠트를 먼저 계산한 뒤 분할하며 각 구역은 재계산하지 않는다. `verify(source_path, report_path)`는 저장 파일 해시, 원본 삼각형의 중복·누락, 구역별 좌표·UV·노멀·탄젠트·색·재질과 섹션을 대조한다. 두 함수는 PIE와 미저장 원본을 거부하며, `prepare`는 미저장 갱신 대상도 거부한다. 실행은 기존 Python 경로에서 모듈을 import하거나 BUILD_EDITOR.md의 짧은 임시 런처 방식을 따른다.
- Landscape 수정분 재베이크 순서: 세 proxy의 높이 PNG와 `height_export.json`을 저장한 뒤, 일반 Python의 `terrain_source_data.py`로 `new_mesh.json`과 입력 해시·범위 검증 보고서를 만든다. 이 입력으로 네이티브 `SM_IslandTerrainMesh`를 갱신하고 저장·재로드해 원본 지형과 높이·재질을 확인한다. 기존 chunk 자산을 백업한 다음 에디터 Python에서 `terrain_chunks.prepare(SOURCE, '/Game/CompanyGrowth/Environment/Terrain/CityChunks', 절대_보고서_경로, update_existing=True)`를 실행하고, 저장 후 새 에디터에서 `terrain_chunks.verify(SOURCE, 절대_보고서_경로)`를 다시 실행한다. 원본 텍스처 색을 바꿨다면 알베도 베이크·머티리얼도 별도로 검증한다. 일회성 export·검증 스크립트와 실패한 중간 PNG/JSON은 결과 확인 후 정리하고, 재사용 도구·최종 보고서·복구 백업은 보존한다.
- 가로등3종은 기존4 LOD의 geometry·재질·충돌·소켓을 유지했다. LOD0 화면 크기2.0과 LOD1 약20m 기준을 유지하고 LOD2/3를 약35/55m 기준으로 당겼다. 화면 크기 계산은 `3.8 * mesh_radius_cm / distance_cm`; 실제 전환 거리는 FOV·화면 비율·거리 스케일에 따라 달라진다. `SM_Street_Lamp` 화면 크기2/.430112/.245778/.156404, `SM_Street_Lamp_Large`2/.525/.3/.190909, `SM_Park_Light`2/.449266/.256723/.163369다.
- 제거한81개: 쓰레기봉투21, 비치볼7, 닫힌 피자 상자3, 열린 피자 상자2, 타이어15, 콘16, 신문12, 작은 야외 테이블5. 전용 HISM 액터7개를 제거하고 테이블은 원래 변환 그대로4개 남겼다. 휴지통은 `SM_Rubbish_Bin`84개와 `SM_Street_Bin`28개를 보존했다. 소품 원본 메시 자산은 지우지 않았다.
- 근거는 `Saved/CityOpt/TerrainProps20261010/`이다. `props_chunks_verified.json`은 가로등 디스크 재로드·휴지통112개·비대상2679개 컴포넌트의 배치 보존, `final_chunk_reload.json`은 최종47구역 디스크 재로드, `terrain_verified.json`은 배치·렌더 삼각형·충돌·저장 파일, `render_descriptors_verified.json`은 실제 RenderData 좌표/UV/재질·48개 저장 액터 descriptor 및 삭제된 소품 descriptor 부재를 기록한다. `render_attribute_difference.json`에서 최종 렌더 노멀은 모두 일치한다. 탄젠트/바이탄젠트는1개 삼각형에서 값이 달라 완전한 렌더 버퍼 동일성을 주장하지 않는다. 현재 지형 재질에는 Normal 입력 연결이 없다.
- `terrain_visual.json`·`terrain_whole_*`·`terrain_coast_*`는 동일 카메라에서 원본/분할 메시만 렌더한 진단 비교다. 실제 게임의 전체 조명·스트리밍·A9 외관 검증과 구분한다. 카메라가 외부에서 이동했던 초기 viewport 비교는 무효로 처리한다. 원본 자산과 대상 액터의 복구본은 `before/`에 있다.
- 메시 재빌드가 수정 표시한 신규 구역47개 참조 패키지는 검증 후 범위를 한정해 다시 저장했다. `owned_references_saved.json`에서 이번 작업 대상의 미저장 패키지0개와 다른 작업의 미저장 패키지 보존을 확인한다.
- 분할은 **전체45000삼각형을 줄이지 않는다**. 화면 밖 구역을 제외할 수 있도록 한다. 원본 충돌 메시가 남고 신규 렌더 메시·액터·드로우 호출이 추가되므로 메모리/FPS 개선을 단정하지 않는다. A9 사용은 보류 중이며 이 채팅에서는 APK를 만들거나 기기에 설치하지 않았다. 폰 반영에는 새 쿠킹·패키징·설치가 필요하며 같은 해상도·카메라에서 draw/prims/frame을 비교해야 한다. 이번 작업은 C++·게임 로직을 변경하지 않았다. 일회성 실행·검증·촬영 Python은 정리하고 재사용 저작 도구와 결과·백업만 보존한다.

### Landscape 수정분 재베이크와 가려진 도시 바닥 (2026-10-11)

> 이 재생성·평탄화 결과는 호수 주변 형상 이상에 따른 사용자 요청으로2026-10-14에 철회했다. 현재 렌더 지형은 아래 「호수 외관과 재베이크 롤백」을 따른다.

- 사용자가 수정한 세 Landscape proxy에서 높이를 다시 추출하고 기존 입력 생성 규칙을 재사용했다. 예전 높이 내보내기를 같은 도구로 재구성한 입력 해시는 기존 `mesh.json`과 일치한다. 최신 입력은73,622정점·144,870삼각형이며, 네이티브 감축 결과는44,999삼각형이다. 감축 목표45,000과 실제 결과가1개 다른 것은 기록하며 정확히45,000개를 만들기 위한 임의 면 추가는 하지 않는다.
- 입력 JSON은 예전 규칙과 동일하지만 UE 메시 저작에는 별도 면 방향 변환이 필요하다. `append_buffers_to_mesh(..., 0, True)`의 마지막 인수는 변경 알림 지연이며 면 뒤집기가 아니다. 원본 저장 메시의 XY 투영은음수44,996/양수1면인데 최초 새 후보는양수43,969/음수1면으로 반대였다. `GeometryScript_Normals.flip_normals()`로 삼각형 방향과 노멀을 함께 뒤집어 원본의 시계 방향 상면을 복구한 뒤 노멀·탄젠트를 계산한다. 재질을 양면으로 바꿔 문제를 가리지 않는다. 면 순서를 무시한 좌표·UV·색·재질 표면은 복구 전후 일치하며, 방향을 포함한 최종 source/chunk 대조와 실제 상면 캡처를 별도로 통과해야 한다.
- 원본 Landscape는 편집 가능한 상태로 보존한다. 도시 Plot의 논리 경계나 현재 건물 바운드를 가림 근거로 쓰지 않는다. 실제 불투명 `SM_Sidewalk_Flat` 바닥5개의 배치 변환과 원본 사각형을 읽고, 바닥 가장자리에서20m 안쪽이며 정점에 붙은 모든 면이 같은 바닥 아래에 들어가는186정점만 모바일 메시에서 평탄화했다. 높이는 바닥보다1m 아래로 두고 지면을 삭제하지 않는다. 해안·노출 경사면에는 이 규칙을 적용하지 않는다.
- 최종 `SM_CoveredPartitioned`는200m 셀별로 평면을 단순화하고 경계 정점을 용접·T-junction을 해소한 **43,973삼각형**이다. 기존 공식 `SM_IslandTerrainMesh`와46구역을 갱신했고 비게 된4_8셀 자산·액터를 제거했다. 기존45,000 대비1,027개(약2.28%) 감소다. 전역 단순화 후보43,928개는 최대 구역 폭이55,669→75,304cm로 늘어 컬링이 불리해졌고, 별도 경계 분할 후보44,955개는51구역으로 늘어 채택하지 않았다. 최종 후보는 최신 기본 메시의46구역·최대 폭55,669cm를 유지한다.
- 같은XY의 최상단 지형 높이를 비교한68,136표본 중 노출 영역67,168개에서 누락0, 최대 높이 차이약9.1e-13cm다. 이 검사는 동일한 감축 입력 대비 가려진 바닥 처리를 검증하며 원본 Landscape 전체와의 무손실 동등성을 뜻하지 않는다. 기본 감축 메시의 접힌 면1개와 퇴화 투영면3개 때문에 정점Z와 수직선의 최상단 교차를 혼용하지 않는다. UV와 코너별 노멀·탄젠트·색·재질은 최종 source와 구역을 별도로 대조한다.
- 알베도는 현재 Landscape의 BaseColor를4096² `RTF_RGBA8_SRGB`로 재베이크하고 `TextureFactory`로 **기존** `T_IslandTerrain_Albedo`에 가져온다. 임포트 결과 경로·소스 파일·크기를 확인하고 sRGB·밉·스트리밍·필터 등 기존 설정을 보존한다. BaseColor만 사용하는 재질이므로 캡처의 미사용 알파는 압축에서 제외한다. 텍스처 컴파일 중 `Blueprint_GetSizeX/Y`는 대체 리소스 크기를 반환할 수 있어 임포트 직후 크기만으로 성공을 판정하지 않는다.
- 최종 source·46구역·알베도48자산은 디스크 재로드와 네이티브 컴파일 완료 후 검증했다(`final_verification.json`). source/구역 전체 삼각형·코너 속성,47액터의 변환·표시·충돌,47개 저장 descriptor, 원본 Landscape4패키지 해시가 통과했다. 전체 맵은 시작부터 미저장이었으므로 저장·폐기하지 않았고, 다른 미저장 콘텐츠 패키지도 건드리지 않았다.
- 같은 카메라의 BaseColor 진단에서 통합/분할 지형 이미지는 픽셀과 파일 해시가 일치했다. 도시 고정 바닥을 포함한 기본/최적화 비교는1600×900에서채널 평균 차이0.043/255, 최대12/255,8 초과 픽셀68개다. 차이0이라고 주장하지 않으며 최종 이미지를 직접 열어 상면 표시와 바닥 외관을 확인했다. `visual_verification.json`은 촬영 성공, `visual_pixel_comparison.json`은 픽셀 비교 근거다. `RejectedOrientation/`의 검은 중간 캡처와 일관성 검사 결과는 최종 외관 통과 근거에서 제외한다.
- 적용·검증 근거는 `Saved/CityOpt/TerrainRebake20261011/`의 `applied.json`, `albedo_applied.json`, `terrain_chunks.json`, `shape_validation.json`, `orientation_repair.json`, 최종 상태는 `status.json`이다. 원본 백업은 `Before/Content/`에 있다. 모바일 지형만 갱신하며 C++·게임 로직·다른 작업의 미저장 패키지는 변경하지 않는다. A9 FPS·게임 전체 조명·Android 스트리밍 성능은 별도 실기기 검증 대상이며 이번 작업에서 새 APK는 만들지 않았다.

### 호수 외관과 재베이크 롤백 (2026-10-14)

- 사용자가 호수 이상을 지적하며 이전 상태로 되돌리라고 요청했다. 추가 평탄화186정점은 월드Y약-76,677~-55,633cm이며 호수 중심(-22,277,-36,203)cm와 떨어져 있다. 평탄화 전후의 호수 높이는 동일했지만, 전체 메시 재생성 전 저장 지형과는 달랐다. 호수 중심 지면은 이전9,714.4cm, 재생성 후9,272.2cm, 수면은10,764.3cm다. 따라서 「평탄화로 호수 바닥이 올라왔다」고 단정하지 않는다. 이전 캡처 구도는 호수를 포함하지 않아 당시 외관 검수 범위가 부족했다.
-10-11 이전 백업의 통합 지형·알베도를 복구하고 **45,000삼각형·47렌더 구역**으로 되돌렸다. 삭제됐던4_8구역의 렌더 액터1개는 같은 변환·재질·NoCollision 계약으로 다시 생성했다. 기존47액터와 새 액터1개를 저장했고48개 WP descriptor를 확인했다. 원본 Landscape4패키지 해시와 물2개 액터의 변환은 보존했다. 사용자가 편집한 Landscape의 수정은 남아 있으며 렌더 메시에는 이번 재베이크 결과를 사용하지 않는다.
- 열린 편집기가 원본 파일의 쓰기 핸들을 잡고 있어 직접 덮어쓰기는 변경0개에서 중단했다. 백업을 임시 에셋으로 읽고 Geometry Script·TextureFactory·네이티브 저장을 사용했다. 원본/복구 지형의 모든 삼각형과 좌표·UV·노멀·탄젠트·색·재질 수치는 일치한다. JSON 해시에는0.0/-0.0 차이가 있어 기존 도구의 `Counter` 비교와 부호 있는0만 정규화한 해시를 함께 대조했다. 나머지 수치 차이는 허용하지 않았다. 알베도4096² 픽셀은 원본과 차이0이다.
-49자산 디스크 재로드·원본과 동일한 삼각형 합·구역 누락/중복·저장 파일 해시를 확인했다. 같은 호수 구도의 `before.jpg`/`after.jpg`를 직접 열어 둥근 경계 복구를 확인했다. 게임 전체 성능이나 A9 검증을 뜻하지 않으며 APK는 만들지 않았다. 복구 직전 파일 백업은 `Saved/CityOpt/LakeAudit20261014/BeforeRestore/`, 최종 보고서는 `restored.json`, `restored_chunks.json`, `albedo_pixel_verification.json`, `editor_cleanup.json`, `completion.json`이다.
- 원본 Landscape22컴포넌트는 게임에서 숨겨져 있지만 에디터에서는 메시와 동시에 보이도록 켜져 있었다. 현재 세션에서는 원본4액터의 `set_is_temporarily_hidden_in_editor(True)`만 적용해 겹침을 정리했고 에셋·맵 저장 없이 미저장 상태가 유지됨을 확인했다. 편집기 재시작 후 이 임시 표시 상태는 돌아올 수 있다. 원본 지형을 조각할 때에는 원본과 렌더 구역을 함께 표시하지 않도록 각각의 에디터 표시를 구분한다.
- 원본 로드용 임시 에셋2개와 이번 일회성 Python은 작업 종료 시 제거했다. 원본 높이·복구 백업·유효 검증 보고서·캡처·알베도 소스 PNG는 유지한다. C++·재사용 도구·게임 기능에는 변경이 없다.
