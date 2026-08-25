// Fill out your copyright notice in the Description page of Project Settings.

#include "Data/EmployeeStatsData.h"
#include "Data/EmployeeTypes.h"
#include "Data/FatigueConfig.h"
#include "Entity/Officeworker/EmployeeBehaviorComponent.h"

float UEmployeeStatsHelper::CalculateBaseEfficiencyForLevel(int32 Level)
{
	// 레벨 1 = 1.0배, 레벨 2 = 1.02배, 레벨 3 = 1.04배...
	// 공식: 1.0 + (Level - 1) * 0.02
	return 1.0f + (Level - 1) * 0.02f;
}

int32 UEmployeeStatsHelper::CalculateOverall(const FEmployeeStats& Stats)
{
	return Stats.WorkSpeed +
	       Stats.CritChance +
	       Stats.Composure +
	       Stats.ExpGain +
	       Stats.Stamina +
	       Stats.Focus;
}

int32 UEmployeeStatsHelper::CalculateEffectiveOverall(const FEmployeeStats& Stats, int32 EnhancementLevel)
{
	// ★는 스탯 성장의 유일 경로라, 저장값 합만 보여주면 ★15 직원도 스폰값 그대로 표시된다
	return CalculateOverall(Stats)
		+ static_cast<int32>(EEmployeeStatIndex::Count) * UEmployeeTypeHelper::GetEnhanceStatBonus(EnhancementLevel);
}

int32 UEmployeeStatsHelper::GetStatValueByIndex(const FEmployeeStats& Stats, uint8 StatIndex)
{
	switch (StatIndex)
	{
	case 0: return Stats.WorkSpeed;
	case 1: return Stats.CritChance;
	case 2: return Stats.Composure;
	case 3: return Stats.ExpGain;
	case 4: return Stats.Stamina;
	case 5: return Stats.Focus;
	default: return 0;
	}
}

FText UEmployeeStatsHelper::GetStatDisplayName(uint8 StatIndex)
{
	// ⚠ UMETA(DisplayName) 을 런타임에 읽지 말 것 — 조회가 WITH_EDITOR 안에만 있어(Enum.cpp) 패키징 빌드에서는
	//   식별자("WorkSpeed"/"CritChance")로 떨어진다. PIE 는 멀쩡해 보여 안 걸린다. 표시명 SOT 는 이 함수.
	//   EEmployeeStatIndex 의 UMETA 와 아래 문자열은 반드시 같이 고칠 것.
	// 침착성/체력의 괄호는 값이 셋 다 "초" 단위라 라벨만으로 구분이 안 되는 걸 푸는 장치다.
	switch (static_cast<EEmployeeStatIndex>(StatIndex))
	{
	case EEmployeeStatIndex::WorkSpeed:  return FText::FromString(TEXT("업무속도"));
	case EEmployeeStatIndex::CritChance: return FText::FromString(TEXT("크리티컬 확률"));
	case EEmployeeStatIndex::Composure:  return FText::FromString(TEXT("침착성(이탈)"));
	case EEmployeeStatIndex::ExpGain:    return FText::FromString(TEXT("경험치획득"));
	case EEmployeeStatIndex::Stamina:    return FText::FromString(TEXT("체력(졸음)"));
	case EEmployeeStatIndex::Focus:      return FText::FromString(TEXT("업무집중도"));
	default:                             return FText::GetEmpty();
	}
}

namespace
{
	// 침착성/체력은 기획자 튜닝 표면인 DA_FatigueConfig 소유 — 표기도 거기서 읽는다.
	// 에셋 부재 시 CDO 로 폴백 — 리터럴을 다시 적으면 헤더 기본값과 발산한다(실제로 0.017/0.8 이 0.030/0.9 와 어긋났던 이력)
	const UFatigueConfig* LoadFatigueConfig()
	{
		static const TSoftObjectPtr<UFatigueConfig> ConfigPtr(FSoftObjectPath(
			TEXT("/Game/CompanyGrowth/Data/DA_FatigueConfig.DA_FatigueConfig")));
		const UFatigueConfig* Loaded = ConfigPtr.LoadSynchronous();
		return Loaded ? Loaded : GetDefault<UFatigueConfig>();
	}

	// 배출 간격(초) — GetSpeedFactor 의 스탯 항만. 버프/책상 보너스는 직원별 상황이라 카드에서 제외한다.
	float FeltWorkIntervalSec(float V)
	{
		const float SF = 1.0f + V * EmployeeStatTuning::WorkSpeedFactorPerPoint;
		return FMath::Clamp(UEmployeeBehaviorComponent::BaseWorkInterval / SF,
			UEmployeeBehaviorComponent::MinWorkInterval, UEmployeeBehaviorComponent::BaseWorkInterval);
	}

