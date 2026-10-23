#include "Entity/Plot/PlotWalkwayRules.h"

#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace PlotWalkwayPrivate
{
constexpr int32 MaxCells = 8192;
constexpr int32 MaxTargets = 128;
constexpr int32 Unreached = MAX_int32;

bool IsFinitePoint(const FVector2D& Point)
{
	return FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y);
}

bool IsValidBox(const FBox2D& Bounds)
{
	return Bounds.bIsValid && IsFinitePoint(Bounds.Min) && IsFinitePoint(Bounds.Max)
		&& Bounds.Min.X < Bounds.Max.X && Bounds.Min.Y < Bounds.Max.Y
		&& FMath::IsFinite(Bounds.Max.X - Bounds.Min.X)
		&& FMath::IsFinite(Bounds.Max.Y - Bounds.Min.Y)
		&& IsFinitePoint(Bounds.GetCenter());
}

bool Overlaps(const FBox2D& Left, const FBox2D& Right)
{
	return Left.Min.X < Right.Max.X && Right.Min.X < Left.Max.X
		&& Left.Min.Y < Right.Max.Y && Right.Min.Y < Left.Max.Y;
}

void Neighbors(const FPlotWalkwayGrid& Grid, int32 Cell, int32 (&Out)[4])
{
	const int32 X = Cell % Grid.Size.X;
	const int32 Y = Cell / Grid.Size.X;
	Out[0] = X > 0 ? Cell - 1 : INDEX_NONE;
	Out[1] = X + 1 < Grid.Size.X ? Cell + 1 : INDEX_NONE;
	Out[2] = Y > 0 ? Cell - Grid.Size.X : INDEX_NONE;
	Out[3] = Y + 1 < Grid.Size.Y ? Cell + Grid.Size.X : INDEX_NONE;
}

bool IsGroundAt(const FPlotWalkwayGrid& Grid, const FVector2D& Point)
{
	const double X = (static_cast<double>(Point.X) - Grid.Origin.X) / Grid.CellSize;
	const double Y = (static_cast<double>(Point.Y) - Grid.Origin.Y) / Grid.CellSize;
	if (!FMath::IsFinite(X) || !FMath::IsFinite(Y)
		|| X < 0.0 || Y < 0.0 || X >= Grid.Size.X || Y >= Grid.Size.Y)
	{
		return false;
	}
	return Grid.Ground[FMath::FloorToInt(X) + FMath::FloorToInt(Y) * Grid.Size.X] == 1;
}

// A side entry bridges the cell edge to the target's facing edge. A positive gap
// needs an entire cell width of projected frontage and ground; corner contact cannot connect.
bool HasConnector(const FPlotWalkwayGrid& Grid, const FBox2D& Cell,
	const FBox2D& TargetBox, int32 TargetIndex,
	TConstArrayView<FPlotWalkwayTarget> Targets, TConstArrayView<FBox2D> FixedBlockers,
	const TFunction<bool(const FBox2D&)>& IsOccupied, FBox2D& OutBounds)
{
	OutBounds = FBox2D(ForceInit);
	bool bHorizontal = false;
	double From = 0.0;
	double To = 0.0;
	double ProjectionMin = 0.0;
	double ProjectionMax = 0.0;
	double TargetInside = 0.0;
	const double InsetX = FMath::Min3(1.0, static_cast<double>(Grid.CellSize) * 0.001,
		(TargetBox.Max.X - TargetBox.Min.X) * 0.5);
	const double InsetY = FMath::Min3(1.0, static_cast<double>(Grid.CellSize) * 0.001,
		(TargetBox.Max.Y - TargetBox.Min.Y) * 0.5);

	if (Cell.Max.X <= TargetBox.Min.X && TargetBox.Min.X - Cell.Max.X <= Grid.CellSize)
	{
		bHorizontal = true;
		From = Cell.Max.X;
		To = TargetBox.Min.X;
		ProjectionMin = FMath::Max(Cell.Min.Y, TargetBox.Min.Y);
		ProjectionMax = FMath::Min(Cell.Max.Y, TargetBox.Max.Y);
		TargetInside = TargetBox.Min.X + InsetX;
	}
	else if (TargetBox.Max.X <= Cell.Min.X && Cell.Min.X - TargetBox.Max.X <= Grid.CellSize)
	{
		bHorizontal = true;
		From = Cell.Min.X;
		To = TargetBox.Max.X;
		ProjectionMin = FMath::Max(Cell.Min.Y, TargetBox.Min.Y);
		ProjectionMax = FMath::Min(Cell.Max.Y, TargetBox.Max.Y);
		TargetInside = TargetBox.Max.X - InsetX;
	}
	else if (Cell.Max.Y <= TargetBox.Min.Y && TargetBox.Min.Y - Cell.Max.Y <= Grid.CellSize)
	{
		From = Cell.Max.Y;
		To = TargetBox.Min.Y;
		ProjectionMin = FMath::Max(Cell.Min.X, TargetBox.Min.X);
		ProjectionMax = FMath::Min(Cell.Max.X, TargetBox.Max.X);
		TargetInside = TargetBox.Min.Y + InsetY;
	}
	else if (TargetBox.Max.Y <= Cell.Min.Y && Cell.Min.Y - TargetBox.Max.Y <= Grid.CellSize)
	{
		From = Cell.Min.Y;
		To = TargetBox.Max.Y;
		ProjectionMin = FMath::Max(Cell.Min.X, TargetBox.Min.X);
		ProjectionMax = FMath::Min(Cell.Max.X, TargetBox.Max.X);
		TargetInside = TargetBox.Max.Y - InsetY;
	}
	else
	{
		return false;
	}
	if (ProjectionMin >= ProjectionMax)
	{
		return false;
	}
	const double Gap = FMath::Abs(To - From);
	if (Gap > 0.0 && ProjectionMax - ProjectionMin < Grid.CellSize)
	{
		return false;
	}
	const double MidProjection = (ProjectionMin + ProjectionMax) * 0.5;
	const FVector2D Inside = bHorizontal
		? FVector2D(TargetInside, MidProjection) : FVector2D(MidProjection, TargetInside);
	if (!IsGroundAt(Grid, Inside))
	{
		return false;
	}
	if (Gap == 0.0)
	{
		return true;
	}

	// A positive-gap connector is a full cell-width rectangle. The free cell and
	// target share a full projected side, so the rectangle bridges their faces.
	if (bHorizontal)
	{
		OutBounds = FBox2D(FVector2D(FMath::Min(From, To), Cell.Min.Y),
			FVector2D(FMath::Max(From, To), Cell.Max.Y));
	}
	else
	{
		OutBounds = FBox2D(FVector2D(Cell.Min.X, FMath::Min(From, To)),
			FVector2D(Cell.Max.X, FMath::Max(From, To)));
	}
	const double PlotMaxX = Grid.Origin.X + static_cast<double>(Grid.Size.X) * Grid.CellSize;
	const double PlotMaxY = Grid.Origin.Y + static_cast<double>(Grid.Size.Y) * Grid.CellSize;
	if (OutBounds.Min.X < Grid.Origin.X || OutBounds.Min.Y < Grid.Origin.Y
		|| OutBounds.Max.X > PlotMaxX || OutBounds.Max.Y > PlotMaxY)
	{
		return false;
	}
	const int32 FirstX = FMath::FloorToInt((OutBounds.Min.X - Grid.Origin.X) / Grid.CellSize);
	const int32 LastX = FMath::CeilToInt((OutBounds.Max.X - Grid.Origin.X) / Grid.CellSize) - 1;
	const int32 FirstY = FMath::FloorToInt((OutBounds.Min.Y - Grid.Origin.Y) / Grid.CellSize);
	const int32 LastY = FMath::CeilToInt((OutBounds.Max.Y - Grid.Origin.Y) / Grid.CellSize) - 1;
	for (int32 Y = FirstY; Y <= LastY; ++Y)
	{
		for (int32 X = FirstX; X <= LastX; ++X)
		{
			if (Grid.Ground[X + Y * Grid.Size.X] != 1)
			{
				return false;
			}
		}
	}
	for (const FBox2D& Blocker : FixedBlockers)
	{
		if (Overlaps(OutBounds, Blocker))
		{
			return false;
		}
	}
	if (IsOccupied)
	{
		if (IsOccupied(OutBounds)) { return false; }
	}
	else
	{
		for (int32 Other = 0; Other < Targets.Num(); ++Other)
		{
			if (Other != TargetIndex && Overlaps(OutBounds, Targets[Other].Bounds))
			{
				return false;
			}
		}
	}
	return true;
}

