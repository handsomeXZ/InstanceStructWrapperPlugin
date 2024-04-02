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
#include "Example/RichTextBlockSchemaRun.h"

#include "InstancedStructWrapper.h"

#include "RichTextBlockSchemaDecorator.generated.h"

class ISlateStyle;

//----------------------------------------------------------------------------------------
// 这里不得不将 Decorator 和 StyleSheet分开定义，因为每个RichTextBlock都会实例化一份 Decorator
//----------------------------------------------------------------------------------------

// Decorator 提供组装Slate的功能（每个Decorator都会由RichTextBlock实例化）
UCLASS(Abstract, Blueprintable)
class INSTANCEDSTRUCTWRAPPER_API URichTextBlockSchemaDecorator : public URichTextBlockDecorator
{
	GENERATED_BODY()

public:
	URichTextBlockSchemaDecorator(const FObjectInitializer& ObjectInitializer);

	// 暂时以int32为索引，后续考虑加入MapWrapper


	// （Beta）动态开关仍处于开发阶段
	UFUNCTION(BlueprintCallable, Category = "SchemaDecorator")
	void SetEnableSlateForwardExtension(bool bIsEnable, int32 Index);
	// （Beta）动态开关仍处于开发阶段
	UFUNCTION(BlueprintCallable, Category = "SchemaDecorator")
	void SetEnableSlateBackwardExtension(bool bIsEnable, int32 Index);
	// （Beta）动态开关和动态参数仍处于开发阶段
	UFUNCTION(BlueprintCallable, Category = "SchemaDecorator")
	void EnableSlateForwardExtensionWithPayload(int32 Index, FInstancedStruct Payload);
	// （Beta）动态开关和动态参数仍处于开发阶段
	UFUNCTION(BlueprintCallable, Category = "SchemaDecorator")
	void EnableSlateBackwardExtensionWithPayload(int32 Index, FInstancedStruct Payload);
	UFUNCTION(BlueprintPure, Category = "SchemaDecorator")
	bool IsEnableSlateForwardExtension(int32 Index) const;
	UFUNCTION(BlueprintPure, Category = "SchemaDecorator")
	bool IsEnableSlateBackwardExtension(int32 Index) const;
	
	virtual TSharedPtr<ITextDecorator> CreateDecorator(URichTextBlock* InOwner) override;
public:
	friend struct FSchemaDecoratorProxy;

	UPROPERTY(EditAnywhere, Category = Schema, meta = (ExcludeBaseStruct, BaseStruct = "/Script/InstancedStructWrapper.SchemaDecoratorChooserBase"))
	TObjectPtr<URichTextBlockSchemaDecoratorStyleSheet> StyleSheet;

	// 默认支持32个附加渲染器
	uint32 ForwardAdditionSet;
	uint32 BackwardAdditionSet;

	// 为ForwardAddition和BackwardAddition提供传参
	UPROPERTY(Transient)
	TMap<int32, FInstancedStruct> ForwardPayloadMap;
	UPROPERTY(Transient)
	TMap<int32, FInstancedStruct> BackwardPayloadMap;
};

// StyleSheet 提供用于组装Slate的数据
UCLASS()
class INSTANCEDSTRUCTWRAPPER_API URichTextBlockSchemaDecoratorStyleSheet : public UDataAsset
{
	GENERATED_BODY()

public:
	URichTextBlockSchemaDecoratorStyleSheet() {}
public:
	UPROPERTY(EditAnywhere, Category = Schema, meta = (ExcludeBaseStruct, BaseStruct = "/Script/InstancedStructWrapper.SchemaDecoratorChooserBase"))
	FInstancedStructWrapper Chooser;

	UPROPERTY(EditAnywhere, Category = Appearance, meta = (RequiredAssetDataTags = "RowStructure=/Script/UMG.RichImageRow"))
	TObjectPtr<class UDataTable> ImageStyle;
	UPROPERTY(EditAnywhere, Category = Appearance, meta = (RequiredAssetDataTags = "RowStructure=/Script/UMG.RichTextStyleRow"))
	TObjectPtr<class UDataTable> TextStyle;

