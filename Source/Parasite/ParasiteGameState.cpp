#include "ParasiteGameState.h"
#include "ParasitePlayerState.h"
#include "MallBuilder.h"
#include "Net/UnrealNetwork.h"

AParasiteGameState::AParasiteGameState()
{
}

void AParasiteGameState::BeginPlay()
{
	Super::BeginPlay();
	UMallBuilder::BuildStaticGeometry(GetWorld());
}

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

float AParasiteGameState::GetInfectionByTeam(EParasiteTeam Team) const
{
	// Team A attacks nest B and vice versa.
	return (Team == EParasiteTeam::TeamA) ? NestBInfection : (Team == EParasiteTeam::TeamB ? NestAInfection : 0.f);
}

int32 AParasiteGameState::GetTeamDNA(EParasiteTeam Team) const
{
	int32 Total = 0;
	for (const APlayerState* PS : PlayerArray)
	{
		if (const AParasitePlayerState* Parasite = Cast<AParasitePlayerState>(PS))
		{
			if (Parasite->GetTeam() == Team)
			{
				Total += Parasite->LifetimeDNA;
			}
		}
	}
	return Total;
}

int32 AParasiteGameState::GetTeamPlayerCount(EParasiteTeam Team) const
{
	int32 Count = 0;
	for (const APlayerState* PS : PlayerArray)
	{
		const AParasitePlayerState* Parasite = Cast<AParasitePlayerState>(PS);
		if (Parasite && Parasite->GetTeam() == Team)
		{
			++Count;
		}
	}
	return Count;
}
