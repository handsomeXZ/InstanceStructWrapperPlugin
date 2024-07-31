#include "ConfigVarsDetails.h"

#include "ConfigVarsLinker.h"
#include "ConfigVarsReader.h"
#include "ConfigVarsTypes.h"

#include "ConfigVarsLinkerEditorData.h"

#include "ScopedTransaction.h"
#include "IPropertyUtilities.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "IStructureDataProvider.h"
#include "InstancedStruct.h"
#include "StructViewerModule.h"
#include "Styling/SlateIconFinder.h"
#include "Engine/UserDefinedStruct.h"

#define LOCTEXT_NAMESPACE "ConfigVarsDetails"

////////////////////////////////////

class FConfigVarsDetailUtils
{
public:
	// 不断向外查找，必定成功
	static void FindOuterObject(TSharedRef<IPropertyHandle> PropertyHandle, OUT TArray<UObject*>& OuterObjects)
	{
		OuterObjects.Empty();
		PropertyHandle->GetOuterObjects(OuterObjects);
		if (!OuterObjects.IsEmpty())
		{
			return;
		}

		if (PropertyHandle->GetParentHandle().IsValid())
		{
			FindOuterObject(PropertyHandle->GetParentHandle().ToSharedRef(), OuterObjects);
		}
		else
		{
			return;
		}
	}

	static FStructView LoadOrAddData(UObject* Outermost, FConfigVarsBag* ConfigVarsBag, const UScriptStruct* DataStruct)
	{
		if (!Outermost || !ConfigVarsBag)
		{
			return FStructView();
		}

		return ConfigVarsBag->EditorLoadOrAddData(Outermost, DataStruct);
	}

