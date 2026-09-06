#include "MatchSim.h"

#include <algorithm>

namespace Parasite
{
	bool FPlayer::HasUpgrade(EUpgrade Upgrade) const
	{
		return std::find(Upgrades.begin(), Upgrades.end(), Upgrade) != Upgrades.end();
	}

	bool FZone::Contains(const FVec3& Point) const
	{
		return bValid &&
			Point.X >= Min.X && Point.X <= Max.X &&
			Point.Y >= Min.Y && Point.Y <= Max.Y &&
			Point.Z >= Min.Z && Point.Z <= Max.Z;
	}

	FMatchSim::FMatchSim(const FRules& InRules)
		: Rules(InRules)
	{
		NestA.OwningTeam = ETeam::A;
		NestB.OwningTeam = ETeam::B;
	}

	// -----------------------------------------------------------------------
	// Lookups
	// -----------------------------------------------------------------------

	FPlayer* FMatchSim::MutablePlayer(PlayerId Player)
	{
		for (FPlayer& Candidate : Players)
		{
			if (Candidate.Id == Player)
			{
				return &Candidate;
			}
		}
		return nullptr;
	}

	const FPlayer* FMatchSim::FindPlayer(PlayerId Player) const
	{
		return const_cast<FMatchSim*>(this)->MutablePlayer(Player);
	}

	FHost* FMatchSim::MutableHost(HostId Host)
	{
		for (FHost& Candidate : Hosts)
		{
			if (Candidate.Id == Host && Candidate.bAlive)
			{
				return &Candidate;
			}
		}
		return nullptr;
	}

	const FHost* FMatchSim::FindHost(HostId Host) const
	{
		return const_cast<FMatchSim*>(this)->MutableHost(Host);
	}

	const FHost* FMatchSim::HostOfPlayerBody(PlayerId Player) const
	{
		const FPlayer* Found = FindPlayer(Player);
		return Found ? FindHost(Found->BodyHost) : nullptr;
	}

	const FNest& FMatchSim::GetNest(ETeam OwningTeam) const
	{
		return (OwningTeam == ETeam::B) ? NestB : NestA;
	}

	// -----------------------------------------------------------------------
	// World
	// -----------------------------------------------------------------------

	HostId FMatchSim::AddHost(EHostType Type, EHostMobility Mobility, const std::string& DisplayName, const FVec3& Location)
	{
		FHost Host;
		Host.Id = NextHostId++;
		Host.Type = Type;
		Host.Mobility = Mobility;
		Host.DisplayName = DisplayName;
		Host.Location = Location;
		Hosts.push_back(Host);
		return Host.Id;
	}

	void FMatchSim::RemoveHost(HostId Host)
	{
		FHost* Found = MutableHost(Host);
		if (!Found)
		{
			return;
		}
		// Anybody riding a host that is destroyed is spat back out.
		if (Found->bPossessed && Found->Rider != InvalidPlayer)
		{
			ExitPossession(Found->Rider, false);
		}
		Found->bAlive = false;
	}

	void FMatchSim::SetHostLocation(HostId Host, const FVec3& Location)
	{
		if (FHost* Found = MutableHost(Host))
		{
			Found->Location = Location;
		}
	}

	void FMatchSim::SetNestLocation(ETeam OwningTeam, const FVec3& Location)
	{
		FNest& Nest = (OwningTeam == ETeam::B) ? NestB : NestA;
		Nest.OwningTeam = OwningTeam;
		Nest.Location = Location;
	}

	void FMatchSim::SetRestrictedZone(ETeam ForTeam, const FVec3& Min, const FVec3& Max)
	{
		FZone& Zone = (ForTeam == ETeam::B) ? RestrictedForB : RestrictedForA;
		Zone.Min = Min;
		Zone.Max = Max;
		Zone.bValid = true;
	}

	// -----------------------------------------------------------------------
	// Players
	// -----------------------------------------------------------------------

	ETeam FMatchSim::PickBalancedTeam() const
	{
		const int32_t CountA = GetTeamPlayerCount(ETeam::A);
		const int32_t CountB = GetTeamPlayerCount(ETeam::B);
		return (CountA <= CountB) ? ETeam::A : ETeam::B;
	}

