# 공개 스냅샷 정책

## 기준

- Source repository: private Azure DevOps repository
- Snapshot commit: `7df820f2469822c17f2ce1e5c60f4c46093a27dc`
- Snapshot date: `2026-08-25`
- Direction: Azure private source → GitHub public portfolio only

GitHub 변경을 Azure로 역병합하지 않습니다. 다음 공개 갱신도 깨끗하게 커밋된 Azure revision에서 allowlist로 추출하고 사람이 diff를 검토한 뒤 반영합니다.

## 포함

- `Source` 전체 878파일
- 선별한 DataTable CSV 3개
- C++ Automation Test 소스 전체
- 선별한 Balance·렌더링·문서 동기화 도구
- 정제한 AI Agent·Command·Skill·Hook
- 클라이언트 아키텍처와 검증 플레이북

## 제외

- 외부/Fab/Marketplace 에셋과 `Content`
- 실제 `Config`, PlayFab·Firebase 설정과 배포 URL
- Firebase 서버 구현
- `Plugins`, 바이너리, 빌드·캐시·저장 결과
- 기존 Azure Git 히스토리와 원격 URL
- 개인 경로, 세션 상태, 백업, 자동 Push Hook
- 비공개 게임 기획과 미래 콘텐츠 계획

## 의도적 코드 정제

`Source/CompanyGrowthRenewal/Private/Manager/ChatManagerSubsystem.cpp`의 실제 Cloud Run 폴백 주소를 `https://YOUR_SERVICE_URL.example.com`으로 교체했습니다. 그 외 `Source` 파일은 기준 커밋의 내용과 동일해야 합니다.

공개 도구 검증 중 `Tools/Balance/verify/coverage.json`에서 워크스테이션 Diamond 지출 2개 경로의 등재 누락을 발견해 `DK8`, `DK9`로 문서화했습니다. 게임 소스 동작은 변경하지 않았습니다.

## 갱신 게이트

1. 비허용 바이너리와 에셋 0개
2. 고신뢰 비밀 패턴 0개
3. 개인 경로·Azure URL·실제 배포 URL 0개
4. README 로컬 링크 전부 존재
5. 공개 도구의 독립 테스트 통과
6. 새 Snapshot commit과 날짜 기록
