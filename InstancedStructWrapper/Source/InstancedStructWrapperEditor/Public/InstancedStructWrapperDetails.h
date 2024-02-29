#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "IDetailCustomNodeBuilder.h"

#include "InstancedStructDetails.h"

class IPropertyHandle;
class IDetailPropertyRow;
class IPropertyHandle;
class FInstancedStructDetails;


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
private:
	TSharedPtr<FInstancedStructDetails> InstancedStructDetails;

	TSharedPtr<IPropertyHandle> StructProperty;
};

class INSTANCEDSTRUCTWRAPPEREDITOR_API FInstancedStructWrapperDataDetails : public FInstancedStructDataDetails
{
public:
	FInstancedStructWrapperDataDetails(TSharedPtr<IPropertyHandle> InStructProperty, TSharedPtr<IPropertyHandle> TempStructProperty);

};