// PARASITE - unit tests for the match simulation.
//
// These run without Unreal: see Tests/run_tests.sh. They cover the gameplay
// rules that a play test would otherwise have to catch by hand - possession
// limits, cooldowns, hijacking, nest infection, DNA, win conditions, joins and
// disconnects.
#include "../Source/Parasite/Core/MatchSim.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace Parasite;

static int Failures = 0;
static int Checks = 0;
static const char* CurrentTest = "";

static void Check(bool Condition, const char* Expression, int Line)
{
	++Checks;
	if (!Condition)
	{
		++Failures;
		std::printf("  FAIL %s:%d  %s\n", CurrentTest, Line, Expression);
	}
}

static void CheckNear(float Actual, float Expected, float Tolerance, const char* Expression, int Line)
{
	++Checks;
	if (std::fabs(Actual - Expected) > Tolerance)
	{
		++Failures;
		std::printf("  FAIL %s:%d  %s (got %.3f, wanted %.3f)\n", CurrentTest, Line, Expression, Actual, Expected);
	}
}

#define CHECK(expr)					Check((expr), #expr, __LINE__)
#define CHECK_NEAR(a, b, tol)		CheckNear((a), (b), (tol), #a " ~= " #b, __LINE__)
#define TEST(name)					CurrentTest = name; std::printf("- %s\n", name);

/** Advances the simulation in small steps so timers behave as they do in game. */
static void Advance(FMatchSim& Sim, float Seconds, float Step = 0.1f)
{
	for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += Step)
	{
		Sim.Tick(Step);
	}
}

/** A match already in progress with two players, one per team. */
static FMatchSim MakeRunningMatch(PlayerId& OutA, PlayerId& OutB, FRules Rules = FRules())
{
	FMatchSim Sim(Rules);
	OutA = Sim.AddPlayer("alpha");
	OutB = Sim.AddPlayer("bravo");
	Sim.SetNestLocation(ETeam::A, FVec3(-8000.f, 0.f, 0.f));
	Sim.SetNestLocation(ETeam::B, FVec3(8000.f, 0.f, 0.f));
	Sim.SetPlayerLocation(OutA, FVec3(-5000.f, 0.f, 0.f));
	Sim.SetPlayerLocation(OutB, FVec3(5000.f, 0.f, 0.f));
	Advance(Sim, Rules.CountdownDuration + 0.5f);
	return Sim;
}

// ---------------------------------------------------------------------------

static void TestTeamAssignment()
{
	TEST("team assignment balances and caps at ten players");
	FMatchSim Sim;
	std::vector<PlayerId> Joined;
	for (int Index = 0; Index < 10; ++Index)
	{
		const PlayerId Player = Sim.AddPlayer("player" + std::to_string(Index));
		CHECK(Player != InvalidPlayer);
		Joined.push_back(Player);
		// Teams never differ by more than one, at any point during the join order.
		const int Difference = Sim.GetTeamPlayerCount(ETeam::A) - Sim.GetTeamPlayerCount(ETeam::B);
		CHECK(Difference >= 0 && Difference <= 1);
	}
	CHECK(Sim.GetTeamPlayerCount(ETeam::A) == 5);
	CHECK(Sim.GetTeamPlayerCount(ETeam::B) == 5);

	// The eleventh player is refused: 5v5 is the ceiling.
	CHECK(Sim.AddPlayer("eleventh") == InvalidPlayer);

	// A leaver frees their slot and the next join rebalances onto the short team.
	Sim.RemovePlayer(Joined[0]);
	CHECK(Sim.GetTeamPlayerCount(ETeam::A) == 4);
	const PlayerId Replacement = Sim.AddPlayer("late");
	CHECK(Replacement != InvalidPlayer);
	CHECK(Sim.FindPlayer(Replacement)->Team == ETeam::A);
}

