#pragma once

#include "CoreMinimal.h"

/** Every sound the prototype needs. All are synthesised at runtime, no assets. */
enum class EParasiteSound : uint8
{
	Possess,
	PossessFail,
	PossessExit,
	ParasiteMove,
	ScanPulse,
	Detected,
	NestInfect,
	NestDamage,
	MatchStart,
	MatchEnd,
	UIClick
};

/**
 * Tiny procedural synthesiser. Builds a USoundWaveProcedural on the fly so the
 * project ships with zero audio assets while still having readable feedback.
 */
class PARASITE_API FParasiteAudio
{
public:
	/** Plays at a world location (falls back to 2D when no location is given). */
	static void Play(const UObject* WorldContext, EParasiteSound Sound, const FVector& Location);

	/** Plays 2D for the local player (UI, warnings). */
	static void Play2D(const UObject* WorldContext, EParasiteSound Sound);
};
