# UI_TYPOGRAPHY — 폰트 & 텍스트 단일 진실

> **모든 텍스트 작업(폰트 선택/크기/스타일 토큰/특수문자) 전에 이 문서를 우선 참조.**
> - "무엇으로, 어떤 모양으로"(부품/색/골격) = `UI_STYLE_CATALOG.md`
> - "어떻게 만드는가"(절차/함정) = `UI_CREATION_PLAYBOOK.md`
> - "어떤 글자를, 어떤 폰트로, 몇 px로" = **이 문서**
>
> 작성: 2026-06-05 (9-에이전트 전수 조사 — 4,679 에셋 바이너리 스캔 + C++/Config/외부 조사 기반)

## 운영 규칙 (비협상)

1. **새 텍스트는 반드시 §2 시맨틱 스케일의 스타일을 지정** — WBP TextBlock에 폰트 직접 하드코딩 금지 (기존 72개 WBP가 이 anti-pattern; 신규 추가 금지).
2. 폰트 역할 분리(§1)를 벗어나는 사용 금지 — 특히 **Bungee에 한글 절대 금지** (한글 글리프 0 → 박스).
3. 스타일/폰트를 새로 만들면 **이 문서에 즉시 등록**.
4. 사이즈는 §4 캔버스 기준(2560×1440)으로만 — 새 사이즈 티어 추가는 아틀라스 비용이 있으므로 기존 티어 우선.

---

## 1. 폰트 3종 + 역할 (사용자 확정 2026-06-05)

| 폰트 | 역할 | 굵기 | 에셋 (`/Game/CompanyGrowth/Font/`) | 라이선스 |
|---|---|---|---|---|
| **NEXON Lv1 Gothic** | **메인** — 타이틀/본문/버튼/라벨 등 게임 톤 전반 | Light/Regular/Bold | `NEXONLv1Gothic{Light,Regular,Bold}(_Font)` | 넥슨 무료 (상업·임베드 OK, 수정·재판매 금지, 출처표기 권장) |
| **Pretendard** | **재무·수치·정보** — 재화/스탯/재무 수치, 깔끔한 정보 표시 | Regular/Medium/SemiBold/Bold | `Pretendard/F_Pretendard` (4-typeface 컴포짓) | SIL OFL 1.1 |
| **Bungee** | **특수 디스플레이** — Latin/숫자 전용 장식 (수량 뱃지, "LEVEL UP" 류) | Regular | `_Source/Bungee-Regular(_Font)` | SIL OFL |

- `_Font` 접미사 = UFont(컴포짓), 무접미사 = UFontFace. NEXON은 굵기별 UFont 분리(`TypefaceFontName="Default"`), Pretendard는 `F_Pretendard` 하나에 4 typeface(`Bold/SemiBold/Medium/Regular`).
- ⚠ **Bungee는 한글 글리프가 없다** (Latin-only). 한글이 들어갈 수 있는 TextBlock에 바인딩 금지. 현재 사용처: `UIE_ItemCard_QtyBelow`, `UIE_TraitCard_QtyBelow` (수량 숫자 — 안전).
- ⚠ **엔진 Roboto = 사고**: 프로젝트 폰트 기본값 미설정(`CustomDefaultFont` 없음)이라 **폰트/스타일을 안 정한 TextBlock은 조용히 Roboto로 렌더** → 한글 폴백 깨짐. §8 누수 목록 참조.

## 2. 시맨틱 타이포 스케일 — `CUI_Style_Text_*` (v2 확정 스펙)

`Content/CompanyGrowth/UI/CommonStyle/Text/`의 `UCommonTextStyle` BP. **CommonTextBlock 전용** (plain TextBlock엔 Style 슬롯 없음 → CommonTextBlock 사용 권장).

> **v2 결정 (2026-06-05)**: ① 역할 분리 — 텍스트 계열=NEXON / Number 계열=Pretendard. ② 사이즈를 2560×1440 캔버스 기준으로 재작성 (v1은 1080p 감각으로 작성돼 ~60% 작았음).
> **마이그레이션 스크립트**: `docs/04_ArtDirection/UIChromePrompts/make_text_scale.py` (에디터 Python 콘솔 실행 — 이 스크립트가 스케일의 재생성 SOT).

