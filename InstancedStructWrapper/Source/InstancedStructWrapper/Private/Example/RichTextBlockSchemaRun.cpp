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
FSchemaSlateAdditionRendererParam::FSchemaSlateAdditionRendererParam(FSchemaSlateAdditionRendererDelta& InAdditionDelta)
	: RendererType(ESlateAdditionRendererType::ForwardAddition)
	, Brush(nullptr)
	, AdditionDelta(InAdditionDelta)
{

}

void FSchemaSlateAdditionRendererDelta::Rollback(const FTextArgs& TextArgs)
{
	TextArgs.Block->SetLocationOffset(TextArgs.Block->GetLocationOffset() - TextArgsOffset);

	if (TextArgs.bIsLastVisibleBlock)
	{
		CurLineIndex = -1;
	}
}

void FSchemaSlateAdditionRendererDelta::Update(const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect)
{
	++CurBlockIndex;
	bCurIsFirstBlock = false;
	bCurIsLastBlock = false;

	if (GetTypeHash(&(TextArgs.Line)) != LineViewID || FirstBlockID == GetTypeHash(&(TextArgs.Block.Get())))
	{
		// 重新进入每行第一块可视的Block
		
		++CurLineIndex;
		CurBlockIndex = 0;

		bCurIsFirstBlock = true;

		LineViewID = GetTypeHash(&(TextArgs.Line));
		FirstBlockID = GetTypeHash(&(TextArgs.Block.Get()));

		UpdateVisibleBlock(TextArgs, AllottedGeometry, MyCullingRect);

		TextArgsOffset = FVector2D(0.0);
	}

	if (TextArgs.Block == TextArgs.Line.Blocks[LastVisibleBlockIndex])
	{
		bCurIsLastBlock = true;
	}

	TextArgs.Block->SetLocationOffset(TextArgs.Block->GetLocationOffset() + TextArgsOffset);
}

void FSchemaSlateAdditionRendererDelta::UpdateVisibleBlock(const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect)
{
	// The block size and offset values are pre-scaled, so we need to account for that when converting the block offsets into paint geometry
	const float InverseScale = Inverse(AllottedGeometry.Scale);

	auto IsVisible = [&AllottedGeometry, &InverseScale, &MyCullingRect](FVector2D Offset, FVector2D Size)
	{
		// Is this line visible?  This checks if the culling rect, which represents the AABB around the last clipping rect, intersects the 
		// line of text, this requires that we get the text line into render space.
		// TODO perhaps save off this line view rect during text layout?
		const FVector2D LocalLineOffset = Offset * InverseScale;
		FSlateRect Rect(AllottedGeometry.GetRenderBoundingRect(FSlateRect(LocalLineOffset, LocalLineOffset + (Size * InverseScale))));

		return FSlateRect::DoRectanglesIntersect(Rect, MyCullingRect);
	};

	FirstVisibleBlockIndex = -1;
	LastVisibleBlockIndex = -1;
	for (int32 LineIndex = 0; LineIndex < TextArgs.Line.Blocks.Num(); ++LineIndex)
	{
		const TSharedRef<ILayoutBlock>& Block = TextArgs.Line.Blocks[LineIndex];
		if (IsVisible(Block->GetLocationOffset(), Block->GetSize()))
		{
			if (FirstVisibleBlockIndex == -1)
			{
				FirstVisibleBlockIndex = LineIndex;
			}
			LastVisibleBlockIndex = LineIndex;
		}
	}

}

FSchemaSlateAdditionRendererParam PrepareParams(const FTextArgs& TextArgs, const TSharedRef<const FString>& ContentText, const FTextRange& TextRange, FSchemaSlateAdditionRendererDelta& AdditionDelta)
{
	FSchemaSlateAdditionRendererParam Params(AdditionDelta);

	for (int32 index = 0; index < TextArgs.Line.Blocks.Num(); ++index)
	{
		if (TextArgs.Line.Blocks[index] == TextArgs.Block)
		{
			Params.BlockIndex = index;
			Params.LineModelIndex = TextArgs.Line.ModelIndex;

			break;
		}
	}

	return Params;
}



