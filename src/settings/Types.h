#pragma once
#include <string>
#include <cstring>
#include <unordered_map>
#include <chrono>
#include <cstdio>
#include <cctype>
#include "dgb/Spoof.h"
#include "dgb/lazy.hpp"
#include "menu/string_encryption.hpp"
#include "game/StringHash.hpp"
#include <algorithm>


inline std::string NormalizeBlueprintName(const std::string& rawName);

struct ContainerInfo {
    const char* displayName;
    int typeIndex;
};

namespace SceneTypes {

    namespace Salvage {
        enum Type : int {
            Agave = 0,
            Apricot,
            Fertilizer,
            GreatMullein,
            Lemon,
            MossRock,
            Mushroom,
            MushroomCluster,
            Olive,
            COUNT
        };

        inline const char* Labels[] = {
            "Agave",
            "Apricot",
            "Fertilizer",
            "Great Mullein",
            "Lemon",
            "Moss Rock",
            "Mushroom",
            "Mushroom Cluster",
            "Olive"
        };

        inline const std::unordered_map<uint32_t, ContainerInfo>& GetSalvageContainerHashMap() {
            static const std::unordered_map<uint32_t, ContainerInfo> table = {
                { 0x10e8a63fu,           {"Agave", Agave}},
                { 0x03d6258du,          {"Apricot", Apricot}},
                { 0x000ea3dbu,       {"Fertilizer", Fertilizer}},
                { 0x4ab4dea4u,     {"Great Mullein", GreatMullein}},
                { 0x11aecd36u,            {"Lemon", Lemon}},
                { 0x3f7eb5ebu,        {"Moss Rock", MossRock}},
                { 0x14df4b15u,        {"Mushroom", Mushroom}},
                { 0xd8b4a957u, {"Mushroom Cluster", MushroomCluster}},
                { 0x11e8dd3au,            {"Olive", Olive}}
            };
            return table;
        }

        inline const ContainerInfo* Get(const std::string& name) {
            std::string normalized = NormalizeBlueprintName(name);
            const auto& map = GetSalvageContainerHashMap();
            auto it = map.find(TagDigest::Digest(normalized));
            return (it != map.end()) ? &it->second : nullptr;
        }
    }

    namespace Container {
        enum Type : int {
            AmmoBox = 0,
            AndroidContainer,
            AntennaSet,
            Backpack,
            Bus,
            Cabinet,
            Car,
            CartShelfDrawer,
            CenterBench,
            Closet,
            ComsConsole,
            Computer,
            Console,
            Container,
            Crate,
            CupboardDrawer,
            Desk,
            DeskDrawer,
            DrawerShelf,
            Drawers,
            Dumpster,
            ElectricalCabinet,
            Freezer,
            Fridge,
            FruitBasket,
            FuseBox,
            Generator,
            GrenadeBox,
            InformationTerminal,
            KitchenBench,
            KitchenCupboard,
            Locker,
            Machine,
            MachinePanel,
            MedicalBag,
            MedicalDrawer,
            MedicalLocker,
            MuseumCrate,
            Refrigerator,
            RocketParts,
            Rover,
            SecurityCamera,
            SeedBox,
            SeedConsole,
            Server,
            ShelfDrawer,
            SolarPanelHatch,
            StreetSweeper,
            Suitcase,
            SupportStructure,
            ToolTrolley,
            TrailerCompressor,
            TrashBin,
            Trashcan,
            TruckScanner,
            TVUnit,
            UtilityTruck,
            WallCabinet,
            WallPanel,
            WasabiPanel,
            WeaponCase,
            Workbench,
            COUNT
        };

        inline const char* Labels[] = {
            "Ammo Box",
            "Android Container",
            "Antenna Set",
            "Backpack",
            "Bus",
            "Cabinet",
            "Car",
            "Cart Shelf Drawer",
            "Center Bench",
            "Closet",
            "Coms Console",
            "Computer",
            "Console",
            "Container",
            "Crate",
            "Cupboard Drawer",
            "Desk",
            "Desk Drawer",
            "Drawer Shelf",
            "Drawers",
            "Dumpster",
            "Electrical Cabinet",
            "Freezer",
            "Fridge",
            "Fruit Basket",
            "Fuse Box",
            "Generator",
            "Grenade Box",
            "Information Terminal",
            "Kitchen Bench",
            "Kitchen Cupboard",
            "Locker",
            "Machine",
            "Machine Panel",
            "Medical Bag",
            "Medical Drawer",
            "Medical Locker",
            "Museum Crate",
            "Refrigerator",
            "Rocket Parts",
            "Rover",
            "Security Camera",
            "Seed Box",
            "Seed Console",
            "Server",
            "Shelf Drawer",
            "Solar Panel Hatch",
            "Street Sweeper",
            "Suitcase",
            "Support Structure",
            "Tool Trolley",
            "Trailer Compressor",
            "Trash Bin",
            "Trashcan",
            "Truck Scanner",
            "TV Unit",
            "Utility Truck",
            "Wall Cabinet",
            "Wall Panel",
            "Wasabi Panel",
            "Weapon Case",
            "Workbench"
        };