| 스타일 | 폰트 / Typeface | Size (v2) | (v1 구) | 용도 |
|---|---|---|---|---|
| `Display_L` | NEXON Bold | **52** | 28 | 화면 대표 타이틀 |
| `Display_M` | NEXON Bold | **44** | 22 | 대형 모달 타이틀 |
| `Heading_L` | NEXON Bold | **38** | 18 | 모달/섹션 제목 |
| `Heading_M` | NEXON Bold | **32** | 16 | 카드 제목, 서브섹션 |
| `Body_L` | NEXON Regular | **28** | 14 | 본문 강조 |
| `Body_M` | NEXON Regular | **26** | 14 | 본문 기본 |
| `Body_S` | NEXON Regular | **24** | 12 | 본문 축소 (밀도 높은 패널) |
| `Caption` | NEXON Regular | **20** | 11 | 보조 설명 — **모바일 최소 하한** |
| `Label_L` | NEXON Bold | **26** | 14 | 버튼/칩 라벨 |
| `Label_M` | NEXON Bold | **22** | 12 | 소형 라벨 |
| `Number_L` | Pretendard SemiBold | **34** | 20 | 대표 수치 (재화 카운터 등) |
| `Number_M` | Pretendard SemiBold | **28** | 14 | 일반 수치 (스탯/비용) |
| `Number_S` | Pretendard Medium | **24** | 12 | 보조 수치 |

- 스케일 스타일엔 **색이 안 박혀 있음**(기본 흰색) — 색은 WBP에서 ColorAndOpacity로, 또는 §3 색 토큰으로.
- ⚠ **라이트 플레이트(크림/화이트 카드) 위에는 스케일 스타일 슬롯을 쓰지 말 것** — 기본 흰색이 baked 되어 있어 글자가 되덮이며 사실상 실종된다. 라이트 면에서는 **Font + 잉크 색 직접 지정**(캡션 하한 `#5A6B7D`·22px). 왜/상세 = `UI_STYLE_CATALOG.md` §1.2.
- 재무 패널 **본문**까지 Pretendard로 가야 할 때는 `Fin_*` 변형을 추가하고 이 표에 등록 (현재 미생성 — 필요 시점에).
- **v2 에셋 적용 완료 (2026-06-05, 14/14 — Roboto였던 LargeSize 수리 포함, 디스크 검증됨).** v1 스펙은 에셋 바이너리 추출로 복원한 값 (생성 스크립트가 존재하지 않았음).

## 3. 색 토큰 — `CUI_Text_*` (6종, `make_text_styles.py`가 SOT)

전부 **NEXON Bold / Size 24 고정** (CommonTextStyle은 폰트+크기+색이 한 덩어리 — 색만 따로 못 가짐).

| 토큰 | 색 (sRGB) | 비고 |
|---|---|---|
| `CUI_Text_OnDark` | `#ECEEF0` | 다크 패널 위 |
| `CUI_Text_OnLight` | `#2F2F2F` | 라이트 카드 위 |
| `CUI_Text_OnCream` | `#4B2E2B` | 웜크림 본문 (`UI_CHROME_WARM_CREAM_SPEC.md`) |
| `CUI_Text_OnCream_Sub` | `#8C5A3C` | 웜크림 보조 |
| `CUI_Text_Gain` | `#5A8000` | 상승값 그린 |
| `CUI_Text_ButtonLabel_Green` | `#FFF8F0` + 아웃라인 2px `#4E7A02` | 그린 CTA 라벨 |

**⚠ size 24 baked 함정**: 다른 크기 텍스트에 색 토큰을 지정하면 24로 강제됨. 크기가 다르면 **스케일 스타일(§2) + 색 수동 지정**으로. (장기적으로 크기×색 매트릭스가 필요해지면 토큰 복제 — `make_text_styles.py` 헤더 참조.)

## 4. 사이즈 가이드 (캔버스 2560×1440, DPI 1.0)

