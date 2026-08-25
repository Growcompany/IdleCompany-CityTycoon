# 모바일 렌더링 세팅 SOT — AA·노출·해상도·발광체 (2026-07-02 확정)

> **이 문서가 단일 진실 소스.** 야간 "가로등 shimmer" 사건(2026-07-01~02)의 전면 조사로 확정된 모바일 렌더 파이프라인 설정과 그 이유. 관련 시스템 상세: `STREET_LAMPS.md` / `AMBIENT_TRAFFIC.md` / `AVIATION_BEACONS.md` / `DAY_NIGHT_CYCLE.md`.

## 한눈 요약

| 항목 | 확정값 | 위치 |
|---|---|---|
| AA | **MSAA 4x** (`r.Mobile.AntiAliasing=3` + `r.MSAACount=4`) | `DefaultEngine.ini` |
| 노출 | **Manual 강제** + **시간대별 bias** (낮 -0.15 / 밤 +0.50, 여명·황혼 자동 보간) | 디바이스프로파일 + `BP_TimeCycleSky` |
| 렌더 해상도 | 고사양(Android_High) **100% + 동적해상도 OFF** / 저사양 83.33% + dynres | `DefaultDeviceProfiles.ini` |
| 발광체 문법 | **baked-halo 스프라이트** (큰 소프트 밉 텍스처 + 낮은 HDR) | 각 매니저 + MI |

---

## 1. AA = MSAA 4x (모바일 정석)

**실기기(S23) 3종 A/B 판정 (2026-07-02):**
- **TAA**(=2): 기하는 부드럽지만 **밝은 점 파이어플라이**(velocity 없는 발광 카드에 히스토리 박스클램프가 단일프레임 백색 팝) + velocity 패스 드로우콜 추가. 소프트 카드화 이후에도 잔존. 모바일은 Responsive AA 컴파일아웃이라 탈출구 없음(구조적 탈락).
- **FXAA**(=1): 라이트는 안정이나 **이동 시 기하 경계 크롤링**("지글지글") — 시간 축적이 없어 구조적.
- **MSAA 4x**(=3): 기하 경계 하드웨어 AA + 시간 아티팩트 0. **타일 GPU(Adreno/Mali)는 온-타일 resolve라 거의 무비용** — 데스크톱과 달리 모바일에서 MSAA가 정석인 이유. 이 게임 아트(깔끔한 직선 건물+발광 점)에 정확히 부합.

**판정 이력 정정 (2026-07-03):** 07-02 밤 6종 비교에서 "MSAA 지글" 판정으로 FXAA로 잠시 전환했으나, 그 MSAA 빌드는 **PPR 게이트에 걸려 조용히 무AA로 폴백된 가짜**였고(확대판독 블렌딩픽셀 0), 당시 라이트 깜빡임도 AA가 아닌 블룸 플러터(HDR20 미저장 사고)가 원인. 게이트 픽스 + 블룸 절연(§4) + 차량 매프레임 재조준 후 **진짜 MSAA 4x 첫 시험 → 실기기 판정 OK, 최종 확정**.

**함정:**
- `r.MobileMSAA`는 **UE5.4에서 삭제된 죽은 키** (엔진 전체 소비자 0). 진짜 노브 = `r.MSAACount`.
- **PPR 게이트**: `r.Mobile.PixelProjectedReflectionQuality=1`이면 모바일 MSAA가 **조용히 무AA로 폴백**. AndroidScalability가 =1을 깔고, 엔진 기본 iPhone8 프로필(iPhoneX+가 상속)도 =1 — 프로젝트 전 프로필 =0으로 소탕 완료(2026-07-03). MSAA는 부팅 시에만 초기화라 라이브 토글 불가, 판정은 쿠킹 필수.
- `r.EyeAdaptation.MethodOverride` 값 = EAutoExposureMethod enum 순서: **0=Histogram, 1=Basic, 2=Manual** (1을 히스토그램으로 오독 주의).
- `r.AntiAliasingMethod=4`(TSR)는 데스크톱 전용 — 모바일은 `r.Mobile.AntiAliasing`만 본다.

## 2. 노출 = Manual 강제 + 시간대별 bias

**왜 (측정으로 확정):**
- 모바일 오토 노출(Basic/Histogram 모두)은 이 밤 씬에서 **자기지속 이완 진동** — 화면 평균 42↔126 (2.97배), 주기 ~8-10초. 이것이 "가로등 halo가 세졌다 약해졌다"의 진범 (밤 씬에서 중간톤=halo뿐이라 노출 스윙이 거기서만 보임).
- **모바일 렌더패스는 PPV의 메터링 모드(AEM_Manual)를 무시**하고 오토로 폴백 — PPV의 다른 값(블룸 등)은 정상 적용됨. PIE(데스크톱)는 PPV Manual을 존중 → "PIE는 멀쩡한데 폰만 떨리는" 이유.

