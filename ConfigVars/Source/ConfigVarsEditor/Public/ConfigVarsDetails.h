#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Styling/SlateStyle.h"
#include "StructViewerFilter.h"

#include "StructView.h"

struct FConfigVarsViewModel;

class FConfigVarsDetails : public IPropertyTypeCustomization
{
public:
	FConfigVarsDetails();
	virtual ~FConfigVarsDetails();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	void OnPropertyValueChangedWithData(const FPropertyChangedEvent& ChangedEvent);

protected:
	TSharedPtr<FConfigVarsViewModel> ViewModel;
};

struct FConfigVarsViewModel : public TSharedFromThis<FConfigVarsViewModel>
{
	FConfigVarsViewModel(TSharedRef<IPropertyHandle> InPropertyHandle, IPropertyTypeCustomizationUtils& StructCustomizationUtils);
	~FConfigVarsViewModel();

	void Init();

	void GenerateHeader(class FDetailWidgetRow& HeaderRow);
	void GenerateChildren(class IDetailChildrenBuilder& StructBuilder);

protected:
	const FString& GetMetaData(const FName& MetaName) const;
	bool HasMetaData(const FName& MetaName) const;

	TSharedRef<SWidget> GenerateStructPicker();
	void OnStructPicked(const UScriptStruct* InStruct);
	FText GetDisplayValueString() const;
	FText GetTooltipText() const;
	const FSlateBrush* GetDisplayValueIcon() const;

	UScriptStruct* DataStruct;
	TSharedPtr<IPropertyHandle> PropertyHandle;

	TSharedPtr<SComboButton> ComboButton;
	TSharedPtr<IPropertyUtilities> PropUtils;

	/**
	 * @TODO：暂时先不允许批量修改
	 * 1. 边界条件未处理好
	 * 2. MetaFromOuter不好处理
	 */
	 TWeakObjectPtr<UObject> OuterObject;
};

/**
 * Filter used by the instanced struct struct picker.
 */
class FConfigVarsFilter : public IStructViewerFilter
{
public:
	/** The base struct for the property that classes must be a child-of. */
	const UScriptStruct* BaseStruct = nullptr;

	// A flag controlling whether we allow UserDefinedStructs
	bool bAllowUserDefinedStructs = false;

	// A flag controlling whether we allow to select the BaseStruct
	bool bAllowBaseStruct = true;

	virtual bool IsStructAllowed(const FStructViewerInitializationOptions& InInitOptions, const UScriptStruct* InStruct, TSharedRef<FStructViewerFilterFuncs> InFilterFuncs) override;
	virtual bool IsUnloadedStructAllowed(const FStructViewerInitializationOptions& InInitOptions, const FSoftObjectPath& InStructPath, TSharedRef<FStructViewerFilterFuncs> InFilterFuncs) override;
};