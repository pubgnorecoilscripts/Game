#include "ParasiteGameMode.h"
#include "ParasiteGameState.h"
#include "ParasitePlayerState.h"
#include "ParasitePlayerController.h"
#include "ParasiteCharacter.h"
#include "ParasiteHUD.h"
#include "ParasiteNest.h"
#include "ParasiteNPC.h"
#include "PossessablePawn.h"
#include "PossessableComponent.h"
#include "MallBuilder.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/GameSession.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Containers/StringConv.h"

AParasiteGameMode::AParasiteGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	GameStateClass = AParasiteGameState::StaticClass();
	PlayerStateClass = AParasitePlayerState::StaticClass();
	PlayerControllerClass = AParasitePlayerController::StaticClass();
	DefaultPawnClass = AParasiteCharacter::StaticClass();
	HUDClass = AParasiteHUD::StaticClass();

	bUseSeamlessTravel = false;
}

AParasiteGameMode* AParasiteGameMode::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetAuthGameMode<AParasiteGameMode>() : nullptr;
}

void AParasiteGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	MatchSeed = FMath::Rand();
	if (GameSession)
	{
		GameSession->MaxPlayers = Sim.GetRules().MaxPlayers;
	}
}

void AParasiteGameMode::BeginPlay()
{
	Super::BeginPlay();

	UMallBuilder::SpawnGameplayActors(GetWorld());

	// Five starts per team, taken from the mall layout.
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

	// The restricted areas that pay an infiltration bonus: each team's back of house.
	FVector Min, Max;
	UMallBuilder::GetRestrictedZone(EParasiteTeam::TeamA, Min, Max);
	Sim.SetRestrictedZone(Parasite::ETeam::A, ToCore(Min), ToCore(Max));
	UMallBuilder::GetRestrictedZone(EParasiteTeam::TeamB, Min, Max);
	Sim.SetRestrictedZone(Parasite::ETeam::B, ToCore(Min), ToCore(Max));

	SpawnNests();
}

// ---------------------------------------------------------------------------
// Host registry
// ---------------------------------------------------------------------------

void AParasiteGameMode::RegisterHost(UPossessableComponent* Host)
{
	if (!Host || !Host->GetOwner() || Host->GetHostId() != Parasite::InvalidHost)
	{
		return;
	}
	const Parasite::HostId Id = Sim.AddHost(
		ToCore(Host->HostType), ToCore(Host->Mobility),
		TCHAR_TO_UTF8(*Host->HostDisplayName),
		ToCore(Host->GetOwner()->GetActorLocation()));

	Host->SetHostId(Id);
	HostComponents.Add(Id, Host);
}

void AParasiteGameMode::UnregisterHost(UPossessableComponent* Host)
{
	if (!Host || Host->GetHostId() == Parasite::InvalidHost)
	{
		return;
	}
	const Parasite::HostId Id = Host->GetHostId();

	// The simulation spits out anybody riding it; the world catches up on the
	// next tick through ApplyPossessionChanges.
	Sim.RemoveHost(Id);
	HostComponents.Remove(Id);
	Host->SetHostId(Parasite::InvalidHost);
}

UPossessableComponent* AParasiteGameMode::FindHostComponent(Parasite::HostId HostId) const
{
	const TObjectPtr<UPossessableComponent>* Found = HostComponents.Find(HostId);
	return Found ? Found->Get() : nullptr;
}

AParasitePlayerController* AParasiteGameMode::FindController(Parasite::PlayerId PlayerId) const
{
	const TObjectPtr<AParasitePlayerController>* Found = ControllerByPlayer.Find(PlayerId);
	return Found ? Found->Get() : nullptr;
}

// ---------------------------------------------------------------------------
// Joining and leaving
// ---------------------------------------------------------------------------

void AParasiteGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	AParasitePlayerController* PC = Cast<AParasitePlayerController>(NewPlayer);
	AParasitePlayerState* PS = PC ? PC->GetParasitePlayerState() : nullptr;
	if (!PC || !PS)
	{
		return;
	}

	const Parasite::PlayerId Id = Sim.AddPlayer(TCHAR_TO_UTF8(*PS->GetPlayerName()));
	if (Id == Parasite::InvalidPlayer)
	{
		// The simulation refused the join: the match is already full.
		if (GameSession)
		{
			GameSession->KickPlayer(PC, FText::FromString(TEXT("Server is full (10 players).")));
		}
		return;
	}

	PC->SetSimPlayerId(Id);
	ControllerByPlayer.Add(Id, PC);
	if (const Parasite::FPlayer* SimPlayer = Sim.FindPlayer(Id))
	{
		PS->Team = FromCore(SimPlayer->Team);
	}

	// Late joiners drop straight into the running match.
	if (Sim.GetPhase() == Parasite::EMatchPhase::InProgress)
	{
		RestartPlayer(PC);
	}
}

