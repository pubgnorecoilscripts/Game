// PARASITE - rule constants and value types.
//
// This header, and everything else under Core/, is deliberately free of any
// Unreal dependency. The rules of the game live here so they can be compiled and
// unit tested on their own (see Tests/), and the Unreal layer is a thin shell
// that feeds this simulation and renders its results.
#pragma once

#include <cstdint>
#include <cmath>
#include <string>

namespace Parasite
{
	using PlayerId = int32_t;
	using HostId = int32_t;

	static constexpr PlayerId InvalidPlayer = -1;
	static constexpr HostId InvalidHost = -1;

	enum class ETeam : uint8_t
	{
		None = 0,
		A = 1,
		B = 2
	};

	/** Broad category of a possession host. Drives duration and DNA reward. */
	enum class EHostType : uint8_t
	{
		Prop,
		NPC,
		Vehicle,
		Player
	};

	/** How a host is allowed to move while ridden. */
	enum class EHostMobility : uint8_t
	{
		Static,		// plants, vending machines: wobble and shuffle only
		Slide,		// chairs, tables, bins
		Wheeled,	// carts, cars
		Walk,		// NPCs and players
		Hinge		// doors
	};

	enum class EMatchPhase : uint8_t
	{
		Lobby,
		Countdown,
		InProgress,
		PostMatch
	};

	enum class EUpgrade : uint8_t
	{
		None = 0,
		Jumper,			// possession range
		Mimic,			// enemy possession duration
		Infiltrator		// harder to detect while possessing
	};

	/** Why a possession attempt succeeded or failed. Drives the client message. */
	enum class EPossessResult : uint8_t
	{
		Success,
		NoSuchPlayer,
		NoSuchHost,
		WrongPhase,
		OnCooldown,
		AlreadyPossessing,
		OutOfRange,
		HostOccupied,
		HostIsFriendly,
		SelfPossession,
		Hijacked			// the asking player is being ridden themselves
	};

	const char* ToString(EPossessResult Result);
	const char* ToString(ETeam Team);
	const char* ToString(EHostType Type);

	ETeam EnemyOf(ETeam Team);

	/** Minimal vector so the core can own nest and scan distances. */
	struct FVec3
	{
		float X = 0.f, Y = 0.f, Z = 0.f;

		FVec3() = default;
		FVec3(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}

		float DistanceSquaredTo(const FVec3& Other) const
		{
			const float DX = X - Other.X;
			const float DY = Y - Other.Y;
			const float DZ = Z - Other.Z;
			return DX * DX + DY * DY + DZ * DZ;
		}

		float DistanceTo(const FVec3& Other) const { return std::sqrt(DistanceSquaredTo(Other)); }
	};

	/**
	 * Every tunable number in the game. Held as a struct rather than constants so
	 * tests can run a match on a compressed timeline.
	 */
	struct FRules
	{
		// Possession.
		float BasePossessRange		= 300.f;	// 3 m
		float JumperRangeBonus		= 250.f;
		float PropDuration			= 30.f;
		float NPCDuration			= 45.f;
		float PlayerDuration		= 8.f;
		float MimicDurationBonus	= 4.f;
		float PossessCooldown		= 3.f;
		float LeapCooldown			= 2.5f;
		float LeapRange				= 900.f;

		// Detection.
		float ScanCooldown			= 20.f;
		float ScanRadius			= 2200.f;
		float ScanRevealTime		= 1.5f;
		float InfiltratorScanScale	= 0.45f;

		// Resist: fraction added per mash, so ~9 presses throws a parasite out.
		float ResistPerInput		= 0.12f;

		// Nest.
		float NestInfectSeconds		= 20.f;
		float NestRadius			= 350.f;
		float NestExpelPenalty		= 35.f;		// infection percent refunded to defenders
		float NestPulseInterval		= 6.f;
		float NestPulseGrace		= 5.f;		// pulse clamps an intruder's remaining time

		// Match.
		float MatchDuration			= 900.f;	// 15 minutes
		float CountdownDuration		= 5.f;
		float PostMatchDuration		= 15.f;
		int32_t MinPlayersToStart	= 1;
		int32_t MaxPlayers			= 10;

		// DNA.
		int32_t DNA_PossessProp		= 5;
		int32_t DNA_PossessNPC		= 10;
		int32_t DNA_PossessEnemy	= 40;
		int32_t DNA_Infiltrate		= 25;
		int32_t DNA_NestTick		= 10;
		int32_t DNA_ExpelParasite	= 30;
		float NestDNAInterval		= 3.f;
		int32_t UpgradeCost			= 60;
		int32_t MaxUpgrades			= 2;
	};
}
