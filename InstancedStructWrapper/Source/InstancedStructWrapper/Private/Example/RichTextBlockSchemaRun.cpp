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

FRichSchemaDecorator::FRichSchemaDecorator(URichTextBlock* InOwnerBlock, URichTextBlockSchemaDecoratorStyleSheet* InStyleSheet, TSharedPtr<FSlateAdditionBatchRun> InSlateAdditionRun)
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
FSchemaSlateAdditionRendererParam::FSchemaSlateAdditionRendererParam(FSchemaSlateAdditionRendererBatchDelta& InAdditionBatchDelta)
	: RendererType(ESlateAdditionRendererType::ForwardAddition)
	, Brush(nullptr)
	, AdditionBatchDelta(InAdditionBatchDelta)
{

}

void FSchemaSlateAdditionRendererBatchDelta::Rollback(const FTextArgs& TextArgs, int32 LayerId)
{
	TextArgs.Block->SetLocationOffset(TextArgs.Block->GetLocationOffset() - TextArgsOffset_Line);
	TextArgs.Block->SetLocationOffset(TextArgs.Block->GetLocationOffset() - TextArgsOffset_Block);

	TextArgsOffset_Block = FVector2D(0.0);
}

void FSchemaSlateAdditionRendererBatchDelta::Update(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, int32& LayerId)
{
	++CurBlockIndex;
	bCurIsFirstBlock = false;
	bCurIsLastBlock = false;

	if (PrevLineID != (uint64)&(TextArgs.Line) || CurrentTime < PaintArgs.GetCurrentTime())
	{
		if (CurrentTime < PaintArgs.GetCurrentTime())
		{
			CurrentTime = PaintArgs.GetCurrentTime();
			CurLineIndex = -1;
		}

		++CurLineIndex;
		PrevLineID = (uint64) & (TextArgs.Line);

		// 在未初始化或重新进入遍历时
		// 这里因为Line都存储在连续堆内存中，地址从低到高，所以可以借助指针大小来判断是否重新进入了遍历。
		UpdateVisibleBlock(TextArgs, AllottedGeometry, MyCullingRect);
		
		CurBlockIndex = 0;
		
		// 重新进入每行第一块可视的Block
		bCurIsFirstBlock = true;
		TextArgsOffset_Line = FVector2D(0.0);
		TextArgsOffset_Block = FVector2D(0.0);

		// 继承新的LayerId
		RealLayerId = LayerId;
	}

	// 还原LayerId，确保Block绘制时，不会涉及前面同一Line的Forward和BackwardAddition
	LayerId = RealLayerId;
	
	if (TextArgs.Line.Blocks[LastVisibleBlockIndex] == TextArgs.Block)
	{
		// 进入每行最后一块可视的Block
		bCurIsLastBlock = true;
	}
}

void FSchemaSlateAdditionRendererBatchDelta::UpdateVisibleBlock(const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect)
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

void GetExtensionMetrics(const FSchemaSlateAdditionRendererParam& Params, EHorizontalAlignment HAlign, EVerticalAlignment VAlign, FMargin Padding, FVector2f BrushSize, const FTextArgs& TextArgs, const float InFontScale, float& OutLineThickness, FVector2D& Offset, float& Width)
{
	TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();
	FSlateFontInfo FontInfo;

	// 仅存储了默认字体的信息
	float MaxFontHeight = FontCache->GetMaxCharacterHeight(TextArgs.DefaultStyle.Font, InFontScale);

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
		Top = MaxLineHeight / 2.0 - BrushSize.Y / 2.0 * Scale;
		Bottom = MaxLineHeight / 2.0 + BrushSize.Y / 2.0 * Scale;
		break;
	}
	case VAlign_Bottom: {
		Top = MaxLineHeight - BrushSize.Y * Scale;
		Bottom = MaxLineHeight;
		break;
	}
	case VAlign_Top: {
		Top = 0;
		Bottom = BrushSize.Y * Scale;
		break;
	}
	case VAlign_Fill: {
		Top = 0;
		Bottom = MaxLineHeight;
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

	Offset.X += Params.AdditionBatchDelta.TextArgsOffset_Line.X;
	Offset.Y += Params.AdditionBatchDelta.TextArgsOffset_Line.Y;
}