	static void MarkPendingRemoved(UObject* Outermost, int32 ExportIndex, bool bIsPendingRemoved)
	{
		if (!Outermost || ExportIndex == INDEX_NONE)
		{
			return;
		}
		UPackage* Package = Outermost->GetPackage();

		// 由ConfigVarsLinker继续寻找或创建
		UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("Template_ConfigVarsLinker"));
		if (!ConfigVarsLinker)
		{
			ConfigVarsLinker = NewObject<UConfigVarsLinker>(Package, UConfigVarsLinker::StaticClass(), FName("Template_ConfigVarsLinker"), RF_Public | RF_Standalone);
		}
		if (ConfigVarsLinker)
		{
			return ConfigVarsLinker->MarkPendingRemoved(ExportIndex, bIsPendingRemoved);
		}
	}

	static void ImmediateRemoveData(UObject* Outermost, FConfigVarsBag* ConfigVarsBag)
	{
		if (!Outermost || !ConfigVarsBag)
		{
			return;
		}
		UPackage* Package = Outermost->GetPackage();

		// 由ConfigVarsLinker继续寻找或创建
		UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("Template_ConfigVarsLinker"));
		if (!ConfigVarsLinker)
		{
			ConfigVarsLinker = NewObject<UConfigVarsLinker>(Package, UConfigVarsLinker::StaticClass(), FName("Template_ConfigVarsLinker"), RF_Public | RF_Standalone);
		}
		if (ConfigVarsLinker)
		{
			return ConfigVarsLinker->ImmediateRemoveData(*ConfigVarsBag);
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
		UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("Template_ConfigVarsLinker"));
		if (!ConfigVarsLinker)
		{
			ConfigVarsLinker = NewObject<UConfigVarsLinker>(Package, UConfigVarsLinker::StaticClass(), FName("Template_ConfigVarsLinker"), RF_Public | RF_Standalone);
		}
		if (ConfigVarsLinker)
		{
			return ConfigVarsLinker->GetLinkerEditorData();
		}

		return nullptr;
	}

	static FPropertyAccess::Result GetLastConfigVarsStruct(TSharedRef<IPropertyHandle> StructProperty, const UScriptStruct*& OutCommonStruct)
	{
		bool bHasResult = false;
		bool bHasMultipleValues = false;

		TArray<UObject*> OuterObjects;
		FindOuterObject(StructProperty, OuterObjects);

		StructProperty->EnumerateRawData([&OutCommonStruct, &bHasResult, &bHasMultipleValues, &OuterObjects](void* RawData, const int32 DataIndex, const int32 /*NumDatas*/)
			{
				FConfigVarsBag* Bag = static_cast<FConfigVarsBag*>(RawData);
				if (Bag)
				{
					if (ensureMsgf(OuterObjects.IsValidIndex(DataIndex), TEXT("Expecting packges and raw data to match.")))
					{
						if (OuterObjects[DataIndex]->HasAnyFlags(RF_ClassDefaultObject))
						{
							return false;
						}

						// 如果不存在懒加载数据，不会主动创建
						FStructView StructData = FConfigVarsDetailUtils::LoadOrAddData(OuterObjects[DataIndex], Bag, nullptr);

						if (!bHasResult)
						{
							OutCommonStruct = StructData.GetScriptStruct();
						}
						else if (OutCommonStruct != StructData.GetScriptStruct())
						{
							bHasMultipleValues = true;
						}

						bHasResult = true;
					}
				}

				return true;
			});

		if (bHasMultipleValues)
		{
			return FPropertyAccess::MultipleValues;
		}

		return bHasResult ? FPropertyAccess::Success : FPropertyAccess::Fail;
	}

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

		TArray<UObject*> OuterObjects;
		//StructPropertyHandle->GetOuterPackages(Packages);
		FConfigVarsDetailUtils::FindOuterObject(StructProperty.ToSharedRef(), OuterObjects);

		// 非事务，不允许撤回
		StructProperty->EnumerateRawData([&InFunc, &OuterObjects](void* RawData, const int32 DataIndex, const int32 /*NumDatas*/)
			{
				FConfigVarsBag* Bag = static_cast<FConfigVarsBag*>(RawData);
				UPackage* Package = nullptr;
				const UScriptStruct* ScriptStruct = nullptr;
				uint8* Memory = nullptr;
				if (Bag)
				{
					if (ensureMsgf(OuterObjects.IsValidIndex(DataIndex), TEXT("Expecting packges and raw data to match.")))
					{
						Package = OuterObjects[DataIndex]->GetPackage();

						FStructView StructView = FConfigVarsDetailUtils::LoadOrAddData(OuterObjects[DataIndex], Bag, nullptr);

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

bool FConfigVarsFilter::IsStructAllowed(const FStructViewerInitializationOptions& InInitOptions, const UScriptStruct* InStruct, TSharedRef<FStructViewerFilterFuncs> InFilterFuncs)
{
	if (InStruct->IsA<UUserDefinedStruct>())
	{
		return bAllowUserDefinedStructs;
	}

	if (InStruct == BaseStruct)
	{
		return bAllowBaseStruct;
	}

	if (InStruct->HasMetaData(TEXT("Hidden")))
	{
		return false;
	}

	// Query the native struct to see if it has the correct parent type (if any)
	return !BaseStruct || InStruct->IsChildOf(BaseStruct);
}

bool FConfigVarsFilter::IsUnloadedStructAllowed(const FStructViewerInitializationOptions& InInitOptions, const FSoftObjectPath& InStructPath, TSharedRef<FStructViewerFilterFuncs> InFilterFuncs)
{
	// User Defined Structs don't support inheritance, so only include them requested
	return bAllowUserDefinedStructs;
}
//////////////////////////////////////////////////////////////////////////

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

void FConfigVarsDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	ViewModel = MakeShared<FConfigVarsViewModel>(StructPropertyHandle, StructCustomizationUtils);

	ViewModel->Init();

	ViewModel->GenerateHeader(HeaderRow);

	StructPropertyHandle->GetParentHandle()->SetOnPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(this, &FConfigVarsDetails::OnPropertyValueChangedWithData));
	StructPropertyHandle->GetParentHandle()->SetOnChildPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(this, &FConfigVarsDetails::OnPropertyValueChangedWithData));
}

void FConfigVarsDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (!ViewModel.IsValid())
	{
		return;
	}

	ViewModel->GenerateChildren(StructBuilder);
}

void FConfigVarsDetails::OnPropertyValueChangedWithData(const FPropertyChangedEvent& ChangedEvent)
{
	
}

//////////////////////////////////////////////////////////////////////////

