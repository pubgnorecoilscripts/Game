#include "MallBuilder.h"
#include "PossessablePawn.h"
#include "ParasiteNPC.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------------------
// Layout. Team A lives west (-X), team B east (+X).
// ---------------------------------------------------------------------------
namespace MallLayout
{
	static constexpr float Half			= 10000.f;		// 100 m each way
	static constexpr float WallHeight	= 1400.f;
	static constexpr float UpperZ		= 700.f;
	static constexpr float WallThick	= 40.f;

	static const FLinearColor ColFloor		= FLinearColor(0.32f, 0.30f, 0.28f);
	static const FLinearColor ColWall		= FLinearColor(0.55f, 0.53f, 0.48f);
	static const FLinearColor ColAccent		= FLinearColor(0.20f, 0.55f, 0.60f);
	static const FLinearColor ColSign		= FLinearColor(0.95f, 0.30f, 0.45f);
	static const FLinearColor ColMetal		= FLinearColor(0.40f, 0.42f, 0.45f);
	static const FLinearColor ColPlanter	= FLinearColor(0.25f, 0.18f, 0.14f);
}

using namespace MallLayout;

// ---------------------------------------------------------------------------
// AMallBlock
// ---------------------------------------------------------------------------

AMallBlock::AMallBlock()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterial> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	if (BasicMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, BasicMaterial.Object);
	}
	SphereAsset = SphereMesh.Succeeded() ? SphereMesh.Object : nullptr;
}

void AMallBlock::Build(const FVector& Extent, const FLinearColor& Colour, bool bUseSphere)
{
	if (bUseSphere && SphereAsset)
	{
		Mesh->SetStaticMesh(SphereAsset);
	}
	Mesh->SetWorldScale3D(FVector(
		FMath::Max(Extent.X, 1.f), FMath::Max(Extent.Y, 1.f), FMath::Max(Extent.Z, 1.f)) / 50.f);

	if (UMaterialInstanceDynamic* Dynamic = Mesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Dynamic->SetVectorParameterValue(TEXT("Color"), Colour);
	}
}

// ---------------------------------------------------------------------------
// AMallLighting
// ---------------------------------------------------------------------------

AMallLighting::AMallLighting()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-55.f, 35.f, 0.f));
	Sun->SetIntensity(2.4f);
	Sun->SetLightColor(FLinearColor(1.f, 0.94f, 0.82f));

	Sky = CreateDefaultSubobject<USkyLightComponent>(TEXT("Sky"));
	Sky->SetupAttachment(Root);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SourceType = ESkyLightSourceType::SLS_CapturedScene;
	Sky->bRealTimeCapture = true;
	Sky->bLowerHemisphereIsBlack = false;
	Sky->SetIntensity(1.2f);
	Sky->SetLightColor(FLinearColor(0.55f, 0.6f, 0.75f));

	// A cheap fill so nothing is ever pitch black, whatever the sky capture does.
	Fill = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Fill"));
	Fill->SetupAttachment(Root);
	Fill->SetMobility(EComponentMobility::Movable);
	Fill->SetRelativeRotation(FRotator(-40.f, 210.f, 0.f));
	Fill->SetIntensity(1.1f);
	Fill->SetLightColor(FLinearColor(0.6f, 0.7f, 0.9f));
	Fill->SetCastShadows(false);
}

// ---------------------------------------------------------------------------
// AMallElevator
// ---------------------------------------------------------------------------

AMallElevator::AMallElevator()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterial> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Platform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Platform"));
	SetRootComponent(Platform);
	Platform->SetMobility(EComponentMobility::Movable);
	Platform->SetCollisionProfileName(TEXT("BlockAll"));
	Platform->SetWorldScale3D(FVector(6.f, 6.f, 0.4f));
	if (CubeMesh.Succeeded())
	{
		Platform->SetStaticMesh(CubeMesh.Object);
	}
	if (BasicMaterial.Succeeded())
	{
		Platform->SetMaterial(0, BasicMaterial.Object);
	}
}

