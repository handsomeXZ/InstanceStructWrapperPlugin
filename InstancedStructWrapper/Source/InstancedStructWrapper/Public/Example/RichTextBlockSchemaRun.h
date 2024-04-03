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

// 增量值，在SlateRun结束后回滚，一般可以交由ForwardAdditiong
struct FSchemaSlateAdditionRendererDelta
{
	FSchemaSlateAdditionRendererDelta() {}
	void Rollback(const FTextArgs& TextArgs);
	void Update(const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect);
	void UpdateVisibleBlock(const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect);
	
	int32 CurLineIndex = -1;			// 非准确Index，指可视的Index
	int32 CurBlockIndex = 0;			// 非准确Index，指可视的Index

	int32 FirstVisibleBlockIndex = 0;	// 准确Index，指可视的Index
	int32 LastVisibleBlockIndex = 0;	// 准确Index，指可视的Index

	bool bCurIsFirstBlock = false;
	bool bCurIsLastBlock = false;

	uint32 LineViewID = 0;
	uint32 FirstBlockID = 0;	// 第一个可视Block
	FVector2D TextArgsOffset;
};

struct FSchemaSlateAdditionRendererParam
{
	FSchemaSlateAdditionRendererParam(FSchemaSlateAdditionRendererDelta& AdditionDelta);

	ESlateAdditionRendererType RendererType;

	int32 LineModelIndex = -1;	// 未被手动换行的整串文本
	int32 BlockIndex = -1;		// 准确Index

	const FSlateBrush* Brush;
	// 增量值，在SlateRun结束后回滚，一般可以交由ForwardAddition修改
	FSchemaSlateAdditionRendererDelta& AdditionDelta;
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
	virtual int32 OnPaint(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const { return LayerId; }
	virtual bool Supports(const FSchemaSlateAdditionRendererParam& Params) const { return false; }
	virtual const UScriptStruct* NeedPayload() const { return nullptr; }

	UPROPERTY(EditAnywhere)
	FSlateBrush DefaultBrush;
	// 默认启用渲染器
	UPROPERTY(EditAnywhere)
	bool bDefaultEnable = false;
	// 在计算渲染位置时，会更依赖文字内容的位置（目前仅影响垂直位置）
	UPROPERTY(EditAnywhere)
	bool bFontImportant = false;
};
USTRUCT(DisplayName="Brush MultiLine Renderer")
struct INSTANCEDSTRUCTWRAPPER_API FSchemaSlateAdditionRenderer_Brush_MultiLine : public FSchemaSlateAdditionRenderer
{
	GENERATED_BODY()
	virtual ~FSchemaSlateAdditionRenderer_Brush_MultiLine() {}
	virtual int32 OnPaint(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;
	virtual bool Supports(const FSchemaSlateAdditionRendererParam& Params) const;

	UPROPERTY(EditAnywhere)
	TEnumAsByte<EHorizontalAlignment> HAlign = HAlign_Center;
	UPROPERTY(EditAnywhere)
	TEnumAsByte<EVerticalAlignment> VAlign = VAlign_Center;
	UPROPERTY(EditAnywhere)
	FMargin Padding;
};
USTRUCT(DisplayName="Head Placeholder MultiLine Renderer")
struct INSTANCEDSTRUCTWRAPPER_API FSchemaSlateAdditionRenderer_HeadPlaceholder_MultiLine : public FSchemaSlateAdditionRenderer
{
	GENERATED_BODY()
	virtual ~FSchemaSlateAdditionRenderer_HeadPlaceholder_MultiLine() {}
	virtual int32 OnPaint(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;
	virtual bool Supports(const FSchemaSlateAdditionRendererParam& Params) const;

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

	friend class FSchemaSlateWidgetRun;
	friend class FSchemaSlateTextRun;

	// 默认支持32个附加渲染器
	// 或许用一个BitArray更合适？
	uint32 AdditionSet;

	int32 BackwardBeginIndex;
	int32 BackwardEndIndex;

	TArray<FSlateBrush> BrushInstance;

	mutable FSchemaSlateAdditionRendererDelta AdditionDelta;
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