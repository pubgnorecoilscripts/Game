#include "ParasiteNPC.h"
#include "PossessableComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Materials/Material.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "AIController.h"
#include "Net/UnrealNetwork.h"

AParasiteNPC::AParasiteNPC()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterial> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	BodyMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.3f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMesh.Object);
	}
	if (BasicMaterial.Succeeded())
	{
		BodyMesh->SetMaterial(0, BasicMaterial.Object);
	}

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(RootComponent);
	HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 65.f));
	HeadMesh->SetRelativeScale3D(FVector(0.42f, 0.42f, 0.42f));
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (SphereMesh.Succeeded())
	{
		HeadMesh->SetStaticMesh(SphereMesh.Object);
	}
	if (BasicMaterial.Succeeded())
	{
		HeadMesh->SetMaterial(0, BasicMaterial.Object);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 380.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 70.f);
	CameraBoom->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	Possessable = CreateDefaultSubobject<UPossessableComponent>(TEXT("Possessable"));
	Possessable->HostType = EHostType::NPC;
	Possessable->Mobility = EHostMobility::Walk;
	Possessable->HostDisplayName = TEXT("NPC");
	Possessable->CameraDistance = 380.f;
	Possessable->CameraHeight = 70.f;

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 420.f, 0.f);
	Move->MaxWalkSpeed = 240.f;
	Move->bUseControllerDesiredRotation = false;
}

void AParasiteNPC::BeginPlay()
{
	Super::BeginPlay();
	if (BodyMesh && BodyMesh->GetMaterial(0))
	{
		BodyMaterial = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	OnRep_Colour();
	IdleTimer = FMath::FRandRange(0.f, 2.f);
}

void AParasiteNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AParasiteNPC, Waypoints);
	DOREPLIFETIME(AParasiteNPC, ShirtColour);
}

void AParasiteNPC::OnRep_Colour()
{
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), ShirtColour);
	}
}

void AParasiteNPC::SetRevealed(float Seconds)
{
	RevealEndTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) + Seconds;
}

void AParasiteNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bShouldReveal = Now < RevealEndTime;
	if (bShouldReveal != bRevealApplied && BodyMaterial)
	{
		bRevealApplied = bShouldReveal;
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), bShouldReveal ? FLinearColor(1.f, 0.1f, 0.6f) : ShirtColour);
	}

	// The server drives the idle brain; possessed NPCs are driven by their player.
	if (HasAuthority() && !IsPlayerControlled())
	{
		TickIdleBrain(DeltaSeconds);
	}
}

void AParasiteNPC::TickIdleBrain(float DeltaSeconds)
{
	if (Waypoints.Num() == 0)
	{
		return;
	}
	if (IdleTimer > 0.f)
	{
		IdleTimer -= DeltaSeconds;
		return;
	}

	const FVector Target = Waypoints[WaypointIndex % Waypoints.Num()];
	FVector ToTarget = Target - GetActorLocation();
	ToTarget.Z = 0.f;

	if (ToTarget.SizeSquared() < FMath::Square(90.f))
	{
		WaypointIndex = (WaypointIndex + 1) % Waypoints.Num();
		IdleTimer = FMath::FRandRange(1.f, 4.f);		// stop and look at nothing for a while
		StuckTimer = 0.f;
		return;
	}

	AddMovementInput(ToTarget.GetSafeNormal(), 1.f);

	// If a shopper walks into a bin for long enough, it just picks another target.
	StuckTimer = (GetVelocity().SizeSquared2D() < 400.f) ? StuckTimer + DeltaSeconds : 0.f;
	if (StuckTimer > 3.f)
	{
		StuckTimer = 0.f;
		WaypointIndex = (WaypointIndex + 1) % Waypoints.Num();
	}
}

void AParasiteNPC::DriveForward(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Value);
	}
}

void AParasiteNPC::DriveRight(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Value);
	}
}