void AMallElevator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	if (WaitTimer > 0.f)
	{
		WaitTimer -= DeltaSeconds;
		return;
	}

	FVector Location = GetActorLocation();
	Location.Z += Direction * 160.f * DeltaSeconds;
	if (Location.Z >= TopZ)
	{
		Location.Z = TopZ;
		Direction = -1.f;
		WaitTimer = 4.f;
	}
	else if (Location.Z <= BottomZ)
	{
		Location.Z = BottomZ;
		Direction = 1.f;
		WaitTimer = 4.f;
	}
	SetActorLocation(Location, true);
}

// ---------------------------------------------------------------------------
// Build helpers
// ---------------------------------------------------------------------------

namespace
{
	AMallBlock* Block(UWorld* World, const FVector& Centre, const FVector& Extent, const FLinearColor& Colour,
		float Yaw = 0.f, float Pitch = 0.f, bool bSphere = false)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AMallBlock* NewBlock = World->SpawnActor<AMallBlock>(AMallBlock::StaticClass(), Centre, FRotator(Pitch, Yaw, 0.f), Params);
		if (NewBlock)
		{
			NewBlock->Build(Extent, Colour, bSphere);
		}
		return NewBlock;
	}

	/** Three walls, a floor tint and a fascia sign: one shop unit. */
	void BuildShop(UWorld* World, const FVector& Centre, float Width, float Depth, float FacingYaw, const FLinearColor& Colour)
	{
		const FRotationMatrix Rotation(FRotator(0.f, FacingYaw, 0.f));
		const FVector Forward = Rotation.GetUnitAxis(EAxis::X);		// points into the atrium
		const FVector Side = Rotation.GetUnitAxis(EAxis::Y);

		const float HalfW = Width * 0.5f;
		const float HalfD = Depth * 0.5f;
		const FVector Up(0.f, 0.f, WallHeight * 0.5f);

		Block(World, Centre - Forward * HalfD + Up, FVector(WallThick, HalfW, WallHeight * 0.5f), Colour, FacingYaw);
		Block(World, Centre + Side * HalfW + Up, FVector(HalfD, WallThick, WallHeight * 0.5f), Colour, FacingYaw);
		Block(World, Centre - Side * HalfW + Up, FVector(HalfD, WallThick, WallHeight * 0.5f), Colour, FacingYaw);
		Block(World, Centre + FVector(0.f, 0.f, 6.f), FVector(HalfD, HalfW, 6.f), Colour * 0.7f, FacingYaw);
		Block(World, Centre + Forward * HalfD + FVector(0.f, 0.f, WallHeight - 120.f),
			FVector(30.f, HalfW * 0.8f, 120.f), ColSign, FacingYaw);
	}

	/** A ramp standing in for a dead escalator. */
	void BuildStairs(UWorld* World, const FVector& Bottom, float Yaw)
	{
		const float Run = 1600.f;
		const float Rise = UpperZ;
		const float Pitch = -FMath::RadiansToDegrees(FMath::Atan2(Rise, Run));
		const FVector Forward = FRotationMatrix(FRotator(0.f, Yaw, 0.f)).GetUnitAxis(EAxis::X);

		Block(World, Bottom + Forward * (Run * 0.5f) + FVector(0.f, 0.f, Rise * 0.5f),
			FVector(FMath::Sqrt(Run * Run + Rise * Rise) * 0.5f, 260.f, 25.f), ColMetal, Yaw, Pitch);
		Block(World, Bottom + Forward * (Run + 300.f) + FVector(0.f, 0.f, Rise),
			FVector(300.f, 300.f, 25.f), ColMetal, Yaw);
	}

	APossessablePawn* SpawnProp(UWorld* World, const FVector& Location, float Yaw, const FString& Name,
		EHostType Type, EHostMobility Mobility, const FVector& Extent, const FLinearColor& Colour,
		const FVector& DetailOffset = FVector::ZeroVector, const FVector& DetailExtent = FVector::ZeroVector)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APossessablePawn* Prop = World->SpawnActor<APossessablePawn>(APossessablePawn::StaticClass(),
			Location + FVector(0.f, 0.f, Extent.Z), FRotator(0.f, Yaw, 0.f), Params);
		if (Prop)
		{
			Prop->Configure(Name, Type, Mobility, Extent, Colour, DetailOffset, DetailExtent);
		}
		return Prop;
	}

	void SpawnShopper(UWorld* World, const TArray<FVector>& Route, const FLinearColor& Colour)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (AParasiteNPC* NPC = World->SpawnActor<AParasiteNPC>(AParasiteNPC::StaticClass(),
			Route[0] + FVector(0.f, 0.f, 95.f), FRotator::ZeroRotator, Params))
		{
			NPC->Waypoints = Route;
			NPC->ShirtColour = Colour;
			NPC->OnRep_Colour();
		}
	}
}

