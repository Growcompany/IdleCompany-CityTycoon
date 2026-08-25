# UI_STYLE_CATALOG — 공용 부품 & 스타일 단일 진실

> **새 UI를 디자인하거나 조립하기 전에 이 문서를 우선 참조.**
> - "어떻게 만드는가"(절차/함정/등록) = `UI_CREATION_PLAYBOOK.md`
> - "무엇으로, 어떤 모양으로 만드는가"(부품/토큰/골격) = **이 문서**
>
> 작성: 2026-06-04 (3-에이전트 전수 조사 + 검증 기반)

## 운영 규칙 (비협상)

1. 새 UI는 **반드시 이 카탈로그의 기존 부품 먼저 검토** — 새 위젯/브러시 발명은 카탈로그에 없을 때만.
2. 새 공용 부품(C++ 베이스 or UIE_ 변형)을 만들면 **이 문서에 즉시 등록**.
3. 같은 디자인이 N곳에 복붙된 anti-pattern 발견 시 → 변형 WBP(Class Defaults baked-in) 추출 후 등록.
4. 레거시 부품(§6) 사용 금지 — 발견 시 UIE_ 변형으로 교체 제안.
5. **마감(만듦새) 기본 점검** — 새 UI 요소를 만들 때 아래 "공통 마감 체크리스트"를 반드시 훑을 것. 특히 **월드/맵 위에 뜨는 요소(HUD 버튼·카드·패널·아이콘 액자)는 접지 그림자 필수** — 없으면 "붕 떠" 보이는 이질감(2026-06-08 프로필 버튼 사고).

### 공통 마감 체크리스트 (요소 만들 때마다)
1. **접지 그림자** — 월드/맵 위에 뜨면 하단에 소프트 그림자. 표준 패턴 = `Image` `Shadow_Img`: 텍스처 `Glow_Oval`(`UI/Textures/Gradation/`), TintColor 검정, 슬롯 HAlign_Center·VAlign_Bottom, 맨 뒤(backmost) 슬롯, 필요 시 `RenderTransform Translation Y +` 로 하단 살짝 빼기, `ColorAndOpacity.A`로 농도 조절(0.4~0.6). 적용처: `UIE_ItemCard`(원조), `UI_InGameLayer` 프로필 버튼.
2. **외곽선/키라인** — 배경과 분리되게 1개.
3. **광택/하이라이트** — 입체감(상단 광택 or 림 글로우). 과하면 노이즈.
4. **이웃과 디자인 언어 일치** — 코너 라운딩·마감 수준·재질이 옆 요소와 따로 놀지 않는지.
5. **다크 위 어두운 콘텐츠 대비** — 어두운 아이콘/썸네일은 밝은 안쪽 판(IconPlate) 위에 올려 묻힘 방지.
6. **컬러 CTA 라벨 아웃라인 (2026-07-21 사용자 지시)** — `Btn_Green/Blue` 등 **컬러 버튼에 라벨을 얹으면 아웃라인 색을 버튼 색 계열의 진한 섀이드로 반드시 지정** — 기본 검정/회색 방치 금지 (입장/재배치 회색 아웃라인 사고). 표준: **그린 = 텍스트 `#FFF8F0` + 2px `#4E7A02`**(`CUI_Text_ButtonLabel_Green` 토큰과 동일) / **블루 = 2px `#1B4F79`**. 다른 색 버튼은 같은 원리로 파생(버튼 fill 의 어두운 톤). UButtonWidget 계열 프로퍼티 = `bEnableTextOutline` + `OutlineSize 2` + `OutlineColor`(linear). 공통 버튼을 새 위치에 배치/재스타일할 때 이 항목을 함께 세팅해야 완료로 간주.
7. **비율 대응 (16:10 ~ 19.5:9) — 두 극단에서 확인** (사용자 지시 2026-07-28). 가상 세로는 **항상 1440 고정**, 가상 가로만 **2304(16:10, 갤탭 S8) ~ 3120(19.5:9)** 로 변한다(`UIScaleRule` 기본 ShortestSide + 커브 1440→1.0). ⚠ **디자인 기준 2560 은 최악이 아니라 중간** — 가로 안전 예산은 **2304** 다. 2560 에 꽉 채우면 태블릿에서 256px 가 밖으로 나간다. ⇒ 풀스크린/HUD 는 앵커 기반, 중앙 모달은 폭 고정 대신 `MinDesiredWidth`, 세로 예산은 선검산. 디자이너 Screen Size 를 **2304×1440 과 3120×1440** 으로 번갈아 볼 것(기본 2560 만 보면 최악을 건너뛴다). 상세/사고사례 = `UI_CREATION_PLAYBOOK.md` §4.5
8. **크기 지정 충돌 lint (SizeBox ↔ MinWidth)** — 배치 후 **한 축의 크기를 주장하는 주체가 둘 이상인지** 훑을 것. `SizeBox`의 `WidthOverride`/`HeightOverride`와 자식의 `MinDesiredWidth`/`MinWidth`(또는 임베드한 부품이 내부에 baked 해 둔 SizeBox)가 서로 다른 값을 주장하면 **큰 쪽이 조용히 이겨** 의도한 레이아웃이 안 나온다 — 디테일 패널엔 내가 넣은 숫자가 그대로 보이므로 "숫자는 맞는데 화면이 다르다"로 오인하기 쉽다. 규칙: **크기 권위는 한 곳** — SizeBox 로 잡았으면 자식 `Min*` 은 0, 자식 `Min*` 으로 잡을 거면 SizeBox 오버라이드를 걸지 말 것. **왜**: Slate 는 desired size 를 max 로 합성해서, 두 주장이 공존하면 에러 없이 큰 쪽으로 수렴한다.
9. **크기 고정 vs 최소 — 기본은 최소 (사용자 지시 2026-07-29, 비협상)** — `SizeBox` 를 놓기 전에 **"이건 정말 이 크기인가, 아니면 최소 크기인가"** 를 물을 것. `Width/HeightOverride` 는 desired 를 그 값에 못박아 **콘텐츠가 커져도 안 늘고 부모가 좁아져도 못 줄이는 양방향 실패**다 — "최소폭 역할"을 의도했다면 잘못된 선언이고, 자식이 밖으로 삐져 이웃을 침범한다. **기본은 `MinDesiredWidth`/`MinDesiredHeight`**, 고정은 ① 이미지/썸네일 비율 박스 ② 자체 desired 가 없는 위젯의 그 축만(`UProgressBar` 높이 — §4-A) ③ 배지·행 높이 규격에 한한다. 절차와 함정(`clear_*` 만 하면 죽은 값이 남아 오진을 부름) = `UI_CREATION_PLAYBOOK.md` §4.5-7. 실사고 = 오피스 스트립 `WellBox.WidthOverride=170` → 값 문자열이 길어지자 글자만 바 밖으로.
   - ⚠ **`Max*` 제약을 고정폭으로 읽지 말 것** (2026-07-29 실측): `MaxDesiredWidth`(당시 360, 2026-08-06 부터 **420**) 은 **상한**이라 실제 콘텐츠가 짧으면 그만큼만 차지한다. 따라서 **VerticalBox 의 폭은 "가장 넓은 자식"이 정한다** — 짧은 이름(≈110px) 아래에 긴 행(≈277px)을 넣으면 컬럼 전체가 그 행만큼 넓어져 옆 요소를 밀어낸다. 사고: `UI_OfficeLayer` 배너의 `NameClampBox` 를 360 고정으로 오인해 "가로 예산 0" 으로 계산 → 티어 행을 산업 줄에 병합했더니 재화칩이 밀려남. **가로 예산 계산의 기준은 상한이 아니라 실제 최장 콘텐츠.**
10. **물리 크기(mm) 검산 — 7번이 못 잡는 축** (2026-08-06 실측). 7번의 두 극단은 **둘 다 세로 1440** 이라 터치타겟·폰트의 **물리 크기 문제를 통째로 건너뛴다**(기준 기기 갤탭은 "가로 최악 = 물리 크기 최선"). 폰 환산 상수 **1 virtual px ≈ 0.0495mm**(S24+), 폰트 실제 em = **`Size × 1.333`**(Slate 96 DPI) ⇒ **폰 sp ≈ `Size × 0.355`**. 하한: **터치타겟 141 virtual(≈7mm)**, **텍스트 `FontSize 34`(≈12sp)**, 부제 28(10sp)까지. 88px 버튼은 폰에서 **24dp** 다.
    - ⚠⚠ **SafeZone 은 둥근 모서리를 안 막는다** — 엔진이 `DisplayCutout` 만 읽고 `RoundedCorner` API 는 안 부른다(`AndroidApplication.cpp:155`). 가장자리에 붙는 UI 는 **SafeZone + 명시적 코너 패딩 72** 를 둘 다 걸 것(슬롯 패딩은 기기 인셋에 **합산**된다 — `SSafeZone.cpp:173`).
    - ⚠ **월드 추적 위젯의 부모 캔버스는 SafeZone 으로 감싸지 말 것** — 캔버스가 줄면 화면 밖 판정도 같이 줄어 가장자리 버블/링이 일찍 사라진다. **배경은 풀블리드, 인터랙티브 행(Row)만 감쌀 것.** 표준 예 = `UI_OfficeLayer.OfficeTopBand`(배경 3장 Fill + `BandSafeZone → BandRow`).
    - 상세 = `UI_CREATION_PLAYBOOK.md` §4.6 / `docs/superpowers/specs/2026-08-06-office-band-mobile-safearea-design.md`

---

## 1. 스타일 토큰

### ★ 글로벌 톤 (사용자 확정 2026-06-10 — 새 화면 디자인의 출발점)

**게임 주 컬러 = 다크&그레이(-네이비) 크롬 + 블루 액센트. 웜크림은 카드/칩/플레이트 등 "내용물"에서만.** (SOT: `UI_CHROME_WARM_CREAM_SPEC.md` §0)

| 용도 | 값 | 비고 |
|---|---|---|
| 크롬 배경 | `#1B2230` 베이스 + 상단 시트 `#2A3447` (GradationImg_White_Top 틴트) | 풀스크린/패널 셸 |
| 글래스 서피스 | 화이트 fill 3.5~5% + 헤어라인 8~10% | 레일/플레이트. RoundedBox 또는 풀블리드 |
| 배경 패턴 | `pattern_thin_0029` 타일, 화이트 2~3% | Tiling=Both, AddressX/Y=Wrap |
| 액센트 블루 | `#3D9BE0` (linear 0.047, 0.328, 0.745) | **기능색 전용** — 탭 선택 밴드/포커스/선택 상태. 스타일: `CUI_Style2_TabButton_BlueRail`. ⚠ **장식(타이틀 디바이더 등)에는 쓰지 말 것** — 장식은 크롬 잉크 계열로(§1.1) |
| 크롬 위 텍스트 | 타이틀 `#ECEEF0` / 보조 `#B9C2CF` / 뮤트 `#97A3B6` | |
| 사이드 레일 | 좌측 가장자리 풀블리드 + 우측 헤어라인만 (느슨한 경계, 라운드 카드 금지) | 적용 사례: UI_ShopPanel |
| 타이틀 장식 | **허용** ([확정 2026-07-20 전면 금지 → 철회 2026-07-29]). 표준 = 좌우 대칭 페이드 디바이더 200×6 + 아이콘 48 + 타이틀. 장식 색은 크롬 잉크 `#E6E9F0` 저알파, **블루 금지**. 상세 = §1.1 | |
| 탭 선택 밴드 | 블루 그라데이션 스윕 (GradationImg_White_Left 텍스처 + 블루 틴트, 좌→우 페이드) | `CUI_Style2_TabButton_BlueRail` |

**배치 관행 (사용자 확정 2026-06-10):** `UIE_Resource(_Card)` 등 칩/카운터 부품은 **SizeBox로 감싸 폭(WidthOverride)만 지정 + 슬롯 HAlign_Fill** — 칩이 좌우로 자연스럽게 늘어남. **HeightOverride/ScaleBox 금지**(높이는 칩 원본, 비율 변형 금지 — 2026-06-10 ScaleBox 시도 기각). 위치도 SizeBox 단위로 잡는다. 적용 사례: UI_ShopPanel 상단 지갑(400), UIE_ShopItemCard 가격 칩(240).

### 1.1 ★★ 타이틀/헤더 장식 — 허용 (금지 규칙 철회, 2026-07-29 사용자 지시)

> **[확정 2026-07-20 → 철회 2026-07-29]** 「타이틀 장식 전면 금지」는 폐기됐다. 장식을 붙여도 된다.
> 이 절이 SOT — 아래 문서들의 구 「전면 금지」 서술보다 우선한다:
> `SHOP_GACHA_UI_SPEC.md` · `UI_CHROME_WARM_CREAM_SPEC.md` · `SHOP_PANEL.md` · `WidgetTrees/README.md` · `DOBO_ASSET_INTEGRATION.md`

**표준 헤더 = 라이브 `UI_BuildingManagePanel` 헤더** (`Overlay_151`). 새 패널 헤더는 이걸 복제한다.

```
Overlay
├─ UIE_HelpButton                                    VAlign_Center, 좌
├─ HorizontalBox (HAlign_Center / VAlign_Center)
│   DividerL   Image 200×6, gradient_horizontal_mirror(양끝 페이드), #E6E9F0 A=0.4, padding Right 18
│   Icon       Image 48×48, #E6E9F0
│   Title      TextBlock, NEXON Bold 36, LetterSpacing 42, #E6E9F0
│   DividerR   DividerL 과 동일, padding Left 18
└─ UIE_CloseButton                                   HAlign_Right, VAlign_Bottom
```

- **좌우 대칭 디바이더가 기본형.** 타이틀 좌우로 뻗다가 양끝이 페이드되는 가로 라인.
- **장식 색은 크롬 잉크 계열**(`#E6E9F0` 저알파). ⚠ **블루 `#3D9BE0` 는 여전히 기능색 전용** — 탭 선택/포커스/선택 상태에만. 이 규칙은 철회 대상이 아니다. 장식에 블루를 쓰면 기능색 신호가 희석된다.
- 위계는 장식 **더하기** 타이포·여백·톤·깊이로 세운다 (§1.1b 엘리베이션 계단).

**과거 기각 이력 (금지가 아니라 취향 판정 — 재제안하려면 알고 할 것)**

| 형태 | 이력 |
|---|---|
| 타이틀 밑줄 | 2026-06-10 기각("짜침") |
| 좌측 세로 블루 액센트 바 6×36 | 2026-07-09 기각 → 2026-07-20 재기각 |

이 두 형태는 **규칙 때문이 아니라 그 모양이 별로여서** 기각됐다. 지금은 규칙상 막히지 않지만, 두 번 퇴짜맞은 형태를 다시 들고 가는 건 권하지 않는다. 대칭 디바이더가 실제로 채택돼 라이브에 있는 형태다.

**잔존 액센트 바**: `UI_CodexPanel` — 금지가 풀렸으므로 **급히 제거할 이유는 없다.** 다만 블루라 기능색 원칙에는 여전히 걸린다. 해당 패널을 손볼 때 표준 헤더로 교체하는 쪽을 권장. (가챠 2종 `UI_BuildingTraitGachaPanel`·`UI_OfficeRecuritmentLayer` 은 2026-08-05 표준 헤더로 교체 완료.)

### 1.1b ★★ 표면 엘리베이션 계단 (다크 깊이 축 SOT — 2026-07-23)

**위계 수단 중 깊이(surface elevation) 축이 무표준이었다.** 다크가 4계보로 갈려 있던 것을 확정값으로 승격해 SOT로 못 박는다. (이 절은 §1.1 의 장식 금지 철회와 무관하게 그대로 유효 — 깊이는 장식과 별개 축이다.) **신규 색 발명 0** — 각 계보가 이미 쓰던 값이다.

| 계단 | 색 | 용도 |
|---|---|---|
| **S0** 셸 | `#17191C` (Panel_Float v2 확정, **변경 금지**) | 모달/대형 패널 바닥 셸 |
| **S1** 크롬 | `#1B2230` | 풀스크린/맵 크롬 배경 |
| **S2** 카드 | 셸 위 화이트 fill 3.5~5% (RoundedBox) | 셸 위 콘텐츠 카드/플레이트 |
| **S3** 서브면 | `#2A3447` | 카드 위에 겹쳐 뜨는 서브 요소(칩 웰/토글판) |

- **규칙**: 같은 평면 위에 겹치는 면은 **한 단 밝게**. 딤 위에 뜨는 모달은 밝기가 아니라 **그림자·립**으로 분리(모달을 S3로 올리지 말 것 — 딤이 이미 분리).
- **S3 기성 스타일 = `CUI_Style_Border_S3`** (`UI/CommonStyle/Border/`, 2026-07-27 신설). `CUI_Style_Border_Dark` 복제본으로 코너/브러시 설정은 같고 tint 만 S3. ⚠ **`Border_Dark` 의 실측값은 linear `(0.0156,0.0156,0.0156, A=0.9)` = 무채색 near-black** 이라 검정 배경 위에 얹으면 면이 사라진다(보상 토스트에서 실제 발생). 검정/딤 위 서브면은 `Dark` 말고 **`S3`** 를 쓸 것. 알파도 1.0 — 반투명이면 뒤 검정이 비쳐 밝기 차가 다시 깎인다.
- **층 구분용 1px 스트로크 금지** — 밝기 계단 또는 립으로. 헤어라인은 면 경계가 아니라 광택/키라인 용도.
- **전역 광원 12시 고정** — 라이트 플레이트 상단 하이라이트 금지(§1.2와 정합).
- **반투명 HUD 칩은 계단 준수 대상 제외** — 낮/밤 월드 위에 떠서 배경이 유동적(A0.92 유지).
- 표현은 각 계보가 이미 가진 채움 파라미터(`M_UIPanel_Rounded`의 FillTop/FillBottom/LipHeight)로 — 신규 머티리얼 0.

### 1.2 ★★ 라이트 플레이트 위 텍스트 (크림/화이트 카드 — 텍스트 실종 사고 방지)

밝은 플레이트(웜크림 카드 · 화이트 리포트판 · `Panel_Cream` 계열 콘텐츠 바닥) 위에 텍스트를 올릴 때의 규칙. 다크 크롬 기준으로 만든 스타일을 그대로 가져오면 **글자가 사라진다**.

| 항목 | 규칙 |
|---|---|
| 스타일 슬롯 | **§2 시맨틱 스케일 스타일 슬롯(`CUI_Style_Text_*`)을 지정하지 말 것** — 스케일 스타일에 **흰색이 baked** 되어 있어 라이트 면에서 흰 글자 = 실종. **Font(패밀리/굵기/사이즈)와 잉크 색을 직접 지정**한다 |
| 캡션 잉크 하한 | **`#5A6B7D` 이상 진하게 + 최소 22px** (뮤트 캡션이라도 이보다 옅거나 작게 가지 않는다) |
| 목업 이식 | **웹 목업의 뮤트 색상을 숫자 그대로 옮기지 말 것** — CSS 목업에서 "은은한 회색"으로 보이던 값이 실기기에서는 훨씬 더 옅게 읽힌다. 라이트 면 보조 텍스트는 목업보다 **한 단계 진하게** 잡고 실기 1회 보정 |

- **왜**: 스케일 스타일 슬롯의 baked 화이트가 라이트 배경에서 대비를 소멸시킨다. 게다가 `UCommonTextBlock`은 `SynchronizeProperties → UpdateFromStyle`이 매 컴파일마다 `SetFont`/`SetColor`를 스타일 값으로 **되덮으므로**, C++ 로 색을 다시 칠해도 다음 컴파일에 원복된다(=버그처럼 보이는 실종). 라이트 면에서는 스타일 슬롯을 아예 비우는 것이 유일하게 안정적이다 (`UI_TYPOGRAPHY.md` §7 `UpdateFromStyle` 함정과 동일 뿌리).
- 라이트 면에 얹는 **흰 글리프 공용 부품**은 인스턴스 `ColorAndOpacity` 틴트로 잉크색을 곱해 쓸 것 (`UI_CREATION_PLAYBOOK.md` 레이아웃 함정 절).

