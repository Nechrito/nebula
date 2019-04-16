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
#include "ids/id.h"
#include "io/uri.h"
#include "util/arrayallocator.h"
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include "util/dictionary.h"

//------------------------------------------------------------------------------
namespace Scripting
{

ID_16_TYPE(MonoAssemblyId);

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
	MonoAssemblyId Load(IO::URI const& uri);
	/// Execute function in an assembly. This is not very efficient and should not be used in place of mono delegates
	void Exec(MonoAssemblyId assembly, Util::String const& function);
	/// Check if mono server is open
	bool const IsOpen();
private:
	MonoDomain* domain;

	Util::ArrayAllocator<MonoAssembly*> assemblies;
	Util::Dictionary<Util::String, uint32_t> assemblyTable;

	bool waitForDebugger;

	bool isOpen;
};

} // namespace Scripting
//------------------------------------------------------------------------------
