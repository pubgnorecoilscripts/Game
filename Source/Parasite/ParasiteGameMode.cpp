#include "ParasiteGameMode.h"
#include "ParasiteGameState.h"
#include "ParasitePlayerState.h"
#include "ParasitePlayerController.h"
#include "ParasiteCharacter.h"
#include "ParasiteHUD.h"
#include "ParasiteNest.h"
#include "MallBuilder.h"
#include "PossessableComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/GameSession.h"
#include "EngineUtils.h"
#include "Engine/World.h"

AParasiteGameMode::AParasiteGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	GameStateClass = AParasiteGameState::StaticClass();
	PlayerStateClass = AParasitePlayerState::StaticClass();
	PlayerControllerClass = AParasitePlayerController::StaticClass();
	DefaultPawnClass = AParasiteCharacter::StaticClass();
	HUDClass = AParasiteHUD::StaticClass();

	bStartPlayersAsSpectators = false;
	bUseSeamlessTravel = false;
}

void AParasiteGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	MatchSeed = FMath::Rand();

	// 5v5 ceiling. Fewer players is fine, more is not.
	if (GameSession)
	{
		GameSession->MaxPlayers = ParasiteRules::MaxPlayers;
	}
}

void AParasiteGameMode::BeginPlay()
{
	Super::BeginPlay();

	UMallBuilder::SpawnGameplayActors(GetWorld());

	// Player starts, five per team, created from the mall layout.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		if (APlayerStart* StartA = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(),
			UMallBuilder::GetTeamSpawn(EParasiteTeam::TeamA, Index), FRotator(0.f, 0.f, 0.f), Params))
		{
			StartA->PlayerStartTag = FName("TeamA");
			TeamAStarts.Add(StartA);
		}
		if (APlayerStart* StartB = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(),
			UMallBuilder::GetTeamSpawn(EParasiteTeam::TeamB, Index), FRotator(0.f, 180.f, 0.f), Params))
		{
			StartB->PlayerStartTag = FName("TeamB");
			TeamBStarts.Add(StartB);
		}
	}

	EnterPhase(EMatchPhase::Lobby);
}

// ---------------------------------------------------------------------------
// Phases
// ---------------------------------------------------------------------------

void AParasiteGameMode::EnterPhase(EMatchPhase NewPhase)
{
	AParasiteGameState* GS = GetGameState<AParasiteGameState>();
	if (!GS)
	{
		return;
	}
	GS->Phase = NewPhase;

	switch (NewPhase)
	{
	case EMatchPhase::Lobby:
		GS->PhaseTimeRemaining = 0.f;
		GS->WinningTeam = EParasiteTeam::None;
		GS->ResultReason.Empty();
		break;

	case EMatchPhase::Countdown:
		GS->PhaseTimeRemaining = ParasiteRules::CountdownDuration;
		break;

	case EMatchPhase::InProgress:
		GS->PhaseTimeRemaining = ParasiteRules::MatchDuration;
		GS->NestAInfection = 0.f;
		GS->NestBInfection = 0.f;
		InfiltrationCredited.Reset();
		SpawnNests();
		SpawnAllPlayers();
		BroadcastSound(EParasiteSound::MatchStart);
		break;

	case EMatchPhase::PostMatch:
		GS->PhaseTimeRemaining = ParasiteRules::PostMatchDuration;
		BroadcastSound(EParasiteSound::MatchEnd);
		break;
	}
}

void AParasiteGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AParasiteGameState* GS = GetGameState<AParasiteGameState>();
	if (!GS)
	{
		return;
	}

	switch (GS->Phase)
	{
	case EMatchPhase::Lobby:
		// One player is enough to start: the prototype should never be unplayable
		// just because nobody else showed up.
		if (GetNumPlayers() >= 1)
		{
			EnterPhase(EMatchPhase::Countdown);
		}
		break;

	case EMatchPhase::Countdown:
		GS->PhaseTimeRemaining -= DeltaSeconds;
		if (GS->PhaseTimeRemaining <= 0.f)
		{
			EnterPhase(EMatchPhase::InProgress);
		}
		break;

	case EMatchPhase::InProgress:
		GS->PhaseTimeRemaining = FMath::Max(0.f, GS->PhaseTimeRemaining - DeltaSeconds);
		TickPossessionTimers();
		TickInfiltrationRewards(DeltaSeconds);
		EvaluateWinCondition();
		break;

	case EMatchPhase::PostMatch:
		GS->PhaseTimeRemaining -= DeltaSeconds;
		if (GS->PhaseTimeRemaining <= 0.f)
		{
			RestartMatch();
		}
		break;
	}
}

// ---------------------------------------------------------------------------
// Teams and spawning
// ---------------------------------------------------------------------------

