#pragma once
//------------------------------------------------------------------------------
/**
	Brute force system

	(C) 2018 Individual contributors, see AUTHORS file
*/
//------------------------------------------------------------------------------
#include "visibilitysystem.h"
#include "jobs/jobs.h"
namespace Visibility
{

extern void BruteforceSystemJobFunc(const Jobs::JobFuncContext& ctx);

class BruteforceSystem : public VisibilitySystem
{
private:
	friend class ObserverContext;

	/// setup from load info
	void Setup(const BruteforceSystemLoadInfo& info);

	/// run system
	void Run();
};

} // namespace Visibility
