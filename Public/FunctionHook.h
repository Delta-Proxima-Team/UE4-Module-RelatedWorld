// Copyright Delta-Proxima Team (c) 2007-2020

#pragma once

#include "CoreMinimal.h"

#define HOOK_COMPONENT(ComponentClass) ComponentClass* p_comp = Cast<ComponentClass>(p_this->GetComponentByClass(ComponentClass::StaticClass()))
#define HOOK_CONTROLLER_COMPONENT(ComponentClass) ComponentClass* p_comp = Cast<ComponentClass>(p_this->GetPawn()->GetComponentByClass(ComponentClass::StaticClass()))

#define DECLARE_UFUNCTION_HOOK(Class, Func) \
void HOOK_##Class##_##Func##_Implementation(UObject* Context, FFrame& Stack, RESULT_DECL); \
\
struct FHOOK_##Class##_##Func \
{ \
	UFunction* Function; \
	FNativeFuncPtr StoredPtr; \
	FNativeFuncPtr HookedPtr; \
	FNativeFuncPtr RedirectPtr; \
	EFunctionFlags StoredFlags; \
	EFunctionFlags HookFlags; \
\
	FHOOK_##Class##_##Func() \
	{ \
		Function = Class::StaticClass()->FindFunctionByName(TEXT(#Func)); \
		check(Function); \
		HookedPtr = Function->GetNativeFunc(); \
		HookFlags = Function->FunctionFlags; \
		RedirectPtr = &HOOK_##Class##_##Func##_Implementation; \
	} \
}

#define DEFINE_UFUNCTION_HOOK(Class, Func) FHOOK_##Class##_##Func Hook_##Class##_##Func##_Struct

#define IMPLEMENT_UFUNCTION_HOOK(Class, Func) void HOOK_##Class##_##Func##_Implementation(UObject* Context, FFrame& Stack, RESULT_DECL) \
{ \
	Class* p_this = CastChecked<Class>(Context); \


#define END_UFUNCTION_HOOK }
	

#define FLAGS_UFUNCTION_HOOK(Class, Func, Flags) Hook_##Class##_##Func##_Struct.HookFlags |= Flags

#define ENABLE_UFUNCTION_HOOK(Class, Func) \
if (Hook_##Class##_##Func##_Struct.StoredPtr != Hook_##Class##_##Func##_Struct.HookedPtr) \
{ \
	Hook_##Class##_##Func##_Struct.StoredFlags = Hook_##Class##_##Func##_Struct.Function->FunctionFlags; \
	Hook_##Class##_##Func##_Struct.StoredPtr = Hook_##Class##_##Func##_Struct.HookedPtr; \
	Hook_##Class##_##Func##_Struct.Function->FunctionFlags = Hook_##Class##_##Func##_Struct.HookFlags; \
	Hook_##Class##_##Func##_Struct.Function->SetNativeFunc(Hook_##Class##_##Func##_Struct.RedirectPtr); \
}

#define DISABLE_UFUNCTION_HOOK(Class, Func) \
if (Hook_##Class##_##Func##_Struct.StoredPtr == Hook_##Class##_##Func##_Struct.HookedPtr) \
{ \
	Hook_##Class##_##Func##_Struct.Function->FunctionFlags = Hook_##Class##_##Func##_Struct.StoredFlags; \
	Hook_##Class##_##Func##_Struct.StoredPtr = nullptr; \
	Hook_##Class##_##Func##_Struct.Function->SetNativeFunc(Hook_##Class##_##Func##_Struct.HookedPtr); \
}
