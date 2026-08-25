// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ManagerBase.generated.h"

class UCGGameInstance;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBeginDestroyDelegate, UManagerBase* Manager);
UCLASS()
class COMPANYGROWTHRENEWAL_API UManagerBase : public UObject
{
	GENERATED_BODY()
	
protected:
	UCGGameInstance* GameInstance;

	FName ManagerName;
	TSharedPtr<FOnBeginDestroyDelegate> OnBeginDestroyDelegate = nullptr;

	void SetManagerName(FName name)
	{
		ManagerName = name;
	}

	bool IsRelease = false;
	bool IsDestroy = false;

public:
	void SetGameInstance(UCGGameInstance* instance);

	template <typename UserClass>
	FDelegateHandle AddOnBeginDestroyHandle(UserClass* classInstance,
		typename TMemFunPtrType<false, UserClass, void(UManagerBase* Manager)>::Type InFunc)
	{
		static_assert(std::is_base_of_v<UObject, UserClass>, "UserClass must be derived from UObject");

		if (!OnBeginDestroyDelegate)
		{
			OnBeginDestroyDelegate = MakeShareable(new FOnBeginDestroyDelegate()); // MakeShareable을 통해 TSharedPtr로 래핑
		}

		return OnBeginDestroyDelegate->AddUObject(classInstance, InFunc);
	}

	void ClearOnBeginDestroyHandle()
	{
		if (OnBeginDestroyDelegate.IsValid())
		{
			OnBeginDestroyDelegate->Clear();
		}
	}

	FString GetManagerName() const
	{
		return ManagerName.ToString();
	}

	virtual void Init()
	{
	}

	virtual void Prepare()
	{
	}

	virtual void Release()
	{
		IsRelease = true;
	}

	virtual void Destroy()
	{
		IsDestroy = true;
	}

	virtual void BeginDestroy() override;
};
