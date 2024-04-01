#pragma once

#define PRIVATE_DEFINE(Class, Type, Member, ...) struct FExternal_##Member##_##__VA_ARGS__ {\
        using type = Type Class::*;\
        };\
        template struct FPtrTaker<FExternal_##Member##_##__VA_ARGS__, &Class::Member>;
#define PRIVATE_GET(Obj, Member, ...) (Obj->*FAccess<FExternal_##Member##_##__VA_ARGS__>::ptr)

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