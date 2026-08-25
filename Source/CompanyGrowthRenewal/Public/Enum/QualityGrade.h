#pragma once

#include "CoreMinimal.h"
#include "Enum/CompanyType.h"
#include "QualityGrade.generated.h"

/**
 * 프로젝트 품질 등급
 * 1~3단계의 달성률 가중 평균으로 결정됨
 * 등급별로 수익 배율과 기본 수명이 다름
 */
UENUM(BlueprintType)
enum class EQualityGrade : uint8
{
	F UMETA(DisplayName = "F"),    // 0.5 ~ 0.79 | x0.5 | 0.5분/프로젝트
	D UMETA(DisplayName = "D"),    // 0.8 ~ 0.99 | x0.8 | 0.75분/프로젝트
	C UMETA(DisplayName = "C"),    // 1.0 ~ 1.19 | x1.0 | 1분/프로젝트
	B UMETA(DisplayName = "B"),    // 1.2 ~ 1.49 | x1.2 | 1.5분/프로젝트
	A UMETA(DisplayName = "A"),    // 1.5 ~ 1.79 | x1.5 | 2분/프로젝트
	S UMETA(DisplayName = "S"),    // 1.8 ~ 2.0  | x2.0 | 3분/프로젝트

	Max UMETA(Hidden)
};

/**
 * 프로젝트 품질 등급 경계 — [0.5, 2.0] 4등분. 등급(QualityScoreToGrade)과 기획 보드의
 * 예상 등급 표시가 **이 상수만** 읽는다. 리튠은 여기 한 곳에서만 — 값이 두 벌로 갈리면
 * 카드 판정이 실제 결과와 다른 말을 하게 된다.
 */
constexpr float QualityThreshold_S = 1.625f;
constexpr float QualityThreshold_A = 1.25f;
constexpr float QualityThreshold_B = 0.875f;

/**
 * 품질 점수를 등급으로 변환
 * @param QualityScore 0.5 ~ 2.0 범위의 품질 점수
 * @return 해당하는 품질 등급
 */
inline EQualityGrade QualityScoreToGrade(float QualityScore)
{
	// 2026-07-30: 프로젝트 품질은 S/A/B/C 4등급만. [0.5,2.0] 을 정확히 4등분.
	// 출시 게이트(MeetsMinimumClearScore = 전 활성 스텝이 각자 목표의 50%) 통과 하한이 곧 Q 0.5 라
	// "게이트를 넘었으면 최소 C" 가 자동으로 성립한다.
	// (공동 근거였던 "개발비 선불이라 손해 출시는 불가"는 개발비 폐지 2026-08-15 로 무효 — 게이트 논거만 남는다)
	// D/F 는 enum 에 남는다 — WorldMapManager 가 "미발견" 센티넬로, TradePort 가 배율 분기로 쓴다.
	// 제조 양산은 이 함수를 쓰지 않는다 → ProductionScoreToGrade (아래) 참조.
	if (QualityScore >= QualityThreshold_S) return EQualityGrade::S;
	if (QualityScore >= QualityThreshold_A) return EQualityGrade::A;
	if (QualityScore >= QualityThreshold_B) return EQualityGrade::B;
	return EQualityGrade::C;
}

/**
 * 제조 양산 전용 등급 변환 (6등급 유지)
 *
 * 프로젝트 개발의 4등급화(2026-07-30)는 "출시 게이트 하한 Q=0.5 가 곧 C" 라는 프로젝트 고유 논리에서
 * 나왔다(공동 근거였던 개발비 선불 논거는 2026-08-15 개발비 폐지로 무효). 제조 양산에는 게이트가
 * 없어 그 논리가 적용되지 않으므로 구 6등급 경계를 그대로 쓴다.
 * — 공유했다면 GetBaseProductionQuantity 의 D=3/F=1 이 사문화되고 Q 0.5~0.87 구간 수량이 1~3 → 8 로
 *   최대 8배 부풀었다(2026-07-31 회귀 수정).
 */
