#include "Example/RichTextBlockSchemaDecorator.h"

#include "Widgets/SOverlay.h"
#include "Components/RichTextBlock.h"
#include "Components/RichTextBlockImageDecorator.h"
#include "Blueprint/UserWidget.h"

#define LOCTEXT_NAMESPACE "SchemaDecorator"

class FRichInlineImage : public FRichTextDecorator
{
public:
	FRichInlineImage(URichTextBlock* InOwner, URichTextBlockSchemaDecoratorStyleSheet* InStyleSheet)
		: FRichTextDecorator(InOwner)
		, StyleSheet(InStyleSheet)
	{
	}

	virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override
	{
		FName ParseName = GetParseName();
		FName ParseMetaData = GetParseMetaData();
		if (ParseName.IsNone() || ParseMetaData.IsNone() || !IsValid(StyleSheet))
		{
			return false;
		}

		if (RunParseResult.Name == ParseName.ToString() && RunParseResult.MetaData.Contains(ParseMetaData.ToString()))
		{
			const FTextRange& IdRange = RunParseResult.MetaData[ParseMetaData.ToString()];
			const FString TagName = Text.Mid(IdRange.BeginIndex, IdRange.EndIndex - IdRange.BeginIndex);

			if (StyleSheet->Chooser.IsValid())
			{
				if (FSchemaDecoratorChooserBase* Chooser = StyleSheet->Chooser.GetMutablePtr<FSchemaDecoratorChooserBase>())
				{
					return Chooser->GetTargetDataRow(*TagName) != nullptr;
				}
			}
		}

		return false;
	}

protected:
	virtual TSharedPtr<SWidget> CreateDecoratorWidget(const FTextRunInfo& RunInfo, const FTextBlockStyle& TextStyle) const override
	{
		FName ParseName = GetParseName();
		FName ParseMetaData = GetParseMetaData();
		FString TagName = RunInfo.MetaData[ParseMetaData.ToString()];
		if (ParseName.IsNone() || ParseMetaData.IsNone() || !IsValid(StyleSheet))
		{
			return TSharedPtr<SWidget>();
		}

		FInstancedStructContainerWrapper* TargetDataRow = nullptr;
		if (StyleSheet->Chooser.IsValid())
		{
			if (FSchemaDecoratorChooserBase* Chooser = StyleSheet->Chooser.GetMutablePtr<FSchemaDecoratorChooserBase>())
			{
				TargetDataRow = Chooser->GetTargetDataRow(*TagName);
			}
		}

		if (!TargetDataRow)
		{
			return TSharedPtr<SWidget>();
		}

		TSharedPtr<SOverlay> Overlay = SNew(SOverlay);
		for (auto It = TargetDataRow->begin(); It; ++It)
		{
			FStructView StructView = *It;
			if (FSchemaDecoratorOverlayStyleBase* OverlayStyle = StructView.GetPtr<FSchemaDecoratorOverlayStyleBase>())
			{
				TSharedPtr<SWidget> Widget = OverlayStyle->GetStyleWidget(StyleSheet);
				if (Widget.IsValid())
				{
					Overlay->AddSlot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						Widget.ToSharedRef()
					];
				}
			}
		}

		return Overlay;
	}

	FName GetParseName() const
	{
		if (StyleSheet->Chooser.IsValid())
		{
			if (FSchemaDecoratorChooserBase* Chooser = StyleSheet->Chooser.GetMutablePtr<FSchemaDecoratorChooserBase>())
			{
				return Chooser->GetParseName();
			}
		}

		return NAME_None;
	}

	FName GetParseMetaData() const
	{
		if (StyleSheet->Chooser.IsValid())
		{
			if (FSchemaDecoratorChooserBase* Chooser = StyleSheet->Chooser.GetMutablePtr<FSchemaDecoratorChooserBase>())
			{
				return Chooser->GetParseMetaData();
			}
		}

		return NAME_None;
	}

private:
	URichTextBlockSchemaDecoratorStyleSheet* StyleSheet;
};