- 기준 밴드 (`UI_STYLE_CATALOG.md` §1과 동기): **본문 24~28 / 타이틀 38~44** / 모달 ≈1900×900 / 행높이 ≈90.
- **모바일 최소 가독 하한 (캔버스 px)**: 캡션 **≥20**, 본문 **≥24**, 타이틀 **≥38**. 실기 6" 폰에서 캔버스가 다운스케일되므로(1080p 폰 ≈ ×0.75) 에디터에서 "딱 보이는" 크기는 실기에선 작다 — **여유 있게**.
- 한글은 받침 획 밀도 때문에 같은 px에서 Latin보다 읽기 어려움 → **작은 텍스트에 Light 굵기 금지** (가는 획이 모바일 래스터에서 증발).
- **사이즈 가짓수 통제**: (폰트, 크기, 아웃라인) 조합마다 글리프 아틀라스가 새로 구워짐. 한글은 글리프 수가 많아 조합 남발 시 `FontCache flush` 히치 발생 — §2 티어만 사용. 모바일 폰트 에셋 아틀라스 페이지는 1024×1024 권장.
- SDF 미적용 (래스터 TTF) — `MOBILE_UI_QUALITY.md` §6 참조. FXAA→TSR/MSAA 교체로 텍스트 선명도는 이미 보정됨.

## 5. 재무/숫자 표기 규칙

- **UE 5.4 Slate는 OpenType `tnum`(고정폭 숫자) 토글 불가** — `FSlateFontInfo`에 feature 설정이 없음. Pretendard 기본 숫자는 가변폭이라 자릿수만으로 세로 정렬이 안 맞음.
- **정렬 해법 (표준)**: 숫자 TextBlock **우측 정렬 + `MinDesiredWidth`** (또는 고정폭 SizeBox 셀). 완전 고정폭이 필수인 화면(원장/리더보드)만 자릿수 슬롯(UniformGridPanel) 방식.
- 큰 수는 기존 **`UGlobalUtilFunctions::AbbreviateNumber`(만/억/조) 경유** (`project_number_abbreviation` 메모리 / ResourceWidget 패턴) — 비용=Ceil/보유=Floor, 정확수치는 클릭→ItemTooltip.
- **★ 슬롯 폭 산정 근거 — AbbreviateNumber 는 문자열 길이를 묶는다 (2026-07-29 구현 확인)**: 1만 미만은 콤마 원본이라 최대 `9,999`(5자), 1만 이상은 **유효숫자 3자리 + 단위 1자**라 최대 `9990만`(5자)이고, 넘치면 만→억→조→경으로 승격하므로 **값이 아무리 커져도 5자를 안 넘는다**(`GlobalUtilFuctions.cpp:87` `ApplySignificant(.., 3, ..)` + 승격 로직). ⇒ 숫자 칸 폭은 "이 게임의 최대 수치"가 아니라 **5자 기준**으로 잡으면 된다(22px NEXON 기준 약 66px). 축약을 안 태운 생짜 숫자는 이 보장이 없다 — 자릿수가 무한히 늘어난다.
- **넘칠 때의 안전망 = `ScaleBox`(`Stretch=ScaleToFit` + `StretchDirection=DownOnly`)**: Slate 텍스트는 기본 클리핑이 없어 슬롯을 넘으면 **잘리는 게 아니라 이웃을 침범**한다. 평소 1:1, 넘칠 때만 축소되므로 좁은 칸에 들어가는 숫자/라벨엔 기본으로 씌울 것. 선례: `UI_InGameLayer` 레벨 뱃지(3자리 레벨), `UIE_OfficeProjectCardRequest` 개발 초점 6칸.
- 숫자 폰트는 §2 `Number_*` (Pretendard SemiBold/Medium).
- **좁은 예외 `[확정 2026-08-11]`:** `UCoinFlyoutContainerWidget`이 WBP 없이 네이티브로 만드는 **직원 머리 위 수익 피드백**은 정적인 재무 정보가 아니라 순간적인 게임 피드백이므로 NEXON Bold 36을 사용한다. 이 한 경로만의 예외이며 재사용 `CUI_Style` 티어로 확장하지 않는다. 정적 재무 UI와 `UIE_FundsToast` 금액은 계속 Pretendard를 사용하고, `UIE_FundsToast`는 SemiBold 34를 유지한다.

