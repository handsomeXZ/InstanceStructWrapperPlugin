#include "ConfigVarsDetails.h"

#include "ConfigVarsLinker.h"
#include "ConfigVarsReader.h"
#include "ConfigVarsTypes.h"

#include "ConfigVarsLinkerEditorData.h"

#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "IStructureDataProvider.h"
#include "InstancedStruct.h"

#define LOCTEXT_NAMESPACE "ConfigVarsDetails"

////////////////////////////////////

class FConfigVarsDetailUtils
{
public:
	static FStructView LoadOrAddData(UObject* Outermost, FConfigVarsBag* ConfigVarsBag, const UScriptStruct* DataStruct)
	{
		if (!Outermost || !ConfigVarsBag)
		{
			return FStructView();
		}
		UPackage* Package = Outermost->GetPackage();


		// 由ConfigVarsLinker继续寻找或创建
		UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("ConfigVarsLinker"));
		if (!ConfigVarsLinker)
		{
			ConfigVarsLinker = NewObject<UConfigVarsLinker>(Package, UConfigVarsLinker::StaticClass(), FName("ConfigVarsLinker"), RF_Public | RF_Standalone);
		}
		if (ConfigVarsLinker)
		{
			return ConfigVarsLinker->LoadOrAddData(*ConfigVarsBag, DataStruct);
		}

		return FStructView();
	}

	static void MarkPendingRemoved(UObject* Outermost, int32 ExportIndex, bool bIsPendingRemoved)
	{
		if (!Outermost || ExportIndex == INDEX_NONE)
		{
			return;
		}
		UPackage* Package = Outermost->GetPackage();

		// 由ConfigVarsLinker继续寻找或创建
		UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("ConfigVarsLinker"));
		if (!ConfigVarsLinker)
		{
			ConfigVarsLinker = NewObject<UConfigVarsLinker>(Package, UConfigVarsLinker::StaticClass(), FName("ConfigVarsLinker"), RF_Public | RF_Standalone);
		}
		if (ConfigVarsLinker)
		{
			return ConfigVarsLinker->MarkPendingRemoved(ExportIndex, bIsPendingRemoved);
		}
	}

	static UConfigVarsLinkerEditorData* GetLinkerEditorData(UObject* Outermost)
	{
		if (!Outermost)
		{
			return nullptr;
		}
		UPackage* Package = Outermost->GetPackage();

		// 由ConfigVarsLinker继续寻找或创建
		UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("ConfigVarsLinker"));
		if (!ConfigVarsLinker)
		{
			ConfigVarsLinker = NewObject<UConfigVarsLinker>(Package, UConfigVarsLinker::StaticClass(), FName("ConfigVarsLinker"), RF_Public | RF_Standalone);
		}
		if (ConfigVarsLinker)
		{
			return ConfigVarsLinker->GetLinkerEditorData();
		}

		return nullptr;
	}
};

/**
 * 有效的Handle会在析构时自动标记PendingRemoved ExportData。
 * 注意：
 * 这里仅标记，而不会马上移除数据，需要直到真正进行序列化时才会处理移除。
 */
struct FConfigVarsBagHandle : TSharedFromThis<FConfigVarsBagHandle>
{
	FConfigVarsBagHandle(int32 InExportIndex, UPackage* InPackage, TSharedRef<IPropertyHandle> InPropertyHandle)
		: ExportIndex(InExportIndex)
		, Package(InPackage)
		, PropertyHandle(InPropertyHandle)
	{
	}
	
	~FConfigVarsBagHandle()
	{
		ResetReference();
	}

	void Init()
	{
		if (!Package.IsValid())
		{
			return;
		}

		if (UConfigVarsLinkerEditorData* LinkerEditorData = FConfigVarsDetailUtils::GetLinkerEditorData(Package.Get()))
		{
			LinkerEditorData->OnConfigVarsBagPropertyNodeChanged.Broadcast(Package.Get(), PropertyHandle->GetPropertyPath());
			OnConfigVarsBagPropertyNodeChangedHandle = LinkerEditorData->OnConfigVarsBagPropertyNodeChanged.AddSP(this, &FConfigVarsBagHandle::OnConfigVarsBagPropertyNodeChanged);
		}

		FConfigVarsDetailUtils::MarkPendingRemoved(Package.Get(), ExportIndex, false);
	}

	void ResetReference()
	{
		if (Package.IsValid())
		{
			FConfigVarsDetailUtils::MarkPendingRemoved(Package.Get(), ExportIndex, true);
		}

		if (UConfigVarsLinkerEditorData* LinkerEditorData = FConfigVarsDetailUtils::GetLinkerEditorData(Package.Get()))
		{
			LinkerEditorData->OnConfigVarsBagPropertyNodeChanged.Remove(OnConfigVarsBagPropertyNodeChangedHandle);
		}


		ExportIndex = INDEX_NONE;
		Package.Reset();
		PropertyHandle.Reset();
	}


	void OnConfigVarsBagPropertyNodeChanged(UPackage* InPackage, FStringView PropertyPath)
	{
		if (!PropertyHandle.IsValid() || PropertyHandle->GetPropertyPath() != PropertyPath)
		{
			return;
		}

		if (Package.IsValid() && Package.Get() == InPackage)
		{
			ResetReference();
		}
	}

