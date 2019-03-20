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

using namespace IO;

namespace Scripting
{
__ImplementClass(Scripting::MonoServer, 'MONO', Scripting::ScriptServer);
__ImplementSingleton(Scripting::MonoServer);

using namespace Util;
using namespace IO;

#define __EXPORT extern "C" __declspec(dllexport)

__EXPORT void Foobar()
{
	n_printf("Testing");
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
		this->domain = mono_jit_init("Nebula Scripting Subsystem");
		if (!domain)
		{
			n_error("Failed to initialize Mono JIT runtime!");
		}
		
		IO::URI uri = IO::URI("scr:test.dll");
		Util::String path = uri.AsString();

		// setup executable
		MonoAssembly *assembly;
		assembly = mono_domain_assembly_open(domain, path.AsCharPtr());
		if (!assembly)
			n_error("Error loading mono assembly!");

		char* argc[1] = { "test.dll" };

		int retval = mono_jit_exec(domain, assembly, 1, argc);

		n_printf("mono returned %i\n", retval);

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