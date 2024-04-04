#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "IDetailCustomNodeBuilder.h"
#include "Styling/SlateStyle.h"

#include "InstancedStructWrapper.h"

#include "InstancedStructDetails.h"

#include "InstancedStructWrapperDetails.generated.h"

class IPropertyHandle;
class IDetailPropertyRow;
class IPropertyHandle;
class FInstancedStructDetails;
class ISlateStyle;

UCLASS()
class INSTANCEDSTRUCTWRAPPEREDITOR_API UInstancedStructSchemaBase : public UObject
{
	GENERATED_BODY()
public:
	// FInstancedStructWrapper
	// 覆盖SComboButton的内容
	virtual TSharedPtr<SWidget> GetButtonContentOverride(TSharedRef<IPropertyHandle> StructProperty) const;
	// 提供SComboButton右边的ExtensionWidget
	virtual TSharedPtr<SWidget> GetButtonContentExtension(TSharedRef<IPropertyHandle> StructProperty) const;
	// ~ FInstancedStructWrapper

	// FInstancedStructContainerWrapper
	// 提供Array顶部的ExtensionWidget
	virtual TSharedPtr<SWidget> GetContainerTopExtension(TSharedRef<IPropertyHandle> StructProperty, TSharedRef<struct FInstancedStructWrapperContainerViewModel> ViewModel) const;
	// ~ FInstancedStructContainerWrapper
};

class FInstancedStructWrapperEditorStyle
	: public FSlateStyleSet
{
public:
	static FInstancedStructWrapperEditorStyle& Get();

	static void Register();
	static void Unregister();

private:
	FInstancedStructWrapperEditorStyle();
};

USTRUCT()
struct FInstancedStructContainerArray
{
	GENERATED_BODY()
public:
	FInstancedStructContainerArray() {}

	UPROPERTY(EditAnywhere)
	TArray<FInstancedStructWrapper> Data;
};

/**
 * Type customization for FInstancedStructWrapperDetails.
 */
class INSTANCEDSTRUCTWRAPPEREDITOR_API FInstancedStructWrapperDetails : public IPropertyTypeCustomization
{
public:

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

protected:
	// 通过元数据来定义自定义行为
	void InitSchemaClass();
	void CustomizeValueWidgetBySchema(class FDetailWidgetDecl& ValueWidgetDecl);

	FSlateColor GetBorderColor() const;
	FLinearColor GetFontColor() const;
	TSharedPtr<SWidget> GetButtonContentOverride() const;
	TSharedPtr<SWidget> GetButtonContentExtension() const;
protected:
	void OnTextCommitted(const FText& NewLabel, ETextCommit::Type CommitType);
	FText GetCommentAsText() const;
	FText GetTooltipText() const;
private:
	TSharedPtr<FInstancedStructDetails> InstancedStructDetails;

	TSharedPtr<IPropertyHandle> StructProperty;

	// 通过元数据来定义自定义行为，重定义的行为由SchemaClass提供
	UClass* SchemaClass = nullptr;
};

class INSTANCEDSTRUCTWRAPPEREDITOR_API FInstancedStructWrapperDataDetails : public FInstancedStructDataDetails
{
public:
	FInstancedStructWrapperDataDetails(TSharedPtr<IPropertyHandle> InStructProperty, TSharedPtr<IPropertyHandle> TempStructProperty);

};

struct FInstancedStructWrapperContainerViewModel : public TSharedFromThis<FInstancedStructWrapperContainerViewModel>
{
	FInstancedStructWrapperContainerViewModel() : PropertyHandle(nullptr), PropertyOuter(nullptr) {}
	FInstancedStructWrapperContainerViewModel(TSharedRef<IPropertyHandle> InPropertyHandle);

	struct FInstancedStructContainerWrapper* GetContainer();
	TSharedPtr<IPropertyHandle> GetPropertyHandle() { return PropertyHandle; }
	FInstancedStructContainerArray& GetContainerProxy() { return ContainerProxy; }

	void OnContainerValueChanged(UObject*, FPropertyChangedEvent& ChangedEvent);
	void OnContainerProxyValueChanged();
	void UpdateChildMetaData();

	FSimpleMulticastDelegate OnContainerChanged;
protected:
	FInstancedStructContainerArray ContainerProxy;
	
	TSharedPtr<IPropertyHandle> PropertyHandle;
	UObject* PropertyOuter;
};

class FInstancedStructWrapperContainerDetails : public IPropertyTypeCustomization
{
public:
	FInstancedStructWrapperContainerDetails();
	virtual ~FInstancedStructWrapperContainerDetails();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	// 通过元数据来定义自定义行为
	void InitSchemaClass();

	void OverrideProperty(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder);
	TSharedPtr<SWidget> GetContainerTopExtension() const;
protected:
	TSharedPtr<FInstancedStructWrapperContainerViewModel> ContainerViewModel;

	// 通过元数据来定义自定义行为，重定义的行为由SchemaClass提供
	UClass* SchemaClass = nullptr;

	FDelegateHandle OnObjectPropertyChangedHandle;
};