        inline const std::unordered_map<uint32_t, ContainerInfo>& GetContainerHashMap() {
            static const std::unordered_map<uint32_t, ContainerInfo> table = {
        { 0x1283c8e6u,     {"Ammo Box", AmmoBox}},
        { 0xc28d427au, {"Ammo Box (High Tier)", AmmoBox}},
        { 0x6405229au,                     {"Android Container (A)", AndroidContainer}},
        { 0x6405229bu,                    {"Android Container (B)", AndroidContainer}},
        { 0xc3840f2au,              {"Antenna Set", AntennaSet}},
        { 0xfd6054cbu,              {"Backpack", Backpack}},
        { 0xb4318e63u,                          {"Bus", Bus}},
        { 0xb4318e64u,                          {"Bus", Bus}},
        { 0xba7f6519u,                    {"Cabinet (M)", Cabinet}},
        { 0x65ec0f9fu,                   {"Cabinet (S)", Cabinet}},
        { 0x96ce4959u,               {"Cabinet (Red)", Cabinet}},
        { 0x5cd0a3a7u,               {"Cabinet (Blue)", Cabinet}},
        { 0x08ab617bu,            {"Cabinet (Yellow)", Cabinet}},
        { 0x827a7be1u,             {"Cabinet (Clean)", Cabinet}},
        { 0x2ae939efu,                      {"Car", Car}},
        { 0x2ae939f0u,                     {"Car", Car}},
        { 0x2ae939f1u,                      {"Car", Car}},
        { 0x2ae939f2u,                     {"Car", Car}},
        { 0x2ef3cb10u,                      {"Car", Car}},
        { 0x2ef3cb11u,                     {"Car", Car}},
        { 0x2ef3cb12u,                      {"Car", Car}},
        { 0xb91b4b99u,                            {"Car", Car}},
        { 0x2f8c8823u,       {"Cart Shelf Drawer", CartShelfDrawer}},
        { 0x2f8c8824u,        {"Cart Shelf Drawer", CartShelfDrawer}},
        { 0x019c4765u,  {"Cart Shelf Drawer (Clean)", CartShelfDrawer}},
        { 0x4e968426u,  {"Cart Shelf Drawer (Clean)", CartShelfDrawer}},
        { 0x347d1d1eu,                {"Center Bench", CenterBench}},
        { 0x624d9dccu,             {"Closet", Closet}},
        { 0x8aaf7d6du,               {"Closet", Closet}},
        { 0x8aaf81aeu,               {"Closet", Closet}},
        { 0xa39646a6u,                    {"Coms Console", ComsConsole}},
        { 0x5ce97ee0u,                    {"Computer", Computer}},
        { 0x6d295a62u,                       {"Computer", Computer}},
        { 0x3ba38ce4u,                {"Computer (Clean)", Computer}},
        { 0xb259e260u,                    {"Computer", Computer}},
        { 0xd35dde16u,                        {"Console", Console}},
        { 0xca6cb9a1u,                {"Container (Large)", Container}},
        { 0x4823d1fcu,                         {"Crate", Crate}},
        { 0x4823d1fdu,                         {"Crate", Crate}},
        { 0xff8fe999u,                       {"Crate", Crate}},
        { 0xb2785d65u,      {"Cupboard Drawer", CupboardDrawer}},
        { 0xa4a6c17fu,                     {"Desk (Blue)", Desk}},
        { 0xddaa2ab8u,                          {"Desk", Desk}},
        { 0x7ace1083u,            {"Desk (SpaceTravelCo)", Desk}},
        { 0x2c3baf27u,                 {"Desk Drawer (Large)", DeskDrawer}},
        { 0x2cc0a795u,                 {"Desk Drawer (Small)", DeskDrawer}},
        { 0xda1d6d67u,           {"Desk Drawer (Large White)", DeskDrawer}},
        { 0xcc49acc0u,                 {"Drawer Shelf", DrawerShelf}},
        { 0x029d44c9u,                      {"Drawers", Drawers}},
        { 0xb5ad6436u,                   {"Dumpster", Dumpster}},
        { 0x07442345u,          {"Electrical Cabinet", ElectricalCabinet}},
        { 0x42983407u,     {"Electrical Cabinet (Clean)", ElectricalCabinet}},
        { 0x2353c10du,              {"Freezer", Freezer}},
        { 0xb0978dd6u,              {"Fridge", Fridge}},
        { 0x826066bfu,        {"Fruit Basket", FruitBasket}},
        { 0xc779237du,                         {"Fuse Box", FuseBox}},
        { 0x6ab5010du,                {"Generator", Generator}},
        { 0x6ab5010eu,                {"Generator", Generator}},
        { 0x49420cacu, {"Grenade Box", GrenadeBox}},
        { 0x15b7939bu,     {"Information Terminal", InformationTerminal}},
        { 0xe882241fu,        {"Kitchen Bench", KitchenBench}},
        { 0x3f3fc8d6u, {"Kitchen Bench", KitchenBench}},
        { 0xead88ad4u,   {"Kitchen Bench", KitchenBench}},
        { 0xead88ad5u,   {"Kitchen Bench", KitchenBench}},
        { 0xc4f22356u, {"Kitchen Bench", KitchenBench}},
        { 0x03d0855du, {"Kitchen Cupboard", KitchenCupboard}},
        { 0xf6196573u, {"Kitchen Cupboard", KitchenCupboard}},
        { 0x4321d6eau,                    {"Locker", Locker}},
        { 0xf83adfb9u, {"Locker (SpaceTravelCo)", Locker}},
        { 0xb86bdcf1u,              {"Locker (Blue)", Locker}},
        { 0xc644fc3au,              {"Locker (Green)", Locker}},
        { 0x564cde35u,      {"Locker (School)", Locker}},
        { 0x5ffb666au,                     {"Locker", Locker}},
        { 0x6f87b255u,                        {"Locker", Locker}},
        { 0x6f87b256u,                        {"Locker", Locker}},
        { 0x8055ae78u,                  {"Machine", Machine}},
        { 0x165a28d2u,                  {"Machine", Machine}},
        { 0x9c3102d1u,            {"Machine (Left Door)", Machine}},
        { 0x22c024a4u,          {"Machine (Right Door)", Machine}},
        { 0x7b8eeb77u,      {"Machine (Left Door Snow)", Machine}},
        { 0x59eb292au,     {"Machine (Right Door Snow)", Machine}},
        { 0x86bc254du,       {"Machine Panel (Large)", MachinePanel}},
        { 0xd276dbafu,      {"Machine Panel (Large)", MachinePanel}},
        { 0xce3b1dfau, {"Machine Panel (SpaceTravelCo)", MachinePanel}},
        { 0xe6ddc0bfu, {"Machine Panel (Green)", MachinePanel}},
        { 0xb8c2d3adu, {"Machine Panel (Small Green)", MachinePanel}},
        { 0x8d37e5abu, {"Medical Bag", MedicalBag}},
        { 0x941bb49eu,              {"Medical Drawer", MedicalDrawer}},
        { 0x5a09ca59u,               {"Medical Locker", MedicalLocker}},
        { 0x931db16fu,                {"Museum Crate", MuseumCrate}},
        { 0x931db5b0u,                 {"Museum Crate", MuseumCrate}},
        { 0xf22182aeu,        {"Refrigerator", Refrigerator}},
        { 0x1995328bu,                 {"Rocket Parts (A)", RocketParts}},
        { 0x19a74b0cu,                 {"Rocket Parts (B)", RocketParts}},
        { 0x19b9638du,                 {"Rocket Parts (C)", RocketParts}},
        { 0xd08c32efu,                             {"Rover", Rover}},
        { 0x80c319cbu,              {"Security Camera", SecurityCamera}},
        { 0xab2df5f6u,                     {"Seed Box", SeedBox}},
        { 0xf0deed00u,                 {"Seed Console", SeedConsole}},
        { 0xd6b7c077u,          {"Server", Server}},
        { 0x1c5b4573u,   {"Server (Black)", Server}},
        { 0x1cb91b87u,    {"Server (Green)", Server}},
        { 0xf536a631u,       {"Server (Red)", Server}},
        { 0x1c6d6eb9u,    {"Server (Clean)", Server}},
        { 0x8b321c68u,    {"Server (MCP)", Server}},
        { 0x9c0b6c83u,     {"Server (Red)", Server}},
        { 0x6301c2fcu,                {"Shelf Drawer", ShelfDrawer}},
        { 0x3508f69du,                 {"Solar Panel Hatch", SolarPanelHatch}},
        { 0xee8a97ebu,               {"Street Sweeper", StreetSweeper}},
        { 0x94f4b363u,               {"Suitcase", Suitcase}},
        { 0xfba02a07u,            {"Support Structure", SupportStructure}},
        { 0x19d882cdu,               {"Tool Trolley", ToolTrolley}},
        { 0x24886eb4u,          {"Tool Trolley (Blue)", ToolTrolley}},
        { 0x16343047u,     {"Tool Trolley (Yellow MCP)", ToolTrolley}},
        { 0xf474ab5du,        {"Trailer Compressor", TrailerCompressor}},
        { 0x340eb74cu,                   {"Trash Bin", TrashBin}},
        { 0x695eb2bdu,                  {"Trash Bin", TrashBin}},
        { 0x2dba2a4du,           {"Trash Bin (Green)", TrashBin}},
        { 0x0e1505b8u,          {"Trash Bin (Yellow)", TrashBin}},
        { 0xc63b1cb5u,                      {"Trashcan", Trashcan}},
        { 0x69ef42acu,                {"Truck Scanner", TruckScanner}},
        { 0x69ef42adu,                 {"Truck Scanner", TruckScanner}},
        { 0x3211cf6cu,                {"TV Unit", TVUnit}},
        { 0x3797c3deu,                      {"Utility Truck", UtilityTruck}},
        { 0x1ea731b3u,              {"Wall Cabinet", WallCabinet}},
        { 0x1ea731b4u,             {"Wall Cabinet", WallCabinet}},
        { 0x26bc53f5u,             {"Wall Cabinet", WallCabinet}},
        { 0x5f61504fu,       {"Wall Panel (Large)", WallPanel}},
        { 0x17475029u,        {"Wall Panel (Main)", WallPanel}},
        { 0x1d957237u,                  {"Wall Panel (Medium)", WallPanel}},
        { 0x3890c176u,       {"Wall Panel (Small Large)", WallPanel}},
        { 0x3915b9e4u,        {"Wall Panel (Small)", WallPanel}},
        { 0x4ec80268u,                         {"Wasabi Panel", WasabiPanel}},
        { 0x6c5a69f9u, {"Weapon Case", WeaponCase}},
        { 0x872a0619u, {"Weapon Case (High Tier)", WeaponCase}},
        { 0x00261ce4u,                       {"Workbench", Workbench}},
        { 0x00262125u,                      {"Workbench", Workbench}}
            };
            return table;
        }

