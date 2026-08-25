const chunks = [];
process.stdin.on('data', d => chunks.push(d));
process.stdin.on('end', () => {
  try {
    const j = JSON.parse(Buffer.concat(chunks).toString());
    const p = (j.prompt || '').toLowerCase();
    const hints = [];

    // 기존 패턴
    if (/데이터\s*테이블|datatable|dt_/.test(p)) {
      hints.push('DataTable 작업: CLAUDE.md "DataTable 추가 시 필수 패턴" + ue5-datatable 스킬');
    }
    if (/위젯|widget|wbp_|ui_|uie_/.test(p)) {
      hints.push('Widget 작업: ue5-widget 스킬 + CLAUDE.md "좌표 변환 규칙" + "델리게이트 바인딩 규칙"');
    }
    if (/서브시스템|subsystem/.test(p)) {
      hints.push('Subsystem 작업: ue5-subsystem 스킬');
    }
    if (/컴포넌트|component/.test(p)) {
      hints.push('Component 작업: ue5-component 스킬');
    }
    if (/이어서|이어하|계속|다음에|지난/.test(p)) {
      hints.push('연속 작업: git status, 최근 커밋, 활성 계획 문서를 근거로 진행상황 복원');
    }

    // 빌드/컴파일
    if (/빌드|build|컴파일|compile/.test(p)) {
      hints.push('빌드: 에디터 종료 확인 (Live Coding 충돌). /build-check로 사전 문법 검증 가능');
    }

    // 코드 리뷰
    if (/리뷰|review|검토|점검/.test(p)) {
      hints.push('리뷰: /full-review로 diff 기반 품질·보안 점검');
    }

    // 저장/로드
    if (/저장|save|로드|load|세이브/.test(p)) {
      hints.push('저장/로드: SaveLoadManager의 로드 완료 가드와 저장/로드 대칭성부터 확인');
    }

    // 강화 시스템
    if (/강화|enhancement|upgrade|업그레이드/.test(p)) {
      hints.push('강화: DataTable을 수치의 단일 진실 원천으로 유지하고 코드 폴백을 추가하지 말 것');
    }

    // 기획서
    if (/기획|design\s*doc|gdd|설계/.test(p)) {
      hints.push('설계: docs/ARCHITECTURE.md + 공개된 시스템별 기술 문서 + 실제 소스를 함께 확인');
    }

    // 성능/최적화
    if (/최적화|성능|performance|fps|프레임/.test(p)) {
      hints.push('최적화: docs/08_Optimization/PERFORMANCE_OPTIMIZATION.md + docs/08_Optimization/MOBILE_RENDERING.md 참고');
    }

    // 트레이트
    if (/트레이트|trait|building\s*trait|건물\s*특성/.test(p)) {
      hints.push('건물 특성: UBuildingTraitManagerSubsystem 공개 API와 DataTable 매핑부터 확인');
    }

    // 커밋/푸시
    if (/커밋|commit|푸시|push/.test(p)) {
      hints.push('커밋/푸시: 현재 브랜치와 변경 범위를 확인하고 사용자 승인 없이 원격 Push하지 말 것');
    }

    // 직원
    if (/직원|employee|사원|채용|recruit/.test(p)) {
      hints.push('직원: 기존 EmployeeManager 공개 API와 관련 테스트를 먼저 확인');
    }

    // 월드맵/무역
    if (/월드맵|worldmap|무역|trade|수출|판매/.test(p)) {
      hints.push('월드맵: WorldMapManager와 DataTable 매핑을 먼저 확인');
    }

    // 스테이지/프로젝트
    if (/스테이지|stage|프로젝트\s*보드|project\s*board/.test(p)) {
      hints.push('스테이지: 현재 공개 API와 저장 구조, 관련 테스트를 먼저 확인');
    }

    // 채광/공장
    if (/채광|mine|공장|factory|제조/.test(p)) {
      hints.push('채광/공장: 기존 Manager와 DataTable 정의를 먼저 확인');
    }

    // 보안
    if (/보안|security|취약|exploit/.test(p)) {
      hints.push('보안: /full-review로 현재 diff와 PlayFab·Firebase 경계 점검');
    }

    if (hints.length) {
      console.log(JSON.stringify({ systemMessage: hints.join(' | ') }));
    }
  } catch (e) {}
});
