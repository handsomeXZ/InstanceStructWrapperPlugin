#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "IDetailCustomNodeBuilder.h"
#include "EditorUndoClient.h"
#include "InstancedStructDetails.h"

class IPropertyHandle;
class IDetailPropertyRow;
class IPropertyHandle;
class FInstancedStructDetails;
class FInstancedStructProvider;

/**
 * Type customization for FInstancedStructWrapperDetails.
 */
class INSTANCEDSTRUCTWRAPPEREDITOR_API FInstancedStructWrapperDetails : public IPropertyTypeCustomization
{
public:

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

protected:
	void OnTextCommitted(const FText& NewLabel, ETextCommit::Type CommitType);
	FText GetCommentAsText() const;
	FText GetTooltipText() const;
private:
	TSharedPtr<FInstancedStructDetails> InstancedStructDetails;

	TSharedPtr<IPropertyHandle> StructProperty;
};

class INSTANCEDSTRUCTWRAPPEREDITOR_API FInstancedStructWrapperDataDetails : public IDetailCustomNodeBuilder, public FSelfRegisteringEditorUndoClient, public TSharedFromThis<FInstancedStructWrapperDataDetails>
{
public:
	FInstancedStructWrapperDataDetails(TSharedPtr<IPropertyHandle> InStructProperty);
	~FInstancedStructWrapperDataDetails();

	//~ Begin IDetailCustomNodeBuilder interface
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override;
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool RequiresTick() const override { return true; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override;
	//~ End IDetailCustomNodeBuilder interface

	/** FEditorUndoClient interface */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	// Called when a child is added, override to customize a child row.
	virtual void OnChildRowAdded(IDetailPropertyRow& ChildRow) {}

private:
	void OnUserDefinedStructReinstancedHandle(const class UUserDefinedStruct& Struct);

	/** Pre/Post change notifications for struct value changes */
	void OnStructValuePreChange();
	void OnStructValuePostChange();
	void OnStructHandlePostChange();

	/** Returns type of the instanced struct for each instance/object being edited. */
	TArray<TWeakObjectPtr<const UStruct>> GetInstanceTypes() const;

	/** Cached instance types, used to invalidate the layout when types change. */
	TArray<TWeakObjectPtr<const UStruct>> CachedInstanceTypes;

	/** Handle to the struct property being edited */
	TSharedPtr<IPropertyHandle> StructProperty;

	/** Struct provider for the structs. */
	TSharedPtr<FInstancedStructProvider> StructProvider;

	/** Delegate that can be used to refresh the child rows of the current struct (eg, when changing struct type) */
	FSimpleDelegate OnRegenerateChildren;

	/** The last time that SyncEditableInstanceFromSource was called, in FPlatformTime::Seconds() */
	double LastSyncEditableInstanceFromSourceSeconds = 0.0;

	/** True if we're currently handling a StructValuePostChange */
	bool bIsHandlingStructValuePostChange = false;

	FDelegateHandle UserDefinedStructReinstancedHandle;
};