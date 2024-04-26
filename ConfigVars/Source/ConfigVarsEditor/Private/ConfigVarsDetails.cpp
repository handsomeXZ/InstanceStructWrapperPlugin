#include "ConfigVarsDetails.h"

#include "ConfigVarsLinker.h"
#include "ConfigVarsReader.h"

#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"

#include "PrivateAccessor.h"

//////////////////////////////////////////////////////////////////////////
#include "PropertyNode.h"
#include "PropertyHandleImpl.h"
//////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "ConfigVarsDetails"

PRIVATE_DEFINE_VAR(FPropertyNode, TWeakPtr<FPropertyNode>, ParentNodeWeakPtr);

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
			TSharedPtr<IPropertyHandle> StructProviderHandle = Row->GetPropertyHandle()->GetParentHandle();

			// 必须将Outmost的ParentNode设为OuterUObject，否则不能支持EditInlineNew的实例化Object。
			// 注意：UDataTable的OuterObject是空的，所以它也就不能支持实例化Object。
			TSharedPtr<FPropertyNode> PropertyNode = StaticCastSharedRef<FPropertyHandleBase>(StructProviderHandle.ToSharedRef())->GetPropertyNode();
			PRIVATE_GET_VAR(PropertyNode.Get(), ParentNodeWeakPtr) = StaticCastSharedRef<FPropertyHandleBase>(ViewModel->PropertyHandle.ToSharedRef())->GetPropertyNode();

			Row->DisplayName(ViewModel->PropertyHandle->GetPropertyDisplayName());
		}
		return true;
	});

}

#undef LOCTEXT_NAMESPACE