	// 평균 이탈 간격(초) = 1 / 실효 초당 폭주확률. GetEffectiveBoltChance 의 포화곡선을 그대로 미러링한다
	// (구 표기는 폐기된 절벽식 Min(V×Scale, Cap) 을 쓰고 있어 실제 동작과 어긋나 있었다).
	float FeltBoltIntervalSec(float V)
	{
		const UFatigueConfig* Cfg = LoadFatigueConfig();
		const float HalfPoint = FMath::Max(1.0f, Cfg->MaxBoltReduction / FMath::Max(KINDA_SMALL_NUMBER, Cfg->ComposureBoltScale));
		const float Reduction = Cfg->MaxBoltReduction * (V / (V + HalfPoint));
		const float PerSec = Cfg->BoltChancePerSec * (1.0f - Reduction);
		return PerSec > KINDA_SMALL_NUMBER ? 1.0f / PerSec : 0.0f;
	}

	// 개발 중 꾸벅(Slacking)까지 걸리는 시간(초) = 임계 / 초당 피로 누적.
	// ⚠ GetWorkDuration(BaseWorkDuration × 체력) 과 헷갈리지 말 것 — 그건 Operation/Idle 의 착석 근무 사이클이고,
	//   졸음은 개발(Stage) 전용 피로 사슬(TickFatigue)이 소유한다. 라벨이 "체력(졸음)" 이므로 이쪽이 맞다.
	float FeltDrowsySec(float V)
	{
		const UFatigueConfig* Cfg = LoadFatigueConfig();
		const float GainPerSec = Cfg->GainPerSec / (1.0f + V * Cfg->StaminaGainScale);
		return GainPerSec > KINDA_SMALL_NUMBER ? Cfg->SlackingThreshold / GainPerSec : 0.0f;
	}

	// 착석 근무/휴식 사이클(Operation·Idle). 졸음과 다른 축이라 툴팁 보조 문장에만 쓴다.
	// 기본치는 컴포넌트 CDO 가 소유(EditDefaultsOnly) — 리터럴 재기입 금지.
	float FeltWorkDurationSec(float V)
	{
		return GetDefault<UEmployeeBehaviorComponent>()->BaseWorkDuration
			* (1.0f + V * EmployeeStatTuning::WorkDurationStaminaScale);
	}

	float FeltFocusPercent(float V)
	{
		return FMath::Min(V * EmployeeStatTuning::FocusPerPoint, EmployeeStatTuning::MaxFocusChance) * 100.0f;
	}

	// 이 직원이 실제로 크리를 터뜨릴 확률 = 스테이지 기본 + 스탯 기여. 스탯분만 찍으면 ★0 에서 0.7% 라
	// "크리가 안 터지는데 왜 0.7% 냐"가 되고, 반대로 기본치를 빼고 보면 실제보다 낮게 속인다.
	// 버프/피버/큐브는 상황값이라 제외 — 카드가 약속하는 건 "가만히 뒀을 때의 이 직원"이다.
	float FeltCritPercent(float V)
	{
		const float Total = EmployeeStatTuning::BaseCritChance + V * EmployeeStatTuning::CritChancePerPoint;
		return FMath::Min(Total, EmployeeStatTuning::MaxCritChance) * 100.0f;
	}

	// 값 자리 한 칸에 들어갈 체감 수치 1개 + 그 단위. 값/델타 두 표면이 같은 규칙을 쓰도록 여기 한 곳에 모은다.
	struct FFeltReadout
	{
		float Value = 0.0f;
		const TCHAR* Suffix = TEXT("");
		int32 Decimals = 0;
		bool bAlwaysSign = false;   // 증가만 하는 배율 축(경험치)은 평시에도 "+" 를 달아야 방향이 읽힌다
	};

	FFeltReadout MakeFeltReadout(EEmployeeStatIndex Stat, float V)
	{
		switch (Stat)
		{
		case EEmployeeStatIndex::WorkSpeed:  return { FeltWorkIntervalSec(V), TEXT("초"), 2, false };
		case EEmployeeStatIndex::CritChance: return { FeltCritPercent(V), TEXT("%"), 1, false };
		case EEmployeeStatIndex::Composure:  return { FeltBoltIntervalSec(V), TEXT("초"), 0, false };
		case EEmployeeStatIndex::ExpGain:    return { V * EmployeeStatTuning::ExpGainMultiplierPerPoint * 100.0f, TEXT("%"), 0, true };
		case EEmployeeStatIndex::Stamina:    return { FeltDrowsySec(V), TEXT("초"), 0, false };
		case EEmployeeStatIndex::Focus:      return { FeltFocusPercent(V), TEXT("%"), 0, false };
		default:                             return {};
		}
	}