## 6. 특수문자/글리프 정책

- **이모지만 금지, 특수문자는 허용 + in-game 글리프 검증 필수** (CLAUDE.md 규칙이 SOT. 2026-04-14 em-dash 박스 회귀 이력).
- 현재 표시 문자열에서 쓰는 위험 글리프 (30개 파일): `★ ☆ ▲ ▼ ↑ ▶ × — →`. 새 글리프 사용 전 NEXON/Pretendard가 커버하는지 실기 확인, 박스면 ASCII/`UImage` 대체.
- **NEXON Lv1 Gothic ttf cmap 실측 (2026-07-04, Bold/Regular 동일)**: **없음(→박스)** = `—`(U+2014 em-dash), `–`(U+2013 en-dash). **있음** = `―`(U+2015 전각 줄표), `…`(U+2026), `→`(U+2192), `★`(U+2605), `·`(U+00B7 middot), `-`(U+002D). ⇒ **한글 조판 대시는 서구 관습 `—`(U+2014)가 아니라 `―`(U+2015)** 를 쓸 것(NEXON은 한글폰트라 U+2015만 보유). 목업/CSS가 U+2014를 쓰면 UMG 이식 시 U+2015로 치환. 재확인법(PowerShell WPF): `New-Object System.Windows.Media.GlyphTypeface(uri)` → `.CharacterToGlyphMap.ContainsKey(0x2015)`.
- ⚠ **폴백 패밀리 미설정**: `F_Pretendard`·NEXON 컴포짓 모두 Fallback Font Family가 비어 있음 → 폰트에 없는 글리프는 엔진 기본 폴백 의존. 심볼 누락 사고 재발 시 컴포짓에 심볼 커버리지 넓은 폴백 페이스 추가가 정석 (Font 에셋 에디터 → Fallback Font Family). — TODO §10.

## 7. C++ 텍스트 스타일링 패턴

- **폰트 객체는 원칙적으로 WBP(디자이너) 소유.** C++은 기존 `FSlateFontInfo`를 복사해 Size/LetterSpacing/Outline만 수정: `UButtonWidget::SetTextSize/SetLetterSpacing/ApplyTextOutline`, `UStatRowWidget::SetFontSize`, `UResourceWidget`(Size). 폰트 경로 하드코딩 금지.
  - 예외 1: 에디터 전용 `BuildModalTreeBuilder.cpp` (위젯 트리 생성 툴 — NEXON 28 / Pretendard SemiBold 38 명시).
  - 예외 2: §5의 직원 머리 위 수익 피드백. `UCoinFlyoutContainerWidget`은 WBP 없는 네이티브 행이라 NEXON Bold 36 폰트 에셋을 직접 로드한다. **이 예외를 다른 재무 숫자나 토스트로 일반화하지 않는다.**
- ⚠ **CommonTextBlock `UpdateFromStyle` 함정**: Style이 지정된 CommonTextBlock에 `SetFont()` 해도 스타일이 다시 덮어씀. 코드로 폰트를 직접 지정하려면 **Style을 비워야** 함 (BuildModalTreeBuilder가 이 회피책 사용. v2 스케일 적용 후 Heading_L=38이 되므로 빌더의 회피책은 제거 가능 — TODO).
- C++에는 `UCommonTextStyle` 참조/`SetStyle(텍스트)` 호출이 0 — 스타일 지정은 전적으로 WBP 에디터 작업.
- 런타임 생성 텍스트(FloatingNumber, NiagaraTextEffect 등)도 원칙적으로 폰트는 WBP 소유, 코드는 텍스트/색만 주입한다. 예외는 위 직원 수익 피드백 1건뿐이다.

## 8. 레거시 & 알려진 누수 (정리 대상)

