const { execFileSync } = require('child_process');
const chunks = [];
process.stdin.on('data', d => chunks.push(d));
process.stdin.on('end', () => {
  try {
    const j = JSON.parse(Buffer.concat(chunks).toString());
    const p = (j.tool_input && j.tool_input.file_path) || '';
    const msgs = [];
    const cwd = process.env.CODEX_PROJECT_ROOT || process.cwd();

    // .h 수정 — 실제 include 카운트 표시
    if (/\.h$/i.test(p)) {
      const basename = p.replace(/^.*[/\\]/, '');
      try {
        const result = execFileSync('git', ['grep', '-l', '#include.*' + basename], {
          cwd, encoding: 'utf8', windowsHide: true, timeout: 5000
        }).trim();
        const files = result ? result.split('\n').filter(Boolean) : [];
        if (files.length > 0) {
          msgs.push('.h 수정됨 - ' + files.length + '개 파일이 이 헤더를 include 중 (영향 범위 확인 권장)');
        } else {
          msgs.push('.h 수정됨 - include하는 파일 없음 (신규 헤더인 경우 정상)');
        }
      } catch (e) {
        if (e.status === 1) {
          msgs.push('.h 수정됨 - include하는 파일 없음 (신규 헤더인 경우 정상)');
        } else {
          msgs.push('.h 수정됨 - #include하는 파일들 영향 검토 권장');
        }
      }
    }

    // TableManagerSubsystem
    if (/TableManagerSubsystem\.(cpp|h)$/i.test(p)) {
      msgs.push('TableManager 변경 - AGENTS.md "DataTable 추가 시 필수 패턴" 확인');
    }

    // Widget
    if (/Widget.*\.(cpp|h)$/i.test(p)) {
      msgs.push('Widget 수정 - NativeConstruct/NativeDestruct 델리게이트 쌍 + AbsoluteToLocal 좌표 규칙 체크');
    }

    // SaveLoadManager
    if (/SaveLoadManager\.(cpp|h)$/i.test(p)) {
      msgs.push('SaveLoad 변경 - 저장/로드 대칭성, 초기 로드 가드, 지연 저장 정책 확인');
    }

    // CSV — DataTable reimport 리마인더
    if (/\.csv$/i.test(p)) {
      const dtName = p.replace(/^.*[/\\]/, '').replace(/_Import\.csv$/i, '').replace(/\.csv$/i, '');
      msgs.push('CSV 수정됨 [' + dtName + '] - UE 에디터에서 해당 DataTable Reimport 필요 (우클릭 → Reimport)');
    }

    // Enum 파일
    if (/[/\\]Enum[/\\].*\.h$/i.test(p)) {
      msgs.push('Enum 수정 - DataTable CSV 컬럼 값 + BP 참조 + switch/if 분기 동기화 필요 여부 확인');
    }

    // Data 구조체 파일
    if (/[/\\]Data[/\\].*\.h$/i.test(p)) {
      msgs.push('Data 구조체 수정 - SaveLoadManager 직렬화 호환성 + DataTable Row 구조 동기화 확인');
    }

    // Manager 파일
    if (/[/\\]Manager[/\\].*\.(cpp|h)$/i.test(p) && !/SaveLoadManager|TableManagerSubsystem/i.test(p)) {
      msgs.push('Manager 수정 - 다른 Manager/Widget에서 이 Manager를 호출하는 곳 영향 체크 권장');
    }

    if (msgs.length) {
      console.log(JSON.stringify({ systemMessage: msgs.join(' | ') }));
    }
  } catch (e) {}
});
