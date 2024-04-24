#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Styling/SlateStyle.h"

#include "StructView.h"

struct FConfigVarsViewModel : public TSharedFromThis<FConfigVarsViewModel>
{
	FConfigVarsViewModel(TSharedRef<IPropertyHandle> InPropertyHandle);

	TSharedPtr<IPropertyHandle> PropertyHandle;
	FStructView ConfigVarsDataCache;
};

class FConfigVarsDetails : public IPropertyTypeCustomization
{
public:
	FConfigVarsDetails();
	virtual ~FConfigVarsDetails();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	TSharedPtr<FConfigVarsViewModel> ViewModel;
};