void AParasiteGameMode::AssignTeam(AParasitePlayerState* PlayerState)
{
	AParasiteGameState* GS = GetGameState<AParasiteGameState>();
	if (!PlayerState || !GS || PlayerState->Team != EParasiteTeam::None)
	{
		return;
	}
	const int32 CountA = GS->GetTeamPlayerCount(EParasiteTeam::TeamA);
	const int32 CountB = GS->GetTeamPlayerCount(EParasiteTeam::TeamB);
	PlayerState->Team = (CountA <= CountB) ? EParasiteTeam::TeamA : EParasiteTeam::TeamB;
}

void AParasiteGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	AParasitePlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<AParasitePlayerState>() : nullptr;
	AssignTeam(PS);

	AParasiteGameState* GS = GetGameState<AParasiteGameState>();
	if (GS && GS->Phase == EMatchPhase::InProgress)
	{
		// Late joiner: drop them straight in.
		RestartPlayer(NewPlayer);
	}
}

void AParasiteGameMode::Logout(AController* Exiting)
{
	if (AParasitePlayerController* PC = Cast<AParasitePlayerController>(Exiting))
	{
		// Free whatever they were riding so the host is not stuck forever.
		PC->ServerExitPossession(false);
		if (AParasiteCharacter* Body = PC->ParasiteBody.Get())
		{
			Body->Destroy();
		}
	}
	if (AParasitePlayerState* PS = Exiting ? Exiting->GetPlayerState<AParasitePlayerState>() : nullptr)
	{
		InfiltrationCredited.Remove(PS);
	}
	Super::Logout(Exiting);
}

AActor* AParasiteGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const AParasitePlayerState* PS = Player ? Player->GetPlayerState<AParasitePlayerState>() : nullptr;
	const EParasiteTeam Team = PS ? PS->GetTeam() : EParasiteTeam::TeamA;

	TArray<TObjectPtr<APlayerStart>>& Starts = (Team == EParasiteTeam::TeamB) ? TeamBStarts : TeamAStarts;
	int32& Cursor = (Team == EParasiteTeam::TeamB) ? StartIndexB : StartIndexA;
	if (Starts.Num() > 0)
	{
		APlayerStart* Chosen = Starts[Cursor % Starts.Num()];
		++Cursor;
		if (IsValid(Chosen))
		{
			return Chosen;
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AParasiteGameMode::RestartPlayer(AController* NewPlayer)
{
	AParasitePlayerController* PC = Cast<AParasitePlayerController>(NewPlayer);
	if (PC)
	{
		// Drop any host and destroy the stale body before making a new one.
		PC->ServerExitPossession(false);
		if (AParasiteCharacter* OldBody = PC->ParasiteBody.Get())
		{
			if (PC->GetPawn() == OldBody)
			{
				PC->UnPossess();
			}
			OldBody->Destroy();
			PC->SetParasiteBody(nullptr);
		}
	}
	Super::RestartPlayer(NewPlayer);

	if (PC)
	{
		if (AParasiteCharacter* Body = Cast<AParasiteCharacter>(PC->GetPawn()))
		{
			PC->SetParasiteBody(Body);
			if (const AParasitePlayerState* PS = PC->GetParasitePlayerState())
			{
				Body->ApplyTeamColour(PS->GetTeam());
			}
		}
	}
}

void AParasiteGameMode::SpawnAllPlayers()
{
	StartIndexA = 0;
	StartIndexB = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AParasitePlayerController* PC = Cast<AParasitePlayerController>(It->Get()))
		{
			AssignTeam(PC->GetParasitePlayerState());
			RestartPlayer(PC);
		}
	}
}

// ---------------------------------------------------------------------------
// Nests
// ---------------------------------------------------------------------------

void AParasiteGameMode::ClearNests()
{
	for (TObjectPtr<AParasiteNest>& Nest : Nests)
	{
		if (IsValid(Nest))
		{
			Nest->Destroy();
		}
	}
	Nests.Reset();
}

void AParasiteGameMode::SpawnNests()
{
	ClearNests();
	MatchSeed = FMath::Rand();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 Index = 0; Index < 2; ++Index)
	{
		const EParasiteTeam Team = (Index == 0) ? EParasiteTeam::TeamA : EParasiteTeam::TeamB;
		const FVector Location = UMallBuilder::GetNestLocation(Team, MatchSeed + Index * 7);
		if (AParasiteNest* Nest = GetWorld()->SpawnActor<AParasiteNest>(AParasiteNest::StaticClass(), Location, FRotator::ZeroRotator, Params))
		{
			Nest->OwningTeam = Team;
			Nest->Infection = 0.f;
			Nest->Health = 100.f;
			Nests.Add(Nest);
		}
	}
}

// ---------------------------------------------------------------------------
// Scoring and win condition
// ---------------------------------------------------------------------------

