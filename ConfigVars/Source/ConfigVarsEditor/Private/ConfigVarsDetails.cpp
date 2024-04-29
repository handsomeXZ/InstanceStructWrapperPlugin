#include "ConfigVarsDetails.h"

#include "ConfigVarsLinker.h"
#include "ConfigVarsReader.h"

#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "IStructureDataProvider.h"
#include "InstancedStruct.h"

#define LOCTEXT_NAMESPACE "ConfigVarsDetails"

////////////////////////////////////

class FInstancedStructProvider : public IStructureDataProvider
{
public:
	FInstancedStructProvider() = default;

	explicit FInstancedStructProvider(const TSharedPtr<IPropertyHandle>& InStructProperty)
		: StructProperty(InStructProperty)
	{
	}

	virtual ~FInstancedStructProvider() override {}

	void Reset()
	{
		StructProperty = nullptr;
	}

	virtual bool IsValid() const override
	{
		bool bHasValidData = false;
		EnumerateInstances([&bHasValidData](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
			{
				if (ScriptStruct && Memory)
				{
					bHasValidData = true;
					return false; // Stop
				}
				return true; // Continue
			});

		return bHasValidData;
	}

	virtual const UStruct* GetBaseStructure() const override
	{
		// Taken from UClass::FindCommonBase
		auto FindCommonBaseStruct = [](const UScriptStruct* StructA, const UScriptStruct* StructB)
			{
				const UScriptStruct* CommonBaseStruct = StructA;
				while (CommonBaseStruct && StructB && !StructB->IsChildOf(CommonBaseStruct))
				{
					CommonBaseStruct = Cast<UScriptStruct>(CommonBaseStruct->GetSuperStruct());
				}
				return CommonBaseStruct;
			};

		const UScriptStruct* CommonStruct = nullptr;
		EnumerateInstances([&CommonStruct, &FindCommonBaseStruct](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
			{
				if (ScriptStruct)
				{
					CommonStruct = FindCommonBaseStruct(ScriptStruct, CommonStruct);
				}
				return true; // Continue
			});

		return CommonStruct;
	}

	virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances) const override
	{
		// The returned instances need to be compatible with base structure.
		// This function returns empty instances in case they are not compatible, with the idea that we have as many instances as we have outer objects.
		const UScriptStruct* CommonStruct = Cast<UScriptStruct>(GetBaseStructure());
		EnumerateInstances([&OutInstances, CommonStruct](const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)
			{
				TSharedPtr<FStructOnScope> Result;

				if (CommonStruct && ScriptStruct && ScriptStruct->IsChildOf(CommonStruct))
				{
					Result = MakeShared<FStructOnScope>(ScriptStruct, Memory);
					Result->SetPackage(Package);
				}

				OutInstances.Add(Result);

				return true; // Continue
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

		FInstancedStruct& InstancedStruct = *reinterpret_cast<FInstancedStruct*>(ParentValueAddress);
		if (ExpectedType && InstancedStruct.GetScriptStruct() && InstancedStruct.GetScriptStruct()->IsChildOf(ExpectedType))
		{
			return InstancedStruct.GetMutableMemory();
		}

		return nullptr;
	}

protected:

	void EnumerateInstances(TFunctionRef<bool(const UScriptStruct* ScriptStruct, uint8* Memory, UPackage* Package)> InFunc) const
	{
		if (!StructProperty.IsValid())
		{
			return;
		}

		TArray<UPackage*> Packages;
		StructProperty->GetOuterPackages(Packages);


		// 非事务，不允许撤回
		StructProperty->EnumerateRawData([&InFunc, &Packages](void* RawData, const int32 DataIndex, const int32 /*NumDatas*/)
			{
				FConfigVarsBag* Bag = static_cast<FConfigVarsBag*>(RawData);
				UPackage* Package = nullptr;
				const UScriptStruct* ScriptStruct = nullptr;
				uint8* Memory = nullptr;
				if (Bag)
				{
					if (ensureMsgf(Packages.IsValidIndex(DataIndex), TEXT("Expecting packges and raw data to match.")))
					{
						Package = Packages[DataIndex];
						Bag->Outermost = Packages[DataIndex];

						FStructView StructView = Bag->LoadOrAddData(Packages[DataIndex], nullptr);

						ScriptStruct = StructView.GetScriptStruct();
						Memory = StructView.GetMemory();
					}
				}
				return InFunc(ScriptStruct, Memory, Package);
			});


	}

	TSharedPtr<IPropertyHandle> StructProperty;
};

////////////////////////////////////

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

	ViewModel = MakeShared<FConfigVarsViewModel>(StructPropertyHandle);

	// 非事务，不允许撤回
	StructPropertyHandle->EnumerateRawData([&Packages, ViewModel = ViewModel, ConfigVarsDataStruct](void* RawData, const int32 DataIndex, const int32 /*NumDatas*/)
	{
		FConfigVarsBag* Bag = static_cast<FConfigVarsBag*>(RawData);
		if (Bag)
		{
			if (ensureMsgf(Packages.IsValidIndex(DataIndex), TEXT("Expecting packges and raw data to match.")))
			{
				if (Packages[DataIndex]->HasAnyFlags(RF_ClassDefaultObject))
				{
					return false;
				}
				Bag->Outermost = Packages[DataIndex];
				ViewModel->ConfigVarsDataCache = Bag->LoadOrAddData(Packages[DataIndex], ConfigVarsDataStruct);
			}
		}
		return true;
	});

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			StructPropertyHandle->CreatePropertyValueWidget()
		];
}

void FConfigVarsDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (!ViewModel.IsValid())
	{
		return;
	}

	TSharedRef<FInstancedStructProvider> NewStructProvider = MakeShared<FInstancedStructProvider>(StructPropertyHandle);

	TArray<TSharedPtr<IPropertyHandle>> ChildProperties = StructPropertyHandle->AddChildStructure(NewStructProvider);
	for (TSharedPtr<IPropertyHandle> ChildHandle : ChildProperties)
	{
		IDetailPropertyRow& Row = StructBuilder.AddProperty(ChildHandle.ToSharedRef());
	}

}

#undef LOCTEXT_NAMESPACE