	PlayerId FMatchSim::AddPlayer(const std::string& Name)
	{
		if (static_cast<int32_t>(Players.size()) >= Rules.MaxPlayers)
		{
			return InvalidPlayer;
		}

		FPlayer Player;
		Player.Id = NextPlayerId++;
		Player.Name = Name;
		Player.Team = PickBalancedTeam();
		Players.push_back(Player);

		// The parasite body is itself a host, so enemy possession runs through
		// exactly the same path as possessing a chair.
		const HostId Body = AddHost(EHostType::Player, EHostMobility::Walk, "ENEMY", FVec3());
		if (FHost* BodyHost = MutableHost(Body))
		{
			BodyHost->OwningPlayer = Player.Id;
		}
		Players.back().BodyHost = Body;
		return Players.back().Id;
	}

	void FMatchSim::RemovePlayer(PlayerId Player)
	{
		FPlayer* Found = MutablePlayer(Player);
		if (!Found)
		{
			return;
		}
		// Free the host they were riding, and throw out anybody riding them.
		ExitPossession(Player, false);
		const HostId Body = Found->BodyHost;
		if (const FHost* BodyHost = FindHost(Body))
		{
			if (BodyHost->bPossessed && BodyHost->Rider != InvalidPlayer)
			{
				ExitPossession(BodyHost->Rider, false);
			}
		}
		RemoveHost(Body);

		Players.erase(std::remove_if(Players.begin(), Players.end(),
			[Player](const FPlayer& Candidate) { return Candidate.Id == Player; }), Players.end());
	}

	void FMatchSim::SetPlayerLocation(PlayerId Player, const FVec3& Location)
	{
		if (const FPlayer* Found = FindPlayer(Player))
		{
			SetHostLocation(Found->BodyHost, Location);
		}
	}

	FVec3 FMatchSim::GetPlayerPresence(PlayerId Player) const
	{
		const FPlayer* Found = FindPlayer(Player);
		if (!Found)
		{
			return FVec3();
		}
		if (Found->CurrentHost != InvalidHost)
		{
			if (const FHost* Host = FindHost(Found->CurrentHost))
			{
				return Host->Location;
			}
		}
		const FHost* Body = FindHost(Found->BodyHost);
		return Body ? Body->Location : FVec3();
	}

	// -----------------------------------------------------------------------
	// Derived values
	// -----------------------------------------------------------------------

	float FMatchSim::GetPossessRange(PlayerId Player) const
	{
		const FPlayer* Found = FindPlayer(Player);
		if (!Found)
		{
			return Rules.BasePossessRange;
		}
		return Rules.BasePossessRange + (Found->HasUpgrade(EUpgrade::Jumper) ? Rules.JumperRangeBonus : 0.f);
	}

	float FMatchSim::GetHostDuration(EHostType Type, PlayerId Rider) const
	{
		switch (Type)
		{
		case EHostType::NPC:
			return Rules.NPCDuration;
		case EHostType::Player:
		{
			const FPlayer* Found = FindPlayer(Rider);
			const bool bMimic = Found && Found->HasUpgrade(EUpgrade::Mimic);
			return Rules.PlayerDuration + (bMimic ? Rules.MimicDurationBonus : 0.f);
		}
		default:
			return Rules.PropDuration;
		}
	}

	float FMatchSim::GetStealthScale(PlayerId Player) const
	{
		const FPlayer* Found = FindPlayer(Player);
		return (Found && Found->HasUpgrade(EUpgrade::Infiltrator)) ? Rules.InfiltratorScanScale : 1.f;
	}

	int32_t FMatchSim::DNAForHost(EHostType Type) const
	{
		switch (Type)
		{
		case EHostType::Player:	return Rules.DNA_PossessEnemy;
		case EHostType::NPC:	return Rules.DNA_PossessNPC;
		default:				return Rules.DNA_PossessProp;
		}
	}

	void FMatchSim::AwardDNA(FPlayer& Player, int32_t Amount)
	{
		if (Amount <= 0)
		{
			return;
		}
		Player.DNA += Amount;
		Player.LifetimeDNA += Amount;
	}

	// -----------------------------------------------------------------------
	// Possession
	// -----------------------------------------------------------------------