// ---------------------------------------------------------------------------
// Static geometry
// ---------------------------------------------------------------------------

void UMallBuilder::BuildStaticGeometry(UWorld* World)
{
	if (!World)
	{
		return;
	}
	// Never build the mall twice into one world.
	for (TActorIterator<AMallBlock> It(World); It; ++It)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<AMallLighting>(AMallLighting::StaticClass(), FVector(0.f, 0.f, 2000.f), FRotator::ZeroRotator, Params);

	// --- Shell -----------------------------------------------------------
	Block(World, FVector(0.f, 0.f, -50.f), FVector(Half, Half, 50.f), ColFloor);
	Block(World, FVector(0.f, 0.f, WallHeight * 2.f), FVector(Half, Half, 40.f), ColWall * 0.6f);
	Block(World, FVector(Half, 0.f, WallHeight), FVector(WallThick, Half, WallHeight), ColWall);
	Block(World, FVector(-Half, 0.f, WallHeight), FVector(WallThick, Half, WallHeight), ColWall);
	Block(World, FVector(0.f, Half, WallHeight), FVector(Half, WallThick, WallHeight), ColWall);
	Block(World, FVector(0.f, -Half, WallHeight), FVector(Half, WallThick, WallHeight), ColWall);

	// --- Upper floor: a ring around the open atrium ----------------------
	const float RingOuter = 8000.f;
	const float RingInner = 3600.f;
	const float RingWidth = (RingOuter - RingInner) * 0.5f;
	const float RingMid = (RingOuter + RingInner) * 0.5f;
	Block(World, FVector(RingMid, 0.f, UpperZ), FVector(RingWidth, RingOuter, 25.f), ColFloor * 1.1f);
	Block(World, FVector(-RingMid, 0.f, UpperZ), FVector(RingWidth, RingOuter, 25.f), ColFloor * 1.1f);
	Block(World, FVector(0.f, RingMid, UpperZ), FVector(RingInner, RingWidth, 25.f), ColFloor * 1.1f);
	Block(World, FVector(0.f, -RingMid, UpperZ), FVector(RingInner, RingWidth, 25.f), ColFloor * 1.1f);
	Block(World, FVector(RingInner, 0.f, UpperZ + 90.f), FVector(20.f, RingInner, 90.f), ColAccent);
	Block(World, FVector(-RingInner, 0.f, UpperZ + 90.f), FVector(20.f, RingInner, 90.f), ColAccent);
	Block(World, FVector(0.f, RingInner, UpperZ + 90.f), FVector(RingInner, 20.f, 90.f), ColAccent);
	Block(World, FVector(0.f, -RingInner, UpperZ + 90.f), FVector(RingInner, 20.f, 90.f), ColAccent);

	// --- Atrium centrepiece: a dead fountain ------------------------------
	Block(World, FVector(0.f, 0.f, 40.f), FVector(600.f, 600.f, 40.f), ColAccent * 0.8f);
	Block(World, FVector(0.f, 0.f, 160.f), FVector(180.f, 180.f, 160.f), ColAccent, 0.f, 0.f, true);

	// --- Eight shop units --------------------------------------------------
	BuildShop(World, FVector(6200.f, -5200.f, 0.f), 2600.f, 2400.f, 180.f, FLinearColor(0.50f, 0.34f, 0.30f));
	BuildShop(World, FVector(6200.f, 0.f, 0.f), 2600.f, 2400.f, 180.f, FLinearColor(0.30f, 0.42f, 0.52f));
	BuildShop(World, FVector(6200.f, 5200.f, 0.f), 2600.f, 2400.f, 180.f, FLinearColor(0.48f, 0.46f, 0.24f));
	BuildShop(World, FVector(-6200.f, -5200.f, 0.f), 2600.f, 2400.f, 0.f, FLinearColor(0.36f, 0.30f, 0.50f));
	BuildShop(World, FVector(-6200.f, 5200.f, 0.f), 2600.f, 2400.f, 0.f, FLinearColor(0.22f, 0.48f, 0.36f));
	BuildShop(World, FVector(0.f, 6200.f, 0.f), 2600.f, 2400.f, -90.f, FLinearColor(0.52f, 0.28f, 0.34f));
	BuildShop(World, FVector(-3200.f, 6200.f, 0.f), 2400.f, 2400.f, -90.f, FLinearColor(0.30f, 0.30f, 0.55f));
	BuildShop(World, FVector(3200.f, 6200.f, 0.f), 2400.f, 2400.f, -90.f, FLinearColor(0.45f, 0.40f, 0.22f));

	// --- Food court --------------------------------------------------------
	Block(World, FVector(0.f, -6000.f, 8.f), FVector(3200.f, 1800.f, 8.f), FLinearColor(0.42f, 0.35f, 0.22f));
	Block(World, FVector(0.f, -7900.f, 300.f), FVector(3200.f, 60.f, 300.f), ColWall);
	Block(World, FVector(0.f, -7700.f, WallHeight - 200.f), FVector(1400.f, 30.f, 140.f), ColSign);

	// --- Storage, team A back of house ------------------------------------
	Block(World, FVector(-8000.f, 7000.f, WallHeight * 0.5f), FVector(WallThick, 2400.f, WallHeight * 0.5f), ColWall * 0.8f);
	Block(World, FVector(-6800.f, 8600.f, WallHeight * 0.5f), FVector(1200.f, WallThick, WallHeight * 0.5f), ColWall * 0.8f);

	// --- Parking, team B back of house ------------------------------------
	Block(World, FVector(8000.f, -7000.f, WallHeight * 0.5f), FVector(WallThick, 2400.f, WallHeight * 0.5f), ColWall * 0.8f);
	Block(World, FVector(6800.f, -8600.f, WallHeight * 0.5f), FVector(1200.f, WallThick, WallHeight * 0.5f), ColWall * 0.8f);
	for (int32 Slot = 0; Slot < 6; ++Slot)
	{
		Block(World, FVector(8600.f, -5200.f - Slot * 700.f, 2.f), FVector(1000.f, 20.f, 2.f), FLinearColor::White);
	}

	// --- Bathrooms ----------------------------------------------------------
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float X = (Side == 0) ? 1600.f : -1600.f;
		Block(World, FVector(X, 8600.f, WallHeight * 0.5f), FVector(700.f, WallThick, WallHeight * 0.5f), ColAccent * 0.6f);
		Block(World, FVector(X + 700.f, 8100.f, WallHeight * 0.5f), FVector(WallThick, 500.f, WallHeight * 0.5f), ColAccent * 0.6f);
		Block(World, FVector(X - 700.f, 8100.f, WallHeight * 0.5f), FVector(WallThick, 500.f, WallHeight * 0.5f), ColAccent * 0.6f);
		Block(World, FVector(X, 7700.f, WallHeight - 150.f), FVector(300.f, 25.f, 100.f), ColSign);
	}

	// --- Maintenance corridor behind the west shops ------------------------
	Block(World, FVector(-8600.f, 0.f, WallHeight * 0.5f), FVector(WallThick, 4000.f, WallHeight * 0.5f), ColWall * 0.7f);
	Block(World, FVector(-9400.f, 0.f, WallHeight * 0.5f), FVector(WallThick, 4000.f, WallHeight * 0.5f), ColWall * 0.7f);
	Block(World, FVector(-9000.f, 0.f, WallHeight - 40.f), FVector(400.f, 4000.f, 40.f), ColWall * 0.5f);

	// --- Two staircases and the lift shaft ---------------------------------
	BuildStairs(World, FVector(3000.f, 3000.f, 0.f), 45.f);
	BuildStairs(World, FVector(-3000.f, -3000.f, 0.f), -135.f);
	Block(World, FVector(1800.f, -1800.f, WallHeight * 0.5f), FVector(320.f, 30.f, WallHeight * 0.5f), ColMetal);
	Block(World, FVector(2130.f, -2130.f, WallHeight * 0.5f), FVector(30.f, 320.f, WallHeight * 0.5f), ColMetal);

	// --- Pillars, planters and signage -------------------------------------
	for (int32 Ring = 0; Ring < 12; ++Ring)
	{
		const FVector Direction = FRotationMatrix(FRotator(0.f, Ring * 30.f, 0.f)).GetUnitAxis(EAxis::X);
		Block(World, Direction * 4600.f + FVector(0.f, 0.f, WallHeight * 0.5f), FVector(90.f, 90.f, WallHeight * 0.5f), ColWall * 0.9f);
		Block(World, Direction * 2600.f + FVector(0.f, 0.f, 45.f), FVector(140.f, 140.f, 45.f), ColPlanter);
		Block(World, Direction * 2600.f + FVector(0.f, 0.f, 170.f), FVector(110.f, 110.f, 110.f), FLinearColor(0.15f, 0.45f, 0.18f), 0.f, 0.f, true);
	}
	Block(World, FVector(0.f, 3200.f, 1100.f), FVector(700.f, 25.f, 150.f), ColSign);
	Block(World, FVector(0.f, -3200.f, 1100.f), FVector(700.f, 25.f, 150.f), ColSign);
}

