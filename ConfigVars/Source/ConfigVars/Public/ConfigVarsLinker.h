#pragma once

#include "CoreMinimal.h"

#include "BitArray.h"

#include "ConfigVarsLinker.generated.h"

typedef TMap<int32, UObject*> FImportObjectMap;

DECLARE_DELEGATE_OneParam(FLoadConfigVarsAsyncDelegate, TArray<UConfigVarsData*>);

struct FConfigVarsImport
{
	FConfigVarsImport() {}
	FConfigVarsImport(UObject* InObject)
		: ObjectPath(InObject)
	{}
	FConfigVarsImport(FSoftObjectPath& InObjectPath)
		: ObjectPath(InObjectPath)
	{}

	FSoftObjectPath	ObjectPath;

	friend FArchive& operator<<(FArchive& Ar, FConfigVarsImport& Import);
};

struct FConfigVarsExport
{
	FConfigVarsExport()
		: ObjectName(NAME_None)
		, SerialLocation(0)
		, ClassIndex(INDEX_NONE)
		, ImportSet(0)
	{}
	/**
	 * The name of the UObject represented by this resource.
	 * Serialized
	 */
	FName			ObjectName;

	/**
	 * The location offset from Export Serialize Head.
	 * Depending on the loading method, the starting position is actually inaccurate and needs to be corrected.
	 * Serialized
	 */
	int64         	SerialLocation;

	/**
	 * Location of the resource for this export's class (if non-zero).
	 */
	int32  			ClassIndex;


	FBitArray		ImportSet;

	friend FArchive& operator<<(FArchive& Ar, FConfigVarsExport& Export);
};


UCLASS()
class UConfigVarsLinker : public UObject
{
	GENERATED_BODY()
public:
	virtual void Serialize(FStructuredArchive::FRecord Record) override final;

	// 序列化为Import（这里记录的ImportObject，仅会在对应的ExportObject加载前才会被加载）
	int32 ImportObject(class UObject* ImportObj);

	UConfigVarsData* LoadData(int32 ExportIndex);
	void LoadData_Async(int32 ExportIndex, FLoadConfigVarsAsyncDelegate CallBack, int32 Priority);

#if WITH_EDITOR
	UConfigVarsData* LoadOrAddData(int32& InOutExportIndex, const UClass* TemplateDataClass);
	void RemoveData(int32 ExportIndex);
#endif

private:
	friend class FArchiveConfigVars;
	friend class FConfigVarsUtils;

	// 是否跳过反序列化
	void SerializeHeadData(FStructuredArchive::FRecord Record);
	void SerializeExportData(FStructuredArchive::FRecord Record);
	void SerializeTableData(FStructuredArchive::FRecord Record);

	// 序列化为Export（暂时不提供给外部）
	void ExportObject(FStructuredArchive::FRecord Record, class UConfigVarsData* ExportObj);

	// 真正反序列化Export数据
	void ProcessPendingLoadExports(FStructuredArchive::FRecord Record);
	void PushToPendingLoadExports(const TArray<int32>& ExportIndexs);
	
	// 同步加载Imports（批量加载可以起到优化作用）
	void LoadImports_Sync(TArray<int32> ExportIndexs);
	// 同步加载Exports（批量加载可以起到优化作用）
	void LoadExports_Sync(TArray<int32> ExportIndexs, TArray<UConfigVarsData*>& ExportObjs);

	// 异步加载Import（非批量）
	int32 LoadImport_Async(int32 ExportIndex, FLoadPackageAsyncDelegate CallBack, int32 Priority);

	// 异步加载Exports（批量加载可以起到优化作用）
	void LoadExports_Async_Request(TArray<int32> ExportIndexs, FLoadConfigVarsAsyncDelegate CallBack, int32 Priority);
	void LoadExports_Async_LoadImports(TArray<int32> ExportIndexs, FLoadConfigVarsAsyncDelegate CallBack, int32 Priority);
	void LoadExports_Async_LoadExports(TArray<int32> ExportIndexs, FLoadConfigVarsAsyncDelegate CallBack, int32 Priority);

	// 确保所有Export都被加载
	void VerifyAllExportLoaded();

	// Runtime时，不能再手动修改ImportTable和ExportTable，否则存在线程风险
	TArray<FConfigVarsImport> ImportTable;
	TArray<FConfigVarsExport> ExportTable;

	UPROPERTY(Transient)
	TArray<class UConfigVarsData*> ExportObjects;

	// -----------------------------------------------------------------------------------
	// 用于存储待反序列化的Export队列。
	TLockFreePointerListFIFO<void, PLATFORM_CACHE_LINE_SIZE> PendingLoadExports_Async;
	// Import 依赖加载的计数器
	TMap<FGuid, int32> LoadingImportCounter;
	// -----------------------------------------------------------------------------------

#if WITH_EDITOR
	FLinkerLoad* CreateLinker_Sync();
#endif

};

template<>
struct TStructOpsTypeTraits<UConfigVarsLinker> : public TStructOpsTypeTraitsBase2<UConfigVarsLinker>
{
	enum
	{
		WithSerializer = true,
	};
};

/************************************************************************/
/* ConfigVarsData，让FInstancedStruct类型的ConfigVars支持懒加载和缓存优化。	*/
/* 为了支持使用PlaceholderObject，必须继承自UObject。							*/
/* 遵守：内部数据都为静态数据，不能修改	。										*/
/************************************************************************/
UCLASS(Abstract)
class CONFIGVARS_API UConfigVarsData : public UObject
{
	GENERATED_BODY()
public:
	virtual void Serialize(FArchive& Ar) override final {}
	virtual void Serialize(FStructuredArchive::FRecord Record) override final {}

	virtual void SerializeConfigVars(FStructuredArchive::FRecord ExportRecord, UConfigVarsLinker* Linker);

private:
	template<typename SrcType>
	void SerializeProperties(FStructuredArchive::FRecord ExportRecord, UConfigVarsLinker* Linker, const UStruct* DataStruct, SrcType* SrcData);
	template<typename SrcType>
	void SerializeItem(FStructuredArchive::FRecord PropertyRecord, FProperty* ChildProperty, UConfigVarsLinker* Linker, const UStruct* DataStruct, SrcType* SrcData);
};

template<>
struct TStructOpsTypeTraits<UConfigVarsData> : public TStructOpsTypeTraitsBase2<UConfigVarsData>
{
	enum
	{
		WithSerializer = true,
	};
};