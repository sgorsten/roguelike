#include "common.h"
#include "map.h"

const Tile::Type Tile::types[NUM_TILES] = {
    {{Color::Black, ' '}, false, "void"},
    {{Color::Gray, '.'}, true, "dirt floor"},
    {{Color::Blue, 0xb2}, false, "wall"},
    {{Color::Blue, 0xb1}, false, "secret door"},
    {{Color::Brown, '+'}, false, "closed door"},
    {{Color::Brown, '/'}, true, "open door"}
};

BresenhamLine::const_iterator & BresenhamLine::const_iterator::operator ++ ()
{
    point += mainStep;
    error -= sideDelta;
    if(error < 0)
    {
        point += sideStep;
        error += mainDelta;
    }
    return *this;
}

BresenhamLine::BresenhamLine(int2 a, bool includeA, int2 b, bool includeB)
{
    int mainDelta = abs(b.x - a.x), sideDelta = abs(b.y - a.y);
    Direction mainStep = a.x < b.x ? Direction::East : Direction::West, sideStep = a.y < b.y ? Direction::South : Direction::North;
    if(abs(b.y - a.y) > abs(b.x - a.x))
    {
        std::swap(mainDelta, sideDelta);
        std::swap(mainStep, sideStep);
    }

    first = {a,mainStep,sideStep,mainDelta,sideDelta,mainDelta/2};
    last = {b,mainStep,sideStep,mainDelta,sideDelta,mainDelta/2};
    if(includeB) ++last;
    if(!includeA && first != last) ++first;
}

static bool CheckLineOfSight(const Map & map, const int2 & viewer, const int2 & target, bool isNeighbor)
{
    if(isNeighbor && !map.GetTile(target).IsWalkable()) return false;
    for(auto point : BresenhamLine(viewer, false, target, isNeighbor))
    {
        if(!map.GetTile(point).IsWalkable()) // For now, assume unwalkable tiles block visibility
        {
            return false;
        }
    }
    return true;
}

bool Map::HasLineOfSight(const int2 & viewer, const int2 & target) const
{
    return CheckLineOfSight(*this, viewer, target, false) 
        || CheckLineOfSight(*this, viewer, target + int2(1,0), true)
        || CheckLineOfSight(*this, viewer, target + int2(0,1), true)
        || CheckLineOfSight(*this, viewer, target - int2(1,0), true)
        || CheckLineOfSight(*this, viewer, target - int2(0,1), true)
        || CheckLineOfSight(*this, viewer, target + int2(-1,-1), true)
        || CheckLineOfSight(*this, viewer, target + int2(+1,-1), true)
        || CheckLineOfSight(*this, viewer, target + int2(+1,+1), true)
        || CheckLineOfSight(*this, viewer, target + int2(-1,+1), true);
}

///////////////////////////
// Procedural generation //
///////////////////////////

static std::vector<std::pair<int2,Direction>> EnumerateDoorCandidates(const Rect & room, const Rect & other)
{
    std::vector<std::pair<int2,Direction>> candidates;
    if(other.a.x > room.b.x) for(int y=room.a.y; y<room.b.y; y+=2) candidates.push_back({{room.b.x-1, y}, Direction::East});
    if(other.b.x < room.a.x) for(int y=room.a.y; y<room.b.y; y+=2) candidates.push_back({{room.a.x, y}, Direction::West});
    if(other.a.y > room.b.y) for(int x=room.a.x; x<room.b.x; x+=2) candidates.push_back({{x, room.b.y-1}, Direction::South});
    if(other.b.y < room.a.y) for(int x=room.a.x; x<room.b.x; x+=2) candidates.push_back({{x, room.a.y}, Direction::North});
    return candidates;
}

static std::vector<int2> EnumerateIntersections(const Map & map)
{
    std::vector<int2> intersections;
    for(int2 c={1,1}; c.y<MAP_HEIGHT; c.y+=2)
    {
        for(c.x=1; c.x<MAP_WIDTH; c.x+=2)
        {
            if(!map.GetTile(c).IsWalkable()) continue;
            if(map.GetTile(c+int2(-1,-1)).IsWalkable()) continue;
            if(map.GetTile(c+int2(+1,-1)).IsWalkable()) continue;
            if(map.GetTile(c+int2(+1,+1)).IsWalkable()) continue;
            if(map.GetTile(c+int2(-1,+1)).IsWalkable()) continue;
            int pathCount = 0;
            if(map.GetTile(c+int2(-1,0)).IsWalkable()) ++pathCount;
            if(map.GetTile(c+int2(+1,0)).IsWalkable()) ++pathCount;
            if(map.GetTile(c+int2(0,-1)).IsWalkable()) ++pathCount;
            if(map.GetTile(c+int2(0,+1)).IsWalkable()) ++pathCount;
            if(pathCount >= 3) intersections.push_back(c);
        }
    }
    return intersections;
}