void AParasiteGameMode::Logout(AController* Exiting)
{
	if (AParasitePlayerController* PC = Cast<AParasitePlayerController>(Exiting))
	{
		const Parasite::PlayerId Id = PC->GetSimPlayerId();
		if (Id != Parasite::InvalidPlayer)
		{
			// Frees whatever they were riding and releases anybody riding them.
			Sim.RemovePlayer(Id);
			ControllerByPlayer.Remove(Id);
			LastKnownHost.Remove(Id);
			ApplyPossessionChanges();
		}
		if (AParasiteCharacter* Body = PC->ParasiteBody.Get())
		{
			Body->Destroy();
		}
	}
	Super::Logout(Exiting);
}

AActor* AParasiteGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const AParasitePlayerState* PS = Player ? Player->GetPlayerState<AParasitePlayerState>() : nullptr;
	const EParasiteTeam Team = PS ? PS->Team : EParasiteTeam::TeamA;

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
		// Leave any host and drop the stale body before making a new one.
		if (PC->GetSimPlayerId() != Parasite::InvalidPlayer)
		{
			Sim.ExitPossession(PC->GetSimPlayerId(), false);
			ApplyPossessionChanges();
		}
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
			Body->ApplyTeamColour(PC->GetParasitePlayerState() ? PC->GetParasitePlayerState()->Team : EParasiteTeam::None);
			Sim.SetPlayerLocation(PC->GetSimPlayerId(), ToCore(Body->GetActorLocation()));

			// Point the body's component at the host the simulation already made
			// for this player, so hijack timers and scans replicate through it.
			if (const Parasite::FPlayer* SimPlayer = Sim.FindPlayer(PC->GetSimPlayerId()))
			{
				if (UPossessableComponent* BodyHost = Body->Possessable)
				{
					BodyHost->SetHostId(SimPlayer->BodyHost);
					HostComponents.Add(SimPlayer->BodyHost, BodyHost);
				}
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
			RestartPlayer(PC);
		}
	}
}

// ---------------------------------------------------------------------------
// The simulation loop
// ---------------------------------------------------------------------------

void AParasiteGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PushWorldIntoSim();
	Sim.Tick(DeltaSeconds);
	ApplyPossessionChanges();
	PullSimIntoWorld();
	SyncPhase();
}

void AParasiteGameMode::PushWorldIntoSim()
{
	// Hosts move around the mall; the simulation needs to know where they are to
	// judge range, scans and nest presence.
	for (const TPair<int32, TObjectPtr<UPossessableComponent>>& Pair : HostComponents)
	{
		if (const UPossessableComponent* Host = Pair.Value.Get())
		{
			if (const AActor* HostActor = Host->GetOwner())
			{
				Sim.SetHostLocation(Pair.Key, ToCore(HostActor->GetActorLocation()));
			}
		}
	}
	// A player's parasite body is a host too, but it is spawned by the game mode
	// rather than registered, so push it separately.
	for (const TPair<int32, TObjectPtr<AParasitePlayerController>>& Pair : ControllerByPlayer)
	{
		const AParasitePlayerController* PC = Pair.Value.Get();
		if (PC && PC->ParasiteBody)
		{
			Sim.SetPlayerLocation(Pair.Key, ToCore(PC->ParasiteBody->GetActorLocation()));
		}
	}
}

