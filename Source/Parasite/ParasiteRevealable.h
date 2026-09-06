#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ParasiteRevealable.generated.h"

UINTERFACE(MinimalAPI)
class UParasiteRevealable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by anything that can be lit up by a parasite scan. Keeps the
 * possession component from having to know what kinds of actor exist.
 */
class PARASITE_API IParasiteRevealable
{
	GENERATED_BODY()

public:
	virtual void OnRevealChanged(bool bRevealed) = 0;
};