inline EQualityGrade ProductionScoreToGrade(float QualityScore)
{
	if (QualityScore >= 1.8f) return EQualityGrade::S;
	if (QualityScore >= 1.5f) return EQualityGrade::A;
	if (QualityScore >= 1.2f) return EQualityGrade::B;
	if (QualityScore >= 1.0f) return EQualityGrade::C;
	if (QualityScore >= 0.8f) return EQualityGrade::D;
	return EQualityGrade::F;
}

/**
 * 품질 등급을 알파벳 문자열로 변환
 */
inline FString QualityGradeToAlphabetString(EQualityGrade Grade)
{
	switch (Grade)
	{
	case EQualityGrade::S: return TEXT("S");
	case EQualityGrade::A: return TEXT("A");
	case EQualityGrade::B: return TEXT("B");
	case EQualityGrade::C: return TEXT("C");
	case EQualityGrade::D: return TEXT("D");
	case EQualityGrade::F: return TEXT("F");
	default: return TEXT("?");
	}
}

/**
 * 등급 문자 → 표시색 (S 골드 / A 그린 / B 블루 / C 회색 / 그 외 레드).
 * 품질 등급(S~F)과 리뷰 궁합 등급(S~D)이 같은 알파벳 스케일이라 램프를 공유한다.
 * 문자열을 받는 이유 = 궁합 등급이 FName 으로 오기 때문. 품질 등급은 QualityGradeToAlphabetString 을 거쳐 넣는다.
 */
inline FLinearColor GradeLetterToColor(const FString& GradeLetter)
{
	if (GradeLetter == TEXT("S")) { return FLinearColor::FromSRGBColor(FColor(0xD9, 0x9E, 0x0B)); }
	if (GradeLetter == TEXT("A")) { return FLinearColor::FromSRGBColor(FColor(0x21, 0x9E, 0x52)); }
	if (GradeLetter == TEXT("B")) { return FLinearColor::FromSRGBColor(FColor(0x17, 0x6B, 0xB8)); }
	if (GradeLetter == TEXT("C")) { return FLinearColor::FromSRGBColor(FColor(0x6B, 0x78, 0x8C)); }
	return FLinearColor::FromSRGBColor(FColor(0xB3, 0x59, 0x59));
}

/**
 * 품질 등급별 수익 배율 반환
 */
inline float GetQualityGradeRevenueMultiplier(EQualityGrade Grade)
{
	switch (Grade)
	{
	case EQualityGrade::S: return 2.0f;
	case EQualityGrade::A: return 1.5f;
	case EQualityGrade::B: return 1.2f;
	case EQualityGrade::C: return 1.0f;
	case EQualityGrade::D: return 0.8f;
	case EQualityGrade::F: return 0.5f;
	default: return 1.0f;
	}
}

/**
 * 품질 등급별 프로젝트당 기본 수명 반환 (분 단위)
 * 실제 운영시간 = 이 값 × ProjectNumber, 하한 3분 (ComputeEffectiveOperationTime 이 단일 소유)
 */
inline float GetQualityGradeBaseLifespanMinutes(EQualityGrade Grade)
{
	switch (Grade)
	{
	// 2026-07-30: 4등급 체계로 압축. 구 S 3.0 은 품질이 수익에 3중으로 곱해지는 구조(ScoreMult ×
	// 궁합SpreadMult × 수명)에서 S/C 회수율 격차를 24배까지 벌렸다. 1.80 이면 6.86배.
	// 수명 자체는 방치 체감이라 과도 압축(1.3)하지 않는다 — T10 S 운영 175분 = 세션 간격의 42%.
	case EQualityGrade::S: return 1.80f;
	case EQualityGrade::A: return 1.50f;
	case EQualityGrade::B: return 1.25f;
	case EQualityGrade::C: return 1.0f;
	// D/F 는 프로젝트 품질로는 도달 불가(QualityScoreToGrade 참조). 제조/무역 경로 호환용.
	case EQualityGrade::D: return 0.75f;
	case EQualityGrade::F: return 0.5f;
	default: return 1.0f;
	}
}
