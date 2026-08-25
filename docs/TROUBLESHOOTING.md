# 대표 트러블슈팅

## 1. 야간 Flickering·Shimmer

### 문제

모바일 Vulkan 야간 화면에서 밝기와 윤곽이 프레임마다 흔들리는 현상이 발생했습니다.

### 접근

노출, AA, 리샘플링, Tonemapper 선명화를 독립 변수로 나누고 동일 구도·시간대의 A/B 캡처와 밝기 시계열을 비교했습니다. 정지 화면이 선명하다는 이유만으로 시간축 Shimmer 해결을 주장하지 않았습니다.

### 결과

오토 노출 진동을 확인해 모바일 고정 노출 정책으로 전환하고, 정량 검증 스크립트를 재사용 가능한 도구로 남겼습니다.

- [진단 플레이북](07_Reference/TROUBLESHOOTING_FLICKERING.md)
- [모바일 렌더링 기록](08_Optimization/MOBILE_RENDERING.md)
- [검증 스크립트](../Tools/MainMapPreview/validate_night_readability_ab.py)

## 2. UMG 좌표 공간 불일치

### 문제

모바일 SafeZone과 DPI가 적용되면 월드 좌표 기반 UI가 다른 위치에 나타났습니다.

### 접근과 결과

Screen 좌표를 ViewportScale로 나누는 가정을 제거하고, Viewport Local → Absolute → Target Canvas Local 변환으로 통일했습니다. 중첩 컨테이너와 SafeZone 보정을 Geometry에 맡겼습니다.

- [UI 생성 플레이북](05_UI/UI_CREATION_PLAYBOOK.md)
- [대표 적용부](../Source/CompanyGrowthRenewal/Private/UI/Panel/InGameLayerWidget.cpp)

## 3. 저장 초기화 순서

### 문제

최초 로드가 끝나기 전에 저장이 실행되면 빈 런타임 상태가 기존 세이브를 덮을 수 있었습니다.

### 접근과 결과

로드 완료 전 저장을 차단하고, 고빈도 변경은 지연 저장으로 통합했습니다. 저장·로드 구조가 바뀔 때 Round Trip 테스트로 직렬화 대칭성을 확인합니다.

- [SaveLoadManager.cpp](../Source/CompanyGrowthRenewal/Private/Manager/SaveLoadManager.cpp)
- [Round Trip 테스트](../Source/CompanyGrowthRenewal/Private/Tests/OfficeExteriorSaveRoundTripTests.cpp)

## 4. Blueprint 직렬화 잔재

### 문제

에디터 세션에서 저장된 동적 머티리얼 참조가 다시 로드되며 런타임 생성 상태를 오염시키는 문제가 있었습니다.

### 접근과 결과

메모리상의 현재 포인터 유효성만 검사하지 않고, 직렬화된 로드 객체인지 구분해 런타임 인스턴스를 재생성했습니다. 결과를 특정 에셋의 우연한 수정이 아니라 객체 수명주기 규칙으로 일반화했습니다.

- [직렬화된 MID를 판별해 재생성하는 구현](../Source/CompanyGrowthRenewal/Private/Entity/Officeworker/StickOfficeworker.cpp)

## 검증 원칙

문제를 고쳤다는 표현은 원래 현상을 재현하는 증거가 사라졌을 때만 사용합니다. 빌드 성공은 런타임·실기기 성공을 대신하지 않으며, 정지 이미지도 시간축 문제 해결을 대신하지 않습니다.
