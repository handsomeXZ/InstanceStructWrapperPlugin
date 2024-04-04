#include "InstancedStructWrapperDetails.h"

#include "IDetailChildrenBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "DetailLayoutBuilder.h"
#include "Styling/SlateStyleRegistry.h"
#include "ScopedTransaction.h"

#include "IStructureDataProvider.h"
#include "PropertyNode.h"
#include "PropertyHandleImpl.h"

#include "PrivateAccessor.h"

#define LOCTEXT_NAMESPACE "InstancedStructWrapperDetails"

PRIVATE_DEFINE(SComboButton, SHorizontalBox::FSlot*, ButtonContentSlot);
PRIVATE_DEFINE(SComboButton, TSharedPtr<SButton>, ButtonPtr);
PRIVATE_DEFINE(SComboButton, TSharedPtr<SHorizontalBox>, HBox);
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

TSharedPtr<SWidget> UInstancedStructSchemaBase::GetButtonContentOverride(TSharedRef<IPropertyHandle> StructProperty) const
{
	return nullptr;
}

TSharedPtr<SWidget> UInstancedStructSchemaBase::GetButtonContentExtension(TSharedRef<IPropertyHandle> StructProperty) const
{
	return nullptr;
}

TSharedPtr<SWidget> UInstancedStructSchemaBase::GetContainerTopExtension(TSharedRef<IPropertyHandle> StructProperty, TSharedRef<FInstancedStructWrapperContainerViewModel> ViewModel) const
{
	return nullptr;
}

FInstancedStructWrapperEditorStyle::FInstancedStructWrapperEditorStyle()
	: FSlateStyleSet(TEXT("InstancedStructWrapperEditorStyle"))
{
	FComboButtonStyle ComboButtonStyle = FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("ComboButton");
	ComboButtonStyle.ButtonStyle.NormalPadding = FMargin(0);
	ComboButtonStyle.ButtonStyle.PressedPadding = FMargin(0);
	ComboButtonStyle.ButtonStyle.Normal = *FAppStyle::Get().GetBrush("NoBrush");
	ComboButtonStyle.ButtonStyle.Hovered = *FAppStyle::Get().GetBrush("NoBrush");
	ComboButtonStyle.ButtonStyle.Pressed = *FAppStyle::Get().GetBrush("NoBrush");

	Set("InstancedStructWrapperEditorStyle.ComboButton", ComboButtonStyle);
}

void FInstancedStructWrapperEditorStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FInstancedStructWrapperEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