        inline const ContainerInfo* Get(const std::string& name) {
            std::string normalized = NormalizeBlueprintName(name);
            const auto& map = GetContainerHashMap();
            auto it = map.find(TagDigest::Digest(normalized));
            return (it != map.end()) ? &it->second : nullptr;
        }
    }
}

struct ArcInfoResolved
{
    int index;
    const char* display;
};

static std::string ArcNameToLower(const std::string& raw)
{
    std::string lower = raw;
    for (char& c : lower)
        c = (char)tolower((unsigned char)c);
    return lower;
}

// Heuristic for renamed / elite / BP-wrapped ARC pawns not in the hard list.
static bool LooksLikeArcEnemy(const std::string& lower)
{
    if (lower.find("pioneer") != std::string::npos)
        return false;
    if (lower.find("pickup") != std::string::npos)
        return false;
    if (lower.find("container") != std::string::npos)
        return false;
    if (lower.find("projectile") != std::string::npos)
        return false;
    if (lower.find("camera") != std::string::npos)
        return false;

    static const char* kHints[] = {
        "lightdrone", "heavydrone", "elitedrone", "tickbot", "turretenemy",
        "chonk", "rollbot", "bullcrab", "pinger", "snitchbot", "c_runner",
        "peppermint", "spearmint", "c_sniper", "bp_extractor", "arcassessor",
        "enemy_arc", "arc_enemy", "arcpawn", "c_wasp", "c_hornet",
    };
    for (const char* h : kHints) {
        if (lower.find(h) != std::string::npos)
            return true;
    }
    return false;
}

