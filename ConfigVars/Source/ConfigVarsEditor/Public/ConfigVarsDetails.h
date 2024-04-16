#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Styling/SlateStyle.h"

class UConfigVarsData;

struct FConfigVarsViewModel : public TSharedFromThis<FConfigVarsViewModel>
{
	FConfigVarsViewModel(TSharedRef<IPropertyHandle> InPropertyHandle, const UClass* InConfigVarsDataClass);

	TSharedPtr<IPropertyHandle> PropertyHandle;
	const UClass* ConfigVarsDataClass;
	UConfigVarsData* ConfigVarsDataCache;
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