void AParasiteGameMode::ApplyPossessionChanges()
{
	// The simulation has already decided who is riding what. All that is left is
	// to make the Unreal side agree: swap pawns, hide bodies, move cameras.
	for (const Parasite::FPlayer& SimPlayer : Sim.GetPlayers())
	{
		AParasitePlayerController* PC = FindController(SimPlayer.Id);
		if (!PC)
		{
			continue;
		}
		const int32 NewHost = SimPlayer.CurrentHost;
		const int32* Previous = LastKnownHost.Find(SimPlayer.Id);
		const int32 OldHost = Previous ? *Previous : Parasite::InvalidHost;
		if (NewHost == OldHost)
		{
			continue;
		}
		LastKnownHost.Add(SimPlayer.Id, NewHost);

		if (OldHost != Parasite::InvalidHost)
		{
			ExitHostInWorld(PC, FindHostComponent(OldHost));
		}
		if (NewHost != Parasite::InvalidHost)
		{
			EnterHostInWorld(PC, FindHostComponent(NewHost));
		}
	}

	// A player whose body is being ridden by somebody else.
	for (const Parasite::FPlayer& SimPlayer : Sim.GetPlayers())
	{
		AParasitePlayerController* PC = FindController(SimPlayer.Id);
		const Parasite::FHost* Body = PC ? Sim.FindHost(SimPlayer.BodyHost) : nullptr;
		if (!PC || !Body || !PC->ParasiteBody)
		{
			continue;
		}
		PC->ParasiteBody->SetHijacked(Body->bPossessed, Body->ResistProgress);
	}
}

void AParasiteGameMode::EnterHostInWorld(AParasitePlayerController* Controller, UPossessableComponent* Host)
{
	if (!Controller || !Host || !Host->GetOwner())
	{
		return;
	}
	AActor* HostActor = Host->GetOwner();
	Host->bPossessed = true;
	Host->Rider = Controller->GetParasitePlayerState();

	if (AParasiteCharacter* Victim = Cast<AParasiteCharacter>(HostActor))
	{
		// An enemy player is driven in place: the victim keeps their controller,
		// their connection and their state. Only the steering changes hands.
		Controller->SetHijackVictim(Victim);
		if (Controller->ParasiteBody)
		{
			Controller->ParasiteBody->SetDormant(true, FVector::ZeroVector);
		}
		Controller->SetViewTargetWithBlend(Victim, 0.25f);

		if (AParasitePlayerController* VictimPC = Cast<AParasitePlayerController>(Victim->GetController()))
		{
			VictimPC->ClientShowNotice(TEXT("!! SOMETHING IS INSIDE YOU - MASH [R] TO RESIST !!"), 4.f);
			VictimPC->ClientPlayCue(static_cast<uint8>(EParasiteSound::Detected));
		}
	}
	else if (APawn* HostPawn = Cast<APawn>(HostActor))
	{
		// Objects, NPCs and vehicles hand their pawn over for real.
		if (Controller->ParasiteBody)
		{
			Controller->ParasiteBody->SetDormant(true, FVector::ZeroVector);
		}
		if (AController* Existing = HostPawn->GetController())
		{
			Existing->UnPossess();		// take the wheel from an NPC's AI
		}
		Controller->UnPossess();
		Controller->Possess(HostPawn);
	}

	Controller->ClientShowNotice(FString::Printf(TEXT("POSSESSED: %s"), *Host->HostDisplayName), 2.f);
	Controller->ClientPlayCue(static_cast<uint8>(EParasiteSound::Possess));
	FParasiteAudio::Play(this, EParasiteSound::Possess, HostActor->GetActorLocation());
}

void AParasiteGameMode::ExitHostInWorld(AParasitePlayerController* Controller, UPossessableComponent* Host)
{
	if (!Controller)
	{
		return;
	}
	FVector WakeLocation = Controller->ParasiteBody ? Controller->ParasiteBody->GetActorLocation() : FVector::ZeroVector;
	AActor* HostActor = Host ? Host->GetOwner() : nullptr;

	if (Host)
	{
		Host->bPossessed = false;
		Host->Rider = nullptr;
		Host->PossessionTimeRemaining = 0.f;
	}
	if (IsValid(HostActor))
	{
		WakeLocation = HostActor->GetActorLocation() + FVector(0.f, 0.f, 60.f);

		if (AParasiteCharacter* Victim = Cast<AParasiteCharacter>(HostActor))
		{
			// Pop out beside the victim, not inside them.
			WakeLocation = Victim->GetActorLocation() - Victim->GetActorForwardVector() * 120.f + FVector(0.f, 0.f, 40.f);
		}
		else if (AParasiteNPC* NPC = Cast<AParasiteNPC>(HostActor))
		{
			if (Controller->GetPawn() == NPC)
			{
				Controller->UnPossess();
			}
			if (!NPC->GetController())
			{
				NPC->SpawnDefaultController();		// hand it back its own dim brain
			}
		}
	}
	Controller->SetHijackVictim(nullptr);

	if (AParasiteCharacter* Body = Controller->ParasiteBody.Get())
	{
		Body->SetDormant(false, WakeLocation);
		if (Controller->GetPawn() != Body)
		{
			Controller->UnPossess();
			Controller->Possess(Body);
		}
		Controller->SetViewTargetWithBlend(Body, 0.2f);
		Sim.SetPlayerLocation(Controller->GetSimPlayerId(), ToCore(WakeLocation));
	}

	Controller->ClientPlayCue(static_cast<uint8>(EParasiteSound::PossessExit));
	if (IsValid(HostActor))
	{
		FParasiteAudio::Play(this, EParasiteSound::PossessExit, HostActor->GetActorLocation());
	}
}