	EPossessResult FMatchSim::TryPossess(PlayerId Player, HostId Host)
	{
		FPlayer* Asker = MutablePlayer(Player);
		if (!Asker)
		{
			return EPossessResult::NoSuchPlayer;
		}
		if (Phase != EMatchPhase::InProgress)
		{
			return EPossessResult::WrongPhase;
		}
		if (Asker->CurrentHost != InvalidHost)
		{
			return EPossessResult::AlreadyPossessing;
		}
		if (Time < Asker->PossessReadyTime)
		{
			return EPossessResult::OnCooldown;
		}
		// A parasite being ridden cannot go anywhere itself.
		if (const FHost* Body = FindHost(Asker->BodyHost))
		{
			if (Body->bPossessed)
			{
				return EPossessResult::Hijacked;
			}
		}

		FHost* Target = MutableHost(Host);
		if (!Target)
		{
			return EPossessResult::NoSuchHost;
		}
		if (Target->OwningPlayer == Player)
		{
			return EPossessResult::SelfPossession;
		}
		if (Target->bPossessed)
		{
			return EPossessResult::HostOccupied;
		}
		// Enemy bodies only: never ride a team mate.
		if (Target->Type == EHostType::Player)
		{
			const FPlayer* Victim = FindPlayer(Target->OwningPlayer);
			if (!Victim || Victim->Team == Asker->Team)
			{
				return EPossessResult::HostIsFriendly;
			}
		}

		const FVec3 From = GetPlayerPresence(Player);
		if (From.DistanceTo(Target->Location) > GetPossessRange(Player))
		{
			return EPossessResult::OutOfRange;
		}

		Target->bPossessed = true;
		Target->Rider = Player;
		Target->ResistProgress = 0.f;
		Target->PossessionEndTime = Time + GetHostDuration(Target->Type, Player);
		Asker->CurrentHost = Target->Id;
		AwardDNA(*Asker, DNAForHost(Target->Type));
		return EPossessResult::Success;
	}

	EPossessResult FMatchSim::TryPossessNearest(PlayerId Player, HostId& OutHost)
	{
		OutHost = InvalidHost;
		const FPlayer* Asker = FindPlayer(Player);
		if (!Asker)
		{
			return EPossessResult::NoSuchPlayer;
		}

		const FVec3 From = GetPlayerPresence(Player);
		const float Range = GetPossessRange(Player);

		HostId Best = InvalidHost;
		float BestDistance = Range;
		for (const FHost& Host : Hosts)
		{
			if (!Host.bAlive || Host.bPossessed || Host.OwningPlayer == Player)
			{
				continue;
			}
			if (Host.Type == EHostType::Player)
			{
				const FPlayer* Victim = FindPlayer(Host.OwningPlayer);
				if (!Victim || Victim->Team == Asker->Team)
				{
					continue;
				}
			}
			const float Distance = From.DistanceTo(Host.Location);
			if (Distance <= BestDistance)
			{
				BestDistance = Distance;
				Best = Host.Id;
			}
		}

		if (Best == InvalidHost)
		{
			return EPossessResult::NoSuchHost;
		}
		const EPossessResult Result = TryPossess(Player, Best);
		if (Result == EPossessResult::Success)
		{
			OutHost = Best;
		}
		return Result;
	}

	bool FMatchSim::ExitPossession(PlayerId Player, bool bExpelled)
	{
		FPlayer* Rider = MutablePlayer(Player);
		if (!Rider || Rider->CurrentHost == InvalidHost)
		{
			return false;
		}

		FVec3 ExitLocation = GetPlayerPresence(Player);
		if (FHost* Host = MutableHost(Rider->CurrentHost))
		{
			Host->bPossessed = false;
			Host->Rider = InvalidPlayer;
			Host->PossessionEndTime = 0.f;
			Host->ResistProgress = 0.f;
			ExitLocation = Host->Location;
		}
		Rider->CurrentHost = InvalidHost;
		Rider->PossessReadyTime = Time + Rules.PossessCooldown;

		// The parasite pops out where its host stood.
		SetHostLocation(Rider->BodyHost, ExitLocation);

		// Being thrown out inside an enemy nest hands progress back to its defenders.
		if (bExpelled)
		{
			FNest& EnemyNest = (Rider->Team == ETeam::A) ? NestB : NestA;
			if (Rider->Team != ETeam::None &&
				ExitLocation.DistanceTo(EnemyNest.Location) <= Rules.NestRadius * 1.5f)
			{
				EnemyNest.Infection = std::max(0.f, EnemyNest.Infection - Rules.NestExpelPenalty);
				EnemyNest.Health = std::min(100.f, 100.f - EnemyNest.Infection);
			}
		}
		return true;
	}