struct FRouteTree
{
	TArray<int32> Distance;
	TArray<int32> Turns;
	TArray<int32> Parent;
};

FRouteTree BuildRouteTree(const FPlotWalkwayGrid& Grid, const TArray<uint8>& Free, int32 Entrance)
{
	FRouteTree Tree;
	Tree.Distance.Init(Unreached, Free.Num());
	Tree.Turns.Init(Unreached, Free.Num() * 4);
	Tree.Parent.Init(INDEX_NONE, Free.Num() * 4);
	TArray<int32> Queue;
	Queue.Reserve(Free.Num());
	Queue.Add(Entrance);
	Tree.Distance[Entrance] = 0;
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const int32 Cell = Queue[Head];
		int32 Adjacent[4];
		Neighbors(Grid, Cell, Adjacent);
		for (int32 Direction = 0; Direction < 4; ++Direction)
		{
			const int32 Next = Adjacent[Direction];
			if (Next == INDEX_NONE || Free[Next] == 0)
			{
				continue;
			}
			if (Tree.Distance[Next] == Unreached)
			{
				Tree.Distance[Next] = Tree.Distance[Cell] + 1;
				Queue.Add(Next);
			}
			if (Tree.Distance[Next] != Tree.Distance[Cell] + 1)
			{
				continue;
			}
			const int32 NextState = Next * 4 + Direction;
			if (Cell == Entrance)
			{
				Tree.Turns[NextState] = 0;
				continue;
			}
			for (int32 OldDirection = 0; OldDirection < 4; ++OldDirection)
			{
				const int32 OldState = Cell * 4 + OldDirection;
				if (Tree.Turns[OldState] == Unreached)
				{
					continue;
				}
				const int32 Turns = Tree.Turns[OldState] + (OldDirection == Direction ? 0 : 1);
				if (Turns < Tree.Turns[NextState]
					|| (Turns == Tree.Turns[NextState] && OldState < Tree.Parent[NextState]))
				{
					Tree.Turns[NextState] = Turns;
					Tree.Parent[NextState] = OldState;
				}
			}
		}
	}
	return Tree;
}

int32 BestState(const FRouteTree& Tree, int32 Cell, int32 Entrance)
{
	if (Cell == Entrance)
	{
		return INDEX_NONE;
	}
	int32 Winner = INDEX_NONE;
	for (int32 Direction = 0; Direction < 4; ++Direction)
	{
		const int32 State = Cell * 4 + Direction;
		if (Tree.Turns[State] != Unreached
			&& (Winner == INDEX_NONE || Tree.Turns[State] < Tree.Turns[Winner]))
		{
			Winner = State;
		}
	}
	return Winner;
}
}

FVector2D FPlotWalkwayRules::CellCenter(const FPlotWalkwayGrid& Grid, int32 Index)
{
	const int32 X = Grid.Size.X > 0 ? Index % Grid.Size.X : 0;
	const int32 Y = Grid.Size.X > 0 ? Index / Grid.Size.X : 0;
	return Grid.Origin + FVector2D((X + 0.5f) * Grid.CellSize, (Y + 0.5f) * Grid.CellSize);
}

FBox2D FPlotWalkwayRules::CellBounds(const FPlotWalkwayGrid& Grid, int32 Index)
{
	const int32 X = Grid.Size.X > 0 ? Index % Grid.Size.X : 0;
	const int32 Y = Grid.Size.X > 0 ? Index / Grid.Size.X : 0;
	const FVector2D Min = Grid.Origin + FVector2D(X * Grid.CellSize, Y * Grid.CellSize);
	return FBox2D(Min, Min + FVector2D(Grid.CellSize, Grid.CellSize));
}

