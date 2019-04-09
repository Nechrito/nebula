#pragma once
//------------------------------------------------------------------------------
/**
    @class Scripting::MonoServer
  
    Mono backend for the Nebula scripting subsystem.
        
    (C) 2019 Individual contributors, see AUTHORS file
*/
#include "core/refcounted.h"
#include "core/singleton.h"
#include "util/string.h"
#include "io/uri.h"
#include "util/arrayallocator.h"
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>

//------------------------------------------------------------------------------
namespace Scripting
{
class MonoServer : public Core::RefCounted
{
    __DeclareClass(MonoServer);
    __DeclareSingleton(MonoServer);
public:
    /// constructor
	MonoServer();
    /// destructor
    virtual ~MonoServer();
    /// open the script server
    bool Open();
    /// close the script server
    void Close();
    /// enable debugging. this needs to be called before Open()
	void SetDebuggingEnabled(bool enabled);
	/// Load mono exe or DLL at path
	void Load(IO::URI const& uri);
	/// Check if mono server is open
	bool const IsOpen();
private:
	MonoDomain* domain;

	Util::ArrayAllocator<MonoAssembly*> assemblies;
	Util::Dictionary<IO::URI, uint32_t> assemblyTable;

	bool waitForDebugger;

	bool isOpen;
};

} // namespace Scripting
//------------------------------------------------------------------------------