static void TestMatchFlow()
{
	TEST("match flows lobby -> countdown -> in progress -> post match -> rematch");
	FRules Rules;
	Rules.MatchDuration = 10.f;
	Rules.PostMatchDuration = 2.f;
	FMatchSim Sim(Rules);

	CHECK(Sim.GetPhase() == EMatchPhase::Lobby);
	Sim.Tick(0.1f);
	CHECK(Sim.GetPhase() == EMatchPhase::Lobby);		// nobody has joined yet

	Sim.AddPlayer("alpha");
	Sim.Tick(0.1f);
	CHECK(Sim.GetPhase() == EMatchPhase::Countdown);
	CHECK_NEAR(Sim.GetPhaseTimeRemaining(), Rules.CountdownDuration, 0.2f);

	Advance(Sim, Rules.CountdownDuration + 0.2f);
	CHECK(Sim.GetPhase() == EMatchPhase::InProgress);
	CHECK_NEAR(Sim.GetPhaseTimeRemaining(), Rules.MatchDuration, 0.3f);

	Advance(Sim, Rules.MatchDuration + 0.5f);
	CHECK(Sim.GetPhase() == EMatchPhase::PostMatch);
	CHECK(!Sim.GetResultReason().empty());

	Advance(Sim, Rules.PostMatchDuration + 0.5f);
	CHECK(Sim.GetPhase() == EMatchPhase::Countdown);		// rematch, automatically
}

static void TestPossessionBasics()
{
	TEST("possessing a prop takes the host, awards DNA and expires on time");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);

	const HostId Chair = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "CHAIR", FVec3(-5100.f, 0.f, 0.f));
	CHECK(Sim.TryPossess(Alpha, Chair) == EPossessResult::Success);
	CHECK(Sim.FindPlayer(Alpha)->CurrentHost == Chair);
	CHECK(Sim.FindHost(Chair)->bPossessed);
	CHECK(Sim.FindHost(Chair)->Rider == Alpha);
	CHECK(Sim.FindPlayer(Alpha)->DNA == Sim.GetRules().DNA_PossessProp);

	// Cannot enter a second host while riding one.
	const HostId Bin = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "TRASH BIN", FVec3(-5100.f, 50.f, 0.f));
	CHECK(Sim.TryPossess(Alpha, Bin) == EPossessResult::AlreadyPossessing);

	// The 30 second prop limit is enforced by the simulation, not the client.
	Advance(Sim, Sim.GetRules().PropDuration + 0.5f);
	CHECK(Sim.FindPlayer(Alpha)->CurrentHost == InvalidHost);
	CHECK(!Sim.FindHost(Chair)->bPossessed);
}

static void TestPossessionLimits()
{
	TEST("possession respects range, cooldown, occupancy and host type durations");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);
	const FRules& Rules = Sim.GetRules();

	// Out of range: 3 m is 300 uu.
	const HostId Far = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "PLANT", FVec3(-5000.f + 400.f, 0.f, 0.f));
	CHECK(Sim.TryPossess(Alpha, Far) == EPossessResult::OutOfRange);

	// Jumper extends the reach to cover it.
	const HostId Near = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "BENCH", FVec3(-5100.f, 0.f, 0.f));
	CHECK(Sim.TryPossess(Alpha, Near) == EPossessResult::Success);
	CHECK(Sim.ExitPossession(Alpha, false));

	// Exiting starts a three second cooldown.
	CHECK(Sim.TryPossess(Alpha, Near) == EPossessResult::OnCooldown);
	Advance(Sim, Rules.PossessCooldown + 0.2f);
	CHECK(Sim.TryPossess(Alpha, Near) == EPossessResult::Success);

	// A host with somebody already inside is not available.
	Sim.SetPlayerLocation(Bravo, FVec3(-5100.f, 0.f, 0.f));
	CHECK(Sim.TryPossess(Bravo, Near) == EPossessResult::HostOccupied);

	// NPCs last longer than props, enemies shortest of all.
	CHECK_NEAR(Sim.GetHostDuration(EHostType::Prop, Alpha), Rules.PropDuration, 0.001f);
	CHECK_NEAR(Sim.GetHostDuration(EHostType::NPC, Alpha), Rules.NPCDuration, 0.001f);
	CHECK_NEAR(Sim.GetHostDuration(EHostType::Player, Alpha), Rules.PlayerDuration, 0.001f);
}

static void TestNPCPossession()
{
	TEST("NPC possession runs 45 seconds and returns the NPC when it ends");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);

	const HostId Shopper = Sim.AddHost(EHostType::NPC, EHostMobility::Walk, "NPC", FVec3(-5050.f, 0.f, 0.f));
	CHECK(Sim.TryPossess(Alpha, Shopper) == EPossessResult::Success);
	CHECK(Sim.FindPlayer(Alpha)->DNA == Sim.GetRules().DNA_PossessNPC);

	// Still inside at 44 seconds, out at 46.
	Advance(Sim, 44.f);
	CHECK(Sim.FindPlayer(Alpha)->CurrentHost == Shopper);
	Advance(Sim, 2.f);
	CHECK(Sim.FindPlayer(Alpha)->CurrentHost == InvalidHost);
	CHECK(!Sim.FindHost(Shopper)->bPossessed);
}