void AParasiteGameMode::TickPossessionTimers()
{
	// The server owns every possession clock. Done here rather than in the
	// controller because PlayerTick is not guaranteed to run for remote clients.
	const float Now = GetWorld()->GetTimeSeconds();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AParasitePlayerController* PC = Cast<AParasitePlayerController>(It->Get());
		AParasitePlayerState* PS = PC ? PC->GetParasitePlayerState() : nullptr;
		if (!PS || !PS->CurrentHost)
		{
			continue;
		}
		const UPossessableComponent* Host = PS->CurrentHost->FindComponentByClass<UPossessableComponent>();
		if (!IsValid(PS->CurrentHost) || !Host || Now >= Host->PossessionEndTime)
		{
			PC->ServerExitPossession(false);
		}
	}
}

void AParasiteGameMode::TickInfiltrationRewards(float DeltaSeconds)
{
	InfiltrationTimer += DeltaSeconds;
	if (InfiltrationTimer < 1.f)
	{
		return;
	}
	InfiltrationTimer = 0.f;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AParasitePlayerController* PC = Cast<AParasitePlayerController>(It->Get());
		AParasitePlayerState* PS = PC ? PC->GetParasitePlayerState() : nullptr;
		if (!PS || PS->GetTeam() == EParasiteTeam::None || InfiltrationCredited.Contains(PS))
		{
			continue;
		}
		// Infiltration only counts while wearing a disguise - walking in as a
		// naked parasite is not infiltration, it is a stroll.
		const AActor* Body = PS->CurrentHost.Get();
		if (!Body)
		{
			continue;
		}
		if (UMallBuilder::IsRestrictedArea(Body->GetActorLocation(), PS->GetTeam()))
		{
			PS->AwardDNA(ParasiteRules::DNA_Infiltrate);
			InfiltrationCredited.Add(PS);
			PC->ClientNotify(TEXT("INFILTRATION BONUS"), 2.f);
		}
	}
}

void AParasiteGameMode::EvaluateWinCondition()
{
	AParasiteGameState* GS = GetGameState<AParasiteGameState>();
	if (!GS)
	{
		return;
	}

	if (GS->NestBInfection >= 100.f)
	{
		FinishMatch(EParasiteTeam::TeamA, TEXT("TEAM B NEST FULLY INFECTED"));
		return;
	}
	if (GS->NestAInfection >= 100.f)
	{
		FinishMatch(EParasiteTeam::TeamB, TEXT("TEAM A NEST FULLY INFECTED"));
		return;
	}
	if (GS->PhaseTimeRemaining > 0.f)
	{
		return;
	}

	// Time up: highest infection wins, DNA breaks the tie.
	const float InfectionByA = GS->NestBInfection;
	const float InfectionByB = GS->NestAInfection;
	if (!FMath::IsNearlyEqual(InfectionByA, InfectionByB, 0.01f))
	{
		const EParasiteTeam Winner = (InfectionByA > InfectionByB) ? EParasiteTeam::TeamA : EParasiteTeam::TeamB;
		FinishMatch(Winner, TEXT("TIME UP - HIGHEST INFECTION"));
		return;
	}

	const int32 DNAA = GS->GetTeamDNA(EParasiteTeam::TeamA);
	const int32 DNAB = GS->GetTeamDNA(EParasiteTeam::TeamB);
	if (DNAA == DNAB)
	{
		FinishMatch(EParasiteTeam::None, TEXT("TIME UP - DEAD HEAT"));
	}
	else
	{
		FinishMatch(DNAA > DNAB ? EParasiteTeam::TeamA : EParasiteTeam::TeamB, TEXT("TIME UP - DNA TIE BREAK"));
	}
}

void AParasiteGameMode::FinishMatch(EParasiteTeam Winner, const FString& Reason)
{
	AParasiteGameState* GS = GetGameState<AParasiteGameState>();
	if (!GS || GS->Phase == EMatchPhase::PostMatch)
	{
		return;
	}
	GS->WinningTeam = Winner;
	GS->ResultReason = Reason;

	// Everybody comes home.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AParasitePlayerController* PC = Cast<AParasitePlayerController>(It->Get()))
		{
			PC->ServerExitPossession(false);
		}
	}
	EnterPhase(EMatchPhase::PostMatch);
}

void AParasiteGameMode::BroadcastSound(EParasiteSound Sound)
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AParasitePlayerController* PC = Cast<AParasitePlayerController>(It->Get()))
		{
			PC->ClientPlaySound(static_cast<uint8>(Sound));
		}
	}
}

void AParasiteGameMode::RestartMatch()
{
	ClearNests();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AParasitePlayerController* PC = Cast<AParasitePlayerController>(It->Get()))
		{
			PC->ResetForNewMatch();
		}
	}
	EnterPhase(EMatchPhase::Countdown);
}
