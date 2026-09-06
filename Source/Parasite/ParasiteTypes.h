#pragma once

#include "CoreMinimal.h"
#include "ParasiteTypes.generated.h"

/** Which team a player belongs to. */
UENUM(BlueprintType)
enum class EParasiteTeam : uint8
{
	None	UMETA(DisplayName = "Unassigned"),
	TeamA	UMETA(DisplayName = "Team A"),
	TeamB	UMETA(DisplayName = "Team B")
};

/** Broad category of a possession host. Drives duration and DNA rewards. */
UENUM(BlueprintType)
enum class EHostType : uint8
{
	Prop	UMETA(DisplayName = "Prop"),
	NPC		UMETA(DisplayName = "NPC"),
	Vehicle	UMETA(DisplayName = "Vehicle"),
	Player	UMETA(DisplayName = "Enemy Player")
};

/** How a possessed host is allowed to move. */
UENUM(BlueprintType)
enum class EHostMobility : uint8
{
	Static		UMETA(DisplayName = "Static"),		// plants, vending machines: wobble only
	Slide		UMETA(DisplayName = "Slide"),		// chairs, tables, bins
	Wheeled		UMETA(DisplayName = "Wheeled"),		// carts, cars
	Walk		UMETA(DisplayName = "Walk"),		// NPCs and players
	Hinge		UMETA(DisplayName = "Hinge")		// doors
};

/** Match phase, replicated on the game state. */
UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	Lobby		UMETA(DisplayName = "Lobby"),
	Countdown	UMETA(DisplayName = "Countdown"),
	InProgress	UMETA(DisplayName = "In Progress"),
	PostMatch	UMETA(DisplayName = "Post Match")
};

/** The three in-match evolution upgrades. */
UENUM(BlueprintType)
enum class EParasiteUpgrade : uint8
{
	None		UMETA(DisplayName = "None"),
	Jumper		UMETA(DisplayName = "Jumper"),		// +possession range
	Mimic		UMETA(DisplayName = "Mimic"),		// +enemy possession duration
	Infiltrator	UMETA(DisplayName = "Infiltrator")	// harder to detect while possessing
};

/** A transient world marker (ping or scan hit) drawn on the HUD. */
USTRUCT(BlueprintType)
struct FParasiteMarker
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	float ExpiryTime = 0.f;

	UPROPERTY()
	FColor Color = FColor::White;

	UPROPERTY()
	FString Label;
};

/** Global tuning. Central so designers only touch one place. */
namespace ParasiteRules
{
	static constexpr float BasePossessRange		= 300.f;	// 3 m
	static constexpr float JumperRangeBonus		= 250.f;	// Jumper upgrade
	static constexpr float PropDuration			= 30.f;
	static constexpr float NPCDuration			= 45.f;
	static constexpr float PlayerDuration		= 8.f;
	static constexpr float MimicDurationBonus	= 4.f;
	static constexpr float PossessCooldown		= 3.f;
	static constexpr float ScanCooldown			= 20.f;
	static constexpr float ScanRadius			= 2200.f;
	static constexpr float ScanRevealTime		= 1.5f;
	static constexpr float InfiltratorScanScale	= 0.45f;	// scan radius multiplier vs. this player
	static constexpr float LeapRange			= 900.f;
	static constexpr float LeapCooldown			= 2.5f;

	static constexpr float NestInfectSeconds	= 20.f;		// uninterrupted seconds for 100%
	static constexpr float NestResetOnExpel		= 35.f;		// percent lost when expelled
	static constexpr float NestRadius			= 350.f;
	static constexpr float NestPulseInterval	= 6.f;
	static constexpr float NestPulseRadius		= 700.f;

	static constexpr float MatchDuration		= 900.f;	// 15 minutes
	static constexpr float CountdownDuration	= 5.f;
	static constexpr float PostMatchDuration	= 15.f;

	static constexpr int32 DNA_PossessEnemy		= 40;
	static constexpr int32 DNA_Infiltrate		= 25;
	static constexpr int32 DNA_NestTick			= 10;
	static constexpr int32 DNA_PossessHost		= 5;
	static constexpr int32 DNA_ExpelParasite	= 30;
	static constexpr int32 UpgradeCost			= 60;
	static constexpr int32 MaxUpgrades			= 2;

	static constexpr int32 MaxPlayers			= 10;
}
