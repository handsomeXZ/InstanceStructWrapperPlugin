#include "InstancedStructWrapperDetails.h"

#include "IDetailChildrenBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "DetailLayoutBuilder.h"

#include "IStructureDataProvider.h"
#include "PropertyNode.h"
#include "PropertyHandleImpl.h"

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

PRIVATE_DEFINE(FPropertyNode, TWeakFieldPtr<FProperty>, Property);

FPropertyAccess::Result GetStructData(TSharedPtr<IPropertyHandle> StructProperty, const UScriptStruct*& OutCommonStruct, FInstancedStructWrapper*& OutInstancedStructWrapper)
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

FPropertyAccess::Result GetStructContainerData(TSharedPtr<IPropertyHandle> StructProperty, FInstancedStructContainerWrapper*& OutInstancedStructWrapper)
{
	bool bHasResult = false;
	bool bHasMultipleValues = false;

	StructProperty->EnumerateRawData([&bHasResult, &bHasMultipleValues, &OutInstancedStructWrapper](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (FInstancedStructContainerWrapper* InstancedStruct = static_cast<FInstancedStructContainerWrapper*>(RawData))
			{
				if (!bHasResult)
				{
					OutInstancedStructWrapper = InstancedStruct;
				}
				else
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

class FInstancedStructContainerProvider : public IStructureDataProvider
{
public:
	FInstancedStructContainerProvider() = default;

	explicit FInstancedStructContainerProvider(const TSharedPtr<FInstancedStructWrapperContainerViewModel>& InContainerViewModel)
		: ContainerViewModel(InContainerViewModel)
	{
	}

	virtual ~FInstancedStructContainerProvider() override {}

	void Reset()
	{
		ContainerViewModel = nullptr;
	}

	virtual bool IsValid() const override
	{
		return ContainerViewModel.IsValid();
	}

	virtual const UStruct* GetBaseStructure() const override
	{
		return FInstancedStructContainerArray::StaticStruct();
	}

	virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances) const override
	{
		FInstancedStructContainerArray& ContainerProxy = ContainerViewModel->GetContainerProxy();

		TSharedPtr<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FInstancedStructContainerArray::StaticStruct(), (uint8*)&ContainerProxy);
		OutInstances.Add(StructOnScope);
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

		//FInstancedStruct& InstancedStruct = *reinterpret_cast<FInstancedStruct*>(ParentValueAddress);
		//if (ExpectedType && InstancedStruct.GetScriptStruct() && InstancedStruct.GetScriptStruct()->IsChildOf(ExpectedType))
		//{
		//	return InstancedStruct.GetMutableMemory();
		//}

		return ParentValueAddress;
	}

protected:


	TSharedPtr<FInstancedStructWrapperContainerViewModel> ContainerViewModel;
};

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

	WidgetDecl.Widget = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.ColorAndOpacity(this, &FInstancedStructWrapperDetails::GetFontColor)
		.BorderBackgroundColor(this, &FInstancedStructWrapperDetails::GetBorderColor)
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			InternalWidget.ToSharedRef()
		];

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
	FInstancedStructWrapper* Wrapper = nullptr;
	const UScriptStruct* CommonStruct = nullptr;
	FProperty* Property = StructPropertyHandle->GetProperty();
	const FPropertyAccess::Result Result = GetStructData(StructPropertyHandle, CommonStruct, Wrapper);

	TSharedPtr<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(nullptr, nullptr);
	TArray<TSharedPtr<IPropertyHandle>> ChildPropertyHandles = StructPropertyHandle->AddChildStructure(StructOnScope.ToSharedRef());

	// 因为是伪造的，返回结果为空，但此时StructPropertyHandle的ChildNode不为空
	TSharedPtr<IPropertyHandle> TempPropHandle = StructPropertyHandle->GetChildHandle(0);
	TSharedPtr<FPropertyNode> PropertyNode = StaticCastSharedRef<FPropertyHandleBase>(TempPropHandle.ToSharedRef())->GetPropertyNode();

	// 伪造临时InstancedStruct属性
	FStructProperty* TempProp = new FStructProperty(Property->GetOwnerUObject(), FName(TEXT("InstancedStruct")), RF_Public);
	TempProp->Struct = FInstancedStruct::StaticStruct();

	// 注入临时Property
	PRIVATE_GET(PropertyNode.Get(), Property) = TempProp;

	TSharedRef<FInstancedStructWrapperDataDetails> DataDetails = MakeShared<FInstancedStructWrapperDataDetails>(StructPropertyHandle, TempPropHandle);

	StructBuilder.AddCustomBuilder(DataDetails);
}

