#include "common.h"
#include "game.h"
#include "player.h"

Actor * Game::GetActor(const int2 & position)
{
    for(auto & actor : actors)
    {
        if(actor.position == position)
        {
            return &actor;
        }
    }
    return nullptr;
}

int2 Game::SelectRandomLocation()
{
    std::uniform_int_distribution<int> x(0, MAP_WIDTH-1);
    std::uniform_int_distribution<int> y(0, MAP_HEIGHT-1);
    while(true)
    {
        int2 location = {x(engine), y(engine)};
        if(map.GetTile(location).IsWalkable()) return location;
    }
}

void Game::SpawnPlayer(const Race & race, const int2 & position)
{
    playerBrain = std::make_shared<PlayerBrain>(messages);
    Spawn(race, position, playerBrain);
}

void Game::Spawn(const Race & race, const int2 & position, std::shared_ptr<Brain> brain)
{
    Actor actor;
    actor.race = &race;
    actor.gender = race.isGendered ? (Dice{1,2,0}.Roll(engine) == 1 ? Gender::Female : Gender::Male) : Gender::Neuter;
    actor.hitPoints = race.hitPoints;
    actor.position = position;
    actor.brain = move(brain);
    actor.delay = 0;
    actor.isDead = false;
    actors.push_back(actor);
}

void Game::Tick()
{
    // Select an actor to act during this tick
    Actor * tickActor = nullptr;
    for(auto & actor : actors)
    {
        if(!tickActor || actor.delay < tickActor->delay)
        {
            tickActor = &actor;
        }
    }
    assert(tickActor);

    // Allow the actor to act
    auto action = tickActor->Think(*this);
    tickActor->delay += action.GetCost();
    action.Execute(*this, *tickActor);

    // Erase dead actors
    for(auto it = begin(actors); it != end(actors); )
    {
        if(it->isDead)
        {
            if(it->brain == playerBrain) quit = true; // NOTE: Should handle this more gracefully
            it = actors.erase(it);
        }
        else ++it;
    }
}