	int32 ExportIndex;
	TWeakObjectPtr<UPackage> Package;
	TSharedPtr<IPropertyHandle> PropertyHandle;
	FDelegateHandle OnConfigVarsBagPropertyNodeChangedHandle;
};

class FConfigVarsDataProvider : public IStructureDataProvider
{
public:
	FConfigVarsDataProvider() = default;

	explicit FConfigVarsDataProvider(const TSharedPtr<IPropertyHandle>& InStructProperty)
		: StructProperty(InStructProperty)
	{

	}

	virtual ~FConfigVarsDataProvider() override
	{
		Reset();
	}

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

	virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances, const UStruct* ExpectedBaseStructure) const override
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

						FStructView StructView = FConfigVarsDetailUtils::LoadOrAddData(Packages[DataIndex], Bag, nullptr);

						ScriptStruct = StructView.GetScriptStruct();
						Memory = StructView.GetMemory();
					}
				}
				return InFunc(ScriptStruct, Memory, Package);
			});


	}
	
private:
	TSharedPtr<IPropertyHandle> StructProperty;
};

struct FConfigVarsViewModel : public TSharedFromThis<FConfigVarsViewModel>
{
	FConfigVarsViewModel(TSharedRef<IPropertyHandle> InPropertyHandle);
	~FConfigVarsViewModel();

	void Init();
	TSharedRef<FConfigVarsDataProvider> GetConfigVarsDataProvider();

	TSharedPtr<IPropertyHandle> PropertyHandle;
	FStructView ConfigVarsDataCache;

	TArray<TSharedPtr<FConfigVarsBagHandle>> ConfigVarsBagHandles;

	TWeakPtr<FConfigVarsDataProvider> ConfigVarsDataProvider;
};

FConfigVarsViewModel::FConfigVarsViewModel(TSharedRef<IPropertyHandle> InPropertyHandle)
	: PropertyHandle(InPropertyHandle)
	, ConfigVarsDataCache(nullptr)
{
}

FConfigVarsViewModel::~FConfigVarsViewModel()
{
}

void FConfigVarsViewModel::Init()
{
	TArray<UPackage*> Packages;
	PropertyHandle->GetOuterPackages(Packages);

	ConfigVarsBagHandles.Empty(Packages.Num());

	PropertyHandle->EnumerateRawData([this, &Packages](void* RawData, const int32 DataIndex, const int32 /*NumDatas*/)
		{
			FConfigVarsBag* Bag = static_cast<FConfigVarsBag*>(RawData);
			if (Bag)
			{
				if (ensureMsgf(Packages.IsValidIndex(DataIndex), TEXT("Expecting packges and raw data to match.")))
				{
					TSharedRef<FConfigVarsBagHandle> ConfigVarsBagHandle = MakeShared<FConfigVarsBagHandle>(Bag->GetExportIndex(), Packages[DataIndex], PropertyHandle.ToSharedRef());
					ConfigVarsBagHandle->Init();

					ConfigVarsBagHandles.Add(ConfigVarsBagHandle);
				}
			}
			return true;
		});
}

TSharedRef<FConfigVarsDataProvider> FConfigVarsViewModel::GetConfigVarsDataProvider()
{
	TSharedRef<FConfigVarsDataProvider> ProviderRef = MakeShared<FConfigVarsDataProvider>(PropertyHandle);

	ConfigVarsDataProvider = ProviderRef;

	return ProviderRef;
}
////////////////////////////////////

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
	FStructView ConfigVarsDataCache;
	StructPropertyHandle->EnumerateRawData([&Packages, &ConfigVarsDataCache, ConfigVarsDataStruct](void* RawData, const int32 DataIndex, const int32 /*NumDatas*/)
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
				ConfigVarsDataCache = FConfigVarsDetailUtils::LoadOrAddData(Packages[DataIndex], Bag, ConfigVarsDataStruct);
			}
		}
		return true;
	});


	ViewModel->Init();
	ViewModel->ConfigVarsDataCache = ConfigVarsDataCache;


	StructPropertyHandle->GetParentHandle()->SetOnPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(this, &FConfigVarsDetails::OnPropertyValueChangedWithData));
	StructPropertyHandle->GetParentHandle()->SetOnChildPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(this, &FConfigVarsDetails::OnPropertyValueChangedWithData));

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			StructPropertyHandle->CreatePropertyValueWidget(false)
		];
}

void FConfigVarsDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (!ViewModel.IsValid())
	{
		return;
	}

	TSharedRef<FConfigVarsDataProvider> NewStructProvider = ViewModel->GetConfigVarsDataProvider();

	TArray<TSharedPtr<IPropertyHandle>> ChildProperties = StructPropertyHandle->AddChildStructure(NewStructProvider);
	for (TSharedPtr<IPropertyHandle> ChildHandle : ChildProperties)
	{
		IDetailPropertyRow& Row = StructBuilder.AddProperty(ChildHandle.ToSharedRef());
	}
}

void FConfigVarsDetails::OnPropertyValueChangedWithData(const FPropertyChangedEvent& ChangedEvent)
{
	check(ViewModel->ConfigVarsDataProvider.IsValid());
}

#undef LOCTEXT_NAMESPACE