FBox2D FPlotWalkwayRules::TrimStraightRoad(const FBox2D& GroundedRoad, const FVector2D& Inward,
	const TFunction<bool(const FBox2D&)>& IsOccupied)
{
	using namespace PlotWalkwayPrivate;
	if (!IsValidBox(GroundedRoad) || !IsFinitePoint(Inward)) { return FBox2D(ForceInit); }
	const bool bAlongX = FMath::Abs(Inward.X) > SMALL_NUMBER && FMath::Abs(Inward.Y) <= SMALL_NUMBER;
	const bool bAlongY = FMath::Abs(Inward.Y) > SMALL_NUMBER && FMath::Abs(Inward.X) <= SMALL_NUMBER;
	if (!bAlongX && !bAlongY) { return FBox2D(ForceInit); }
	if (!IsOccupied) { return GroundedRoad; }
	const double FullLength = bAlongX
		? GroundedRoad.Max.X - GroundedRoad.Min.X : GroundedRoad.Max.Y - GroundedRoad.Min.Y;
	const bool bPositive = bAlongX ? Inward.X > 0.0 : Inward.Y > 0.0;
	auto Prefix = [&](double Length) -> FBox2D
	{
		FBox2D Bounds = GroundedRoad;
		if (bAlongX)
		{
			if (bPositive) { Bounds.Max.X = Bounds.Min.X + Length; }
			else { Bounds.Min.X = Bounds.Max.X - Length; }
		}
		else
		{
			if (bPositive) { Bounds.Max.Y = Bounds.Min.Y + Length; }
			else { Bounds.Min.Y = Bounds.Max.Y - Length; }
		}
		return Bounds;
	};
	if (IsOccupied(Prefix(FMath::Min(0.5, FullLength)))) { return FBox2D(ForceInit); }
	if (!IsOccupied(GroundedRoad)) { return GroundedRoad; }
	double Low = 0.0;
	double High = FullLength;
	while (High - Low > 0.5)
	{
		const double Middle = (Low + High) * 0.5;
		if (IsOccupied(Prefix(Middle))) { High = Middle; }
		else { Low = Middle; }
	}
	const double ClearLength = Low - 0.5;
	return ClearLength > 0.0 ? Prefix(ClearLength) : FBox2D(ForceInit);
}