void AParasiteGameMode::PullSimIntoWorld()
{
	const float Now = Sim.GetTime();

	// Hosts: possession clock and scan reveal.
	for (const TPair<int32, TObjectPtr<UPossessableComponent>>& Pair : HostComponents)
	{
		UPossessableComponent* Host = Pair.Value.Get();
		const Parasite::FHost* SimHost = Sim.FindHost(Pair.Key);
		if (!Host || !SimHost)
		{
			continue;
		}
		Host->PossessionTimeRemaining = SimHost->bPossessed ? FMath::Max(0.f, SimHost->PossessionEndTime - Now) : 0.f;

		const bool bRevealed = Sim.GetRevealEndTime(Pair.Key) > Now;
		if (Host->bRevealed != bRevealed)
		{
			Host->bRevealed = bRevealed;
			Host->OnRep_Revealed();		// the server needs the visual too
		}
	}

	// Players: the replicated scoreboard and HUD values.
	for (const Parasite::FPlayer& SimPlayer : Sim.GetPlayers())
	{
		AParasitePlayerController* PC = FindController(SimPlayer.Id);
		AParasitePlayerState* PS = PC ? PC->GetParasitePlayerState() : nullptr;
		if (!PS)
		{
			continue;
		}
		PS->Team = FromCore(SimPlayer.Team);
		PS->DNA = SimPlayer.DNA;
		PS->LifetimeDNA = SimPlayer.LifetimeDNA;
		PS->InfectionTicks = SimPlayer.InfectionTicks;
		PS->PossessCooldownRemaining = FMath::Max(0.f, SimPlayer.PossessReadyTime - Now);
		PS->ScanCooldownRemaining = FMath::Max(0.f, SimPlayer.ScanReadyTime - Now);
		PS->NumUpgrades = static_cast<int32>(SimPlayer.Upgrades.size());

		PS->Upgrades.Reset();
		for (Parasite::EUpgrade Upgrade : SimPlayer.Upgrades)
		{
			PS->Upgrades.Add(static_cast<EParasiteUpgrade>(Upgrade));
		}

		UPossessableComponent* Host = FindHostComponent(SimPlayer.CurrentHost);
		PS->CurrentHost = Host ? Host->GetOwner() : nullptr;
		PS->CurrentHostName = Host ? Host->HostDisplayName : FString();
		PS->HostTimeRemaining = Host ? Host->PossessionTimeRemaining : 0.f;
	}

	// Match state.
	if (AParasiteGameState* GS = GetGameState<AParasiteGameState>())
	{
		GS->Phase = FromCore(Sim.GetPhase());
		GS->PhaseTimeRemaining = Sim.GetPhaseTimeRemaining();
		GS->NestAInfection = Sim.GetNest(Parasite::ETeam::A).Infection;
		GS->NestBInfection = Sim.GetNest(Parasite::ETeam::B).Infection;
		GS->WinningTeam = FromCore(Sim.GetWinner());
		GS->ResultReason = UTF8_TO_TCHAR(Sim.GetResultReason().c_str());
	}

	// Nests.
	for (AParasiteNest* Nest : Nests)
	{
		if (!IsValid(Nest))
		{
			continue;
		}
		const Parasite::FNest& SimNest = Sim.GetNest(ToCore(Nest->OwningTeam));
		Nest->Infection = SimNest.Infection;
		Nest->Health = SimNest.Health;
		Nest->bUnderAttack = SimNest.bUnderAttack;
		if (SimNest.bPulsedThisTick)
		{
			FParasiteAudio::Play(this, EParasiteSound::NestDamage, Nest->GetActorLocation());
		}
	}
}

