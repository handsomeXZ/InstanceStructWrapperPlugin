#include "InstancedStructWrapperDetails.h"

#include "InstancedStructWrapper.h"

#include "IDetailChildrenBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "DetailLayoutBuilder.h"

#include "StructUtilsDelegates.h"
#include "IStructureDataProvider.h"


#define LOCTEXT_NAMESPACE "InstancedStructWrapperDetails"

#define PRIVATE_DEFINE(Class, Type, Member, ...) struct FExternal_##Member##_##__VA_ARGS__ {\
        using type = Type Class::*;\
        };\
        template struct FPtrTaker<FExternal_##Member##_##__VA_ARGS__, &Class::Member>;
#define PRIVATE_GET(Obj, Member, ...) Obj->*FAccess<FExternal_##Member##_##__VA_ARGS__>::ptr

template <typename Tag>
class FAccess {
public:
	inline static typename Tag::type ptr;
};

template <typename Tag, typename Tag::type V>
struct FPtrTaker {
	struct FTransferer {
		FTransferer() {
			FAccess<Tag>::ptr = V;
		}
	};
	inline static FTransferer tr;
};

PRIVATE_DEFINE(SComboButton, SHorizontalBox::FSlot*, ButtonContentSlot);
PRIVATE_DEFINE(FSlotBase, TSharedRef<SWidget>, Widget);
PRIVATE_DEFINE(SBoxPanel, TPanelChildren<SBoxPanel::FSlot>, Children, 1);
PRIVATE_DEFINE(TPanelChildren<SBoxPanel::FSlot>, TArray<TUniquePtr<SBoxPanel::FSlot>>, Children, 2);


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

		StructProperty->EnumerateRawData([&InFunc, &Packages](void* RawData, const int32 DataIndex, const int32 /*NumDatas*/)
			{
				const UScriptStruct* ScriptStruct = nullptr;
				uint8* Memory = nullptr;
				UPackage* Package = nullptr;
				if (FInstancedStruct* InstancedStruct = static_cast<FInstancedStruct*>(RawData))
				{
					ScriptStruct = InstancedStruct->GetScriptStruct();
					Memory = InstancedStruct->GetMutableMemory();
					if (ensureMsgf(Packages.IsValidIndex(DataIndex), TEXT("Expecting packges and raw data to match.")))
					{
						Package = Packages[DataIndex];
					}
				}

				return InFunc(ScriptStruct, Memory, Package);
			});
	}

	TSharedPtr<IPropertyHandle> StructProperty;
};
namespace UE::StructUtils::Private
{

	FPropertyAccess::Result GetStruct(TSharedPtr<IPropertyHandle> StructProperty, const UScriptStruct*& OutCommonStruct, FInstancedStructWrapper*& OutInstancedStructWrapper)
	{
		bool bHasResult = false;
		bool bHasMultipleValues = false;

		StructProperty->EnumerateRawData([&OutCommonStruct, &bHasResult, &bHasMultipleValues, &OutInstancedStructWrapper](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
			{
				if (FInstancedStructWrapper* InstancedStruct = static_cast<FInstancedStructWrapper*>(RawData))
				{
					const UScriptStruct* Struct = InstancedStruct->GetScriptStruct();

					if (!bHasResult)
					{
						OutCommonStruct = Struct;
						OutInstancedStructWrapper = InstancedStruct;
					}
					else if (OutCommonStruct != Struct)
					{
						bHasMultipleValues = true;
					}

					bHasResult = true;
				}

				return true;
		});

		if (bHasMultipleValues)
		{
			return FPropertyAccess::MultipleValues;
		}

		return bHasResult ? FPropertyAccess::Success : FPropertyAccess::Fail;
	}

} // UE::StructUtils::Private

TSharedRef<IPropertyTypeCustomization> FInstancedStructWrapperDetails::MakeInstance()
{
	TSharedRef<FInstancedStructWrapperDetails> WrapperDetails = MakeShared<FInstancedStructWrapperDetails>();

	WrapperDetails->InstancedStructDetails = StaticCastSharedRef<FInstancedStructDetails>(FInstancedStructDetails::MakeInstance());

	return WrapperDetails;
}

void FInstancedStructWrapperDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;

