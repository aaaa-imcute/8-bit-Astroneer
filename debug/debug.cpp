#include"main.hpp"
int main() {
	init();
	//if (MOD_DEV)generateTemplateDatapack();
	loadMods();
	if (initSave()) {
		for (int i = 0; i < 256; i++) {
			planets["Sylva"].removeBlock(0, 0, i);
		}
		player.item = planets["Sylva"].setBlock({ "player",(32 << 16) | '@',{
			{1,make_shared<Item>(Item{ "player_oxygen_tank",(90 << 16) | '@',{},1,90 }),true},
			{1,make_shared<Item>(Item{ "player_battery",(100 << 16) | '@',{},1,100 }),true},
			{1,make_shared<Item>(Item{ "player_terrain_tool",(8 << 16) | '@',{{1,nullptr},{1,nullptr},{1,nullptr}},1,0,0}),true},
			{4,nullptr,false,"air",true},
			{1,nullptr,false,"air",false,'c'},
			{1,nullptr,false,"air",false,'v'},
			{1,nullptr},
			{1,nullptr},
			{1,nullptr},
			{1,nullptr},
			{1,nullptr},
			{1,nullptr},
			{1,nullptr},
			{1,nullptr},
			{1,make_shared<Item>(Item{ "platform_pacemaker",(255 << 16) | '@',{{2,nullptr,false,"oxygenator"}},1}),true},
			{1,createItem({ "truncated_pentachoron", (128 << 16) | '!' ,{} ,1}),true},
		},256 }, 0, 0, 0);
		player.x = player.y = player.z = 0;
		player.planet = "Sylva";
		planets["Sylva"].setBlock({ "test_l_weighted_cube",(255 << 16) | '#',{},3 }, 0, 1, 0);
		planets["Sylva"].setBlock({ "platform_pacemaker",(255 << 16) | '@',{},1 }, 1, 1, 0);
		planets["Sylva"].setBlock({ "platform_test_siren",(0 << 16) | '@',{},1 }, 2, 1, 0);
		planets["Sylva"].setBlock({ "platform_medium_a",(255 << 16) | '#', {{1,createItem({ "canister",16711715,{{1,nullptr},{1,nullptr}},2 ,0,32}),false,"air",true}}, 2 }, 1, 2, 0);

		planets["Sylva"].setBlock({ "platform_medium_a",(255 << 16) | '#', {{1,createItem({ "canister_resin",16711715,{{1,nullptr},{1,nullptr}},2 ,3,32}),false,"air",true}}, 2
			}, 1, 4, 0);
		planets["Sylva"].setBlock({ "platform_power_extenders",15007779,{},1 }, 1, 6, 0);
		planets["Sylva"].setBlock({ "test_power_void",(255 << 16) | '@',{},1 ,0,10 }, 1, 7, 0);
		planets["Sylva"].setBlock({ "platform_medium_a",(255 << 16) | '#', {{1,createItem({ "test_power_source",(255 << 16) | '@',{},1 ,0,10}),false,"air",true}}, 2
			}, 2, 4, 0);
		planets["Sylva"].setBlock({ "battery_small",(255 << 16) | '@',{},1 ,0,1 }, 2, 5, 0);
		player.updates.push_back({ "Sylva",0,0,0 });
		player.updates.push_back({ "Sylva",1,1,0 });
		saveGame();
	}
	while (1) {
		string temp = eventLoop();
		cout.write(temp.c_str(), temp.size());
		cout.flush();
		key = _getch();
	}
	return 0;
}