FConfigVarsViewModel::FConfigVarsViewModel(TSharedRef<IPropertyHandle> InPropertyHandle, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
	: DataStruct(nullptr)
	, PropertyHandle(InPropertyHandle)
	, PropUtils(StructCustomizationUtils.GetPropertyUtilities())
{
}

FConfigVarsViewModel::~FConfigVarsViewModel()
{
}

void FConfigVarsViewModel::Init()
{

}

const FString& FConfigVarsViewModel::GetMetaData(const FName& MetaName) const
{
	static const FName NAME_MetaFromOuter = "MetaFromOuter";
	static const FString EmtpyMetaData;

	if (!OuterObject.IsValid())
	{
		return EmtpyMetaData;
	}

	bool bMetaFromOuter = PropertyHandle->HasMetaData(NAME_MetaFromOuter);

	if (bMetaFromOuter)
	{
		return OuterObject->GetClass()->GetMetaData(MetaName);
	}
	else
	{
		return PropertyHandle->GetMetaData(MetaName);
	}

}

bool FConfigVarsViewModel::HasMetaData(const FName& MetaName) const
{
	static const FName NAME_MetaFromOuter = "MetaFromOuter";

	if (!OuterObject.IsValid())
	{
		return false;
	}

	bool bMetaFromOuter = PropertyHandle->HasMetaData(NAME_MetaFromOuter);

	if (bMetaFromOuter)
	{
		return OuterObject->GetClass()->HasMetaData(MetaName);
	}
	else
	{
		return PropertyHandle->HasMetaData(MetaName);
	}
}

void FConfigVarsViewModel::GenerateHeader(FDetailWidgetRow& HeaderRow)
{
	TArray<UObject*> OuterObjects;
	FConfigVarsDetailUtils::FindOuterObject(PropertyHandle.ToSharedRef(), OuterObjects);

	/**
	 * @TODO：暂时先不允许批量修改
	 * 1. 边界条件未处理好
	 * 2. MetaFromOuter不好处理
	 */
	if (OuterObjects.Num() > 1)
	{
		HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ConfigVarsValue", "Multiple Values"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
	}

	OuterObject = OuterObjects[0];
	
	static const FName NAME_DataStruct = "DataStruct";
	static const FName NAME_InheritedStruct = "InheritedDataStruct";

	// DataStruct是否是明确的数据类型指向，如果不是，则允许FInstancedStruct一样的配置方式
	bool bInheritedStruct = HasMetaData(NAME_InheritedStruct);

	const FString& DataStructName = GetMetaData(NAME_DataStruct);
	if (!DataStructName.IsEmpty())
	{
		DataStruct = UClass::TryFindTypeSlow<UScriptStruct>(DataStructName);
		if (!DataStruct)
		{
			DataStruct = LoadObject<UScriptStruct>(nullptr, *DataStructName);
		}
	}


	// 未配置Struct
	if (!DataStruct)
	{
		HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ConfigVarsValue", "None"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
		return;
	}

	if (bInheritedStruct)
	{
		HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(ComboButton, SComboButton)
			.OnGetMenuContent(this, &FConfigVarsViewModel::GenerateStructPicker)
			.ContentPadding(0)
			.IsEnabled(true)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SImage)
					.Image(this, &FConfigVarsViewModel::GetDisplayValueIcon)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &FConfigVarsViewModel::GetDisplayValueString)
					.ToolTipText(this, &FConfigVarsViewModel::GetTooltipText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];
	}
	else
	{
		// 非事务，不允许撤回
		FStructView ConfigVarsDataCache;
		PropertyHandle->EnumerateRawData([&OuterObjects, &ConfigVarsDataCache, DataStruct = DataStruct](void* RawData, const int32 DataIndex, const int32 /*NumDatas*/)
		{
			FConfigVarsBag* Bag = static_cast<FConfigVarsBag*>(RawData);
			if (Bag)
			{
				if (ensureMsgf(OuterObjects.IsValidIndex(DataIndex), TEXT("Expecting packges and raw data to match.")))
				{
					if (OuterObjects[DataIndex]->HasAnyFlags(RF_ClassDefaultObject))
					{
						return false;
					}
					ConfigVarsDataCache = FConfigVarsDetailUtils::LoadOrAddData(OuterObjects[DataIndex], Bag, DataStruct);
				}
			}
			return true;
		});

		HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			PropertyHandle->CreatePropertyValueWidget(false)
		];
	}
}

void FConfigVarsViewModel::GenerateChildren(class IDetailChildrenBuilder& StructBuilder)
{
	TSharedRef<FConfigVarsDataProvider> NewStructProvider = MakeShared<FConfigVarsDataProvider>(PropertyHandle);

	TArray<TSharedPtr<IPropertyHandle>> ChildProperties = PropertyHandle->AddChildStructure(NewStructProvider);


	for (TSharedPtr<IPropertyHandle> ChildHandle : ChildProperties)
	{
		IDetailPropertyRow& Row = StructBuilder.AddProperty(ChildHandle.ToSharedRef());
	}
}

