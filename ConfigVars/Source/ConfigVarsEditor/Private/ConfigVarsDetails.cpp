#include "ConfigVarsDetails.h"

#include "ConfigVarsLinker.h"
#include "ConfigVarsReader.h"

#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"

#define LOCTEXT_NAMESPACE "ConfigVarsDetails"

//////////////////////////////////////////////////////////////////////////

FConfigVarsViewModel::FConfigVarsViewModel(TSharedRef<IPropertyHandle> InPropertyHandle)
	: PropertyHandle(InPropertyHandle)
	, ConfigVarsDataCache(nullptr)
{

}

FConfigVarsDetails::FConfigVarsDetails()
{

}

FConfigVarsDetails::~FConfigVarsDetails()
{

}

TSharedRef<IPropertyTypeCustomization> FConfigVarsDetails::MakeInstance()
{
	TSharedRef<FConfigVarsDetails> Details = MakeShared<FConfigVarsDetails>();

	return Details;
}

void FConfigVarsDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	static const FName NAME_DataStruct = "DataStruct";
	UScriptStruct* ConfigVarsDataStruct = nullptr;
	{
		const FString& DataStructName = StructPropertyHandle->GetMetaData(NAME_DataStruct);
		if (!DataStructName.IsEmpty())
		{
			ConfigVarsDataStruct = UClass::TryFindTypeSlow<UScriptStruct>(DataStructName);
			if (!ConfigVarsDataStruct)
			{
				ConfigVarsDataStruct = LoadObject<UScriptStruct>(nullptr, *DataStructName);
			}
		}
	}

	if (!ConfigVarsDataStruct)
	{
		return;
	}

	TArray<UPackage*> Packages;
	StructPropertyHandle->GetOuterPackages(Packages);

	if (Packages.Num() != 1 || Packages[0]->HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	ViewModel = MakeShared<FConfigVarsViewModel>(StructPropertyHandle);

	FConfigVarsBag* Bag = nullptr;


	// 非事务，不允许撤回
	StructPropertyHandle->EnumerateRawData([&Bag, &Packages, ViewModel = ViewModel, ConfigVarsDataStruct](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		Bag = static_cast<FConfigVarsBag*>(RawData);
		if (Bag)
		{
			Bag->Outermost = Packages[0];
			ViewModel->ConfigVarsDataCache = Bag->LoadOrAddData(Packages[0], ConfigVarsDataStruct);
		}
		return true;
	});


}

void FConfigVarsDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (!ViewModel.IsValid())
	{
		return;
	}


	StructPropertyHandle->EnumerateRawData([&StructBuilder, ViewModel = ViewModel](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		if (FConfigVarsBag* Bag = static_cast<FConfigVarsBag*>(RawData))
		{
			TSharedPtr<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(ViewModel->ConfigVarsDataCache.GetScriptStruct(), ViewModel->ConfigVarsDataCache.GetMemory());
			
			FAddPropertyParams Params = FAddPropertyParams()
				.UniqueId(ViewModel->PropertyHandle->GetProperty()->GetFName())
				.AllowChildren(true);

			IDetailPropertyRow* Row = StructBuilder.AddExternalStructureProperty(StructOnScope.ToSharedRef(), NAME_None, Params);
			Row->DisplayName(ViewModel->PropertyHandle->GetPropertyDisplayName());
		}
		return true;
	});

}

#undef LOCTEXT_NAMESPACE