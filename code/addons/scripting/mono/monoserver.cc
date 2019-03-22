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

using namespace IO;

namespace Scripting
{
__ImplementClass(Scripting::MonoServer, 'MONO', Scripting::ScriptServer);
__ImplementSingleton(Scripting::MonoServer);

using namespace Util;
using namespace IO;

#define __EXPORT extern "C" __declspec(dllexport)

__EXPORT void N_Print(char* str)
{
	n_printf(str);
}

__EXPORT void Foobar(Util::String const& str)
{
	n_printf("Testing %s\n", str.AsCharPtr());
}

//------------------------------------------------------------------------------
/**
*/
MonoServer::MonoServer()
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
bool
MonoServer::Open()
{
    n_assert(!this->IsOpen());    
    if (ScriptServer::Open())
    {
		// Intialize JIT runtime

		mono_debug_init(MonoDebugFormat::MONO_DEBUG_FORMAT_MONO);
		
		this->domain = mono_jit_init("Nebula Scripting Subsystem");
		if (!domain)
		{
			n_error("Failed to initialize Mono JIT runtime!");
		}
		
		IO::URI uri = IO::URI("bin:scripts.dll");
		Util::String path = uri.AsString();

		// setup executable
		MonoAssembly* assembly;
		assembly = mono_domain_assembly_open(domain, path.AsCharPtr());
		if (!assembly)
			n_error("Mono initialization: Could not load Mono assembly!");



		char* argc[1] = { "scripts.dll" };

		MonoImage* image = mono_assembly_get_image(assembly);

		MonoClass* cls = mono_class_from_name(image, "", "Nebula");

		MonoMethodDesc* desc = mono_method_desc_new(":Main()", false);
		MonoMethod* entryPoint = mono_method_desc_search_in_class(desc, cls);

		if (!entryPoint)
			n_error("Could not find entry point for Mono scripts!");

		auto ret = mono_runtime_invoke(entryPoint, NULL, NULL, NULL);

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



} // namespace Scripting