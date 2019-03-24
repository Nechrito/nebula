//------------------------------------------------------------------------------
//  monobindings.cc
//  (C) 2019 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "foundation/stdneb.h"
#include "core/debug.h"

#include "monobindings.h"
namespace Mono
{

//------------------------------------------------------------------------------
/**
*/
void
N_Print(const char * string, int32_t is_stdout)
{
	n_printf(string);
}

//------------------------------------------------------------------------------
/**
*/
void
N_Error(const char * string, int32_t is_stdout)
{
	n_error(string);
}

//------------------------------------------------------------------------------
/**
*/
void
N_Log(const char * log_domain, const char * log_level, const char * message, int32_t fatal, void * user_data)
{
	if (fatal)
	{
		n_error(message);
	}
	else
	{
		n_warning(message);
	}
}


} // namespace Mono