static void TestEnemyPossession()
{
	TEST("enemy possession lasts eight seconds, never permanently");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);
	const FRules& Rules = Sim.GetRules();

	// Move the two parasites next to each other.
	Sim.SetPlayerLocation(Bravo, FVec3(-5100.f, 0.f, 0.f));
	const HostId BravoBody = Sim.FindPlayer(Bravo)->BodyHost;

	CHECK(Sim.TryPossess(Alpha, BravoBody) == EPossessResult::Success);
	CHECK(Sim.FindHost(BravoBody)->Rider == Alpha);
	CHECK(Sim.FindPlayer(Alpha)->DNA == Rules.DNA_PossessEnemy);

	// The victim keeps their own identity: they still exist, still on their team,
	// still holding their own DNA. Only their body is being driven.
	CHECK(Sim.FindPlayer(Bravo)->Team == ETeam::B);
	CHECK(Sim.FindPlayer(Bravo)->CurrentHost == InvalidHost);

	// Eight seconds, then ejected automatically.
	Advance(Sim, Rules.PlayerDuration - 1.f);
	CHECK(Sim.FindHost(BravoBody)->bPossessed);
	Advance(Sim, 2.f);
	CHECK(!Sim.FindHost(BravoBody)->bPossessed);
	CHECK(Sim.FindPlayer(Alpha)->CurrentHost == InvalidHost);
}

static void TestEnemyPossessionRules()
{
	TEST("a hijacked parasite cannot act, and team mates cannot be ridden");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);
	const PlayerId Charlie = Sim.AddPlayer("charlie");		// joins team A, with alpha
	CHECK(Sim.FindPlayer(Charlie)->Team == ETeam::A);

	Sim.SetPlayerLocation(Charlie, FVec3(-5000.f, 0.f, 0.f));
	CHECK(Sim.TryPossess(Alpha, Sim.FindPlayer(Charlie)->BodyHost) == EPossessResult::HostIsFriendly);
	CHECK(Sim.TryPossess(Alpha, Sim.FindPlayer(Alpha)->BodyHost) == EPossessResult::SelfPossession);

	// While being ridden, the victim cannot possess or leap.
	Sim.SetPlayerLocation(Bravo, FVec3(-5100.f, 0.f, 0.f));
	CHECK(Sim.TryPossess(Alpha, Sim.FindPlayer(Bravo)->BodyHost) == EPossessResult::Success);
	const HostId Chair = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "CHAIR", FVec3(-5100.f, 20.f, 0.f));
	CHECK(Sim.TryPossess(Bravo, Chair) == EPossessResult::Hijacked);
	CHECK(!Sim.TryLeap(Bravo));
}

static void TestResist()
{
	TEST("mashing resist throws the parasite out early and pays the victim");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);

	Sim.SetPlayerLocation(Bravo, FVec3(-5100.f, 0.f, 0.f));
	const HostId BravoBody = Sim.FindPlayer(Bravo)->BodyHost;
	CHECK(Sim.TryPossess(Alpha, BravoBody) == EPossessResult::Success);

	// Resisting fills a meter; one press is not enough.
	CHECK(!Sim.AddResist(Bravo));
	CHECK(Sim.FindHost(BravoBody)->ResistProgress > 0.f);
	CHECK(Sim.FindHost(BravoBody)->bPossessed);

	bool bExpelled = false;
	for (int Press = 0; Press < 20 && !bExpelled; ++Press)
	{
		bExpelled = Sim.AddResist(Bravo);
	}
	CHECK(bExpelled);
	CHECK(!Sim.FindHost(BravoBody)->bPossessed);
	CHECK(Sim.FindPlayer(Alpha)->CurrentHost == InvalidHost);
	CHECK(Sim.FindPlayer(Bravo)->DNA == Sim.GetRules().DNA_ExpelParasite);

	// Resisting when nobody is inside you does nothing.
	CHECK(!Sim.AddResist(Bravo));
}