static ArcInfoResolved ResolveArcInfo(const std::string& raw)
{
    struct ArcEntry { const char* pattern; int index; const char* display; };
    // Longer / more specific patterns first. Matching is case-insensitive.
    static const ArcEntry entries[] = {
        { "c_lightdrone_elite",           1,  "Hornet"        },
        { "lightdrone_elite",             1,  "Hornet"        },
        { "c_lightdrone",                 0,  "Wasp"          },
        { "lightdrone",                   0,  "Wasp"          },
        { "c_tickbot",                    2,  "Tick"          },
        { "tickbot",                      2,  "Tick"          },
        { "c_turretenemy",                3,  "Turret"        },
        { "turretenemy",                  3,  "Turret"        },
        { "c_chonkplatform_mortar",      10,  "Bombardier"    },
        { "chonkplatform_mortar",        10,  "Bombardier"    },
        { "bombardier",                  10,  "Bombardier"    },
        { "c_chonk",                      4,  "Bastion"       },
        { "bastion",                      4,  "Bastion"       },
        { "c_heavydrone_laser",          19,  "Vaporizer"     },
        { "heavydrone_laser",            19,  "Vaporizer"     },
        { "vaporizer",                   19,  "Vaporizer"     },
        { "c_heavydrone_missile",         5,  "Rocketeer"     },
        { "heavydrone_missile",           5,  "Rocketeer"     },
        { "rocketeer",                    5,  "Rocketeer"     },
        { "c_heavydrone",                 5,  "Rocketeer"     },
        { "c_rollbot_flamethrower",       6,  "Fireball"      },
        { "rollbot_flamethrower",         6,  "Fireball"      },
        { "c_rollbot_pop",                7,  "Pop"           },
        { "rollbot_pop",                  7,  "Pop"           },
        { "c_rollbot_boom",              18,  "Comet"         },
        { "rollbot_boom",                18,  "Comet"         },
        { "c_rollbot_01_blockout",       11,  "Surveyor"      },
        { "rollbot_01_blockout",         11,  "Surveyor"      },
        { "surveyor",                    11,  "Surveyor"      },
        { "c_bullcrab",                   8,  "Leaper"        },
        { "bullcrab",                     8,  "Leaper"        },
        { "leaper",                       8,  "Leaper"        },
        { "c_pinger",                     9,  "Spotter"       },
        { "pinger",                       9,  "Spotter"       },
        { "spotter",                      9,  "Spotter"       },
        { "c_snitchbot",                 12,  "Snitch"        },
        { "snitchbot",                   12,  "Snitch"        },
        { "c_runner",                    13,  "Shredder"      },
        { "shredder",                    13,  "Shredder"      },
        { "c_peppermint",                14,  "The Queen"     },
        { "peppermint",                  14,  "The Queen"     },
        { "c_sniper",                    15,  "Sentinel"      },
        { "sentinel",                    15,  "Sentinel"      },
        { "c_spearmint",                 16,  "Matriarch"     },
        { "spearmint",                   16,  "Matriarch"     },
        { "matriarch",                   16,  "Matriarch"     },
        { "c_elitedrone_flamethrower",   17,  "Firefly"       },
        { "elitedrone_flamethrower",     17,  "Firefly"       },
        { "firefly",                     17,  "Firefly"       },
        { "bp_extractor",                20,  "Arc Assessor"  },
        { "arcassessor",                 20,  "Arc Assessor"  },
        { "arc_assessor",                20,  "Arc Assessor"  },
    };

    const std::string lower = ArcNameToLower(raw);
    for (const auto& e : entries) {
        if (lower.find(e.pattern) != std::string::npos)
            return { e.index, e.display };
    }

    // Keep unknown ARC-like pawns in ESP instead of dropping them.
    if (LooksLikeArcEnemy(lower))
        return { -2, "ARC" };

    return { -1, "" };
}


inline const char* ExtractionCategoryToString(uint8_t c)
{
    switch (c)
    {
    case 0x0:  return "Dormant";
    case 0x1:  return "Disabled";
    case 0x2:  return "Closed";
    case 0x3:  return "Closing";
    case 0x4:  return "Requested";
    case 0x5:  return "Arriving";
    case 0x6:  return "Ready";
    case 0x7:  return "EExtractionState_MAX";
    default:   return "Unknown";
    }
}


inline const char* ItemCategoryToString(uint8_t c)
{
    switch (c)
    {
    case 0x0:  return "Launcher";
    case 0x1:  return "Rifle";
    case 0x2:  return "Consumable";
    case 0x3:  return "LauncherPayload";
    case 0x4:  return "LauncherUniversalPayload";
    case 0x5:  return "Rifle Ammo";
    case 0x6:  return "Throwable";
    case 0x7:  return "Deployable";
    case 0x8:  return "Modification";
    case 0x9:  return "Gadget";
    case 0xA:  return "Armor";
    case 0xB:  return "CraftingReagent";
    case 0xC:  return "Backpack";
    case 0xD:  return "CraftingPart";
    case 0xE:  return "Misc";
    case 0xF:  return "MeleeWeapon";
    case 0x10: return "Generator";
    case 0x11: return "Currency";
    case 0x12: return "Recipe";
    case 0x13: return "MetaItem";
    case 0x14: return "Unarmed";
    case 0x15: return "Augment";
    default:   return "Unknown";
    }
}

