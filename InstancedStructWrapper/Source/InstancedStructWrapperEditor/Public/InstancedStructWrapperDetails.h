﻿#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "IDetailCustomNodeBuilder.h"

#include "InstancedStructWrapper.h"

#include "InstancedStructDetails.h"

#include "InstancedStructWrapperDetails.generated.h"

class IPropertyHandle;
class IDetailPropertyRow;
class IPropertyHandle;
class FInstancedStructDetails;


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
	void OnTextCommitted(const FText& NewLabel, ETextCommit::Type CommitType);
	FText GetCommentAsText() const;
	FText GetTooltipText() const;
	FSlateColor GetBorderColor() const;
	FLinearColor GetFontColor() const;
private:
	TSharedPtr<FInstancedStructDetails> InstancedStructDetails;

	TSharedPtr<IPropertyHandle> StructProperty;
};

class INSTANCEDSTRUCTWRAPPEREDITOR_API FInstancedStructWrapperDataDetails : public FInstancedStructDataDetails
{
public:
	FInstancedStructWrapperDataDetails(TSharedPtr<IPropertyHandle> InStructProperty, TSharedPtr<IPropertyHandle> TempStructProperty);

};

struct FInstancedStructWrapperContainerViewModel : public TSharedFromThis<FInstancedStructWrapperContainerViewModel>
{
	FInstancedStructWrapperContainerViewModel() : PropertyHandle(nullptr) {}
	FInstancedStructWrapperContainerViewModel(TSharedRef<IPropertyHandle> InPropertyHandle);

	struct FInstancedStructContainerWrapper* GetContainer();
	TSharedPtr<IPropertyHandle> GetPropertyHandle() { return PropertyHandle; }
	FInstancedStructContainerArray& GetContainerProxy() { return ContainerProxy; }

	void OnContainerProxyValueChanged();
	void UpdateChildMetaData();

	FSimpleMulticastDelegate OnContainerChanged;
protected:
	FInstancedStructContainerArray ContainerProxy;
	

	TSharedPtr<IPropertyHandle> PropertyHandle;
};

class FInstancedStructWrapperContainerDetails : public IPropertyTypeCustomization
{
public:
	FInstancedStructWrapperContainerDetails();
	virtual ~FInstancedStructWrapperContainerDetails() {}

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	void OverrideProperty(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder);
protected:
	TSharedPtr<FInstancedStructWrapperContainerViewModel> ContainerViewModel;
};