void FInstancedStructWrapperDetails::OnTextCommitted(const FText& NewLabel, ETextCommit::Type CommitType)
{
	if (!StructProperty.IsValid())
	{
		return;
	}
	StructProperty->NotifyPreChange();

	StructProperty->EnumerateRawData([&NewLabel](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (FInstancedStructWrapper* Wrapper = static_cast<FInstancedStructWrapper*>(RawData))
			{
				Wrapper->DisplayNameOverride = NewLabel;
			}
			return true;
		});

	StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
	StructProperty->NotifyFinishedChangingProperties();
}

FText FInstancedStructWrapperDetails::GetCommentAsText() const
{
	if (!StructProperty.IsValid())
	{
		return FText();
	}

	FInstancedStructWrapper* Wrapper = nullptr;
	const UScriptStruct* CommonStruct = nullptr;
	const FPropertyAccess::Result Result = GetStructData(StructProperty, CommonStruct, Wrapper);

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
	const FPropertyAccess::Result Result = GetStructData(StructProperty, CommonStruct, Wrapper);

	if (CommonStruct && Result == FPropertyAccess::Success)
	{
		return CommonStruct->GetToolTipText();
	}

	return GetCommentAsText();
}

FSlateColor FInstancedStructWrapperDetails::GetBorderColor() const
{
	static const FName NAME_BorderColor = "BorderColor";

	FInstancedStructWrapper* Wrapper = nullptr;
	const UScriptStruct* CommonStruct = nullptr;
	const FPropertyAccess::Result Result = GetStructData(StructProperty, CommonStruct, Wrapper);

	if (CommonStruct)
	{
		const FString& BorderColor = CommonStruct->GetMetaData(NAME_BorderColor);
		if (!BorderColor.IsEmpty())
		{
			return FLinearColor(COLOR(*BorderColor));
		}
	}

	return FLinearColor::Transparent;
}

FLinearColor FInstancedStructWrapperDetails::GetFontColor() const
{
	static const FName NAME_FontColor = "FontColor";

	FInstancedStructWrapper* Wrapper = nullptr;
	const UScriptStruct* CommonStruct = nullptr;
	const FPropertyAccess::Result Result = GetStructData(StructProperty, CommonStruct, Wrapper);

	if (CommonStruct)
	{
		const FString& FontColor = CommonStruct->GetMetaData(NAME_FontColor);
		if (!FontColor.IsEmpty())
		{
			return FLinearColor(COLOR(*FontColor));
		}
	}

	return FLinearColor::White;
}

//////////////////////////////////////////////////////////////////////////
// FInstancedStructWrapperDataDetails

PRIVATE_DEFINE(FInstancedStructDataDetails, TSharedPtr<IPropertyHandle>, StructProperty);

FInstancedStructWrapperDataDetails::FInstancedStructWrapperDataDetails(TSharedPtr<IPropertyHandle> InStructProperty, TSharedPtr<IPropertyHandle> TempStructProperty)
	: FInstancedStructDataDetails(TempStructProperty)
{
#if DO_CHECK
	FStructProperty* StructProp = CastFieldChecked<FStructProperty>(InStructProperty->GetProperty());
	check(StructProp);
	check(StructProp->Struct == FInstancedStructWrapper::StaticStruct());
#endif


	PRIVATE_GET(this, StructProperty) = InStructProperty;
}



//////////////////////////////////////////////////////////////////////////
// FInstancedStructWrapperContainerViewModel
FInstancedStructWrapperContainerViewModel::FInstancedStructWrapperContainerViewModel(TSharedRef<IPropertyHandle> InPropertyHandle)
	: PropertyHandle(InPropertyHandle)
{
	const FInstancedStructContainerWrapper* Container = GetContainer();

	for (int32 Index = 0; Index < Container->Num(); ++Index)
	{
		GetContainerProxy().Data.Emplace((*Container)[Index]);
		GetContainerProxy().Data.Last().DisplayNameOverride = Container->DisplayNameOverride[Index];
	}
}