	bool FMatchSim::AddResist(PlayerId Victim)
	{
		FPlayer* Found = MutablePlayer(Victim);
		if (!Found)
		{
			return false;
		}
		FHost* Body = MutableHost(Found->BodyHost);
		if (!Body || !Body->bPossessed)
		{
			return false;
		}
		Body->ResistProgress = std::min(1.f, Body->ResistProgress + Rules.ResistPerInput);
		if (Body->ResistProgress < 1.f)
		{
			return false;
		}

		const PlayerId Rider = Body->Rider;
		ExitPossession(Rider, true);
		AwardDNA(*Found, Rules.DNA_ExpelParasite);
		return true;
	}

	// -----------------------------------------------------------------------
	// Detection and movement abilities
	// -----------------------------------------------------------------------

	bool FMatchSim::TryScan(PlayerId Player, std::vector<HostId>& OutRevealed)
	{
		OutRevealed.clear();
		FPlayer* Scanner = MutablePlayer(Player);
		if (!Scanner || Time < Scanner->ScanReadyTime)
		{
			return false;
		}
		Scanner->ScanReadyTime = Time + Rules.ScanCooldown;

		const FVec3 Origin = GetPlayerPresence(Player);
		for (const FHost& Host : Hosts)
		{
			if (!Host.bAlive || !Host.bPossessed || Host.Rider == InvalidPlayer)
			{
				continue;
			}
			const FPlayer* Rider = FindPlayer(Host.Rider);
			if (!Rider || Rider->Team == Scanner->Team)
			{
				continue;		// only enemy infiltrators light up
			}
			// Infiltrator shrinks the radius they can be caught inside.
			const float Radius = Rules.ScanRadius * GetStealthScale(Host.Rider);
			if (Origin.DistanceTo(Host.Location) <= Radius)
			{
				OutRevealed.push_back(Host.Id);
				Reveals.emplace_back(Host.Id, Time + Rules.ScanRevealTime);
			}
		}
		return true;
	}

	float FMatchSim::GetRevealEndTime(HostId Host) const
	{
		float Latest = 0.f;
		for (const std::pair<HostId, float>& Reveal : Reveals)
		{
			if (Reveal.first == Host)
			{
				Latest = std::max(Latest, Reveal.second);
			}
		}
		return Latest;
	}

	bool FMatchSim::TryLeap(PlayerId Player)
	{
		FPlayer* Found = MutablePlayer(Player);
		if (!Found || Time < Found->LeapReadyTime || Found->CurrentHost != InvalidHost)
		{
			return false;
		}
		const FHost* Body = FindHost(Found->BodyHost);
		if (!Body || Body->bPossessed)
		{
			return false;		// a hijacked parasite is not going anywhere
		}
		Found->LeapReadyTime = Time + Rules.LeapCooldown;
		return true;
	}

	bool FMatchSim::TryPurchaseUpgrade(PlayerId Player, EUpgrade Upgrade)
	{
		FPlayer* Found = MutablePlayer(Player);
		if (!Found || Upgrade == EUpgrade::None)
		{
			return false;
		}
		if (static_cast<int32_t>(Found->Upgrades.size()) >= Rules.MaxUpgrades ||
			Found->HasUpgrade(Upgrade) || Found->DNA < Rules.UpgradeCost)
		{
			return false;
		}
		Found->DNA -= Rules.UpgradeCost;
		Found->Upgrades.push_back(Upgrade);
		return true;
	}

	// -----------------------------------------------------------------------
	// Match flow
	// -----------------------------------------------------------------------

	void FMatchSim::EnterPhase(EMatchPhase NewPhase)
	{
		Phase = NewPhase;
		switch (NewPhase)
		{
		case EMatchPhase::Lobby:
			PhaseTimeRemaining = 0.f;
			Winner = ETeam::None;
			ResultReason.clear();
			break;

		case EMatchPhase::Countdown:
			PhaseTimeRemaining = Rules.CountdownDuration;
			break;

		case EMatchPhase::InProgress:
			PhaseTimeRemaining = Rules.MatchDuration;
			NestA.Infection = 0.f;
			NestB.Infection = 0.f;
			NestA.Health = 100.f;
			NestB.Health = 100.f;
			NestA.bUnderAttack = false;
			NestB.bUnderAttack = false;
			Winner = ETeam::None;
			ResultReason.clear();
			for (FPlayer& Player : Players)
			{
				Player.bCreditedInfiltration = false;
			}
			break;

		case EMatchPhase::PostMatch:
			PhaseTimeRemaining = Rules.PostMatchDuration;
			ReleaseEverybody();
			break;
		}
	}

