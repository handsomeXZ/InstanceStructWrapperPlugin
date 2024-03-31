#include "Example/RichTextBlockSchemaDecoratorEditor.h"

#include "UMGStyle.h"
#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "SchemaDecoratorEditor"

//////////////////////////////////////////////////////////////////////////
// Overlay Style Schema
//////////////////////////////////////////////////////////////////////////
TSharedPtr<SWidget> USchemaDecoratorOverlayStyle_BaseSchema::GetButtonContentOverride(TSharedRef<IPropertyHandle> StructProperty) const
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("FloatingBorder"))
		.BorderBackgroundColor(FLinearColor(FColor(81, 81, 81)))
		.Cursor(EMouseCursor::Hand)
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(6.0f, 0.0f))
			[
				SNew(SImage)
				.Image(FUMGStyle::Get().GetBrush("ClassIcon.RichTextBlock"))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(3.0f, 0.0f))
			.AutoWidth()
			[
				SNew(SBox)
				.MinDesiredHeight(28.0f)
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("FloatingBorder"))
					.BorderBackgroundColor(FLinearColor(FColor(53, 53, 53)))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Padding(FMargin(6.0f, 0.0f))
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor(FColor(255, 56, 56)))
						.Text(LOCTEXT("BaseSchema", "None Rich"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Justification(ETextJustify::Center)
					]
				]
			]
		];
}

TSharedPtr<SWidget> USchemaDecoratorOverlayStyle_TextSchema::GetButtonContentOverride(TSharedRef<IPropertyHandle> StructProperty) const
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("FloatingBorder"))
		.BorderBackgroundColor(FLinearColor(FColor(81, 81, 81)))
		.Cursor(EMouseCursor::Hand)
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(6.0f, 0.0f))
			[
				SNew(SImage)
				.Image(FUMGStyle::Get().GetBrush("ClassIcon.TextBlock"))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(3.0f, 0.0f))
			.AutoWidth()
			[
				SNew(SBox)
				.MinDesiredHeight(28.0f)
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("FloatingBorder"))
					.BorderBackgroundColor(FLinearColor(FColor(53, 53, 53)))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Padding(FMargin(6.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TextSchema", "Text"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Justification(ETextJustify::Center)
					]
				]
			]
		];
}

TSharedPtr<SWidget> USchemaDecoratorOverlayStyle_ImageSchema::GetButtonContentOverride(TSharedRef<IPropertyHandle> StructProperty) const
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("FloatingBorder"))
		.BorderBackgroundColor(FLinearColor(FColor(41, 41, 41)))
		.Cursor(EMouseCursor::Hand)
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(6.0f, 0.0f))
			[
				SNew(SImage)
				.Image(FUMGStyle::Get().GetBrush("ClassIcon.Image"))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(3.0f, 0.0f))
			.AutoWidth()
			[
				SNew(SBox)
				.MinDesiredHeight(28.0f)
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("FloatingBorder"))
					.BorderBackgroundColor(FLinearColor(FColor(53, 53, 53)))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Padding(FMargin(6.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ImageSchema", "Image"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Justification(ETextJustify::Center)
					]
				]
				
			]
		];
}

TSharedPtr<SWidget> USchemaDecoratorOverlayStyle_UserWidgetSchema::GetButtonContentOverride(TSharedRef<IPropertyHandle> StructProperty) const
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("FloatingBorder"))
		.BorderBackgroundColor(FLinearColor(FColor(41, 41, 41)))
		.Cursor(EMouseCursor::Hand)
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(6.0f, 0.0f))
			[
				SNew(SImage)
				.Image(FUMGStyle::Get().GetBrush("ClassIcon.Button"))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(3.0f, 0.0f))
			.AutoWidth()
			[
				SNew(SBox)
				.MinDesiredHeight(28.0f)
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("FloatingBorder"))
					.BorderBackgroundColor(FLinearColor(FColor(53, 53, 53)))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Padding(FMargin(6.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("UserWidgetSchema", "UserWidget"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Justification(ETextJustify::Center)
					]
				]
			]
		];
}

//////////////////////////////////////////////////////////////////////////
// ~End Overlay Style Schema
//////////////////////////////////////////////////////////////////////////



#undef LOCTEXT_NAMESPACE