FInstancedStructWrapperEditorStyle& FInstancedStructWrapperEditorStyle::Get()
{
	static FInstancedStructWrapperEditorStyle Instance;
	return Instance;
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
	// 初始化
	StructProperty = StructPropertyHandle;
	InitSchemaClass();

	// 获取默认Widget
	InstancedStructDetails->CustomizeHeader(StructPropertyHandle, HeaderRow, StructCustomizationUtils);
	FDetailWidgetDecl& WidgetDecl = HeaderRow.ValueContent();
	
	// 重定义Widget
	CustomizeValueWidgetBySchema(WidgetDecl);
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

void FInstancedStructWrapperDetails::InitSchemaClass()
{
	FInstancedStructWrapper* Wrapper = nullptr;
	const UScriptStruct* CommonStruct = nullptr;
	const FPropertyAccess::Result Result = GetStructData(StructProperty, CommonStruct, Wrapper);

	if (!CommonStruct)
	{
		static const FName NAME_BaseStruct = "BaseStruct";
		{
			const FString& BaseStructName = StructProperty->GetMetaData(NAME_BaseStruct);
			if (!BaseStructName.IsEmpty())
			{
				CommonStruct = UClass::TryFindTypeSlow<UScriptStruct>(BaseStructName);
				if (!CommonStruct)
				{
					CommonStruct = LoadObject<UScriptStruct>(nullptr, *BaseStructName);
				}
			}
		}
	}


	static const FName NAME_SchemaClass = "SchemaClass";
	SchemaClass = nullptr;
	if (CommonStruct)
	{
		const FString& SchemaClassName = CommonStruct->GetMetaData(NAME_SchemaClass);
		if (!SchemaClassName.IsEmpty())
		{
			SchemaClass = UClass::TryFindTypeSlow<UClass>(SchemaClassName);
			if (!SchemaClass)
			{
				SchemaClass = LoadObject<UClass>(nullptr, *SchemaClassName);
			}
		}
	}
}

void FInstancedStructWrapperDetails::CustomizeValueWidgetBySchema(FDetailWidgetDecl& ValueWidgetDecl)
{
	TSharedPtr<SComboButton> InternalWidget = StaticCastSharedRef<SComboButton>(ValueWidgetDecl.Widget);

	FSlotBase* SlotBase = PRIVATE_GET(InternalWidget.Get(), ButtonContentSlot);
	TSharedPtr<SButton> ButtonPtr = PRIVATE_GET(InternalWidget.Get(), ButtonPtr);

	TSharedPtr<SHorizontalBox> MainHorizontalBox = PRIVATE_GET(InternalWidget.Get(), HBox);
	TSharedPtr<SHorizontalBox> ChildHorizontalBox = StaticCastSharedRef<SHorizontalBox>(PRIVATE_GET(SlotBase, Widget));


	// 重定义一些外观样式
	TSharedPtr<SHorizontalBox> ValueWidget;
	ValueWidgetDecl.Widget = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.ColorAndOpacity(this, &FInstancedStructWrapperDetails::GetFontColor)
		.BorderBackgroundColor(this, &FInstancedStructWrapperDetails::GetBorderColor)
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(ValueWidget, SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				InternalWidget.ToSharedRef()
			]
		];

	TSharedPtr<SWidget> ContentOverrideWidget = GetButtonContentOverride();
	TSharedPtr<SWidget> ContentExtensionWidget = GetButtonContentExtension();
	if (ContentExtensionWidget.IsValid())
	{
		ValueWidget->AddSlot()
		[
			ContentExtensionWidget.ToSharedRef()
		];
	}

	if (ContentOverrideWidget.IsValid())
	{
		// 清空外部容器的样式
		ValueWidgetDecl.MinWidth = 0.0f;
		ValueWidgetDecl.MaxWidth = 0.0f;
		ButtonPtr->SetContentPadding(FMargin(0));
		ButtonPtr->SetButtonStyle(&FInstancedStructWrapperEditorStyle::Get().GetWidgetStyle<FComboButtonStyle>("InstancedStructWrapperEditorStyle.ComboButton").ButtonStyle);

		MainHorizontalBox->ClearChildren();	// ClearChildren的同时可以将DownArrow清除
		MainHorizontalBox->AddSlot()
		.Padding(0)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SBox)
				.MinDesiredWidth(4.0f)
				.MinDesiredHeight(28.0f)
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					ContentOverrideWidget.ToSharedRef()
				]
			]
		];
	}
	else
	{
		// 如果没有重写内容，就使用默认Widget，同时提供可编辑文本框

		TPanelChildren<SBoxPanel::FSlot>& BoxChildren = PRIVATE_GET(ChildHorizontalBox.Get(), Children, 1);
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

TSharedPtr<SWidget> FInstancedStructWrapperDetails::GetButtonContentOverride() const
{
	if (IsValid(SchemaClass) && SchemaClass->IsChildOf(UInstancedStructSchemaBase::StaticClass()) && StructProperty.IsValid())
	{
		return SchemaClass->GetDefaultObject<UInstancedStructSchemaBase>()->GetButtonContentOverride(StructProperty.ToSharedRef());
	}

	return nullptr;
}

TSharedPtr<SWidget> FInstancedStructWrapperDetails::GetButtonContentExtension() const
{
	if (IsValid(SchemaClass) && SchemaClass->IsChildOf(UInstancedStructSchemaBase::StaticClass()) && StructProperty.IsValid())
	{
		return SchemaClass->GetDefaultObject<UInstancedStructSchemaBase>()->GetButtonContentExtension(StructProperty.ToSharedRef());
	}

	return nullptr;
}

void FInstancedStructWrapperDetails::OnTextCommitted(const FText& NewLabel, ETextCommit::Type CommitType)
{
	if (!StructProperty.IsValid())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("InstancedStructWrapper", "OnTextCommitted"));

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
	, PropertyOuter(nullptr)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	PropertyOuter = OuterObjects[0];

	const FInstancedStructContainerWrapper* Container = GetContainer();
	GetContainerProxy().Data.Empty(Container->Num());

	for (int32 Index = 0; Index < Container->Num(); ++Index)
	{
		GetContainerProxy().Data.Emplace((*Container)[Index]);
		GetContainerProxy().Data.Last().DisplayNameOverride = Container->DisplayNameOverride.IsEmpty() ? FText() : Container->DisplayNameOverride[Index];
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
	FScopedTransaction Transaction(TEXT("InstancedStructWrapperContainer"), LOCTEXT("InstancedStructWrapperContainer", "OnPropertyValueChanged"), PropertyOuter);

	PropertyHandle->NotifyPreChange();

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

		UpdateChildMetaData();

		OnContainerChanged.Broadcast();
	}

	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();

	// 理论上，仅在添加/删除和修改类型时，才会执行OnContainerProxyValueChanged进行重构。
	// 如果都进行重新创建，开销太大了，不过因为只是编辑器行为，先不管了。
}

void FInstancedStructWrapperContainerViewModel::OnContainerValueChanged(UObject* Outer, FPropertyChangedEvent& ChangedEvent)
{	
	if (Outer && Outer == PropertyOuter && ChangedEvent.ChangeType == EPropertyChangeType::Unspecified)
	{
		// 此时应当为Undo，不应该再进行属性通知了
		const FInstancedStructContainerWrapper* Container = GetContainer();
		GetContainerProxy().Data.Empty(Container->Num());

		for (int32 Index = 0; Index < Container->Num(); ++Index)
		{
			GetContainerProxy().Data.Emplace((*Container)[Index]);
			GetContainerProxy().Data.Last().DisplayNameOverride = Container->DisplayNameOverride.IsEmpty() ? FText() : Container->DisplayNameOverride[Index];
		}

		OnContainerChanged.Broadcast();
	}
}