void AParasiteGameMode::SyncPhase()
{
	const EMatchPhase Phase = FromCore(Sim.GetPhase());
	if (Phase == LastPhase)
	{
		return;
	}
	LastPhase = Phase;

	switch (Phase)
	{
	case EMatchPhase::InProgress:
		SpawnNests();
		SpawnAllPlayers();
		BroadcastSound(EParasiteSound::MatchStart);
		break;

	case EMatchPhase::PostMatch:
		BroadcastSound(EParasiteSound::MatchEnd);
		break;

	default:
		break;
	}
}

void AParasiteGameMode::SpawnNests()
{
	for (AParasiteNest* Nest : Nests)
	{
		if (IsValid(Nest))
		{
			Nest->Destroy();
		}
	}
	Nests.Reset();
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
			Nests.Add(Nest);
			Sim.SetNestLocation(ToCore(Team), ToCore(Location));
		}
	}
}

void AParasiteGameMode::BroadcastSound(EParasiteSound Sound)
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AParasitePlayerController* PC = Cast<AParasitePlayerController>(It->Get()))
		{
			PC->ClientPlayCue(static_cast<uint8>(Sound));
		}
	}
}

// ---------------------------------------------------------------------------
// Player requests
// ---------------------------------------------------------------------------

void AParasiteGameMode::RequestPossess(AParasitePlayerController* Controller)
{
	if (!Controller)
	{
		return;
	}
	Parasite::HostId Chosen = Parasite::InvalidHost;
	const Parasite::EPossessResult Result = Sim.TryPossessNearest(Controller->GetSimPlayerId(), Chosen);
	if (Result != Parasite::EPossessResult::Success)
	{
		Controller->ClientShowNotice(UTF8_TO_TCHAR(Parasite::ToString(Result)), 1.5f);
		Controller->ClientPlayCue(static_cast<uint8>(EParasiteSound::PossessFail));
		return;
	}
	ApplyPossessionChanges();
}

void AParasiteGameMode::RequestExit(AParasitePlayerController* Controller)
{
	if (Controller && Sim.ExitPossession(Controller->GetSimPlayerId(), false))
	{
		Controller->ClientShowNotice(TEXT("LEFT HOST"), 1.5f);
		ApplyPossessionChanges();
	}
}

void AParasiteGameMode::RequestInteract(AParasitePlayerController* Controller)
{
	// Doors are opened rather than possessed, so a disguised player can move
	// through the mall without breaking character.
	if (!Controller)
	{
		return;
	}
	const FVector Origin = FromCore(Sim.GetPlayerPresence(Controller->GetSimPlayerId()));
	for (const TPair<int32, TObjectPtr<UPossessableComponent>>& Pair : HostComponents)
	{
		UPossessableComponent* Host = Pair.Value.Get();
		if (!Host || Host->Mobility != EHostMobility::Hinge || !Host->GetOwner())
		{
			continue;
		}
		if (FVector::Dist(Host->GetOwner()->GetActorLocation(), Origin) <= 260.f)
		{
			if (APossessablePawn* Door = Cast<APossessablePawn>(Host->GetOwner()))
			{
				Door->ToggleHinge();
				Controller->ClientShowNotice(TEXT("DOOR"), 1.f);
				return;
			}
		}
	}
	RequestPossess(Controller);
}

void AParasiteGameMode::RequestScan(AParasitePlayerController* Controller)
{
	if (!Controller)
	{
		return;
	}
	std::vector<Parasite::HostId> Revealed;
	if (!Sim.TryScan(Controller->GetSimPlayerId(), Revealed))
	{
		Controller->ClientShowNotice(TEXT("SCAN ON COOLDOWN"), 1.5f);
		Controller->ClientPlayCue(static_cast<uint8>(EParasiteSound::PossessFail));
		return;
	}

	Controller->ClientPlayCue(static_cast<uint8>(EParasiteSound::ScanPulse));
	FParasiteAudio::Play(this, EParasiteSound::ScanPulse, FromCore(Sim.GetPlayerPresence(Controller->GetSimPlayerId())));

	for (Parasite::HostId Id : Revealed)
	{
		const Parasite::FHost* SimHost = Sim.FindHost(Id);
		if (!SimHost)
		{
			continue;
		}
		Controller->ClientAddMarker(FromCore(SimHost->Location), FColor(255, 25, 150), TEXT("PARASITE"), Sim.GetRules().ScanRevealTime);

		// The hunted feel the ping too. That is the whole mind game.
		if (AParasitePlayerController* Prey = FindController(SimHost->Rider))
		{
			Prey->ClientShowNotice(TEXT("YOU WERE SCANNED"), 1.5f);
			Prey->ClientPlayCue(static_cast<uint8>(EParasiteSound::Detected));
		}
	}
	const FString Summary = Revealed.empty()
		? FString(TEXT("SCAN: CLEAR"))
		: FString::Printf(TEXT("SCAN: %d CONTACT(S)"), static_cast<int32>(Revealed.size()));
	Controller->ClientShowNotice(Summary, 1.8f);
}