void GetExtensionMetrics(EHorizontalAlignment HAlign, EVerticalAlignment VAlign, FMargin Padding, FVector2f BrushSize, const FTextArgs& TextArgs, const float InFontScale, float& OutLineThickness, FVector2D& Offset, float& Width, bool bFontImportant = false)
{
	TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();
	FSlateFontInfo FontInfo;

	float MaxFontHeight = FontCache->GetMaxCharacterHeight(TextArgs.DefaultStyle.Font, InFontScale);
	float BaseFontline = FontCache->GetBaseline(TextArgs.DefaultStyle.Font, InFontScale);

	float MaxBlockHeight = TextArgs.Block->GetSize().Y;

	float MaxLineHeight = TextArgs.Line.TextHeight;

	FCharacterList& CharacterList = FontCache->GetCharacterList(FontInfo, InFontScale);

	// 获取缩放比
	float Scale = PRIVATE_GET(&CharacterList, FontKey).GetScale();

	Offset = FVector2D(0);

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
		if (bFontImportant)
		{
			Top = (MaxFontHeight - BaseFontline) / 2.0 - BrushSize.Y / 2.0 * Scale;
			Bottom = (MaxFontHeight - BaseFontline) / 2.0 + BrushSize.Y / 2.0 * Scale;
			Top += MaxBlockHeight - MaxFontHeight;
			Bottom += MaxBlockHeight - MaxFontHeight;
		}
		else
		{
			Top = MaxLineHeight / 2.0 - BrushSize.Y / 2.0 * Scale;
			Bottom = MaxLineHeight / 2.0 + BrushSize.Y / 2.0 * Scale;
		}

		break;
	}
	case VAlign_Bottom: {
		if (bFontImportant)
		{
			Top = (MaxFontHeight - BaseFontline) - BrushSize.Y * Scale;
			Bottom = MaxFontHeight - BaseFontline;
			Top += MaxBlockHeight - MaxFontHeight;
			Bottom += MaxBlockHeight - MaxFontHeight;
		}
		else
		{
			Top = MaxLineHeight - BrushSize.Y * Scale;
			Bottom = MaxLineHeight;
		}
		break;
	}
	case VAlign_Top: {
		if (bFontImportant)
		{
			Top = 0;
			Bottom = BrushSize.Y * Scale;
			Top += MaxBlockHeight - MaxFontHeight;
			Bottom += MaxBlockHeight - MaxFontHeight;
		}
		else
		{
			Top = 0;
			Bottom = BrushSize.Y * Scale;
		}
		break;
	}
	case VAlign_Fill: {
		if (bFontImportant)
		{
			Top = 0;
			Bottom = MaxFontHeight;
			Top += MaxBlockHeight - MaxFontHeight;
			Bottom += MaxBlockHeight - MaxFontHeight;
		}
		else
		{
			Top = 0;
			Bottom = MaxLineHeight;
		}
	}
	}

	Left += Padding.Left;
	Right -= Padding.Right;
	Top += Padding.Top;
	Bottom -= Padding.Bottom;

	Width = Right - Left;
	OutLineThickness = Bottom - Top;

	Offset.X = Left;
	Offset.Y = Top;

	Offset.X += TextArgs.Block->GetLocationOffset().X;
	Offset.Y += TextArgs.Block->GetLocationOffset().Y - TextArgs.Line.Offset.Y - (MaxLineHeight - MaxBlockHeight);	// 实际的局部Offset = LocalOffset - LineOffset - (Line 高 - Block 高)
}

int32 FSchemaSlateAdditionRenderer_Brush_MultiLine::OnPaint(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();

	float LineThickness;
	FVector2D Offset = FVector2D(0);
	float Width = TextArgs.Line.Size.X;

	GetExtensionMetrics(HAlign, VAlign, Padding, Params.Brush->ImageSize, TextArgs, AllottedGeometry.Scale, LineThickness, Offset, Width, bFontImportant);

	const FVector2f Location(TextArgs.Line.Offset.X + Offset.X, TextArgs.Line.Offset.Y + Offset.Y);
	const FVector2f Size(Width, FMath::Max<int16>(1, LineThickness));

	// The block size and offset values are pre-scaled, so we need to account for that when converting the block offsets into paint geometry
	const float InverseScale = Inverse(AllottedGeometry.Scale);

	const ESlateDrawEffect DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	if (Size.X)
	{
		const FLinearColor LineColorAndOpacity = Params.Brush->TintColor.GetSpecifiedColor();

		// Draw underline
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(TransformVector(InverseScale, Size), FSlateLayoutTransform(TransformPoint(InverseScale, Location))),
			Params.Brush,
			DrawEffects,
			LineColorAndOpacity
		);
	}

	return LayerId;
}

bool FSchemaSlateAdditionRenderer_Brush_MultiLine::Supports(const FSchemaSlateAdditionRendererParam& Params) const
{
	// 前向渲染在每行第一个
	// 延迟渲染在每行最后一个
	if (Params.AdditionDelta.bCurIsFirstBlock && Params.RendererType == ESlateAdditionRendererType::ForwardAddition ||
		Params.AdditionDelta.bCurIsLastBlock && Params.RendererType == ESlateAdditionRendererType::BackwardAddition)
	{
		return true;
	}

	return false;
}

