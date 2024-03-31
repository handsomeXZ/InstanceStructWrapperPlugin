#include "Example/RichTextBlockSchemaDecorator.h"

#include "Widgets/SOverlay.h"
#include "Components/RichTextBlock.h"
#include "Components/RichTextBlockImageDecorator.h"

#define LOCTEXT_NAMESPACE "SchemaDecorator"

class FRichInlineImage : public FRichTextDecorator
{
public:
	FRichInlineImage(URichTextBlock* InOwner, URichTextBlockSchemaDecorator* InDecorator)
		: FRichTextDecorator(InOwner)
		, Decorator(InDecorator)
	{
	}

	virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override
	{
		FName ParseName = GetParseName();
		FName ParseMetaData = GetParseMetaData();
		if (ParseName.IsNone() || ParseMetaData.IsNone())
		{
			return false;
		}

		if (RunParseResult.Name == ParseName.ToString() && RunParseResult.MetaData.Contains(ParseMetaData.ToString()))
		{
			const FTextRange& IdRange = RunParseResult.MetaData[ParseMetaData.ToString()];
			const FString TagName = Text.Mid(IdRange.BeginIndex, IdRange.EndIndex - IdRange.BeginIndex);

			if (Decorator->Chooser.IsValid())
			{
				if (FSchemaDecoratorChooserBase* Chooser = Decorator->Chooser.GetMutablePtr<FSchemaDecoratorChooserBase>())
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
		if (ParseName.IsNone() || ParseMetaData.IsNone())
		{
			return TSharedPtr<SWidget>();
		}

		FInstancedStructContainerWrapper* TargetDataRow = nullptr;
		if (Decorator->Chooser.IsValid())
		{
			if (FSchemaDecoratorChooserBase* Chooser = Decorator->Chooser.GetMutablePtr<FSchemaDecoratorChooserBase>())
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
				TSharedPtr<SWidget> Widget = OverlayStyle->GetStyleWidget(Decorator);
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
		if (Decorator->Chooser.IsValid())
		{
			if (FSchemaDecoratorChooserBase* Chooser = Decorator->Chooser.GetMutablePtr<FSchemaDecoratorChooserBase>())
			{
				return Chooser->GetParseName();
			}
		}

		return NAME_None;
	}

	FName GetParseMetaData() const
	{
		if (Decorator->Chooser.IsValid())
		{
			if (FSchemaDecoratorChooserBase* Chooser = Decorator->Chooser.GetMutablePtr<FSchemaDecoratorChooserBase>())
			{
				return Chooser->GetParseMetaData();
			}
		}

		return NAME_None;
	}

private:
	URichTextBlockSchemaDecorator* Decorator;
};

/////////////////////////////////////////////////////
// URichTextBlockSchemaDecorator
URichTextBlockSchemaDecorator::URichTextBlockSchemaDecorator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedPtr<ITextDecorator> URichTextBlockSchemaDecorator::CreateDecorator(URichTextBlock* InOwner)
{
	return MakeShareable(new FRichInlineImage(InOwner, this));
}

/////////////////////////////////////////////////////
// FInstancedStructContainerWrapper
FInstancedStructContainerWrapper* FSchemaDecoratorChooserBase::GetTargetDataRow(FName InName)
{
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
// Overlay Style
//////////////////////////////////////////////////////////////////////////

TSharedPtr<SWidget> FSchemaDecoratorOverlayStyle_Text::GetStyleWidget(URichTextBlockSchemaDecorator* Decorator) const
{
	if (!IsValid(Decorator) || !IsValid(Decorator->TextStyle))
	{
		return TSharedPtr<SWidget>();
	}

	FString ContextString;
	if (FRichTextStyleRow* TextStyleRow = Decorator->TextStyle->FindRow<FRichTextStyleRow>(Style, ContextString, true))
	{
		return SNew(STextBlock)
			.Font(TextStyleRow->TextStyle.Font)
			.Text(Text);
	}

	return TSharedPtr<SWidget>();
}

TSharedPtr<SWidget> FSchemaDecoratorOverlayStyle_Image::GetStyleWidget(URichTextBlockSchemaDecorator* Decorator) const
{
	if (!IsValid(Decorator) || !IsValid(Decorator->ImageStyle))
	{
		return TSharedPtr<SWidget>();
	}

	FString ContextString;
	if (FRichImageRow* ImageStyleRow = Decorator->ImageStyle->FindRow<FRichImageRow>(Style, ContextString, true))
	{
		return SNew(SImage)
			.Image(&(ImageStyleRow->Brush));
	}

	return TSharedPtr<SWidget>();
}

TSharedPtr<SWidget> FSchemaDecoratorOverlayStyle_UserWidget::GetStyleWidget(URichTextBlockSchemaDecorator* Decorator) const
{
	return TSharedPtr<SWidget>();
}
//////////////////////////////////////////////////////////////////////////
// ~End Overlay Style
//////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////
// Example
//////////////////////////////////////////////////////////////////////////

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