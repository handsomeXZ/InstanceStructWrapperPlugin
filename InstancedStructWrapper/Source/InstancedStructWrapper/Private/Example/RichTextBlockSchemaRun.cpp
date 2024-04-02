#include "Example/RichTextBlockSchemaRun.h"

#include "Fonts/FontMeasure.h"
#include "Styling/SlateStyle.h"
#include "Styling/ISlateStyle.h"
#include "Framework/Text/ShapedTextCache.h"

#include "Example/RichTextBlockSchemaDecorator.h"
#include "PrivateAccessor.h"

PRIVATE_DEFINE(URichTextBlock, TSharedPtr<FSlateStyleSet>, StyleInstance);
PRIVATE_DEFINE(FCharacterList, FSlateFontKey, FontKey);
PRIVATE_DEFINE(FSlateWidgetRun, TSharedRef<const FString>, Text, SlateWidgetRun);
PRIVATE_DEFINE(FSlateTextRun, TSharedRef<const FString>, Text, SlateTextRun);

FRichSchemaDecorator::FRichSchemaDecorator(URichTextBlock* InOwnerBlock, URichTextBlockSchemaDecoratorStyleSheet* InStyleSheet, TSharedPtr<FSlateAdditionRun> InSlateAdditionRun)
	: OwnerBlock(InOwnerBlock)
	, StyleSheet(InStyleSheet)
	, SlateAdditionRun(InSlateAdditionRun)
{
}

