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
	FRichSchemaDecorator(URichTextBlock* InOwnerBlock, URichTextBlockSchemaDecoratorStyleSheet* InStyleSheet, TSharedPtr<struct FSlateAdditionBatchRun> InSlateAdditionRun);
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

	TSharedPtr<struct FSlateAdditionBatchRun> SlateAdditionRun;
	TSharedPtr<class FSlateStyleSet> StyleInstance;
};


//////////////////////////////////////////////////////////////////////////

// 增量值，每次绘制Batch时刷新的数据
struct FSchemaSlateAdditionRendererBatchDelta
{
	FSchemaSlateAdditionRendererBatchDelta() {}
	void Rollback(const FTextArgs& TextArgs, int32 LayerId);
	void Update(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, int32& LayerId);
	void UpdateVisibleBlock(const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect);
	
	int32 CurLineIndex = -1;			// 非准确Index，仅递增，指可视的Index
	int32 CurBlockIndex = -1;			// 非准确Index，仅递增，指可视的Index

	int32 FirstVisibleBlockIndex = -1;	// 准确Index，指可视的Index，每行
	int32 LastVisibleBlockIndex = -1;	// 准确Index，指可视的Index，每行

	bool bCurIsFirstBlock = false;
	bool bCurIsLastBlock = false;

	int32 RealLayerId = 0;
	int32 BackwardMaxLayerId = 0;		// 仅在Backward执行前后才会更新
public:
	// Delta...
	FVector2D TextArgsOffset_Line;
	FVector2D TextArgsOffset_Block;
private:
	uint64 PrevLineID = UINT64_MAX;
	double CurrentTime = -1;
};

// 每次Block绘制时会封装的一些参数
struct FSchemaSlateAdditionRendererParam
{
	FSchemaSlateAdditionRendererParam(FSchemaSlateAdditionRendererBatchDelta& AdditionBatchDelta);

	ESlateAdditionRendererType RendererType;

	const FSlateBrush* Brush;

	// 增量值，每次绘制Batch时刷新的数据
	FSchemaSlateAdditionRendererBatchDelta& AdditionBatchDelta;
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
	bool bNeedPlacehold = false;
	UPROPERTY(EditAnywhere)
	FMargin Padding;
};
USTRUCT(DisplayName="Head Placeholder FirstLine Renderer")
struct INSTANCEDSTRUCTWRAPPER_API FSchemaSlateAdditionRenderer_HeadPlaceholder_FirstLine : public FSchemaSlateAdditionRenderer_HeadPlaceholder_MultiLine
{
	GENERATED_BODY()
	virtual ~FSchemaSlateAdditionRenderer_HeadPlaceholder_FirstLine() {}
	virtual bool Supports(const FSchemaSlateAdditionRendererParam& Params) const;
};
//////////////////////////////////////////////////////////////////////////

struct FSlateAdditionBatchRun : TSharedFromThis<FSlateAdditionBatchRun>
{
	FSlateAdditionBatchRun(const URichTextBlockSchemaDecoratorStyleSheet* InStyleSheet);
	virtual ~FSlateAdditionBatchRun() {}

	void SetEnable(ESlateAdditionRendererType Type, int32 Index, bool bIsEnable);
	void SetBrush(ESlateAdditionRendererType Type, int32 Index, FSlateBrush Brush);

protected:
	virtual int32 Begin(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled);
	virtual int32 End(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled);

	virtual int32 DrawForward(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;
	virtual int32 DrawBackward(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;

	virtual void Rollback(const FTextArgs& TextArgs, int32 LayerId);
	virtual void Update(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, int32& LayerId);
	virtual int32 OnPaint(const FInstancedStructContainer& SlateAdditions, uint32 BeginIndex, FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;

	friend class FSchemaSlateWidgetRun;
	friend class FSchemaSlateTextRun;

	/**
	 * 默认支持32个附加渲染器，原本的富文本会尽量进行合批渲染。
	 * 但AdditionRender可以对此造成影响，因为每加一个渲染器将会增加一层LayerId。
	 * 请尽量选择'最'前向渲染和'最'延迟渲染。这样才不会影响原有富文本的合批。
	 */
	uint32 AdditionSet;	// 或许用一个BitArray来记录更合适？

	int32 BackwardBeginIndex;
	int32 BackwardEndIndex;

	TArray<FSlateBrush> BrushInstance;

	const URichTextBlockSchemaDecoratorStyleSheet* StyleSheet;

	mutable FSchemaSlateAdditionRendererBatchDelta AdditionBatchDelta;
};


class FSchemaSlateWidgetRun : public FSlateWidgetRun
{
public:
	static TSharedRef<FSchemaSlateWidgetRun> Create(TSharedPtr<FSlateAdditionBatchRun> InSlateAdditionRun, const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FSlateWidgetRun::FWidgetRunInfo& InWidgetInfo, const FTextRange& InRange);
	
	virtual int32 OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual ~FSchemaSlateWidgetRun() {}

protected:
	FSchemaSlateWidgetRun(TSharedPtr<FSlateAdditionBatchRun> InSlateAdditionRun, const TSharedRef<class FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FSlateWidgetRun::FWidgetRunInfo& InWidgetInfo, const FTextRange& InRange);
	
	TSharedPtr<FSlateAdditionBatchRun> SlateAdditionRun;
};

class FSchemaSlateTextRun : public FSlateTextRun
{
public:
	static TSharedRef<FSchemaSlateTextRun> Create(TSharedPtr<FSlateAdditionBatchRun> InSlateAdditionRun, const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FTextBlockStyle& Style, const FTextRange& InRange);

	virtual int32 OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual ~FSchemaSlateTextRun() {}

protected:
	FSchemaSlateTextRun(TSharedPtr<FSlateAdditionBatchRun> InSlateAdditionRun, const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FTextBlockStyle& InStyle, const FTextRange& InRange);

	TSharedPtr<FSlateAdditionBatchRun> SlateAdditionRun;
};