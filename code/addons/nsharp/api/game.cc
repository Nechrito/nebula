//------------------------------------------------------------------------------
//  game.cc
//  (C) 2019 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "stdneb.h"
#include "game.h"
#include "basegamefeature/managers/entitymanager.h"
#include "basegamefeature/components/transformcomponent.h"

namespace Scripting
{

namespace Api
{

//------------------------------------------------------------------------------
/**
*/
bool
IsEntityValid(unsigned int entity)
{
	return Game::EntityManager::Instance()->IsAlive(entity);
}

//------------------------------------------------------------------------------
/**
*/
unsigned int
CreateEntity()
{
	return Game::EntityManager::Instance()->NewEntity().id;
}

//------------------------------------------------------------------------------
/**
*/
void
DeleteEntity(unsigned int entity)
{
	Game::EntityManager::Instance()->DeleteEntity(entity);
}

} // namespace Api

} // namespace Base