// ---------------------------------------------------------------------------
// Gameplay actors
// ---------------------------------------------------------------------------

void UMallBuilder::SpawnGameplayActors(UWorld* World)
{
	if (!World)
	{
		return;
	}
	for (TActorIterator<APossessablePawn> It(World); It; ++It)
	{
		return;		// already populated
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<AMallElevator>(AMallElevator::StaticClass(), FVector(2000.f, -2000.f, 20.f), FRotator::ZeroRotator, Params);

	// --- Food court tables and chairs --------------------------------------
	for (int32 TableIndex = 0; TableIndex < 8; ++TableIndex)
	{
		const float X = -2400.f + (TableIndex % 4) * 1600.f;
		const float Y = -5400.f - (TableIndex / 4) * 1200.f;
		SpawnProp(World, FVector(X, Y, 20.f), 0.f, TEXT("TABLE"), EHostType::Prop, EHostMobility::Slide,
			FVector(90.f, 90.f, 40.f), FLinearColor(0.55f, 0.40f, 0.25f));
		for (int32 ChairIndex = 0; ChairIndex < 4; ++ChairIndex)
		{
			const float Angle = ChairIndex * 90.f;
			const FVector Offset = FRotationMatrix(FRotator(0.f, Angle, 0.f)).GetUnitAxis(EAxis::X) * 190.f;
			SpawnProp(World, FVector(X, Y, 20.f) + Offset, Angle + 180.f, TEXT("CHAIR"), EHostType::Prop, EHostMobility::Slide,
				FVector(40.f, 40.f, 25.f), FLinearColor(0.25f, 0.30f, 0.38f), FVector(-35.f, 0.f, 60.f), FVector(10.f, 40.f, 45.f));
		}
	}

	// --- Abandoned shopping carts -------------------------------------------
	const FVector CartSpots[] = {
		FVector(1200.f, 1600.f, 20.f), FVector(-1500.f, 2400.f, 20.f), FVector(4200.f, -1200.f, 20.f),
		FVector(-4200.f, -2000.f, 20.f), FVector(500.f, -4200.f, 20.f), FVector(-3000.f, 4400.f, 20.f),
		FVector(7200.f, -6400.f, 20.f), FVector(-7200.f, 6400.f, 20.f)
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CartSpots); ++Index)
	{
		SpawnProp(World, CartSpots[Index], Index * 37.f, TEXT("SHOPPING CART"), EHostType::Prop, EHostMobility::Wheeled,
			FVector(55.f, 40.f, 45.f), FLinearColor(0.65f, 0.66f, 0.70f), FVector(-45.f, 0.f, 55.f), FVector(8.f, 38.f, 30.f));
	}

	// --- Vending machines, bins, plants ---------------------------------------
	const FVector VendingSpots[] = {
		FVector(4550.f, 2600.f, 20.f), FVector(-4550.f, -2600.f, 20.f),
		FVector(1700.f, 7600.f, 20.f), FVector(-8900.f, 2200.f, 20.f)
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(VendingSpots); ++Index)
	{
		SpawnProp(World, VendingSpots[Index], Index * 90.f, TEXT("VENDING MACHINE"), EHostType::Prop, EHostMobility::Static,
			FVector(45.f, 80.f, 105.f), FLinearColor(0.75f, 0.15f, 0.20f), FVector(48.f, 0.f, 20.f), FVector(6.f, 60.f, 70.f));
	}
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const FVector Direction = FRotationMatrix(FRotator(0.f, Index * 36.f, 0.f)).GetUnitAxis(EAxis::X);
		SpawnProp(World, Direction * 3400.f + FVector(0.f, 0.f, 20.f), Index * 36.f, TEXT("TRASH BIN"),
			EHostType::Prop, EHostMobility::Slide, FVector(38.f, 38.f, 50.f), FLinearColor(0.20f, 0.35f, 0.22f));
		SpawnProp(World, Direction * 5600.f + FVector(0.f, 0.f, 20.f), Index * 36.f, TEXT("PLANT"),
			EHostType::Prop, EHostMobility::Static, FVector(55.f, 55.f, 70.f), FLinearColor(0.16f, 0.42f, 0.18f));
	}

	// --- Benches ---------------------------------------------------------------
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const float Angle = 30.f + Index * 60.f;
		const FVector Direction = FRotationMatrix(FRotator(0.f, Angle, 0.f)).GetUnitAxis(EAxis::X);
		SpawnProp(World, Direction * 1500.f + FVector(0.f, 0.f, 20.f), Angle + 90.f, TEXT("BENCH"),
			EHostType::Prop, EHostMobility::Slide, FVector(140.f, 45.f, 28.f), FLinearColor(0.45f, 0.33f, 0.22f));
	}

	// --- Doors ------------------------------------------------------------------
	const FVector DoorSpots[] = {
		FVector(-8000.f, 5400.f, 20.f), FVector(8000.f, -5400.f, 20.f),
		FVector(-8600.f, 3900.f, 20.f), FVector(-8600.f, -3900.f, 20.f), FVector(0.f, 8500.f, 20.f)
	};
	const float DoorYaws[] = { 90.f, 90.f, 0.f, 0.f, 0.f };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(DoorSpots); ++Index)
	{
		SpawnProp(World, DoorSpots[Index], DoorYaws[Index], TEXT("DOOR"), EHostType::Prop, EHostMobility::Hinge,
			FVector(20.f, 110.f, 130.f), FLinearColor(0.38f, 0.28f, 0.20f));
	}

	// --- Vehicles ----------------------------------------------------------------
	for (int32 Index = 0; Index < 4; ++Index)
	{
		SpawnProp(World, FVector(8600.f, -5500.f - Index * 700.f, 20.f), 90.f, TEXT("CAR"), EHostType::Vehicle, EHostMobility::Wheeled,
			FVector(220.f, 95.f, 65.f), FLinearColor(0.15f + Index * 0.2f, 0.2f, 0.35f), FVector(-20.f, 0.f, 95.f), FVector(90.f, 80.f, 40.f));
	}
	SpawnProp(World, FVector(-8600.f, -5500.f, 20.f), 90.f, TEXT("VAN"), EHostType::Vehicle, EHostMobility::Wheeled,
		FVector(250.f, 110.f, 110.f), FLinearColor(0.7f, 0.7f, 0.68f));

	// --- Shoppers and staff --------------------------------------------------------
	const TArray<TArray<FVector>> Routes = {
		{ FVector(2500.f, 2500.f, 95.f), FVector(2500.f, -2500.f, 95.f), FVector(-2500.f, -2500.f, 95.f), FVector(-2500.f, 2500.f, 95.f) },
		{ FVector(5200.f, 0.f, 95.f), FVector(0.f, 0.f, 95.f), FVector(-5200.f, 0.f, 95.f), FVector(0.f, 0.f, 95.f) },
		{ FVector(0.f, 5200.f, 95.f), FVector(0.f, -5200.f, 95.f) },
		{ FVector(6000.f, -5200.f, 95.f), FVector(1500.f, -5200.f, 95.f), FVector(1500.f, -1500.f, 95.f) },
		{ FVector(-6000.f, 5200.f, 95.f), FVector(-1500.f, 5200.f, 95.f), FVector(-1500.f, 1500.f, 95.f) },
		{ FVector(-1600.f, 7800.f, 95.f), FVector(1600.f, 7800.f, 95.f), FVector(0.f, 4000.f, 95.f) },
		{ FVector(7200.f, -7200.f, 95.f), FVector(7200.f, -2500.f, 95.f) },
		{ FVector(-7200.f, 7200.f, 95.f), FVector(-7200.f, 2500.f, 95.f) }
	};
	const FLinearColor ShirtColours[] = {
		FLinearColor(0.85f, 0.75f, 0.55f), FLinearColor(0.35f, 0.55f, 0.85f), FLinearColor(0.85f, 0.35f, 0.45f),
		FLinearColor(0.45f, 0.75f, 0.45f), FLinearColor(0.80f, 0.80f, 0.80f), FLinearColor(0.55f, 0.40f, 0.70f),
		FLinearColor(0.90f, 0.60f, 0.25f), FLinearColor(0.30f, 0.70f, 0.70f)
	};
	for (int32 Index = 0; Index < Routes.Num(); ++Index)
	{
		SpawnShopper(World, Routes[Index], ShirtColours[Index % UE_ARRAY_COUNT(ShirtColours)]);
	}
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

