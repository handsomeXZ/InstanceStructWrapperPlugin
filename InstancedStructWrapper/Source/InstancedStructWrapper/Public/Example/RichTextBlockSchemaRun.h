#pragma once

#include "CoreMinimal.h"
#include "Framework/Text/ITextDecorator.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/SlateWidgetRun.h"

#include "RichTextBlockSchemaRun.generated.h"

class URichTextBlockSchemaDecorator;
class URichTextBlockSchemaDecoratorStyleSheet;


struct FSchemaDecoratorProxy
{
	FSchemaDecoratorProxy(URichTextBlockSchemaDecorator* InOwnerSchemaDecorator)
		: OwnerDecorator(InOwnerSchemaDecorator)
	{}
	
	uint32 GetForwardAdditionSet();
	uint32 GetBackwardAdditionSet();
	const TMap<int32, FInstancedStruct>& GetForwardPayloadMap();
	const TMap<int32, FInstancedStruct>& GetBackwardPayloadMap();

	bool IsValid() { return OwnerDecorator.IsValid(); }

private:
	TWeakObjectPtr<URichTextBlockSchemaDecorator> OwnerDecorator;
};


class FRichSchemaDecorator : public ITextDecorator, public TSharedFromThis<FRichSchemaDecorator>
{
public:
	FRichSchemaDecorator(URichTextBlock* InOwnerBlock, URichTextBlockSchemaDecoratorStyleSheet* InStyleSheet, URichTextBlockSchemaDecorator* InOwnerSchemaDecorator)
		: OwnerBlock(InOwnerBlock)
		, StyleSheet(InStyleSheet)
		, OwnerSchemaDecorator(InOwnerSchemaDecorator)
	{
	}
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
struct FSchemaSlateAdditionRendererParam
{
	FSchemaSlateAdditionRendererParam(const TSharedRef<const FString>& InContentText);

	int32 LineModelIndex = 0;	// 未被手动换行的整串文本
	int32 LineIndex = 0;
	int32 BlockIndex = 0;

	TSharedRef<const FString> ContentText;
	FTextRange TextRange;

	const FInstancedStruct* Payload;
};

USTRUCT(BlueprintType)
struct INSTANCEDSTRUCTWRAPPER_API FSlateAdditionCommonPayload
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSlateBrush BrushOverride;
};

USTRUCT()
struct INSTANCEDSTRUCTWRAPPER_API FSchemaSlateAdditionRenderer
{
	GENERATED_BODY()
	virtual ~FSchemaSlateAdditionRenderer() {}
	virtual int32 OnPaint(const FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const { return LayerId; }
	virtual bool Supports(const FSchemaSlateAdditionRendererParam& Params) const { return false; }
	virtual const UScriptStruct* NeedPayload() const { return nullptr; }

	void GetExtensionMetrics(EHorizontalAlignment HAlign, EVerticalAlignment VAlign, FMargin Padding, FVector2f BrushSize, const FTextArgs& TextArgs, const float InFontScale, float& OutLineThickness, FVector2f& Offset, float& Width) const;


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
	virtual const UScriptStruct* NeedPayload() const { return FSlateAdditionCommonPayload::StaticStruct(); }

	UPROPERTY(EditAnywhere)
	FSlateBrush Brush;
	UPROPERTY(EditAnywhere)
	TEnumAsByte<EHorizontalAlignment> HAlign = HAlign_Center;
	// 目前不支持别的VAlign格式，因为我们只能取到字体的高度，暂时没法获得其他控件的高度。所以无法计算富文本每行的真实高度。
	UPROPERTY(VisibleAnywhere)
	TEnumAsByte<EVerticalAlignment> VAlign = VAlign_Center;
	UPROPERTY(EditAnywhere)
	FMargin Padding;
};
//////////////////////////////////////////////////////////////////////////

struct FSchemaSlateRunExtension : TSharedFromThis<FSchemaSlateRunExtension>
{
	FSchemaSlateRunExtension(const FInstancedStructContainer& InForwardAddition, const FInstancedStructContainer& InBackwardAddition, FSchemaDecoratorProxy InDecoratorProxy);
	virtual ~FSchemaSlateRunExtension() {}

	virtual int32 DrawForward(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled);
	virtual int32 DrawBackward(FSchemaSlateAdditionRendererParam& Params, const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled);

protected:
	friend class FSchemaSlateWidgetRun;
	friend class FSchemaSlateTextRun;

	const FInstancedStructContainer& ForwardAddition;
	const FInstancedStructContainer& BackwardAddition;

	FSchemaDecoratorProxy DecoratorProxy;
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