	void FMatchSim::ReleaseEverybody()
	{
		for (const FPlayer& Player : Players)
		{
			if (Player.CurrentHost != InvalidHost)
			{
				ExitPossession(Player.Id, false);
			}
		}
	}

	void FMatchSim::RestartMatch()
	{
		ReleaseEverybody();
		for (FPlayer& Player : Players)
		{
			Player.DNA = 0;
			Player.LifetimeDNA = 0;
			Player.InfectionTicks = 0;
			Player.Upgrades.clear();
			Player.PossessReadyTime = 0.f;
			Player.ScanReadyTime = 0.f;
			Player.LeapReadyTime = 0.f;
			Player.bCreditedInfiltration = false;
		}
		Reveals.clear();
		EnterPhase(EMatchPhase::Countdown);
	}

	void FMatchSim::TickPossessionTimers()
	{
		for (const FHost& Host : Hosts)
		{
			if (Host.bAlive && Host.bPossessed && Time >= Host.PossessionEndTime)
			{
				ExitPossession(Host.Rider, false);
			}
		}
	}

	void FMatchSim::TickNests(float DeltaSeconds)
	{
		FNest* AllNests[2] = { &NestA, &NestB };
		for (FNest* Nest : AllNests)
		{
			std::vector<PlayerId> Attackers;
			for (const FPlayer& Player : Players)
			{
				if (Player.Team == ETeam::None || Player.Team == Nest->OwningTeam)
				{
					continue;
				}
				if (GetPlayerPresence(Player.Id).DistanceTo(Nest->Location) <= Rules.NestRadius)
				{
					Attackers.push_back(Player.Id);
				}
			}

			Nest->bUnderAttack = !Attackers.empty();
			Nest->bPulsedThisTick = false;

			if (Nest->bUnderAttack && Nest->Infection < 100.f)
			{
				// Extra attackers help, with diminishing returns, so a five stack
				// cannot burst a nest down in four seconds.
				const float Rate = (100.f / Rules.NestInfectSeconds) *
					(1.f + 0.25f * static_cast<float>(Attackers.size() - 1));
				Nest->Infection = std::min(100.f, Nest->Infection + Rate * DeltaSeconds);
				Nest->Health = std::max(0.f, 100.f - Nest->Infection);

				Nest->DNATimer += DeltaSeconds;
				if (Nest->DNATimer >= Rules.NestDNAInterval)
				{
					Nest->DNATimer = 0.f;
					for (PlayerId Attacker : Attackers)
					{
						if (FPlayer* Found = MutablePlayer(Attacker))
						{
							AwardDNA(*Found, Rules.DNA_NestTick);
							Found->InfectionTicks++;
						}
					}
				}
			}
			else if (!Nest->bUnderAttack)
			{
				// Progress stops when the parasite leaves, but is not lost.
				Nest->DNATimer = 0.f;
			}

			// Defensive pulse: cuts an intruder's remaining stay short.
			Nest->PulseTimer += DeltaSeconds;
			if (Nest->PulseTimer >= Rules.NestPulseInterval)
			{
				Nest->PulseTimer = 0.f;
				if (Nest->bUnderAttack)
				{
					Nest->bPulsedThisTick = true;
					for (PlayerId Attacker : Attackers)
					{
						const FPlayer* Found = FindPlayer(Attacker);
						if (!Found || Found->CurrentHost == InvalidHost)
						{
							continue;
						}
						if (FHost* Host = MutableHost(Found->CurrentHost))
						{
							Host->PossessionEndTime = std::min(Host->PossessionEndTime, Time + Rules.NestPulseGrace);
						}
					}
				}
			}
		}
	}