static void CarveTunnel(Map & map, std::mt19937 & engine, const Rect & roomA, const Rect & roomB)
{
    auto doorsA = EnumerateDoorCandidates(roomA, roomB);
    auto doorsB = EnumerateDoorCandidates(roomB, roomA);
    auto doorA = doorsA[std::uniform_int_distribution<size_t>(0,doorsA.size()-1)(engine)];
    auto doorB = doorsB[std::uniform_int_distribution<size_t>(0,doorsB.size()-1)(engine)];
    auto pointA = doorA.first + int2(doorA.second) * 2;
    auto pointB = doorB.first + int2(doorB.second) * 2;
    auto delta = pointB - pointA, absDelta = delta.Abs() / 2;
    int2 stepMain = {delta.x > 0 ? 1 : -1, 0};
    int2 stepSide = {0, delta.y > 0 ? 1 : -1};
    if(absDelta.y > absDelta.x)
    {
        std::swap(absDelta.x, absDelta.y);
        std::swap(stepMain, stepSide);
    }
    int turn = std::uniform_int_distribution<int>(0, absDelta.x)(engine);
    auto point = pointA;
    map[point] = TILE_FLOOR;
    for(int i=0; i<turn; ++i)
    {
        point += stepMain;
        map[point] = TILE_FLOOR;
        point += stepMain;
        map[point] = TILE_FLOOR;
    }
    for(int i=0; i<absDelta.y; ++i)
    {
        point += stepSide;
        map[point] = TILE_FLOOR;
        point += stepSide;
        map[point] = TILE_FLOOR;
    }
    for(int i=turn; i<absDelta.x; ++i)
    {
        point += stepMain;
        map[point] = TILE_FLOOR;
        point += stepMain;
        map[point] = TILE_FLOOR;
    }
    map[doorA.first + doorA.second] = TILE_FLOOR;
    map[doorB.first + doorB.second] = TILE_FLOOR;
}

Map GenerateRandomMap(std::mt19937 & engine)
{
    const std::uniform_int_distribution<int> roomWidth(3,5);
    const std::uniform_int_distribution<int> roomHeight(2,4);
    const int2 places = int2(MAP_WIDTH-2, MAP_HEIGHT-2) / 2; // We can place rooms at even coordinates which do not touch either boundary

    // Place a bunch of rooms
    std::vector<Rect> rooms;
    for(int i=0; i<1000 && rooms.size() < 8; ++i)
    {
        int2 size = {roomWidth(engine), roomHeight(engine)};
        int2 place = {
            std::uniform_int_distribution<int>(0, places.x - size.x)(engine),
            std::uniform_int_distribution<int>(0, places.y - size.y)(engine)
        };
        Rect room = {place*2 + int2(1,1), (place+size)*2};
        Rect expandedRoom = {room.a-int2(2,2), room.b+int2(2,2)};
        bool good = true;
        for(auto & other : rooms)
        {
            if(expandedRoom.Intersects(other))
            {
                good = false;
            }
        }
        if(good) rooms.push_back(room);
    }

    // Fill with solid wall
    Map map;
    map.Fill({{0,0}, {MAP_WIDTH,MAP_HEIGHT}}, TILE_WALL); 

    // Carve rooms
    for(auto & room : rooms) map.Fill(room, TILE_FLOOR);

    // Carve tunnels
    for(int i=1; i<rooms.size(); ++i)
    {
        int j = std::uniform_int_distribution<int>(0, i-1)(engine);
        CarveTunnel(map, engine, rooms[i], rooms[j]);
    }

    // Find intersections, and place some hidden doors
    for(auto point : EnumerateIntersections(map))
    {
        if(std::uniform_real_distribution<float>()(engine) < 0.2f)
        {
            static const Direction directions[] = {Direction::North, Direction::East, Direction::South, Direction::West};
            while(true)
            {
                auto dir = directions[std::uniform_int_distribution<int>(0,3)(engine)];
                if(map[point+dir] != TILE_WALL)
                {
                    map[point+dir] = TILE_HIDDEN_DOOR;

                    auto lastPoint = point+dir;
                    auto curPoint = lastPoint+dir;
                    while(true)
                    {
                        int neighbors = 0;
                        for(int i=0; i<4; ++i) if(map.GetTile(curPoint + directions[i]).IsWalkable() && curPoint + directions[i] != lastPoint) ++neighbors;
                        if(neighbors >= 2)
                        {
                            map[lastPoint] = TILE_HIDDEN_DOOR;
                            break;
                        }
                        for(int i=0; i<4; ++i)
                        {
                            auto newPoint = curPoint + directions[i];
                            if(map.GetTile(newPoint).IsWalkable() && newPoint != lastPoint)
                            {
                                lastPoint = curPoint;
                                curPoint = newPoint;
                                break;
                            }
                        }
                    }

                    break;
                }
            }
        }
    }

    // Place doors on rooms
    for(auto & room : rooms)
    {
        for(int x=room.a.x; x<room.b.x; x+=2)
        {
            if(map.GetTile({x,room.a.y-1}).IsWalkable()) map[{x,room.a.y-1}] = TILE_CLOSED_DOOR;
            if(map.GetTile({x,room.b.y}).IsWalkable()) map[{x,room.b.y}] = TILE_CLOSED_DOOR;
        }

        for(int y=room.a.y; y<room.b.y; y+=2)
        {
            if(map.GetTile({room.a.x-1,y}).IsWalkable()) map[{room.a.x-1,y}] = TILE_CLOSED_DOOR;
            if(map.GetTile({room.b.x,y}).IsWalkable()) map[{room.b.x,y}] = TILE_CLOSED_DOOR;
        }
    }

    return map;
}