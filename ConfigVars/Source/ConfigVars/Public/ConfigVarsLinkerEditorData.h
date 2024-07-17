#pragma once

#include "CoreMinimal.h"

#include "ConfigVarsLinkerEditorData.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnConfigVarsBagPropertyNodeChanged, UPackage*, FStringView /*PropertyPath*/)

UCLASS()
class UConfigVarsLinkerEditorData : public UObject
{
	GENERATED_BODY()
public:

	// 用于确保序列化时，ExportData有序紧凑，在新增ArrayItem时会被重置。
	TSet<int32> ExportDataOrderSet;
	// 用于确保序列化时，ExportData的数据嵌套深度，在新增ArrayItem时会被重置。
	TArray<int32> ExportDataDepthSet;

	// 用于记录已经确定的序列化顺序，仅当次序列化时有效。
	TSet<int32> ExportDataSerializeOrderSet;
	// 用于记录已经确定的最顶层数据的序列化顺序，仅当次序列化时有效。
	TSet<int32> TopOrderSet;

	// 用于记录需要被移除的ExportData。
	TSet<int32> PendingRemovedSet;

	FOnConfigVarsBagPropertyNodeChanged OnConfigVarsBagPropertyNodeChanged;
};