inline std::string NormalizeBlueprintName(const std::string& rawName)
{
    std::string name = rawName;

    if (name.size() > 2 && name.substr(name.size() - 2) == "_C")
        name = name.substr(0, name.size() - 2);

    size_t tierPos = name.find("_T");
    if (tierPos != std::string::npos && tierPos + 2 < name.size() && isdigit(name[tierPos + 2]))
        name = name.substr(0, tierPos);

    return name;
}



inline const std::unordered_map<uint32_t, const char*>& GetWeaponNameHashMap()
{
    static const std::unordered_map<uint32_t, const char*> table = {
        { 0x6c8454f3u, "Burletta" },
        { 0xebf09e1fu, "Venator" },
        { 0x88f0b319u, "Hairpin" },
        { 0xf92f784fu, "Anvil" },
        { 0x3be9266cu, "Rattler" },
        { 0x5723f6acu, "Kettle" },
        { 0x3853cf76u, "Arpeggio" },
        { 0xf080394au, "Tempest" },
        { 0xcacc5563u, "Bettina" },
        { 0xf0fa1301u, "Rattler" },
        { 0x910e3d48u, "Ferro" },
        { 0x6d32ddc1u, "Renegade" },
        { 0x5ae1023fu, "Aphelion" },
        { 0x403c1a09u, "Osprey" },
        { 0x87e2016au, "Jupiter" },
        { 0x540b6effu, "Equalizer" },
        { 0xddc20a3fu, "Il Toro" },
        { 0xceb38946u, "Vulcano" },
        { 0x865cbec7u, "Dolabra" },
        { 0x53657458u, "Torrente" },
        { 0x4b74e0bfu, "Bobcat" },
        { 0x7efeb6dfu, "Canto" },
        { 0x037fc424u, "Stitcher" },
        { 0x4225216bu, "Hullcracker" },
        { 0xa86fd0f1u, "Raider Tool" },
        { 0x60b486ebu, "Seeker Grenade" },
        { 0x693dc4e8u, "Shield" },
        { 0x18382671u, "SnapHook" },
        { 0xbbaaff72u, "Herbal Bandage" },
        { 0x11f1adc0u, "Defibrillator" },
        { 0x1bde929eu, "ARC Powershell" },
        { 0x40801fa0u, "Sterized Bandage" },
        { 0x32a590b7u, "Deadline" },
        { 0xe748565fu, "Wolfpack" },
        { 0xb731dae0u, "Heavy Fuze Grenade" },
        { 0x6f006e78u, "Bandage" },
        { 0x18352df2u, "Vita Shot" },
        { 0x7e687d72u, "Smoke Grenade" },
        { 0x05a4da5du, "ShowStopper" },
        { 0xcc0338e1u, "Light Impact Grenade" },
        { 0x238aa336u, "Triggers 'Nade" },
        { 0xd0d82b78u, "ZipLine" },
        { 0xb54a1ddbu, "Barricade Kit" },
        { 0x881bc4d1u, "Leaper Pulse Unit" },
        { 0xd58fbd13u, "PhotoElectric Cloak" },
        { 0xdb5ae3b3u, "Vita Spray" },
        { 0x91dbc63au, "Agave" },
        { 0x5cefc3eau, "Agave Juice" },
        { 0x8e113048u, "Apricot" },
        { 0x8e7d15eeu, "Fruit Mix" },
        { 0x92a1ed31u, "Lemon" },
        { 0xd5e69818u, "Moss" },
        { 0xe67bad30u, "Mushroom" },
        { 0xee5ba448u, "Olives" },
        { 0x30335e9cu, "Prickly Pear" },
        { 0x930e98f7u, "Resin" },
        { 0x8fd64233u, "Volcanic Rock" },
        { 0xc8912489u, "Adrenaline Shot" },
        { 0x766f115du, "Armor Pack" },
        { 0xe32efcecu, "Sticky Grenade" },
        { 0x7b386ad8u, "Binoculars" },
        { 0x2aa22700u, "Jump Mine" },
        { 0xa2d051bdu, "Sound Trap" },
        { 0xd8b45f9au, "Corrosive Grenade" },
        { 0x88eb0773u, "Door Blocker" },
        { 0x03c43540u, "Flute" },
        { 0x3fc555f0u, "Instrument" },
        { 0x26aa2dd6u, "Integrated Binoculars" },
        { 0x35030315u, "Shield Repair Kit" },
        { 0xecca6c1fu, "Jolt Mine" },
        { 0x29311bc0u, "Impulse Mine" },
        { 0x8790f0d5u, "Stamina Drain Mine" },
        { 0xa5f27743u, "Fire Spray" },
        { 0x6e410139u, "Decoy Device" },
        { 0x4dd77f35u, "Glowstick" },
        { 0x4dd7ae42u, "Blue Glowstick" },
        { 0x092af7abu, "Green Glowstick" },
        { 0x7e7b2955u, "Red Glowstick" },
        { 0x579f7ed6u, "Yellow Glowstick" },
        { 0xabfd92f7u, "Shrapnel Grenade" },
        { 0x5fe8a191u, "Light Smoke Grenade" },
        { 0x65d8e680u, "Trailblazer Flare" },
        { 0x6f6b7afbu, "Trailblazer Flare" },
    };
    return table;
}

inline std::string ResolveWeaponInfo(const std::string& blueprintName)
{
    std::string normalized = NormalizeBlueprintName(blueprintName);
    const uint32_t h = TagDigest::Digest(normalized);
    const auto& table = GetWeaponNameHashMap();
    auto it = table.find(h);
    if (it != table.end())
        return it->second;
    return blueprintName;
}

