#include "Misc/AutomationTest.h"
#include "Office/OfficeSeatAssignmentRules.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace OfficeSeatAssignmentTests
{
FEmployeeInstance Employee(int32 EmployeeID, int32 BuildingIndex)
{
	FEmployeeInstance Result;
	Result.EmployeeID = EmployeeID;
	Result.AssignedBuildingIndex = BuildingIndex;
	Result.bIsAssigned = true;
	return Result;
}

FWorkstationSaveData Workstation(int32 EmployeeID)
{
	FWorkstationSaveData Result;
	FChairSlotState Chair;
	Chair.OccupantEmployeeID = EmployeeID;
	Chair.CurrentSkinID = TEXT("ChairSkinFixture");
	Result.ChairStates.Add(Chair);
	return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCGROfficeSavedSeatValidationTest,
	"CGR.Office.SeatAssignment.SavedSeatValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCGROfficeSavedSeatValidationTest::RunTest(const FString& Parameters)
{
	using namespace OfficeSeatAssignmentTests;
	FEmployeeInstance BenchedEmployee = Employee(16, 4);
	BenchedEmployee.bIsAssigned = false;
	const TArray<FEmployeeInstance> Roster = { Employee(14, 4), Employee(15, 4), Employee(20, 5), BenchedEmployee };
	TSet<int32> SeatedEmployees;
	FWorkstationSaveData MissingEmployee = Workstation(1);
	OfficeSeatAssignmentRules::ValidateSavedSeats(MissingEmployee, Roster, 4, SeatedEmployees);
	TestEqual(TEXT("Missing employee leaves a usable empty seat"), MissingEmployee.ChairStates[0].OccupantEmployeeID, INDEX_NONE);
	TestEqual(TEXT("Clearing an assignment preserves chair skin"), MissingEmployee.ChairStates[0].CurrentSkinID, FName(TEXT("ChairSkinFixture")));

	FWorkstationSaveData ValidEmployee = Workstation(14);
	OfficeSeatAssignmentRules::ValidateSavedSeats(ValidEmployee, Roster, 4, SeatedEmployees);
	TestEqual(TEXT("Valid saved assignment survives before the live roster loads"), ValidEmployee.ChairStates[0].OccupantEmployeeID, 14);

	FWorkstationSaveData DuplicateEmployee = Workstation(14);
	OfficeSeatAssignmentRules::ValidateSavedSeats(DuplicateEmployee, Roster, 4, SeatedEmployees);
	TestEqual(TEXT("One employee cannot reserve two workstations"), DuplicateEmployee.ChairStates[0].OccupantEmployeeID, INDEX_NONE);

	FWorkstationSaveData ForeignEmployee = Workstation(20);
	OfficeSeatAssignmentRules::ValidateSavedSeats(ForeignEmployee, Roster, 4, SeatedEmployees);
	TestEqual(TEXT("Employee belonging to another building cannot reserve this seat"), ForeignEmployee.ChairStates[0].OccupantEmployeeID, INDEX_NONE);

	FWorkstationSaveData BenchedSeat = Workstation(16);
	OfficeSeatAssignmentRules::ValidateSavedSeats(BenchedSeat, Roster, 4, SeatedEmployees);
	TestEqual(TEXT("Benched employee cannot keep a stale seat reservation"), BenchedSeat.ChairStates[0].OccupantEmployeeID, INDEX_NONE);

	FWorkstationSaveData EmptySeat = Workstation(INDEX_NONE);
	OfficeSeatAssignmentRules::ValidateSavedSeats(EmptySeat, Roster, 4, SeatedEmployees);
	TestEqual(TEXT("Empty seat stays empty"), EmptySeat.ChairStates[0].OccupantEmployeeID, INDEX_NONE);

	FWorkstationSaveData SecondValidEmployee = Workstation(15);
	OfficeSeatAssignmentRules::ValidateSavedSeats(SecondValidEmployee, Roster, 4, SeatedEmployees);
	TestEqual(TEXT("Different valid employee retains their seat"), SecondValidEmployee.ChairStates[0].OccupantEmployeeID, 15);

	TSet<int32> ReloadedEmployees;
	TestEqual(TEXT("Reloading a valid seat clears nothing"),
		OfficeSeatAssignmentRules::ValidateSavedSeats(ValidEmployee, Roster, 4, ReloadedEmployees), 0);
	TestEqual(TEXT("Fresh load preserves the same valid assignment"), ValidEmployee.ChairStates[0].OccupantEmployeeID, 14);

	TSet<int32> DoubleDeskEmployees;
	FWorkstationSaveData DoubleDesk = Workstation(15);
	DoubleDesk.WorkstationType = EWorkstationType::Double;
	DoubleDesk.ChairStates.Add(Workstation(15).ChairStates[0]);
	TestEqual(TEXT("Duplicate chairs release exactly one reservation"),
		OfficeSeatAssignmentRules::ValidateSavedSeats(DoubleDesk, Roster, 4, DoubleDeskEmployees), 1);
	TestEqual(TEXT("First chair keeps its valid employee"), DoubleDesk.ChairStates[0].OccupantEmployeeID, 15);
	TestEqual(TEXT("Second chair cannot reserve the same employee"), DoubleDesk.ChairStates[1].OccupantEmployeeID, INDEX_NONE);

	TSet<int32> EmptyRosterEmployees;
	FWorkstationSaveData OrphanedDesk = Workstation(14);
	OrphanedDesk.ChairStates.Add(Workstation(15).ChairStates[0]);
	TestEqual(TEXT("Empty roster releases all reservations"),
		OfficeSeatAssignmentRules::ValidateSavedSeats(OrphanedDesk, {}, 4, EmptyRosterEmployees), 2);
	TestEqual(TEXT("Empty roster clears first chair"), OrphanedDesk.ChairStates[0].OccupantEmployeeID, INDEX_NONE);
	TestEqual(TEXT("Empty roster clears second chair"), OrphanedDesk.ChairStates[1].OccupantEmployeeID, INDEX_NONE);
	return true;
}
#endif
