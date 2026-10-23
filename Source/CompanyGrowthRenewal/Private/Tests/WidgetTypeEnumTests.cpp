#include "Misc/AutomationTest.h"
#include "Enum/WidgetType.h"

// 값이 겹치면 DT_WidgetClass 를 저장할 때 행 종류가 먼저 선언된 이름으로 바뀐다 ― 겹침은 컴파일러도 UHT 도 안 막는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWidgetTypeUniqueValuesTest,
	"CGR.WidgetType.UniqueValues",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWidgetTypeUniqueValuesTest::RunTest(const FString& Parameters)
{
	const UEnum* Enum = StaticEnum<EWidgetType>();
	if (!TestNotNull(TEXT("EWidgetType 리플렉션"), Enum))
	{
		return false;
	}
	TMap<int64, FString> FirstByValue;
	for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
	{
		const int64 Value = Enum->GetValueByIndex(Index);
		const FString Name = Enum->GetNameStringByIndex(Index);
		if (const FString* Taken = FirstByValue.Find(Value))
		{
			AddError(FString::Printf(TEXT("EWidgetType 값 %lld 가 %s 와 %s 에 겹친다 ― 새 값은 enum 맨 끝에 붙일 것"), Value, **Taken, *Name));
			continue;
		}
		FirstByValue.Add(Value, Name);
	}
	return true;
}
