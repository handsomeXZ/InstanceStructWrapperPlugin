#include "Example/RichTextBlockSchemaRun.h"

#include "Fonts/FontMeasure.h"
#include "Styling/SlateStyle.h"
#include "Styling/ISlateStyle.h"
#include "Framework/Text/ShapedTextCache.h"

#include "Example/RichTextBlockSchemaDecorator.h"
#include "PrivateAccessor.h"

PRIVATE_DEFINE(URichTextBlock, TSharedPtr<FSlateStyleSet>, StyleInstance);
PRIVATE_DEFINE(FCharacterList, FSlateFontKey, FontKey);

bool FRichSchemaDecorator::IsEnableSlateForwardExtension() const
{
	if (IsValidData())
	{
		return OwnerSchemaDecorator->IsEnableSlateForwardExtension();
	}

	return false;
}

bool FRichSchemaDecorator::IsEnableSlateBackwardExtension() const
{
	if (IsValidData())
	{
		return OwnerSchemaDecorator->IsEnableSlateBackwardExtension();
	}

	return false;
}

bool FRichSchemaDecorator::Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const
{
	if (!IsValidData())
	{
		return false;
	}

	// 只要配了数据，必定有效。

	return true;
}

bool FRichSchemaDecorator::SupportsDecoratorWidget(const FTextRunParseResults& RunParseResult, const FString& Text) const
{
	FName ParseName = GetParseName();
	FName ParseMetaData = GetParseMetaData();
	if (ParseName.IsNone() || ParseMetaData.IsNone() || !IsValidData())
	{
		return false;
	}

	if (RunParseResult.Name == ParseName.ToString() && RunParseResult.MetaData.Contains(ParseMetaData.ToString()))
	{
		const FTextRange& IdRange = RunParseResult.MetaData[ParseMetaData.ToString()];
		const FString TagName = Text.Mid(IdRange.BeginIndex, IdRange.EndIndex - IdRange.BeginIndex);

		if (FSchemaDecoratorChooserBase* Chooser = StyleSheet->Chooser.GetMutablePtr<FSchemaDecoratorChooserBase>())
		{
			return Chooser->GetTargetDataRow(*TagName) != nullptr;
		}
	}

	return false;
}

TSharedRef<ISlateRun> FRichSchemaDecorator::Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText, const TSharedRef< FString >& InOutModelText, const ISlateStyle* Style)
{
	// 数据有效性已经由Supports保证了。

	FTextRange ModelRange;
	ModelRange.BeginIndex = InOutModelText->Len();

	FTextRunInfo RunInfo(RunParseResult.Name, FText::FromString(OriginalText.Mid(RunParseResult.ContentRange.BeginIndex, RunParseResult.ContentRange.EndIndex - RunParseResult.ContentRange.BeginIndex)));
	for (const TPair<FString, FTextRange>& Pair : RunParseResult.MetaData)
	{
		RunInfo.MetaData.Add(Pair.Key, OriginalText.Mid(Pair.Value.BeginIndex, Pair.Value.EndIndex - Pair.Value.BeginIndex));
	}

	TSharedPtr<FSchemaSlateRunExtension> SlateRunExtension = CreateSlateRunExtension();

	const FTextBlockStyle& TextStyle = OwnerBlock->GetCurrentDefaultTextStyle();

	TSharedPtr<ISlateRun> SlateRun;
	
	if (SupportsDecoratorWidget(RunParseResult, OriginalText))
	{
		TSharedPtr<SWidget> DecoratorWidget = CreateDecoratorWidget(RunInfo, TextStyle);

		*InOutModelText += TEXT('\u200B'); // Zero-Width Breaking Space
		ModelRange.EndIndex = InOutModelText->Len();

		// Calculate the baseline of the text within the owning rich text
		// Requested on demand as the font may not be loaded right now
		const FSlateFontInfo Font = TextStyle.Font;
		const float ShadowOffsetY = FMath::Min(0.0f, TextStyle.ShadowOffset.Y);

		TAttribute<int16> GetBaseline = TAttribute<int16>::CreateLambda([Font, ShadowOffsetY]()
			{
				const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
				return FontMeasure->GetBaseline(Font) - ShadowOffsetY;
			});

		FSlateWidgetRun::FWidgetRunInfo WidgetRunInfo(DecoratorWidget.ToSharedRef(), GetBaseline);
		SlateRun = FSchemaSlateWidgetRun::Create(SlateRunExtension.ToSharedRef(), TextLayout, RunInfo, InOutModelText, WidgetRunInfo, ModelRange);
	}
	else
	{
		// Assume there's a text handler if widget is empty, if there isn't one it will just display an empty string
		FTextBlockStyle TempStyle = TextStyle;
		CreateDecoratorText(RunParseResult, OriginalText, TempStyle, *InOutModelText);

		ModelRange.EndIndex = InOutModelText->Len();
		SlateRun = FSchemaSlateTextRun::Create(SlateRunExtension.ToSharedRef(), RunInfo, InOutModelText, TempStyle, ModelRange);
	}

	return SlateRun.ToSharedRef();
}