FVector UMallBuilder::GetTeamSpawn(EParasiteTeam Team, int32 PlayerIndex)
{
	const float X = (Team == EParasiteTeam::TeamB) ? 8800.f : -8800.f;
	return FVector(X, -1600.f + (PlayerIndex % 5) * 800.f, 140.f);
}

FVector UMallBuilder::GetNestLocation(EParasiteTeam Team, int32 Seed)
{
	// Several plausible hiding places per side, so the nest is not in the same
	// corner every match.
	static const FVector TeamASpots[] = {
		FVector(-6800.f, 8200.f, 120.f),		// storage room
		FVector(-9000.f, 1500.f, 120.f),		// maintenance corridor
		FVector(-6200.f, -5200.f, 120.f),		// back of the west shop
		FVector(-1600.f, 8100.f, 120.f)			// behind the bathrooms
	};
	static const FVector TeamBSpots[] = {
		FVector(6800.f, -8200.f, 120.f),		// parking bay corner
		FVector(6200.f, 5200.f, 120.f),			// back of the east shop
		FVector(8600.f, 2200.f, 120.f),			// service niche
		FVector(1600.f, -7600.f, 120.f)			// behind the food court counter
	};
	const int32 Index = FMath::Abs(Seed) % 4;
	return (Team == EParasiteTeam::TeamA) ? TeamASpots[Index] : TeamBSpots[Index];
}

void UMallBuilder::GetRestrictedZone(EParasiteTeam ForTeam, FVector& OutMin, FVector& OutMax)
{
	if (ForTeam == EParasiteTeam::TeamA)
	{
		// Team B's parking and service side.
		OutMin = FVector(6000.f, -10000.f, -500.f);
		OutMax = FVector(10000.f, -4000.f, 2000.f);
	}
	else
	{
		// Team A's storage and maintenance side.
		OutMin = FVector(-10000.f, 4000.f, -500.f);
		OutMax = FVector(-6000.f, 10000.f, 2000.f);
	}
}
