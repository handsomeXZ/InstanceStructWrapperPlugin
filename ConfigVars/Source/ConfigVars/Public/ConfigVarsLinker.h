#pragma once

#include "CoreMinimal.h"

#include "BitArray.h"
#include "InstancedStruct.h"
#include "StructView.h"

#include "ConfigVarsLinker.generated.h"

typedef TMap<int32, UObject*> FImportObjectMap;
#define MAX_EXPORTINDEX 0xfffeui16	// 65534
#define OVER_EXPORTINDEX 0xffffui16	// 65535

DECLARE_DELEGATE_OneParam(FLoadConfigVarsAsyncDelegate, TArray<FStructView>);

class UConfigVarsLinkerEditorData;

struct FConfigVarsImport
{
	FConfigVarsImport() {}
	FConfigVarsImport(const UObject* InObject)
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
		: SerialLocation(0)
		, ClassIndex(INDEX_NONE)
		, Depth(0)
		, ImportSet(0)
	{}

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


	/**
	 * 服务于ConfigVars嵌套，最外层永远为1.
	 */
	uint8			Depth;


	FBitArray		ImportSet;

	friend FArchive& operator<<(FArchive& Ar, FConfigVarsExport& Export);
};

struct FLoadedConfigVarsData
{
	FLoadedConfigVarsData() : ExportIndex(INDEX_NONE) {}
	FLoadedConfigVarsData(int32 InExportIndex, const UScriptStruct* DataStruct)
		: ExportIndex(InExportIndex)
		, Data(DataStruct)
	{}

	int32 ExportIndex;
	FInstancedStruct Data;
};

enum EConfigVarsSerialStage : uint8
{
	None            = 0,
	Cooking			= 1 << 1,
	PreSerialize	= 1 << 2,
	Serialize		= 1 << 3,
};
ENUM_CLASS_FLAGS(EConfigVarsSerialStage);

UCLASS()
class CONFIGVARS_API UConfigVarsLinker : public UObject
{
	GENERATED_BODY()
public:
	virtual void Serialize(FStructuredArchive::FRecord Record) override final;
	virtual bool Rename(const TCHAR* NewName = nullptr, UObject* NewOuter = nullptr, ERenameFlags Flags = REN_None) override;

	// 序列化为Import（这里记录的ImportObject，仅会在对应的ExportObject加载前才会被加载）
	int32 ImportObject(const UObject* ImportObj);

	FStructView LoadData(int32 ExportIndex);
	void LoadData_Async(int32 ExportIndex, int32 Priority);
	void LoadData_Multi_Async(int32 BeginExportIndex, int32 EndExportIndex, int32 Priority);
	// 加载嵌套的懒加载块，Depth为深度，Depth = -1时，加载当前数据块下的所有嵌套数据块
	void LoadData_Nested_Async(int32 ExportIndex, int32 Priority);

	UConfigVarsLinkerEditorData* GetLinkerEditorData();

private:
	friend struct FConfigVarsBag;
	friend class FArchiveConfigVars;
	friend class FConfigVarsUtils;
	friend class FConfigVarsReaderUtils;
	friend class FConfigVarsDetailUtils;

	// 是否跳过反序列化
	void SerializeHeadData(FStructuredArchive::FRecord Record);
	void SerializeExportData(FStructuredArchive::FRecord Record);
	void SerializeTableData(FStructuredArchive::FRecord Record);

	// 序列化为Export（暂时不提供给外部）
	void ExportStruct(FStructuredArchive::FRecord Record, FStructView StructData);

	// 真正反序列化Export数据
	void ProcessPendingLoadExports(FStructuredArchive::FRecord Record);
	void PushToPendingLoadExports(uint16 ExportIndexBegin, uint16 ExportIndexEnd);
	
	// 同步加载Imports（批量加载可以起到优化作用）
	void LoadImports_Sync(uint16 ExportIndexBegin, uint16 ExportIndexEnd);
	// 同步加载Exports（批量加载可以起到优化作用）
	void LoadExports_Sync(uint16 ExportIndexBegin, uint16 ExportIndexEnd, TArray<FStructView>& OutExportData);

	// 异步加载Import（非批量）
	int32 LoadImport_Async(int32 ExportIndex, FLoadPackageAsyncDelegate LoadPackageAsyncDelegate, int32 Priority);

	// 异步加载Exports（批量加载可以起到优化作用）
	void LoadExports_Async_Request(uint16 ExportIndexBegin, uint16 ExportIndexEnd, int32 Priority);
	void LoadExports_Async_LoadImports(uint16 ExportIndexBegin, uint16 ExportIndexEnd, int32 Priority);
	void LoadExports_Async_LoadExports(uint16 ExportIndexBegin, uint16 ExportIndexEnd, int32 Priority);

	// 序列化前预处理数据
	void VerifyData(FArchive& Ar, EConfigVarsSerialStage Stage, TFunction<void()> Func);
	// 处理 PendingRemovedExportData
	void VerifyPendingRemovedExport();
	// 确保所有Export都被加载
	void VerifyAllExportLoaded();
	// 处理嵌套的数据
	void VerifyNestedData(FArchive& Ar, int32 ExportIndex);

#if WITH_EDITOR
	FStructView LoadOrAddData(struct FConfigVarsBag& ConfigVarsBag, const UScriptStruct* TemplateDataStruct, UObject* Outermost);
	void ImmediateRemoveData(struct FConfigVarsBag& ConfigVarsBag);
	void MarkPendingRemoved(int32 ExportIndex, bool bIsPendingRemoved);
	FLinkerLoad* CreateLinker_Sync();
#endif
	int32 GetSerialExportIndex(int32 OldExportIndex);

	void AddAsyncLoadFlag();
	void ClearAsyncLoadFlag();

	// ExportTable和ImportTable是一种优化后的序列化数据。（所以ExportTable这些数据会丢弃不必要的信息，在编辑时，与ExportData不一定相对应）
	// ExportData是未优化的待序列化数据和优化后的反序列化数据。

	// Runtime时，不能再手动修改ImportTable和ExportTable，否则存在线程风险
	TArray<FConfigVarsImport> ImportTable;
	TArray<FConfigVarsExport> ExportTable;

	UPROPERTY(Transient)
	TArray<FInstancedStruct> ExportData;

	// -----------------------------------------------------------------------------------
	// 用于存储待反序列化的ExportIndex队列。
	TLockFreePointerListFIFO<void, PLATFORM_CACHE_LINE_SIZE> PendingLoadExports_Async;

	// 用于存储已经被反序列化的ExportData队列
	TLockFreePointerListFIFO<FLoadedConfigVarsData, PLATFORM_CACHE_LINE_SIZE> LoadedConfigVarsDatas_Async;
	// ExportData的写入仅发生在游戏线程，在其他线程的读取需要加锁。
	FCriticalSection ExportDataCritical;

	// Import 依赖加载的计数器
	TMap<FGuid, int32> LoadingImportCounter;

	// 受限于AsyncPackage，所有异步加载请求都会变成最后一次请求的优先级。
	int32 LastAsyncLoadPriority = 0;
	// -----------------------------------------------------------------------------------

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	UConfigVarsLinkerEditorData* LinkerEditorData = nullptr;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> ExportDataOuter;
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