int32 FSchemaSlateAdditionRenderer_HeadPlaceholder_MultiLine::OnPaint(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();

	float LineThickness;
	FVector2D Offset = FVector2D(0);
	float Width = TextArgs.Line.Size.X;

	GetExtensionMetrics(HAlign_Left, VAlign_Center, Padding, Params.Brush->ImageSize, TextArgs, AllottedGeometry.Scale, LineThickness, Offset, Width, bFontImportant);

	const FVector2f Location(TextArgs.Line.Offset.X + Offset.X, TextArgs.Line.Offset.Y + Offset.Y);
	const FVector2f Size(Width, FMath::Max<int16>(1, LineThickness));

	// 强行让后续文本的头部被空出一段内容
	TextArgs.Block->SetLocationOffset(TextArgs.Block->GetLocationOffset() + FVector2D(Width, 0));
	Params.AdditionDelta.TextArgsOffset += FVector2D(Width, 0);

	// The block size and offset values are pre-scaled, so we need to account for that when converting the block offsets into paint geometry
	const float InverseScale = Inverse(AllottedGeometry.Scale);

	const ESlateDrawEffect DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	if (Size.X)
	{
		const FLinearColor LineColorAndOpacity = Params.Brush->TintColor.GetSpecifiedColor();

		// 有待研究，不自增1会导致Box即使在Text之后才绘制，也只能遮挡Text的外轮廓，而不会遮挡Text内容
		++LayerId;

		// Draw underline
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(TransformVector(InverseScale, Size), FSlateLayoutTransform(TransformPoint(InverseScale, Location))),
			Params.Brush,
			DrawEffects,
			LineColorAndOpacity
		);
	}

	return LayerId;
}

bool FSchemaSlateAdditionRenderer_HeadPlaceholder_MultiLine::Supports(const FSchemaSlateAdditionRendererParam& Params) const
{
	// 前向渲染在每行第一个
	// 延迟渲染在每行最后一个
	if (Params.AdditionDelta.bCurIsFirstBlock && Params.RendererType == ESlateAdditionRendererType::ForwardAddition ||
		Params.AdditionDelta.bCurIsLastBlock && Params.RendererType == ESlateAdditionRendererType::BackwardAddition)
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
			}
			BrushInstance.Add(AdditionRenderer->DefaultBrush);
		}
		else
		{
			BrushInstance.Add(FSlateBrush());
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
			}
			BrushInstance.Add(AdditionRenderer->DefaultBrush);
		}
		else
		{
			BrushInstance.Add(FSlateBrush());
		}
	}
}

int32 FSlateAdditionRun::DrawForward(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	Params.RendererType = ESlateAdditionRendererType::ForwardAddition;
	return OnPaint(StyleSheet->ForwardAddition, 0, Params, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

int32 FSlateAdditionRun::DrawBackward(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	Params.RendererType = ESlateAdditionRendererType::BackwardAddition;
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
		if (Index <= BackwardEndIndex - BackwardBeginIndex)
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
		if (Index <= BackwardEndIndex - BackwardBeginIndex)
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
		SlateAdditionRun->AdditionDelta.Update(TextArgs, AllottedGeometry, MyCullingRect);

		FSchemaSlateAdditionRendererParam Param = PrepareParams(TextArgs, PRIVATE_GET(this, Text, SlateWidgetRun), GetTextRange(), SlateAdditionRun->AdditionDelta);
		LayerId = SlateAdditionRun->DrawForward(Param, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}
	
	LayerId = FSlateWidgetRun::OnPaint(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (SlateAdditionRun.IsValid())
	{
		FSchemaSlateAdditionRendererParam Param = PrepareParams(TextArgs, PRIVATE_GET(this, Text, SlateWidgetRun), GetTextRange(), SlateAdditionRun->AdditionDelta);
		LayerId = SlateAdditionRun->DrawBackward(Param, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

		SlateAdditionRun->AdditionDelta.Rollback(TextArgs);
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
		SlateAdditionRun->AdditionDelta.Update(TextArgs, AllottedGeometry, MyCullingRect);

		FSchemaSlateAdditionRendererParam Param = PrepareParams(TextArgs, PRIVATE_GET(this, Text, SlateTextRun), GetTextRange(), SlateAdditionRun->AdditionDelta);
		LayerId = SlateAdditionRun->DrawForward(Param, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	LayerId = FSlateTextRun::OnPaint(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (SlateAdditionRun.IsValid())
	{
		FSchemaSlateAdditionRendererParam Param = PrepareParams(TextArgs, PRIVATE_GET(this, Text, SlateTextRun), GetTextRange(), SlateAdditionRun->AdditionDelta);
		LayerId = SlateAdditionRun->DrawBackward(Param, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

		SlateAdditionRun->AdditionDelta.Rollback(TextArgs);
	}

	return LayerId;
}