bool FRichSchemaDecorator::Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const
{
	if (!IsValidData())
	{
		return false;
	}

	if (StyleSheet->ForwardAddition.Num() || StyleSheet->BackwardAddition.Num())
	{
		// 只要配了Addition数据，必定覆盖。
		return true;
	}

	if (SupportsDecoratorWidget(RunParseResult, Text))
	{
		// 只要字段内容支持自定义Widget，就覆盖。
		return true;
	}

	return false;
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
		SlateRun = FSchemaSlateWidgetRun::Create(SlateAdditionRun, TextLayout, RunInfo, InOutModelText, WidgetRunInfo, ModelRange);
	}
	else
	{
		// Assume there's a text handler if widget is empty, if there isn't one it will just display an empty string
		FTextBlockStyle TempStyle = TextStyle;
		CreateDecoratorText(RunParseResult, OriginalText, TempStyle, *InOutModelText);

		ModelRange.EndIndex = InOutModelText->Len();
		SlateRun = FSchemaSlateTextRun::Create(SlateAdditionRun, RunInfo, InOutModelText, TempStyle, ModelRange);
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

bool FRichSchemaDecorator::IsValidData() const
{
	return IsValid(OwnerBlock) && IsValid(StyleSheet) && StyleSheet->Chooser.IsValid();
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
FSchemaSlateAdditionRendererParam::FSchemaSlateAdditionRendererParam()
	: Brush(nullptr)
{

}


FSchemaSlateAdditionRendererParam PrepareParams(const FTextArgs& TextArgs, const TSharedRef<const FString>& ContentText, const FTextRange& TextRange)
{
	FSchemaSlateAdditionRendererParam Params;

	for (int32 index = 0; index < TextArgs.Line.Blocks.Num(); ++index)
	{
		if (TextArgs.Line.Blocks[0] == TextArgs.Block)
		{
			Params.BlockIndex = index;
			Params.LineModelIndex = TextArgs.Line.ModelIndex;

			int32 ModelLength = ContentText->Len();

			// 仅单线程可以这样用
			static int32 LineIndex = 0;
			if (TextArgs.Block->GetTextRange().BeginIndex == 0 && TextArgs.Line.Range.BeginIndex == 0 && TextRange.BeginIndex == 0)
			{
				LineIndex = 0;
			}

			Params.LineIndex = LineIndex;

			if (TextArgs.bIsLastVisibleBlock)
			{
				++LineIndex;
			}

			break;
		}
	}

	return Params;
}



void GetExtensionMetrics(EHorizontalAlignment HAlign, EVerticalAlignment VAlign, FMargin Padding, FVector2f BrushSize, const FTextArgs& TextArgs, const float InFontScale, float& OutLineThickness, FVector2f& Offset, float& Width)
{
	TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();
	FSlateFontInfo FontInfo;

	float MaxHeight = FontCache->GetMaxCharacterHeight(TextArgs.DefaultStyle.Font, InFontScale);
	//float Baseline = FontCache->GetBaseline(TextArgs.DefaultStyle.Font, InFontScale);
	FCharacterList& CharacterList = FontCache->GetCharacterList(FontInfo, InFontScale);

	// 获取缩放比
	float Scale = PRIVATE_GET(&CharacterList, FontKey).GetScale();

	Offset = FVector2f(0);

	float Left = 0;
	float Right = 0;
	float Top = 0;
	float Bottom = 0;

	switch (HAlign)
	{
	case HAlign_Center: {
		Left = Width / 2.0 - BrushSize.X * Scale / 2.0;
		Right = Width / 2.0 + BrushSize.X * Scale / 2.0;
		break;
	}
	case HAlign_Right: {
		Left = Width - BrushSize.X * Scale;
		Right = Width;
		break;
	}
	case HAlign_Left: {
		Left = 0;
		Right = BrushSize.X * Scale;
		break;
	}
	case HAlign_Fill: {
		Left = 0;
		Right = Width;
		break;
	}
	}

	switch (VAlign)
	{
	case VAlign_Center: {
		Top = (MaxHeight /*- Baseline*/) / 2.0 - BrushSize.Y / 2.0 * Scale;
		Bottom = (MaxHeight /*- Baseline*/) / 2.0 + BrushSize.Y / 2.0 * Scale;
		break;
	}
	default: {
		// 目前不支持别的VAlign格式，因为我们只能取到字体的高度，暂时没法获得其他控件的高度。所以无法计算富文本每行的真实高度。
	}
	}


	Left += Padding.Left;
	Right -= Padding.Right;
	Top += Padding.Top;
	Bottom -= Padding.Bottom;

	Offset.X = Left;
	Offset.Y = Top;
	Width = Right - Left;
	OutLineThickness = Bottom - Top;

}

int32 FSchemaSlateAdditionRenderer_Brush_MultiLine::OnPaint(const FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();

	float LineThickness;
	FVector2f Offset;
	float Width = TextArgs.Line.Size.X;

	GetExtensionMetrics(HAlign, VAlign, Padding, Params.Brush->ImageSize, TextArgs, AllottedGeometry.Scale, LineThickness, Offset, Width);

	const FVector2f Location(TextArgs.Line.Offset.X + Offset.X, TextArgs.Line.Offset.Y + Offset.Y);
	const FVector2f Size(Width, FMath::Max<int16>(1, LineThickness));

	// The block size and offset values are pre-scaled, so we need to account for that when converting the block offsets into paint geometry
	const float InverseScale = Inverse(AllottedGeometry.Scale);

	if (Size.X)
	{
		const FLinearColor LineColorAndOpacity = Params.Brush->TintColor.GetColor(InWidgetStyle);

		// Draw underline
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(TransformVector(InverseScale, Size), FSlateLayoutTransform(TransformPoint(InverseScale, Location))),
			Params.Brush,
			ESlateDrawEffect::NoBlending,
			LineColorAndOpacity
		);
	}

	return LayerId;
}

bool FSchemaSlateAdditionRenderer_Brush_MultiLine::Supports(const FSchemaSlateAdditionRendererParam& Params) const
{
	// 每行第一个Block执行渲染
	if (Params.BlockIndex == 0)
	{
		return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////

FSlateAdditionRun::FSlateAdditionRun(const URichTextBlockSchemaDecoratorStyleSheet* InStyleSheet)
	: StyleSheet(InStyleSheet)
	, AdditionSet(0)
{
	int32 Index = 0;

	BackwardBeginIndex = StyleSheet->ForwardAddition.Num();
	BackwardEndIndex = BackwardBeginIndex + StyleSheet->BackwardAddition.Num() - 1;

	BrushInstance.Empty(FMath::Min(BackwardEndIndex + 1, 32));

	for (auto It = StyleSheet->ForwardAddition.begin(); It && Index < 32; ++It, ++Index)
	{
		FConstStructView StructView = *It;
		if (const FSchemaSlateAdditionRenderer* AdditionRenderer = StructView.GetPtr<const FSchemaSlateAdditionRenderer>())
		{
			if (AdditionRenderer->bDefaultEnable)
			{
				AdditionSet |= (1 << Index);
				BrushInstance.Add(AdditionRenderer->DefaultBrush);
			}
		}
	}

	for (auto It = StyleSheet->BackwardAddition.begin(); It && Index < 32; ++It, ++Index)
	{
		FConstStructView StructView = *It;
		if (const FSchemaSlateAdditionRenderer* AdditionRenderer = StructView.GetPtr<const FSchemaSlateAdditionRenderer>())
		{
			if (AdditionRenderer->bDefaultEnable)
			{
				AdditionSet |= (1 << Index);
				BrushInstance.Add(AdditionRenderer->DefaultBrush);
			}
		}
	}
}

int32 FSlateAdditionRun::DrawForward(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	return OnPaint(StyleSheet->ForwardAddition, 0, Params, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

int32 FSlateAdditionRun::DrawBackward(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	return OnPaint(StyleSheet->BackwardAddition, BackwardBeginIndex, Params, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

int32 FSlateAdditionRun::OnPaint(const FInstancedStructContainer& SlateAdditions, uint32 BeginIndex, FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	for (auto It = SlateAdditions.begin(); It; ++It)
	{
		if (BeginIndex >= 32) break;

		if (AdditionSet & (1 << BeginIndex))
		{
			FConstStructView StructView = *It;
			const FSchemaSlateAdditionRenderer* AdditionRenderer = StructView.GetPtr<const FSchemaSlateAdditionRenderer>();

			if (!AdditionRenderer)
			{
				continue;
			}

			if (!AdditionRenderer->Supports(Params))
			{
				continue;
			}

			Params.Brush = &BrushInstance[BeginIndex];
			LayerId = AdditionRenderer->OnPaint(Params, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
		}

		BeginIndex++;
	}

	return LayerId;
}

void FSlateAdditionRun::SetEnable(ESlateAdditionRendererType Type, int32 Index, bool bIsEnable)
{
	switch (Type)
	{
	case ESlateAdditionRendererType::ForwardAddition: {
		if (Index < BackwardBeginIndex)
		{
			if (bIsEnable)
			{
				AdditionSet |= 1 << Index;
			}
			else
			{
				AdditionSet &= 0 ^ (1 << Index);
			}
		}
		break;
	}
	case ESlateAdditionRendererType::BackwardAddition: {
		if (Index < BackwardEndIndex - BackwardBeginIndex)
		{
			Index = Index + BackwardBeginIndex;
			if (bIsEnable)
			{
				AdditionSet |= 1 << Index;
			}
			else
			{
				AdditionSet &= 0 ^ (1 << Index);
			}
		}
	}
	}
}

void FSlateAdditionRun::SetBrush(ESlateAdditionRendererType Type, int32 Index, FSlateBrush Brush)
{
	switch (Type)
	{
	case ESlateAdditionRendererType::ForwardAddition: {
		if (Index < BackwardBeginIndex)
		{
			BrushInstance[Index] = Brush;
		}
		break;
	}
	case ESlateAdditionRendererType::BackwardAddition: {
		if (Index < BackwardEndIndex - BackwardBeginIndex)
		{
			Index = Index + BackwardBeginIndex;
			BrushInstance[Index] = Brush;
		}
	}
	}
}


FSchemaSlateWidgetRun::FSchemaSlateWidgetRun(TSharedPtr<FSlateAdditionRun> InSlateAdditionRun, const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FSlateWidgetRun::FWidgetRunInfo& InWidgetInfo, const FTextRange& InRange)
	: FSlateWidgetRun(TextLayout, InRunInfo, InText, InWidgetInfo, InRange)
	, SlateAdditionRun(InSlateAdditionRun)
{

}

TSharedRef<FSchemaSlateWidgetRun> FSchemaSlateWidgetRun::Create(TSharedPtr<FSlateAdditionRun> InSlateAdditionRun, const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FSlateWidgetRun::FWidgetRunInfo& InWidgetInfo, const FTextRange& InRange)
{
	return MakeShareable(new FSchemaSlateWidgetRun(InSlateAdditionRun, TextLayout, InRunInfo, InText, InWidgetInfo, InRange));
}

int32 FSchemaSlateWidgetRun::OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{

	if (SlateAdditionRun.IsValid())
	{
		FSchemaSlateAdditionRendererParam Param = PrepareParams(TextArgs, PRIVATE_GET(this, Text, SlateWidgetRun), GetTextRange());
		LayerId = SlateAdditionRun->DrawForward(Param, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}
	
	LayerId = FSlateWidgetRun::OnPaint(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);


	if (SlateAdditionRun.IsValid())
	{
		FSchemaSlateAdditionRendererParam Param = PrepareParams(TextArgs, PRIVATE_GET(this, Text, SlateWidgetRun), GetTextRange());
		LayerId = SlateAdditionRun->DrawBackward(Param, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	return LayerId;
}

FSchemaSlateTextRun::FSchemaSlateTextRun(TSharedPtr<FSlateAdditionRun> InSlateAdditionRun, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FTextBlockStyle& InStyle, const FTextRange& InRange)
	: FSlateTextRun(InRunInfo, InText, InStyle, InRange)
	, SlateAdditionRun(InSlateAdditionRun)
{

}

TSharedRef<FSchemaSlateTextRun> FSchemaSlateTextRun::Create(TSharedPtr<FSlateAdditionRun> InSlateAdditionRun, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FTextBlockStyle& Style, const FTextRange& InRange)
{
	return MakeShareable(new FSchemaSlateTextRun(InSlateAdditionRun, InRunInfo, InText, Style, InRange));
}

int32 FSchemaSlateTextRun::OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (SlateAdditionRun.IsValid())
	{
		FSchemaSlateAdditionRendererParam Param = PrepareParams(TextArgs, PRIVATE_GET(this, Text, SlateTextRun), GetTextRange());
		LayerId = SlateAdditionRun->DrawForward(Param, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	LayerId = FSlateTextRun::OnPaint(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);


	if (SlateAdditionRun.IsValid())
	{
		FSchemaSlateAdditionRendererParam Param = PrepareParams(TextArgs, PRIVATE_GET(this, Text, SlateTextRun), GetTextRange());
		LayerId = SlateAdditionRun->DrawBackward(Param, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	return LayerId;
}