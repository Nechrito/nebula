//------------------------------------------------------------------------------
//  monoserver.cc
//  (C) 2019 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "foundation/stdneb.h"
#include "scripting/mono/monoserver.h"
#include "io/ioserver.h"
#include "io/textreader.h"
#include "mono/metadata/mono-config.h"
#include "mono/utils/mono-error.h"
#include "mono/metadata/debug-helpers.h"
#include "mono/metadata/mono-debug.h"
#include "mono/utils/mono-logger.h"
#include "debug/debugserver.h"
#include "monobindings.h"

using namespace IO;

namespace Scripting
{
__ImplementClass(Scripting::MonoServer, 'MONO', Scripting::ScriptServer);
__ImplementSingleton(Scripting::MonoServer);

using namespace Util;
using namespace IO;

static Mono::MonoBindings bindings;

//------------------------------------------------------------------------------
/**
*/
MonoServer::MonoServer() :
	waitForDebugger(false)
{
    __ConstructSingleton;
}

//------------------------------------------------------------------------------
/**
*/
MonoServer::~MonoServer()
{
    if (this->IsOpen())
    { 
        this->Close();
    }
    __DestructSingleton;
}

//------------------------------------------------------------------------------
/**
*/
static void*
ScriptingAlloc(size_t bytes)
{
	return Memory::Alloc(Memory::HeapType::ScriptingHeap, bytes);
}

//------------------------------------------------------------------------------
/**
*/
static void*
ScriptingRealloc(void* ptr, size_t bytes)
{
	return Memory::Realloc(Memory::HeapType::ScriptingHeap, ptr, bytes);
}

//------------------------------------------------------------------------------
/**
*/
static void
ScriptingDealloc(void* ptr)
{
	Memory::Free(Memory::HeapType::ScriptingHeap, ptr);
}

//------------------------------------------------------------------------------
/**
*/
static void*
ScriptingCalloc(size_t count, size_t size)
{
	const size_t bytes = size * count;
	void* ptr = Memory::Alloc(Memory::HeapType::ScriptingHeap, bytes);
	memset(ptr, 0, bytes);
	return ptr;
}

//------------------------------------------------------------------------------
/**
*/
bool
MonoServer::Open()
{
    n_assert(!this->IsOpen());    
    if (ScriptServer::Open())
    {
		/// Setup a custom allocator for unmanaged memory allocated in CLR
		MonoAllocatorVTable mem_vtable = { 
			1,
			ScriptingAlloc,
			ScriptingRealloc,
			ScriptingDealloc,
			ScriptingCalloc
		};
		
		if (!mono_set_allocator_vtable(&mem_vtable))
			n_warning("Mono allocator not set!");

		mono_config_parse(NULL);

		if (waitForDebugger)
		{
			static const char* options[] = {
				"--debugger-agent=address=0.0.0.0:55555,transport=dt_socket,server=y",
				"--soft-breakpoints",
				// "--trace"
			};
			mono_jit_parse_options(sizeof(options) / sizeof(char*), (char**)options);
		}

		// Intialize JIT runtime
		this->domain = mono_jit_init("Nebula Scripting Subsystem");
		if (!domain)
		{
			n_error("Failed to initialize Mono JIT runtime!");
		}

		if (waitForDebugger)
		{
			mono_debug_init(MONO_DEBUG_FORMAT_MONO);
			mono_debug_domain_create(this->domain);
		}

		mono_trace_set_log_handler(Mono::N_Log, nullptr);
		mono_trace_set_print_handler(Mono::N_Print);
		mono_trace_set_printerr_handler(Mono::N_Error);

		IO::URI uri = IO::URI("bin:scripts.dll");
		Util::String path = uri.AsString();

		// setup executable
		MonoAssembly* assembly;
		assembly = mono_domain_assembly_open(domain, path.AsCharPtr());
		if (!assembly)
			n_error("Mono initialization: Could not load Mono assembly!");

		mono_assembly_set_main(assembly);

		char* argc[1] = { "scripts.dll" };

		MonoImage* image = mono_assembly_get_image(assembly);

		MonoClass* cls = mono_class_from_name(image, "Nebula", "AppEntry");

		MonoMethodDesc* desc = mono_method_desc_new(":Main()", false);
		MonoMethod* entryPoint = mono_method_desc_search_in_class(desc, cls);

		if (!entryPoint)
			n_error("Could not find entry point for Mono scripts!");

		bindings = Mono::MonoBindings();
		bindings.Initialize();

		mono_runtime_invoke(entryPoint, NULL, NULL, NULL);

		return true;
    }
    return false;
}

//------------------------------------------------------------------------------
/**
*/
void
MonoServer::Close()
{
    n_assert(this->IsOpen());    
    
    // this will unregister all commands
    ScriptServer::Close();

    // shutdown mono runtime
	mono_jit_cleanup(this->domain);
}

//------------------------------------------------------------------------------
/**
*/
bool
MonoServer::Eval(const String& str)
{
    n_assert(this->IsOpen());    
    n_assert(str.IsValid());

	return false;
}


//------------------------------------------------------------------------------
/**
    Evaluate script in file
*/
bool
MonoServer::EvalFile(const IO::URI& file)
{
    n_assert(this->IsOpen());
    auto stream = IO::IoServer::Instance()->CreateStream(file);
    Ptr<TextReader> reader = IO::TextReader::Create();
    reader->SetStream(stream);
    if (reader->Open())
    {
        Util::String str = reader->ReadAll();
        reader->Close();
        return this->Eval(str);
    }
    return false;
}

//------------------------------------------------------------------------------
/**
*/
void
MonoServer::SetDebuggingEnabled(bool enabled)
{
	this->waitForDebugger = enabled;
}

} // namespace Scripting