// Hash table is keyed on BP_WeaponActor_* / BP_ItemActor_* (see Debug 52:
// AssaultRifle_LowTier_01 -> Rattler). Stowed slots expose DA_WeaponVisuals_*
// with the same suffix; rewrite them onto the BP key before hashing.
// Throwables/utilities live as BP_ItemActor_* — mapping DA_Item_* onto
// WeaponActor broke Smoke / Trigger / mine name resolution.
inline std::string WeaponLookupName(const std::string& rawName)
{
    std::string n = NormalizeBlueprintName(rawName);
    if (n.size() >= 9 && n.compare(0, 9, "Default__") == 0)
        n = n.substr(9);
    if (n.size() > 17 && n.compare(0, 17, "DA_WeaponVisuals_") == 0)
        return std::string("BP_WeaponActor_") + n.substr(17);
    if (n.size() > 14 && n.compare(0, 14, "DA_WeaponItem_") == 0)
        return std::string("BP_WeaponActor_") + n.substr(14);
    if (n.size() > 17 && n.compare(0, 17, "DA_ItemDataAsset_") == 0)
        return std::string("BP_ItemActor_") + n.substr(17);
    if (n.size() > 8 && n.compare(0, 8, "DA_Item_") == 0)
        return std::string("BP_ItemActor_") + n.substr(8);
    return n;
}

inline bool IsRawBlueprintName(const std::string& s)
{
    return s.find("BP_") != std::string::npos ||
           s.find("DA_") != std::string::npos ||
           s.find("Default__") != std::string::npos;
}

inline const char* HeldItemSubstringFallback(const std::string& lookup)
{
    if (lookup.find("RemoteFlareTrap") != std::string::npos ||
        lookup.find("Remote_Flare") != std::string::npos)
        return "Trailblazer Flare";
    if (lookup.find("AssaultRifle_Bullpup") != std::string::npos)
        return "Arpeggio";
    if (lookup.find("SMG_LowTier") != std::string::npos)
        return "Stitcher";
    if (lookup.find("SMG_01") != std::string::npos && lookup.find("LowTier") == std::string::npos)
        return "Bobcat";
    if (lookup.find("HandCannon_SingleAction") != std::string::npos)
        return "Anvil";
    if (lookup.find("BattleRifle_LeverAction") != std::string::npos)
        return "Renegade";
    if (lookup.find("StickyRemote") != std::string::npos)
        return "Sticky Grenade";
    if (lookup.find("Trigger") != std::string::npos)
        return "Triggers 'Nade";
    if (lookup.find("LightSmoke") != std::string::npos || lookup.find("Light_Smoke") != std::string::npos)
        return "Light Smoke Grenade";
    if (lookup.find("Smoke") != std::string::npos)
        return "Smoke Grenade";
    if (lookup.find("ShowStopper") != std::string::npos || lookup.find("Show_Stopper") != std::string::npos)
        return "ShowStopper";
    if (lookup.find("Wolfpack") != std::string::npos)
        return "Wolfpack";
    if (lookup.find("LightImpact") != std::string::npos || lookup.find("Light_Impact") != std::string::npos)
        return "Light Impact Grenade";
    if (lookup.find("Shrapnel") != std::string::npos)
        return "Shrapnel Grenade";
    if (lookup.find("Corrosive") != std::string::npos)
        return "Corrosive Grenade";
    if (lookup.find("Seeker") != std::string::npos)
        return "Seeker Grenade";
    if (lookup.find("Fuze") != std::string::npos || lookup.find("Fuse") != std::string::npos)
        return "Heavy Fuze Grenade";
    if (lookup.find("HEGrenade") != std::string::npos || lookup.find("HE_Grenade") != std::string::npos)
        return "HE Grenade";
    if (lookup.find("JumpMine") != std::string::npos || lookup.find("Jump_Mine") != std::string::npos)
        return "Jump Mine";
    if (lookup.find("Jolt") != std::string::npos)
        return "Jolt Mine";
    if (lookup.find("Impulse") != std::string::npos)
        return "Impulse Mine";
    if (lookup.find("Stamina") != std::string::npos)
        return "Stamina Drain Mine";
    if (lookup.find("SoundTrap") != std::string::npos || lookup.find("Sound_Trap") != std::string::npos)
        return "Sound Trap";
    if (lookup.find("Flare") != std::string::npos)
        return "Trailblazer Flare";
    if (lookup.find("Glowstick") != std::string::npos || lookup.find("GlowStick") != std::string::npos)
        return "Glowstick";
    if (lookup.find("Grenade") != std::string::npos)
        return "Grenade";
    if (lookup.find("Mine") != std::string::npos)
        return "Mine";
    if (lookup.find("Trap") != std::string::npos)
        return "Trap";
    if (lookup.find("HealingHoT") != std::string::npos)
        return "Herbal Bandage";
    if (lookup.find("Adrenalin") != std::string::npos)
        return "Adrenaline Shot";
    if (lookup.find("ShieldOverTime") != std::string::npos)
        return "Armor Pack";
    if (lookup.find("Snaphook") != std::string::npos || lookup.find("SnapHook") != std::string::npos)
        return "Snap Hook";
    if (lookup.find("Defibrillator") != std::string::npos)
        return "Defibrillator";
    if (lookup.find("Binocular") != std::string::npos)
        return "Binoculars";
    if (lookup.find("Barricade") != std::string::npos)
        return "Barricade Kit";
    if (lookup.find("ZipLine") != std::string::npos || lookup.find("Zipline") != std::string::npos)
        return "ZipLine";
    if (lookup.find("DoorBlocker") != std::string::npos || lookup.find("Door_Blocker") != std::string::npos)
        return "Door Blocker";
    if (lookup.find("Decoy") != std::string::npos)
        return "Decoy Device";
    if (lookup.find("Cloak") != std::string::npos)
        return "PhotoElectric Cloak";
    return nullptr;
}

