#pragma once

#include "CoreMinimal.h"

/** Every sound the prototype needs. All synthesised at runtime, no assets. */
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
 * A tiny procedural synthesiser, so the project ships with no audio assets and
 * still gives readable feedback.
 */
class PARASITE_API FParasiteAudio
{
public:
	static void Play(const UObject* WorldContext, EParasiteSound Sound, const FVector& Location);
	static void Play2D(const UObject* WorldContext, EParasiteSound Sound);
};