/////////////////////////////////////////////////////
// URichTextBlockSchemaDecorator
URichTextBlockSchemaDecorator::URichTextBlockSchemaDecorator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, StyleSheet(nullptr)
{
}

TSharedPtr<ITextDecorator> URichTextBlockSchemaDecorator::CreateDecorator(URichTextBlock* InOwner)
{
	return MakeShareable(new FRichInlineImage(InOwner, StyleSheet));
}

//////////////////////////////////////////////////////////////////////////
// Overlay Style
//////////////////////////////////////////////////////////////////////////

TSharedPtr<SWidget> FSchemaDecoratorOverlayStyle_Text::GetStyleWidget(URichTextBlockSchemaDecoratorStyleSheet* StyleSheet) const
{
	if (!IsValid(StyleSheet) || !IsValid(StyleSheet->TextStyle))
	{
		return TSharedPtr<SWidget>();
	}

	FString ContextString;
	if (FRichTextStyleRow* TextStyleRow = StyleSheet->TextStyle->FindRow<FRichTextStyleRow>(Style, ContextString, true))
	{
		return SNew(STextBlock)
			.Font(TextStyleRow->TextStyle.Font)
			.Text(Text);
	}

	return TSharedPtr<SWidget>();
}

TSharedPtr<SWidget> FSchemaDecoratorOverlayStyle_Image::GetStyleWidget(URichTextBlockSchemaDecoratorStyleSheet* StyleSheet) const
{
	if (!IsValid(StyleSheet) || !IsValid(StyleSheet->ImageStyle))
	{
		return TSharedPtr<SWidget>();
	}

	FString ContextString;
	if (FRichImageRow* ImageStyleRow = StyleSheet->ImageStyle->FindRow<FRichImageRow>(Style, ContextString, true))
	{
		return SNew(SImage)
			.Image(&(ImageStyleRow->Brush));
	}

	return TSharedPtr<SWidget>();
}

TSharedPtr<SWidget> FSchemaDecoratorOverlayStyle_UserWidget::GetStyleWidget(URichTextBlockSchemaDecoratorStyleSheet* StyleSheet) const
{
	if (!IsValid(UserWidgetClass))
	{
		return TSharedPtr<SWidget>();
	}

	if (UUserWidget* Widget = NewObject<UUserWidget>(GetTransientPackage(), UserWidgetClass))
	{
		return Widget->TakeWidget();
	}

	return TSharedPtr<SWidget>();
}
//////////////////////////////////////////////////////////////////////////
// ~End Overlay Style
//////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////
// Example
//////////////////////////////////////////////////////////////////////////

// Common Name Chooser
FInstancedStructContainerWrapper* FSchemaDecoratorChooser_Common::GetTargetDataRow(FName InName)
{
	if (FInstancedStructContainerWrapper* WrapperPtr = Styles.Find(InName))
	{
		return WrapperPtr;
	}

	return nullptr;
}

// MappingName To Key
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
FInstancedStructContainerWrapper* FSchemaDecoratorChooser_MappingToKey::GetTargetDataRow(FName InName)
{
	UWorld* World = nullptr;
	for (auto WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType == EWorldType::Game || WorldContext.WorldType == EWorldType::PIE)
		{
			World = WorldContext.World();
			break;
		}
	}

	if (!World)
	{
		return nullptr;
	}


	UEnhancedInputLocalPlayerSubsystem* Subsystem = nullptr;
	if (UGameInstance* GI = World->GetGameInstance())
	{
		if (GI->GetFirstLocalPlayerController())
		{
			Subsystem = GI->GetFirstLocalPlayerController()->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		}
	}

	if (Subsystem)
	{
		// @TODO: GetPlayerMappedKey这个接口似乎要被遗弃了
		FKey Key = Subsystem->GetPlayerMappedKey(InName);
		if (FInstancedStructContainerWrapper* WrapperPtr = Styles.Find(Key))
		{
			return WrapperPtr;
		}
	}


	return nullptr;
}

// ~MappingName To Key

#undef LOCTEXT_NAMESPACE