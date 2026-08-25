// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h" // 전역함수를 언리얼에서 관리하게 해줌
#include "Math/RandomStream.h"
#include "GlobalUtilFunctions.generated.h"

class UProgressBar;
class UImage;

// 큰 수 축약 표기 스타일. Auto는 현재 컬처로 한국식(만/억/조)/서양식(K/M/B) 자동 선택.
UENUM(BlueprintType)
enum class ENumberAbbrevStyle : uint8
{
	Auto          UMETA(DisplayName = "Auto (by culture)"),
	KoreanUnit    UMETA(DisplayName = "Korean (man/eok/jo)"),
	WesternSuffix UMETA(DisplayName = "Western (K/M/B)")
};

// 축약 시 끝자리 처리. 기본 Floor(보유/수치, 방치형 관행). 비용 표기는 Ceil 권장.
UENUM(BlueprintType)
enum class ENumberRoundMode : uint8
{
	Floor UMETA(DisplayName = "Floor (truncate)"),
	Round UMETA(DisplayName = "Round"),
	Ceil  UMETA(DisplayName = "Ceil (round up)")
};

UCLASS()
class COMPANYGROWTHRENEWAL_API UGlobalUtilFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 맵에서 x,y 벡터를 200 단위로 끊어서 리턴
	UFUNCTION()
	static FVector SteppedPosition(const FVector& position);

	// Random으로 String을 생성해주는 함수
	UFUNCTION()
	static FString GenerateRandomStringWithSeed(int32 Length);

	// 큰 정수를 UI 표시용으로 축약. 1만(서양식 1K) 미만은 콤마 원본, 이상은 단위+유효숫자 3자리.
	// 통화/단위 접미사(원, /s 등)는 호출부에서 붙일 것.
	UFUNCTION(BlueprintPure, Category = "Util|Number")
	static FText AbbreviateNumber(int64 Value, ENumberAbbrevStyle Style = ENumberAbbrevStyle::Auto, ENumberRoundMode RoundMode = ENumberRoundMode::Floor);

	UFUNCTION(BlueprintPure, Category = "Util|Number")
	static FText FormatFundsAmount(int64 Value, bool bShowPositiveSign = false, ENumberRoundMode RoundMode = ENumberRoundMode::Floor);

	// 소수 값(수익/초 등) 오버로드. 큰 값은 정수 경로로 위임.
	UFUNCTION(BlueprintPure, Category = "Util|Number", meta = (DisplayName = "Abbreviate Number (Float)"))
	static FText AbbreviateNumberFloat(double Value, ENumberAbbrevStyle Style = ENumberAbbrevStyle::Auto, ENumberRoundMode RoundMode = ENumberRoundMode::Floor);

	// AbbreviateNumber 결과를 숫자부와 한글 단위(만/억/조/경)로 쪼갠다 — 단위를 숫자보다 작게 조판하는
	// 금액 레이아웃용(같은 자리에서 금액이 커 보인다). 단위가 없으면 OutUnit 은 빈 문자열.
	UFUNCTION(BlueprintPure, Category = "Util|Number")
	static void SplitAbbreviatedNumber(int64 Value, FString& OutNumber, FString& OutUnit,
		ENumberAbbrevStyle Style = ENumberAbbrevStyle::Auto, ENumberRoundMode RoundMode = ENumberRoundMode::Floor);

	// 정확한 전체 수치(콤마 그룹핑). 축약 표기 클릭 시 "자세히" 표시용.
	UFUNCTION(BlueprintPure, Category = "Util|Number")
	static FText FormatExactNumber(int64 Value);

	// 소요/남은 시간 한글 표기 ("52초" / "11분 40초" / "2시간 5분"). 가장 큰 두 단위까지만.
	// 위젯마다 흩어진 FormatDuration 사본들의 신규 표준 — 새 시간 표기는 이걸 쓸 것.
	UFUNCTION(BlueprintPure, Category = "Util|Number")
	static FText FormatDurationKorean(double Seconds);

	// 견적/예상 소요 표기 — 자릿수를 뭉갠 뒤 FormatDurationKorean (10분 미만 10초 / 1시간 미만 1분 / 그 이상 10분 단위).
	// 예상치를 초 단위까지 말하면 실제와 어긋났을 때 그 정밀도가 거짓말이 된다.
	UFUNCTION(BlueprintPure, Category = "Util|Number")
	static FText FormatEstimateDurationKorean(double Seconds);

	// 적립 속도 한글 표기 ("분당 +1.1억" / "시간당 +2.4억"). 단위는 SecondsPerEvent 로 고른다 —
	// 말한 기간 안에 적립이 최소 한 번은 들어오는 단위라야 표기가 거짓말을 하지 않는다.
	// (6분에 한 번 떨어지는 회사에 "초당 +6만"을 붙이면 화면은 0원인 채로 멈춰 있다)
	UFUNCTION(BlueprintPure, Category = "Util|Number")
	static FString FormatRatePerBestUnit(int64 PerSecond, float SecondsPerEvent = 1.f);

	// Step(1 기획/2 개발/3 QA) 대표색 — orb·플로팅 텍스트 단일 출처
	UFUNCTION(BlueprintPure, Category = "Util|Color")
	static FLinearColor GetStepColor(int32 Step);

	// 진행바 채움 끝에 글로우 헤드를 붙인다 (T_UI_ProgressHead_* — DOBO_ASSET_INTEGRATION §5).
	// Head 는 Bar 를 덮는 CanvasPanel 의 자식이어야 한다(슬롯 alignment 0.5,0.5).
	// 위치는 앵커(정규화)로 잡아 바 픽셀 폭에 의존하지 않는다 — 레이아웃 전 첫 호출에도 정확.
	// HeadSpill = 헤드가 바 위아래로 번지는 양(한쪽, px). 높이 = 바두께 + 2*Spill 로 자동 산출되고
	// 가로는 저작 브러시의 가로세로비를 유지한다 — 모양(원형 Comet / 세로형 Streak)은 WBP 가, 크기는 코드가 소유.
	UFUNCTION(BlueprintCallable, Category = "Util|UI")
	static void UpdateProgressHead(UProgressBar* Bar, UImage* Head, float Percent01,
		bool bSyncTint = true, float HeadSpill = 15.0f);

	// 런타임 진입 시 헤드를 숨긴다 — WBP 저작값은 디자이너 확인용 고정 프리뷰(35%)라
	// 첫 UpdateProgressHead 전에 그대로 보이면 거짓 데이터가 된다. 패널 초기화에서 1회 호출.
	UFUNCTION(BlueprintCallable, Category = "Util|UI")
	static void InitProgressHead(UImage* Head);

	// 유저가 누른 지점(Slate absolute 좌표 — Geometry::AbsoluteToLocal 과 짝). 팝오버/이펙트 앵커의 단일 출처.
	// ⚠ FSlateApplication::GetCursorPos() 직접 호출 금지: 모바일 터치는 OS 커서를 안 움직여(FAndroidCursor 는
	// 하드웨어 마우스 전용) 항상 (0,0) 이 나온다. 터치 좌표는 release 라우팅(OnClicked) 중까지 유효.
	UFUNCTION(BlueprintPure, Category = "Util|UI")
	static FVector2D GetPointerAbsolutePosition();
};
