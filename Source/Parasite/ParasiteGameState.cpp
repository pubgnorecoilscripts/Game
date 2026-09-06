#include "ParasiteGameState.h"
#include "MallBuilder.h"
#include "Net/UnrealNetwork.h"

void AParasiteGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AParasiteGameState, Phase);
	DOREPLIFETIME(AParasiteGameState, PhaseTimeRemaining);
	DOREPLIFETIME(AParasiteGameState, NestAInfection);
	DOREPLIFETIME(AParasiteGameState, NestBInfection);
	DOREPLIFETIME(AParasiteGameState, WinningTeam);
	DOREPLIFETIME(AParasiteGameState, ResultReason);
}

void AParasiteGameState::BeginPlay()
{
	Super::BeginPlay();
	UMallBuilder::BuildStaticGeometry(GetWorld());
}

float AParasiteGameState::GetInfectionByTeam(EParasiteTeam Team) const
{
	// Team A attacks nest B and vice versa.
	if (Team == EParasiteTeam::TeamA)
	{
		return NestBInfection;
	}
	if (Team == EParasiteTeam::TeamB)
	{
		return NestAInfection;
	}
	return 0.f;
}