void AParasiteGameMode::RequestLeap(AParasitePlayerController* Controller, const FVector& Direction)
{
	AParasiteCharacter* Body = Controller ? Controller->ParasiteBody.Get() : nullptr;
	if (!Body || !Sim.TryLeap(Controller->GetSimPlayerId()))
	{
		return;
	}

	// Snap towards a host in leap range if the player is roughly facing one.
	FVector LaunchDir = Direction.GetSafeNormal();
	const FVector Origin = Body->GetActorLocation();
	float BestDistanceSq = FMath::Square(Sim.GetRules().LeapRange);
	for (const TPair<int32, TObjectPtr<UPossessableComponent>>& Pair : HostComponents)
	{
		const UPossessableComponent* Host = Pair.Value.Get();
		const Parasite::FHost* SimHost = Sim.FindHost(Pair.Key);
		if (!Host || !Host->GetOwner() || !SimHost || SimHost->bPossessed)
		{
			continue;
		}
		const FVector ToTarget = Host->GetOwner()->GetActorLocation() - Origin;
		if (ToTarget.SizeSquared() < BestDistanceSq && FVector::DotProduct(ToTarget.GetSafeNormal(), LaunchDir) > 0.35f)
		{
			BestDistanceSq = ToTarget.SizeSquared();
			LaunchDir = ToTarget.GetSafeNormal();
		}
	}

	Body->LaunchCharacter(LaunchDir * 900.f + FVector(0.f, 0.f, 420.f), true, true);
	FParasiteAudio::Play(this, EParasiteSound::ParasiteMove, Origin);
}

void AParasiteGameMode::RequestResist(AParasitePlayerController* Controller)
{
	if (!Controller)
	{
		return;
	}
	if (Sim.AddResist(Controller->GetSimPlayerId()))
	{
		Controller->ClientShowNotice(TEXT("YOU FORCED IT OUT"), 2.f);
		ApplyPossessionChanges();
	}
}

void AParasiteGameMode::RequestUpgrade(AParasitePlayerController* Controller, EParasiteUpgrade Upgrade)
{
	if (!Controller)
	{
		return;
	}
	const bool bBought = Sim.TryPurchaseUpgrade(Controller->GetSimPlayerId(), ToCore(Upgrade));
	Controller->ClientShowNotice(bBought ? TEXT("EVOLVED") : TEXT("CANNOT EVOLVE (DNA / LIMIT)"), 2.f);
	Controller->ClientPlayCue(static_cast<uint8>(bBought ? EParasiteSound::Possess : EParasiteSound::PossessFail));
}

void AParasiteGameMode::RequestPing(AParasitePlayerController* Controller, const FVector& Location)
{
	const AParasitePlayerState* PS = Controller ? Controller->GetParasitePlayerState() : nullptr;
	if (!PS)
	{
		return;
	}
	// A player controller is only relevant to its owner, so a multicast would not
	// reach the rest of the team: send each team mate their own copy.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AParasitePlayerController* Other = Cast<AParasitePlayerController>(It->Get());
		const AParasitePlayerState* OtherPS = Other ? Other->GetParasitePlayerState() : nullptr;
		if (OtherPS && OtherPS->Team == PS->Team)
		{
			Other->ClientAddMarker(Location, FColor(255, 220, 60), TEXT("PING"), 8.f);
			Other->ClientPlayCue(static_cast<uint8>(EParasiteSound::UIClick));
		}
	}
}

void AParasiteGameMode::RequestRematch()
{
	Sim.RestartMatch();
	ApplyPossessionChanges();
}
