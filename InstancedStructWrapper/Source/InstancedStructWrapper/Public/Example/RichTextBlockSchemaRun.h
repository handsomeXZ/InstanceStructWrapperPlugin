#pragma once

#include "CoreMinimal.h"
#include "Framework/Text/ITextDecorator.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/SlateWidgetRun.h"

#include "RichTextBlockSchemaRun.generated.h"

class URichTextBlockSchemaDecorator;
class URichTextBlockSchemaDecoratorStyleSheet;

class FRichSchemaDecorator : public ITextDecorator, public TSharedFromThis<FRichSchemaDecorator>
{
public:
	FRichSchemaDecorator(URichTextBlock* InOwnerBlock, URichTextBlockSchemaDecoratorStyleSheet* InStyleSheet, URichTextBlockSchemaDecorator* InOwnerSchemaDecorator)
		: OwnerBlock(InOwnerBlock)
		, StyleSheet(InStyleSheet)
		, OwnerSchemaDecorator(InOwnerSchemaDecorator)
	{
	}
	bool IsEnableSlateForwardExtension() const;
	bool IsEnableSlateBackwardExtension() const;

	virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override;

	virtual TSharedRef<ISlateRun> Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText, const TSharedRef< FString >& InOutModelText, const ISlateStyle* Style) override final;
protected:
	virtual bool SupportsDecoratorWidget(const FTextRunParseResults& RunParseResult, const FString& Text) const;
	virtual TSharedPtr<SWidget> CreateDecoratorWidget(const FTextRunInfo& RunInfo, const FTextBlockStyle& TextStyle) const;
	virtual void CreateDecoratorText(const FTextRunParseResults& RunParseResult, const FString& OriginalText, FTextBlockStyle& InOutTextStyle, FString& InOutString) const;
	virtual TSharedPtr<struct FSchemaSlateRunExtension> CreateSlateRunExtension();

	bool IsValidData() const;
	FName GetParseName() const;
	FName GetParseMetaData() const;

private:
	URichTextBlock* OwnerBlock;
	URichTextBlockSchemaDecoratorStyleSheet* StyleSheet;
	URichTextBlockSchemaDecorator* OwnerSchemaDecorator;

	TSharedPtr<class FSlateStyleSet> StyleInstance;
};


//////////////////////////////////////////////////////////////////////////
USTRUCT(BlueprintType)
struct INSTANCEDSTRUCTWRAPPER_API FSchemaSlateExtensionStyleAddition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSlateBrush Brush;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<EHorizontalAlignment> HAlign = HAlign_Center;
	// 目前不支持别的VAlign格式，因为我们只能取到字体的高度，暂时没法获得其他控件的高度。所以无法计算富文本每行的真实高度。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TEnumAsByte<EVerticalAlignment> VAlign = VAlign_Center;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDefaultEnable;
};
USTRUCT(BlueprintType)
struct INSTANCEDSTRUCTWRAPPER_API FSchemaSlateExtensionStyle
{
	GENERATED_BODY()

	// 前向渲染，在文本之前渲染。将出现在文本后面。
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSchemaSlateExtensionStyleAddition ForwardAddition;
	// 延迟渲染，在文本之后渲染。将出现在文本前面。
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSchemaSlateExtensionStyleAddition BackwardAddition;
};

struct FSchemaSlateRunExtension : TSharedFromThis<FSchemaSlateRunExtension>
{
	FSchemaSlateRunExtension(const FSchemaSlateExtensionStyle& SlateExtensionStyle, TSharedPtr<FRichSchemaDecorator> InOwnerDecorator);
	virtual ~FSchemaSlateRunExtension() {}

	virtual bool SupportsForward() const;
	virtual bool SupportsBackward() const;

	virtual int32 OnPaint(const FSchemaSlateExtensionStyleAddition& AdditionStyle, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;
	
protected:
	virtual void GetExtensionMetrics(const FSchemaSlateExtensionStyleAddition& AdditionStyle, const FTextArgs& TextArgs, const float InFontScale, int16& OutLinePos, int16& OutLineThickness, FVector2f& Offset, float& Width) const;

	friend class FSchemaSlateWidgetRun;
	friend class FSchemaSlateTextRun;

	FSchemaSlateExtensionStyle ExtensionStyle;
	TWeakPtr<FRichSchemaDecorator> OwnerDecorator;
};


class FSchemaSlateWidgetRun : public FSlateWidgetRun
{
public:
	static TSharedRef<FSchemaSlateWidgetRun> Create(TSharedPtr<FSchemaSlateRunExtension> InSlateRunExtension, const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FSlateWidgetRun::FWidgetRunInfo& InWidgetInfo, const FTextRange& InRange);
	
	virtual int32 OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual ~FSchemaSlateWidgetRun() {}

protected:
	FSchemaSlateWidgetRun(TSharedPtr<FSchemaSlateRunExtension> InSlateRunExtension, const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FSlateWidgetRun::FWidgetRunInfo& InWidgetInfo, const FTextRange& InRange);
	
	TSharedPtr<FSchemaSlateRunExtension> SlateRunExtension;
};

class FSchemaSlateTextRun : public FSlateTextRun
{
public:
	static TSharedRef<FSchemaSlateTextRun> Create(TSharedPtr<FSchemaSlateRunExtension> InSlateRunExtension, const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FTextBlockStyle& Style, const FTextRange& InRange);

	virtual int32 OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual ~FSchemaSlateTextRun() {}

protected:
	FSchemaSlateTextRun(TSharedPtr<FSchemaSlateRunExtension> InSlateRunExtension, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FTextBlockStyle& InStyle, const FTextRange& InRange);

	TSharedPtr<FSchemaSlateRunExtension> SlateRunExtension;
};