int32 FSchemaSlateAdditionRenderer_Brush_MultiLine::OnPaint(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();

	float LineThickness;
	FVector2D Offset = FVector2D(0);
	float Width = TextArgs.Line.Size.X;

	GetExtensionMetrics(Params, HAlign, VAlign, Padding, Params.Brush->ImageSize, TextArgs, AllottedGeometry.Scale, LineThickness, Offset, Width);

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
	if (Params.AdditionBatchDelta.bCurIsFirstBlock && Params.RendererType == ESlateAdditionRendererType::ForwardAddition ||
		Params.AdditionBatchDelta.bCurIsLastBlock && Params.RendererType == ESlateAdditionRendererType::BackwardAddition)
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

	GetExtensionMetrics(Params, HAlign_Left, VAlign_Center, Padding, Params.Brush->ImageSize, TextArgs, AllottedGeometry.Scale, LineThickness, Offset, Width);

	const FVector2f Location(TextArgs.Line.Offset.X + Offset.X, TextArgs.Line.Offset.Y + Offset.Y);
	const FVector2f Size(Width, FMath::Max<int16>(1, LineThickness));

	// 强行让后续文本的头部被空出一段内容
	TextArgs.Block->SetLocationOffset(TextArgs.Block->GetLocationOffset() + FVector2D(Width, 0));
	Params.AdditionBatchDelta.TextArgsOffset_Line += FVector2D(Width, 0);

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

bool FSchemaSlateAdditionRenderer_HeadPlaceholder_MultiLine::Supports(const FSchemaSlateAdditionRendererParam& Params) const
{
	// 前向渲染在每行第一个
	// 延迟渲染在每行最后一个
	if (Params.AdditionBatchDelta.bCurIsFirstBlock && Params.RendererType == ESlateAdditionRendererType::ForwardAddition ||
		Params.AdditionBatchDelta.bCurIsLastBlock && Params.RendererType == ESlateAdditionRendererType::BackwardAddition)
	{
		return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////

FSlateAdditionBatchRun::FSlateAdditionBatchRun(const URichTextBlockSchemaDecoratorStyleSheet* InStyleSheet)
	: AdditionSet(0)
	, StyleSheet(InStyleSheet)
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

int32 FSlateAdditionBatchRun::Begin(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled)
{
	Update(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, LayerId);

	int32 PrevLayerId = LayerId;

	FSchemaSlateAdditionRendererParam Params(AdditionBatchDelta);
	LayerId = DrawForward(Params, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (LayerId > PrevLayerId)
	{
		AdditionBatchDelta.RealLayerId = LayerId;
	}

	return LayerId;
}

int32 FSlateAdditionBatchRun::End(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled)
{
	int32 PrevLayerId = LayerId;

	FSchemaSlateAdditionRendererParam Params(AdditionBatchDelta);
	LayerId = DrawBackward(Params, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (LayerId > PrevLayerId)
	{
		AdditionBatchDelta.RealLayerId = LayerId;
	}

	Rollback(TextArgs, LayerId);

	return LayerId;
}

int32 FSlateAdditionBatchRun::DrawForward(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	Params.RendererType = ESlateAdditionRendererType::ForwardAddition;
	return OnPaint(StyleSheet->ForwardAddition, 0, Params, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

int32 FSlateAdditionBatchRun::DrawBackward(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	Params.RendererType = ESlateAdditionRendererType::BackwardAddition;
	return OnPaint(StyleSheet->BackwardAddition, BackwardBeginIndex, Params, PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

int32 FSlateAdditionBatchRun::OnPaint(const FInstancedStructContainer& SlateAdditions, uint32 BeginIndex, FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
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

void FSlateAdditionBatchRun::SetEnable(ESlateAdditionRendererType Type, int32 Index, bool bIsEnable)
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

void FSlateAdditionBatchRun::SetBrush(ESlateAdditionRendererType Type, int32 Index, FSlateBrush Brush)
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

void FSlateAdditionBatchRun::Update(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, int32& LayerId)
{
	AdditionBatchDelta.Update(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, LayerId);

	//TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();
	//FSlateFontInfo FontInfo;
	//FCharacterList& CharacterList = FontCache->GetCharacterList(FontInfo, AllottedGeometry.Scale);
	// 获取缩放比
	//float Scale = PRIVATE_GET(&CharacterList, FontKey).GetScale();
	float MaxLineHeight = TextArgs.Line.TextHeight;
	float MaxBlockHeight = TextArgs.Block->GetSize().Y;

	float OffsetY = TextArgs.Line.Offset.Y - TextArgs.Block->GetLocationOffset().Y;

	switch (StyleSheet->Alignment)
	{
	case VAlign_Center: {
		OffsetY += MaxLineHeight / 2.0 - MaxBlockHeight / 2.0;
		break;
	}
	case VAlign_Top: {
		OffsetY += 0;
		break;
	}
	case VAlign_Bottom: {
		OffsetY += MaxLineHeight - MaxBlockHeight;
		break;
	}
	case VAlign_Fill: {
		OffsetY += MaxLineHeight / 2.0 - MaxBlockHeight / 2.0;
		// 不支持Fill，默认变更为VAlign_Center
		break;
	}
	}

	AdditionBatchDelta.TextArgsOffset_Block += FVector2D(0, OffsetY);

	TextArgs.Block->SetLocationOffset(TextArgs.Block->GetLocationOffset() + AdditionBatchDelta.TextArgsOffset_Block);
	TextArgs.Block->SetLocationOffset(TextArgs.Block->GetLocationOffset() + AdditionBatchDelta.TextArgsOffset_Line);
}

void FSlateAdditionBatchRun::Rollback(const FTextArgs& TextArgs, int32 LayerId)
{
	AdditionBatchDelta.Rollback(TextArgs, LayerId);
}

//////////////////////////////////////////////////////////////////////////

FSchemaSlateWidgetRun::FSchemaSlateWidgetRun(TSharedPtr<FSlateAdditionBatchRun> InSlateAdditionRun, const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FSlateWidgetRun::FWidgetRunInfo& InWidgetInfo, const FTextRange& InRange)
	: FSlateWidgetRun(TextLayout, InRunInfo, InText, InWidgetInfo, InRange)
	, SlateAdditionRun(InSlateAdditionRun)
{

}

TSharedRef<FSchemaSlateWidgetRun> FSchemaSlateWidgetRun::Create(TSharedPtr<FSlateAdditionBatchRun> InSlateAdditionRun, const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FSlateWidgetRun::FWidgetRunInfo& InWidgetInfo, const FTextRange& InRange)
{
	return MakeShareable(new FSchemaSlateWidgetRun(InSlateAdditionRun, TextLayout, InRunInfo, InText, InWidgetInfo, InRange));
}

int32 FSchemaSlateWidgetRun::OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (SlateAdditionRun.IsValid())
	{
		LayerId = SlateAdditionRun->Begin(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}
	
	LayerId = FSlateWidgetRun::OnPaint(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (SlateAdditionRun.IsValid())
	{
		LayerId = SlateAdditionRun->End(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}
	return LayerId;
}

FSchemaSlateTextRun::FSchemaSlateTextRun(TSharedPtr<FSlateAdditionBatchRun> InSlateAdditionRun, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FTextBlockStyle& InStyle, const FTextRange& InRange)
	: FSlateTextRun(InRunInfo, InText, InStyle, InRange)
	, SlateAdditionRun(InSlateAdditionRun)
{

}

TSharedRef<FSchemaSlateTextRun> FSchemaSlateTextRun::Create(TSharedPtr<FSlateAdditionBatchRun> InSlateAdditionRun, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FTextBlockStyle& Style, const FTextRange& InRange)
{
	return MakeShareable(new FSchemaSlateTextRun(InSlateAdditionRun, InRunInfo, InText, Style, InRange));
}

int32 FSchemaSlateTextRun::OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (SlateAdditionRun.IsValid())
	{
		LayerId = SlateAdditionRun->Begin(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	LayerId = FSlateTextRun::OnPaint(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (SlateAdditionRun.IsValid())
	{
		LayerId = SlateAdditionRun->End(PaintArgs, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	return LayerId;
}