// PARASITE - the authoritative match simulation.
//
// No Unreal types. The server owns one FMatchSim; the Unreal layer forwards
// player requests into it, then mirrors the results onto actors and replicated
// properties. Tests/CoreTests.cpp drives this class directly.
#pragma once

#include "ParasiteRules.h"

#include <string>
#include <vector>

namespace Parasite
{
	/** A possessable thing in the world. Player parasites are hosts too. */
	struct FHost
	{
		HostId Id = InvalidHost;
		EHostType Type = EHostType::Prop;
		EHostMobility Mobility = EHostMobility::Slide;
		std::string DisplayName = "OBJECT";
		FVec3 Location;

		bool bPossessed = false;
		PlayerId Rider = InvalidPlayer;
		float PossessionEndTime = 0.f;

		/** Set only for EHostType::Player: the parasite body this host is. */
		PlayerId OwningPlayer = InvalidPlayer;

		/** 0..1, filled by the victim mashing resist while hijacked. */
		float ResistProgress = 0.f;

		bool bAlive = true;
	};

	struct FPlayer
	{
		PlayerId Id = InvalidPlayer;
		std::string Name;
		ETeam Team = ETeam::None;

		int32_t DNA = 0;
		int32_t LifetimeDNA = 0;
		int32_t InfectionTicks = 0;

		std::vector<EUpgrade> Upgrades;

		float PossessReadyTime = 0.f;
		float ScanReadyTime = 0.f;
		float LeapReadyTime = 0.f;

		/** The host being ridden, or InvalidHost when walking around as a parasite. */
		HostId CurrentHost = InvalidHost;

		/** The player's own parasite body, which enemies can hijack. */
		HostId BodyHost = InvalidHost;

		bool bCreditedInfiltration = false;
		bool bConnected = true;

		bool HasUpgrade(EUpgrade Upgrade) const;
	};

	struct FNest
	{
		ETeam OwningTeam = ETeam::None;
		FVec3 Location;
		float Infection = 0.f;		// 0..100, progress made by the ENEMY of OwningTeam
		float Health = 100.f;
		bool bUnderAttack = false;
		float PulseTimer = 0.f;
		float DNATimer = 0.f;
		bool bPulsedThisTick = false;
	};

	/** An axis aligned box marking one team's back of house. */
	struct FZone
	{
		FVec3 Min;
		FVec3 Max;
		bool bValid = false;

		bool Contains(const FVec3& Point) const;
	};

	/**
	 * The whole ruleset. Deterministic, server authoritative, and independent of
	 * any engine: feed it requests and Tick it.
	 */
	class FMatchSim
	{
	public:
		explicit FMatchSim(const FRules& InRules = FRules());

		// --- World -------------------------------------------------------
		HostId AddHost(EHostType Type, EHostMobility Mobility, const std::string& DisplayName, const FVec3& Location);
		void RemoveHost(HostId Host);
		void SetHostLocation(HostId Host, const FVec3& Location);
		const FHost* FindHost(HostId Host) const;
		const std::vector<FHost>& GetHosts() const { return Hosts; }

		void SetNestLocation(ETeam OwningTeam, const FVec3& Location);
		const FNest& GetNest(ETeam OwningTeam) const;

		/** Marks the area ForTeam earns an infiltration bonus for reaching. */
		void SetRestrictedZone(ETeam ForTeam, const FVec3& Min, const FVec3& Max);

		// --- Players -----------------------------------------------------
		/** Joins a player, balances them onto a team and creates their body host. */
		PlayerId AddPlayer(const std::string& Name);
		void RemovePlayer(PlayerId Player);
		const FPlayer* FindPlayer(PlayerId Player) const;
		const std::vector<FPlayer>& GetPlayers() const { return Players; }

		/** Moves a player's parasite body (the Unreal layer pushes this each tick). */
		void SetPlayerLocation(PlayerId Player, const FVec3& Location);

		/** Where the player physically is: their host if riding one, else their body. */
		FVec3 GetPlayerPresence(PlayerId Player) const;

		// --- Actions -----------------------------------------------------
		EPossessResult TryPossess(PlayerId Player, HostId Host);

		/** Finds the best host within range and possesses it. */
		EPossessResult TryPossessNearest(PlayerId Player, HostId& OutHost);

		/** Leaves the current host. bExpelled applies the nest penalty. */
		bool ExitPossession(PlayerId Player, bool bExpelled);

		/** Reveals enemy-ridden hosts near the player. Returns false on cooldown. */
		bool TryScan(PlayerId Player, std::vector<HostId>& OutRevealed);

		bool TryLeap(PlayerId Player);

		/** Victim mashes resist. Returns true when the parasite is thrown out. */
		bool AddResist(PlayerId Victim);

		bool TryPurchaseUpgrade(PlayerId Player, EUpgrade Upgrade);

		// --- Derived values ----------------------------------------------
		float GetPossessRange(PlayerId Player) const;
		float GetHostDuration(EHostType Type, PlayerId Rider) const;
		float GetStealthScale(PlayerId Player) const;

		// --- Flow ---------------------------------------------------------
		void Tick(float DeltaSeconds);
		void RestartMatch();

		EMatchPhase GetPhase() const { return Phase; }
		float GetPhaseTimeRemaining() const { return PhaseTimeRemaining; }
		float GetTime() const { return Time; }

		/** Infection percent this team has inflicted on the enemy nest. */
		float GetInfectionByTeam(ETeam Team) const;
		int32_t GetTeamDNA(ETeam Team) const;
		int32_t GetTeamPlayerCount(ETeam Team) const;

		ETeam GetWinner() const { return Winner; }
		const std::string& GetResultReason() const { return ResultReason; }

		const FRules& GetRules() const { return Rules; }

		/** Hosts revealed by the most recent scan, and when the reveal ends. */
		float GetRevealEndTime(HostId Host) const;

	private:
		FPlayer* MutablePlayer(PlayerId Player);
		FHost* MutableHost(HostId Host);
		const FHost* HostOfPlayerBody(PlayerId Player) const;

		void AwardDNA(FPlayer& Player, int32_t Amount);
		void EnterPhase(EMatchPhase NewPhase);
		void TickNests(float DeltaSeconds);
		void TickPossessionTimers();
		void TickInfiltration();
		void EvaluateWinCondition();
		void FinishMatch(ETeam InWinner, const std::string& Reason);
		void ReleaseEverybody();
		ETeam PickBalancedTeam() const;
		int32_t DNAForHost(EHostType Type) const;

		FRules Rules;

		std::vector<FPlayer> Players;
		std::vector<FHost> Hosts;
		FNest NestA;
		FNest NestB;
		FZone RestrictedForA;
		FZone RestrictedForB;

		std::vector<std::pair<HostId, float>> Reveals;

		EMatchPhase Phase = EMatchPhase::Lobby;
		float PhaseTimeRemaining = 0.f;
		float Time = 0.f;
		ETeam Winner = ETeam::None;
		std::string ResultReason;

		HostId NextHostId = 1;
		PlayerId NextPlayerId = 1;
	};
}