	InstancedStructDetails->CustomizeHeader(StructPropertyHandle, HeaderRow, StructCustomizationUtils);

	FDetailWidgetDecl& WidgetDecl = HeaderRow.ValueContent();
	TSharedPtr<SComboButton> InternalWidget = StaticCastSharedRef<SComboButton>(WidgetDecl.Widget);

	FSlotBase* SlotBase = PRIVATE_GET(InternalWidget.Get(), ButtonContentSlot);

	TSharedPtr<SHorizontalBox> HorizontalBox = StaticCastSharedRef<SHorizontalBox>(PRIVATE_GET(SlotBase, Widget));

	TPanelChildren<SBoxPanel::FSlot>& BoxChildren = PRIVATE_GET(HorizontalBox.Get(), Children, 1);
	TArray<TUniquePtr<SBoxPanel::FSlot>>& BoxChildrenChildren = PRIVATE_GET(&BoxChildren, Children, 2);

	//TSharedPtr<STextBlock> TextBlock = StaticCastSharedRef<STextBlock>(PRIVATE_GET(BoxChildrenChildren[1].Get(), Widget));

	PRIVATE_GET(BoxChildrenChildren[1].Get(), Widget) = SNew(SHorizontalBox)
	+ SHorizontalBox::Slot()
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SInlineEditableTextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.OnVerifyTextChanged_Lambda([](const FText& NewLabel, FText& OutErrorMessage)
		{
			return NewLabel.IsEmpty() || !NewLabel.IsEmptyOrWhitespace();	// 允许是空内容
		})
		.OnTextCommitted(this, &FInstancedStructWrapperDetails::OnTextCommitted)
		.Text(this, &FInstancedStructWrapperDetails::GetCommentAsText)
		.ToolTipText(this, &FInstancedStructWrapperDetails::GetTooltipText)
		.Justification(ETextJustify::Center)
	];
}

void FInstancedStructWrapperDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<FInstancedStructWrapperDataDetails> DataDetails = MakeShared<FInstancedStructWrapperDataDetails>(StructProperty);
	StructBuilder.AddCustomBuilder(DataDetails);
}

void FInstancedStructWrapperDetails::OnTextCommitted(const FText& NewLabel, ETextCommit::Type CommitType)
{
	if (!StructProperty.IsValid())
	{
		return;
	}
	FInstancedStructWrapper* Wrapper = nullptr;
	const UScriptStruct* CommonStruct = nullptr;
	const FPropertyAccess::Result Result = UE::StructUtils::Private::GetStruct(StructProperty, CommonStruct, Wrapper);

	if (Result == FPropertyAccess::Success)
	{
		if (CommonStruct && Wrapper)
		{
			Wrapper->DisplayNameOverride = NewLabel;
		}
	}
}

FText FInstancedStructWrapperDetails::GetCommentAsText() const
{
	if (!StructProperty.IsValid())
	{
		return FText();
	}

	FInstancedStructWrapper* Wrapper = nullptr;
	const UScriptStruct* CommonStruct = nullptr;
	const FPropertyAccess::Result Result = UE::StructUtils::Private::GetStruct(StructProperty, CommonStruct, Wrapper);

	if (Result == FPropertyAccess::Success)
	{
		if (CommonStruct && Wrapper)
		{
			if (Wrapper->DisplayNameOverride.IsEmpty())
			{
				return CommonStruct->GetDisplayNameText();
			}
			else
			{
				return Wrapper->DisplayNameOverride;
			}
		}
	}
	else if (Result == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}

	return LOCTEXT("NullScriptStruct", "None");
}

FText FInstancedStructWrapperDetails::GetTooltipText() const
{
	FInstancedStructWrapper* Wrapper = nullptr;
	const UScriptStruct* CommonStruct = nullptr;
	const FPropertyAccess::Result Result = UE::StructUtils::Private::GetStruct(StructProperty, CommonStruct, Wrapper);

	if (CommonStruct && Result == FPropertyAccess::Success)
	{
		return CommonStruct->GetToolTipText();
	}

	return GetCommentAsText();
}