TSharedRef<SWidget> FConfigVarsViewModel::GenerateStructPicker()
{
	TSharedRef<FConfigVarsFilter> StructFilter = MakeShared<FConfigVarsFilter>();
	StructFilter->BaseStruct = DataStruct;
	StructFilter->bAllowUserDefinedStructs = false;
	StructFilter->bAllowBaseStruct = true;

	const UScriptStruct* SelectedStruct = nullptr;
	const FPropertyAccess::Result Result = FConfigVarsDetailUtils::GetLastConfigVarsStruct(PropertyHandle.ToSharedRef(), SelectedStruct);

	FStructViewerInitializationOptions Options;
	Options.bShowNoneOption = true;
	Options.StructFilter = StructFilter;
	Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
	Options.DisplayMode = EStructViewerDisplayMode::ListView;
	Options.bAllowViewOptions = true;
	Options.SelectedStruct = SelectedStruct;

	FOnStructPicked OnPicked(FOnStructPicked::CreateSP(this, &FConfigVarsViewModel::OnStructPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer").CreateStructViewer(Options, OnPicked)
			]
		];
}

void FConfigVarsViewModel::OnStructPicked(const UScriptStruct* InStruct)
{
	if (PropertyHandle && PropertyHandle->IsValidHandle())
	{
		TArray<UObject*> OuterObjects;
		FConfigVarsDetailUtils::FindOuterObject(PropertyHandle.ToSharedRef(), OuterObjects);

		FScopedTransaction Transaction(LOCTEXT("OnStructPicked", "Set Struct"));

		PropertyHandle->NotifyPreChange();

		PropertyHandle->EnumerateRawData([InStruct, &OuterObjects](void* RawData, const int32 DataIndex, const int32 /*NumDatas*/)
		{
			FConfigVarsBag* Bag = static_cast<FConfigVarsBag*>(RawData);
			if (Bag)
			{
				if (ensureMsgf(OuterObjects.IsValidIndex(DataIndex), TEXT("Expecting packges and raw data to match.")))
				{
					if (OuterObjects[DataIndex]->HasAnyFlags(RF_ClassDefaultObject))
					{
						return false;
					}

					if (InStruct == nullptr)
					{
						FConfigVarsDetailUtils::ImmediateRemoveData(OuterObjects[DataIndex], Bag);
					}
					FConfigVarsDetailUtils::LoadOrAddData(OuterObjects[DataIndex], Bag, InStruct);
				}
			}
			return true;
		});

		PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		PropertyHandle->NotifyFinishedChangingProperties();

		// Property tree will be invalid after changing the struct type, force update.
		if (PropUtils.IsValid())
		{
			PropUtils->ForceRefresh();
		}
	}

	ComboButton->SetIsOpen(false);
}

FText FConfigVarsViewModel::GetDisplayValueString() const
{
	const UScriptStruct* SelectedStruct = nullptr;
	const FPropertyAccess::Result Result = FConfigVarsDetailUtils::GetLastConfigVarsStruct(PropertyHandle.ToSharedRef(), SelectedStruct);

	if (Result == FPropertyAccess::Success)
	{
		if (SelectedStruct)
		{
			return SelectedStruct->GetDisplayNameText();
		}
		return LOCTEXT("NullScriptStruct", "None");
	}
	if (Result == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}

	return FText::GetEmpty();
}

FText FConfigVarsViewModel::GetTooltipText() const
{
	const UScriptStruct* SelectedStruct = nullptr;
	const FPropertyAccess::Result Result = FConfigVarsDetailUtils::GetLastConfigVarsStruct(PropertyHandle.ToSharedRef(), SelectedStruct);

	if (SelectedStruct && Result == FPropertyAccess::Success)
	{
		return SelectedStruct->GetToolTipText();
	}

	return GetDisplayValueString();
}

const FSlateBrush* FConfigVarsViewModel::GetDisplayValueIcon() const
{
	static const FName NAME_ConfigVarsIcon = "ConfigVarsIcon";

	const FString& ConfigVarsIcon = GetMetaData(NAME_ConfigVarsIcon);

	if (!ConfigVarsIcon.IsEmpty())
	{
		return FSlateIconFinder::FindIcon(FName(ConfigVarsIcon)).GetIcon();
	}
	else
	{
		return FSlateIconFinder::FindIcon(NAME_ConfigVarsIcon).GetIcon();
	}
}

#undef LOCTEXT_NAMESPACE