// Humanize unmatched BP_ItemActor_FooBar_C → "Foo Bar" so held utilities never blank out.
inline std::string HumanizeBlueprintLeaf(const std::string& lookup)
{
    std::string n = lookup;
    static const char* kPrefs[] = {
        "BP_ItemActor_", "BP_WeaponActor_", "BP_WItem_", "BP_Actor_Throwable_",
        "BP_Item_", "ItemActor_", "WeaponActor_"
    };
    for (const char* p : kPrefs) {
        const size_t len = strlen(p);
        if (n.size() > len && n.compare(0, len, p) == 0) {
            n = n.substr(len);
            break;
        }
    }
    if (n.size() > 2 && n.compare(n.size() - 2, 2, "_C") == 0)
        n.resize(n.size() - 2);
    for (char& c : n) {
        if (c == '_')
            c = ' ';
    }
    // Insert spaces before CamelCase boundaries: SmokeGrenade → Smoke Grenade
    std::string out;
    out.reserve(n.size() + 8);
    for (size_t i = 0; i < n.size(); ++i) {
        const char c = n[i];
        if (i > 0 && isupper((unsigned char)c) && islower((unsigned char)n[i - 1]))
            out.push_back(' ');
        out.push_back(c);
    }
    while (!out.empty() && out.front() == ' ')
        out.erase(out.begin());
    return out;
}

inline std::string PrettyHeldItemName(const std::string& blueprintName)
{
    if (blueprintName.empty())
        return {};
    if (blueprintName.find("Stowed") != std::string::npos)
        return {};

    const std::string lookup = WeaponLookupName(blueprintName);
    if (lookup.find("Stowed") != std::string::npos)
        return {};

    std::string resolved = ResolveWeaponInfo(lookup);
    if (resolved != lookup && !IsRawBlueprintName(resolved))
        return resolved;

    if (const char* fb = HeldItemSubstringFallback(lookup))
        return fb;

    // Never drop ItemActor / WeaponActor to blank — that hid smoke/trigger in hand.
    if (IsRawBlueprintName(lookup) ||
        lookup.find("WeaponActor") != std::string::npos ||
        lookup.find("ItemActor") != std::string::npos ||
        lookup.find("Throwable") != std::string::npos)
    {
        std::string leaf = HumanizeBlueprintLeaf(lookup);
        if (!leaf.empty() && !IsRawBlueprintName(leaf))
            return leaf;
        return {};
    }
    if (IsRawBlueprintName(resolved))
        return {};
    return resolved;
}

// Threat subtypes.
//
// The matching chain is the one this file has always used and its order is
// load-bearing - "Sticky" has to be tested before the generic "Grenade", and
// "SoundTrap" before "Trap". Filtering needs the same answer the label does, so
// rather than keep a second chain in step with this one, both come out of a
// single table: the chain resolves an id, the table turns that id into the
// on-screen label and into a bit position in its category's filter mask.
enum class ThreatCategory : int { None = -1, Grenade = 0, Mine = 1, Trap = 2 };

struct ThreatSubtypeInfo {
    const char* label;         // drawn on screen; unchanged from before
    ThreatCategory category;
    int bit;                   // position within that category's filter mask
};

// Bit ordering here is the dropdown ordering, which is why the catch-alls sit
// last in each category even though the chain resolves them late.
inline constexpr ThreatSubtypeInfo kThreatSubtypes[] = {
    { "Sticky Grenade",        ThreatCategory::Grenade,  0 },
    { "Smoke Grenade",         ThreatCategory::Grenade,  1 },
    { "Light Impact Grenade",  ThreatCategory::Grenade,  2 },
    { "Shrapnel Grenade",      ThreatCategory::Grenade,  3 },
    { "Corrosive Grenade",     ThreatCategory::Grenade,  4 },
    { "Seeker Grenade",        ThreatCategory::Grenade,  5 },
    { "Heavy Fuze Grenade",    ThreatCategory::Grenade,  6 },
    { "HE Grenade",            ThreatCategory::Grenade,  7 },
    { "Wolfpack",              ThreatCategory::Grenade,  8 },
    { "ShowStopper",           ThreatCategory::Grenade,  9 },
    { "Jump Mine",             ThreatCategory::Mine,     0 },
    { "Jolt Mine",             ThreatCategory::Mine,     1 },
    { "Impulse Mine",          ThreatCategory::Mine,     2 },
    { "Stamina Drain Mine",    ThreatCategory::Mine,     3 },
    { "Sound Trap",            ThreatCategory::Trap,     0 },
    { "Triggers 'Nade",        ThreatCategory::Grenade, 10 },
    { "Grenade",               ThreatCategory::Grenade, 10 },
    { "Mine",                  ThreatCategory::Mine,     4 },
    { "Trailblazer Flare",     ThreatCategory::Trap,     1 },
    { "Trap",                  ThreatCategory::Trap,     2 },
};

inline constexpr int kGrenadeFilterCount = 11;
inline constexpr int kMineFilterCount = 5;
inline constexpr int kTrapFilterCount = 3;

// Where anything the chain does not recognise lands. Defaulting these on is
// what keeps a newly shipped grenade visible instead of silently filtered out
// until someone notices it missing.
inline constexpr int ThreatOtherBit(ThreatCategory c)
{
    return c == ThreatCategory::Grenade ? 10 : (c == ThreatCategory::Mine ? 4 : 2);
}

inline constexpr int ThreatFilterCount(ThreatCategory c)
{
    return c == ThreatCategory::Grenade ? kGrenadeFilterCount
         : (c == ThreatCategory::Mine ? kMineFilterCount : kTrapFilterCount);
}