static void TestScan()
{
	TEST("scan reveals enemy hosts only, respects range, cooldown and Infiltrator");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);
	const FRules& Rules = Sim.GetRules();

	// Bravo hides in a bin right next to alpha; a team mate hides further away.
	const HostId EnemyBin = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "TRASH BIN", FVec3(-5000.f, 100.f, 0.f));
	Sim.SetPlayerLocation(Bravo, FVec3(-5000.f, 150.f, 0.f));
	CHECK(Sim.TryPossess(Bravo, EnemyBin) == EPossessResult::Success);

	const PlayerId Charlie = Sim.AddPlayer("charlie");		// team A, same as alpha
	const HostId FriendlyCart = Sim.AddHost(EHostType::Prop, EHostMobility::Wheeled, "CART", FVec3(-5000.f, 200.f, 0.f));
	Sim.SetPlayerLocation(Charlie, FVec3(-5000.f, 220.f, 0.f));
	CHECK(Sim.TryPossess(Charlie, FriendlyCart) == EPossessResult::Success);

	std::vector<HostId> Revealed;
	CHECK(Sim.TryScan(Alpha, Revealed));
	CHECK(Revealed.size() == 1);
	CHECK(Revealed[0] == EnemyBin);			// the team mate is not exposed
	CHECK(Sim.GetRevealEndTime(EnemyBin) > Sim.GetTime());

	// Twenty second cooldown.
	CHECK(!Sim.TryScan(Alpha, Revealed));
	Advance(Sim, Rules.ScanCooldown + 0.5f);
	CHECK(Sim.TryScan(Alpha, Revealed));

	// A host beyond the scan radius stays hidden: no map-wide reveal.
	Sim.SetHostLocation(EnemyBin, FVec3(-5000.f + Rules.ScanRadius + 500.f, 0.f, 0.f));
	Advance(Sim, Rules.ScanCooldown + 0.5f);
	CHECK(Sim.TryScan(Alpha, Revealed));
	CHECK(Revealed.empty());
}

static void TestInfiltratorUpgrade()
{
	TEST("Infiltrator shrinks the radius a scan can catch you in");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);
	const FRules& Rules = Sim.GetRules();

	// Bravo buys Infiltrator, then hides at 60% of the normal scan radius:
	// inside a plain scan, outside an Infiltrator-scaled one.
	const HostId Bin = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "TRASH BIN", FVec3(5000.f, 100.f, 0.f));
	CHECK(Sim.TryPossess(Bravo, Bin) == EPossessResult::Success);
	CHECK(Sim.ExitPossession(Bravo, false));

	std::vector<HostId> Revealed;
	const float HidingDistance = Rules.ScanRadius * 0.6f;
	Sim.SetHostLocation(Bin, FVec3(-5000.f + HidingDistance, 0.f, 0.f));
	Sim.SetPlayerLocation(Bravo, FVec3(-5000.f + HidingDistance, 0.f, 0.f));
	Advance(Sim, Rules.PossessCooldown + 0.2f);
	CHECK(Sim.TryPossess(Bravo, Bin) == EPossessResult::Success);

	CHECK(Sim.TryScan(Alpha, Revealed));
	CHECK(Revealed.size() == 1);			// caught without the upgrade

	// Give bravo enough DNA to evolve, then scan again.
	while (Sim.FindPlayer(Bravo)->DNA < Rules.UpgradeCost)
	{
		Sim.ExitPossession(Bravo, false);
		Advance(Sim, Rules.PossessCooldown + 0.2f);
		CHECK(Sim.TryPossess(Bravo, Bin) == EPossessResult::Success);
	}
	CHECK(Sim.TryPurchaseUpgrade(Bravo, EUpgrade::Infiltrator));
	CHECK_NEAR(Sim.GetStealthScale(Bravo), Rules.InfiltratorScanScale, 0.001f);

	Advance(Sim, Rules.ScanCooldown + 0.5f);
	CHECK(Sim.TryScan(Alpha, Revealed));
	CHECK(Revealed.empty());				// now hidden at the same distance
}