**아키텍처:**
1. `DefaultDeviceProfiles.ini [Android] +CVars=r.EyeAdaptation.MethodOverride=2` — 모바일 노출을 Manual로 강제 (전 안드로이드).
2. `ATimeCycleSky`: `DayExposureBias` / `NightExposureBias` (EditAnywhere) — SkyLight와 같은 태양 알파로 lerp → 전역 PPV `Settings.AutoExposureBias`에 런타임 주입 (게임월드 전용, PPV 저장값 불변). 여명/황혼 2h 블렌드 자동.
3. `EditorPreviewPostProcess`: 정확히 MainMap editor world + ES3.1에서만 editor-only transient Unbound/Manual PP를 켜고 런타임과 같은 bias를 사용한다. 저장 PPV/맵/Blueprint는 수정하지 않는다.
4. **철학: "밝기는 조명이 책임"** — 별도 Moon 라이트 없이 단일 DirectionalLight의 태양광+그림자 없는 야간 fill과 SkyLight가 낮밤 룩을 만든다. 일반 게임 기본은 `SkyNightIntensity=0.45`, `NightDirectionalIntensity=0.0`이며, N1/N2의 더 밝은 야간값은 저장하지 않는 Vulkan A/B 후보다. MainMap에 수용된 낮 배치값은 `SunMaxIntensity=0.8`, `SkyDayIntensity=0.5`다.

**MainMap 확정값 (2026-08-20 B2):** `DayExposureBias=-0.15`, `NightExposureBias=+0.50`. `BP_TimeCycleSky` CDO와 MainMap 배치 액터를 같은 값으로 유지한다. 낮 건물 판정은 12:00 고정의 Android Vulkan Mobile Preview + 실제 game world Vulkan ES3.1로 한다. 일반 SM6 Lit/Unlit는 노출 경로가 달라 기준으로 쓰지 않는다.

에디터 facade 프리뷰도 같은 조건에서 `DT_CityCompany.SkinID → DT_BuildingSkin`을 transient MID로 적용한다. 저장/PIE 경계에서 정확히 복원하므로 MainMap에 프리뷰 MID를 bake하지 않는다. 프리뷰는 낮 12:00 외벽·스킨 색감 판정용이며, `LightID`/창문 발광이 필요한 야경은 실제 Vulkan game world가 기준이다. Python 콘솔 수동 제어는 `import mp; mp.on()` / `mp.refresh()` / `mp.off()`.

**튜닝법:** PIE 실행 → 아웃라이너 `BP_TimeCycleSky` → Details `Sky|Exposure` + `Sky|Lighting` 슬라이더 (라이브 반영). 콘솔 `Day`/`Night` 치트로 전환하며 튜닝 → 값 저장 → 쿠킹.

## 3. 렌더 해상도 — 고사양 100%

- 베이스 [Android]: `SecondaryScreenPercentage=83.33` + `DynamicRes mode 2` (저사양 유지).
- **[Android_High]: 100% + dynres OFF** — MSAA/FXAA(비시간적 AA)에선 83.33% 비정수 업스케일 리샘플링이 이동 시 광원 플러터의 **절반**(실측: 프레임간 변동 2.9%→1.4%). 이 게임은 CPU(RHI) 바운드라 GPU fill 여유 있음 (A9 프로파일).

## 4. 발광체 = baked-halo 문법 (모바일 문법)

**원칙:** "작고 강한 HDR 점 + 런타임 블룸 번짐"(데스크톱 문법)은 모바일에서 구조적으로 떨림 — 모바일 블룸은 **TAA 이전 원시 씬에서 매 프레임 재계산**(PostProcessing.cpp:2361 vs TAA :2626, 데스크톱과 순서 반대)이라 어떤 AA로도 번짐 펄스를 못 잡는다. 정석 = **번짐을 텍스처에 굽기**: 큰 소프트 방사 텍스처(밉맵) 카드 + 낮은 HDR → 밉 필터링이 이동·축소를 사전 안정화.