TSharedPtr<SWidget> FRichSchemaDecorator::CreateDecoratorWidget(const FTextRunInfo& RunInfo, const FTextBlockStyle& TextStyle) const
{
	FName ParseName = GetParseName();
	FName ParseMetaData = GetParseMetaData();
	FString TagName = RunInfo.MetaData[ParseMetaData.ToString()];
	if (ParseName.IsNone() || ParseMetaData.IsNone() || !IsValidData())
	{
		return TSharedPtr<SWidget>();
	}

	FInstancedStructContainerWrapper* TargetDataRow = nullptr;
	if (FSchemaDecoratorChooserBase* Chooser = StyleSheet->Chooser.GetMutablePtr<FSchemaDecoratorChooserBase>())
	{
		TargetDataRow = Chooser->GetTargetDataRow(*TagName);
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

void FRichSchemaDecorator::CreateDecoratorText(const FTextRunParseResults& RunParseResult, const FString& OriginalText, FTextBlockStyle& InOutTextStyle, FString& InOutString) const
{
	if (!IsValidData())
	{
		return;
	}

	if (!(RunParseResult.Name.IsEmpty()) && PRIVATE_GET(OwnerBlock, StyleInstance)->HasWidgetStyle<FTextBlockStyle>(FName(*RunParseResult.Name)))
	{
		InOutString += OriginalText.Mid(RunParseResult.ContentRange.BeginIndex, RunParseResult.ContentRange.EndIndex - RunParseResult.ContentRange.BeginIndex);
		InOutTextStyle = PRIVATE_GET(OwnerBlock, StyleInstance)->GetWidgetStyle<FTextBlockStyle>(FName(*RunParseResult.Name));
	}
	else
	{
		InOutString += OriginalText.Mid(RunParseResult.OriginalRange.BeginIndex, RunParseResult.OriginalRange.EndIndex - RunParseResult.OriginalRange.BeginIndex);
		// 否则，使用DefaultTextStyle
	}
}

TSharedPtr<FSchemaSlateRunExtension> FRichSchemaDecorator::CreateSlateRunExtension()
{
	return MakeShared<FSchemaSlateRunExtension>(StyleSheet->SlateExtensionStyle, SharedThis(this));
}

bool FRichSchemaDecorator::IsValidData() const
{
	return IsValid(OwnerBlock) && IsValid(StyleSheet) && StyleSheet->Chooser.IsValid() && IsValid(OwnerSchemaDecorator);
}

FName FRichSchemaDecorator::GetParseName() const
{
	if (IsValidData())
	{
		if (FSchemaDecoratorChooserBase* Chooser = StyleSheet->Chooser.GetMutablePtr<FSchemaDecoratorChooserBase>())
		{
			return Chooser->GetParseName();
		}
	}

	return NAME_None;
}

FName FRichSchemaDecorator::GetParseMetaData() const
{
	if (IsValidData())
	{
		if (FSchemaDecoratorChooserBase* Chooser = StyleSheet->Chooser.GetMutablePtr<FSchemaDecoratorChooserBase>())
		{
			return Chooser->GetParseMetaData();
		}
	}

	return NAME_None;
}



//////////////////////////////////////////////////////////////////////////
FSchemaSlateRunExtension::FSchemaSlateRunExtension(const FSchemaSlateExtensionStyle& SlateExtensionStyle, TSharedPtr<FRichSchemaDecorator> InOwnerDecorator)
	: ExtensionStyle(SlateExtensionStyle)
	, OwnerDecorator(InOwnerDecorator)
{

}

bool FSchemaSlateRunExtension::SupportsForward() const
{
	if (OwnerDecorator.IsValid())
	{
		return OwnerDecorator.Pin()->IsEnableSlateForwardExtension();
	}
	
	return false;
}

bool FSchemaSlateRunExtension::SupportsBackward() const
{
	if (OwnerDecorator.IsValid())
	{
		return OwnerDecorator.Pin()->IsEnableSlateBackwardExtension();
	}

	return false;
}

int32 FSchemaSlateRunExtension::OnPaint(const FSchemaSlateExtensionStyleAddition& AdditionStyle, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();

	const uint16 MaxHeight = FontCache->GetMaxCharacterHeight(TextArgs.DefaultStyle.Font, AllottedGeometry.Scale);
	const int16 Baseline = FontCache->GetBaseline(TextArgs.DefaultStyle.Font, AllottedGeometry.Scale);

	int16 LinePos, LineThickness;
	FVector2f Offset = FVector2f(0);
	float Width	= TextArgs.Line.Size.X;
	GetExtensionMetrics(AdditionStyle, TextArgs, AllottedGeometry.Scale, LinePos, LineThickness, Offset, Width);

	const FVector2f Location(TextArgs.Line.Offset.X + Offset.X, TextArgs.Line.Offset.Y + Offset.Y + MaxHeight + Baseline - (LinePos * 0.5f));
	const FVector2f Size(Width, FMath::Max<int16>(1, LineThickness));

	// The block size and offset values are pre-scaled, so we need to account for that when converting the block offsets into paint geometry
	const float InverseScale = Inverse(AllottedGeometry.Scale);

	if (Size.X)
	{
		const FLinearColor LineColorAndOpacity = TextArgs.DefaultStyle.ColorAndOpacity.GetColor(InWidgetStyle);
		UE::Slate::FDeprecateVector2DResult ShadowOffset = TextArgs.DefaultStyle.ShadowOffset;

		const bool ShouldDropShadow = TextArgs.DefaultStyle.ShadowColorAndOpacity.A > 0.f && ShadowOffset.SizeSquared() > 0.f;

		// A negative shadow offset should be applied as a positive offset to the underline to avoid clipping issues
		const FVector2f DrawShadowOffset(
			(ShadowOffset.X > 0.0f) ? ShadowOffset.X * AllottedGeometry.Scale : 0.0f,
			(ShadowOffset.Y > 0.0f) ? ShadowOffset.Y * AllottedGeometry.Scale : 0.0f
		);
		const FVector2f DrawUnderlineOffset(
			(ShadowOffset.X < 0.0f) ? -ShadowOffset.X * AllottedGeometry.Scale : 0.0f,
			(ShadowOffset.Y < 0.0f) ? -ShadowOffset.Y * AllottedGeometry.Scale : 0.0f
		);

		// Draw the optional shadow
		if (ShouldDropShadow)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++LayerId,
				AllottedGeometry.ToPaintGeometry(TransformVector(InverseScale, Size), FSlateLayoutTransform(TransformPoint(InverseScale, Location + DrawShadowOffset))),
				&AdditionStyle.Brush,
				bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
				TextArgs.DefaultStyle.ShadowColorAndOpacity * InWidgetStyle.GetColorAndOpacityTint()
			);
		}

		// Draw underline
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(TransformVector(InverseScale, Size), FSlateLayoutTransform(TransformPoint(InverseScale, Location + DrawUnderlineOffset))),
			&AdditionStyle.Brush,
			bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
			LineColorAndOpacity * InWidgetStyle.GetColorAndOpacityTint()
		);
	}

	return LayerId;
}

