#include "ZigGlobalObject.h"
#include "helpers.h"

#include <JavaScriptCore/CallFrameInlines.h>
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/Completion.h>
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/JSContextInternal.h>
#include <JavaScriptCore/JSInternalPromise.h>
#include <JavaScriptCore/JSModuleLoader.h>
#include <JavaScriptCore/JSNativeStdFunction.h>
#include <JavaScriptCore/JSPromise.h>
#include <JavaScriptCore/JSSourceCode.h>
#include <JavaScriptCore/JSValueInternal.h>
#include <JavaScriptCore/JSVirtualMachineInternal.h>
#include <JavaScriptCore/ObjectConstructor.h>
#include <JavaScriptCore/SourceOrigin.h>
#include <JavaScriptCore/Identifier.h>
#include <wtf/URL.h>
#include <JavaScriptCore/ClassInfo.h>
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/VM.h>
#include <JavaScriptCore/WasmFaultSignalHandler.h>
#include <JavaScriptCore/JSCast.h>
#include <JavaScriptCore/InitializeThreading.h>
#include "ZigConsoleClient.h"

#include <JavaScriptCore/JSLock.h>
#include <wtf/StdLibExtras.h>

#include <iostream>
#include <cstdlib>
#include <exception>

#include <JavaScriptCore/JSClassRef.h>
#include <JavaScriptCore/JSCallbackObject.h>

using JSGlobalObject = JSC::JSGlobalObject;
using Exception = JSC::Exception;
using JSValue = JSC::JSValue;
using JSString = JSC::JSString;
using JSModuleLoader = JSC::JSModuleLoader;
using JSModuleRecord = JSC::JSModuleRecord;
using Identifier = JSC::Identifier;
using SourceOrigin = JSC::SourceOrigin;
using JSObject = JSC::JSObject;
using JSNonFinalObject = JSC::JSNonFinalObject;
namespace JSCastingHelpers = JSC::JSCastingHelpers;


extern "C" JSC__JSGlobalObject* Zig__GlobalObject__create(JSClassRef* globalObjectClass, int count, void* console_client) {
    std::set_terminate([](){ Zig__GlobalObject__onCrash(); });
    JSC::initialize();
    
    JSC::VM& vm = JSC::VM::create(JSC::LargeHeap).leakRef();

    #if ENABLE(WEBASSEMBLY)
        JSC::Wasm::enableFastMemory();
    #endif

    JSC::JSLockHolder locker(vm);
    Zig::GlobalObject *globalObject = Zig::GlobalObject::create(vm, Zig::GlobalObject::createStructure(vm, JSC::jsNull()));
    globalObject->setConsole(globalObject);

    if (count > 0) {
        globalObject->installAPIGlobals(globalObjectClass, count);
    }


    JSC::gcProtect(globalObject);
    vm.ref();
    return globalObject;
}

