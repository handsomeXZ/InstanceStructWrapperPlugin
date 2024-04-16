#pragma once

#define PRIVATE_DEFINE_VAR(Class, Type, Member, ...) struct FExternal_Var_##Member##_##__VA_ARGS__ {\
        using type = Type Class::*;\
        };\
        template struct FPtrTaker<FExternal_Var_##Member##_##__VA_ARGS__, &Class::Member>;

#define PRIVATE_DEFINE_FUNC(Class, ReturnType, Member, ...) struct FExternal_Func_##Member {\
        using type = ReturnType(Class::*)(__VA_ARGS__);\
        };\
        template struct FPtrTaker<FExternal_Func_##Member, &Class::Member>;


#define PRIVATE_GET_VAR(Obj, Member, ...) (Obj->*FAccess<FExternal_Var_##Member##_##__VA_ARGS__>::ptr)
#define PRIVATE_GET_FUNC(Obj, Member) (Obj->*FAccess<FExternal_Func_##Member>::ptr)

template <typename Tag>
class FAccess {
public:
	inline static typename Tag::type ptr;
};

template <typename Tag, typename Tag::type V>
struct FPtrTaker {
	struct FTransferer {
		FTransferer() {
			FAccess<Tag>::ptr = V;
		}
	};
	inline static FTransferer tr;
};