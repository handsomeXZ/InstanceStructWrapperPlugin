#include "ConfigVarsDetails.h"

#include "ConfigVarsLinker.h"
#include "ConfigVarsReader.h"

#include "IStructureDataProvider.h"
#include "IDetailChildrenBuilder.h"

#define LOCTEXT_NAMESPACE "ConfigVarsDetails"

class FConfigVarsDataProvider : public IStructureDataProvider
{
public:
	FConfigVarsDataProvider() = default;

	explicit FConfigVarsDataProvider(const TSharedPtr<FConfigVarsViewModel>& InViewModel)
		: ViewModel(InViewModel)
	{
	}

	virtual ~FConfigVarsDataProvider() override {}

	void Reset()
	{
		ViewModel = nullptr;
	}

	virtual bool IsValid() const override
	{
		return ViewModel.IsValid();
	}

	virtual const UStruct* GetBaseStructure() const override
	{
		if (!ViewModel.IsValid())
		{
			return nullptr;
		}
		return ViewModel->ConfigVarsDataClass;
	}

	virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances) const override
	{
		if (!ViewModel.IsValid())
		{
			return;
		}
		ViewModel->PropertyHandle->EnumerateRawData([&OutInstances, ViewModel = ViewModel](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (FConfigVarsBag* Bag = static_cast<FConfigVarsBag*>(RawData))
			{
				TSharedPtr<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(ViewModel->ConfigVarsDataClass, (uint8*)ViewModel->ConfigVarsDataCache);
				OutInstances.Add(StructOnScope);
			}
			return true;
		});
		
	}

	virtual bool IsPropertyIndirection() const override
	{
		return true;
	}

	virtual uint8* GetValueBaseAddress(uint8* ParentValueAddress, const UStruct* ExpectedType) const override
	{
		if (!ParentValueAddress)
		{
			return nullptr;
		}
		return ParentValueAddress;
	}

protected:

	TSharedPtr<FConfigVarsViewModel> ViewModel;
};

//////////////////////////////////////////////////////////////////////////

FConfigVarsViewModel::FConfigVarsViewModel(TSharedRef<IPropertyHandle> InPropertyHandle, const UClass* InConfigVarsDataClass)
	: PropertyHandle(InPropertyHandle)
	, ConfigVarsDataClass(InConfigVarsDataClass)
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
	static const FName NAME_DataClass = "DataClass";
	UClass* ConfigVarsDataClass = nullptr;
	{
		const FString& DataClassName = StructPropertyHandle->GetMetaData(NAME_DataClass);
		if (!DataClassName.IsEmpty())
		{
			ConfigVarsDataClass = UClass::TryFindTypeSlow<UClass>(DataClassName);
			if (!ConfigVarsDataClass)
			{
				ConfigVarsDataClass = LoadObject<UClass>(nullptr, *DataClassName);
			}
		}
	}

	if (!ConfigVarsDataClass)
	{
		return;
	}

	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.Num() != 1 || OuterObjects[0]->HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	ViewModel = MakeShared<FConfigVarsViewModel>(StructPropertyHandle, ConfigVarsDataClass);

	FConfigVarsBag* Bag = nullptr;


	// 非事务，不允许撤回
	StructPropertyHandle->EnumerateRawData([&Bag, &OuterObjects, ViewModel = ViewModel, ConfigVarsDataClass](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		Bag = static_cast<FConfigVarsBag*>(RawData);
		if (Bag)
		{
			Bag->Outermost = OuterObjects[0];
			ViewModel->ConfigVarsDataCache = Bag->LoadOrAddData(OuterObjects[0], ConfigVarsDataClass);
		}
		return true;
	});

	check(Bag);
}

void FConfigVarsDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (!ViewModel.IsValid())
	{
		return;
	}


	//ContainerViewModel->UpdateChildMetaData();
	//ArrayHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(ContainerViewModel.ToSharedRef(), &FInstancedStructWrapperContainerViewModel::OnContainerProxyValueChanged));
	//ArrayHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(ContainerViewModel.ToSharedRef(), &FInstancedStructWrapperContainerViewModel::OnContainerProxyValueChanged));
	
	//FObjectProperty* ObjectProperty = CastField<FObjectProperty>(DataCacheHandle->GetProperty());

	StructPropertyHandle->EnumerateRawData([&StructBuilder, ViewModel = ViewModel](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		if (FConfigVarsBag* Bag = static_cast<FConfigVarsBag*>(RawData))
		{
			FAddPropertyParams Params = FAddPropertyParams()
				.UniqueId(ViewModel->ConfigVarsDataCache->GetFName())
				.AllowChildren(true)
				.HideRootObjectNode(true);

			IDetailPropertyRow* Row = StructBuilder.AddExternalObjects({ ViewModel->ConfigVarsDataCache }, Params);
		}
		return true;
	});

}

#undef LOCTEXT_NAMESPACE