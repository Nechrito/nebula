#pragma once
//------------------------------------------------------------------------------
/**
    @class Scripting::MonoServer
  
    Mono backend for the Nebula scripting subsystem.
        
    (C) 2019 Individual contributors, see AUTHORS file
*/
#include "scripting/scriptserver.h"
#include "util/string.h"
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>

//------------------------------------------------------------------------------
namespace Scripting
{
class MonoServer : public ScriptServer
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
    /// evaluate a script statement in a string
    bool Eval(const Util::String& str);	
    /// evaluate script in file
    bool EvalFile(const IO::URI& file);
	/// enable debugging. this needs to be called before Open()
	void SetDebuggingEnabled(bool enabled);
private:
	MonoDomain* domain;

	bool waitForDebugger;
};

} // namespace Scripting
//------------------------------------------------------------------------------
