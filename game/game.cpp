#include"main.hpp"
int main() {
	init();
	//if (MOD_DEV)generateTemplateDatapack();
	loadMods();
	if (initSave()) {
		player.x = player.y = 0;
		player.planet = "Sylva";
		for (player.z = 255; planets[player.planet].getBlock(player.x, player.y, player.z) == nullptr; player.z--) {}
		player.z++;
		player.item = planets[player.planet].setBlock({ "player",(32 << 16) | '@',{
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
			{1,make_shared<Item>(Item{ "player_printer",(255 << 16) | '@',{{1,nullptr},{1,nullptr}},1,0,0}),true},
		},256 }, player.x, player.y, player.z);
	}
	while (1) {
		string temp = eventLoop();
		cout.write(temp.c_str(), temp.size());
		cout.flush();
		key = _getch();
	}
	return 0;
}