static void TestUpgrades()
{
	TEST("evolutions cost DNA, cap at two, and change the numbers they promise");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);
	const FRules& Rules = Sim.GetRules();

	CHECK(!Sim.TryPurchaseUpgrade(Alpha, EUpgrade::Jumper));		// no DNA yet

	// Farm DNA by hopping in and out of a chair.
	const HostId Chair = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "CHAIR", FVec3(-5100.f, 0.f, 0.f));
	while (Sim.FindPlayer(Alpha)->DNA < Rules.UpgradeCost * 3)
	{
		CHECK(Sim.TryPossess(Alpha, Chair) == EPossessResult::Success);
		Sim.ExitPossession(Alpha, false);
		Advance(Sim, Rules.PossessCooldown + 0.2f);
	}

	const float BaseRange = Sim.GetPossessRange(Alpha);
	CHECK(Sim.TryPurchaseUpgrade(Alpha, EUpgrade::Jumper));
	CHECK_NEAR(Sim.GetPossessRange(Alpha), BaseRange + Rules.JumperRangeBonus, 0.001f);
	CHECK(!Sim.TryPurchaseUpgrade(Alpha, EUpgrade::Jumper));		// no duplicates

	CHECK(Sim.TryPurchaseUpgrade(Alpha, EUpgrade::Mimic));
	CHECK_NEAR(Sim.GetHostDuration(EHostType::Player, Alpha), Rules.PlayerDuration + Rules.MimicDurationBonus, 0.001f);

	// Two is the cap, however much DNA is left.
	CHECK(Sim.FindPlayer(Alpha)->DNA >= Rules.UpgradeCost);
	CHECK(!Sim.TryPurchaseUpgrade(Alpha, EUpgrade::Infiltrator));
	CHECK(Sim.FindPlayer(Alpha)->Upgrades.size() == 2);
}

static void TestNestInfection()
{
	TEST("a nest takes twenty uninterrupted seconds, and progress stops when you leave");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);
	const FRules& Rules = Sim.GetRules();

	// Alpha (team A) walks into team B's nest.
	Sim.SetPlayerLocation(Alpha, FVec3(8000.f, 0.f, 0.f));
	Advance(Sim, 10.f);
	CHECK_NEAR(Sim.GetInfectionByTeam(ETeam::A), 50.f, 4.f);

	// Stepping out stops progress without losing it.
	Sim.SetPlayerLocation(Alpha, FVec3(0.f, 0.f, 0.f));
	const float Held = Sim.GetInfectionByTeam(ETeam::A);
	Advance(Sim, 5.f);
	CHECK_NEAR(Sim.GetInfectionByTeam(ETeam::A), Held, 0.01f);
	CHECK(!Sim.GetNest(ETeam::B).bUnderAttack);

	// Defenders sitting in their own nest do nothing to it.
	Sim.SetPlayerLocation(Bravo, FVec3(8000.f, 0.f, 0.f));
	Advance(Sim, 5.f);
	CHECK_NEAR(Sim.GetInfectionByTeam(ETeam::A), Held, 0.01f);
	CHECK_NEAR(Sim.GetInfectionByTeam(ETeam::B), 0.f, 0.01f);

	// Back in to finish the job.
	Sim.SetPlayerLocation(Alpha, FVec3(8000.f, 0.f, 0.f));
	Advance(Sim, Rules.NestInfectSeconds);
	CHECK_NEAR(Sim.GetInfectionByTeam(ETeam::A), 100.f, 0.01f);
}

static void TestNestExpulsionAndPulse()
{
	TEST("expelling an intruder claws progress back, and the pulse cuts their stay short");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);
	const FRules& Rules = Sim.GetRules();

	// Alpha rides an NPC into team B's nest; bravo is defending it.
	const HostId Shopper = Sim.AddHost(EHostType::NPC, EHostMobility::Walk, "NPC", FVec3(-5050.f, 0.f, 0.f));
	CHECK(Sim.TryPossess(Alpha, Shopper) == EPossessResult::Success);
	Sim.SetHostLocation(Shopper, FVec3(8000.f, 0.f, 0.f));

	// The nest pulse should have clamped the 45 second NPC stay down to ~5.
	Advance(Sim, Rules.NestPulseInterval + 0.2f);
	CHECK(Sim.FindPlayer(Alpha)->CurrentHost == Shopper);
	CHECK(Sim.FindHost(Shopper)->PossessionEndTime <= Sim.GetTime() + Rules.NestPulseGrace + 0.5f);

	const float Before = Sim.GetInfectionByTeam(ETeam::A);
	CHECK(Before > 10.f);

	// Being expelled inside the nest refunds progress to the defenders.
	Sim.SetPlayerLocation(Alpha, FVec3(8000.f, 0.f, 0.f));
	CHECK(Sim.ExitPossession(Alpha, true));
	CHECK_NEAR(Sim.GetInfectionByTeam(ETeam::A), std::max(0.f, Before - Rules.NestExpelPenalty), 0.5f);

	// Leaving a host far from any nest costs nothing.
	Sim.SetPlayerLocation(Alpha, FVec3(0.f, 0.f, 0.f));
	Advance(Sim, Rules.PossessCooldown + 0.2f);
	const HostId Chair = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "CHAIR", FVec3(0.f, 0.f, 0.f));
	CHECK(Sim.TryPossess(Alpha, Chair) == EPossessResult::Success);
	const float Untouched = Sim.GetInfectionByTeam(ETeam::A);
	CHECK(Sim.ExitPossession(Alpha, true));
	CHECK_NEAR(Sim.GetInfectionByTeam(ETeam::A), Untouched, 0.01f);
}

