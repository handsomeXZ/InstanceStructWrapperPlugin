#include "InstancedStructWrapper.h"

#define LOCTEXT_NAMESPACE "InstancedStructWrapper"

FInstancedStructWrapper::FInstancedStructWrapper()
	: DisplayNameOverride(FText())
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

#undef LOCTEXT_NAMESPACE