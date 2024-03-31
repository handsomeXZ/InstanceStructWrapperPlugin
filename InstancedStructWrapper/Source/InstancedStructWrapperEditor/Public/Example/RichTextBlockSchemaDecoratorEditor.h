#pragma once

#include "CoreMinimal.h"

#include "InstancedStructWrapperDetails.h"
#include "Widgets/SCompoundWidget.h"

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
	virtual TSharedPtr<SWidget> GetContainerTopExtension(TSharedRef<IPropertyHandle> StructProperty, TSharedRef<FInstancedStructWrapperContainerViewModel> ViewModel) const override;
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

class SOverlayStylePreview : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SOverlayStylePreview)
		{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FInstancedStructWrapperContainerViewModel> ViewModel);

	void GenerateOverlayChildren();
	void OnContainerChanged();
private:
	TSharedPtr<SOverlay> OverlyPanel;

	TSharedPtr<FInstancedStructWrapperContainerViewModel> ContainerViewModel;
};