static void TestInfiltrationBonus()
{
	TEST("infiltration pays once, and only while wearing a disguise");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);
	Sim.SetRestrictedZone(ETeam::A, FVec3(6000.f, -10000.f, -500.f), FVec3(10000.f, -4000.f, 2000.f));

	// Walking in as a bare parasite earns nothing.
	Sim.SetPlayerLocation(Alpha, FVec3(8000.f, -6000.f, 0.f));
	Advance(Sim, 1.f);
	CHECK(Sim.FindPlayer(Alpha)->DNA == 0);

	// In a disguise it pays, once.
	const HostId Car = Sim.AddHost(EHostType::Vehicle, EHostMobility::Wheeled, "CAR", FVec3(8000.f, -6000.f, 0.f));
	CHECK(Sim.TryPossess(Alpha, Car) == EPossessResult::Success);
	const int32_t AfterPossess = Sim.FindPlayer(Alpha)->DNA;
	Advance(Sim, 1.f);
	CHECK(Sim.FindPlayer(Alpha)->DNA == AfterPossess + Sim.GetRules().DNA_Infiltrate);

	const int32_t AfterBonus = Sim.FindPlayer(Alpha)->DNA;
	Advance(Sim, 3.f);
	CHECK(Sim.FindPlayer(Alpha)->DNA == AfterBonus);		// never paid twice
}

static void TestWinByInfection()
{
	TEST("a fully infected nest ends the match immediately");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);

	Sim.SetPlayerLocation(Alpha, FVec3(8000.f, 0.f, 0.f));
	Advance(Sim, Sim.GetRules().NestInfectSeconds + 1.f);

	CHECK(Sim.GetPhase() == EMatchPhase::PostMatch);
	CHECK(Sim.GetWinner() == ETeam::A);
	CHECK(Sim.GetResultReason() == "TEAM B NEST FULLY INFECTED");
}

static void TestWinOnTime()
{
	TEST("time up is decided on infection, then on team DNA");
	FRules Rules;
	Rules.MatchDuration = 6.f;
	PlayerId Alpha, Bravo;

	{
		// Team B gets a little way into team A's nest; team A gets nowhere.
		FMatchSim Sim = MakeRunningMatch(Alpha, Bravo, Rules);
		Sim.SetPlayerLocation(Bravo, FVec3(-8000.f, 0.f, 0.f));
		Advance(Sim, Rules.MatchDuration + 0.5f);
		CHECK(Sim.GetPhase() == EMatchPhase::PostMatch);
		CHECK(Sim.GetWinner() == ETeam::B);
		CHECK(Sim.GetResultReason() == "TIME UP - HIGHEST INFECTION");
	}
	{
		// Neither team touches a nest, so DNA breaks the tie.
		FMatchSim Sim = MakeRunningMatch(Alpha, Bravo, Rules);
		const HostId Chair = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "CHAIR", FVec3(-5100.f, 0.f, 0.f));
		CHECK(Sim.TryPossess(Alpha, Chair) == EPossessResult::Success);
		Advance(Sim, Rules.MatchDuration + 0.5f);
		CHECK(Sim.GetWinner() == ETeam::A);
		CHECK(Sim.GetResultReason() == "TIME UP - DNA TIE BREAK");
		CHECK(Sim.GetTeamDNA(ETeam::A) > Sim.GetTeamDNA(ETeam::B));
	}
	{
		// Dead level on both counts.
		FMatchSim Sim = MakeRunningMatch(Alpha, Bravo, Rules);
		Advance(Sim, Rules.MatchDuration + 0.5f);
		CHECK(Sim.GetWinner() == ETeam::None);
		CHECK(Sim.GetResultReason() == "TIME UP - DEAD HEAT");
	}
}