### ★ 캔버스 기준 해상도 (공식 — 사용자 확정 2026-06-05)
- **2560×1440 (QHD, 가로)** 가 모든 UI 설계·치수의 단일 기준. 근거: `DefaultEngine.ini` `DesignScreenSize=(X=2560,Y=1440)` + DPI 커브 세로 1440→스케일 **1.0** (1080→0.75, 720→0.5).
- WBP 디자이너 Screen Size, 목업, SizeBox/폰트/패딩 스펙 전부 **이 기준 픽셀**로 잡을 것.
- 참고 치수감: 중앙 대형 모달 ≈ 1900×900 / 타일·버튼 행 높이 ≈ 90 / 본문 폰트 ≈ 24~28 / 타이틀 ≈ 38~44.
- ⚠ 함정: 1600×900·1920×1080 감각으로 치수를 잡으면 실기에서 ~1.6×/1.33× 작아 보임 (2026-06-05 빌드 모달 1차 골조가 이 함정을 밟음 — 1180×480은 1600 기준이었음).

### 아이콘/일러스트 스타일
- **G1 Casual Polish** — `docs/04_ArtDirection/STYLE_GUIDE_G1.md` 가 SOT.
- 핵심 금지: 순백(#FFFFFF) 아이콘 요소(크림/아이보리 사용), gradient shading/글로시/메탈릭, 다크 배경, 이모지.

### 산업 시그니처 색 (6종)
> `CORE_REDESIGN_DIRECTION.md` v2.1 "시각 정체성" 절과 동기 — 변경 시 양쪽 갱신.

| 산업 | Hex |
|---|---|
| 게임 | `#A855F7` |
| IT | `#22D3EE` |
| 금융 | `#E8A93A` |
| 반도체 | `#6366F1` |
| 자동차 | `#EF4444` |
| 가전 | `#F59E0B` |

- 글리프(아이콘) 색: **크림 `#F0E6D0`** — 시그니처색 타일 위에 올릴 때. 순백 금지.
- 적용 방식: 글리프 텍스처는 화이트/크림 1종 재사용, 타일색은 DT(시그니처색)에서 — 베이크 금지(데이터 주도).
- **금융 색상 역할 `[확정 2026-08-11]`**: 산업 타일·업종 칩처럼 **대표색 하나만 쓰는 UI accent는 warm gold `#E8A93A`**를 사용한다. 금융 커버/일러스트의 보조 팔레트에는 에메랄드·틸네이비를 계속 쓸 수 있으며, 대표색 변경을 에메랄드 전면 금지로 확대 해석하지 않는다.

### 등급(Rarity) 색
- `FLootBoxRarityUtility::GetRarityColor()` — `Public/Enum/LootBoxRarity.h:53` 이 코드 SOT.
- 사용처: CardInfoWidget(빌딩 카드 배경), SkinInfoWidget, RecruitmentGameMode. 새 등급 색은 직접 hex 박지 말고 이 유틸 경유.
- 등급별 카드 머티리얼: `MI_TraitCard_{Common,Rare,Epic,Legendary,Mythic}`.

### 패널/버튼 배경 자원 (Content/CompanyGrowth/UI/)
| 자원 | 경로 | 용도 |
|---|---|---|
| `Border_Round{25,50,75,100}`, `Border_Round25_Thick`, `BorderWhite_Round25` | `UITextures/Border/` | 9-slice 라운드 패널/칩 배경 (UBorder 브러시) |
| `M_UIPanel_Rounded` (SDF) + `UIE_PanelBorder` | `Materials/Border/`, `Elements/Border/` | 패널 배경 요소 위젯 — Style `Panel_Float` 등. **`MI_Border_Panel_Float`은 2026-07-20 다크 리스타일**(대형 패널 11개 공용 셸): 근흑 #17191C **상단 광원 11% 그라데이션** + 하단 **립**(두께) + **이중 림**(근흑 아우터 2px / 라이트 헤어라인 3.5px #5A5D60) + 코너 53px@900, 내부그림자·광택 0. ⚠ **이 마스터의 `Gloss*`는 하드밴드, `Tex*`는 가로 줄무늬 — 사용 금지**(실측). 확정값/함정 = `specs/2026-07-20-panel-float-dark-restyle-design.md` §0 |
| **SDF 프레임 키트** — `M_UI_MissionCardBG` + `M_UI_MissionCardLine` (마스터) | `Materials/` | **범용 라운드박스 액자(아이콘/썸네일/카드/칩)**. 이름만 "Mission"일 뿐 미션 전용 아님. ⚠ 마스터 리네임 금지(MissionTracker 참조) — **인스턴스만 새로 찍어 재사용**. 아래 "SDF 프레임 키트 상세" 참조 |
| `MI_Border`, `MI_Border_Gradation/Light/Light_Resource/White` | `Materials/` | Border 머티리얼 변형 |
| `MI_Button_Hover/Pressed(_2,_3)`, `MI_MasterButtonTap` | `Materials/` | 버튼 상태 머티리얼 |
| **`M_UI_TechGrid` + `MI_UI_TechGrid`** | `Materials/` | **패널용 절차적 테크 그리드**(블루프린트 격자 톤). 파라미터: `TileScale`(밀도 — **낮을수록 칸 큼**, 3~6 권장) · `LineColor` · `LineAlpha`. 원리: `TexCoord × TileScale` → `pattern_thick_0016`(Wrap) 샘플, 라인만 표시. **UMG 브러시 Tiling 대체**(아래 ⚠). 튜닝 = MI 스칼라 파라미터(`set_material_instance_scalar_parameter_value`, 툴 `C:\tmp\grid_tile.py` 패턴). 최초 적용: `UI_OfficeMapMain` StripGrid (2026-07-05). 인스턴스: `MI_UI_TechGrid_Chrome`(직원창 풀스크린 크롬, TileScale 9) / `MI_UI_TechGrid_Panel`(패널 셸용, TileScale 6) |
| **`M_UI_GachaBokeh` + `MI_UI_GachaBokeh`** | `Materials/` | **풀스크린 배경 광원 드리프트**(2026-08-05 신설, 가챠 허브 2종). 대형 광원 웅덩이 2겹(`Glow_Oval`) + 미세 그레인(`perlin_noise_small`, 밴딩 디더). 노브 = `Tile1~3`(작을수록 큼)·`Alpha1~3`·`Contrast1~3`·`Tint1~3`·**`Speed1~3`(벡터 R=가로/G=세로)**·`Brightness`. **C++ 틱 0**(머티리얼 자가 애니). ⚠ 함정 3종 = ①발광에 마스크 곱하면 합성이 알파²로 사라짐(발광=가중평균색, 불투명도=마스크) ②UV 비율 보정 필수(`v_tiling` = 텍스처비 × 1440/2560) ③체감 속도 = `Speed×2560/Tile` 이라 타일 키우면 정지로 보임. 상세 = `GACHA_PANELS.md` §10.1 |
| **`M_GachaRevealRT`** / **`M_GachaRevealRT_Window`** | `Materials/` | **가챠 리빌 초상 컷아웃**(RT 샘플). `RT`(Texture) 파라미터, `Emissive=RT.RGB` / `Opacity=OneMinus(RT.A)` — 캡처가 **인버스 알파**(캐릭터 0·배경 1)를 주므로 루마키 없이 실루엣만 뚫는다. `_Window`(2026-08-12 신설, 멀티 뽑기)는 여기에 **`UVWindow`(Vector: R=OffU, G=OffV, B=ScaleU, A=ScaleV, 기본 `(0,0,1,1)`=풀 텍스처)** 크롭을 추가 — 와이드 RT 한 장을 슬롯별 MID 로 잘라 쓴다(`AGachaCaptureStage::GetSlotUVWindow`). ⚠ **VectorParameter 기본 출력은 float3(RGB)** 라 `.ba` ComponentMask 는 컴파일 실패("Not enough components") → 채널 핀(B/A·R/G)을 `AppendVector` 로 조립할 것 (2026-08-12 실측) |
| **`M_UI_NoiseTile` + `MI_UI_NoiseTile_Plate`** | `Materials/` | **표면 그레인(질감) 타일링** (2026-07-21 신설, 강화 모달 v4). `TexCoord × TileScale` → `perlin_noise_small`(Wrap) R 샘플 → `Opacity = R × NoiseAlpha`, 색 = `TintColor`(잉크). 파라미터: `TileScale`(3~4 = 원본밀도) · `NoiseAlpha`(0.04~0.06 권장) · `TintColor`. 라이트 플레이트 위 종이 그레인 용도 — Image 브러시로 풀블리드(라운드 코너 침범 방지 패딩 ~8). ⚠ Panel_Float 마스터는 질감 불가(TexMode=줄무늬)라 이게 정석 대체 |

#### 선별 임포트 텍스처 (dobo_ui 팩 21장 — 2026-07-22)

상세/제외사유/배선법 = **`DOBO_ASSET_INTEGRATION.md` 가 SOT**. 여기엔 "무엇이 있는지"만.

| 폴더 (`UI/Textures/`) | 에셋 | 용도 |
|---|---|---|
| `Effect/ProgressHead/` | `T_UI_ProgressHead_Soft` `_Comet` `_Streak` | 진행바 **채움 끝 글로우 캡** / Streak = 기준선 마커. §4-A 표준 바에는 머리 표시가 없어 이걸로 보강 |
| `Border/Selection/` | `T_UI_SelectBracket_Full` `_Corner` | **4코너 선택 브래킷** — 글로우/링과 달리 대상을 안 가림(썸네일·초상화 선택). 틴트 `#3D9BE0`(선택=기능색) |
| `UIIcon/Rating/` | `T_UIIcon_Star_Round` `_Star_Fill` `_Pip_Rhombus` | 별/핍. 현 `MakeStarString` **텍스트 ★ 대체**. 미획득 = 같은 텍스처 알파 12~20% |
| `Effect/Sparkle/` | `T_UI_Sparkle_Petal4` `_Cross4` `_Bloom4` / `T_UI_Swirl_Thin` `_Comma` / `T_UI_Sunburst_Ring` | **하드엣지** 스파클(기존 Kenney star_*는 소프트 글로우 — 역할 다름, 겹쳐 레이어링) / 나선(신규 형태) / 스피너 링 |
| `UIIcon/` | `T_UIIcon_GridView` `_Announce` `_MapPin` `_Exclaim` | CasualPack 62종에 없던 4종. GridView = `Hamburger`(목록)의 짝 |
| `Border/TreeEdge/` | `T_UI_TreeEdge_Branch` `_Elbow` `_Diagonal` | 노드 그래프 커넥터(로드맵/직능 분기). ⚠ 유일한 **NPOT + NoMipmaps** — 확대 배치 금지 |

- ⚠ **아이콘 마감 컨벤션 = 순백 플랫** (2026-07-22 픽셀 실측): `T_UIIcon_*` 와 `hub_*` 는 **dark 0.0% / meanV 255** 로 외곽선이 없다.
  검은 외곽선이 baked 된 `CasualPack/Icons/B&W`(dark 25.4%) 쪽이 오히려 이질적인 외부 세트다.
  **새 글리프는 순백 플랫로 맞출 것** — 색을 굽지 말고 런타임 틴트(카탈로그 "글리프 화이트 1종 재사용" 원칙과 동일 뿌리).
- ⚠ 위 폴더 중 **`Border/Selection` · `Border/TreeEdge` · `UIIcon/Rating` 은 쿠킹 미등록** — WBP 하드 참조면 무관,
  DT `TSoftObjectPtr` 로 물리면 Additional Asset Directories 등록 필수.

- **브러시는 전부 WBP 쪽에서 지정** — C++ SetBrush 코드 0 (grep 검증). 배경 교체 = 에디터 작업.
- ⚠⚠ **UMG Image 브러시 `Tiling=Both`는 UE5.4에서 반복 안 됨** (2026-07-05 실측: `DrawAs=Image`·`ImageSize=(24,24)`·텍스처 `AddressXY=Wrap` 전부 정상인데도 텍스처 한 장이 위젯 전체로 늘어남. Python readback으로 3속성 다 정상 확인). ⇒ **반복 패턴/그리드는 브러시 타일링 말고 머티리얼 UV 타일링**(`TexCoord × TileScale` → Wrap 텍스처 샘플)로 만들 것 — 정석 자산 = `M_UI_TechGrid`(위 표). 파라미터라 밀도/색/알파 완전 제어 + 재붙여넣기·리컴파일에 안 죽음. (기존 `pattern_thin_0029` 브러시 타일이 됐다면 에디터 셋업 차이일 수 있으나, 신규는 머티리얼이 확실.)
- ⚠ `docs/04_ArtDirection/UIChromePrompts/assets`의 dark_glass/corp_light/maple 크롬은 **docs 전용 목업 — Content 미임포트, 라이브 UI 참조 0**. 적용된 것으로 가정 금지.

#### SDF 프레임 키트 상세 (라운드박스 액자 — 신규 액자/썸네일/칩은 여기부터)

순수 SDF(HLSL signed-distance) 라운드박스 → 해상도 무관·코너 픽셀단위 정합. 9-slice 텍스처보다 깔끔. **새 액자가 필요하면 마스터 인스턴스만 생성**(새 마스터/텍스처 금지).

| 마스터 | 역할 | 파라미터 |
|---|---|---|
| `M_UI_MissionCardBG` | 채움(fill) | `TintCol`(채움색), `RadiusPx`(코너) — 알파는 Image `ColorAndOpacity.A` |
| `M_UI_MissionCardLine` | 외곽선 + 광택(sheen) | `LineCol`, `RadiusPx`, `ThickPx`(선 두께), `GlintWidth/Speed/Gain` — 알파 = `ring*(BaseA + spot*GlintGain)` |

조립 규칙(Overlay 적층, 위→아래 = 앞→뒤):
1. **솔리드 라인**: `M_UI_MissionCardLine` 인스턴스 + `GlintGain=0` + Image A≈1 → 또렷한 키라인
2. **글린트(선택)**: 같은 마스터 별도 Image + `GlintGain>0` + Image A=0 → 라인 색 안 흐리고 광택만 흐름. `GlintSpeed=0`이면 정적 하이라이트, 작게(≈0.15) 주면 "살짝" 도는 광택
3. **채움**: `M_UI_MissionCardBG` 인스턴스 (맨 뒤)
- ⚠ BG/Line/Glint 셋 다 `RadiusPx`·`ThickPx` 동일해야 코너 겹침 정확.
- ⚠ 색은 linear 저장 — sRGB hex 직접 주입 금지(스크립트에서 변환).
- ⚠⚠ **균일 코너만 가능** (`RadiusPx` 단일) — "오른쪽 아래만 직각" 같은 **모서리별(per-corner) 반경 불가**. 모서리별이 필요하면 **SDF 키트 쓰지 말고 아래 "라운드박스 액자(모서리별)" 패턴** 사용.

**⚠⚠ 마스터 3계열 구분 (2026-07-10 HLSL 실측 — 이름만 보고 혼용 금지):**

| 마스터 계열 | 지오메트리 | 크기 파라미터 | 용도 |
|---|---|---|---|
| `M_UI_MissionCard{BG,Line}` | **우측 400px 연장(ext=400 하드코딩) + FadeStart/EndPx 우측 페이드** — 오른쪽 라운드 코너 없음 | Wpx/Hpx 필수(베이크) | 우측 페이드 카드 **고정 크기 전용**(미션 트래커 카드). **대칭 박스에 쓰면 오른쪽 직각 잘림** |
| **`M_UI_FadeCard{BG,Line}`** (2026-08-07 신설) | 위와 같은 실루엣(좌측만 라운드 + 우측 페이드) 인데 **fwidth 자가 측정 + 페이드가 UV 비율(`FadeStartUV`/`FadeEndUV`, 기본 0.82/1.0)** | **없음 — 어떤 크기든 자동** | **크기가 변하는 페이드 카드**(리스트 행처럼 높이가 접힘↔펼침으로 바뀌는 것). 소비자 = `UIE_GoalTrackerRow` |
| `M_UI_Chip{BG,Line}` | 대칭 라운드박스(ext=0) | Wpx/Hpx 필수 — 고정크기=MI 베이크 / **가변크기=UResourceWidget `ChipBG`/`ChipLine` BindWidgetOptional + 지오메트리 MID 주입**(NativeTick, 크기 변화 시 재주입) | HUD 재화 칩(UIE_Resource_Hud) |
| `M_UI_Tray{BG,Line}`, `M_UI_TraySheet` | **fwidth 자가 측정**(크기 파라미터 불필요) + extY=4000 하단 연장(아래 직각) | 없음 — 어떤 크기든 자동 | 하단 도크 셸프(양맵 DockTray). Sheet는 셰이더 내장 상단광(FadeStart/EndPx=UV.y×1000 구간, TintCol.a=바닥 알파 배수) |

**⚠⚠ 크기가 변하는 위젯에 Wpx/Hpx 계열을 쓰지 말 것 (2026-08-07 실측 — 3회 왕복 사고)**: `Wpx/Hpx` 마스터에 C++ MID 주입(`NativeTick`)을 붙이는 방식은 **런타임엔 맞지만 UMG 디자이너에서는 거짓말을 한다** — ① 디자이너 프리뷰는 `NativeTick` 을 돌리지 않고 ② 주입 소스가 보통 `UUserWidget` 의 geometry 인데 카드는 그보다 작은 내부 Overlay 라 크기가 어긋난다. 결과 = MI 베이크값(예: 620×88)이 실제 크기로 늘어나 **코너가 거대해지고 페이드 지점이 밀린다.** 저작자가 실물을 못 보므로 수정 루프가 성립하지 않는다. ⇒ **가변 크기면 자가 측정 마스터**, 고정 크기일 때만 Wpx/Hpx 베이크.
- 자가 측정의 유일한 대가 = **반경/두께가 물리 px 기준**이라 DPI 0.75 기기에서 r14 가 상대적으로 ~33% 커 보인다(페이드는 UV 비율이라 무영향). 소형 반경(≤16)에서는 허용 범위.
- 검증 요령: `RenderingLibrary.draw_material_to_render_target`(월드 컨텍스트 **필수** — `None` 이면 조용히 no-op) 로 620×88 / 620×300 두 크기에 그려 놓고 픽셀을 읽으면 코너·페이드를 정확히 확인할 수 있다.

- fwidth 방식 주의: 반경/두께가 **물리 px 기준**이라 DPI 스케일에서 상대 크기가 살짝 커짐(0.75×폰에서 반경 ~33% 상대 증가) — 대형 요소라 허용, 정밀 요소는 Chip 계열+주입.
- **SDF 채택 기준 (2026-08-12 사용자 지시로 강제 승격)**: 다크 위+키라인+모바일 노출(칩/도크/액자/하단 시트) = **SDF 필수 — 절차식 RoundedBox 금지** / 밝은 면·저대비 가장자리·일회성 장식 = 절차식 RoundedBox 허용 / 리스트 엔트리 수십 개 복제 = 드로우 비용(칩당 반투명 2드로우) 고려 후 결정. 절차식의 알려진 한계 = 밤(다크월드)에 옅은 키라인 코너 얼룩 + 모바일 다운스케일 AA 번짐 (UI_OfficeUpgradePanel 시트/칩이 이 한계로 반려·교체된 실사례).
  - ⚠⚠ **참조 트리를 맹신하지 말 것**: 라이브 WBP 트리를 복제 레퍼런스로 쓸 때 그 트리가 RoundedBox 여도 **SDF 표준 이전의 구식 잔존**일 수 있다 (실사고: 오피스 도크 트레이 = RoundedBox 잔존, 같은 역할의 메인맵 `UI_BuildOpen` 도크 = SDF `MI_UI_TrayBG/TrayLine/TraySheet` 3층이 최신 표준). 재질 선택은 이 채택 기준으로 별도 판정하고, 같은 역할의 타 맵 구현이 있으면 최신 쪽을 따른다.
  - ⚠⚠ **Chip 마스터 기본 `FadeStartPx/EndPx = 360/516`** (2026-08-12 실측): 폭이 그보다 넓은 칩은 우측이 페이드로 잘려 보인다. 페이드 불필요 시 `MI_UI_ResChip*` 선례대로 **99999/100000** 으로 끌 것 — 기존 MI(FilterCombo 등)를 미러할 때 이 두 값은 복사 금지.
  - 하단 시트/도크 표준 스택 = `MI_UI_TrayBG` + `MI_UI_TrayLine` + `MI_UI_TraySheet` 3층 (fwidth 자가측정 — 크기 무관). 라이브 레퍼런스 = `UI_BuildOpen` 트리, 신규 적용 = `UI_OfficeUpgradePanel` (2026-08-12).

#### 채널 마스크 + 머티리얼 (SDF 로 못 그리는 실루엣 — 꼬리·비대칭 형태)

**SDF 키트는 대칭 라운드박스 전용이다.** 하단 꼬리가 붙은 말풍선처럼 union 이 필요한 실루엣은 HLSL 로 짜면 파라미터가 급격히 불어난다. 그럴 때의 표준 = **형태는 SVG, 재질은 머티리얼**.

| | |
|---|---|
| 마스크 | `T_UI_BubbleShell_Mask` (512², **sRGB OFF**) — **R**=채움 / **G**=베젤 링 / **B**=상단 광택 / **A**=전체 실루엣 |
| 머티리얼 | `M_UI_BubbleShell` (MD_UI, Translucent, Unlit) — 마스크 1회 샘플 → `strokeMask = saturate(A - R - G)` 로 외곽선을 역산 |
| 인스턴스 | `MI_UI_BubbleShell_{Vault,Report,NoProject,Seat}` — **`StateColor` 하나만 다르다** |
| 저작 도구 | `docs/04_ArtDirection/UIChromePrompts/svg_bubble_render.js` + `svg_bubble_pack.py`(채널 합성) + `make_bubble_shell.py`(임포트·머티리얼 재생성 SOT) |

- **색은 상태색 1개에서 전부 파생** — `Fill=tint(+26%→-16%)` / `Stroke=tint(-56%)` / `Bezel=tint(+52%)` / `Well=tint(-34%)` / 글리프 아웃라인 `tint(-74%)`. 신규 색 발명 0.
- ⚠ **마스크는 색이 아니라 계수다 — sRGB 를 반드시 끌 것.** 감마가 걸리면 베젤 폭과 광택 경계가 어긋난다. 샘플러 타입도 `SAMPLERTYPE_LINEAR_COLOR`(안 맞으면 컴파일 실패).
- ⚠ **셰이딩은 노드 그래프 대신 `MaterialExpressionCustom`(HLSL) 1개로.** `connect_material_expressions` 는 실패해도 예외 없이 `False` 만 반환하고 결과적으로 **기본 머티리얼로 조용히 대체**되므로, 연결 수를 줄이는 것이 곧 방어다(노드 40 → 8).
- **웰처럼 마스크 채널이 모자란 요소는 셰이더에서 절차적으로** (UV → 라운드 사각 SDF). 채널을 늘리려 마스크를 다시 굽지 말 것.
- 소비자: `UIE_BubbleElement` (건물 버블 4상태). 상세 = `specs/2026-08-05-building-bubble-redesign-design.md`

**등록된 인스턴스** (`Content .../UI/Materials/`):
- 다크 카드(미션): `MI_UI_MissionCardBG`(다크 채움) / `MI_UI_MissionCardLine_Base`(흰 라인 A0.15) / `MI_UI_MissionCardLine_Glint`(움직이는 광택) / `MI_UI_BorderGlint_Mission`
- 미션 트래커 행 카드: `MI_UI_GoalRowBG`(#17191C A0.93, r14) / `MI_UI_GoalRowLine`(흰 라인 — 알파는 위젯 `ColorAndOpacity` 15%) / `MI_UI_GoalRowGlint`(`BaseA=0` `GlintGain=2.2`, 기본 알파 0 = 휴면) — 부모는 **`M_UI_FadeCard{BG,Line}`(자가 측정)**. 크기 파라미터가 없으므로 접힘 88 ↔ 펼침 가변 어느 높이에서도 코너 r14 가 정확하고 페이드는 항상 폭의 82%~100%(`FadeStartUV/FadeEndUV`). **C++ 주입 코드 없음** — 구 `UpdateCardMaterialSize`/`CardFadeStartRatio`/`CardBG·CardLine·CardGlint` BindWidget 은 2026-08-07 삭제. 소비자 = `UIE_GoalTrackerRow`(트리에는 남아 있으나 코드가 참조하지 않는 순수 장식 레이어)
  - ⚠ 구 `MI_UI_GoalTrackerScrim`(리스트 뒤 어둠판)은 **2026-08-07 폐기·삭제 완료** — 행 자체가 페이드 카드가 되어 스크림과 이중으로 겹쳐 탁해졌다. 재도입 금지.
- HUD 재화 칩: `MI_UI_ResChipBG`(다크 A0.92, r16) / `MI_UI_ResChipLine`(화이트 18%, 1.5px) — Wpx/Hpx는 C++ 주입
- 하단 도크: `MI_UI_TrayBG`(다크 A0.92, r14) / `MI_UI_TrayLine`(화이트 16%, 1.5px, r14) / `MI_UI_TraySheet`(r10, A0.89, 상단광 40% 깊이·바닥 0.8) — UI_BuildOpen+UI_OfficeMapMain 공유(노브 하나=양맵 동시)
- 인셋 플레이트(책상 도크 장비 블록): `MI_UI_ChipBG_EquipPlate`(S3 #2A3447, r14, BgA 1) / `MI_UI_ChipLine_EquipPlate`(화이트 9%, ThickPx 1.5, r14, 글린트 0) — 부모 `M_UI_Chip{BG,Line}`. **높이가 변하는 자리라 MI 의 Wpx/Hpx 는 씨앗값이고 실치수는 C++ 주입**(`UWorkstationInfoWidget::UpdateEquipPlateMaterialSize`, NativeTick). Fade 는 99999/100000 로 꺼 둠(마스터 기본 360/516 을 두면 폭 656 우측이 잘린다). 같은 성격(다크 인셋 카드)의 새 플레이트는 이 두 인스턴스를 재사용하고 크기 주입만 붙일 것
- 픽칭 추천 카드(티어1 추천 강조, `UIE_OfficeProjectCardRequest`): 카드 외곽 링 `MI_UI_BorderGlint_Recommend` — ⚠ 이름과 달리 부모는 `M_UI_ChipLine`(대칭 링 계열), `M_UI_BorderGlint`와 무관. 리치골드 linear(0.95,0.52,0.06)·BaseA 0.62·ThickPx 6·Glint W.18/S.45/Orbit1, 640×1010 베이크(Hpx 산출값 — 코너 타원 보이면 이 값만 보정). 배지 3레이어 = `MI_UI_ChipBG_RecommendBadge`(딥골드 바닥)+`MI_UI_ChipBG_RecommendSheen`(밝은 골드 시인 램프)+`MI_UI_ChipLine_RecommendBadge`(골드 림+스윕 1.4s, `GlintGain 0`=스윕만 정지 노브). 값 SOT=에셋(가독 보정 반영), C++는 Visibility 토글만. 라벨 패딩 (20,9,20,5) = NEXON 한글 잉크 비대칭(descent 미사용) 상쇄용 — 대칭 패딩으론 못 맞춤

#### 라운드박스 액자(모서리별 코너) — UMG Border/Image RoundedBox

**모서리별 반경(예: 오른쪽 아래만 직각으로 옆 UI에 "이어지는" 느낌)이 필요하면 SDF 키트 대신 이 방식.** UMG `Image`(또는 `Border`)의 브러시 `DrawAs=RoundedBox`는 `OutlineSettings.CornerRadii=(X,Y,Z,W)` = **(TopLeft, TopRight, BottomRight, BottomLeft)** 로 모서리별 반경 지원(`Z=0`이면 BR 직각). 채움=`TintColor`, 외곽선=`OutlineSettings.Color`+`Width`, `RoundingType=FixedRadius`. 색은 linear(SpecifiedColor) 저장.

- **적용 #1 — `UI_InGameLayer` 프로필 액자 (2026-07-10 v2 확정 — 다크 통일 + 뱃지 도킹)**:
  - 방향 = **완전 다크 통일 + 정체성 블록 결합**. 구 v1(BR 직각 + 라이트판 `#D6DAE0` + 베이크 글린트, 2026-06-08)은 폐기 — 주변 HUD 정리 후 유일한 밝은 큰 면 + 유일한 3겹 마감이라 이질감(명도 아웃라이어)으로 판명.
  - 프레임(`FrameBG`) = `Image` RoundedBox: 채움 `#181B1E` A=0.93 + 림 `#606468` `Width=4`, **`CornerRadii=(30,30,30,30)` 균일** (BR 직각 폐기 — 도킹 뱃지가 그 모서리를 사용).
  - 안쪽 판(`InnerPlate`) = **다크 `#22262B`** RoundedBox 균일 20, padding 10. 다크 위 썸네일 침몰 방지는 라이트판 대신 **글로우(`ThumbGlow`)**: `Glow_Oval` 블루틴트(linear 0.305,0.546,0.831) A 0.4, padding (18,26,18,26), `HitTestInvisible`. 썸네일 묻히면 알파 0.5~0.6로 보정.
  - 썸네일(`ProfileImage`) padding 16, ImageSize 128 (유지).
  - **레벨 뱃지 도킹(`LevelBadgeBox`)** = SizeBox 44×44, 프레임 Overlay 슬롯 HAlign_Right+VAlign_Bottom + RenderTranslation(14,10)으로 우하단 모서리에 반쯤 걸침. 내부 = `LevelBadgeBG`(RoundedBox `#23272C`+림 `#6A7178` 2.5, radius 9, **Angle 45 + pivot 0.5**) + SizeBox30→ScaleBox→`PlayerLevelText`(NEXON Bold 21, `#ECEEF0` — 3자리 레벨도 ScaleDown). **전체 `HitTestInvisible`** — 클릭은 아래 `ProfileImageBtn`으로 통과. 구 마켓 스프라이트 뱃지(BasicFrame_Crimped_01_White) 폐기.
  - **이름 플레이트 tuck** = 플레이트(GradationImg_Right 다크)를 프레임 뒤로 20px 밀어 넣고(TopBlock Overlay에서 프레임이 맨 앞 슬롯) 뱃지가 이음새를 핀 → 프레임+뱃지+이름이 한 조립체(고아 요소 0). 텍스트 시작 x는 재화 칩 열과 기준선 일치.
  - 접지 그림자(`Shadow_Img`) = `Glow_Oval` 검정, 하단 앵커, **맨 뒤 슬롯**, RT Y +16, A 0.55 (공통 마감 체크리스트 §1).
  - 글린트 = **없음**. 구 `T_UI_ProfileGlint`는 BR 직각 외곽선용 베이크라 균일 라운드와 불일치 → 트리에서 제거(텍스처 에셋은 잔존).
  - 적층(ProfileFrameOverlay): Shadow_Img → FrameBG → InnerPlate → ThumbGlow → ProfileImage → ProfileImageBtn(투명) → LevelBadgeBox. **트리 SOT = `WidgetTrees/UI_InGameLayer.tree.txt`**, 픽시트 = `InGameHUD_TopBar_MOCKUP.html`. 루트 Overlay의 페인트 순서는 **`InGameCanvas`(풀블리드 월드 추적, 1번) → `SafeZone > HUDCanvas`(고정 HUD, 2번) → `EffectCanvas`(풀블리드 transient FX, 3번)** 로 고정한다. `EffectCanvas`는 `HitTestInvisible`이며 벽돌 비행·자원 델타 숫자만 호스트한다. 월드 추적 캔버스를 SafeZone 안에 넣어 화면 가장자리 건물이 일찍 사라지게 만들지 말 것.
  - 상단 HUD 배치(같은 트리) = **원 밴드**: `[Money 320] gap 4 [수입 유량 268] gap 20 [Brick 320] gap 20 [Diamond 240] gap 20 [Building 240]`. Money와 유량은 같은 화폐의 저량·유량 한 묶음이고, 재화는 여전히 4종이다. ScaleBox 래핑 금지(§1 배치 관행), 미션 트래커는 바로 아래.
  - ⚠ 움직이는 글린트가 필요하고 + 모서리별 코너도 필요하면 → SDF 마스터에 per-corner 반경(4-radii) 확장이 정석이나 미구현. 현재는 "정지 글린트=베이크 텍스처 / 움직이는 글린트=SDF(균일 코너)" 둘 중 택1.

### CommonUI 스타일 (`Content .../UI/CommonStyle/` — 위젯 Style 프로퍼티 지정용)

**버튼 — `CUI_Style2_Btn_*` 가 게임 표준** (사용자 확정 2026-06-05. 참조 수는 Content 전수 grep):

| 스타일 | 참조 수 | 비고 |
|---|---|---|
| `CUI_Style2_Btn_Blue` | 7 | 주력 |
| `CUI_Style2_Btn_Gray` | 6 | 주력 (취소/보조) |
| `CUI_Style2_Btn_Green` | 5 | 주력 (긍정 CTA) |
| `CUI_Style2_Btn_Gold` / `_Purple` / `_Red` | 각 1 | 강조용 |
| `_Black`, `_Black1`, `_Black_Round`, `_White`, `_Yellow`, `_Tap`, `CUI_Style2_TapButton_Blue` | — | 변형 |
| `CUI_Style2_Btn_HubFlat` | 9 | **매트 플랫 도크 버튼** (구 GlassChip에서 리네임) — 평시 무채움, 눌림=검정 32% 가라앉음(매트 눌림 문법), RoundedBox 반경 14, 머티리얼 참조 없음. MainMap+OfficeMap 하단 도크 허브 버튼. **UIE_HubMenuButton CDO 기본 스타일**(인스턴스 오버라이드 금지 — paste/리컴파일에 소실, 2026-07-04 실측 2회. SOT=specs/2026-07-04-hub-dock-tray-design.md) |
| `CUI_Style2_Btn_CardClear` | 2 | **투명 카드 히트 전용** (normal/selected 완전 투명 + pressed 검정 12%) — 비주얼을 트리가 전담하는 CommonButtonBase 카드의 루트 Style (2026-07-09 신설). 사용처: UIE_EmployeeRosterCard · UIE_BuildingSlotHorizon (**자체 눌림 연출이 없어 이 12%가 유일한 피드백인 카드들**) |
| `CUI_Style2_Btn_CardClear_NoPress` | 3 | **위 스타일의 눌림틴트 제거판** (pressed A=0, 2026-07-27 신설). **카드가 자체 눌림 연출을 가진 경우 전용** — `UEntityCardWidgetBase` 파생은 스케일 0.985 + 4px sink + CardEdge 상쇄로 이미 눌림을 표현하므로 12% 틴트가 중복이다. ⚠ **왜 필요한가**: pressed 브러시는 `DrawAs=Image`+`ResourceObject=None` = **라운드 불가 민무늬 사각형**이고, 버튼 배경이라 트리 **뒤**에 + **버튼 전체 지오메트리**(TileView 타일 320×470)에 깔린다 → 카드 아트(300×450)보다 커서 **라운드 모서리 바깥과 여백만 어두워지는 "직각 네모"** 로 보인다. 사용처: UIE_WorkstationCard · UIE_DecorationCard · UIE_BuildEntityCard |
| `CUI_Style2_Btn_Gray_Sq` | 3 | **`Blue_Sq` 의 회색 짝** (2026-07-29 신설 — Blue_Sq 복제 + 슬레이트 틴트). **라디오 그룹은 지오메트리가 같고 채움만 달라야 한다**: 선택 `Blue_Sq` / 미선택 `Gray_Sq`. 구 `Btn_Gray` 를 미선택으로 쓰면 **모양이 갈린다** — Gray 는 `DrawAs=BOX` + 머티리얼 `M_MasterButton_5_Grdation_Bottom`(알약), Blue_Sq 는 `ROUNDED_BOX r12` 플랫(2026-07-29 CDO 실측). ⚠ Blue_Sq 의 `disabled` 는 **alpha 0**(비활성 시 사라짐)이라 이 짝은 불투명(A=1)으로 남겼다. 사용처: UI_ProductionStartPopup 배율 칩 x10/x50 + 최대 버튼 |
| `CUI_Style2_Btn_Blue_Sq` | 1 | **각진 블루 CTA** (2026-07-21 신설) — RoundedBox **고정반경 12** 플랫, Btn_Blue 상태색 이식(pressed는 머티리얼 음영 상실분 보상해 더 어둡게). ⚠ 기존 `Style2_Btn_*`는 머티리얼+half-height라 **버튼이 클수록 알약** — 각진 카드/표 행 언어(코너 16/10) 옆에는 이걸 쓸 것. 사용처: UIE_BuildingSlotHorizon MoveBtn |
| `CUI_Style2_Btn_Green_Sq` | 1 | **`Blue_Sq` 의 그린 짝** (2026-08-07 신설 — Blue_Sq 복제 + `Btn_Green` 상태색 이식, pressed 는 머티리얼 음영 상실분만큼 더 어둡게 0.82배). **각진 카드 언어 안에서 블루 CTA 와 그린 CTA 가 나란히/교대로 뜰 때** 쓴다 — 구 `Btn_Green` 을 짝으로 쓰면 `BOX`+머티리얼(알약)이라 **모양이 갈린다**(`Gray_Sq` 와 같은 사유). 사용처: `UIE_GoalTrackerRow.ClaimButton`(짝 = `ActionButton` Blue_Sq) |
| `CUI_Style2_Btn_Ghost_Sq` | 1 | **`Blue_Sq` 지오메트리의 고스트(2차 액션) 짝** (2026-08-07 신설 — `Gray_Sq` 복제 + 흰 10% 채움 + 흰 16% 키라인 1.5px, r12 유지). ⚠ **`Gray_Sq` 는 이름과 달리 다크 네이비 불투명**이라 "다크 카드 위 고스트 버튼"으로 못 쓴다(카드보다 어두워져 구멍처럼 읽힘) — 컬러 CTA ↔ 취소/해제 토글에는 이걸 쓸 것. 라벨 아웃라인은 **끌 것**(대비가 없어 얼룩). 사용처: `UIE_GoalTrackerRow.ActionButton` 의 "안내 끄기" 상태(`ActionStyle_Stop`) |
| `CUI_Style2_Btn_StepSq` | 2 | **± 스텝 버튼 (지오메트리 고정판)** — 2026-07-29 신설. `Blue_Sq` 복제, **전 상태 `ROUNDED_BOX` r12** 로 통일하고 색만 바꾼다(활성 블루 / 비활성 근흑 A1). ⚠ **왜 `StatPlus` 를 안 쓰는가**: StatPlus 는 `normal/hovered/pressed = BOX + 머티리얼(알약)` 인데 **`disabled` 만 `ROUNDED_BOX` r10 단색** 이라(2026-07-29 CDO 실측), 수량 스텝퍼처럼 **enabled↔disabled 를 오가는 버튼에서는 누를 때마다 모양이 알약↔각진사각으로 바뀐다**. 상태가 자주 바뀌는 버튼은 이 스타일을 쓸 것. 사용처: UI_ProductionStartPopup `MinusButton`/`PlusButton` |
| `CUI_Style2_Btn_StatPlus` | 1 | **[+] 스텝 버튼 전용** (enabled 블루 / **disabled 회색 tintA=1** 표시). ⚠ **상태별 지오메트리 불일치** — normal 계열은 머티리얼 알약, disabled 만 `ROUNDED_BOX` r10 단색이라 **비활성 전환 시 모양이 바뀐다**. 항상 활성인 [+] 에는 무해하지만, 토글되는 버튼에는 `Btn_StepSq` 를 쓸 것. ⚠ `CUI_Style2_Btn_Blue`의 disabled 는 tintA=0(투명)이라 작은 상주 버튼이 비활성 시 "사라져" 보임 — 회색으로 남겨야 하는 [+]/스텝 버튼은 이걸 쓸 것 (2026-07-09 실측). 사용처: UIE_EmployeeStatRow PlusButton |
| `Trait/CUI_Style_Button_Trait_{Common~Mythic}` | — | 특성 등급별 6종 (⚠ 불투명 머티리얼 배경 — 카드 히트용 아님) |

- 1세대 `CUI_Style_Button_*`(Gradation/Build/Card_*/Item/Skin/Icon_Button* 등)도 잔존 — 신규는 `Style2` 우선, 1세대는 기존 위젯 호환용.
- `CUI_Style_Card_White_Gradation(_WithOutline)` → `M_MasterButton_Gradation_Outline_Inst` 참조 (2026-06-05 실수 삭제→git 복구 이력. **스타일↔머티리얼 의존 주의**).

**⚠ 패널 셸의 코너는 "도크 방향"을 박아둔 값이다 (2026-07-29 실측)**: `MI_Border_Panel`(=`CUI_Style_Border_Panel`, `UIE_PanelBorder` 내장)은 `RoundTL/BL=1 · RoundTR/BR=0` — **왼쪽만 둥근 = 화면 오른쪽 가장자리에 붙는 패널 전용**(BuildingManage/Factory/CityCompanyManage 가 그 방향). 반대편(좌측 도크)에 쓰면 둥근 쪽이 화면 밖으로 잘리고 직각이 안쪽에 남아 거꾸로 보인다. **좌측 도크용 = `CUI_Style_Border_Panel_DockLeft` → `MI_Border_Panel_DockLeft`**(코너 4개만 미러한 복제본, `TexStrength/TexMode=0` 으로 줄무늬 제거. 2026-07-29 신설, 소비자: `UI_ChatPanel`). 또한 이 계열은 **`ShadowMargin=0`** 이라 `Panel_Float`(0.02)과 달리 **인셋 보정이 필요 없고**, 대신 `BorderThickness=0.01`(≈짧은변×0.01) 짜리 밝은 림이 있어 full-bleed 자식은 패딩으로 그 림을 피해야 한다.

**보더 (시맨틱)**: `CUI_Style_Border_{Panel, Panel_Float, Panel_DockLeft, Surface, Chip, IconPlate, Rounded, UpgradeSlot, Dark(_Small), White(2,3), WhiteTopGradation, White_Resource(1)}` + `CUI_Style2_Border_{BlackLine,Blue,Dark,White}`. 모달 패널 = `Panel_Float` — **2026-07-20 v2 다크 플랫 리스타일로 `Style2_Border_Dark`(ConfirmCancel) 계열과 언어 통일** (소비자 11 위젯 일괄 반영, PIE 순회 검수 대기). (`Panel_Float_Cream` 스타일+MI는 참조 0 고아로 2026-07-20 삭제됨. **`Panel_Cream`·`Surface_Cream` 도 참조 0 으로 2026-08-06 삭제** — 아래 크림 경고 참조.)

> **⚠⚠ 9분할 보더가 작게 쓰면 눌리는 이유 = `ImageSize` 스케일 (2026-08-06 실측)**
> **`Border_Round25` 는 단순 라운드 사각형이 아니다.** 530×530 가장자리 프로파일 실측:
> | y | RGBA | 정체 |
> |---|---|---|
> | 0~7 | `(0,0,0, α2→83)` | **외부 드롭 섀도우** |
> | 8 | `(209,209,209, α46)` | **1px 밝은 헤어라인 림** |
> | 9 | `(25,26,29, α230)` | 안쪽 하이라이트 1px |
> | 10~ | `(21,22,24, α230)` | 평면 채움 (코너 반경 **20px**) |
>
> **섀도우 + 림 + 다크 채움 = "떠 있는 플레이트"** 가 이 스타일의 캐릭터다. 텍스처를 빼면 밋밋해진다.
>
> Box 브러시는 **그려지는 코너 = `Margin × ImageSize`**, **소스에서 잘라오는 코너 = `Margin × 텍스처크기`**.
> 둘이 다르면 코너가 확대/축소되고 **섀도우와 림의 절대 두께가 같이 스케일**된다 — 이게 진짜 함정이다.
> - `ImageSize=300` → 25px 소스를 14.2px 로 축소 → **1px 림이 0.57px 가 되어 소실**, 섀도우만 남아 경계가 뭉개진다
> - `Margin=0.5`(원본) → 텍스처 100% 가 코너 → 늘어날 가운데 띠 0, 위젯 절반으로 클램프 → 소형에서 거의 직각
> - ⚠ **텍스처를 손실 없이 축소하는 건 해법이 아니다** — 섀도우/림도 같은 비율로 줄어 결과가 동일하다
>
> ⇒ **소형 요소는 `CUI_Style2_Border_Dark_Small`** (2026-08-06 신설, 소비자: `UI_PotentialOdds`).
> **`ImageSize = 530`(텍스처 크기와 동일) → 1:1 렌더**, `Margin = 0.0566`(=30px, 섀도우 9 + 반경 20 을 전부 포함).
> 섀도우 8px · 림 1px · 반경 20px 이 **위젯 크기와 무관하게 저작한 그대로** 나온다.
> ⚠ **최소 사용 크기 60px**(코너 30×2) — 그보다 작으면 클램프되어 다시 눌린다.
> 생성/근거 = `Tools/PotentialOdds/make_small_border.py` · 판독 = `Tools/PotentialOdds/sim_9slice.py`
> (위젯을 안 띄우고 9분할 결과를 PIL 로 렌더 → 코너 7배 확대 비교. **어두운 배경 위에서 봐야** 림/섀도우가 판정된다).
> 원본 `Style2_Border_Dark` 는 **19개 위젯이 공유**하므로 건드리지 않았다 — 전역 교정은 PIE 순회 검수와 함께 별도 작업으로.
> ⚠ **`RoundedBox`(절차적) 로 대체하지 말 것** — 반경은 정확해지지만 **섀도우와 림을 표현할 수 없어** 플레이트가 밋밋해진다
> (2026-08-06 실제로 전환했다가 되돌림). 텍스처가 주는 건 곡률이 아니라 **입체감**이다.

> **⚠⚠ 크림 보더 계열은 "체계"가 아니다 — 신규 사용 금지 (2026-08-06 실측)**
> 이 표에 `*_Cream` 이 여러 개 나열돼 있어 **크림 플레이트 체계가 살아 있는 것처럼 읽히지만, 실사용은 거의 0이다.**
> 실측(Panels+Elements uasset 스캔): `Style2_Border_Dark` **19** / `Border_Panel` **15** / `Border_Dark` **14** / `Panel_Float` **13** / `White` **10**
> vs `Panel_Cream` **0** · `Surface_Cream` **0** · `Chip_Cream` **1**(`UIE_ShopItemCard`) · `IconPlate_Cream` **1** · `UpgradeSlot_Cream` **1**(둘 다 `UIE_UpgradeSlotHorizon`).
> ⇒ **살아 있는 패널 언어는 다크**다. 남은 크림 3종은 **그 두 위젯 전용**이며 신규 패널에 쓰지 말 것.
> ⇒ **사고 이력**: 2026-08-06 잠재 확률표를 크림 플레이트로 지었다가 전면 재작업(스타일 6개 + C++ 등급색 파생 2곳).
> 원인은 에셋이 아니라 **이 표를 "있는 것"이 아니라 "쓰는 것"으로 읽은 것** — 신규 스타일 채택 전 **사용 횟수를 먼저 셀 것**
> (`grep -rla "<스타일명>" Content/CompanyGrowth/UI/{Panels,Elements}`). 0 이면 그건 체계가 아니라 잔재다.
> ⚠ 크림↔다크는 스타일만 바뀌는 게 아니다 — **텍스트 잉크와 등급색 파생이 통째로 따라온다**
> (라이트면: 잉크 `#39465B`/캡션 `#5A6B7D`/등급색 **×0.55** · 다크면: 잉크 `#ECEEF0`/캡션 `#97A3B6`/등급색 **원색**). ⚠ **`Panel_Float` 등 베젤(장식 테두리+라운드 코너) 프레임은 안쪽 BorderSlot content padding 을 넉넉히(≥40~46 @2560 캔버스) 줄 것 — 빡빡하면 타이틀/내용이 베젤에 물려 "정렬 안 맞아" 보임** (2026-06-10 책상패널 교훈).

**텍스트 — 타이포 전체 = `UI_TYPOGRAPHY.md` 가 SOT** (폰트 역할분리 NEXON 메인/Pretendard 수치/Bungee 특수, 시맨틱 스케일 `CUI_Style_Text_*` v2 스펙, 색 토큰 `CUI_Text_*`, 글리프 정책). 구체 폰트명 계열(`Nexon1_Bold_10` 등)은 구세대 — 신규는 시맨틱 스케일 사용.

---

## 2. 모달/패널 골격 패턴

| 패턴 | 내용 |
|---|---|
| 스택 push | `UIBase::PushPromptClass`(중앙 모달) / `PushBottomClass`(바텀시트). 로드는 `EWidgetType` + `TableMgr->GetWidgetClass()` |
| **모달 열림/닫힘 모션 (SOT — 2026-07-23)** | **스택 컨테이너 전이 설정이 전역 레버** — `UI_Base` 의 `MainStack`/`PromptStack`/`BottomStack`(UCommonActivatableWidgetStack) 프로퍼티에 `TransitionType=Zoom` / `TransitionCurveType=CubicOut` / `TransitionDuration=0.22` 설정 시 스택 경유 패널 **전부** 동시 규격화 + 전이 중 입력 차단 0.4→0.22s. **C++ 0줄.** 개별 WBP 에 Show/Hide 애님을 저작하지 말 것(56개 중 8개만 저작돼 들쭉날쭉했던 원인) — 스택 밖 AddToViewport 오버레이만 코드 폴백(RenderScale, opacity는 스위처 tint 와 곱해져 물빠짐). **리스트 인트로 스태거 (SOT — 2026-08-12 코드 실측)**: `IntroDuration 0.28f` / `IntroSlideX 60.f` / `IntroStagger 0.06f` / `IntroMaxDelay 0.48f` + `EaseOutCubic`. ⚠ 구 서술 「간격 40ms / 총 상한 0.35s」는 **코드와 어긋난 오기**였다(2026-08-12 정정). **소비자 2곳 = `RankingEntryCardWidget`(원본) · `GoalTrackerRowWidget`(2026-08-12 추가)** — 두 리스트가 **같은 리듬으로 읽혀야** 하므로 값을 바꾸면 **양쪽 동시에**, 한쪽만 재튜닝 금지. 지금은 공용 헬퍼 없이 상수를 복제해 두고 있고(리포에 독립 `EaseOutCubic` 정의가 4개인 하우스 스타일), **불변식을 지키는 것은 이 줄과 각 헤더의 주석뿐이다** — 세 번째 소비자가 생기면 그때 `UI/Element/Common/` 으로 추출할 것 |
| **딤(어두운 배경)** | **스택이 깔아주지 않음** (PushPromptClass는 사운드만). 각 WBP가 자체 딤 — ProductSellModal 패턴: 풀스크린 투명 `BackgroundBtn`(클릭=DeactivateWidget) + 위에 `UIE_PanelBorder` |
| 입력 모드 | push 시 `GoToUIMode` ↔ 닫을 때 `GoToNormalMode` 쌍 (호출자 책임) |
| 닫기 | `UIE_CloseButton`(`UCloseButtonWidget::OnCloseClicked`) → `DeactivateWidget()` |
| 헤더 기본 크롬 | 패널 헤더 우측 = 닫기 X(`UIE_CloseButton`) + 도움말 ?(`UIE_HelpButton`). **새 패널은 이 둘을 기본 헤더 구성으로 배치** |
| **⚠ 프롬프트 스택 ConfirmCancel 은 풀 재사용 — 소유 시점에 4종을 리셋할 것 (2026-08-07 실측)** | `PushPromptClass(EWidgetType::ConfirmCancel)` 는 **같은 인스턴스를 돌려쓴다.** 지워야 할 잔류가 바인딩만이 아니다: **`SetConfirmOnly`/`SetCancelOnly` 는 버튼 `Visibility` 를 영구히 바꾼다.** 앞선 소비자가 켜 뒀으면 다음 모달이 **취소 버튼 없이** 뜬다("버튼이 사라졌다" 증상). ⇒ 소유 직후 **`OnConfirm.Clear()` · `OnCancel.Clear()` · `SetConfirmOnly(false)` · `SetCancelOnly(false)`** 를 묶어서 호출. 텍스트도 이전 소비자 것이 남으므로 4종 세터(Title/Message/Confirm/Cancel)를 매번 전부 지정하거나 `RestoreDefaultTexts()`. ⚠ 현재 `BuildingManagePanelWidget::ShowSlotUnlockConfirm` · `CityPlotActor` · `OfficeMainWidget` 은 **바인딩만 지우고 표시 상태는 안 되돌린다** — 같은 증상 대기 중 |
| 공용 모달 프레임 | `UConfirmCancelWidget` — TitleText + `ContentSlot`(UNamedSlot) + Confirm/Cancel + `SetConfirmOnly()`/`bAutoRemove`. 단독 알림 또는 커스텀 UI 주입 둘 다 지원. **2026-07-20 프레임 정비**: 타이틀 NEXON Regular 24 → **Bold 38**(본문 26보다 작아 위계 역전이었음) · 흰 플레이트 `Border_237` 안쪽 여백 10 → **36**(위젯 Padding + BorderSlot 양쪽) · 타이틀 밑 구분선 검정 25% → **화이트 10%**(다크 셸 위라 안 보였음). ⇒ **ContentSlot 에 넣는 콘텐츠는 좌우 인셋 0** 으로 짤 것(프레임이 여백을 소유). 임베드 소비자 3: UI_CityCompanyInfo / UI_LaunchConfirmPanel / UI_ProjectReportPanel. 백업 = `_AssetBackups/2026-07-20_ConfirmCancelFrame/` |
| **베스트 레퍼런스** | `ULaunchConfirmWidget`(`UI_LaunchConfirmPanel`) — ConfirmCancelWidget 임베드 + ContentSlot에 IconCard/StatRow/CloseButton 배치. ProjectReport가 쌍둥이. **새 중앙 모달은 이 골격 클론** |
| **섹션 구분 (정보 블록 분리, 2026-06-11)** | 한 패널/카드에 **성격 다른 내용**(설명/효과/세트, 스탯 그룹 등)을 나열할 때: 각 섹션을 **subtle 배경 플레이트**(`UMG.Border` `Background=RoundedBox`, 흰 **3~5%** A / 의미 있는 섹션은 의미색 틴트 — 예: 세트=그린) + **섹션 사이 간격**(VBox slot padding, 다크 셸이 비쳐 구분) 으로 나누고, 블록 그룹 **위·아래를 얇은 풀폭 라인**(`Image` RoundedBox, height 2, 흰 ~18%)으로 경계. **빈 섹션은 C++ 가 배경 Border째 Collapsed**(텍스트만 토글하면 빈 플레이트가 남음 → 섹션 컨테이너를 BindWidget 으로 노출해 토글). 적용: `UIE_TraitDetailPopup`(설명/효과/세트). 라인은 Trail(`MI_..._Dash_A_Line`)로 대체 가능 |

---

### 2.1 리스트 컨테이너 선택 — "TileView 3박자" 규칙

**아래 3박자가 동시에 성립하면 `ListView`가 아니라 `TileView` + `EntryHeight` 고정을 쓴다.**

1. **가로 스크롤** (`Orientation = Horizontal`)
2. **고정 크기 카드** 엔트리 (SizeBox 로 폭·높이가 못 박힌 카드)
3. 엔트리 루트가 **CommonButton 계열**(`UCommonButtonBase` 파생)

- **증상**: 이 조합에서 `ListView`를 쓰면 엔트리가 **세로로 stretch** 되어 카드가 컨테이너 높이만큼 늘어난다(카드 안 비율이 전부 깨짐).
- **처방**: `TileView`로 교체 + `EntryHeight`(및 `EntryWidth`)를 카드 실치수로 고정. 사용처 예: `UI_BuildModal` BuildItemListView(타일 320×515) · `UI_OfficeWorksationPanel`·`UI_OfficeDecorationPanel`(타일 320×470).
- **엔트리 간격 = `HorizontalEntrySpacing`** (UE5.4 에서 구 `EntrySpacing` 은 deprecated, `Horizontal/VerticalEntrySpacing` 로 분리). 미설정 기본값 **0 = 카드가 서로 맞닿음** — 가로 카드 리스트를 만들면 반드시 지정할 것. **`UIE_EntityCardFrame` 계열 카드의 하우스 규격 = 타일 320 × 간격 12**(카드 실폭 300 → 카드 사이 시각 간격 32). 적용처 3: `UI_BuildModal` BuildItemListView(타일 320×515) · `UI_OfficeWorksationPanel` · `UI_OfficeDecorationPanel`(둘 다 타일 320×470, 시트 높이 제약).
- **⚠⚠ 오해 방지 (중요)**: **`ListView` 자체가 문제인 것이 아니다.** 세로 리스트, 그리고 일반(비-CommonButton) 엔트리에는 `ListView`가 정상이고 계속 쓴다. **위 3박자가 전부 겹칠 때만** 해당한다 — "가로 리스트는 무조건 TileView"로 일반화하지 말 것.
- **왜**: UMG 5.4 에서 이 조합이 엔트리 높이 산정을 무너뜨린다(가로 ListView 는 교차축을 fill 로 취급하는데, CommonButton 엔트리는 자기 desired height 를 컨테이너에 관철하지 못한다).
- **추가 함정 — CommonButton 정렬 리셋**: `CommonButtonBase` 파생 위젯은 **컴파일 시 슬롯 정렬(HAlign/VAlign)이 리셋**된다. 카드 안에서 정렬로 잡아 둔 배치가 컴파일 후 사라졌다면 이걸 의심할 것 — 정렬 대신 SizeBox/패딩으로 위치를 못 박는 편이 안전하다.

---

## 3. 부품 카탈로그 — 버튼 (`Public/UI/Element/Buttons/` → `Content .../Elements/Button/`)

| C++ 베이스 | WBP 변형 | 용도 |
|---|---|---|
| `UButtonWidget` | **`UI_Element_Button`** | 텍스트 CTA 베이스 (확인/취소/건설하기 류). CommonButtonBase 계열. WBP 변형이 ButtonText(UCommonTextBlock) 포함 — **C++ 클래스를 직접 인스턴스하면 내부 트리가 없어 글자가 안 보임**, 반드시 WBP 변형으로 배치 (UI_CompanyTypeCard 등 검증) |
| `UIconWithButtonWidget` | **`UIE_RailTab`** | **좌측 세로 레일 탭 공용** (설정/HQ/채팅). CDO=SOT: Style `CUI_Style2_TabButton_WhiteRail` · IconSize 42 · 높이 104 · 라벨 NEXON Bold 30. 인스턴스는 `Text`/`IconTexture` 만 지정. 배치 규격 = 아래 「탭 3가지 패턴」 패턴 C |
| `UGachaBannerButton` (UButtonWidget 파생) | **`UIE_GachaBannerButton`** | **가챠 배너 셀 공용** (2026-08-05 신설 — 5곳 복붙 추출). 티켓 아이콘 + 이름(`ButtonText` 상속) + 서브 한 줄. **셀 = 버튼 자신** 이라 패널 BindWidget 계약(`BannerNormal` 등, 타입 `UCommonButtonBase*`)이 그대로 유지된다(UUserWidget 합성 래퍼 금지 — 타입 체인 단절). 인스턴스 노브 = `TierIconTexture`/`TierIconSize`(기본 56)/`SubTextValue`, 스타일은 CDO `CUI_Style2_TapButton_Blue`. 소비자: `UI_BuildingTraitGachaPanel`(2) · `UI_OfficeRecuritmentLayer`(3). **2026-08-07 추가**: 우측 **보유 수량 배지** `CountPlate`>`CountRow`>[`CountText`(Pretendard SemiBold 34) + `CountUnit`("장" 22 뮤트)] + API `SetOwnedCount(int32)`. ⚠ **배지는 기본 Collapsed** — `SetOwnedCount` 를 부른 패널에서만 뜬다(안 부르는 특성/스킨에 "0장"이 붙지 않게). ⚠ **배지에 티켓 아이콘을 넣지 말 것** — 좌측 `TierIcon` 과 중복. ⚠ `SubText` 는 `SubScale`(ScaleBox ScaleToFit·**DownOnly**) 안에 둘 것 — 레일 실폭이 16:10 에서 **577px** 라 28px 문구의 여유가 ~15px 뿐(넘치면 Slate 는 클리핑 없이 이웃 침범). 상세=`GACHA_PANELS.md` §5.1 |
| `UIconWithButtonWidget` | `UIE_HubMenuButton` | **MainMap+OfficeMap 하단 도크 공용** (2026-07-04 정정 — "메인 전용" 아님). 아이콘·라벨은 CDO IconPadding(0,0,0,36)으로 **분리**(구 겹침 디자인 폐기). 스타일/아웃라인/패딩 전부 **CDO=SOT**, 인스턴스 오버라이드 금지. **아이콘 세트 = `ButtonIcon/HubDock/hub_*` 9종** (2026-07-10, 8종=청키 솔리드 손SVG 소스 `UIChromePrompts/svg_hubdock_render.js`, hub_inventory=구 shopping-bag 글리프 리스케일 — 세트 광학 규격: 캔버스 대비 아트 75%), **CDO IconColor=#ECEEF0 linear(0.8388,0.8550,0.8714)** — 순백 금지, 기본 아이콘=hub_hq. 상세=specs/2026-07-04-hub-dock-tray-design.md §아이콘 세트 v6 |

**가격/비용 CTA 표준 패턴 (2026-06-10 확정):** 버튼 안에 재화 아이콘+가격을 넣지 말 것. **`UIE_Resource` 칩(가격 표시, SetCanAfford 빨강) + `UI_Element_Button`(행동 라벨 "구매" 등)** 분리 구성. 적용 사례: UIE_ShopItemCard (PriceResource + PriceButton). ※ IconWithButton_Horizon 복권 시도는 MinHeight 미반영으로 재기각 — 레거시 유지.

**⚠⚠ 단, 가격이 버튼 "안"에 보여야 하는 자리는 이미 공용 부품이 있다 — `UIE_CostActionButton`(`UCostActionButtonWidget`) (2026-08-07 등재)**

| | |
|---|---|
| 경로 | `Content/CompanyGrowth/UI/Elements/Button/UIE_CostActionButton` (C++ = `Public/UI/Element/Buttons/CostActionButtonWidget.h`) |
| 구조 | `Overlay` = [ `Btn`(UI_Element_Button_Pattern, **Text 비움** — 배경·히트만) / `VBox`( `CostWidget`(UIE_Resource_Card_BGWhite) + `BtnText` ) ]. 버튼 내부 텍스트는 중앙 정렬 한 줄뿐이라 "가격+라벨" 2단을 못 만들어 나온 해법이고, **가격은 여전히 별도 `UIE_Resource` 부품**이라 위 원칙과 어긋나지 않는다 |
| API | `SetCost(Cost, ResourceType, bCheckAfford=true)` · `SetButtonText` · `SetCanAfford` · `SetEnabled`(비용 게이트 오버라이드) · `SetLockedState` · `SetMaxLevelState` · `OnClicked()/OnPressed()/OnReleased()` |
| 공짜로 딸려오는 것 | **afford 자동 체크** · 부족 시 **입력 삼킴 + 셰이크 + 부족액 토스트**(쿨다운 공유) · 부족→충족 전환 펀치 · 활성/비활성 테두리색 |
| 인스턴스 노브 | `ButtonText` · `DefaultResourceType` · `DefaultCost` · `ButtonStyleClass` · `bOverrideTextStyle`+`BtnTextFont/Color/Padding` |
| 소비자 5 | `UIE_DeskUpgradeSlot` · `UIE_UpgradeSlotHorizon(Mini)` · **`UIE_FactoryLine`** · **`UIE_MineLine`** · `UI_CityCompanyManage`(보석 즉시 완료, 2026-08-07) |

- ⚠ **구 이름 `UIE_UpgradeBtn1` / `UUpgradeBtnWidget` 은 2026-08-07 에 리네임됐다.** "Upgrade" 가 역할을 가려(공장/광산 라인은 이미 강화가 아니다) 카탈로그에 안 잡혔고, 그 탓에 같은 구조를 손으로 복붙할 뻔했다. C++ 클래스 리네임은 WBP 5개가 부모로 직렬화하고 있어 `DefaultEngine.ini` `[CoreRedirects]` 항목이 함께 들어가 있다 — **이 리다이렉트를 지우면 소비자 5개가 부모를 잃는다.**
- ⇒ **비용을 낀 버튼이 필요하면 오버레이를 짓지 말고 이걸 먼저 볼 것.**
| `UDisciplineCellButtonWidget` | `UIE_DisciplineCellBtn` | **직능 투자 셀 버튼** (2026-08-12 신설 → 같은 날 **「강화 버튼 셀」**로 개정) — 셀 몸통이 **`UIE_CostActionButton` 실물 레시피**다: `RootOverlay[ Btn(=`UI_Element_Button_Pattern` 인스턴스, Style `CUI_Style2_Btn_Green`, Text 빈값) · FaceBox(SelfHitTestInvisible 콘텐츠 층) ]` 2층 — 즉 게임 전역 강화 버튼(공장/광산/책상)과 **같은 몸통**이다. **눌림(배경 어두워짐)·비활성(회색)은 스타일이 내장 제공** — 커스텀 상태 브러시 0(구 라이트 키캡+골드 스트립+립, 구 `Gold_Sq` 는 폐기). UCostActionButtonWidget 파생이나 **비용 게이트 미사용**(SP=직원별 자원, 카드가 `SetInvestEnabled` 게이팅). `SetDiscipline(이름,총점,투자분)` 주입, 눌림 트래블=FaceBox Y+3(`PressDepth`, 스타일 pressed 와 병행), 아이콘=인스턴스 `IconTexture`. 레이아웃 = `TopRow`[아이콘웰(화이트 A0.25 r10) + 이름/수치] / **`FootTray`**(RoundedBox r12 · 딥그린 `#14210F` A0.22 · 패딩 11/5/8/5) → `FootRow`[`강화` 라벨(Size=Fill·HAlign_Left) | 흰 SP 칩(Auto·HAlign_Right)] `[개정 2026-08-12 저녁]`. **푸터 두 요소는 반드시 트레이로 묶는다** — 버튼 면 위에 라벨과 흰 칩을 그냥 얹으면 "붙여놓은 스티커"로 읽힌다(사용자 판정 "따로 논다"). 확정 수치: 라벨 NEXON Bold **28**/LetterSpacing **60**, 비용 숫자 Pretendard SemiBold **26**, SP 아이콘 **26**, 칩 r13·패딩 10/4·딥그린 링 `#2E6B25` A0.45 w1.5. 잉크는 그린 면 기준 **크림 `#FFF8F0`**(이름·수치·글리프·`강화` 라벨 전부) + **2px 그린 아웃라인**(linear `0.046553/0.145833/0.037082` — 레퍼런스 `UIE_CostActionButton.BtnText` 실측값 직입, §6 컬러 CTA 규칙), `(+N)` 만 골드 `#FFE06C`(= base `ColorEnabled`). 소비자: UIE_DisciplineCard ×6. ⚠⚠ **`UpgradeButton`(UButton)을 트리에 두지 말 것** — base 가 `UpgradeButton` 과 `Btn` 을 **둘 다 옵셔널 바인딩**해서 둘 다 있으면 이중 배선 = **탭당 2회 투자**. ⚠ 이 가족은 §3 폴더 관례 밖 — C++ `Public/UI/Element/Employee/`, WBP `.../Elements/Employee/`(카드와 동거). 이탈 해소: 칩/라벨 크기는 레퍼런스 대조로 확정(구 20/20/22 = 폰 환산 7sp 로 §10 하한 미달이었다). (구 이탈 「키캡/스트립 그라데이션」·「비용 칩 골드 링」은 강화 버튼 몸통 전환으로 **해소** — 흰 칩은 레퍼런스 `CostWidget`(UIE_Resource_Card_BGWhite) 문법 그대로다.) ⚠⚠ **부품을 재사용한다고 선언했으면 그 부품의 실물 T3D 를 리드백해 값 단위로 대조**할 것 — 구 셀이 들고 있던 `Gold_Sq` 를 레퍼런스로 오독해 한 사이클을 날렸다(2026-08-12). |
| `UCloseButtonWidget` | `UIE_CloseButton` | 공용 닫기 X. `OnCloseClicked`. **BindWidget 멤버 타입은 반드시 `UCloseButtonWidget*` (UButtonWidget 아님 — 틀리면 "Button Widget 바인딩 못 찾음" + 동명 프로퍼티 충돌 컴파일 에러)** |
| (WBP, 전용 C++ 없음) | `UIE_HelpButton` | 패널 헤더 **도움말 ?** 버튼. `UIE_CloseButton`(X)과 함께 **패널 기본 헤더 크롬**. 경로 `/Game/CompanyGrowth/UI/Elements/Common/UIE_HelpButton`. 부모 클래스는 에디터에서 확인(아이콘 버튼 계열 추정) |
| `UIconButtonWidget` | `UIE_IconButton` | 아이콘 단독 버튼 |
| `UMaterialIconButtonWidget` | `UIE_ActiveSkillButton`, `UIE_MenuInBtn` | 머티리얼 아이콘 버튼. ⚠ `OnClicked` 는 **dynamic 델리게이트** — `AddDynamic` + `UFUNCTION()` 핸들러여야 한다(`AddUObject` 아님) |
| `UTabButtonWidget` | `UIE_TabButton`, `UIE_PillTabButton` | 탭 배타선택 (CommonButtonGroupBase와 함께) |

**탭 3가지 패턴** (2026-06-05 등록 / 2026-07-29 패턴 C 추가 — 공존 중, 새 탭은 상황 맞는 쪽 선택):
- **패턴 A — 패널 콘텐츠 전환 탭**: `UIE_TabButton`(UTabButtonWidget) + C++ CommonButtonGroupBase. 예: `UI_BuildingManagePanel`의 강화/특성/스킨 탭.
- **패턴 B — 필터/토글 탭**: `UI_Element_Button`(UButtonWidget) 인스턴스에 Style=`CUI_Style2_TapButton_Blue` 지정. 예: `UI_ProductSellModal`의 산업 탭(전체/전자/반도체/자동차, `IndustryTabBorder` 안). 탭 바 배경 = `CUI_Style_Border_Dark_Small`(⚠ 해당 WBP는 Background 오버라이드 MI_Border 검정 0.9가 함께 있어 실표시는 오버라이드).
- **패턴 C — 좌측 세로 레일 탭 (풀사이즈 패널 표준)**: `UIE_RailTab`(UIconWithButtonWidget 파생, `UI/Elements/Button/`) 을 세로로 쌓고, 레일 자체는 **검정 28% 면 + 우측 헤어라인 흰 8%(폭 2)** 로 셸에서 분리. 탭 높이 **104** · 아이콘 **42**(좌 패딩 34) · 라벨 **NEXON Bold 30**(아이콘과 간격 20). 선택 표시는 스타일 `CUI_Style2_TabButton_WhiteRail`(BlueRail 리컬러)의 좌→우 화이트 스윕 밴드 — 별도 장식 금지. 레일 상단엔 아이콘+패널명, 하단은 `Spacer`(Fill). C++ 은 탭을 **CommonButtonGroupBase 에 직접 등록**(합성 래퍼 아님). 소비자 3: `UI_SettingsPanel`(레일 340) · `UI_HQManagePanel`(v4) · `UI_ChatPanel`(레일 300, 2026-07-29). 트리 참조 = `WidgetTrees/UI_ChatPanel.tree.txt`.
| `UEmploymentButtonWidget` | — | 채용 전용 |
| — | `UIE_CollectAllButton`, `UIE_SubTitle` | 일괄수령 / 섹션 소제목 |

⚠ **버튼 2계보**: CommonButtonBase 파생(`OnClicked()` 네이티브) vs UUserWidget 합성(커스텀 델리게이트). 바인딩 전 타입 확인.

## 4. 부품 카탈로그 — 공통 (`Public/UI/Element/Common/` → `Content .../Elements/Common/`)

| C++ | WBP | 용도 |
|---|---|---|
| `UConfirmCancelWidget` | `ConfirmCancelWidget` | 공용 모달 프레임 (§2) |
| `UResourceWidget` | `UIE_Resource`, `UIE_Resource_Card`, `UIE_BuildEntityResource`, **`UIE_Resource_Hud`**, **`UIE_Resource_Light`** | 재화/비용 칩. `SetCanAfford()`→빨강, 클릭→ItemTooltip. 숫자는 AbbreviateNumber 경유. **Hud 변형(2026-07-10 확정)** = 상주 HUD 밴드 전용: 숫자 **Pretendard SemiBold**(아웃라인 0)·**SDF 칩 배경**(2026-07-10 확정 — 절차식 RoundedBox에서 이식): `ChipBG`(MI_UI_ResChipBG, 다크 A0.92 r16)+`ChipLine`(MI_UI_ResChipLine, 화이트 18% 1.5px) 레이어 + C++ Wpx/Hpx 주입(UResourceWidget::UpdateChipMaterialSize). CommonBorder 절차식 브러시는 무력화(패딩 컨테이너로만 유지). 이식 사유 = 절차식은 밤(다크월드)에 옅은 키라인 코너 얼룩(AA 한계) — SDF로 해소, 미션 카드와 동급 코너·**아이콘 웰**(화이트 7.5%, IconWellOverlay)·패딩 **(12,12,12,12)**(2026-08-10 authored 재실측). 인스턴스 노브 = IconSize/FontSize만(나머지 레이아웃 props는 구조상 휴면). 사용처: UI_InGameLayer 4칩(52/34)·UI_OfficeLayer 4칩(48/30). 원본 UIE_Resource는 NEXON 유지 — 전역 Pretendard 전환은 백로그. **Light 변형(2026-08-07 신설)** = **흰/크림 플레이트 위 전용**: 배경 RoundedBox r12(잉크 6% 채움 + 잉크 10% 키라인 1px, `MI_Border` 근흑 캡슐 대체) · 숫자 Pretendard SemiBold + 잉크 `#232C39` · CDO `IconSize 40 / FontSize 40 / bTextSlotFill true`. ⚠ **라이트 면에 원본 `UIE_Resource` 를 얹지 말 것** — 근흑 A0.35 캡슐 + `DT_Resource.UIColor` 초록이 둘 다 다크 HUD 전제라 유효 대비 **1.7:1** 로 무너진다(2026-08-07 인수 모달 실측). 호출부는 `SetResourceType(Type, /*bUseTypeColor=*/false)` 로 DT 색을 끌 것. 트리 = `WidgetTrees/UIE_Resource_Light.tree.txt`. 사용처: UI_CityCompanyInfo 견적 카드 인수 비용 |
| `URevenueRateChipWidget` | `UIE_RevenueRateChip` | **MainMap 상단 수입 유량 칩** (2026-08-09 신설 → 2026-08-10 Resource HUD 계보·상시 0 상태 통합) — Money 바로 우측에 `0/s` 또는 `+N/s`를 표시해 저량(보유액)과 유량을 한 묶음으로 읽힌다. 회계상 순수익이 아니므로 "순수익" 라벨 금지; 라벨이 필요하면 "수입". **268×74 `UIE_Resource_Hud` 동일 셸**: `ChipBG=MI_UI_ResChipBG` + `ChipLine=MI_UI_ResChipLine`, C++ `Wpx/Hpx` 주입, 사방 12px 패딩, 최대 64px 화이트 7.5% 아이콘 웰, 우측 `ScaleBox` 숫자 기준선. `TrendGlyph=T_UIIcon_TrendUp` 46px, `RateText=Number_L`(Pretendard SemiBold 34 / outline 0). 구 좌측 3×44 민트 `LinkAccent`는 제거했다. `SetRate(<1)`은 칩을 접지 않고 텍스트 `#97A3B6`·글리프 45%의 뮤트 `0/s`로 유지하며, 양수는 민트 `+N/s`다. 펄스는 C++ 소유이며 **0→양수 또는 직전 양수 샘플 대비 5% 이상 변화**에만 **0.45초·최대 1.08**로 반응한다(5% 미만은 텍스트만 갱신). Money와의 간격은 **4px** — **WBP 내부 `ChipBox`를 Collapsed로 저작하면 영구 비표시되므로 금지**. 배치/간격 SOT=`WidgetTrees/UI_InGameLayer.tree.txt`; 상태 SOT=`docs/superpowers/specs/2026-08-10-revenue-rate-zero-state-design.md` |
| `UVaultGaugeWidget` | `UIE_VaultGauge` (Building/) | **MainMap 활성 프로젝트 건물 위 금고 채움 게이지** (2026-08-09 모바일 v2 → 2026-08-10 판독성·운영 게이트 개정) — Bar 기준 **104×30**, 런타임 폭 `clamp(건물 화면폭×0.70, 128, 220)`. required=`GaugeBox`/`FillBar`/`FillTip`/`EndingRim`/`PearlBox`. 근거리 **Bar**·중거리 **Pearl**·원거리 **Hidden** LOD이며 명목 경계 0.50/0.62, 전환 경계 0.48/0.52·0.60/0.64 히스테리시스. 후보 조건은 **그 `BuildingIndex`의 활성 Operation 존재**이며 비운영 건물은 빈 Bar/Pearl을 포함해 전 LOD 숨김이다. Bar는 §4-A `GradientTexture0` + `BarFillStyle=Scale` + Margin 0, **민트 채움 고정**, 2.5px near-black 외곽선, 기존 `Glow_Oval` 검정 A≈0.45 접지 그림자 + 경과 75% 초과 `EndingSoon`에만 **앰버 림**. `Idle` 표현은 후보 산출과 상태 조회 사이의 방어적 레이스 폴백일 뿐 정상 화면 계약이 아니다. 채움 끝 `FillTip`은 트랙 내부 5×22 정적 광택 캡, Pearl은 **28×28 `T_SoftGlow` 민트 정적 이미지**(MID·애니메이션 없음). 같은 건물의 실제 활성 Bubble(Appearing/Idle/Disappearing)이 있으면 Bar/Pearl을 숨기며, 캡 탈락 Bubble 후보는 게이지를 숨기지 않는다. 100%는 `VaultFull` 돈 버블만 표시한다. 풀/LOD/중앙 우선순위는 `UInGameLayerWidget` 소유 |
| `UStatRowWidget` | `UIE_StatRow`, `UIE_StageStatRow`, **`UIE_FailStatRow`** | 이름:값 + ProgressBar 행 (IconTexture 지원). **FailStatRow 변형(2026-08-02)** = 출시 실패 페이지 미달 분야 게이지 — 라이트 트랙(잉크 A0.09) + 레드 채움 `BarFillStyle=Scale` + **50% 고정 최소선 마커**(MinimumScore=Target×0.5 계약) + "미달" 태그. ⚠ C++ 호출 순서 = `SetCurrentProgress` 먼저 → `SetStatInfo` 나중(전자가 StatValueText 를 "획득/목표"로 덮음) |
| `UDisciplineBarWidget` | `UIE_DisciplineBar` | **분야(직능) 세로 미니 바 1칸** (2026-08-12 신설) — 값 / 세로 바 / 라벨 한 칸. API = `SetInfo(이름, 획득, 목표, 채움색)` · `SetEmpty(이름)`(요구 없는 분야 = 값 `―` + 빈 트랙 + 잉크 30%, 축이 6칸 고정이라 "요구 없음"도 칸으로 보인다). 값은 **달성률 % 한 줄**(`"{Floor(획득/목표×100)}%"`, **그룹핑 해제** — `1,234%` 방지). **바만 0~1 클램프, 텍스트는 미클램프**라 초과 달성이 `137%` 로 그대로 보인다(획득 점수엔 상한이 없다). 폭 초과는 `ValScale`(ScaleBox DownOnly)가 흡수. 소비 패널의 열 캡션도 `목표 달성률`(구 "획득 / 목표"). **채움색은 호출자 주입** — `UGlobalUtilFunctions::GetStepColor(SlotIdx+1)` 직능 슬롯 위치색(1파랑~6로즈, orb·플로팅 텍스트와 단일 출처, **1-based 주의**)이라 CDO 에 FillColor 노브가 없다. 미달(획득<목표)이면 **값 텍스트만** 앰버 `#C77E08`(`MissInk`); 잉크 노브 = `ValueInk`/`MissInk`/`NameInk`/`EmptyInkAlpha`, 기본값은 **카드 브라운 `#301D0A`**(값 A0.55 / 라벨 A0.62)로 요청 카드와 정합. 치수 = 바 높이 **58** · 폭은 칸 Fill(고정 금지) · 값·라벨 모두 **NEXON Bold 22**(카드 축자 — Pretendard 아님) · 값/라벨 패딩 5. 브러시는 §4-A 표준이 아니라 **요청 카드 DevBar1 레시피**(FillImage 틴트·아웃라인색 미지정 = 인셋 룩, r10/W2.5 / 트랙 = 브라운 잉크 A0.09 채움 + A0.22 아웃라인 1px — 라이트 웰 전제) + `BarFillType=BottomToTop` + `BarFillStyle=Scale` + 브러시 `Margin` 은 **엔진 기본 0.416667 유지**(0 으로 덮으면 기본 스프라이트 그라데이션이 늘어남 — 아래 §4-A 예외). required BindWidget 3(`Text_Value`/`Bar`/`Text_Name`). 트리 = `WidgetTrees/UIE_DisciplineBar.tree.txt`. 사용처: `UI_LaunchConfirmPanel` 미리보기 · `UI_ProjectReportPanel` 결산서 각 `DiscBar1~6` — **쌍둥이 두 패널이 같은 고정 6축과 같은 캡션("목표 달성률")을 공유**하므로 한쪽만 손대 축이 갈리지 않게 할 것(구 `UIE_StageStatRow_Table` 공유 관계를 그대로 승계) |
| `UOccStatRowWidget` | `UIE_OccStatRow`(다크 글래스), `UIE_EmployeeStatRow`(라이트 카드 — 직원창, 옵셔널 `StatBar` 게이지 포함) | 직원 스탯 1행 (이름/값/[+] 투자 의도 발신). 표시명 = enum DisplayName 데이터 주도 |
| `USeparatorWidget` | `UIE_Separator` | 구분선 |
| `UAlertMarkWidget` | `UIE_AlertMark` | 알림 도트 (3가지 통합 패턴 — project_alertmark_system 메모리) |
| `UDisciplineRadarWidget` | `UIE_DisciplineRadar` (Employee/) | **6축 육각 레이더 차트** — 링/스포크/폴리곤/라벨 전부 NativePaint(MakeLines+MakeCustomVerts+MakeText), WBP 빈 트리(자가 SizeBox 루트). API: `SetRadarData(값6, 상한, 등급색)`/`SetAxisLabels(6)`. 스트로크=등급색×StrokeDarken 자동 파생. 부모 SizeBox로 크기 지정(기본 336×284). 사용처: UIE_DisciplineCard (2026-07-20 v7) |
| `UYieldRangeBarWidget` | `UIE_YieldRangeBar` | **회수 구간 바 차트** — [Min,Max] 균등분포 도박용. 트랙/구간채움/본전 100% 눈금/기댓값 삼각 마커/축 라벨 전부 NativePaint(MakeCustomVerts+MakeLines+MakeText), WBP 빈 트리(자가 SizeBox 루트). API: `SetYieldRange(Min%, Max%, EV%, 등급색)` + **`PlayResultReveal(굴린%)`/`OnRevealFinished`**(인수 결과 니들 착지 ― 스윕(구간 왕복 감속)→착지(수렴)→임팩트(펀치+결과 라벨) ~1.5s. 탭 스킵 = 스윕·착지만 건너뛰고 임팩트는 정상 재생 후 닫힘(임팩트 중 재탭 = 즉시 종료). 니들은 트랙 **하단**에서 위를 찌르므로 EV 마커(상단)·본전 눈금(수직선)과 축이 갈린다. 본전 이상 골드/미만 레드. 노브 = Needle*/Sweep*(SweepCycles=왕복 횟수 포함)/Land*/Impact*). 축 도메인 = max(Max, AxisMinSpan 120). EV<100 이면 마커만 레드(평균 손해 경고). 노브=BarWidth/BarHeight/TrackHeight/CornerRadius/FillAlpha/TickOverhang/MarkerW,H/AxisMinSpan/색3/LabelFont. ⚠ **2026-08-07 부터 소비자 0** — 인수 모달 v10(견적 카드)이 바를 걷어내고 리빌을 숫자 카운트업으로 옮겼다. **클래스는 존치**(v6 정산 화면이 니들 리빌을 되살리는 설계). 다시 쓸 때 고칠 것 = 기댓값 마커(균등분포라 항상 채움 정중앙 = 정보 0) · 축 상한 `max(Ymax,120)`(채움이 늘 오른쪽 끝) · `LabelFont` 기본 21(캡션 하한 22 미달) |
| `URadialProgressWidget` | — (**WBP 없음** — C++ 클래스를 트리에 직접 배치) | **원형 게이지(도넛)** — 트랙 원 + 필 아크를 NativePaint(MakeLines)로. API: `SetPercent(0~1)` / `SetFillColor`. 노브 = `Thickness`·`RingSize`·`TrackColor`·`FillColor` (전부 EditAnywhere, **linear**). **자가 트리** — 디자이너가 루트를 안 넣으면 `SizeBox(RingSize)` 를 스스로 만들므로 **바깥 SizeBox 를 씌우지 말 것**(크기 권위 하나 = `RingSize`, 공통 마감 §9). 중앙에 숫자를 넣으려면 Overlay 로 겹칠 것. T3D 배치 = `Begin Object Class=/Script/CompanyGrowthRenewal.RadialProgressWidget Name="..."`. 사용처: `UI_CityCompanyManage` 회수율 링(560/두께38) · `UI_OfficeMapMain` 개발 타이머 카운트다운(84/두께7, 골드→레드) |
| `UTraitSetChipWidget` | `UIE_TraitSetChip` (Building/) | **특성 세트 보너스 요약 칩** — `SetChipData(라벨, bActive)`, 활성(그린)/뮤트 배경 2장 WBP 소유+C++ 가시성 토글. BuildingManagePanel SetBonusBand 가 EWidgetType::TraitSetChip 로 런타임 생성 (2026-07-21) |
| `UBulkModeSelectorWidget` | `UIE_BulkModeSelector` | **강화 배율 선택기** (x1/x10/x50 배타 라디오 체크 스택) — 빌딩/공장 강화 패널 공용. **자가 트리**(WBP 빈 트리면 C++가 플레이트+체크3 구성, 빈 CanvasPanel 루트도 빈 트리로 간주) + 색/크기/패딩은 `Style|Color`·`Style|Layout` **EditAnywhere 노브**(WBP Class Defaults 가 튜닝 표면 — 기본값=Panel_Float v2 셸 #17191C 계열). API: `SetMode/GetMode` + `OnModeChanged`(네이티브). '최대' 모드 기각(노가다 손맛 — 재도입 금지). 디자이너가 트리를 직접 저작하려면 `BulkCheck_x1/x10/x50` 이름 규약으로 배치(자가 트리 대신 바인딩) |
| `UOfficeEventRailWidget` | `UIE_OfficeEventRail` (Office/) | **Office 이벤트 레일** (2026-07-23 신설, 2026-07-24 재배치) — 상태 토스트(자동 만료)와 이벤트 카드(자기 생명주기)가 한 피드에 쌓이는 스택. 새 엔트리가 **맨 아래**, 기존 것은 위로 밀림. API: `AddStatusToast(문구, 시간, 색)` / `AddEventCard(카드)` / `AddTransientCard(카드)` / `RemoveEventCard(카드)` / `ClearRail()`, 상한 `MaxEntries`(초과 시 오래된 **토스트부터** 제거 — 결정 대기 카드는 최후). ⚠ **자체 크롬 0**(배경/테두리 없음 — 비면 화면에서 사라짐) + **전 트리 SelfHitTestInvisible**(사무실 입력 통과, 입력은 카드 내부 버튼만). 폭 단일 권위 = 트리의 `RailWidthBox.WidthOverride`(480) — 엔트리 쪽에 폭을 또 주지 말 것. 토스트는 전역 알림과 같은 `UNotificationElementWidget` 재사용이지만 **전역 컨테이너와는 별개 인스턴스**(전역 경로 무영향). **호스트 = `UOfficeMainWidget` 의 BindWidget `EventRail`** — 개발 스트립(StripRoot) 바로 아래 세로 스택 `StripRailColumn` 에 레이아웃 배치(2026-07-24 정석 레이아웃 재배선, 구 `UOfficeLayerWidget::GetEventRail()` 우하단 절대앵커 지연생성은 폐기). SizeBox 루트라 비면 480×0 = 무해, VBox 슬롯에 그대로 얹힘 |
| `UNotificationElementWidget` | `UIE_FundsToast` (Notifications/) | **수익 수령 전용 상단 토스트** (2026-08-11) — 일반 알림과 같은 전역 스택·슬라이드 애니메이션을 재사용하되 별도 WBP/`EWidgetType::FundsToast`로 격리한다. 화면 문법은 `[DT_Resource Money 아이콘 48px] gap 8px [FormatFundsAmount(Delta,true)]`, 금액 Pretendard SemiBold 34, 높이 78, 기본 2초, 무음. 금액 색은 `DT_Resource[Money].UIColor`를 따른다. `Image_ResourceIcon`은 optional이며 아이콘 로드 실패 시 접고 숫자는 유지한다. `UIE_Notification`/`UIE_OfficeStatusToast` 에셋 트리는 수정 금지 |
| `UMaterialReqRowWidget` | `UIE_MaterialReqRow` | **재료 요구 1행** (2026-07-29 신설) — 아이콘 · 이름 · `보유 / 필요` · 부족분을 한 줄로. **행 배경 자체가 게이지**(`FillBar` ProgressBar, 트랙 투명 + §4-A 표준 fill): 폭을 안 먹으면서 길이·숫자 두 채널로 부족을 전달한다. 부족 시 `LackTint`(레드 10%) + `LackEdge`(좌 4px) 레이어 토글, 병목 재료엔 `최대 N` 표식(`CapMarkBorder`). API = `SetRequirement(Type, Have, Need, CapMax)` 1콜(구 `SetItem`+`SetInsufficient` 2단 대체). 색 노브 = `Style\|Color` EditAnywhere 4종. 폭 **590 고정**(WrapBox 2열 예산 1212 안에 `590×2 + InnerSlotPadding 16 = 1196`), 아이콘 **48**(프로젝트 자원 아이콘 기준선 = `UIE_Resource` CDO 64, HUD 인스턴스 48~52 — 36 은 이 가족 밖이라 같은 아이콘이 화면마다 달라 보인다). 사용처: `UI_ProductionStartPopup` |
| `UCountrySellRowWidget` | `UIE_CountrySellRow` (WorldMap/), **`UIE_CountryDemandCard`** (WorldMap/) | **국가×산업 수요 게이지 행/카드** — `SetData(Country, Industry)` 한 번으로 `UCountryMarketManager::OnDemandChanged` 자동 구독(구 `UDemandGaugeWidget` 흡수판). 표시는 전부 `BindWidgetOptional` 이라 **WBP 변형마다 쓸 것만 골라 배치**하면 된다: `ProgressBar`/`RatioText`(현재/상한)/`RecoveryText`/`MarketRoleText`(DT_CountryMarketRole)/`HubBadge`/`EfficiencyText`/`IconImage`(국기) + 2026-07-29 추가분 `PriceMulText`(DT_CountryDemand.PriceMul)·`SellMulText`·`IndustryAccentImage`(DT_CompanyInfo.AccentColor 틴트). `GetSellMultiplier()` = `ComputeSell` 의 **수량·주문 무관 항만** 곱한 값(`MoneyBias × DemandMul × IndustryPriceMul × 허브 PortPriceMul`) — 수요 변화에 따라 `ApplyRatioVisuals` 에서 자동 갱신. ⚠ `RowButton` 은 **required** BindWidget 이고 `UCommonButtonBase` 는 Abstract 라 트리에 직접 못 넣는다 → `UI_Element_Button_C` 를 `Text=""` + 투명 스타일로 얹는 히트 레이어 패턴(두 변형 모두 이 방식). 변형 = `UIE_CountrySellRow`(판매 모달 11국 라디오 행) / `UIE_CountryDemandCard`(국가 상세 정보 탭 카드, 라이트 플레이트용) |
| `UOfflineGainRowWidget` | `UIE_OfflineGainRow` (Building/) | **오프라인 정산 모달의 빌딩 1행** — `SetRowData(FOfflineGainEntry)` 1콜. 표시는 `BuildingIcon`/`LossRow`/`LossAmountText`/`LifespanTag` + 2026-07-29 추가분 `IndustryGlyph`·`IndustryText`(업종 칩) 전부 **BindWidgetOptional**. ⚠ **건물 카드와 업종은 다른 축** — `FBuildableCardTable` 엔 CompanyType 이 없어(산업 무관 cosmetic, §7) 업종은 **액터 `GetCompanyType()` → `DT_CompanyInfo`** 로 따로 조회한다. 글리프는 텍스처 1종 + `AccentColor` 틴트(§1 산업 시그니처 색), 업종명은 **`DisplayName`(FText) 경유 필수** — `UMETA(DisplayName)` 은 에디터 전용이라 패키징 빌드에서 영어로 떨어진다. 업종 조회 실패 시 칩 2개 통째 Collapsed. 금고 초과 손실과 운영 수명 만료는 별개 사건이라 표시 분리(수명 종료엔 손실 레드 금지) |
| `UProfileAvatarTileWidget` | `UIE_ProfileAvatarTile` (Profile/) | **프로필 이미지 격자 타일** (2026-08-07 신설) — 정사각 썸네일 1칸. `SetProfileImage(FProfileImageData)` 1콜, 배타 선택은 **패널이 소유한 `CommonButtonGroupBase`** 가 몰아주고 타일은 `NativeOnSelected/NativeOnDeselected` 로 브래킷만 토글한다(수동 `CurrentlySelected` 부기 0). ⚠ 선택 표시 = **`T_UI_SelectBracket_Tile`**(2026-08-07 신규 저작, 소스 `UIChromePrompts/svg_selectbracket_render.js`) — 코너 패스 1개를 `rotate(90/180/270, 50 50)` 로 찍어 **4코너 대칭이 구조적으로 보장**된다. dobo 팩 `_Plate` 는 좌하단 비대칭으로 폐기, `_Corner` 는 회전 배치용 단일 코너라 한 귀퉁이만 나온다. ThumbWell 코너 12 는 브래킷 기하(inset 3 / r 8 / arm 26 / stroke 3 @100 viewBox)에 맞춘 값이고, 타일 168 은 터치 하한 144(§10) 초과 기준. **"사용 중" 배지는 의도적으로 없음** — 폰트 하한 28 을 지키면 타일 절반을 덮고 브래킷과 중복 선택 언어가 된다(`UseBadge` 는 BindWidgetOptional 로만 남김). 루트 Style `CUI_Style2_Btn_CardClear` 는 **paste 후 Python 으로 CDO 에** (T3D silent-drop). 사용처: `UI_ProfileImagePanel` |
| `UItemTooltipWidget` | `UIE_ItemTooltip` (Etc/) | 정확수치 툴팁 |
| `ULaunchLootOddsWidget` | — (**WBP 없음** — 자가 트리) | **출시 보상 전체 확률표 팝오버** (2026-08-12 신설) — 픽칭 피드 `[+N]` 더보기 카드가 띄움. ItemTooltip 식 비모달(viewport add·BelowAnchor 플립/클램프·6초 자동 dismiss·HitTestInvisible), 행 전부 코드 생성(아이콘·이름·조건/확률 — 값은 DT_LaunchLoot 파생). `CreateWidget(StaticClass)` 직접 생성 = EWidgetType 등록이 필요 없는 WBP-less 위젯(선례: RadialProgress·CoinFlyout). 목업 SOT = Artifact `f44bf338` v2 |
| `UGuideTooltipWidget` | `UIE_GuideTooltip` | **튜토리얼 코치마크 말풍선** (2026-08-12 신설) — 아이브로우(대상 이름) + 본문 + 4방향 꼬리. API = `SetContent(Eyebrow, Body)` / `SetTail(Dir, Offset)`, 소유자는 `UMissionGuideOverlayWidget`(`EWidgetType::GuideTooltip` 런타임 생성·풀링·위치 주입). **트리 계약 4건**(어기면 조용히 깨짐): ① `TailBox` 는 **캔버스 직계 자식** — C++ 가 `Cast<UCanvasPanelSlot>(TailBox->Slot)` 로 앵커/얼라인/오프셋을 주입한다(직계가 아니면 꼬리가 저작 위치에 고정 + 경고 로그) ② `TailImage` **RenderTransformPivot (0.5,0.5)** — 0/90/180/270 회전 ③ `TailBox` **정사각 30×30** (Left/Right 가 90/270 회전이라 비정사각이면 회전 삼각형이 어긋남, 30 = C++ `ExplainTailSize`) ④ 전체 폭 **652 < 947** (947 이상이면 배치 순수함수의 "양쪽 다 자리 없음" 폴백 = 대상을 덮음). 구조 = 캔버스[ `Body`(SizeBox MinDesiredWidth 652) → Overlay[ `Shadow_Img`(Glow_Oval 검정 A0.5, 접지) / `Plate`(RoundedBox #2A3447 = S3, 화이트 16% 1.5px, r20 균일, **Margin 0**) / `Content`(VBox) ], `TailBox` **뒤 슬롯**(그림자·외곽선이 꼬리 밑동을 가로지르지 않게) ]. **플레이트는 사방 26px 인셋** — 꼬리 상자가 그 바깥 띠를 돌며 22px 돌출 + 4px 겹침(이음매 은폐). 텍스트 = 아이브로우 NEXON Bold 28 `#3D9BE0`(LetterSpacing 120, 대상 지목이라 기능색 정당) / 본문 NEXON Regular 34 `#ECEEF0` + **`WrapTextAt 540` 명시**(MinDesired 컨테이너 안에서 `AutoWrapText` 는 영영 안 감김). **Style 슬롯은 비움** — 스케일 스타일을 지정하면 `UpdateFromStyle` 이 매 컴파일 폰트/색을 되덮는다. 꼬리 텍스처 = `T_UI_GuideTail`(`UI/Textures/Border/Tooltip/`, 순백 플랫 128², 소스 `UIChromePrompts/svg_guidetail_render.js`) + 플레이트색 런타임 틴트. 트리 저작 스크립트 = `WidgetTrees/_applied_UIE_GuideTooltip.py` |
| `UGestureHintWidget` | `UIE_GestureHint` | **제스처 힌트**(2026-08-22 신설) ― 손 글리프 + 홀드 링 + 라벨로 "꾹 눌러 / 끌어서 [배치] / 탭" 을 그리는 순수 표시 위젯(위치·수명은 소유자 책임). API = `SetGesture(Tap|Hold|Drag)` / `SetLabel(FText ― 빈값이면 라벨 Collapsed)` / `GetGesture()`. **required BindWidget 4** = `MotionFx`(`UGestureHintFxWidget`) / `HandImage` / `HoldRing`(`URadialProgressWidget`) / `LabelText`. **트리 계약 4건**: ① `MotionFx` 는 **캔버스 첫 자식**(HandImage 보다 먼저) ― `SObjectWidget::OnPaint` 가 자식 트리를 먼저 그린 뒤 부모 `NativePaint` 를 부르므로, 탭 점 펄스·드래그 점선을 부모가 직접 그리면 **무조건 손 위에** 얹힌다. 손 아래로 깔려면 그리기를 자식 위젯으로 분리하는 수밖에 없다 ② 원점 = **검지 손끝**(텍스처 47% / 7.4% 지점)이 루트 `SizeBox 320×320` 의 중심에 오도록 `HandImage` 를 배치(104px, `RenderTransformPivot (0.47, 0.074)`), 소유자는 캔버스 슬롯 **AutoSize + Alignment (0.5,0.5)** 로 그 중심을 대상에 둔다 ③ **링 색(Fill/Track)만 WBP Class Defaults 권위** ― `URadialProgressWidget` 의 EditAnywhere 노브라 C++ 가 쓰지 않는다. 반면 **탭 펄스·드래그 점선/화살촉 색은 `GestureHintFxWidget.cpp` 의 화이트 상수**(펄스 알파 A·A×0.8, 점선 0.85)로 **WBP 노브가 없다** ― 바꾸려면 그 상수를 고쳐야 한다 ④ `MotionFx` 슬롯은 **AutoSize 금지 — 320×320 명시 + Alignment(0.5,0.5)**: Fx 의 페인트 원점은 자기 AllottedGeometry 중심이고 desired size 는 0 이다(Fx 의 `RebuildWidget` SizeBox 폴백에 기대지 말 것 ― 슬롯이 권위). 치수 = 손 **104** · `HoldRing` **RingSize 132 / Thickness 9**(채움 화이트, 트랙 화이트 16%) · 라벨 **NEXON Bold 32 + 아웃라인 2 `#0E1522`**(딤 없는 사무실 바닥 위에서도 읽히게). 모션(식 = `Public/UI/Element/Common/GestureHintTypes.h`, 테스트 `CGR.UI.GestureHintMotion`) = 탭 바운스 7px/1.1s + 점 펄스 2겹 · 홀드 손 scale .94 + 링 0→1/1.25s · 드래그 ±44px/1.8s(코사인 ease) + 점선·화살촉. **위상은 제스처가 바뀔 때만 0** ― 같은 제스처를 다시 보여주면 루프가 이어진다. 글리프 = `T_UIIcon_Hand`(`UI/Textures/UIIcon/`, 순백 채움 + `#141C2E` 아웃라인 baked 256², 소스 `UIChromePrompts/svg_hand_render.js`). ⚠ **viewBox 는 `-2 -2 68 68`** ― `0 0 64 64` 로 구우면 아웃라인 stroke 6.5 가 손끝(y=3)에서 화폭 밖으로 나가 **손끝이 잘린다**(목업도 같은 함정). 소비자 2 = `UMissionGuideOverlayWidget`(체인 제스처, 1개 풀 + 앵커 액터 투영) / `UOfficeLayerWidget`(캐치 링 옆 라이브 힌트, 링 풀과 같은 인덱스). ⚠ **둘 다 `HitTestInvisible`** ― 손이 링/월드 위를 덮으므로 히트테스트에 들어가면 그 아래 탭이 죽는다. 등록 = `EWidgetType::GestureHint`(`DT_WidgetClass`). 설계 = `docs/superpowers/specs/2026-08-22-tutorial-gesture-hints-design.md`, 목업 = `mockups/GestureHint_MOCKUP.html` |
| `ULaunchRewardRevealWidget` | `UI_LaunchRewardReveal` (Panels/OfficeMap/) | **출시 보상 리빌**(2026-08-22) — 풀스크린 PromptStack, StageTime 단일 클록(탭=스킵), 카드/슬롯/FX 런타임 조립(`UIE_ItemCard`+`SetCardSize`). 노브 = Class Defaults `Reveal` 카테고리. 판정 라벨/색 = `DT_LaunchLootBand`. 스펙 = specs/2026-08-22-launch-reward-reveal-design.md |
| 기타 | `UIE_BubbleElement`, `UIE_BrickCollection`, `UIE_ProjectNameBar`, `UIE_ScoreOrbContainerWidget`, `UIE_FloatingNumber`(Effects/) | 도메인 공통 |

> **TODO (2026-08-12)** — `UIE_OfficeProjectCardRequest` DevPlate 6칸(`DevBar1~6`+`Text_DevVal/Name1~6`, baked 19 BindWidget)은 같은 문법의 선행 사례 → `UIE_DisciplineBar` 로 **마이그레이션 후보**(단일자식 복붙 anti-pattern → 변형 WBP 추출 규칙). (구 "DevBar1~6 에 `Margin` 잠복 결함 의심" 은 **철회** — 카드 쪽이 정상 기준선이었다, §4-A 예외 참조.) 라이브 카드는 PIE 검증본이라 이번 재구성 범위에서는 손대지 않았다.

### 4-A. 표준 ProgressBar (모든 진행/게이지 바 — 사용자 확정 2026-06-13)

새 `UProgressBar`는 단색 RoundedBox fill 로 만들지 말 것. **fill = `GradientTexture0`(엔진 그라디언트) 텍스처 + `FillColorAndOpacity` 틴트** 가 프로젝트 표준(자동 광택). 색 변경은 **`FillColorAndOpacity` 만** 수정(FillImage 의 ResourceObject/TintColor 건드리지 말 것). 트랙=다크그레이(#292929)+near-black 2px 아웃라인 반경7, `BorderPadding=(2,2)` 로 fill 인셋. `GradientTexture0` 는 엔진 기본 텍스처라 쿠킹 등록 불필요. 적용 SOT: `WidgetTrees/UIE_WorkstationOccupantView.tree.txt` 의 `ExpBar`.

```
Begin Object Class=/Script/UMG.ProgressBar Name="<Name>"
   WidgetStyle=(BackgroundImage=(DrawAs=RoundedBox,TintColor=(SpecifiedColor=(R=0.161458,G=0.161458,B=0.161458,A=1.000000)),OutlineSettings=(CornerRadii=(X=7.000000,Y=7.000000,Z=7.000000,W=7.000000),Color=(SpecifiedColor=(R=0.015625,G=0.015625,B=0.015625,A=1.000000)),Width=2.000000,RoundingType=FixedRadius)),FillImage=(DrawAs=RoundedBox,ImageSize=(X=32.000000,Y=32.000000),OutlineSettings=(CornerRadii=(X=7.000000,Y=7.000000,Z=7.000000,W=7.000000),Width=2.000000,RoundingType=FixedRadius),ResourceObject="/Script/Engine.Texture2D'/Engine/EngineResources/GradientTexture0.GradientTexture0'"))
   Percent=0.350000
   BorderPadding=(X=2.000000,Y=2.000000)
   FillColorAndOpacity=(R=0.263000,G=0.788000,B=0.369000,A=1.000000)   // 색만 교체. 골드=(0.94,0.75,0.25) / 그린=(0.263,0.788,0.369) / 블루=(0.239,0.608,0.878)
   DisplayLabel="<Name>"
End Object
```

ProgressBar 는 자체 desired 높이가 없어 부모를 `SizeBox HeightOverride`(예 16)로 감싸 높이 고정.

**⚠⚠ 라운드 채움을 원하면 `BarFillStyle=Scale` 을 반드시 명시 (2026-07-29 엔진 소스 확인)**: 기본값은 **`Mask`** 이고, Mask 는 채움 이미지를 **트랙 전체 크기로 그린 뒤 비율만큼 하드 클립**한다(`SProgressBar.cpp:217` `bScaleWithFillPerc = BarFillStyle == Scale`, 각 방향 case 의 `PushTransformedClip`). 그 결과 **진행 방향 끝의 라운딩이 잘려나가** "트랙은 둥근데 막대 끝만 각짐" 이 된다 — 브러시의 `CornerRadii` 를 아무리 줘도 안 고쳐지므로 브러시를 의심하다 시간을 버리기 쉽다. `Scale` 은 채움을 **비율 크기의 지오메트리로** 그리므로 네 모서리가 다 살아난다(대신 fill 텍스처가 늘어나는 대신 압축됨 — 그라디언트 광택은 오히려 길이 무관하게 일정해져 유리). 세로 바(`BarFillType=BottomToTop`)에서 특히 눈에 띈다. 적용례: `UIE_OfficeProjectCardRequest` 개발 초점 6칸 미니 바.

**⚠ 라이트 플레이트 위의 바는 트랙 색을 갈아야 한다**: 위 표준 트랙(#292929 + near-black 아웃라인)은 다크 크롬 전제라 크림/화이트 판 위에 올리면 검은 덩어리가 된다. 라이트 면에서는 트랙 = 잉크 저알파(예 `(0.03,0.012,0.003,A=0.09)` + 아웃라인 A=0.22 1px)로 대체할 것. 값 0 인 바를 "빈 트랙"으로 읽히게 하려면 아웃라인이 필수(점선은 RoundedBox 가 지원 안 함 — 실선 1px 로 대체).

**⚠⚠ RoundedBox 브러시의 `Margin` 은 0 이어야 한다 (2026-07-29 엔진 소스 확인)**: `ElementBatcher.cpp:806` 이 `DrawType != Image && Margin != 0` 이면 **9분할(9-slice) 경로**로 보내는데, `RoundedBox` 는 `Image` 가 아니라서 이 조건에 걸린다. 그러면 라운드박스 SDF 가 조각별 UV 로 계산돼 모서리가 엉킨다. **디테일 패널에서 반경을 만지다 `Margin` 이 딸려 들어오는 일이 있다**(실측: 같은 트리의 바 6개 중 5개에 `0.416667` 유입 — 반경 10 / ImageSize 24 비율). 브러시 튜닝 후에는 T3D 리드백으로 `Margin=` 줄이 없는지 확인할 것. 이 함정은 라운딩이 안 나올 때 브러시 반경만 의심하게 만들어 시간을 크게 잡아먹는다.

**⚠⚠ 예외 — ProgressBar 스타일 브러시의 `Margin` 은 건드리지 말 것 (2026-08-12 실측, 직전 서술 반증·커밋 `6cbe2894c` 원복)**: 위 "Margin 은 0" 규칙은 **CDO 기본이 0 인 `UImage`/`UBorder` 브러시 한정**이다. `FProgressBarStyle` 의 `BackgroundImage`/`FillImage` 는 **엔진 기본 `Margin` 이 5/12 = 0.416667** 이고 이건 오염이 아니라 **필수값** — 엔진 기본 채움 스프라이트(`ProgressBar_Fill`, 좌→우 그라데이션)를 9분할의 **가장자리로만** 쓰게 묶어두는 값이다. `ResourceObject` 가 None 이어도 `ResourceName` 으로 그 스프라이트가 실제로 샘플되므로, `Margin` 을 0 으로 덮으면 9분할이 풀려 **그라데이션이 채움 폭 전체로 늘어난다**(증상 = 채움이 오른쪽으로 희미해짐. 색을 어떻게 파생해도 안 고쳐진다 — 색이 아니라 텍스처다). 정상 렌더되는 요청 카드 `DevBar1` 도 0.416667 이다. ⇒ **ProgressBar 브러시는 엔진 기본 `Margin` 을 그대로 둘 것**(손저작 시엔 카드 구조체를 필드째 복사). 2026-08-12 `UIE_DisciplineBar` 가 "Margin 명시" 처방을 따랐다가 이 버그를 만들었다.
> **부수 함정 — 브러시/스타일 구조체 대조에 `repr()` 비교 금지**: 값이 같아도 메모리 주소가 섞여 달라 보인다. **필드 단위 수치 비교**로 diff 할 것(위 원인도 전필드 대조로 좁혀냈다).

**⚠ 반경은 "설정했는가"가 아니라 "보이는가"로 판단할 것**: 85×58 막대에 반경 4px 는 눈에 안 보여 그냥 직각으로 읽힌다. CSS 목업 값을 UMG 로 그대로 옮기기 전에 **그 크기에서 그 반경이 보이는지** 검산할 것(실사고: 목업의 `border-radius:4px` 를 그대로 이식 → "다 사각으로 채워진다" 지적 → 트랙 9 / 채움 10 으로 상향).

#### ★ 새 진행바 만들 때 — 채움 끝 글로우 헤드 기본 적용 (2026-07-22 사용자 지시)

**얇은 게이지(숫자 라벨이 바 밖에 있는 바)를 새로 만들면 헤드를 기본으로 함께 넣는다.** 나중에 덧붙이지 말고 처음부터.
현재 적용 상태 = 가챠 피티 3바 · 직원창 EXP · (직원관리 EXP 는 패널 자체가 사장돼 철회).

| 이 바에 헤드를 다는가 | 판단 |
|---|---|
| 숫자 라벨이 **바 밖**(위/옆)에 있고 바는 진행 감각 전용 | ✅ **기본 적용** — 얇을수록 잘 맞음 |
| 퍼센트 텍스트가 **바 위에 겹친** labeled meter | ❌ 달지 않음 — 같은 질문에 두 번 답하며 자리를 다툼(HQ 종합바 = 시행착오 후 철회 / `UIE_MissionTracker` = 텍스트가 정중앙이라 50%에서 정면 충돌, 착수 전 기각) |
| **리스트/반복 엔트리**의 바 (같은 화면에 N개 동시) | ❌ 달지 않음 — 하나를 강조하는 장치를 N개에 복제하면 강조가 0이 된다(액센트→배경 텍스처). 비용이 아니라 **의미**가 사유. 기각례: 공장/광산/무역 라인, `UIE_CountrySellRow` 11행 |
| **라이트 플레이트 위**의 바 | ❌ 헤드 텍스처는 전부 글로우(백색+알파 감쇠)라 흰 면에서 어둡게 틴트하면 얼룩이 된다. 라이트 면 기준선은 코드 선(`MakeLines`)으로 |

- **레시피/함정 전문 = `DOBO_ASSET_INTEGRATION.md` §5.1 이 SOT** (여기선 "언제 다는가"만).
- 3줄 요약: ① 헤드 `UPROPERTY(meta=(BindWidgetOptional)) UImage*` ② 초기화 1회 `UGlobalUtilFunctions::InitProgressHead(Head)`
  ③ `SetPercent` 직후 `UGlobalUtilFunctions::UpdateProgressHead(Bar, Head, P)` — **크기는 바 두께에서 자동 산출**되므로 픽셀값을 손으로 맞추지 말 것.
- WBP 는 바를 `CanvasPanel` 로 감싸고 헤드를 형제로(앵커 배치). 텍스처 = `UI/Textures/Effect/ProgressHead/T_UI_ProgressHead_Comet`(둥근 halo, 기본) / `_Streak`(수직 광선) / `_Soft`(반달).

### 4-B. 제약(RequiredType) 표시 칩 2종 — 팝업 3상태 / 카드 2상태 (2026-08-13 신설)

특성의 장착 제약(`RequiredType`)을 화면에 알리는 부품. **신규 텍스처 0 · 신규 머티리얼 0 · 신규 WBP 0** — 둘 다 같은 WBP 안의 기존 형제를 복제해 만든 절차식 브러시다.

| 부품 | 위치 | 상태 | 브러시 |
|---|---|---|---|
| **요구 칩** `RequirementChipBorder` + `RequirementText` | `UIE_TraitDetailPopup` 헤더 `EyebrowRow`, 등급 칩(`RarityChipBorder`) **바로 오른쪽** | **3상태 상시** — `공통` / `제조 전용` / `프로젝트 전용` | `RarityChipBorder` 복제. `RoundedBox` r18 · 채움 크롬 잉크 `#E6E9F0` **A 0.10** · 림 같은 색 **A 0.32** W1.5 · 패딩 (14,4,14,4) · NEXON **Regular 20** 잉크 `#B9C2CF` |
| **요구 배지** `RequirementBadgeBorder` + `RequirementBadgeText` | `UIE_TraitCard_QtyBelow` `CardStackOverlay` **우상단** (`OS_ReqBadge`, HAlign_Right/VAlign_Top, padding (Right 8, Top 8)) | **2상태(예외만)** — `제조` / `프로젝트`, `None` 이면 `Collapsed` | 형제 `EquippedBadgeBorder` 복제. `RoundedBox` r7 · 채움 다크 슬레이트 **A 0.88** · 림 크롬 잉크 `#E6E9F0` **A 0.60** W2 · 패딩 (8,3,8,3) · NEXON **Bold 18** |

- **자리 분리 계약**: 카드 좌상단은 기존 **「장착 N」 배지**(`OS_Badge`, HAlign_Left/VAlign_Top, padding (Left 8, Top 8)) 가 이미 쓰고 있어 요구 배지를 **우상단**으로 보냈다 — **둘이 동시에 떠도 겹치지 않는다**(장착 중인 전용 특성에서 실제로 동시에 뜬다). 카드에 세 번째 코너 배지를 추가할 일이 생기면 이 대칭이 먼저 깨진다는 점을 알고 시작할 것.
- **왜 팝업만 3상태인가**: 95행 중 **60행이 무제약**이라 카드 전부에 배지를 찍으면 정보가 아니라 배경 텍스처가 된다(§4-A 진행바 헤드의 "N개에 복제하면 강조가 0" 과 같은 원리). 반대로 팝업은 한 장을 정독하는 자리라 **침묵과 「제약 없음」을 구분**하는 값이 비용보다 크다 — 그래서 `공통` 을 상시 표시한다.
- **둘 다 `HitTestInvisible`** (C++ 가 매 표시 때 확정). 칩이 팝업 헤더의 드래그를, 배지가 카드 본체(`UCommonButtonBase`)의 클릭을 먹으면 안 된다. 텍스트 자식엔 따로 줄 필요 없음 — Slate 가 부모의 `HitTestInvisible` 을 전파한다.
- **C++ 는 텍스트와 가시성만 주입**한다(`ApplyRequirementChip` / `ApplyRequirementBadge`). 색·패딩·폰트는 전부 WBP 소관 — 정적 스타일은 T3D 저작이라는 규칙(§운영 규칙) 그대로.
- **저작 placeholder 규약**: 요구 칩은 `공통`(조회 실패 시에도 무해하고 60/95 행에서 사실), 요구 배지는 `Visibility=Collapsed` 유지(2상태 조건부라 디자이너 기본이 숨김이 맞다). ⚠ 최장 라벨(`프로젝트 전용`)을 placeholder 로 두면 **존재하지 않는 제약을 표시**하므로 금지 — 폭 검산은 디자이너 프리뷰에서 일시 교체로 할 것.
- ⚠ **미해결 후속 — 둘 다 절차식 `RoundedBox` 다.** `:217` 채택 기준 1항("다크 위 + 키라인 + 모바일 노출 = SDF 필수, 절차식 RoundedBox 금지")에 **정면으로 걸린다.** 이번에 유지한 이유는 ① 등급 칩(`RarityChipBorder`)이 절차식이라 **바로 옆 칩과 재질 문법이 갈리는 것이 더 나쁘고** ② SDF 전환은 `MI_UI_Chip{BG,Line}` 인스턴스 신규 + 라벨 길이가 3상태로 변하므로 **가변 폭 = 크기 주입**(`:212` Wpx/Hpx 함정)까지 딸려온다. ⇒ 마이그레이션은 **`EyebrowRow` 행 단위**(등급 칩 포함 동시 전환)로 하고, 착수 전에 칩당 반투명 2드로우의 실측이 필요하다(카드 배지는 리스트 N개 복제라 드로우 비용 판정 대상).

## 5. 부품 카탈로그 — 카드 (`Public/UI/Element/Cards/` → `Content .../Elements/Cards/`)

**베이스 계층**: `UCardWidgetBase` / `UEntityCardWidgetBase`(Frame+CardInfo 합성, `HandleCardClicked` PURE) / `UIconCardWidget`(아이콘+텍스트+선택 Glow, getter 오버라이드식).

| C++ | WBP | 용도 |
|---|---|---|
| `UIconCardWidget` | — | 범용 카드 베이스 (선택 상태 Glow 내장) |
| `UCompanyTypeCardWidget` | **`UIE_IndustryTile`** | **산업 선택 버튼(타일)** — `SetCompanyType`/`OnCardClicked`, 선택=AccentColor 채움+크림 글리프+흰 링 / 미선택=WBP 뉴트럴+산업색 글리프. 디자인타임 DT 프리뷰 내장. (구 `UI_CompanyTypeCard` 일러스트 카드는 2026-06-05 삭제 예정 — Build 2b에서 `CompanyTypeImage` BindWidget도 Optional 강등) |
| `UBuildEntityCardWidget` + `UEntityCardFrameWidget` + `UCardInfoWidget` | `UIE_BuildEntityCard`, `UIE_EntityCardFrame`, `UIE_CardInfo`(+`UIE_CardInfo1`) | 빌딩 카드 (ListView Entry). 등급 배경=GetRarityColor, 비용=UIE_Resource, LockBorder |
| 도메인 카드 | `UIE_{TradeOrder,CountryRouter,EventChoice,ItemCard(+QtyBelow/QtyInside),MineResource,Production,RankingEntry,Trait(+QtyBelow),Workstation,Decoration}Card` 등 | 각 시스템 전용 — 새 카드 만들기 전 유사 카드 확인. `UIE_TraitCard_QtyBelow` 의 코너 배지 2종(좌상단 「장착 N」 / 우상단 제약)은 **§4-B** |
| `UItemCardWidget`(+`UItemCardSlotWidget`) | `UIE_ItemCard`(+`_QtyBelow`/`_QtyInside`) | 아이템/보상 카드. ⚠⚠ **`SetIconSize` 는 카드 몸체를 안 줄인다** (2026-08-12 실측 — 몸체는 WBP 루트 `CardSizeBox` 144 고정, 아이콘 박스만 변경 → 축소 배치 시 7장=1078px 침범 사고). **카드째 줄이려면 `SetCardSize(카드, 아이콘)`** — 두 크기를 각각 명시(opt-in, 미호출 시 저작값 144 유지)하고 **저작 패딩(144 기준 10px)을 0 으로 리셋**해 아이콘이 카드에 꽉 차게 중앙 배치(축소 카드에서 패딩이 아이콘을 과소하게 만드는 함정 대응). ⚠⚠ **CommonButtonBase 자체 최소치(CDO MinWidth/MinHeight 144)가 제3의 크기 권위** (2026-08-12 실측) — 내부 SizeBox 만 줄이면 버튼 셀이 144로 남고, 루트 오버레이의 `CardSizeBox` 슬롯이 **좌상단 앵커 저작**이라 아이콘이 셀 왼쪽 위로 쏠린다. `SetCardSize` 가 `SetMinDimensions`+중앙 앵커 전환까지 묶어서 처리. 본체 카드는 `EWidgetType::ItemCard` 등록(2026-08-12). 라벨(`QuantityText`)은 **Bungee = Latin 전용, 한글 금지**. 클릭 툴팁 = 슬롯 변형의 `SetTooltipInfo`(opt-in) — 본체 카드엔 없음, 호출부가 `UIE_ItemTooltip` 직접 소유(PitchFeed 선례) |
| 직원 카드 (Employee/) | `UIE_Employee{Card,FullCard,ListCard,ManageCard,ManageCardInfo,ManageCard_UpgradeMap}`, `UIE_RecruitmentCard` | 직원/채용 |
| `UProductionOrderRowWidget` | `UIE_ProductionOrderRow` | **제작 시작 팝업 좌측 주문서 레일 행** (2026-07-29 신설, `UCommonButtonBase` + `CUI_Style2_Btn_CardClear`). 커버 · 이름 · 등급 배지 · `남은 N개 · 개당 T` + **제작 가능 힌트**(`재료로 N개까지 ― 상한 X` / `X 부족 ― 1개도 못 만듭니다`). 열어보기 전에 만들 수 있는 것이 골라지도록 상한을 행에서 미리 알린다. 등급색 SOT = `GradeColors` 노브 + `GetGradeColor()`(팝업 상세 배지도 이걸 경유 — 두 곳이 갈리지 않게). **폭을 주장하지 않고 `MinDesiredHeight=132`만** — 레일 폭과 충돌 방지 |
| `UReviewCardWidget` | `UIE_ReviewSnsCard` (Office/), `UIE_ReviewReactionToast` (Elements/Office/) | **출시 리뷰 카드 베이스** (2026-08-02 신설 / 2026-08-22 리뷰 페이지 폐기로 축소) — 흰 카드+잉크 7% 키라인, 라이트 플레이트 규칙(Style 슬롯 금지·Font 직접). SnsCard = 닉+코멘트(`SetSnsData` — 실패 페이지 테스터 의견에 재사용, @ 접두사는 호출자 소관). `SetCriticData`(이름+점수배지+한줄평, 배지색 8-10그린/5-7잉크/1-4레드 C++ 주입)는 이제 반응 토스트가 소비. 소비처: `UI_LaunchConfirmPanel` FailPage(2장) · 오피스 이벤트 레일 |
| `UReviewReactionToastWidget` | `UIE_ReviewReactionToast` (Elements/Office/) | **운영 중 반응 토스트** — `UReviewCardWidget` 변형 + 자동 만료(슬라이드 0.32/유지 7/페이드 0.3), 레일 `AddTransientCard` 전용. 폭은 레일 480 권위, 밴드색 3종 `Toast|Color` |
| `UEmployeeIdCardMiniWidget` | `UIE_EmployeeIdCardMini` | **가챠 멀티 리빌 사이드 사원증** (2026-08-12 신설). 센터 사원증(`UI_EmployeeGachaPresentation` IdCardBox)의 **560×773 미러** — 표시 축소는 호스트가 `SetRenderScale(0.58)`, 트리 치수는 손대지 말 것(사진 UV 창이 센터와 같은 비율 0.8 전제). 인쇄 완료 상태라 도장/텍스트 정적. API = `SetCardData(Result, IdOrdinal, Dept, Rank)` / `SetPhotoMaterial(MID)`. 등급 연출은 **자체 NativeTick**(레어+ `CardShine` 스윕 / 에픽+ `BackGlow` 브리딩) — 호스트는 배치·딜링만. 사진은 와이드 RT + `M_GachaRevealRT_Window`(UVWindow 크롭) MID. 트리 SOT = `WidgetTrees/UIE_EmployeeIdCardMini.tree.txt`. 라이트 플레이트 규칙 준수(스타일 슬롯 0, Font+잉크 직접) |
| `UEmployeeRosterCardWidget` | `UIE_EmployeeRosterCard` | **직원창 로스터 레일 가로형 카드** (UCommonButtonBase + IUserObjectListEntry). 얼굴 플레이트 + 이름 + Lv + ★(MakeStarString, 0성 숨김) + 등급 젬(SpawnRarity). 선택 = ListView 아이템 선택 단일 진실(`IsListItemSelected` 재질의로 재활용 대응). 공용 초상화 로더 `LoadPortraitTexture(ID, Size)` static 보유(기존 5곳 중복 루틴의 공유판). 사용처: `UI_EmployeeWindow` RosterListView |

## 6. 레거시 — 사용 금지 (교체 대상)

`UI_Element_Button1`/`Button1WithIcon`/`Button2WithIcon`/`Button_Pattern`, `UIE_UpgradeSlot_Legacy` → UIE_ 변형 사용. 신규 배선 금지.
⚠ 정정(2026-08-05): **`UI_Element_CountIconButton_Pattern` 은 레거시 아님** — 가챠 CTA(`PullButton`/`PullButton10`)의 라이브 표준이다(`GACHA_PANELS.md` §1). 아이콘+라벨 버튼이 필요하면 계속 쓸 것. 구 레거시 표기와 GACHA_PANELS 의 표준 지정이 충돌해 있던 것을 실사용 기준으로 정리했다.
(※ `UI_Element_IconWithButton_{Horizon,Vertical}`은 2026-06-10 레거시 해제 — Horizon은 가격/비용 CTA 표준으로 복권, §3 참조)
⚠ 정정(2026-06-05): **`UI_Element_Button`(베이스, 숫자 없는 것)은 레거시 아님** — UButtonWidget의 라이브 표준 텍스트 버튼 WBP (§3 표 참조).

---

## 7. 현재 건설 흐름 (B안 — 2026-06-05 전환 완료)

- [건설] → `BuildOpenWidget`이 **`EWidgetType::BuildPanel` 슬롯으로 PushPromptClass** → `UBuildModalWidget`(`UI_BuildModal`, 중앙 모달+자체 딤).
  - 구성: 산업 타일 6(`UCompanyTypeCardWidget`/`UIE_IndustryTile`) + 건물 카드 가로 ListView(`UIE_BuildEntityCard`) + 등급 탭(스크롤 점프) + [건설하기] CTA.
  - CTA = 산업+건물+자원+HQ상한 모두 통과 시 활성. 클릭 → `BeginBuild(BuildableInfo, CompanyType)` → 배치 → 확정 시 `FinalizeEntityRegistration` 직후 `SetCompanyType` 베이크 (PlacementHandler `PendingCompanyType`).
  - ⚠ ListView 엔트리 재활용 → 카드 델리게이트는 `OnEntryWidgetGenerated()` + `AddUniqueDynamic` (NativeConstruct 바인딩 금지).
  - ⚠ enum `Mythic` vs 탭 멤버명 `Unique` 불일치 잔존.
- 빌딩 데이터: `FBuildableCardTable`(Name/UIIcon/Rarity/ConstructionCosts — **CompanyType 없음**, 카드=산업 무관 cosmetic).
- 산업 데이터: `DT_CompanyInfo`(`FCompanyInfoTable`) — DisplayName/**GlyphIcon(타일 글리프)**/**AccentColor(시그니처색)**/Description 등. `GetCompanyInfo()` 경유. (구 `Icon` 일러스트 컬럼 + `Icon_*.jpeg` 6장은 사용자 결정으로 **폐기 예정** — Build 2b에서 컬럼 삭제 + CountrySellRow를 글리프+AccentColor 틴트로 교체 후 텍스처 삭제.)
- **삭제된 구 시스템** (2026-06-05): `UBuildPanelWidget`(바텀시트), `UIndustrySelectPanelWidget`(CompanyType==None 클릭 모달), CompanyNone 버블 분기, OfficeGameMode 업종선택 폴백. 잔여: WBP 2개/`EWidgetType::IndustrySelectPanel`/`EBubbleType::CompanyNone`은 에디터 정리 후 Build 2b에서 제거 예정.

## 8. 적용 사례 #1 — 산업 선택 빌드 모달 (레이아웃 ③ + 스타일 ②)

목업(`C:\tmp\ui_mockups\LAYOUT3_style2.png`)의 요소 → 기존 부품 매핑:

| 목업 요소 | 부품 (전부 기존) |
|---|---|
| 딤 배경 | ProductSellModal 패턴 (투명 BackgroundBtn) |
| 중앙 패널 | `UIE_PanelBorder` + `CUI_Style_Border_Panel_Float` |
| 타이틀 "회사 건설" + 확인/취소 | `UConfirmCancelWidget` (ContentSlot 주입) — 텍스트는 `CUI_Style_Text_Heading_L` 스케일 |
| 닫기 X | `UIE_CloseButton` |
| 산업 타일 6장 | `UCompanyTypeCardWidget` 개조 — 타일색=DT 시그니처색(Border_Round25 틴트), 글리프=화이트 텍스처+크림 틴트, 선택=`MI_UI_Glow_Outline`(이미 사용 중) |
| ~~선택 산업 프리뷰~~ | 폐기 — 모달에 프리뷰 없음. 일러스트 6장도 폐기 예정(위 §7 참조) |
| 비용 칩 | `UIE_Resource` (`SetCanAfford`) |
| 건설 CTA | `UButtonWidget` + `CUI_Style2_Btn_Green`(확정) / 취소 `CUI_Style2_Btn_Gray` |
| 스탯/특성 행 | `UIE_StatRow` + `UIE_Separator` |

**신규 에셋은 단 둘**: ① 산업 글리프 텍스처 6장(화이트/크림) ② DT 시그니처색 컬럼. 신규 C++ 거의 0.

---

## 9. 재화/수치 델타 표기 규약 (2026-07-22 추가 — 정직성 스윕)

1. **재화 델타(+N / -N)의 색은 `DT_Resource.UIColor` 가 SOT** — 호출부에서 hex/FLinearColor 리터럴을 직접 넣지 말 것. 자원별 색은 DT lookup(`UTableManagerSubsystem::GetResourceInfo`) 경유로만 얻는다. (지출 표기의 레드는 "지출"이라는 의미색이라 예외 — `UInGameLayerWidget::SpawnAmountPopup` 이 유일 정의처.)
2. **델타가 음수여도 숨기지 않는다.** 손해·감소·차감도 획득과 같은 비중으로 노출한다. **스탯 비교는 화살표 아이콘 없이 전후를 병기** — 표기 형식 `124 → 138 (+14)` (`→` = U+2192, NEXON 글리프 있음). 값 하나만 보여주고 변화량을 감추는 표기 금지.
3. **하락·감소에 액센트 블루(`#3D9BE0`)를 절대 쓰지 않는다.** 블루는 기능색 전용(탭 선택/포커스/선택 상태). 감소는 레드 계열, 증가는 그린 계열(`CUI_Text_Gain #5A8000`).

### 자금(`Money`) 표시 계약 `[확정 2026-08-10 → 개정 2026-08-11]`

| 문맥 | 표기 | 예시 |
|---|---|---|
| 재화 이름 | `자금` | `자금` |
| HUD 보유액 | 자금 아이콘 + 한국식 축약 수치 | `[아이콘] 1.24억` |
| 독립 수익 델타(직원 머리 위) | 자금 아이콘 + 부호 + 한국식 축약 수치 | `[아이콘] +2.5만` |
| HUD 자금 아이콘 옆 증감 | 부호 + 한국식 축약 수치 | `+2.5만` |
| 수익 수령 토스트 | 자금 아이콘 + 부호 + 한국식 축약 수치 | `[아이콘] +12.4만` |
| 일반 텍스트 안내 | 표시명 `자금` + 문맥에 맞는 원본/축약 수치 | `자금이 2.5만 부족합니다` |
| 상세 툴팁 | 이름 + 쉼표 원본 수치 | `자금 124,300,000` |

- `$`, `₩`, `원`은 사용하지 않는다. 내부 enum 및 저장 키 `Money`는 유지한다.
- 수익 수령은 전용 `UIE_FundsToast`를 사용한다. 일반 `UIE_Notification`, `UIE_OfficeStatusToast`, `UI_RewardToast` 에셋은 수정하지 않는다.
- **직원 머리 위 독립 수익 델타:** `UCoinFlyoutContainerWidget`이 WBP 없이 만드는 일시적 게임 피드백 행이다. Money 아이콘 48px, 오른쪽 간격 8px, NEXON Bold 36을 사용한다. 금액은 `FormatFundsAmount(..., true)`, 색은 `DT_Resource[Money].UIColor`를 따르며 기존 1.1초 상승·페이드와 코인 비행을 유지한다. 아이콘 로드 실패 시 숫자 행은 유지한다.
- **수익 수령 토스트:** `UIE_FundsToast`는 기존 상단 알림 스택/슬라이드를 재사용하는 78px 밴드다. Money 아이콘 48px, 오른쪽 간격 8px, 금액 Pretendard SemiBold 34(`Number_L`), 기본 2초, 무음이다. 금액은 `FormatFundsAmount(..., true)`, 아이콘·색은 `DT_Resource[Money]`가 SOT이며 아이콘 로드 실패 시 숫자는 유지한다.
- 이 계약은 실제 재화 `Money` 전용이다. 아래 `ScoreOrb` 예외의 서양식 `K/M` 표기는 변경하지 않는다.

**의도된 예외 — 스코어 오브(`UScoreOrbContainerWidget`)의 점수 표기**: 이 숫자는 **통화가 아니라 추상 포인트**라 Latin 전용 폰트 + 서구식 축약(K/M)을 **의도적으로** 유지한다. 재화 표기 규약(`AbbreviateNumber` 만/억/조)과 "통일"한다는 명목으로 되돌리지 말 것. 재화 단위계 통일 논의의 대상은 코인 플라이아웃 등 **실제 재화** 표기에 한정된다.

---

## 10. 알림(토스트) 위생 규약 (2026-07-22 추가 — 알림 위생 스윕)

1. **상단 HUD 나 미션 트래커가 상시 보여주는 정보는 알림으로 발행하지 않는다.** 상시 노출 중인 값(보유 재화, 진행률, 미션 문구)을 토스트로 중복 발행하면 알림 줄만 소모하고 새 정보가 없다. 토스트는 "화면 어디에도 안 보이는 사건"에만 쓴다.
2. **타입은 생략하지 않는다.** `ShowNotification` 의 `ENotificationType` 기본값은 `Normal`(흰색)이라, 실패를 타입 없이 발행하면 흰색으로 뜬다. 판정 기준: `~부족합니다` / `~할 수 없습니다` / `잠금 해제되지 않았습니다` = `Failed`, `처리 중입니다` / `가득 찼습니다` / 미구현 안내 = `Warning`, 완료·획득 = `Success`.
3. **같은 문구 연속 발행은 컨테이너가 흡수한다.** `UNotificationContainerWidget::AddNotification` 이 1.5초 내 동일 문구를 새 엘리먼트 대신 표시 시간 리셋으로 처리한다. 호출부에서 중복 억제 인자를 넘기지 말 것.
4. **알림음은 타입에서 자동 결정된다** (`UUIManagerSubsystem::GetNotificationSoundTag`). `Normal` 은 볼륨 0.35 의 낮은 틱, 나머지는 정상 볼륨, 타입별 1.5초 쿨다운. 색만 넘기는 `ShowColoredNotification` 은 타입 근거가 없어 **의도적으로 무음** — 소리가 필요하면 `ShowNotification(타입)` 으로 옮긴다.
5. **영구 정원 증가 채용권은 기존 `UI_RewardToast`를 재사용한다.** 첫 건설·빌드업·이후 건설 모두 `일반 채용권 획득` 제목과 일반 채용권 아이콘+`xN`을 함께 보인다. 수령 버튼·모달·자동수령 타이머는 추가하지 않는다.