**블룸 절연 (2026-07-03 확정):** 모바일 블룸 임계는 소프트니 `saturate((luma−T)×0.5)` = T~T+2 램프(PostProcessMobile.usf:358), 입력은 1/4해상도 4×4 무겹침 박스 — 몇 픽셀짜리 이동 HDR 광원은 구조적으로 에너지가 진동한다(×BloomIntensity 증폭). 처방 = **라이트 카드 luma를 임계(3) 밑으로** → 블룸 기여 정확히 0, 번짐은 텍스처가 전담. 밝기 조정 시 **luma(0.3R+0.59G+0.11B) < 3 유지**가 철칙. 참고: `bloom_size_scale`은 **모바일 미사용**(데스크톱 전용 — 커널 하드코딩).

| 시스템 | 카드/스케일 | HDR (LightColor) | 비고 |
|---|---|---|---|
| 차량 (`TrafficManager`) | CardScale 0.55, MinFactor 1.5~MaxGrow 3, **매프레임 재조준**(카메라 1cm 이동시, `RefreshLightBillboards`) | Head (2.76,2.48,1.84)·Tail (5,.17,.08) — **둘 다 luma≤2.5 블룸절연** | `LightCardCamPush=25`(차체 파묻힘 z잘림 방지)·빌딩게이트 1.1×R 히스테리시스 |
| 가로등 (`StreetLampManager`) | Glow 0.72/LOD 1.2, MinFactor 1.8, **매프레임**(interval 0/threshold 1cm), LOD 전환 20m **±10% 히스테리시스** | (3.27,2.35,1.18) — luma 2.5 블룸절연 | 근거리 5알+원거리 LOD 1장, `T_SoftGlowRadial` 공유 |
| 비콘 (`AviationBeaconManager`) | Large 2.25/Normal 1.125 | Brightness 15 (블룸 유지 — 점멸 연출용) | **대형 1.6s/소형 2.8s 이원 점멸** = PerInstanceCustomData0 |

공유 텍스처: `T_SoftGlowRadial` (256² 가우시안, 밉, sRGB off, Clamp) — `M_CarLight` 마스터가 사용, 차/가로등 MI 상속. **MI 값 변경은 반드시 `save_asset`+디스크 타임스탬프 검증** — 에디터 메모리에만 남으면 쿠킹에 반영 안 됨(07-02 사고: HDR 다이어트가 미저장으로 폰에 HDR20 그대로 나감).

## 5. 함정 모음 (재발 방지)

1. **PPV override 체크박스**: 값만 넣고 override 안 켜면 영원히 비활성 (bias=10 잔재 사건 — 활성인 적 없는 값이 문서에 "적용 중"으로 기록돼 있었음).
2. **SkyLight 정적 큐브맵은 렌더러 재생성 시 유실** — AA 방식 등 render cvar 토글 → 외벽 검은 실루엣 + 노출 폭주로 지면 백색. **앱 재시작으로 복구.** 인게임 그래픽 설정 메뉴 추가 시 RecaptureSky 재호출 필요.
3. **cvar 임시 주입은 휘발**: 앱 재시작·AA 토글(scalability 재적용)에 날아감. 영구화는 반드시 ini/디바이스프로파일/에셋으로.
4. **Live Coding `.voltbl` 실패 이력** — cpp 변경이 에디터에 안 실린 채 PIE 보면 오판 (비콘 "안 깜빡" 사건). 쿠킹은 소스에서 새로 컴파일하므로 APK는 무관.
5. 밤 씬 밝기 인상은 **노출×조명×광원HDR 3축의 곱** — "어둡다"면 어느 축인지 분리 진단: 화면 전체=노출, 건물만=조명(SkyNight), 광원만=MI HDR.

## 6. 측정 도구 (재사용 가치 높음)

시각 아티팩트는 눈 판정이 3연속 오진났던 사건 — **측정 먼저**:
- **노출 진동 검출**: `adb shell screencap` 버스트(6~16장) → 그레이스케일 프레임평균 시계열. 진동=평균이 수십% 스윙 / 정상=평탄. (오늘 42↔126 검출)
- **이동 플러터 정량화**: `adb shell screenrecord` + `input swipe`(원격 카메라 팬) → ffmpeg 프레임 추출 → 밝은픽셀(>200) 총량의 프레임간 diff 부호반전 카운트. 반전율 >50%=진동, 실측: 83.33% 렌더 25/38 → 100% 16/41.
- cvar 적용 확인: `am broadcast -a android.intent.action.RUN -p com.funquer.companygrowth -e cmd 'r.XXX'` → `adb logcat -d -s UE`에서 `LastSetBy` 확인 (Console/DeviceProfile/ProjectSetting 출처까지).