static void TestMatchEndReleasesHosts()
{
	TEST("the end of a match empties every host");
	FRules Rules;
	Rules.MatchDuration = 5.f;
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo, Rules);

	const HostId Chair = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "CHAIR", FVec3(-5100.f, 0.f, 0.f));
	CHECK(Sim.TryPossess(Alpha, Chair) == EPossessResult::Success);

	Advance(Sim, Rules.MatchDuration + 0.5f);
	CHECK(Sim.GetPhase() == EMatchPhase::PostMatch);
	CHECK(!Sim.FindHost(Chair)->bPossessed);
	CHECK(Sim.FindPlayer(Alpha)->CurrentHost == InvalidHost);
}

static void TestRematchResets()
{
	TEST("a rematch clears DNA, upgrades, hosts and infection");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);

	const HostId Chair = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "CHAIR", FVec3(-5100.f, 0.f, 0.f));
	CHECK(Sim.TryPossess(Alpha, Chair) == EPossessResult::Success);
	Sim.SetPlayerLocation(Bravo, FVec3(-8000.f, 0.f, 0.f));
	Advance(Sim, 5.f);
	CHECK(Sim.GetInfectionByTeam(ETeam::B) > 0.f);

	Sim.RestartMatch();
	// Walk the attacker back out, or he simply carries on infecting into the
	// next match - which is correct, but not what this test is about.
	Sim.SetPlayerLocation(Bravo, FVec3(0.f, 0.f, 0.f));
	CHECK(Sim.GetPhase() == EMatchPhase::Countdown);
	CHECK(Sim.FindPlayer(Alpha)->DNA == 0);
	CHECK(Sim.FindPlayer(Alpha)->LifetimeDNA == 0);
	CHECK(Sim.FindPlayer(Alpha)->Upgrades.empty());
	CHECK(Sim.FindPlayer(Alpha)->CurrentHost == InvalidHost);
	CHECK(!Sim.FindHost(Chair)->bPossessed);

	Advance(Sim, Sim.GetRules().CountdownDuration + 0.5f);
	CHECK(Sim.GetPhase() == EMatchPhase::InProgress);
	CHECK_NEAR(Sim.GetInfectionByTeam(ETeam::B), 0.f, 0.01f);
}

static void TestDisconnect()
{
	TEST("a disconnect frees the host, and frees the victim of a hijack");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);

	// A leaver riding a chair must not leave it locked for the rest of the match.
	const HostId Chair = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "CHAIR", FVec3(-5100.f, 0.f, 0.f));
	CHECK(Sim.TryPossess(Alpha, Chair) == EPossessResult::Success);
	Sim.RemovePlayer(Alpha);
	CHECK(Sim.FindPlayer(Alpha) == nullptr);
	CHECK(!Sim.FindHost(Chair)->bPossessed);

	// A leaver being ridden must release their attacker.
	const PlayerId Charlie = Sim.AddPlayer("charlie");
	Sim.SetPlayerLocation(Charlie, FVec3(5000.f, 0.f, 0.f));
	Sim.SetPlayerLocation(Bravo, FVec3(5050.f, 0.f, 0.f));
	const PlayerId Attacker = (Sim.FindPlayer(Charlie)->Team == Sim.FindPlayer(Bravo)->Team) ? InvalidPlayer : Charlie;
	CHECK(Attacker != InvalidPlayer);
	CHECK(Sim.TryPossess(Attacker, Sim.FindPlayer(Bravo)->BodyHost) == EPossessResult::Success);
	Sim.RemovePlayer(Bravo);
	CHECK(Sim.FindPlayer(Attacker)->CurrentHost == InvalidHost);
}

static void TestHostDestroyed()
{
	TEST("destroying a host underneath a rider spits them out");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);

	const HostId Cart = Sim.AddHost(EHostType::Prop, EHostMobility::Wheeled, "SHOPPING CART", FVec3(-5100.f, 0.f, 0.f));
	CHECK(Sim.TryPossess(Alpha, Cart) == EPossessResult::Success);
	Sim.RemoveHost(Cart);
	CHECK(Sim.FindPlayer(Alpha)->CurrentHost == InvalidHost);
	CHECK(Sim.FindHost(Cart) == nullptr);
	CHECK(Sim.TryPossess(Alpha, Cart) == EPossessResult::OnCooldown);
}

