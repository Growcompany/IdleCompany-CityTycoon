---
name: "source-command-full-review"
description: "전체 코드 품질 점검 (리뷰 + 간소화 + 보안)"
---

# source-command-full-review

Use this skill when the user asks to run the migrated source command `full-review`.

## Command Template

아래 순서대로 전체 코드 품질 점검을 수행해줘:

1. **변경 범위 파악**: `git diff --stat`으로 현재 변경된 파일 확인
2. **코드 리뷰**: 변경된 C++ 파일들에 대해 다음 체크
   - nullptr 크래시 위험
   - UPROPERTY 누락 (GC 이슈)
   - NativeConstruct/NativeDestruct 델리게이트 쌍 (Widget인 경우)
   - SaveLoad 대칭성 (SaveLoadManager 변경인 경우)
   - DataTable 패턴 준수 (TableManager 변경인 경우)
   - 메모리 누수 가능성
3. **AGENTS.md 규칙 준수 확인**: 금지 패턴 사용 여부 체크
4. **DataTable CSV 변경 확인**: CSV 파일이 변경되었으면 빈 필드 경고
5. **결과 요약**: 심각/권장/확인 3단계로 분류

심각 이슈가 있으면 수정 제안까지 포함해줘.