	// ⚠ 포맷 문자열은 삼항으로 고르지 말 것 — FString::Printf 가 문자 배열 참조로 받아 리터럴을 강제하는데
	//   삼항이 두 리터럴을 const TCHAR* 로 붕괴시켜 static_assert 가 터진다.
	FString FormatFelt(float Value, const TCHAR* Suffix, int32 Decimals, bool bSigned)
	{
		FString Num;
		if (bSigned)
		{
			switch (Decimals)
			{
			case 2:  Num = FString::Printf(TEXT("%+.2f"), Value); break;
			case 1:  Num = FString::Printf(TEXT("%+.1f"), Value); break;
			default: Num = FString::Printf(TEXT("%+.0f"), Value); break;
			}
		}
		else
		{
			switch (Decimals)
			{
			case 2:  Num = FString::Printf(TEXT("%.2f"), Value); break;
			case 1:  Num = FString::Printf(TEXT("%.1f"), Value); break;
			default: Num = FString::Printf(TEXT("%.0f"), Value); break;
			}
		}
		return Num + Suffix;
	}
}

FText UEmployeeStatsHelper::GetStatFeltText(uint8 StatIndex, int32 EffectiveValue)
{
	const EEmployeeStatIndex Stat = static_cast<EEmployeeStatIndex>(StatIndex);
	if (Stat >= EEmployeeStatIndex::Count)
	{
		return FText::GetEmpty();
	}

	const FFeltReadout R = MakeFeltReadout(Stat, static_cast<float>(EffectiveValue));
	return FText::FromString(FormatFelt(R.Value, R.Suffix, R.Decimals, R.bAlwaysSign));
}

FText UEmployeeStatsHelper::GetStatFeltDeltaText(uint8 StatIndex, int32 BaseValue, int32 EffectiveValue)
{
	const EEmployeeStatIndex Stat = static_cast<EEmployeeStatIndex>(StatIndex);
	if (Stat >= EEmployeeStatIndex::Count || BaseValue == EffectiveValue)
	{
		return FText::GetEmpty();
	}

	const FFeltReadout Before = MakeFeltReadout(Stat, static_cast<float>(BaseValue));
	const FFeltReadout After  = MakeFeltReadout(Stat, static_cast<float>(EffectiveValue));
	return FText::FromString(FormatFelt(After.Value - Before.Value, After.Suffix, After.Decimals, true));
}

FText UEmployeeStatsHelper::GetStatEffectText(uint8 StatIndex, int32 EffectiveValue)
{
	const float V = static_cast<float>(EffectiveValue);

	switch (static_cast<EEmployeeStatIndex>(StatIndex))
	{
	case EEmployeeStatIndex::WorkSpeed:
		return FText::FromString(FString::Printf(TEXT("%.2f초마다 성과를 냅니다"), FeltWorkIntervalSec(V)));

	case EEmployeeStatIndex::CritChance:
		return FText::FromString(FString::Printf(TEXT("크리티컬이 %.1f%% 확률로 터집니다"), FeltCritPercent(V)));

	case EEmployeeStatIndex::Composure:
		return FText::FromString(FString::Printf(TEXT("평균 %.0f초마다 자리를 이탈합니다"), FeltBoltIntervalSec(V)));

	case EEmployeeStatIndex::ExpGain:
		return FText::FromString(FString::Printf(TEXT("경험치를 %.0f%% 더 받습니다"),
			V * EmployeeStatTuning::ExpGainMultiplierPerPoint * 100.0f));

	// 졸음은 개발(Stage) 전용 피로 사슬 소유 — 착석 근무 사이클(FeltWorkDurationSec)과 다른 축이라 보조 문장으로 뺀다
	case EEmployeeStatIndex::Stamina:
		return FText::FromString(FString::Printf(TEXT("개발 %.0f초 만에 꾸벅합니다 (평소 근무 %.0f초)"),
			FeltDrowsySec(V), FeltWorkDurationSec(V)));

	// 표기값은 "주 직능 강제 지정" 확률이라 하한이다 — 실패한 굴림에서도 자기 분야가 뽑힐 수 있어
	// 실제 관측 비율은 이보다 높다. 실효 확률은 프로젝트 구성마다 달라 카드에 못 박는다.
	case EEmployeeStatIndex::Focus:
		return FText::FromString(FString::Printf(TEXT("%.0f%% 확률로 자기 분야 성과를 냅니다"), FeltFocusPercent(V)));

	default:
		return FText::GetEmpty();
	}
}