FPlotRoadNetwork FPlotWalkwayRules::BuildRoadNetwork(const FBox2D& SearchBounds,
	const FVector2D& Start, const FVector2D& Inward, double Width, double Step,
	const TFunction<bool(const FBox2D&)>& IsClear, const FPlotRoadNetworkOptions& Options)
{
	using namespace PlotWalkwayPrivate;
	FPlotRoadNetwork Result;
	if (!IsValidBox(SearchBounds) || !IsFinitePoint(Start) || !IsFinitePoint(Inward)
		|| !FMath::IsFinite(Width) || !FMath::IsFinite(Step)
		|| Width <= 0.0 || Step <= 0.0 || Step > Width || !IsClear
		|| !FMath::IsFinite(Options.MinimumDeadEndLength)
		|| !FMath::IsFinite(Options.InitialLength)
		|| !FMath::IsFinite(Options.MinimumParallelGap)
		|| Options.MinimumDeadEndLength < 0.0 || Options.InitialLength < 0.0
		|| Options.MinimumParallelGap < 0.0)
	{
		return Result;
	}
	const bool bInwardX = FMath::Abs(Inward.X) > SMALL_NUMBER && FMath::Abs(Inward.Y) <= SMALL_NUMBER;
	const bool bInwardY = FMath::Abs(Inward.Y) > SMALL_NUMBER && FMath::Abs(Inward.X) <= SMALL_NUMBER;
	if (!bInwardX && !bInwardY) { return Result; }
	const FVector2D Forward = bInwardX
		? FVector2D(Inward.X > 0.0 ? 1.0 : -1.0, 0.0)
		: FVector2D(0.0, Inward.Y > 0.0 ? 1.0 : -1.0);
	const FVector2D Left(-Forward.Y, Forward.X);
	const double Half = Width * 0.5;
	const double LowX = std::ceil((SearchBounds.Min.X + Half - Start.X) / Step);
	const double HighX = std::floor((SearchBounds.Max.X - Half - Start.X) / Step);
	const double LowY = std::ceil((SearchBounds.Min.Y + Half - Start.Y) / Step);
	const double HighY = std::floor((SearchBounds.Max.Y - Half - Start.Y) / Step);
	if (!FMath::IsFinite(LowX) || !FMath::IsFinite(HighX)
		|| !FMath::IsFinite(LowY) || !FMath::IsFinite(HighY)
		|| LowX > 0.0 || HighX < 0.0 || LowY > 0.0 || HighY < 0.0
		|| LowX < -MAX_int32 / 4.0 || HighX > MAX_int32 / 4.0
		|| LowY < -MAX_int32 / 4.0 || HighY > MAX_int32 / 4.0)
	{
		return Result;
	}
	const int32 MinX = static_cast<int32>(LowX);
	const int32 MinY = static_cast<int32>(LowY);
	const int32 SizeX = static_cast<int32>(HighX - LowX + 1.0);
	const int32 SizeY = static_cast<int32>(HighY - LowY + 1.0);
	const int64 Count64 = static_cast<int64>(SizeX) * SizeY;
	if (SizeX <= 0 || SizeY <= 0 || Count64 <= 0 || Count64 > MaxCells) { return Result; }
	const int32 Count = static_cast<int32>(Count64);
	const int32 Root = -MinX + (-MinY) * SizeX;
	const FVector2D WorldDirections[4] = {
		FVector2D(1.0, 0.0), FVector2D(0.0, 1.0),
		FVector2D(-1.0, 0.0), FVector2D(0.0, -1.0)
	};
	TArray<FVector2D> Centers;
	TArray<FBox2D> Squares;
	TArray<uint8> Free;
	Centers.Reserve(Count);
	Squares.Reserve(Count);
	Free.Reserve(Count);
	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		for (int32 X = 0; X < SizeX; ++X)
		{
			const FVector2D Center = Start + FVector2D((MinX + X) * Step, (MinY + Y) * Step);
			const FBox2D Square(Center - FVector2D(Half, Half), Center + FVector2D(Half, Half));
			Centers.Add(Center);
			Squares.Add(Square);
			const double Progress = FVector2D::DotProduct(Center - Start, Forward);
			Free.Add(Progress >= -0.01 && IsClear(Square) ? 1 : 0);
		}
	}
	if (Free[Root] == 0) { return Result; }

	struct FQueueEntry
	{
		int64 Cost;
		int32 Turns;
		int32 State;
	};
	struct FQueueGreater
	{
		bool operator()(const FQueueEntry& A, const FQueueEntry& B) const
		{
			if (A.Cost != B.Cost) { return A.Cost > B.Cost; }
			if (A.Turns != B.Turns) { return A.Turns > B.Turns; }
			return A.State > B.State;
		}
	};
	std::priority_queue<FQueueEntry, std::vector<FQueueEntry>, FQueueGreater> Queue;
	TArray<int64> Costs;
	TArray<int32> Turns;
	TArray<int32> Parent;
	Costs.Init(MAX_int64, Count * 4);
	Turns.Init(MAX_int32, Count * 4);
	Parent.Init(INDEX_NONE, Count * 4);
	TMap<uint64, bool> EdgeClear;
	auto Adjacent = [&](int32 Node, int32 Direction) -> int32
	{
		const int32 X = Node % SizeX;
		const int32 Y = Node / SizeX;
		if (Direction == 0) { return X + 1 < SizeX ? Node + 1 : INDEX_NONE; }
		if (Direction == 1) { return Y + 1 < SizeY ? Node + SizeX : INDEX_NONE; }
		if (Direction == 2) { return X > 0 ? Node - 1 : INDEX_NONE; }
		return Y > 0 ? Node - SizeX : INDEX_NONE;
	};
	auto EdgeKey = [](int32 A, int32 B) -> uint64
	{
		return (static_cast<uint64>(FMath::Min(A, B)) << 32)
			| static_cast<uint32>(FMath::Max(A, B));
	};
	auto Relax = [&](int32 Node, int32 PreviousDirection, int32 PreviousState,
		int64 CurrentCost, int32 CurrentTurns, int32 Direction)
	{
		const int32 Next = Adjacent(Node, Direction);
		if (Next == INDEX_NONE || Next == Root || Free[Next] == 0) { return; }
		const uint64 Key = EdgeKey(Node, Next);
		const FBox2D Sweep(
			FVector2D(FMath::Min(Squares[Node].Min.X, Squares[Next].Min.X),
				FMath::Min(Squares[Node].Min.Y, Squares[Next].Min.Y)),
			FVector2D(FMath::Max(Squares[Node].Max.X, Squares[Next].Max.X),
				FMath::Max(Squares[Node].Max.Y, Squares[Next].Max.Y)));
		bool* bClear = EdgeClear.Find(Key);
		if (!bClear)
		{
			bClear = &EdgeClear.Add(Key, IsClear(Sweep));
		}
		if (!*bClear || (Options.IsEdgeAllowed
			&& !Options.IsEdgeAllowed(Sweep, WorldDirections[Direction]))) { return; }
		const bool bTurn = PreviousDirection != Direction;
		const int64 NewCost = CurrentCost + 1000 + (bTurn ? 250 : 0);
		const int32 NewTurns = CurrentTurns + (bTurn ? 1 : 0);
		const int32 State = Next * 4 + Direction;
		if (NewCost < Costs[State]
			|| (NewCost == Costs[State] && NewTurns < Turns[State])
			|| (NewCost == Costs[State] && NewTurns == Turns[State]
				&& PreviousState < Parent[State]))
		{
			Costs[State] = NewCost;
			Turns[State] = NewTurns;
			Parent[State] = PreviousState;
			Queue.push({ NewCost, NewTurns, State });
		}
	};
	int32 ForwardDirection = INDEX_NONE;
	for (int32 Direction = 0; Direction < 4; ++Direction)
	{
		if (WorldDirections[Direction] == Forward) { ForwardDirection = Direction; }
	}
	for (int32 Direction = 0; Direction < 4; ++Direction)
	{
		if (Direction != (ForwardDirection + 2) % 4)
		{
			Relax(Root, ForwardDirection, INDEX_NONE, 0, 0, Direction);
		}
	}
	while (!Queue.empty())
	{
		const FQueueEntry Current = Queue.top();
		Queue.pop();
		if (Current.Cost != Costs[Current.State] || Current.Turns != Turns[Current.State]) { continue; }
		const int32 Node = Current.State / 4;
		const int32 PreviousDirection = Current.State % 4;
		for (int32 Direction = 0; Direction < 4; ++Direction)
		{
			Relax(Node, PreviousDirection, Current.State, Current.Cost, Current.Turns, Direction);
		}
	}
	TArray<int32> BestState;
	BestState.Init(INDEX_NONE, Count);
	for (int32 Node = 0; Node < Count; ++Node)
	{
		if (Node == Root) { ++Result.ReachableNodeCount; continue; }
		for (int32 Direction = 0; Direction < 4; ++Direction)
		{
			const int32 State = Node * 4 + Direction;
			const int32 Old = BestState[Node];
			if (Costs[State] != MAX_int64
				&& (Old == INDEX_NONE || Costs[State] < Costs[Old]
					|| (Costs[State] == Costs[Old] && Turns[State] < Turns[Old])
					|| (Costs[State] == Costs[Old] && Turns[State] == Turns[Old] && State < Old)))
			{
				BestState[Node] = State;
			}
		}
		if (BestState[Node] != INDEX_NONE) { ++Result.ReachableNodeCount; }
	}
	int32 Goals[3] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };
	auto BetterGoal = [&](int32 Candidate, int32 Current, int32 Kind) -> bool
	{
		if (Current == INDEX_NONE) { return true; }
		const FVector2D CandidateDelta = Centers[Candidate] - Start;
		const FVector2D CurrentDelta = Centers[Current] - Start;
		const double CandidateForward = FVector2D::DotProduct(CandidateDelta, Forward);
		const double CurrentForward = FVector2D::DotProduct(CurrentDelta, Forward);
		const double CandidateLeft = FVector2D::DotProduct(CandidateDelta, Left);
		const double CurrentLeft = FVector2D::DotProduct(CurrentDelta, Left);
		const double CandidatePrimary = Kind == 0 ? CandidateForward
			: Kind == 1 ? CandidateLeft : -CandidateLeft;
		const double CurrentPrimary = Kind == 0 ? CurrentForward
			: Kind == 1 ? CurrentLeft : -CurrentLeft;
		if (CandidatePrimary != CurrentPrimary) { return CandidatePrimary > CurrentPrimary; }
		if (Kind == 0 && FMath::Abs(CandidateLeft) != FMath::Abs(CurrentLeft))
		{
			return FMath::Abs(CandidateLeft) < FMath::Abs(CurrentLeft);
		}
		if (Kind != 0 && CandidateForward != CurrentForward)
		{
			return CandidateForward > CurrentForward;
		}
		const int32 CandidateState = BestState[Candidate];
		const int32 CurrentState = BestState[Current];
		if (Costs[CandidateState] != Costs[CurrentState])
		{
			return Costs[CandidateState] < Costs[CurrentState];
		}
		if (Turns[CandidateState] != Turns[CurrentState])
		{
			return Turns[CandidateState] < Turns[CurrentState];
		}
		return Candidate < Current;
	};
	for (int32 Node = 0; Node < Count; ++Node)
	{
		if (BestState[Node] == INDEX_NONE) { continue; }
		const double Side = FVector2D::DotProduct(Centers[Node] - Start, Left);
		if (BetterGoal(Node, Goals[0], 0)) { Goals[0] = Node; }
		if (Side > 0.0 && BetterGoal(Node, Goals[1], 1)) { Goals[1] = Node; }
		if (Side < 0.0 && BetterGoal(Node, Goals[2], 2)) { Goals[2] = Node; }
	}
	int32 GoalStates[3] = {
		Goals[0] == INDEX_NONE ? INDEX_NONE : BestState[Goals[0]],
		Goals[1] == INDEX_NONE ? INDEX_NONE : BestState[Goals[1]],
		Goals[2] == INDEX_NONE ? INDEX_NONE : BestState[Goals[2]]
	};
	bool bConnectedGoal[3] = { false, false, false };
	FPlotRoadConnection GoalConnections[3];
	if (Options.FindConnection)
	{
		const FVector2D OutwardDirections[3] = { Forward, Left, -Left };
		for (int32 Kind = 0; Kind < 3; ++Kind)
		{
			int32 FewestTurns = MAX_int32;
			int64 LowestCost = MAX_int64;
			for (int32 Node = 0; Node < Count; ++Node)
			{
				if (Node != Root && BestState[Node] == INDEX_NONE) { continue; }
				FPlotRoadConnection Candidate;
				if (!Options.FindConnection(Centers[Node], OutwardDirections[Kind], Candidate)
					|| !IsValidBox(Candidate.Bounds)
					|| !IsValidBox(Candidate.JunctionBounds)
					|| Candidate.Direction != OutwardDirections[Kind])
				{
					continue;
				}
				int32 CandidateState = INDEX_NONE;
				int32 CandidateTurns = Forward == OutwardDirections[Kind] ? 0 : 1;
				int64 CandidateCost = CandidateTurns * 250;
				if (Node != Root)
				{
					CandidateTurns = MAX_int32;
					CandidateCost = MAX_int64;
					for (int32 Direction = 0; Direction < 4; ++Direction)
					{
						const int32 State = Node * 4 + Direction;
						if (Costs[State] == MAX_int64) { continue; }
						const bool bTurn = WorldDirections[Direction] != OutwardDirections[Kind];
						const int32 TotalTurns = Turns[State] + (bTurn ? 1 : 0);
						const int64 TotalCost = Costs[State] + (bTurn ? 250 : 0);
						if (TotalTurns < CandidateTurns
							|| (TotalTurns == CandidateTurns && TotalCost < CandidateCost)
							|| (TotalTurns == CandidateTurns && TotalCost == CandidateCost
								&& State < CandidateState))
						{
							CandidateTurns = TotalTurns;
							CandidateCost = TotalCost;
							CandidateState = State;
						}
					}
				}
				if (CandidateTurns < FewestTurns
					|| (CandidateTurns == FewestTurns && CandidateCost < LowestCost)
					|| (CandidateTurns == FewestTurns && CandidateCost == LowestCost
						&& Node < Goals[Kind]))
				{
					Goals[Kind] = Node;
					GoalStates[Kind] = CandidateState;
					GoalConnections[Kind] = Candidate;
					bConnectedGoal[Kind] = true;
					FewestTurns = CandidateTurns;
					LowestCost = CandidateCost;
				}
			}
		}
	}
	TSet<uint64> SelectedEdges;
	bool bAcceptedGoal[3] = { false, false, false };
	auto IsNearParallel = [&](uint64 First, uint64 Second) -> bool
	{
		const int32 FirstA = static_cast<int32>(First >> 32);
		const int32 FirstB = static_cast<int32>(First & 0xffffffffu);
		const int32 SecondA = static_cast<int32>(Second >> 32);
		const int32 SecondB = static_cast<int32>(Second & 0xffffffffu);
		const bool bFirstHorizontal = FirstB == FirstA + 1 && FirstA / SizeX == FirstB / SizeX;
		const bool bSecondHorizontal = SecondB == SecondA + 1 && SecondA / SizeX == SecondB / SizeX;
		if (bFirstHorizontal != bSecondHorizontal) { return false; }
		const int32 Axis = bFirstHorizontal ? 0 : 1;
		const int32 CrossAxis = 1 - Axis;
		const double Offset = FMath::Abs(Centers[FirstA][CrossAxis] - Centers[SecondA][CrossAxis]);
		if (Offset < 0.01 || Offset >= Width + Options.MinimumParallelGap) { return false; }
		const double Overlap = FMath::Min(Centers[FirstB][Axis], Centers[SecondB][Axis])
			- FMath::Max(Centers[FirstA][Axis], Centers[SecondA][Axis]);
		return Overlap > 0.01;
	};
	for (int32 Phase = 0; Phase < 2; ++Phase)
	{
		for (int32 Kind = 0; Kind < 3; ++Kind)
		{
			if (Goals[Kind] == INDEX_NONE || bConnectedGoal[Kind] != (Phase == 0)) { continue; }
			TSet<uint64> CandidateEdges;
			for (int32 State = GoalStates[Kind]; State != INDEX_NONE; State = Parent[State])
			{
				const int32 Node = State / 4;
				const int32 PreviousNode = Parent[State] == INDEX_NONE ? Root : Parent[State] / 4;
				CandidateEdges.Add(EdgeKey(Node, PreviousNode));
			}
			bool bTooClose = false;
			if (Options.MinimumParallelGap > 0.0)
			{
				for (const uint64 Candidate : CandidateEdges)
				{
					if (SelectedEdges.Contains(Candidate)) { continue; }
					for (const uint64 Existing : SelectedEdges)
					{
						if (IsNearParallel(Candidate, Existing))
						{
							bTooClose = true;
							break;
						}
					}
					if (bTooClose) { break; }
				}
			}
			if (bTooClose) { continue; }
			bAcceptedGoal[Kind] = true;
			for (const uint64 Key : CandidateEdges) { SelectedEdges.Add(Key); }
		}
	}
	if (Options.MinimumDeadEndLength > 0.0 && !SelectedEdges.IsEmpty())
	{
		TArray<uint8> Protected;
		TArray<uint8> GoalAnchor;
		Protected.Init(0, Count);
		GoalAnchor.Init(0, Count);
		Protected[Root] = 1;
		for (int32 Kind = 0; Kind < 3; ++Kind)
		{
			if (!bAcceptedGoal[Kind]) { continue; }
			GoalAnchor[Goals[Kind]] = 1;
			if (bConnectedGoal[Kind]) { Protected[Goals[Kind]] = 1; }
		}
		bool bRemoved;
		do
		{
			bRemoved = false;
			TArray<int32> Degree;
			Degree.Init(0, Count);
			for (const uint64 Key : SelectedEdges)
			{
				++Degree[static_cast<int32>(Key >> 32)];
				++Degree[static_cast<int32>(Key & 0xffffffffu)];
			}
			for (int32 Leaf = 0; Leaf < Count && !bRemoved; ++Leaf)
			{
				if (Degree[Leaf] != 1 || Protected[Leaf] != 0) { continue; }
				TArray<uint64> Chain;
				int32 Node = Leaf;
				int32 Previous = INDEX_NONE;
				while (true)
				{
					int32 Next = INDEX_NONE;
					for (int32 Direction = 0; Direction < 4; ++Direction)
					{
						const int32 Candidate = Adjacent(Node, Direction);
						if (Candidate != INDEX_NONE && Candidate != Previous
							&& SelectedEdges.Contains(EdgeKey(Node, Candidate)))
						{
							Next = Candidate;
							break;
						}
					}
					if (Next == INDEX_NONE) { break; }
					Chain.Add(EdgeKey(Node, Next));
					Previous = Node;
					Node = Next;
					if (Node == Root || Protected[Node] != 0 || Degree[Node] != 2
						|| GoalAnchor[Node] != 0) { break; }
				}
				const double Length = Chain.Num() * Step
					+ (Node == Root ? Options.InitialLength : 0.0);
				if (!Chain.IsEmpty() && Length < Options.MinimumDeadEndLength)
				{
					for (const uint64 Key : Chain) { SelectedEdges.Remove(Key); }
					bRemoved = true;
				}
			}
		}
		while (bRemoved);
	}
	for (int32 Kind = 0; Kind < 3; ++Kind)
	{
		if (!bAcceptedGoal[Kind] || !bConnectedGoal[Kind]) { continue; }
		const FPlotRoadConnection& Connection = GoalConnections[Kind];
		if (!Result.Connections.ContainsByPredicate([&](const FPlotRoadConnection& Existing)
			{
				return Existing.Bounds.Min == Connection.Bounds.Min
					&& Existing.Bounds.Max == Connection.Bounds.Max
					&& Existing.JunctionBounds.Min == Connection.JunctionBounds.Min
					&& Existing.JunctionBounds.Max == Connection.JunctionBounds.Max
					&& Existing.Direction == Connection.Direction;
			}))
		{
			Result.Connections.Add(Connection);
		}
	}
	TArray<uint8> RightEdges;
	TArray<uint8> UpEdges;
	RightEdges.Init(0, Count);
	UpEdges.Init(0, Count);
	for (const uint64 Key : SelectedEdges)
	{
		const int32 A = static_cast<int32>(Key >> 32);
		const int32 B = static_cast<int32>(Key & 0xffffffffu);
		if (B == A + 1 && A / SizeX == B / SizeX) { RightEdges[A] = 1; }
		else if (B == A + SizeX) { UpEdges[A] = 1; }
	}
	auto NodeCost = [&](int32 Node) -> int64
	{
		return Node == Root ? 0 : Costs[BestState[Node]];
	};
	auto EmitRun = [&](int32 BeginNode, int32 EndNode, int32 Increment,
		const FVector2D& PositiveDirection)
	{
		const FBox2D Merged(Squares[BeginNode].Min, Squares[EndNode].Max);
		if (IsClear(Merged))
		{
			Result.Segments.Add(Merged);
			Result.Directions.Add(NodeCost(EndNode) >= NodeCost(BeginNode)
				? PositiveDirection : -PositiveDirection);
			return;
		}
		for (int32 Node = BeginNode; Node != EndNode; Node += Increment)
		{
			const int32 Next = Node + Increment;
			const FBox2D Edge(Squares[Node].Min, Squares[Next].Max);
			if (IsClear(Edge))
			{
				Result.Segments.Add(Edge);
				Result.Directions.Add(NodeCost(Next) >= NodeCost(Node)
					? PositiveDirection : -PositiveDirection);
			}
		}
	};
	for (int32 Y = 0; Y < SizeY; ++Y)
	{
		for (int32 X = 0; X + 1 < SizeX;)
		{
			const int32 First = X + Y * SizeX;
			if (RightEdges[First] == 0) { ++X; continue; }
			const int32 Begin = X;
			while (X + 1 < SizeX && RightEdges[X + Y * SizeX] == 1) { ++X; }
			const int32 End = X + Y * SizeX;
			EmitRun(Begin + Y * SizeX, End, 1, FVector2D(1.0, 0.0));
		}
	}
	for (int32 X = 0; X < SizeX; ++X)
	{
		for (int32 Y = 0; Y + 1 < SizeY;)
		{
			const int32 First = X + Y * SizeX;
			if (UpEdges[First] == 0) { ++Y; continue; }
			const int32 Begin = Y;
			while (Y + 1 < SizeY && UpEdges[X + Y * SizeX] == 1) { ++Y; }
			const int32 End = X + Y * SizeX;
			EmitRun(X + Begin * SizeX, End, SizeX, FVector2D(0.0, 1.0));
		}
	}
	for (int32 Node = 0; Node < Count; ++Node)
	{
		const int32 X = Node % SizeX;
		const int32 Y = Node / SizeX;
		const bool bRight = RightEdges[Node] == 1;
		const bool bLeft = X > 0 && RightEdges[Node - 1] == 1;
		const bool bUp = UpEdges[Node] == 1;
		const bool bDown = Y > 0 && UpEdges[Node - SizeX] == 1;
		const int32 Degree = static_cast<int32>(bRight) + static_cast<int32>(bLeft)
			+ static_cast<int32>(bUp) + static_cast<int32>(bDown);
		if (Degree >= 3 || (Degree == 2 && (bRight || bLeft) && (bUp || bDown))
			|| (Node == Root && (bRight || bLeft || bUp || bDown)
				&& ((bInwardX && (bUp || bDown)) || (bInwardY && (bRight || bLeft)))))
		{
			Result.Junctions.Add(Squares[Node]);
		}
	}
	for (int32 Kind = 0; Kind < 3; ++Kind)
	{
		if (!bAcceptedGoal[Kind] || !bConnectedGoal[Kind]) { continue; }
		const int32 Node = Goals[Kind];
		const FVector2D ArrivalDirection = GoalStates[Kind] == INDEX_NONE
			? Forward : WorldDirections[GoalStates[Kind] % 4];
		if (ArrivalDirection != GoalConnections[Kind].Direction
			&& !Result.Junctions.ContainsByPredicate([&](const FBox2D& Existing)
				{
				return Existing.Min == Squares[Node].Min && Existing.Max == Squares[Node].Max;
				}))
		{
			Result.Junctions.Add(Squares[Node]);
		}
	}
	return Result;
}

