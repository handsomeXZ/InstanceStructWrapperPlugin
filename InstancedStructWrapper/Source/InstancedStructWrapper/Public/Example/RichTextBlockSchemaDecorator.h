#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"
#include "Framework/Text/TextLayout.h"
#include "Framework/Text/ISlateRun.h"
#include "Framework/Text/ITextDecorator.h"
#include "Components/RichTextBlockDecorator.h"
#include "Engine/DataTable.h"

#include "InstancedStructWrapper.h"

#include "RichTextBlockSchemaDecorator.generated.h"

class ISlateStyle;

UCLASS(Abstract, Blueprintable)
class INSTANCEDSTRUCTWRAPPER_API URichTextBlockSchemaDecorator : public URichTextBlockDecorator
{
	GENERATED_BODY()

public:
	URichTextBlockSchemaDecorator(const FObjectInitializer& ObjectInitializer);

	virtual TSharedPtr<ITextDecorator> CreateDecorator(URichTextBlock* InOwner) override;
public:
	UPROPERTY(EditAnywhere, Category = Schema, meta = (ExcludeBaseStruct, BaseStruct = "/Script/InstancedStructWrapper.SchemaDecoratorChooserBase"))
	FInstancedStructWrapper Chooser;

	UPROPERTY(EditAnywhere, Category = Appearance, meta = (RequiredAssetDataTags = "RowStructure=/Script/UMG.RichImageRow"))
	TObjectPtr<class UDataTable> ImageStyle;
	UPROPERTY(EditAnywhere, Category = Appearance, meta = (RequiredAssetDataTags = "RowStructure=/Script/UMG.RichTextStyleRow"))
	TObjectPtr<class UDataTable> TextStyle;
};

USTRUCT()
struct INSTANCEDSTRUCTWRAPPER_API FSchemaDecoratorChooserBase
{
	GENERATED_BODY()
	virtual ~FSchemaDecoratorChooserBase() {}
	virtual FInstancedStructContainerWrapper* GetTargetDataRow(FName InName);
	virtual FName GetParseName() const { return FName(TEXT("Schema")); };
	virtual FName GetParseMetaData() const { return FName(TEXT("Name")); };
};

//////////////////////////////////////////////////////////////////////////
// Overlay Style
//////////////////////////////////////////////////////////////////////////
USTRUCT(meta = (SchemaClass = "/Script/InstancedStructWrapperEditor.SchemaDecoratorOverlayStyle_BaseSchema"))
struct INSTANCEDSTRUCTWRAPPER_API FSchemaDecoratorOverlayStyleBase
{
	GENERATED_BODY()
	virtual TSharedPtr<SWidget> GetStyleWidget(URichTextBlockSchemaDecorator* Decorator) const { return TSharedPtr<SWidget>(); }
	virtual ~FSchemaDecoratorOverlayStyleBase() {}
};

USTRUCT(DisplayName = "Text", meta = (SchemaClass = "/Script/InstancedStructWrapperEditor.SchemaDecoratorOverlayStyle_TextSchema"))
struct FSchemaDecoratorOverlayStyle_Text : public FSchemaDecoratorOverlayStyleBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = Appearance)
	FName Style;
	UPROPERTY(EditAnywhere, Category = Appearance)
	FText Text;

	virtual TSharedPtr<SWidget> GetStyleWidget(URichTextBlockSchemaDecorator* Decorator) const override;
	virtual ~FSchemaDecoratorOverlayStyle_Text() {}
};

USTRUCT(DisplayName = "Image", meta = (SchemaClass = "/Script/InstancedStructWrapperEditor.SchemaDecoratorOverlayStyle_ImageSchema"))
struct FSchemaDecoratorOverlayStyle_Image : public FSchemaDecoratorOverlayStyleBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = Appearance)
	FName Style;

	virtual TSharedPtr<SWidget> GetStyleWidget(URichTextBlockSchemaDecorator* Decorator) const override;
	virtual ~FSchemaDecoratorOverlayStyle_Image() {}
};

USTRUCT(DisplayName = "UserWidget", meta = (SchemaClass = "/Script/InstancedStructWrapperEditor.SchemaDecoratorOverlayStyle_UserWidgetSchema"))
struct FSchemaDecoratorOverlayStyle_UserWidget : public FSchemaDecoratorOverlayStyleBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = Appearance)
	TSubclassOf<UUserWidget> UserWidgetClass;

	virtual TSharedPtr<SWidget> GetStyleWidget(URichTextBlockSchemaDecorator* Decorator) const override;
	virtual ~FSchemaDecoratorOverlayStyle_UserWidget() {}
};

//////////////////////////////////////////////////////////////////////////
// ~End Overlay Style
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// Example
//////////////////////////////////////////////////////////////////////////

// MappingName To Key
USTRUCT(DisplayName = "MappingKeyStyle")
struct INSTANCEDSTRUCTWRAPPER_API FSchemaDecoratorChooser_MappingToKey : public FSchemaDecoratorChooserBase
{
	GENERATED_BODY()
	virtual ~FSchemaDecoratorChooser_MappingToKey() {}
	virtual FInstancedStructContainerWrapper* GetTargetDataRow(FName InName) override;
	virtual FName GetParseName() const override { return FName(TEXT("Key")); };
	virtual FName GetParseMetaData() const override { return FName(TEXT("MappingName")); };

	UPROPERTY(EditAnywhere, meta = (ExcludeBaseStruct, BaseStruct = "/Script/InstancedStructWrapper.SchemaDecoratorOverlayStyleBase"))
	TMap<FKey, FInstancedStructContainerWrapper> Styles;
};
// ~MappingName To Key