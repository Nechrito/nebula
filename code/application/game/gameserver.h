#pragma once
//------------------------------------------------------------------------------
/**
    @class Game::GameServer

    The game server setups and runs the game world.
	Functionality and queries on the game world are divided amongst
	several FeaturesUnits.
    This keeps the game server's interface small and clean, and lets applications
	easily extend functionality by implementing new, or deriving from existing
	game features.

    To add or replace FeatureUnit objects, derive from Game::FeatureUnit and 
    add your features on application start or gamestatehandler enter.

    The GameServer triggers all attached features. Start and Stop is called within 
    the gamestatehandler to allow all features do stuff after everything is loaded 
    and initialized. Load and Save is invoked from the BaseGameFeature which allows
    beginning a new game, load or save a game.
    
    (C) 2007 RadonLabs GmbH
	(C) 2013-2018 Individual contributors, see AUTHORS file
*/
#include "core/refcounted.h"
#include "core/ptr.h"
#include "core/singleton.h"
#include "game/featureunit.h"
#include "debug/debugtimer.h"

//------------------------------------------------------------------------------
namespace Game
{

class GameServer : public Core::RefCounted
{
    __DeclareClass(GameServer)
    __DeclareSingleton(GameServer)
public:
    /// constructor
    GameServer();
    /// destructor
    virtual ~GameServer();

    /// open the game world
    virtual bool Open();
    /// close the game world
    virtual void Close();
    /// start the game world
    virtual bool Start();
    /// has the game world already started
    bool HasStarted() const;
    /// stop the game world
    virtual void Stop();
	/// trigger actions before rendering the game world
	virtual void OnBeginFrame();
    /// trigger the game world
    virtual void OnFrame();
	/// trigger actions after rendering the game world
	virtual void OnEndFrame();

	/// call OnBeforeLoad on all game features
	virtual void NotifyBeforeLoad();
	/// call OnBeforeCleanup on all game features
	virtual void NotifyBeforeCleanup();
   
    /// call OnLoad on all game features
    virtual void NotifyGameLoad();
    /// call OnSave on all game features
    virtual void NotifyGameSave();
        
    /// add game feature
    void AttachGameFeature(const Ptr<FeatureUnit>& feature);
    /// remove game feature
    void RemoveGameFeature(const Ptr<FeatureUnit>& feature);
    /// is feature attached
    bool IsFeatureAttached(const Util::String& stringName) const;

	Util::Array<Ptr<FeatureUnit>> const& GetGameFeatures() const;

protected:
    bool isOpen;
    bool isStarted;
    Util::Array<Ptr<FeatureUnit> > gameFeatures;

	_declare_timer(GameServerOnBeginFrame)
    _declare_timer(GameServerOnFrame)
	_declare_timer(GameServerOnEndFrame)
};

}; // namespace Game
//------------------------------------------------------------------------------