void FInstancedStructWrapperContainerViewModel::UpdateChildMetaData()
{
	static const FName NAME_ExcludeBaseStruct = "ExcludeBaseStruct";
	static const FName NAME_BaseStruct = "BaseStruct";

	// 多获取一次，剔除AddChildStructure的副作用
	TSharedPtr<IPropertyHandle> ArrayHandle = PropertyHandle->GetChildHandle(0)->GetChildHandle(0);
	FProperty* Property = PropertyHandle->GetProperty();
	FProperty* ChildProperty = ArrayHandle->GetProperty();

	if (PropertyHandle->HasMetaData(NAME_ExcludeBaseStruct))
	{
		ChildProperty->SetMetaData(NAME_ExcludeBaseStruct, *(NAME_ExcludeBaseStruct.ToString()));
	}

	const FString& BaseStructName = PropertyHandle->GetMetaData(NAME_BaseStruct);
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

FInstancedStructWrapperContainerDetails::~FInstancedStructWrapperContainerDetails()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnObjectPropertyChangedHandle);
}

void FInstancedStructWrapperContainerDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	ContainerViewModel = MakeShared<FInstancedStructWrapperContainerViewModel>(StructPropertyHandle);
	
	OnObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddThreadSafeSP(ContainerViewModel.ToSharedRef(), &FInstancedStructWrapperContainerViewModel::OnContainerValueChanged);

	InitSchemaClass();
}

void FInstancedStructWrapperContainerDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedPtr<SWidget> TopExtensionWidget = GetContainerTopExtension();
	if (TopExtensionWidget.IsValid())
	{
		FDetailWidgetRow& ExtensionRow = StructBuilder.AddCustomRow(LOCTEXT("InstancedStructWrapperContainerDetails", "Extension"));
		ExtensionRow.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("InstancedStructWrapperContainerDetails", "Extension"))
		]
		.ValueContent()
		[
			TopExtensionWidget.ToSharedRef()
		];
	}

	OverrideProperty(StructPropertyHandle, StructBuilder);
}

void FInstancedStructWrapperContainerDetails::OverrideProperty(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder)
{
	TSharedRef<FInstancedStructContainerProvider> NewStructProvider = MakeShared<FInstancedStructContainerProvider>(ContainerViewModel);
	TArray<TSharedPtr<IPropertyHandle>> ChildProperties = StructPropertyHandle->AddChildStructure(NewStructProvider);

	check(ChildProperties.Num());

	// 这里取第一个属性，FInstancedStructContainerArray::Data
	TSharedPtr<IPropertyHandle> ArrayHandle = ChildProperties[0];

	ContainerViewModel->UpdateChildMetaData();
	ArrayHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(ContainerViewModel.ToSharedRef(), &FInstancedStructWrapperContainerViewModel::OnContainerProxyValueChanged));
	ArrayHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(ContainerViewModel.ToSharedRef(), &FInstancedStructWrapperContainerViewModel::OnContainerProxyValueChanged));

	IDetailPropertyRow& Row = StructBuilder.AddProperty(ArrayHandle.ToSharedRef());

	Row.DisplayName(StructPropertyHandle->GetPropertyDisplayName());
	Row.ToolTip(StructPropertyHandle->GetToolTipText());

	// 对于TArray类型的数据，不允许CustomWidget，因为它每次更新成员都需要重新创建SPropertyValueWidget。
}

void FInstancedStructWrapperContainerDetails::InitSchemaClass()
{
	const UScriptStruct* CommonStruct = nullptr;
	static const FName NAME_BaseStruct = "BaseStruct";
	{
		const FString& BaseStructName = ContainerViewModel->GetPropertyHandle()->GetMetaData(NAME_BaseStruct);
		if (!BaseStructName.IsEmpty())
		{
			CommonStruct = UClass::TryFindTypeSlow<UScriptStruct>(BaseStructName);
			if (!CommonStruct)
			{
				CommonStruct = LoadObject<UScriptStruct>(nullptr, *BaseStructName);
			}
		}
	}


	static const FName NAME_SchemaClass = "SchemaClass";
	SchemaClass = nullptr;
	if (CommonStruct)
	{
		const FString& SchemaClassName = CommonStruct->GetMetaData(NAME_SchemaClass);
		if (!SchemaClassName.IsEmpty())
		{
			SchemaClass = UClass::TryFindTypeSlow<UClass>(SchemaClassName);
			if (!SchemaClass)
			{
				SchemaClass = LoadObject<UClass>(nullptr, *SchemaClassName);
			}
		}
	}
}

TSharedPtr<SWidget> FInstancedStructWrapperContainerDetails::GetContainerTopExtension() const
{
	if (IsValid(SchemaClass) && SchemaClass->IsChildOf(UInstancedStructSchemaBase::StaticClass()) && ContainerViewModel.IsValid())
	{
		return SchemaClass->GetDefaultObject<UInstancedStructSchemaBase>()->GetContainerTopExtension(ContainerViewModel->GetPropertyHandle().ToSharedRef(), ContainerViewModel.ToSharedRef());
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE