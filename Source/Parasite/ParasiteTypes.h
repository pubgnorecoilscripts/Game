#pragma once

#include "CoreMinimal.h"
#include "Core/ParasiteRules.h"
#include "ParasiteTypes.generated.h"

/**
 * Blueprint/replication facing mirrors of the core enums. The values match
 * Parasite::E* exactly, so conversion is a cast in both directions - see the
 * helpers at the bottom of this file.
 */

UENUM(BlueprintType)
enum class EParasiteTeam : uint8
{
	None	UMETA(DisplayName = "Unassigned"),
	TeamA	UMETA(DisplayName = "Team A"),
	TeamB	UMETA(DisplayName = "Team B")
};

UENUM(BlueprintType)
enum class EHostType : uint8
{
	Prop	UMETA(DisplayName = "Prop"),
	NPC		UMETA(DisplayName = "NPC"),
	Vehicle	UMETA(DisplayName = "Vehicle"),
	Player	UMETA(DisplayName = "Enemy Player")
};

UENUM(BlueprintType)
enum class EHostMobility : uint8
{
	Static	UMETA(DisplayName = "Static"),		// plants, vending machines: wobble only
	Slide	UMETA(DisplayName = "Slide"),		// chairs, tables, bins
	Wheeled	UMETA(DisplayName = "Wheeled"),		// carts, cars
	Walk	UMETA(DisplayName = "Walk"),		// NPCs and players
	Hinge	UMETA(DisplayName = "Hinge")		// doors
};

UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	Lobby		UMETA(DisplayName = "Lobby"),
	Countdown	UMETA(DisplayName = "Countdown"),
	InProgress	UMETA(DisplayName = "In Progress"),
	PostMatch	UMETA(DisplayName = "Post Match")
};

UENUM(BlueprintType)
enum class EParasiteUpgrade : uint8
{
	None		UMETA(DisplayName = "None"),
	Jumper		UMETA(DisplayName = "Jumper"),
	Mimic		UMETA(DisplayName = "Mimic"),
	Infiltrator	UMETA(DisplayName = "Infiltrator")
};

/** A transient world marker (ping or scan hit) drawn on the HUD. */
USTRUCT()
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

// --- Conversions -----------------------------------------------------------
// The mirrors are declared in the same order as the core enums, so these are
// plain casts. The static_asserts make sure that stays true.

static_assert(static_cast<uint8>(EParasiteTeam::TeamB) == static_cast<uint8>(Parasite::ETeam::B), "team enums drifted");
static_assert(static_cast<uint8>(EHostType::Player) == static_cast<uint8>(Parasite::EHostType::Player), "host type enums drifted");
static_assert(static_cast<uint8>(EHostMobility::Hinge) == static_cast<uint8>(Parasite::EHostMobility::Hinge), "mobility enums drifted");
static_assert(static_cast<uint8>(EMatchPhase::PostMatch) == static_cast<uint8>(Parasite::EMatchPhase::PostMatch), "phase enums drifted");
static_assert(static_cast<uint8>(EParasiteUpgrade::Infiltrator) == static_cast<uint8>(Parasite::EUpgrade::Infiltrator), "upgrade enums drifted");

FORCEINLINE Parasite::ETeam ToCore(EParasiteTeam Team) { return static_cast<Parasite::ETeam>(Team); }
FORCEINLINE EParasiteTeam FromCore(Parasite::ETeam Team) { return static_cast<EParasiteTeam>(Team); }

FORCEINLINE Parasite::EHostType ToCore(EHostType Type) { return static_cast<Parasite::EHostType>(Type); }
FORCEINLINE EHostType FromCore(Parasite::EHostType Type) { return static_cast<EHostType>(Type); }

FORCEINLINE Parasite::EHostMobility ToCore(EHostMobility Mobility) { return static_cast<Parasite::EHostMobility>(Mobility); }
FORCEINLINE EHostMobility FromCore(Parasite::EHostMobility Mobility) { return static_cast<EHostMobility>(Mobility); }

FORCEINLINE EMatchPhase FromCore(Parasite::EMatchPhase Phase) { return static_cast<EMatchPhase>(Phase); }
FORCEINLINE Parasite::EUpgrade ToCore(EParasiteUpgrade Upgrade) { return static_cast<Parasite::EUpgrade>(Upgrade); }

FORCEINLINE FVector FromCore(const Parasite::FVec3& Vector) { return FVector(Vector.X, Vector.Y, Vector.Z); }
FORCEINLINE Parasite::FVec3 ToCore(const FVector& Vector)
{
	return Parasite::FVec3(static_cast<float>(Vector.X), static_cast<float>(Vector.Y), static_cast<float>(Vector.Z));
}