	// 默认支持32个附加渲染器
	UPROPERTY(EditAnywhere, Category = Appearance, AdvancedDisplay, meta = (ExcludeBaseStruct, BaseStruct = "/Script/InstancedStructWrapper.SchemaSlateAdditionRenderer"))
	FInstancedStructContainerWrapper ForwardAddition;
	// 默认支持32个附加渲染器
	UPROPERTY(EditAnywhere, Category = Appearance, AdvancedDisplay, meta = (ExcludeBaseStruct, BaseStruct = "/Script/InstancedStructWrapper.SchemaSlateAdditionRenderer"))
	FInstancedStructContainerWrapper BackwardAddition;
};

// Chooser 提供筛选组装所需数据的规则
USTRUCT()
struct INSTANCEDSTRUCTWRAPPER_API FSchemaDecoratorChooserBase
{
	GENERATED_BODY()
	virtual ~FSchemaDecoratorChooserBase() {}
	virtual FInstancedStructContainerWrapper* GetTargetDataRow(FName InName) { return nullptr; }
	virtual FName GetParseName() const { return NAME_None; };
	virtual FName GetParseMetaData() const { return NAME_None; };
};

//////////////////////////////////////////////////////////////////////////
// Overlay Style
//////////////////////////////////////////////////////////////////////////
USTRUCT(meta = (SchemaClass = "/Script/InstancedStructWrapperEditor.SchemaDecoratorOverlayStyle_BaseSchema"))
struct INSTANCEDSTRUCTWRAPPER_API FSchemaDecoratorOverlayStyleBase
{
	GENERATED_BODY()
	virtual TSharedPtr<SWidget> GetStyleWidget(URichTextBlockSchemaDecoratorStyleSheet* StyleSheet) const { return TSharedPtr<SWidget>(); }
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
	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin Padding;

	virtual TSharedPtr<SWidget> GetStyleWidget(URichTextBlockSchemaDecoratorStyleSheet* StyleSheet) const override;
	virtual ~FSchemaDecoratorOverlayStyle_Text() {}
};

USTRUCT(DisplayName = "Image", meta = (SchemaClass = "/Script/InstancedStructWrapperEditor.SchemaDecoratorOverlayStyle_ImageSchema"))
struct FSchemaDecoratorOverlayStyle_Image : public FSchemaDecoratorOverlayStyleBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = Appearance)
	FName Style;
	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin Padding;

	virtual TSharedPtr<SWidget> GetStyleWidget(URichTextBlockSchemaDecoratorStyleSheet* StyleSheet) const override;
	virtual ~FSchemaDecoratorOverlayStyle_Image() {}
};

USTRUCT(DisplayName = "UserWidget", meta = (SchemaClass = "/Script/InstancedStructWrapperEditor.SchemaDecoratorOverlayStyle_UserWidgetSchema"))
struct FSchemaDecoratorOverlayStyle_UserWidget : public FSchemaDecoratorOverlayStyleBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = Appearance)
	TSubclassOf<UUserWidget> UserWidgetClass;
	UPROPERTY(EditAnywhere, Category = Appearance)
	FMargin Padding;

	virtual TSharedPtr<SWidget> GetStyleWidget(URichTextBlockSchemaDecoratorStyleSheet* StyleSheet) const override;
	virtual ~FSchemaDecoratorOverlayStyle_UserWidget() {}
};

//////////////////////////////////////////////////////////////////////////
// ~End Overlay Style
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// Example
//////////////////////////////////////////////////////////////////////////

// Common Name Chooser
USTRUCT(DisplayName = "CommonNameChooser", meta=(Tooltip="<Common Name=\"xxx\"/>"))
struct INSTANCEDSTRUCTWRAPPER_API FSchemaDecoratorChooser_Common : public FSchemaDecoratorChooserBase
{
	GENERATED_BODY()
	virtual ~FSchemaDecoratorChooser_Common() {}
	virtual FInstancedStructContainerWrapper* GetTargetDataRow(FName InName) override;
	virtual FName GetParseName() const override { return FName(TEXT("Common")); };
	virtual FName GetParseMetaData() const override { return FName(TEXT("Name")); };

	UPROPERTY(EditAnywhere, meta = (ExcludeBaseStruct, BaseStruct = "/Script/InstancedStructWrapper.SchemaDecoratorOverlayStyleBase"))
	TMap<FName, FInstancedStructContainerWrapper> Styles;
};

// EnhancedInput MappingName To Key Chooser
USTRUCT(DisplayName = "MappingKeyChooser", meta = (Tooltip = "<Key MappingName=\"xxx\"/>"))
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