| 항목 | 상태 |
|---|---|
| `CUI_Style_Text_Nexon1_*` 7종 | 구세대 — 신규 사용 금지. ⚠ **이름의 숫자가 실제 크기와 다름** (Bold_10→실제15, Bold_Black_20→30, Regular_Black_15→22.5, Regular_Black_20→30). 소비자 5 위젯: `UIE_EmployeeManageCardInfo`, `UIE_RecruitmentCard`, `UIE_EmployeeListCard`, `UI_LaunchConfirmPanel`, `UI_ProjectReportPanel` |
| `CUI_Style_Text_LargeSize` | ~~엔진 Roboto Bold 22.5 (한글 깨짐 루트)~~ → **NEXON Bold 22.5로 수리 완료** (2026-06-05) |
| 무스타일 WBP 11개 (Roboto 폴백 위험) | `UI_LoadingWidget`(첫 화면), `UI_EmployeeManagePanel`, `UI_UpgradeMain`, `UI_LootBoxLayer`, `UI_EmployeeManageUpgradePanel`, `UIE_EmployeeFullCard`, `UIE_EntityCardFrame`, `UIE_BuildEntityResource`, `UI_Element_Button1`, `UI_Element_Button1WithIcon`, `UI_Element_Button2WithIcon` (마지막 3개는 §레거시 부품이라 교체가 답) |
| 폰트 직접 하드코딩 WBP 72개 | 동작은 정상(NEXON) — 신규 추가만 금지, 손대는 김에 스케일로 점진 마이그레이션 |
| 로딩스크린 | `DefaultGame.ini` AsyncLoadingScreen이 **엔진 Roboto 20/24/32** 지정 — 한글 팁 텍스트가 Roboto 폴백 렌더 |
| `WORLDMAP_NAVIGATION.md` 폰트/텍스트 절 | **superseded** — pt 계층(36/28/21…)과 "특수문자 전면 금지"는 구정책. 이 문서가 대체 |
| `ST_BuildingSkinNames` | 고아 StringTable 스텁(1엔트리, 참조 0) — 레거시 후보 |

**Localization 상태 (참고)**: en(native)+ko+ja 스캐폴드만 구성(`Config/Localization/`), gather/locres 실행 이력 0. 한글은 소스/DataTable의 FText 직접 — **한글 커버리지는 폰트가 항상 책임져야 함**.

## 9. 라이선스 & 소스 보관 (`Font/_Source/`)

| 폰트 | 소스 파일 | 라이선스 파일 |
|---|---|---|
| NEXON Lv1 Gothic | `_Source/NEXON/*.ttf` 3종 (2026-06-05 vendoring) | `_Source/NEXON/NEXON_FONT_LICENSE_NOTICE.txt` |
| Pretendard | `_Source/Pretendard/*.otf` 4종 | `_Source/Pretendard/LICENSE.txt` |
| Bungee | `_Source/Bungee-Regular.ttf` | `_Source/OFL.txt` |

- NEXON 라이선스는 **고지 유지 + 수정 금지** 조건 — 출시 빌드 크레딧 화면에 폰트 고지 1줄 넣는 것을 권장 (TODO).
- 새 폰트 도입 시: 소스+라이선스를 `_Source/`에 같이 커밋하는 것이 규칙.

## 10. TODO

- [x] `make_text_scale.py` 에디터 실행 → v2 스케일 적용 + LargeSize Roboto 수리 (2026-06-05 완료, 14/14)
- [ ] `UI_BuildModal`(Heading_L 소비자) 시각 확인 / BuildModalTreeBuilder의 38 하드코딩 회피책 정리
- [ ] 컴포짓 폰트 Fallback Font Family 설정 (§6)
- [ ] 무스타일 WBP 11개에 스케일 스타일 지정 (우선순위: UI_LoadingWidget → 공용 버튼 3종)
- [ ] 로딩스크린 폰트를 NEXON으로 교체 (DefaultGame.ini AsyncLoadingScreen Font)
- [ ] 레거시 `Nexon1_*` 소비자 5 위젯 → 스케일 마이그레이션
- [ ] 크레딧 화면에 NEXON 폰트 고지
- [ ] (필요 시) 재무 패널 본문용 `Fin_*` Pretendard 변형 추가