namespace Zig {

const JSC::ClassInfo GlobalObject::s_info = { "GlobalObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(GlobalObject) };

const JSC::GlobalObjectMethodTable GlobalObject::s_globalObjectMethodTable = {
    &supportsRichSourceInfo,
    &shouldInterruptScript,
    &javaScriptRuntimeFlags,
    nullptr, // queueTaskToEventLoop
nullptr,    // &shouldInterruptScriptBeforeTimeout,
    &moduleLoaderImportModule, // moduleLoaderImportModule
    &moduleLoaderResolve, // moduleLoaderResolve
    &moduleLoaderFetch, // moduleLoaderFetch
    &moduleLoaderCreateImportMetaProperties, // moduleLoaderCreateImportMetaProperties
    &moduleLoaderEvaluate, // moduleLoaderEvaluate
    &promiseRejectionTracker, // promiseRejectionTracker
    &reportUncaughtExceptionAtEventLoop,
    &currentScriptExecutionOwner,
    &scriptExecutionStatus,
    nullptr, // defaultLanguage
    nullptr, // compileStreaming
    nullptr, // instantiateStreaming
};

void GlobalObject::reportUncaughtExceptionAtEventLoop(JSGlobalObject* globalObject, Exception* exception) {
    Zig__GlobalObject__reportUncaughtException(globalObject, exception);
}

void GlobalObject::promiseRejectionTracker(JSGlobalObject* obj, JSC::JSPromise* prom, JSC::JSPromiseRejectionOperation reject) {
    Zig__GlobalObject__promiseRejectionTracker(obj, prom, reject == JSC::JSPromiseRejectionOperation::Reject ? 0 : 1);
}

static Zig::ConsoleClient* m_console;

void GlobalObject::setConsole(void* console) {
    m_console = new Zig::ConsoleClient(console);
    this->setConsoleClient(makeWeakPtr(m_console));
}

void GlobalObject::installAPIGlobals(JSClassRef* globals, int count) {
   WTF::Vector<GlobalPropertyInfo> extraStaticGlobals;
    extraStaticGlobals.reserveCapacity((size_t)count);

    for (int i = 0; i < count; i++) {
        auto jsClass = globals[i];

        JSC::JSCallbackObject<JSNonFinalObject>* object = JSC::JSCallbackObject<JSNonFinalObject>::create(this, this->callbackObjectStructure(), jsClass, nullptr);
        if (JSObject* prototype = jsClass->prototype(this))
            object->setPrototypeDirect(vm(), prototype);

        extraStaticGlobals.uncheckedAppend(GlobalPropertyInfo{ JSC::Identifier::fromString(vm(), jsClass->className()),  JSC::JSValue(object),  JSC::PropertyAttribute::DontDelete | 0  });
    }
    this->addStaticGlobals(extraStaticGlobals.data(), count);
    extraStaticGlobals.releaseBuffer();
    
}

JSC::Identifier GlobalObject::moduleLoaderResolve(
    JSGlobalObject* globalObject,
    JSModuleLoader* loader,
    JSValue key,
    JSValue referrer,
    JSValue origin
) {
    auto res = Zig__GlobalObject__resolve(
        globalObject,
        toZigString(key, globalObject),
        toZigString(referrer, globalObject)
    );

    if (res.success) {
        return toIdentifier(res.result.value, globalObject);
    } else {
        auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
        throwException(scope, res.result.err.message, globalObject);
        return globalObject->vm().propertyNames->emptyIdentifier;
    }
    
}

JSC::JSInternalPromise* GlobalObject::moduleLoaderImportModule(JSGlobalObject* globalObject, JSModuleLoader*, JSString* moduleNameValue, JSValue parameters, const SourceOrigin& sourceOrigin)
{
    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* promise = JSC::JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
    RETURN_IF_EXCEPTION(scope, promise->rejectWithCaughtException(globalObject, scope));

    auto sourceURL = sourceOrigin.url();
    auto resolved = Zig__GlobalObject__resolve(globalObject, toZigString(moduleNameValue, globalObject), sourceURL.isEmpty() ? ZigStringCwd : toZigString(sourceURL.fileSystemPath()));
    if (!resolved.success) {
        throwException(scope, resolved.result.err.message, globalObject);
        return promise->rejectWithCaughtException(globalObject, scope);
    }

    auto result = JSC::importModule(globalObject, toIdentifier(resolved.result.value, globalObject), parameters, JSC::jsUndefined());
    RETURN_IF_EXCEPTION(scope, promise->rejectWithCaughtException(globalObject, scope));

    return result;
}


JSC::JSInternalPromise* GlobalObject::moduleLoaderFetch(JSGlobalObject* globalObject, JSModuleLoader* loader, JSValue key, JSValue value1, JSValue value2) {
    JSC::VM& vm = globalObject->vm();
    JSC::JSInternalPromise* promise = JSC::JSInternalPromise::create(vm, globalObject->internalPromiseStructure());

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto rejectWithError = [&](JSC::JSValue error) {
        promise->reject(globalObject, error);
        return promise;
    };

    auto moduleKey = key.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, promise->rejectWithCaughtException(globalObject, scope));
    auto moduleKeyZig = toZigString(moduleKey);

    auto res = Zig__GlobalObject__fetch(
        globalObject,
        moduleKeyZig,
        ZigStringEmpty
    );

    if (!res.success) {
        throwException(scope, res.result.err.message, globalObject);
        RETURN_IF_EXCEPTION(scope, promise->rejectWithCaughtException(globalObject, scope));
    }

    auto code = Zig::toString(res.result.value);
    auto sourceCode = JSC::JSSourceCode::create(
        vm,
        JSC::makeSource(
            code,
            JSC::SourceOrigin { WTF::URL::fileURLWithFileSystemPath(moduleKey) },
            WTFMove(moduleKey),
            TextPosition(),
            JSC::SourceProviderSourceType::Module
        )
    );

    scope.release();
    promise->resolve(globalObject, sourceCode);
    return promise;
}

JSC::JSObject* GlobalObject::moduleLoaderCreateImportMetaProperties(JSGlobalObject* globalObject, JSModuleLoader* loader, JSValue key, JSModuleRecord* record, JSValue val) {
    return nullptr;
    // auto res = Zig__GlobalObject__createImportMetaProperties(
    //     globalObject,
    //     loader,
    //     JSValue::encode(key),
    //     record,
    //     JSValue::encode(val)
    // );

    // return JSValue::decode(res).getObject();
}

JSC::JSValue GlobalObject::moduleLoaderEvaluate(JSGlobalObject* globalObject, JSModuleLoader* moduleLoader, JSValue key, JSValue moduleRecordValue, JSValue scriptFetcher, JSValue sentValue, JSValue resumeMode) {
    // VM& vm = globalObject->vm();
    return moduleLoader->evaluateNonVirtual(globalObject, key, moduleRecordValue, scriptFetcher, sentValue, resumeMode);
}


} // namespace Zig