// Index into kThreatSubtypes, or -1 when nothing matches.
inline int ResolveThreatSubtypeId(const std::string& actorName)
{
    if (actorName.find("Sticky") != std::string::npos)
        return 0;
    if (actorName.find("Smoke") != std::string::npos)
        return 1;
    if (actorName.find("LightImpact") != std::string::npos || actorName.find("Light_Impact") != std::string::npos)
        return 2;
    if (actorName.find("Shrapnel") != std::string::npos)
        return 3;
    if (actorName.find("Corrosive") != std::string::npos)
        return 4;
    if (actorName.find("Seeker") != std::string::npos)
        return 5;
    if (actorName.find("Fuze") != std::string::npos || actorName.find("Fuse") != std::string::npos)
        return 6;
    if (actorName.find("HEGrenade") != std::string::npos || actorName.find("HE_Grenade") != std::string::npos)
        return 7;
    if (actorName.find("Wolfpack") != std::string::npos)
        return 8;
    if (actorName.find("ShowStopper") != std::string::npos || actorName.find("Show_Stopper") != std::string::npos)
        return 9;
    if (actorName.find("JumpMine") != std::string::npos || actorName.find("Jump_Mine") != std::string::npos)
        return 10;
    if (actorName.find("Jolt") != std::string::npos)
        return 11;
    if (actorName.find("Impulse") != std::string::npos)
        return 12;
    if (actorName.find("Stamina") != std::string::npos)
        return 13;
    if (actorName.find("SoundTrap") != std::string::npos || actorName.find("Sound_Trap") != std::string::npos)
        return 14;
    if (actorName.find("Trigger") != std::string::npos)
        return 15; // Triggers 'Nade
    if (actorName.find("Grenade") != std::string::npos)
        return 16;
    if (actorName.find("Mine") != std::string::npos)
        return 17;
    if (actorName.find("Flare") != std::string::npos)
        return 18;
    if (actorName.find("Trap") != std::string::npos)
        return 19;
    return -1;
}

inline const char* ResolveThrowableLabel(const std::string& actorName, const char* fallback)
{
    const int id = ResolveThreatSubtypeId(actorName);
    return id >= 0 ? kThreatSubtypes[id].label : fallback;
}

// Bit position within `cat`'s filter mask.
//
// A name can resolve to a subtype belonging to a different category than the
// bucket it was classified into - "JoltGrenade" would hit the Jolt Mine rule
// while sitting in the Grenade bucket. Indexing the wrong mask with that bit
// would filter on an unrelated checkbox, so a cross-category hit falls back to
// the bucket's own catch-all.
inline int ResolveThreatFilterBit(const std::string& actorName, ThreatCategory cat)
{
    const int id = ResolveThreatSubtypeId(actorName);
    if (id >= 0 && kThreatSubtypes[id].category == cat)
        return kThreatSubtypes[id].bit;
    return ThreatOtherBit(cat);
}

// Whether a threat actor is the inventory representation of an item rather
// than something armed in the world.
//
// BP_ItemActor_* and BP_Actor_Throwable_* exist while the item is in someone's
// hands, in a corpse's pack, in a container, or on the floor - a smoke grenade
// on a dead body is the same class as one about to be thrown at you. BP_WItem_*
// is the spawned in-world version: a placed mine, a set trap. Anything matching
// neither family is treated as in-world, so an unrecognised threat is shown
// rather than silently hidden.
inline bool IsThreatItemActor(const std::string& actorName)
{
    return actorName.rfind("BP_ItemActor_", 0) == 0 ||
           actorName.rfind("BP_Actor_Throwable_", 0) == 0;
}

// Draw-side gate. A bit of -1 means the item was cached before it had a
// subtype resolved, which should show rather than vanish - the category
// toggle above this is what the user set to hide it.
inline bool ThreatTypeSelected(int mask, int bit)
{
    if (bit < 0 || bit > 31)
        return true;
    return (mask & (1 << bit)) != 0;
}

inline const char* const* ThreatFilterNames(ThreatCategory c, int& outCount)
{
    static const char* const kGrenade[] = {
        "Sticky", "Smoke", "Light Impact", "Shrapnel", "Corrosive", "Seeker",
        "Heavy Fuze", "HE", "Wolfpack", "ShowStopper", "Other / Unknown"
    };
    static const char* const kMine[] = {
        "Jump Mine", "Jolt Mine", "Impulse Mine", "Stamina Drain", "Other / Unknown"
    };
    static const char* const kTrap[] = {
        "Sound Trap", "Trailblazer Flare", "Other / Unknown"
    };

    if (c == ThreatCategory::Grenade) { outCount = kGrenadeFilterCount; return kGrenade; }
    if (c == ThreatCategory::Mine)    { outCount = kMineFilterCount;    return kMine; }
    outCount = kTrapFilterCount;
    return kTrap;
}

inline bool IsHeldThrowableName(const std::string& name)
{
    if (name.empty() || name == "None")
        return false;
    // Pretty labels + raw BP leftovers that still mention the class.
    return name.find("Grenade") != std::string::npos ||
        name.find("Smoke") != std::string::npos ||
        name.find("Trigger") != std::string::npos ||
        name.find("ShowStopper") != std::string::npos ||
        name.find("Wolfpack") != std::string::npos ||
        name.find("Mine") != std::string::npos ||
        name.find("Trap") != std::string::npos ||
        name.find("Flare") != std::string::npos ||
        name.find("Trailblazer") != std::string::npos ||
        name.find("Glowstick") != std::string::npos;
}

inline std::string trim(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos)
        return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}


inline std::string MapLocale;


enum class EPlayerStateStatus : UINT8
{
	Alive = 0,
	DBNO = 1,
	Crabbed = 2,
	Dead = 3,
	Defeated = 4,
};