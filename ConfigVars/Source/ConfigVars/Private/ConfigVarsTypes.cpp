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

			// 我们不希望在序列化时修改OldExportIndex，因为会导致异常。
			int32 TempIndex = Linker->GetSerialExportIndex(OldExportIndex);

			Ar << TempIndex;
		}
		else if (Ar.IsLoading())
		{
			Ar << OldExportIndex;
		}
	}
};

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

FConstStructView FConfigVarsBag::LoadData(UObject* Outer) const
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

void FConfigVarsBag::LoadData_Async(UObject* Outer, int32 Priority) const
{
	if (!Outer)
	{
		return;
	}

	if (ExportIndex == INDEX_NONE)	// ExportIndex不存在，不可能找到记录，直接退出
	{
		return;
	}

	UPackage* Package = Outer->GetPackage();

	// 由ConfigVarsLinker继续寻找
	UConfigVarsLinker* ConfigVarsLinker = FindObject<UConfigVarsLinker>(Package, TEXT("ConfigVarsLinker"));
	if (ConfigVarsLinker)
	{
		ConfigVarsLinker->LoadData_Async(ExportIndex, Priority);
	}
}