	void FMatchSim::TickInfiltration()
	{
		for (FPlayer& Player : Players)
		{
			if (Player.bCreditedInfiltration || Player.Team == ETeam::None)
			{
				continue;
			}
			// Infiltration means getting in wearing a disguise. Strolling in as a
			// bare parasite is not infiltration, it is a walk.
			if (Player.CurrentHost == InvalidHost)
			{
				continue;
			}
			const FZone& Zone = (Player.Team == ETeam::B) ? RestrictedForB : RestrictedForA;
			if (Zone.Contains(GetPlayerPresence(Player.Id)))
			{
				AwardDNA(Player, Rules.DNA_Infiltrate);
				Player.bCreditedInfiltration = true;
			}
		}
	}

	void FMatchSim::Tick(float DeltaSeconds)
	{
		Time += DeltaSeconds;

		// Expired reveals are dropped so the list cannot grow without bound.
		Reveals.erase(std::remove_if(Reveals.begin(), Reveals.end(),
			[this](const std::pair<HostId, float>& Reveal) { return Reveal.second <= Time; }), Reveals.end());

		switch (Phase)
		{
		case EMatchPhase::Lobby:
			if (static_cast<int32_t>(Players.size()) >= Rules.MinPlayersToStart)
			{
				EnterPhase(EMatchPhase::Countdown);
			}
			break;

		case EMatchPhase::Countdown:
			PhaseTimeRemaining -= DeltaSeconds;
			if (PhaseTimeRemaining <= 0.f)
			{
				EnterPhase(EMatchPhase::InProgress);
			}
			break;

		case EMatchPhase::InProgress:
			PhaseTimeRemaining = std::max(0.f, PhaseTimeRemaining - DeltaSeconds);
			TickPossessionTimers();
			TickNests(DeltaSeconds);
			TickInfiltration();
			EvaluateWinCondition();
			break;

		case EMatchPhase::PostMatch:
			PhaseTimeRemaining -= DeltaSeconds;
			if (PhaseTimeRemaining <= 0.f)
			{
				RestartMatch();
			}
			break;
		}
	}

	void FMatchSim::EvaluateWinCondition()
	{
		if (NestB.Infection >= 100.f)
		{
			FinishMatch(ETeam::A, "TEAM B NEST FULLY INFECTED");
			return;
		}
		if (NestA.Infection >= 100.f)
		{
			FinishMatch(ETeam::B, "TEAM A NEST FULLY INFECTED");
			return;
		}
		if (PhaseTimeRemaining > 0.f)
		{
			return;
		}

		const float ByA = GetInfectionByTeam(ETeam::A);
		const float ByB = GetInfectionByTeam(ETeam::B);
		if (std::fabs(ByA - ByB) > 0.01f)
		{
			FinishMatch(ByA > ByB ? ETeam::A : ETeam::B, "TIME UP - HIGHEST INFECTION");
			return;
		}

		const int32_t DNAA = GetTeamDNA(ETeam::A);
		const int32_t DNAB = GetTeamDNA(ETeam::B);
		if (DNAA == DNAB)
		{
			FinishMatch(ETeam::None, "TIME UP - DEAD HEAT");
		}
		else
		{
			FinishMatch(DNAA > DNAB ? ETeam::A : ETeam::B, "TIME UP - DNA TIE BREAK");
		}
	}

	void FMatchSim::FinishMatch(ETeam InWinner, const std::string& Reason)
	{
		if (Phase == EMatchPhase::PostMatch)
		{
			return;
		}
		Winner = InWinner;
		ResultReason = Reason;
		EnterPhase(EMatchPhase::PostMatch);
	}

	// -----------------------------------------------------------------------
	// Queries
	// -----------------------------------------------------------------------

	float FMatchSim::GetInfectionByTeam(ETeam Team) const
	{
		// A team's score is the infection it has inflicted on the ENEMY nest.
		if (Team == ETeam::A)
		{
			return NestB.Infection;
		}
		if (Team == ETeam::B)
		{
			return NestA.Infection;
		}
		return 0.f;
	}

	int32_t FMatchSim::GetTeamDNA(ETeam Team) const
	{
		int32_t Total = 0;
		for (const FPlayer& Player : Players)
		{
			if (Player.Team == Team)
			{
				Total += Player.LifetimeDNA;
			}
		}
		return Total;
	}

	int32_t FMatchSim::GetTeamPlayerCount(ETeam Team) const
	{
		int32_t Count = 0;
		for (const FPlayer& Player : Players)
		{
			if (Player.Team == Team && Player.bConnected)
			{
				++Count;
			}
		}
		return Count;
	}
}