FInstancedStructContainerWrapper* FInstancedStructWrapperContainerViewModel::GetContainer()
{
	FInstancedStructContainerWrapper* Wrapper = nullptr;
	//FProperty* Property = PropertyHandle->GetProperty();
	const FPropertyAccess::Result Result = GetStructContainerData(PropertyHandle, Wrapper);

	return Wrapper;
}


void FInstancedStructWrapperContainerViewModel::OnContainerProxyValueChanged()
{
	FInstancedStructContainerWrapper* Container = GetContainer();
	Container->Empty();
	Container->DisplayNameOverride.Empty();

	TArray<FInstancedStruct> StructDatas;
	for (auto& Wrapper : GetContainerProxy().Data)
	{
		StructDatas.Add((FInstancedStruct)Wrapper);
		Container->DisplayNameOverride.Add(Wrapper.DisplayNameOverride);
	}
	Container->Append(StructDatas);

	OnContainerChanged.Broadcast();

	UpdateChildMetaData();
}

void FInstancedStructWrapperContainerViewModel::UpdateChildMetaData()
{
	static const FName NAME_ExcludeBaseStruct = "ExcludeBaseStruct";
	static const FName NAME_BaseStruct = "BaseStruct";

	// 多获取一次，剔除AddChildStructure的副作用
	TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(0)->GetChildHandle(0);
	FProperty* Property = PropertyHandle->GetProperty();
	FProperty* ChildProperty = ChildHandle->GetProperty();

	if (Property->HasMetaData(NAME_ExcludeBaseStruct))
	{
		ChildProperty->SetMetaData(NAME_ExcludeBaseStruct, *(NAME_ExcludeBaseStruct.ToString()));
	}

	const FString& BaseStructName = Property->GetMetaData(NAME_BaseStruct);
	if (!BaseStructName.IsEmpty())
	{
		ChildProperty->SetMetaData(NAME_BaseStruct, *BaseStructName);
	}
}

//////////////////////////////////////////////////////////////////////////
// FInstancedStructWrapperContainerDetails
TSharedRef<IPropertyTypeCustomization> FInstancedStructWrapperContainerDetails::MakeInstance()
{
	TSharedRef<FInstancedStructWrapperContainerDetails> WrapperDetails = MakeShared<FInstancedStructWrapperContainerDetails>();
	return WrapperDetails;
}

FInstancedStructWrapperContainerDetails::FInstancedStructWrapperContainerDetails()
{

}

void FInstancedStructWrapperContainerDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	ContainerViewModel = MakeShared<FInstancedStructWrapperContainerViewModel>(StructPropertyHandle);
}

void FInstancedStructWrapperContainerDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	OverrideProperty(StructPropertyHandle, StructBuilder);
}

void FInstancedStructWrapperContainerDetails::OverrideProperty(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder)
{
	TSharedRef<FInstancedStructContainerProvider> NewStructProvider = MakeShared<FInstancedStructContainerProvider>(ContainerViewModel);
	TArray<TSharedPtr<IPropertyHandle>> ChildProperties = StructPropertyHandle->AddChildStructure(NewStructProvider);

	check(ChildProperties.Num());

	TSharedPtr<IPropertyHandle> ChildHandle = ChildProperties[0];

	ContainerViewModel->UpdateChildMetaData();
	ChildHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(ContainerViewModel.ToSharedRef(), &FInstancedStructWrapperContainerViewModel::OnContainerProxyValueChanged));
	ChildHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(ContainerViewModel.ToSharedRef(), &FInstancedStructWrapperContainerViewModel::OnContainerProxyValueChanged));

	IDetailPropertyRow& Row = StructBuilder.AddProperty(ChildHandle.ToSharedRef());

	Row.DisplayName(StructPropertyHandle->GetPropertyDisplayName());
	Row.ToolTip(StructPropertyHandle->GetToolTipText());

	StructPropertyHandle = ChildHandle.ToSharedRef();
}

#undef LOCTEXT_NAMESPACE