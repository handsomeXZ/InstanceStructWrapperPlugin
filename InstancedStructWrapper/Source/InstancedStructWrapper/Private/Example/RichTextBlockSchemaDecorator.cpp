#include "Example/RichTextBlockSchemaDecorator.h"

#include "Widgets/SOverlay.h"
#include "Components/RichTextBlock.h"
#include "Components/RichTextBlockImageDecorator.h"
#include "Blueprint/UserWidget.h"

#define LOCTEXT_NAMESPACE "SchemaDecorator"

/////////////////////////////////////////////////////
// URichTextBlockSchemaDecorator
URichTextBlockSchemaDecorator::URichTextBlockSchemaDecorator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, StyleSheet(nullptr)
	, bIsEnableSlateForwardExtension(false)
	, bIsEnableSlateBackwardExtension(false)
{
}

TSharedPtr<ITextDecorator> URichTextBlockSchemaDecorator::CreateDecorator(URichTextBlock* InOwner)
{
	if (IsValid(StyleSheet))
	{
		bIsEnableSlateForwardExtension = StyleSheet->SlateExtensionStyle.ForwardAddition.bDefaultEnable;
		bIsEnableSlateBackwardExtension = StyleSheet->SlateExtensionStyle.BackwardAddition.bDefaultEnable;
	}

	return MakeShared<FRichSchemaDecorator>(InOwner, StyleSheet, this);
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
			.TextStyle(&(TextStyleRow->TextStyle))
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