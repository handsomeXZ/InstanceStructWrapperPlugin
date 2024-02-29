#pragma once

#include "CoreMinimal.h"

#include "InstancedStruct.h"

#include "InstancedStructWrapper.generated.h"


USTRUCT(BlueprintType)
struct INSTANCEDSTRUCTWRAPPER_API FInstancedStructWrapper : public FInstancedStruct
{
	GENERATED_BODY()
public:
	FInstancedStructWrapper();

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

