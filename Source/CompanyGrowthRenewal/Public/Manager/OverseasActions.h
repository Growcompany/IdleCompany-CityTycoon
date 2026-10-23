#pragma once

#include "CoreMinimal.h"
#include "Enum/CountryType.h"

class UGameInstance;

// 지도 말풍선·모두 받기·서랍이 같은 판정을 쓰게 모은 받기/팔기 행동. 토스트·사운드는 호출부 몫이다.
namespace OverseasActions
{
	// 자동 수령 나라의 광산은 틱이 수령하므로 대기로 세지 않는다.
	COMPANYGROWTHRENEWAL_API int32 CountPending(const UGameInstance* GI, ECountryType Country);
	COMPANYGROWTHRENEWAL_API int32 CountAllPending(const UGameInstance* GI);

	// 나라 하나는 덜 찬 광산도 받는다(1개 이상). 반환 = 받은 총량.
	COMPANYGROWTHRENEWAL_API int64 ClaimCountry(UGameInstance* GI, ECountryType Country);

	// 서랍 시설 페이지의 큰 버튼 ― 한 시설만 받는다(ClaimCountry = 두 합).
	COMPANYGROWTHRENEWAL_API int64 ClaimCountryFactory(UGameInstance* GI, ECountryType Country);
	COMPANYGROWTHRENEWAL_API int64 ClaimCountryMine(UGameInstance* GI, ECountryType Country);

	// 생산 시작 창과 서랍 「생산 시작」이 같은 시작 경로를 쓴다 ― 라인 시작 성공 뒤에만 재료·칩·주문 수량을 뺀다.
	// 반환 = 새 라인 ID, 실패면 0 이하 + OutFailMessage.
	COMPANYGROWTHRENEWAL_API int32 StartLineFromOrder(UGameInstance* GI, ECountryType Country, int32 OrderID,
		int32 Quantity, FText& OutFailMessage);

	// 범위는 도크 모두 받기와 같다 ― 완료 공장 라인 + 1개 이상 쌓인 광산 라인.
	COMPANYGROWTHRENEWAL_API int64 ClaimAll(UGameInstance* GI);

	// 월드맵 덮개와 같은 판정(UWorldMapPanelWidget::ResolveCountryState != Locked).
	COMPANYGROWTHRENEWAL_API bool IsCountryUnlocked(const UGameInstance* GI, ECountryType Country);

	// 진출 판정이 세는 시설 수 = 광산 라인 + 공장 라인.
	COMPANYGROWTHRENEWAL_API int32 CountFacilities(const UGameInstance* GI, ECountryType Country);

	COMPANYGROWTHRENEWAL_API int64 PreviewSellAll(const UGameInstance* GI, ECountryType Port);

	// 값이 0 이면 말풍선을 띄울 이유가 없으므로 false 다.
	COMPANYGROWTHRENEWAL_API bool FindBestSalePort(const UGameInstance* GI, ECountryType& OutPort, int64& OutMoney);

	// 같은 산업 제품이 여럿이면 앞 판매가 수요를 깎아 PreviewSellAll 보다 적게 나올 수 있다.
	COMPANYGROWTHRENEWAL_API int64 SellAllAt(UGameInstance* GI, ECountryType Port);
}
