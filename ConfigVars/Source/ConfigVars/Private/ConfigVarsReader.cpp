#include "ConfigVarsReader.h"

#include "UObject/Package.h"
#include "UObject/ObjectResource.h"
#include "UObject/UObjectGlobals.h"

#include "BitArray.h"
#include "ConfigVarsLinker.h"

static int32 LRUCacheMaxNum = 128;

FInstancedStruct UConfigVarsBagReader::LoadData(UObject* Outer, FConfigVarsBag ConfigVarsBag)
{
	return FInstancedStruct(ConfigVarsBag.LoadData(Outer));
}

void UConfigVarsBagReader::LoadData_Async(UObject* Outer, FConfigVarsBag ConfigVarsBag, FOnConfigVarsAsyncCallBack CallBack, int32 Priority)
{
	ConfigVarsBag.LoadData_Async(Outer, CallBack, Priority);
}

FConfigVarsBag::~FConfigVarsBag()
{
#if WITH_EDITOR
	if (Outermost && ExportIndex != INDEX_NONE)
	{
		UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Outermost->GetPackage(), TEXT("ConfigVarsLinker"));
		if (ConfigVarsLinker)
		{
			ConfigVarsLinker->RemoveData(ExportIndex);
		}
	}
#endif
}

bool FConfigVarsBag::Serialize(FArchive& Ar)
{
	Ar << ExportIndex;

#if WITH_EDITORONLY_DATA
	if (!Ar.IsFilterEditorOnly())
	{
		Ar << Outermost;
	}
#endif

	return true;
}

FConstStructView FConfigVarsBag::LoadData(UObject* Outer)
{
	if (!Outer)
	{
		return FConstStructView();
	}
	UPackage* Package = Outer->GetPackage();

	if (ExportIndex == INDEX_NONE)	// ExportIndex不存在，不可能找到记录，直接退出
	{
		return FConstStructView();
	}


	// 由ConfigVarsLinker继续寻找
	UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("ConfigVarsLinker"));
	if (ConfigVarsLinker)
	{
		return ConfigVarsLinker->LoadData(ExportIndex);
	}

	return FConstStructView();
}

void FConfigVarsBag::LoadData_Async(UObject* Outer, FOnConfigVarsAsyncCallBack CallBack, int32 Priority)
{
	if (!Outer)
	{
		TArray<FInstancedStruct> NullData;
		CallBack.ExecuteIfBound(NullData);
		return;
	}

	if (ExportIndex == INDEX_NONE)	// ExportIndex不存在，不可能找到记录，直接退出
	{
		TArray<FInstancedStruct> NullData;
		CallBack.ExecuteIfBound(NullData);
		return;
	}

	UPackage* Package = Outer->GetPackage();

	// 由ConfigVarsLinker继续寻找
	UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("ConfigVarsLinker"));
	if (ConfigVarsLinker)
	{
		ConfigVarsLinker->LoadData_Async(ExportIndex, FLoadConfigVarsAsyncDelegate::CreateWeakLambda(Outer, [Package, ExportIndex = ExportIndex, CallBack](TArray<FStructView> OutExportData) {
			TArray<FInstancedStruct> OutStructData;
			OutStructData.Empty(OutExportData.Num());
			for (FStructView Data : OutExportData)
			{
				OutStructData.Emplace(Data);
			}
			CallBack.ExecuteIfBound(OutStructData);
		})
		, Priority);
	}
}

#if WITH_EDITOR
FStructView FConfigVarsBag::LoadOrAddData(UObject* Outer, const UScriptStruct* DataStruct)
{
	if (!Outer)
	{
		return FStructView();
	}
	UPackage* Package = Outer->GetPackage();


	// 由ConfigVarsLinker继续寻找或创建
	UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("ConfigVarsLinker"));
	if (!ConfigVarsLinker)
	{
		ConfigVarsLinker = NewObject<UConfigVarsLinker>(Package, UConfigVarsLinker::StaticClass(), FName("ConfigVarsLinker"), RF_Public | RF_Standalone);
	}
	if (ConfigVarsLinker)
	{
		return ConfigVarsLinker->LoadOrAddData(ExportIndex, DataStruct);
	}

	return FStructView();
}
#endif