FPlotWalkwayResult FPlotWalkwayRules::Solve(const FPlotWalkwayGrid& Grid,
	TConstArrayView<FPlotWalkwayTarget> Targets, TConstArrayView<FBox2D> FixedBlockers,
	bool bBuildPaths, const TFunction<bool(const FBox2D&)>& IsOccupied)
{
	FPlotWalkwayResult Result;
	const int64 Count64 = static_cast<int64>(Grid.Size.X) * Grid.Size.Y;
	if (Grid.Size.X <= 0 || Grid.Size.Y <= 0 || Count64 <= 0
		|| Count64 > PlotWalkwayPrivate::MaxCells || Grid.Ground.Num() != Count64
		|| Targets.Num() > PlotWalkwayPrivate::MaxTargets
		|| !FMath::IsFinite(Grid.CellSize) || Grid.CellSize <= 0.f
		|| !PlotWalkwayPrivate::IsFinitePoint(Grid.Origin))
	{
		return Result;
	}
	const double SpanX = static_cast<double>(Grid.Size.X) * Grid.CellSize;
	const double SpanY = static_cast<double>(Grid.Size.Y) * Grid.CellSize;
	const double MaxX = Grid.Origin.X + SpanX;
	const double MaxY = Grid.Origin.Y + SpanY;
	if (!FMath::IsFinite(MaxX) || !FMath::IsFinite(MaxY)
		|| FMath::Abs(Grid.Origin.X) > std::numeric_limits<float>::max() * 0.5
		|| FMath::Abs(Grid.Origin.Y) > std::numeric_limits<float>::max() * 0.5
		|| SpanX > std::numeric_limits<float>::max() * 0.5
		|| SpanY > std::numeric_limits<float>::max() * 0.5
		|| FMath::Abs(MaxX) > std::numeric_limits<float>::max() * 0.5
		|| FMath::Abs(MaxY) > std::numeric_limits<float>::max() * 0.5)
	{
		return Result;
	}
	const int32 Count = static_cast<int32>(Count64);
	for (const uint8 Ground : Grid.Ground)
	{
		if (Ground > 1)
		{
			return Result;
		}
	}
	for (const int32 Entrance : Grid.EntranceCells)
	{
		if (Entrance < 0 || Entrance >= Count || Grid.Ground[Entrance] != 1)
		{
			return Result;
		}
	}
	for (const TPair<int32, double>& Entry : Grid.EntranceDepth)
	{
		if (Entry.Key < 0 || Entry.Key >= Count || !FMath::IsFinite(Entry.Value)
			|| Entry.Value < 0.0)
		{
			return Result;
		}
	}
	TSet<int32> Ids;
	for (const FPlotWalkwayTarget& Target : Targets)
	{
		if (!PlotWalkwayPrivate::IsValidBox(Target.Bounds) || Ids.Contains(Target.Id))
		{
			return Result;
		}
		Ids.Add(Target.Id);
	}
	for (const FBox2D& Blocker : FixedBlockers)
	{
		if (!PlotWalkwayPrivate::IsValidBox(Blocker))
		{
			return Result;
		}
	}
	Result.bValid = true;
	if (Targets.IsEmpty() || Grid.EntranceCells.IsEmpty())
	{
		return Result;
	}

	TArray<uint8> Free;
	Free.Init(0, Count);
	TArray<FBox2D> Squares;
	Squares.Reserve(Count);
	for (int32 Cell = 0; Cell < Count; ++Cell)
	{
		const FBox2D Square = CellBounds(Grid, Cell);
		Squares.Add(Square);
		if (Grid.Ground[Cell] == 0)
		{
			continue;
		}
		bool bBlocked = false;
		if (IsOccupied)
		{
			bBlocked = IsOccupied(Square);
		}
		else
		{
			for (const FPlotWalkwayTarget& Target : Targets)
			{
				if (PlotWalkwayPrivate::Overlaps(Square, Target.Bounds))
				{
					bBlocked = true;
					break;
				}
			}
		}
		if (!bBlocked)
		{
			for (const FBox2D& Blocker : FixedBlockers)
			{
				if (PlotWalkwayPrivate::Overlaps(Square, Blocker))
				{
					bBlocked = true;
					break;
				}
			}
		}
		Free[Cell] = bBlocked ? 0 : 1;
	}

	TArray<int32> ComponentOf;
	ComponentOf.Init(INDEX_NONE, Count);
	TArray<TArray<int32>> Components;
	for (int32 Start = 0; Start < Count; ++Start)
	{
		if (Free[Start] == 0 || ComponentOf[Start] != INDEX_NONE)
		{
			continue;
		}
		const int32 Component = Components.Num();
		TArray<int32>& Cells = Components.AddDefaulted_GetRef();
		Cells.Add(Start);
		ComponentOf[Start] = Component;
		for (int32 Head = 0; Head < Cells.Num(); ++Head)
		{
			int32 Adjacent[4];
			PlotWalkwayPrivate::Neighbors(Grid, Cells[Head], Adjacent);
			for (const int32 Next : Adjacent)
			{
				if (Next != INDEX_NONE && Free[Next] == 1 && ComponentOf[Next] == INDEX_NONE)
				{
					ComponentOf[Next] = Component;
					Cells.Add(Next);
				}
			}
		}
	}
	TArray<TArray<int32>> Entrances;
	Entrances.SetNum(Components.Num());
	for (const int32 Entrance : Grid.EntranceCells)
	{
		if (ComponentOf[Entrance] != INDEX_NONE)
		{
			Entrances[ComponentOf[Entrance]].AddUnique(Entrance);
		}
	}
	for (TArray<int32>& Group : Entrances)
	{
		Group.Sort();
	}

	TArray<TArray<int32>> Candidates;
	Candidates.SetNum(Targets.Num());
	TArray<TArray<int32>> ComponentTargets;
	ComponentTargets.SetNum(Components.Num());
	for (int32 TargetIndex = 0; TargetIndex < Targets.Num(); ++TargetIndex)
	{
		for (int32 Cell = 0; Cell < Count; ++Cell)
		{
			const int32 Component = ComponentOf[Cell];
			if (Component == INDEX_NONE || Entrances[Component].IsEmpty())
			{
				continue;
			}
			FBox2D ConnectorBounds(ForceInit);
			if (PlotWalkwayPrivate::HasConnector(Grid, Squares[Cell], Targets[TargetIndex].Bounds,
				TargetIndex, Targets, FixedBlockers, IsOccupied, ConnectorBounds))
			{
				Candidates[TargetIndex].Add(Cell);
				ComponentTargets[Component].AddUnique(TargetIndex);
			}
		}
		if (!Candidates[TargetIndex].IsEmpty())
		{
			Result.ReachableTargetIds.Add(Targets[TargetIndex].Id);
		}
	}
	if (!bBuildPaths || Result.ReachableTargetIds.IsEmpty())
	{
		return Result;
	}

	TArray<int32> ChosenEntrance;
	ChosenEntrance.Init(INDEX_NONE, Components.Num());
	TArray<PlotWalkwayPrivate::FRouteTree> Trees;
	Trees.SetNum(Components.Num());
	for (int32 Component = 0; Component < Components.Num(); ++Component)
	{
		if (ComponentTargets[Component].IsEmpty())
		{
			continue;
		}
		ComponentTargets[Component].Sort([&](int32 Left, int32 Right)
		{
			return Targets[Left].Id < Targets[Right].Id;
		});
		double MeanX = 0.0;
		double MeanY = 0.0;
		for (const int32 TargetIndex : ComponentTargets[Component])
		{
			const FVector2D Center = Targets[TargetIndex].Bounds.GetCenter();
			MeanX += Center.X;
			MeanY += Center.Y;
		}
		MeanX /= ComponentTargets[Component].Num();
		MeanY /= ComponentTargets[Component].Num();
		double BestDistance = std::numeric_limits<double>::max();
		double BestDepth = -std::numeric_limits<double>::max();
		for (const int32 Entrance : Entrances[Component])
		{
			const double Depth = Grid.EntranceDepth.FindRef(Entrance);
			const FVector2D Center = CellCenter(Grid, Entrance);
			const double Dx = static_cast<double>(Center.X) - MeanX;
			const double Dy = static_cast<double>(Center.Y) - MeanY;
			const double Distance = Dx * Dx + Dy * Dy;
			if (Depth > BestDepth || (Depth == BestDepth && Distance < BestDistance))
			{
				BestDepth = Depth;
				BestDistance = Distance;
				ChosenEntrance[Component] = Entrance;
			}
		}
		Trees[Component] = PlotWalkwayPrivate::BuildRouteTree(Grid, Free, ChosenEntrance[Component]);
	}

	TSet<int32> Path;
	TSet<int32> UsedEntrances;
	for (int32 TargetIndex = 0; TargetIndex < Targets.Num(); ++TargetIndex)
	{
		int32 WinnerCell = INDEX_NONE;
		int32 WinnerComponent = INDEX_NONE;
		int32 WinnerState = INDEX_NONE;
		int32 WinnerDistance = PlotWalkwayPrivate::Unreached;
		int32 WinnerTurns = PlotWalkwayPrivate::Unreached;
		for (const int32 Cell : Candidates[TargetIndex])
		{
			const int32 Component = ComponentOf[Cell];
			const int32 Entrance = ChosenEntrance[Component];
			if (Entrance == INDEX_NONE)
			{
				continue;
			}
			const PlotWalkwayPrivate::FRouteTree& Tree = Trees[Component];
			const int32 State = PlotWalkwayPrivate::BestState(Tree, Cell, Entrance);
			const int32 Distance = Tree.Distance[Cell];
			const int32 Turns = Cell == Entrance ? 0
				: State == INDEX_NONE ? PlotWalkwayPrivate::Unreached : Tree.Turns[State];
			if (Distance < WinnerDistance
				|| (Distance == WinnerDistance && Turns < WinnerTurns)
				|| (Distance == WinnerDistance && Turns == WinnerTurns
					&& (WinnerComponent == INDEX_NONE || Entrance < ChosenEntrance[WinnerComponent]))
				|| (Distance == WinnerDistance && Turns == WinnerTurns
					&& WinnerComponent != INDEX_NONE && Entrance == ChosenEntrance[WinnerComponent]
					&& Cell < WinnerCell))
			{
				WinnerCell = Cell;
				WinnerComponent = Component;
				WinnerState = State;
				WinnerDistance = Distance;
				WinnerTurns = Turns;
			}
		}
		if (WinnerComponent == INDEX_NONE)
		{
			continue;
		}
		UsedEntrances.Add(ChosenEntrance[WinnerComponent]);
		FBox2D ConnectorBounds(ForceInit);
		if (PlotWalkwayPrivate::HasConnector(Grid, Squares[WinnerCell], Targets[TargetIndex].Bounds,
			TargetIndex, Targets, FixedBlockers, IsOccupied, ConnectorBounds) && ConnectorBounds.bIsValid)
		{
			FPlotWalkwayConnector Connector;
			Connector.TargetId = Targets[TargetIndex].Id;
			Connector.Cell = WinnerCell;
			Connector.Bounds = ConnectorBounds;
			Result.Connectors.Add(Connector);
		}
		Path.Add(ChosenEntrance[WinnerComponent]);
		for (int32 State = WinnerState; State != INDEX_NONE; State = Trees[WinnerComponent].Parent[State])
		{
			Path.Add(State / 4);
		}
	}
	for (const int32 Cell : Path)
	{
		Result.PathCells.Add(Cell);
	}
	Result.PathCells.Sort();
	for (const int32 Entrance : UsedEntrances)
	{
		Result.UsedEntranceCells.Add(Entrance);
	}
	Result.UsedEntranceCells.Sort();
	Result.Connectors.Sort([](const FPlotWalkwayConnector& Left, const FPlotWalkwayConnector& Right)
	{
		return Left.TargetId < Right.TargetId;
	});
	return Result;
}