void FSchemaSlateRunExtension::GetExtensionMetrics(const FSchemaSlateExtensionStyleAddition& AdditionStyle, const FTextArgs& TextArgs, const float InFontScale, int16& OutLinePos, int16& OutLineThickness, FVector2f& Offset, float& Width) const
{
	TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();
	FSlateFontInfo FontInfo;
	FontCache->GetStrikeMetrics(FontInfo, InFontScale, OutLinePos, OutLineThickness);
	FCharacterList& CharacterList = FontCache->GetCharacterList(FontInfo, InFontScale);

	// 获取缩放比
	float Scale = PRIVATE_GET(&CharacterList, FontKey).GetScale();

	switch (AdditionStyle.HAlign)
	{
	case HAlign_Center: {
		Offset.X = Width / 2.0 - AdditionStyle.Brush.ImageSize.X * Scale / 2.0;
		Width = AdditionStyle.Brush.ImageSize.X * Scale;
		break;
	}
	case HAlign_Right: {
		Offset.X = Width - AdditionStyle.Brush.ImageSize.X * Scale;
		Width = AdditionStyle.Brush.ImageSize.X * Scale;
		break;
	}
	case HAlign_Left: {
		Width = AdditionStyle.Brush.ImageSize.X * Scale;
		break;
	}
	}

	switch (AdditionStyle.VAlign)
	{
	case VAlign_Center: {
		OutLineThickness = AdditionStyle.Brush.ImageSize.Y * Scale;
		Offset.Y = -OutLineThickness / 2.0f;
		break;
	}
	default: {
		// 目前不支持别的VAlign格式，因为我们只能取到字体的高度，暂时没法获得其他控件的高度。所以无法计算富文本每行的真实高度。
	}
	}
}


