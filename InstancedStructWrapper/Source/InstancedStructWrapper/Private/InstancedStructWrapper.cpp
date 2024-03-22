#include "InstancedStructWrapper.h"

#define LOCTEXT_NAMESPACE "InstancedStructWrapper"

FInstancedStructWrapper::FInstancedStructWrapper()
#if WITH_EDITOR
#if WITH_EDITORONLY_DATA
	: DisplayNameOverride(FText())
#endif
#endif
{

}

bool FInstancedStructWrapper::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
#if WITH_EDITORONLY_DATA
	Ar << DisplayNameOverride;
#endif
#endif

	return true;
}

bool FInstancedStructContainerWrapper::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
#if WITH_EDITORONLY_DATA
	Ar << DisplayNameOverride;
#endif
#endif

	return true;
}

#undef LOCTEXT_NAMESPACE