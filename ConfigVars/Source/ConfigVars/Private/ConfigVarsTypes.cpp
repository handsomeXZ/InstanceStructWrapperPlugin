#include "ConfigVarsTypes.h"

#include "ConfigVarsLinker.h"

class FConfigVarsReaderUtils
{
public:
	static void SerializeExportIndex(FArchive& Ar, UConfigVarsLinker* Linker, int32& OldExportIndex)
	{
		// 如果Archive正在保存或加载用于持久存储的数据，并且应该跳过瞬态数据。那么允许使用矫正映射后的新ExportIndex
		if (Ar.IsSaving() && Ar.IsPersistent())
		{
			if (!IsValid(Linker) || OldExportIndex == INDEX_NONE)
			{
				Ar << OldExportIndex;
				return;
			}

			// 我们不希望在反序列化时修改OldExportIndex，因为会导致异常。
			int32 TempIndex = Linker->GetSerialExportIndex(OldExportIndex);

			Ar << TempIndex;
		}
		else/* if (Ar.IsLoading()) 蓝图资产的Ar状态有点混乱，先去掉了，反正只是读取，不影响数据*/
		{
			Ar << OldExportIndex;
		}
	}

	static UConfigVarsLinker* GetDuplicatedLinkerForRuntime(const UObject* Outer)
	{
		if (!IsValid(Outer))
		{
			return nullptr;
		}

		UPackage* Package = Outer->GetPackage();

		// 由ConfigVarsLinker继续寻找
		UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("Runtime_ConfigVarsLinker_Duplicated"));
		if (!ConfigVarsLinker)
		{
			ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("Template_ConfigVarsLinker"));
			if (ConfigVarsLinker)
			{
				ConfigVarsLinker = DuplicateObject(ConfigVarsLinker, Package, TEXT("Runtime_ConfigVarsLinker_Duplicated"));
				ConfigVarsLinker->ClearFlags(RF_Public | RF_Standalone);
				ConfigVarsLinker->SetFlags(RF_Transient);
			}
			else
			{
				return nullptr;
			}
		}

		return ConfigVarsLinker;
	}
};

FConfigVarsBag::~FConfigVarsBag()
{

#if WITH_EDITOR && WITH_EDITORONLY_DATA
	if (IsValid(Linker) && !(Linker->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed)))
	{
		Linker->MarkPendingRemoved(ExportIndex, true);
	}
#endif

}

bool FConfigVarsBag::Serialize(FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
	FConfigVarsReaderUtils::SerializeExportIndex(Ar, Linker, ExportIndex);
#else
	Ar << ExportIndex;
#endif



#if WITH_EDITORONLY_DATA
	if (!Ar.IsFilterEditorOnly())
	{
		Ar << Outermost;
	}
#endif

	return true;
}

FConstStructView FConfigVarsBag::LoadData(const UObject* Outer) const
{
	if (ExportIndex == INDEX_NONE)	// ExportIndex不存在，不可能找到记录，直接退出
	{
		return FConstStructView();
	}

	// 由ConfigVarsLinker继续寻找
	UConfigVarsLinker* ConfigVarsLinker = FConfigVarsReaderUtils::GetDuplicatedLinkerForRuntime(Outer);
	if (ConfigVarsLinker)
	{
		return ConfigVarsLinker->LoadData(ExportIndex);
	}

	return FConstStructView();
}

void FConfigVarsBag::LoadData_Async(const UObject* Outer, int32 Priority) const
{
	if (ExportIndex == INDEX_NONE)	// ExportIndex不存在，不可能找到记录，直接退出
	{
		return;
	}

	// 由ConfigVarsLinker继续寻找
	UConfigVarsLinker* ConfigVarsLinker = FConfigVarsReaderUtils::GetDuplicatedLinkerForRuntime(Outer);
	if (ConfigVarsLinker)
	{
		ConfigVarsLinker->LoadData_Async(ExportIndex, Priority);
	}
}

void FConfigVarsBag::LoadData_Multi_Async(const UObject* Outer, FConfigVarsBag ConfigVarsBegin, FConfigVarsBag ConfigVarsEnd, int32 Priority)
{
	if (ConfigVarsBegin.ExportIndex == INDEX_NONE || ConfigVarsEnd.ExportIndex == INDEX_NONE)	// ExportIndex不存在，不可能找到记录，直接退出
	{
		return;
	}

	// 由ConfigVarsLinker继续寻找
	UConfigVarsLinker* ConfigVarsLinker = FConfigVarsReaderUtils::GetDuplicatedLinkerForRuntime(Outer);
	if (ConfigVarsLinker)
	{
		ConfigVarsLinker->LoadData_Multi_Async(ConfigVarsBegin.ExportIndex, ConfigVarsEnd.ExportIndex, Priority);
	}
}

void FConfigVarsBag::LoadData_Nested_Async(const UObject* Outer, FConfigVarsBag ConfigVarsBag, int32 Priority)
{
	if (ConfigVarsBag.ExportIndex == INDEX_NONE)	// ExportIndex不存在，不可能找到记录，直接退出
	{
		return;
	}

	// 由ConfigVarsLinker继续寻找
	UConfigVarsLinker* ConfigVarsLinker = FConfigVarsReaderUtils::GetDuplicatedLinkerForRuntime(Outer);
	if (ConfigVarsLinker)
	{
		ConfigVarsLinker->LoadData_Nested_Async(ConfigVarsBag.ExportIndex, Priority);
	}
}

#if WITH_EDITOR
FStructView FConfigVarsBag::EditorLoadData(const UObject* Outer) const
{
	if (!Outer)
	{
		return FStructView();
	}
	UPackage* Package = Outer->GetPackage();

	if (ExportIndex == INDEX_NONE)	// ExportIndex不存在，不可能找到记录，直接退出
	{
		return FStructView();
	}


	// 由ConfigVarsLinker继续寻找
	UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("Template_ConfigVarsLinker"));
	if (ConfigVarsLinker)
	{
		return ConfigVarsLinker->LoadData(ExportIndex);
	}

	return FStructView();
}

FStructView FConfigVarsBag::EditorLoadOrAddData(UObject* Outer, const UScriptStruct* TemplateDataStruct)
{
	if (!Outer)
	{
		return FStructView();
	}
	UPackage* Package = Outer->GetPackage();

	// 由ConfigVarsLinker继续寻找
	UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("Template_ConfigVarsLinker"));
	if (!ConfigVarsLinker)
	{
		ConfigVarsLinker = NewObject<UConfigVarsLinker>(Package, UConfigVarsLinker::StaticClass(), FName("Template_ConfigVarsLinker"), RF_Public | RF_Standalone);
	}
	if (ConfigVarsLinker)
	{
		return ConfigVarsLinker->LoadOrAddData(*this, TemplateDataStruct, Outer);
	}

	return FStructView();
}
#endif