//////////////////////////////////////////////////////////////////////////
// FInstancedStructWrapperDataDetails
FInstancedStructWrapperDataDetails::FInstancedStructWrapperDataDetails(TSharedPtr<IPropertyHandle> InStructProperty)
{
#if DO_CHECK
	FStructProperty* StructProp = CastFieldChecked<FStructProperty>(InStructProperty->GetProperty());
	check(StructProp);
	check(StructProp->Struct == FInstancedStructWrapper::StaticStruct());
#endif

	StructProperty = InStructProperty;
}

FInstancedStructWrapperDataDetails::~FInstancedStructWrapperDataDetails()
{
	UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.Remove(UserDefinedStructReinstancedHandle);
}

void FInstancedStructWrapperDataDetails::OnUserDefinedStructReinstancedHandle(const UUserDefinedStruct& Struct)
{
	if (StructProvider.IsValid())
	{
		// Reset the struct provider immediately, some update functions might get called with the old struct.
		StructProvider->Reset();
	}
	OnRegenerateChildren.ExecuteIfBound();
}

void FInstancedStructWrapperDataDetails::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
	OnRegenerateChildren = InOnRegenerateChildren;
}

TArray<TWeakObjectPtr<const UStruct>> FInstancedStructWrapperDataDetails::GetInstanceTypes() const
{
	TArray<TWeakObjectPtr<const UStruct>> Result;

	StructProperty->EnumerateConstRawData([&Result](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			TWeakObjectPtr<const UStruct>& Type = Result.AddDefaulted_GetRef();
			if (const FInstancedStruct* InstancedStruct = static_cast<const FInstancedStruct*>(RawData))
			{
				Result.Add(InstancedStruct->GetScriptStruct());
			}
			else
			{
				Result.Add(nullptr);
			}
			return true;
		});

	return Result;
}

void FInstancedStructWrapperDataDetails::OnStructHandlePostChange()
{
	if (StructProvider.IsValid())
	{
		TArray<TWeakObjectPtr<const UStruct>> InstanceTypes = GetInstanceTypes();
		if (InstanceTypes != CachedInstanceTypes)
		{
			OnRegenerateChildren.ExecuteIfBound();
		}
	}
}

void FInstancedStructWrapperDataDetails::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	StructProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FInstancedStructWrapperDataDetails::OnStructHandlePostChange));
	if (!UserDefinedStructReinstancedHandle.IsValid())
	{
		UserDefinedStructReinstancedHandle = UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.AddSP(this, &FInstancedStructWrapperDataDetails::OnUserDefinedStructReinstancedHandle);
	}
}

void FInstancedStructWrapperDataDetails::GenerateChildContent(IDetailChildrenBuilder& ChildBuilder)
{
	// Add the rows for the struct
	TSharedRef<FInstancedStructProvider> NewStructProvider = MakeShared<FInstancedStructProvider>(StructProperty);

	TArray<TSharedPtr<IPropertyHandle>> ChildProperties = StructProperty->AddChildStructure(NewStructProvider);
	for (TSharedPtr<IPropertyHandle> ChildHandle : ChildProperties)
	{
		IDetailPropertyRow& Row = ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
		OnChildRowAdded(Row);
	}

	StructProvider = NewStructProvider;

	CachedInstanceTypes = GetInstanceTypes();
}

void FInstancedStructWrapperDataDetails::Tick(float DeltaTime)
{
	// If the instance types change (e.g. due to selecting new struct type), we'll need to update the layout.
	TArray<TWeakObjectPtr<const UStruct>> InstanceTypes = GetInstanceTypes();
	if (InstanceTypes != CachedInstanceTypes)
	{
		OnRegenerateChildren.ExecuteIfBound();
	}
}

FName FInstancedStructWrapperDataDetails::GetName() const
{
	static const FName Name("InstancedStructDataDetails");
	return Name;
}

void FInstancedStructWrapperDataDetails::PostUndo(bool bSuccess)
{
	// Undo; force a sync next Tick
	LastSyncEditableInstanceFromSourceSeconds = 0.0;
}

void FInstancedStructWrapperDataDetails::PostRedo(bool bSuccess)
{
	// Redo; force a sync next Tick
	LastSyncEditableInstanceFromSourceSeconds = 0.0;
}

#undef LOCTEXT_NAMESPACE