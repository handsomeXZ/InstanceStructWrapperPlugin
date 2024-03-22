#pragma once

#include "CoreMinimal.h"

#include "InstancedStruct.h"
#include "InstancedStructContainer.h"

#include "InstancedStructWrapper.generated.h"


USTRUCT(BlueprintType)
struct INSTANCEDSTRUCTWRAPPER_API FInstancedStructWrapper : public FInstancedStruct
{
	GENERATED_BODY()
public:
	FInstancedStructWrapper();
	explicit FInstancedStructWrapper(const UScriptStruct* InScriptStruct) : FInstancedStruct(InScriptStruct) {}
	explicit FInstancedStructWrapper(const FConstStructView InOther) : FInstancedStruct(InOther) {}
	FInstancedStructWrapper(const FInstancedStruct& InOther) : FInstancedStruct(InOther) {}
	FInstancedStructWrapper(FInstancedStruct&& InOther) : FInstancedStruct(InOther) {}


	/** For StructOpsTypeTraits */
	bool Serialize(FArchive& Ar);

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FText DisplayNameOverride;
#endif
};

template<>
struct TStructOpsTypeTraits<FInstancedStructWrapper> : public TStructOpsTypeTraitsBase2<FInstancedStructWrapper>
{
	enum
	{
		WithSerializer = true,
		WithIdentical = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithAddStructReferencedObjects = true,
		WithStructuredSerializeFromMismatchedTag = true,
		WithGetPreloadDependencies = true,
		WithNetSerializer = true,
		WithFindInnerPropertyInstance = true,
	};
};

USTRUCT(BlueprintType)
struct INSTANCEDSTRUCTWRAPPER_API FInstancedStructContainerWrapper : public FInstancedStructContainer
{
	GENERATED_BODY()
public:
	FInstancedStructContainerWrapper() {}
	FInstancedStructContainerWrapper(const FInstancedStructContainer& InOther) : FInstancedStructContainer(InOther) {}
	FInstancedStructContainerWrapper(FInstancedStructContainer&& InOther) : FInstancedStructContainer(InOther) {}

	/** For StructOpsTypeTraits */
	bool Serialize(FArchive& Ar);

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FText> DisplayNameOverride;
#endif
};

template<>
struct TStructOpsTypeTraits<FInstancedStructContainerWrapper> : public TStructOpsTypeTraitsBase2<FInstancedStructContainerWrapper>
{
	enum
	{
		WithSerializer = true,
		WithIdentical = true,
		WithAddStructReferencedObjects = true,
		WithGetPreloadDependencies = true,
	};
};