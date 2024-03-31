#pragma once

#include "CoreMinimal.h"

#include "InstancedStructWrapper.h"

#include "RichTextBlockSchemaDecoratorEditor.generated.h"

//////////////////////////////////////////////////////////////////////////
// Overlay Style Schema
//////////////////////////////////////////////////////////////////////////
UCLASS()
class USchemaDecoratorOverlayStyle_BaseSchema : public UInstancedStructSchemaBase
{
	GENERATED_BODY()
public:
	virtual TSharedPtr<SWidget> GetButtonContentOverride(TSharedRef<IPropertyHandle> StructProperty) const override;
};

UCLASS()
class USchemaDecoratorOverlayStyle_TextSchema : public UInstancedStructSchemaBase
{
	GENERATED_BODY()
public:
	virtual TSharedPtr<SWidget> GetButtonContentOverride(TSharedRef<IPropertyHandle> StructProperty) const override;
};

UCLASS()
class USchemaDecoratorOverlayStyle_ImageSchema : public UInstancedStructSchemaBase
{
	GENERATED_BODY()
public:
	virtual TSharedPtr<SWidget> GetButtonContentOverride(TSharedRef<IPropertyHandle> StructProperty) const override;
};

UCLASS()
class USchemaDecoratorOverlayStyle_UserWidgetSchema : public UInstancedStructSchemaBase
{
	GENERATED_BODY()
public:
	virtual TSharedPtr<SWidget> GetButtonContentOverride(TSharedRef<IPropertyHandle> StructProperty) const override;
};
//////////////////////////////////////////////////////////////////////////
// ~End Overlay Style Schema
//////////////////////////////////////////////////////////////////////////