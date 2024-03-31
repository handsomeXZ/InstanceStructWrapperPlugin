#include "InstancedStructWrapper.h"

#if WITH_EDITOR
#include "PropertyHandle.h"
#endif

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
	if (!Ar.IsFilterEditorOnly())
	{
		Ar << DisplayNameOverride;
	}
#endif
#endif

	return true;
}

bool FInstancedStructContainerWrapper::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
#if WITH_EDITORONLY_DATA
	if (!Ar.IsFilterEditorOnly())
	{
		Ar << DisplayNameOverride;
	}
#endif
#endif

	return true;
}


#if WITH_EDITOR
TSharedPtr<SWidget> UInstancedStructSchemaBase::GetButtonContentOverride(TSharedRef<IPropertyHandle> StructProperty) const
{
	return nullptr;
}
#endif


#undef LOCTEXT_NAMESPACE