bool IsFirstBlock(const FTextArgs& TextArgs)
{
	if (TextArgs.Line.Blocks.IsEmpty())
	{
		return false;
	}

	return TextArgs.Line.Blocks[0] == TextArgs.Block;
}


FSchemaSlateWidgetRun::FSchemaSlateWidgetRun(TSharedPtr<FSchemaSlateRunExtension> InSlateRunExtension, const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FSlateWidgetRun::FWidgetRunInfo& InWidgetInfo, const FTextRange& InRange)
	: FSlateWidgetRun(TextLayout, InRunInfo, InText, InWidgetInfo, InRange)
	, SlateRunExtension(InSlateRunExtension)
{

}

TSharedRef<FSchemaSlateWidgetRun> FSchemaSlateWidgetRun::Create(TSharedPtr<FSchemaSlateRunExtension> InSlateRunExtension, const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FSlateWidgetRun::FWidgetRunInfo& InWidgetInfo, const FTextRange& InRange)
{
	return MakeShareable(new FSchemaSlateWidgetRun(InSlateRunExtension, TextLayout, InRunInfo, InText, InWidgetInfo, InRange));
}

int32 FSchemaSlateWidgetRun::OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// 为了确保我们的覆盖是直接覆盖在整一行的，所以我们必须判断当前Block是否属于LineView的一个Block
	if (SlateRunExtension.IsValid() && SlateRunExtension->SupportsForward() && IsFirstBlock(TextArgs))
	{
		LayerId = SlateRunExtension->OnPaint(SlateRunExtension->ExtensionStyle.ForwardAddition, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}
	
	LayerId = FSlateWidgetRun::OnPaint(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// 为了确保我们的覆盖是直接覆盖在整一行的，所以我们必须判断当前Block是否属于LineView的一个Block
	if (SlateRunExtension.IsValid() && SlateRunExtension->SupportsBackward() && IsFirstBlock(TextArgs))
	{
		LayerId = SlateRunExtension->OnPaint(SlateRunExtension->ExtensionStyle.BackwardAddition, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	return LayerId;
}

FSchemaSlateTextRun::FSchemaSlateTextRun(TSharedPtr<FSchemaSlateRunExtension> InSlateRunExtension, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FTextBlockStyle& InStyle, const FTextRange& InRange)
	: FSlateTextRun(InRunInfo, InText, InStyle, InRange)
	, SlateRunExtension(InSlateRunExtension)
{

}

TSharedRef<FSchemaSlateTextRun> FSchemaSlateTextRun::Create(TSharedPtr<FSchemaSlateRunExtension> InSlateRunExtension, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FTextBlockStyle& Style, const FTextRange& InRange)
{
	return MakeShareable(new FSchemaSlateTextRun(InSlateRunExtension, InRunInfo, InText, Style, InRange));
}

int32 FSchemaSlateTextRun::OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// 为了确保我们的覆盖是直接覆盖在整一行的，所以我们必须判断当前Block是否属于LineView的一个Block
	if (SlateRunExtension.IsValid() && SlateRunExtension->SupportsForward() && IsFirstBlock(TextArgs))
	{
		LayerId = SlateRunExtension->OnPaint(SlateRunExtension->ExtensionStyle.ForwardAddition, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	LayerId = FSlateTextRun::OnPaint(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (SlateRunExtension.IsValid() && SlateRunExtension->SupportsBackward() && IsFirstBlock(TextArgs))
	{
		LayerId = SlateRunExtension->OnPaint(SlateRunExtension->ExtensionStyle.BackwardAddition, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	return LayerId;
}