static void TestLateJoin()
{
	TEST("a player joining mid match is teamed and can play at once");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);
	Advance(Sim, 60.f);
	CHECK(Sim.GetPhase() == EMatchPhase::InProgress);

	const PlayerId Late = Sim.AddPlayer("late");
	CHECK(Late != InvalidPlayer);
	CHECK(Sim.FindPlayer(Late)->Team != ETeam::None);

	Sim.SetPlayerLocation(Late, FVec3(0.f, 0.f, 0.f));
	const HostId Bin = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "TRASH BIN", FVec3(100.f, 0.f, 0.f));
	CHECK(Sim.TryPossess(Late, Bin) == EPossessResult::Success);
}

static void TestPossessNearest()
{
	TEST("the possess key picks the closest legal host");
	PlayerId Alpha, Bravo;
	FMatchSim Sim = MakeRunningMatch(Alpha, Bravo);

	Sim.SetPlayerLocation(Alpha, FVec3(0.f, 0.f, 0.f));
	const HostId Far = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "PLANT", FVec3(250.f, 0.f, 0.f));
	const HostId Close = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "CHAIR", FVec3(80.f, 0.f, 0.f));

	HostId Chosen = InvalidHost;
	CHECK(Sim.TryPossessNearest(Alpha, Chosen) == EPossessResult::Success);
	CHECK(Chosen == Close);
	CHECK(Far != Close);

	// With nothing in reach it reports honestly rather than reaching across the map.
	Sim.ExitPossession(Alpha, false);
	Advance(Sim, Sim.GetRules().PossessCooldown + 0.2f);
	Sim.SetPlayerLocation(Alpha, FVec3(90000.f, 0.f, 0.f));
	CHECK(Sim.TryPossessNearest(Alpha, Chosen) == EPossessResult::NoSuchHost);
}

static void TestPhaseGating()
{
	TEST("nothing can be possessed before the match starts");
	FMatchSim Sim;
	const PlayerId Alpha = Sim.AddPlayer("alpha");
	const HostId Chair = Sim.AddHost(EHostType::Prop, EHostMobility::Slide, "CHAIR", FVec3(0.f, 0.f, 0.f));
	Sim.SetPlayerLocation(Alpha, FVec3(0.f, 0.f, 0.f));

	CHECK(Sim.TryPossess(Alpha, Chair) == EPossessResult::WrongPhase);
	Advance(Sim, Sim.GetRules().CountdownDuration + 0.5f);
	CHECK(Sim.GetPhase() == EMatchPhase::InProgress);
	CHECK(Sim.TryPossess(Alpha, Chair) == EPossessResult::Success);
}

static void TestMessagesExist()
{
	TEST("every possession result has a message the HUD can print");
	const EPossessResult AllResults[] = {
		EPossessResult::Success, EPossessResult::NoSuchPlayer, EPossessResult::NoSuchHost,
		EPossessResult::WrongPhase, EPossessResult::OnCooldown, EPossessResult::AlreadyPossessing,
		EPossessResult::OutOfRange, EPossessResult::HostOccupied, EPossessResult::HostIsFriendly,
		EPossessResult::SelfPossession, EPossessResult::Hijacked
	};
	for (EPossessResult Result : AllResults)
	{
		CHECK(std::strlen(ToString(Result)) > 0);
		CHECK(std::strcmp(ToString(Result), "UNKNOWN") != 0);
	}
	CHECK(EnemyOf(ETeam::A) == ETeam::B);
	CHECK(EnemyOf(ETeam::B) == ETeam::A);
	CHECK(EnemyOf(ETeam::None) == ETeam::None);
}

int main()
{
	std::printf("PARASITE core tests\n\n");

	TestTeamAssignment();
	TestMatchFlow();
	TestPossessionBasics();
	TestPossessionLimits();
	TestNPCPossession();
	TestEnemyPossession();
	TestEnemyPossessionRules();
	TestResist();
	TestScan();
	TestInfiltratorUpgrade();
	TestUpgrades();
	TestNestInfection();
	TestNestExpulsionAndPulse();
	TestInfiltrationBonus();
	TestWinByInfection();
	TestWinOnTime();
	TestMatchEndReleasesHosts();
	TestRematchResets();
	TestDisconnect();
	TestHostDestroyed();
	TestLateJoin();
	TestPossessNearest();
	TestPhaseGating();
	TestMessagesExist();

	std::printf("\n%d checks, %d failures\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
