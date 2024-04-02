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
	FRichSchemaDecorator(URichTextBlock* InOwnerBlock, URichTextBlockSchemaDecoratorStyleSheet* InStyleSheet, TSharedPtr<struct FSlateAdditionRun> InSlateAdditionRun);
	virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override;

	virtual TSharedRef<ISlateRun> Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText, const TSharedRef< FString >& InOutModelText, const ISlateStyle* Style) override final;
protected:
	virtual bool SupportsDecoratorWidget(const FTextRunParseResults& RunParseResult, const FString& Text) const;
	virtual TSharedPtr<SWidget> CreateDecoratorWidget(const FTextRunInfo& RunInfo, const FTextBlockStyle& TextStyle) const;
	virtual void CreateDecoratorText(const FTextRunParseResults& RunParseResult, const FString& OriginalText, FTextBlockStyle& InOutTextStyle, FString& InOutString) const;

	bool IsValidData() const;
	FName GetParseName() const;
	FName GetParseMetaData() const;

private:
	URichTextBlock* OwnerBlock;
	URichTextBlockSchemaDecoratorStyleSheet* StyleSheet;

	TSharedPtr<struct FSlateAdditionRun> SlateAdditionRun;
	TSharedPtr<class FSlateStyleSet> StyleInstance;
};


//////////////////////////////////////////////////////////////////////////
struct FSchemaSlateAdditionRendererParam
{
	FSchemaSlateAdditionRendererParam();

	int32 LineModelIndex = 0;	// 未被手动换行的整串文本
	int32 LineIndex = 0;
	int32 BlockIndex = 0;

	const FSlateBrush* Brush;
};

UENUM()
enum class ESlateAdditionRendererType : uint8
{
	ForwardAddition,
	BackwardAddition,
};

USTRUCT()
struct INSTANCEDSTRUCTWRAPPER_API FSchemaSlateAdditionRenderer
{
	GENERATED_BODY()
	virtual ~FSchemaSlateAdditionRenderer() {}
	virtual int32 OnPaint(const FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const { return LayerId; }
	virtual bool Supports(const FSchemaSlateAdditionRendererParam& Params) const { return false; }
	virtual const UScriptStruct* NeedPayload() const { return nullptr; }

	UPROPERTY(EditAnywhere)
	FSlateBrush DefaultBrush;
	// 默认启用渲染器
	UPROPERTY(EditAnywhere)
	bool bDefaultEnable = false;
};
USTRUCT(DisplayName="Brush MultiLine Renderer")
struct INSTANCEDSTRUCTWRAPPER_API FSchemaSlateAdditionRenderer_Brush_MultiLine : public FSchemaSlateAdditionRenderer
{
	GENERATED_BODY()
	virtual ~FSchemaSlateAdditionRenderer_Brush_MultiLine() {}
	virtual int32 OnPaint(const FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;
	virtual bool Supports(const FSchemaSlateAdditionRendererParam& Params) const;

	UPROPERTY(EditAnywhere)
	TEnumAsByte<EHorizontalAlignment> HAlign = HAlign_Center;
	// 目前不支持别的VAlign格式，因为我们只能取到字体的高度，暂时没法获得其他控件的高度。所以无法计算富文本每行的真实高度。
	UPROPERTY(VisibleAnywhere)
	TEnumAsByte<EVerticalAlignment> VAlign = VAlign_Center;
	UPROPERTY(EditAnywhere)
	FMargin Padding;
};
//////////////////////////////////////////////////////////////////////////

struct FSlateAdditionRun : TSharedFromThis<FSlateAdditionRun>
{
	FSlateAdditionRun(const URichTextBlockSchemaDecoratorStyleSheet* InStyleSheet);
	virtual ~FSlateAdditionRun() {}

	virtual int32 DrawForward(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;
	virtual int32 DrawBackward(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;

	void SetEnable(ESlateAdditionRendererType Type, int32 Index, bool bIsEnable);
	void SetBrush(ESlateAdditionRendererType Type, int32 Index, FSlateBrush Brush);
protected:
	virtual int32 OnPaint(const FInstancedStructContainer& SlateAdditions, uint32 BeginIndex, FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;
	const URichTextBlockSchemaDecoratorStyleSheet* StyleSheet;

	// 默认支持32个附加渲染器
	uint32 AdditionSet;

	int32 BackwardBeginIndex;
	int32 BackwardEndIndex;

	TArray<FSlateBrush> BrushInstance;
};


class FSchemaSlateWidgetRun : public FSlateWidgetRun
{
public:
	static TSharedRef<FSchemaSlateWidgetRun> Create(TSharedPtr<FSlateAdditionRun> InSlateAdditionRun, const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FSlateWidgetRun::FWidgetRunInfo& InWidgetInfo, const FTextRange& InRange);
	
	virtual int32 OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual ~FSchemaSlateWidgetRun() {}

protected:
	FSchemaSlateWidgetRun(TSharedPtr<FSlateAdditionRun> InSlateAdditionRun, const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FSlateWidgetRun::FWidgetRunInfo& InWidgetInfo, const FTextRange& InRange);
	
	TSharedPtr<FSlateAdditionRun> SlateAdditionRun;
};

class FSchemaSlateTextRun : public FSlateTextRun
{
public:
	static TSharedRef<FSchemaSlateTextRun> Create(TSharedPtr<FSlateAdditionRun> InSlateAdditionRun, const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FTextBlockStyle& Style, const FTextRange& InRange);

	virtual int32 OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual ~FSchemaSlateTextRun() {}

protected:
	FSchemaSlateTextRun(TSharedPtr<FSlateAdditionRun> InSlateAdditionRun, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FTextBlockStyle& InStyle, const FTextRange& InRange);

	TSharedPtr<FSlateAdditionRun> SlateAdditionRun;
};