#ifdef _WIN32
#define _WIN32_WINNT 0x0500
#include <Windows.h>
#include <conio.h>
#else
#include <termios.h>
#endif
#include <iostream>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <string>
#include<algorithm>
#include<cstring>
#include<fstream>
#include<filesystem>
#include<iterator>
#include"cereal/archives/portable_binary.hpp"
#include"cereal/archives/json.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/unordered_map.hpp"
#include "cereal/types/utility.hpp"
#include "PerlinNoise.hpp"
using uint = unsigned int;
using cereal::make_nvp;
constexpr auto W = 100;
constexpr auto H = 50;
constexpr auto MOD_DEV = 0;
using namespace std;
vector<vector<uint> > buf(H, vector<uint>(W, (32 << 16) | ' '));
char key = 0;
void init() {
#ifdef _WIN32
	DWORD dwMode = 0;
	dwMode |= 4;
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO cursorInfo;
	GetConsoleCursorInfo(hOut, &cursorInfo);
	cursorInfo.bVisible = FALSE;
	SetConsoleCursorInfo(hOut, &cursorInfo);
#else
	termios old, current;
	tcgetattr(0, &old); /* grab old terminal i/o settings */
	current = old; /* make new settings same as old settings */
	current.c_lflag &= ~ICANON; /* disable buffered i/o */
	current.c_lflag &= ~ECHO; /* set no echo mode */
	tcsetattr(0, TCSANOW, &current); /* use these new terminal i/o settings now */
#endif
	ios::sync_with_stdio(false);
	setvbuf(stdout, nullptr, _IONBF, 0);
	cout.setf(ios::unitbuf);
	cout << "\033[3J\033[1;1HPress any key to continue";
	cout.flush();
	_getch();//return value ignore.
}
const siv::PerlinNoise::seed_type noiseSeed = 123456u;
const siv::PerlinNoise perlin{ noiseSeed };
struct PowerStatus;
struct Slot;
struct Update;
struct Item {
	string id;
	uint display = 128 << 16 | '?';//oops
	vector<Slot> slots;
	int size = -1;//oops
	int dmg = 0;
	int cfg = 0;
	int sig = 0;
	shared_ptr<Item> ptr = nullptr;
	PowerStatus getPower(shared_ptr<Item> that);
	bool isWorking();
	Update getFacing(Update u, int dist);
	vector<int> _getFacing();
	template<class Archive>
	void serialize(Archive& ar) {
		ar(make_nvp("id", id), make_nvp("display", display), make_nvp("slots", slots), make_nvp("size", size), make_nvp("dmg", dmg), make_nvp("cfg", cfg),make_nvp("sig",sig));
	}
};
struct Slot {
	int size = -1;//oops
	shared_ptr<Item> content;
	bool locked = false;
	string sorter = "air";
	bool uni = false;
	char qTrig = 0;
	bool isFree() const {
		return content == nullptr;
	}
	template<class Archive>
	void serialize(Archive& ar) {
		ar(make_nvp("size", size), make_nvp("content", content), make_nvp("locked", locked), make_nvp("sorter", sorter), make_nvp("uni", uni), make_nvp("qTrig", qTrig));
	}
};
struct BatteryStatus {
	int storage;
	int rate;
	shared_ptr<Item> item;
	int emptiness = 0;
};
struct PowerStatus {
	int used = 0;
	int produced = 0;
	vector<BatteryStatus> stored = {};
};
struct OreParams {
	vector<string> ores;
	double thresh;
	template<class Archive>
	void serialize(Archive& ar) {
		ar(make_nvp("ores", ores), make_nvp("thresh", thresh));
	}
};
struct PlanetMod {
	int difficulty;
	vector<OreParams> ores;
	template<class Archive>
	void serialize(Archive& ar) {
		ar(make_nvp("difficulty", difficulty), make_nvp("ores", ores));
	}
};
struct TerrainToolMods {
	int hardness = 0;
	int range = 1;
	//special effects?
	string special = "";
};
unordered_map<string, vector<int>> powerMap;
unordered_map<string, unsigned char> resourceColors;
unordered_map<string, vector<pair<vector<string>, Item> > > printerRecipes;
unordered_map<string, PlanetMod> planetSettings;
PowerStatus Item::getPower(shared_ptr<Item> that) {
	PowerStatus s;
	if (id == "test_power_source") {
		s.produced = cfg;
	}
	else if (id == "test_power_void") {
		s.used = cfg;
	}
	else if (powerMap.find(id) != powerMap.end()) {
		s.produced = powerMap[id][0]*isWorking();
		s.used = powerMap[id][1]*isWorking();
		if (powerMap[id][2])s.stored.push_back({ powerMap[id][2],powerMap[id][3],that,(dmg >= powerMap[id][2] ? 1 : (dmg <= 0 ? -1 : 0)) });
	}
	for (Slot& a : slots) {
		if (a.content == nullptr)continue;
		auto t = a.content->getPower(a.content);
		s.used += t.used;
		s.produced += t.produced;
		s.stored.insert(s.stored.end(), t.stored.begin(), t.stored.end());
	}
	return s;
}
bool Item::isWorking() {
	return printerRecipes.find(id) == printerRecipes.end() || dmg < 256;
};
//no junk
shared_ptr<Item> createResource(string type) {
	uint a = resourceColors[type] << 16 | '*';
	return make_shared<Item>(Item{ "resource_" + type,a,{},1,255 });
}
shared_ptr<Item> createNull() {
	return nullptr;
}
shared_ptr<Item> createItem(Item i) {
	return make_shared<Item>(i);
}
vector<string> planetNames = { "Sylva","Desolo","Calidor","Vesania","Novus","Atrox" };
vector<vector<vector<shared_ptr<Item> > > > createChunk(string planet, int x, int y) {
	vector<vector<vector<shared_ptr<Item> > > > a;
	a.resize(256);
	for (int i = 0; i < 256; i++) {
		a[i].resize(16);
		for (int j = 0; j < 16; j++) {
			a[i][j].resize(16);
		}
	}
	int planetDiff = planetSettings[planet].difficulty;
	vector<vector<vector<int>>> heightmap(12);
	for (int i = 0; i < 12; i++) {
		heightmap[i].resize(16);
		for (int j = 0; j < 16; j++) {
			heightmap[i][j].resize(16);
			for (int k = 0; k < 16; k++) {
				heightmap[i][j][k] = int(4 * perlin.noise3D((x * 16 + j) * 0.1, (y * 16 + k) * 0.1, i+256*(find(planetNames.begin(),planetNames.end(),planet)-planetNames.begin())) + 10 * i);
			}
		}
	}
	vector <OreParams > ores = planetSettings[planet].ores;//in decreasing priority
	for (int i = 0; i < 6; i += 2) {
		for (int j = 0; j < 16; j++) {
			for (int k = 0; k < 16; k++) {
				for (int l = heightmap[i][j][k]; l < heightmap[i + 1][j][k]; l++) {
					if (l < 0 || l>255)continue;
					int hardness = 3 - i / 2 + planetDiff;
					for (int o = 0; o < ores.size(); o++) {
						if (perlin.noise3D((x * 16 + j) * 0.2, (y * 16 + k) * 0.2, (o * 256 * 256 + 256 * (find(planetNames.begin(), planetNames.end(), planet) - planetNames.begin()) + l) * 0.2) > ores[o].thresh) {//dont add more than 256 planets.
							string id = ores[o].ores[hardness - planetDiff-1];
							a[l][k][j] = createItem({id + "_placed",uint(resourceColors[id]) << 16 | '-',{},256,hardness});
							break;
						}
					}
					if(a[l][k][j]==nullptr)a[l][k][j] = createItem({"soil_placed",255 << 16 | '-',{},256,hardness });
				}
			}
		}
	}
	return a;
}
Item _nilItem;
shared_ptr<Item> nilItem = make_shared<Item>(_nilItem);
struct Planet {
	string name;
	unordered_map<string, vector<vector<vector<shared_ptr<Item> > > > > chunks;
	vector<vector<vector<shared_ptr<Item> > > >& getChunk(int x, int y) {
		string cid = to_string(x) + "," + to_string(y);
		auto a = chunks.find(cid);
		if (a == chunks.end()) {
			ifstream f;
			f.open((".\\save\\world\\chunk_" + name + "_" + cid + ".bin").c_str(), ios::binary);
			if (f.good()) {
				cereal::PortableBinaryInputArchive ain(f);
				vector<vector<vector<shared_ptr<Item> > > > chunk;
				ain(chunk);
				chunks.insert({ cid,chunk });
				return chunks[cid];
			}
			chunks.insert({ cid,createChunk(name, x, y) });
			return chunks[cid];
		}
		else {
			return a->second;
		}
	}
	shared_ptr<Item>& getBlock(int x, int y, int z) {
		if (z < 0 || z>255) {
			return nilItem;
		}
		return getChunk(x >> 4, y >> 4)[z][y & 15][x & 15];
	}
	shared_ptr<Item>& setBlock(Item i, int x, int y, int z) {
		if (z < 0 || z>255) {
			return nilItem;
		}
		(getChunk(x >> 4, y >> 4))[z][y & 15][x & 15] = make_shared<Item>(i);
		return getChunk(x >> 4, y >> 4)[z][y & 15][x & 15];
	}
	void removeBlock(int x, int y, int z) {
		if (z < 0 || z>255)return;
		getChunk(x >> 4, y >> 4)[z][y & 15][x & 15] = nullptr;
	}
};
unordered_map<string, Planet> planets;
struct Update {
	string planet = "lulz";
	int x = 420;
	int y = 69;
	int z = 1337;//if you see those funny numbers its because you made a mistake that made uninitialized update structs which vs told me to initialize values for.
	int totalPower = 80;
	int usedPower = 14;
	int flags = 0;
	static Update fromOther(Update u, int x, int y, int z) {
		u.x = x;
		u.y = y;
		u.z = z;
		return u;
	}
	bool operator==(Update& o) const {
		return (toString() == o.toString());
	}
	bool operator<(Update& o) const {
		return toString() < o.toString();
	}
	string toString() const {
		return "Update_" + planet + "_" + to_string(x) + "_" + to_string(y) + "_" + to_string(z) + "_" + to_string(totalPower) + "_" + to_string(usedPower);
	}
	shared_ptr<Item>& getBlock() const {
		return planets[planet].getBlock(x, y, z);
	}
	vector<Update> neighbors() {
		return {
			fromOther(*this,x + 1, y, z),
			fromOther(*this,x - 1, y, z),
			fromOther(*this,x, y + 1, z),
			fromOther(*this,x, y - 1, z),
			fromOther(*this,x, y, z + 1),
			fromOther(*this,x, y, z - 1),
		};
	}
	int lackPower(int x) const {
		if (usedPower == 0)return x;
		if (totalPower > usedPower)return x;
		return x * totalPower / usedPower;//do not switch order
	}
	bool funnyPower() const {
		return !(flags & 1);//no more funny power
	}
	template<class Archive>
	void serialize(Archive& ar) {
		ar(make_nvp("planet", planet), make_nvp("x", x), make_nvp("y", y), make_nvp("z", z), make_nvp("totalPower", totalPower), make_nvp("usedPower", usedPower));
	}
};
vector<Update> _updates;
unordered_map<string, int> updatesDone;
vector<int> Item::_getFacing() {
	switch (cfg) {
	case 0:
		display = (display >> 16) << 16 | '<';
		return { -1,0,0 };
	case 1:
		display = (display >> 16) << 16 | '>';
		return { 1,0,0 };
	case 2:
		display = (display >> 16) << 16 | '^';
		return { 0,-1,0 };
	case 3:
		display = (display >> 16) << 16 | 'v';
		return { 0,1,0 };
	case 4:
		display = (display >> 16) << 16 | 'x';
		return { 0,0,-1 };
	case 5:
		display = (display >> 16) << 16 | '.';
		return { 0,0,1 };
	}
}
Update Item::getFacing(Update u,int dist){
	vector<int> f = _getFacing();
	f[0] *= dist;
	f[1] *= dist;
	f[2] *= dist;
	u.x += f[0];
	u.y += f[1];
	u.z += f[2];
	return u;
}
int cursorAt = 0;
int cursorSel = 0;
int cursorX = 0, cursorY = 0;
int cursorObjx, cursorObjy, cursorObjz;
bool oxygenDeducted = false;
int playerSpeed = 2;
bool flight = true;
string cursorObjplanet;
shared_ptr<Item> cursorObj;
enum DisplayMode {
	DNORM,
	DUNDER,
	DDEPTH,
	DABOVE
} dmode;
struct PlayerData {
	shared_ptr<Item> item;
	int x = 0, y = 0, z = 0;
	string planet = "Sylva";
	vector<Update> updates;
	template<class Archive>
	void serialize(Archive& ar) {
		ar(make_nvp("x", x), make_nvp("y", y), make_nvp("z", z), make_nvp("planet", planet), make_nvp("updates", updates));
	}
} player;
template<class T>
void saveFile(string name, T& contents) {
	ofstream f;
	f.open(name.c_str(), ios::binary | ios::trunc);
	if (f.good()) {
		cereal::PortableBinaryOutputArchive aout(f);
		aout(contents);
	}
	else {
		saveFile(name, contents);//:devil:
	}
}
void saveGame() {
	for (auto& p : planets) {
		auto& planet = p.second;
		for (auto& c : planet.chunks) {
			saveFile(".\\save\\world\\chunk_" + planet.name + "_" + c.first + ".bin", c.second);
		}
	}
	saveFile(".\\save\\level.bin", player);
}
void loadGame() {
	for (auto& p : planets) {
		auto& planet = p.second;
		planet.chunks.clear();
	}
	ifstream f;
	f.open(".\\save\\level.bin", ios::binary);
	if (f.good()) {
		cereal::PortableBinaryInputArchive ain(f);
		ain(player);
	}
	player.item = planets[player.planet].getBlock(player.x, player.y, player.z);
}
void clearRect(int x, int y, int w, int h) {
	for (int i = x; i < x + w; i++) {
		for (int j = y; j < y + h; j++) {
			buf[j][i] = (255 << 16) | ' ';
		}
	}
}
void displayWorld() {
	clearRect(0, 0, 17, 17);
	auto& sylva = planets[player.planet];
	shared_ptr<Item> a;
	for (int i = -8; i < 9; i++) {
		for (int j = -8; j < 9; j++) {
			switch (dmode) {
			case DNORM:
				a = planets[player.planet].getBlock(player.x + i, player.y + j, player.z);
				if (a != nullptr)buf[j + 8][i + 8] = a->display;
				break;
			case DUNDER:
				a = planets[player.planet].getBlock(player.x + i, player.y + j, player.z - 1);
				if (a != nullptr)buf[j + 8][i + 8] = a->display;
				break;
			case DABOVE:
				a = planets[player.planet].getBlock(player.x + i, player.y + j, player.z + 1);
				if (a != nullptr)buf[j + 8][i + 8] = a->display;
				break;
			case DDEPTH:
				int k = player.z;
				do {
					a = planets[player.planet].getBlock(player.x + i, player.y + j, k);
				} while (--k >= 0 && a == nullptr);
				if (a != nullptr && player.z - k - 1 < 10)buf[j + 8][i + 8] = (255 << 16) | ('0' + (player.z - k - 1));
				break;
			}
		}
	}
}
void displayNumber(int num, int x, int y) {
	//Does not display numbers over 999
	buf[y][x] = (255 << 16) | (num < 0 ? '-' : '+');
	num = abs(num);
	buf[y][x + 3] = (255 << 16) | ('0' + num % 10);
	num /= 10;
	buf[y][x + 2] = (255 << 16) | ('0' + num % 10);
	num /= 10;
	buf[y][x + 1] = (255 << 16) | ('0' + num);

}
void displayItem(shared_ptr<Item>& item, int x, int y) {
	if (item == nullptr) {
		clearRect(x, y, 3, 1);
		buf[y][x] = (255 << 16) | 'n';
		buf[y][x + 1] = (255 << 16) | 'i';
		buf[y][x + 2] = (255 << 16) | 'l';
		return;
	}
	if (item == player.item) {
		clearRect(x, y, max(int(item->id.size()), 12), int(item->slots.size()) + 1);
		y--;
	}
	else {
		clearRect(x, y, max(int(item->id.size()), 12), int(item->slots.size()) + 2);
		for (int i = 0; i < item->id.size(); i++)buf[y][x + i] = (255 << 16) | (item->id[i]);
	}
	displayNumber(item->size, x, y + 1);
	displayNumber(item->dmg, x + 4, y + 1);
	displayNumber(item->cfg, x + 8, y + 1);
	for (int i = 0; i < item->slots.size(); i++) {
		if (item->slots[i].content != nullptr)buf[y + i + 2][x] = item->slots[i].content->display;
		if (item->slots[i].size < 10)buf[y + i + 2][x + 2] = (255 << 16) | ('0' + item->slots[i].size);
		if (item->slots[i].locked)buf[y + i + 2][x + 4] = (255 << 16) | 'L';
		if (item->slots[i].uni)buf[y + i + 2][x + 6] = (255 << 16) | 'U';
		if (item->slots[i].qTrig)buf[y + i + 2][x + 8] = (255 << 16) | item->slots[i].qTrig;
	}
}
/*bool isChildOf(shared_ptr<Item>& a, shared_ptr<Item>& b) {
	if (a == b)return true;
	for (auto& i : a->slots) {
		if (isChildOf(i.content, b))return true;
	}
	return false;
}*/
vector<shared_ptr<Item>> destroyTerrain(Update u, int hardness, int range) {//VERY inefficient
	//TODO:factor in special mods
	u.totalPower = range;
	vector<Update> queue = { u }, network = {};
	while (!queue.empty()) {
		auto i = queue[queue.size() - 1];
		queue.pop_back();
		if (find(network.begin(), network.end(), i) != network.end())continue;
		if (!(i.getBlock() == nullptr)) {
			if (!i.getBlock()->id.ends_with("_placed"))continue;
			network.push_back(i);
			int remove = hardness - i.getBlock()->dmg;
			if (remove > 1)i.totalPower -= remove;
			else i.totalPower -= 1;
			if (i.totalPower < 0)continue;
		}
		else {
			network.push_back(i);
			i.totalPower -= 1;
			if (i.totalPower < 0)continue;//unfortunately due to lasagna caves we cannot not remove power
		}
		auto n = i.neighbors();
		for (auto j : n) {
			queue.push_back(j);
		}
	}
	vector<shared_ptr<Item>> res = { createItem({"soil",(255 << 16) | '-',{},1,0}) };
	for (auto& a : network) {
		auto& block = a.getBlock();
		if (block == nullptr)continue;
		if (block->id == "soil_placed")res[0]->dmg++;
		else {
			block->id.erase(block->id.size() - (string("_placed").size()));
			block->id = "resource_" + block->id;
			block->size = 1;
			block->dmg = 255;
			block->display = ((block->display >> 16) << 16) | '*';
			res.push_back(createItem(*block));
		}
		a.getBlock() = nullptr;
	}
	return res;
}
void givePlayer(shared_ptr<Item> a) {
	for (auto& i : player.item->slots) {
		if (i.locked)continue;
		if (i.content != nullptr)continue;
		if (i.sorter != "air" && i.sorter != a->id)continue;
		if (i.size < a->size || i.size != a->size && !i.uni)continue;
		i.content = a;
		return;
	}
	Update u = { player.planet,player.x,player.y,player.z };
	vector<Update> queue = { u }, network = {};
	while (!queue.empty()) {
		auto i = queue[0];
		queue.erase(queue.begin(), queue.begin() + 1);
		if (find(network.begin(), network.end(), i) != network.end())continue;
		if (i.getBlock() == nilItem)continue;
		if (i.getBlock() == nullptr) {
			i.getBlock() = a;
			return;
		}
		network.push_back(i);
		auto n = i.neighbors();
		for (auto j : n) {
			queue.push_back(j);
		}
	}
	throw "Failed to throw item";
}
TerrainToolMods getTerrainToolMods() {//only the first special mod can function
	TerrainToolMods mods;
	shared_ptr<Item> tool = player.item->slots[2].content;
	for (auto a : tool->slots) {
		if (a.content == nullptr)continue;
		string id = a.content->id;
		if (id.starts_with("mod_drill_")) {
			mods.hardness += a.content->dmg;
		}
		else if (id.starts_with("mod_range_")) {
			mods.range += a.content->dmg;
		}
		else {
			mods.special = id;
		}
	}
	return mods;
}
void processUpdate(Update u,shared_ptr<Item> block);
void processUpdate(Update u);
void processCursor() {
	vector<string> dirs = { "platform_power_extenders","platform_auto_arm" ,"platform_long_auto_arm" ,"platform_fast_auto_arm" };
	if (cursorObj != nullptr && (printerRecipes.find(cursorObj->id) != printerRecipes.end()||cursorObj->id=="repeater_delay"||cursorObj->id=="repeater_count")) {
		if (key == 'y')cursorObj->cfg++;
		if (key == 'h')cursorObj->cfg--;
	}
	if (cursorObj != nullptr && find(dirs.begin(), dirs.end(), cursorObj->id) != dirs.end()){
		if (key == 'y')cursorObj->cfg++;
		cursorObj->cfg %= 6;
		cursorObj->_getFacing();
	}
	if (cursorObj != nullptr && player.item->slots[3].content != nullptr && key == 'z')player.item->slots[3].content->ptr = cursorObj;
	if (cursorObj != nullptr && key == 'f') {
		Update u = { cursorObjplanet,cursorObjx,cursorObjy,cursorObjz,80,14,2 };
		processUpdate(u, cursorObj);
	}
	if (key == 'c' || key == 'v') {
		vector<shared_ptr<Item>> queue = { player.item }, network;
		while (!queue.empty()) {
			auto i = queue[0];
			queue.erase(queue.begin(), queue.begin() + 1);
			if (find(network.begin(), network.end(), i) != network.end())continue;
			if (i == nullptr)continue;
			network.push_back(i);
			for (auto& j : i->slots) {
				if (j.qTrig == key) {
					Update u = { player.planet,player.x,player.y,player.z,80,14,2 };
					processUpdate(u, j.content);
				}
				queue.push_back(j.content);
			}
		}
	}
	if (key == 'u') {
		cursorAt += 1;
		cursorAt %= 3;
		cursorSel = 0;
	}
	if (cursorAt == 2 && cursorObj == nullptr) {
		cursorAt = 0;
		cursorSel = 0;
	}
	if (cursorAt == 1 && player.item->slots.size() == 0) {
		cursorAt = 0;
		cursorSel = 0;
	}
	if (cursorAt == 2 && cursorObj->slots.size() == 0) {
		cursorAt = 0;
		cursorSel = 0;
	}
	int yoff;
	switch (dmode) {
	case DNORM:
		yoff = 0;
		break;
	case DUNDER:
		yoff = -1;
		break;
	case DABOVE:
		yoff = 1;
		break;
	case DDEPTH:
		return;
	}
	auto& block = planets[player.planet].getBlock(cursorX - 8 + player.x, cursorY - 8 + player.y, player.z + yoff);
	switch (cursorAt) {
	case 0://note:possble bugs related to block placing/removing off height limits
		if (key == 'i')cursorSel -= 17;
		if (key == 'k')cursorSel += 17;
		if (key == 'j')cursorSel -= 1;
		if (key == 'l')cursorSel += 1;
		if (cursorSel < 0)cursorSel += 289;
		if (cursorSel >= 289)cursorSel -= 289;
		cursorX = cursorSel % 17;
		cursorY = cursorSel / 17;
		if (key == 'o') {
			cursorObj = block;
			cursorObjx = cursorX - 8 + player.x;
			cursorObjy = cursorY - 8 + player.y;
			cursorObjz = player.z + yoff;
			cursorObjplanet = player.planet;
		}
		if (key == 'b') {
			if (block == nullptr)break;
			if (player.item->slots[3].content != nullptr)break;
			if (player.item->slots[3].size < block->size)break;
			player.item->slots[3].content = block;
			planets[player.planet].removeBlock(cursorX - 8 + player.x, cursorY - 8 + player.y, player.z + yoff);
			player.updates.push_back({ player.planet,cursorX - 8 + player.x, cursorY - 8 + player.y, player.z + yoff });
			player.updates.push_back({ player.planet,player.x,player.y,player.z });
		}
		if (key == 'n') {
			if (player.item->slots[3].content == nullptr)break;
			if (block != nullptr)break;
			block = player.item->slots[3].content;
			player.item->slots[3].content = nullptr;
			player.updates.push_back({ player.planet,cursorX - 8 + player.x, cursorY - 8 + player.y, player.z + yoff });
			player.updates.push_back({ player.planet,player.x,player.y,player.z });
		}
		if (key == 'm') {
			if (player.item->slots[3].content == nullptr)break;
			if (block == nullptr)break;
			for (auto& i : block->slots) {
				if (i.content != nullptr)continue;
				if (i.size != player.item->slots[3].content->size && (!i.uni || i.size > player.item->slots[3].content->size))continue;
				//potential lack of sorter check?
				i.content = player.item->slots[3].content;
				player.item->slots[3].content = nullptr;
				break;
			}
			player.updates.push_back({ player.planet,cursorX - 8 + player.x, cursorY - 8 + player.y, player.z + yoff });
			player.updates.push_back({ player.planet,player.x,player.y,player.z });
		}
		if (key == 'x') {
			TerrainToolMods mods = getTerrainToolMods();
			auto res = destroyTerrain({ player.planet,cursorX - 8 + player.x, cursorY - 8 + player.y, player.z + yoff }, mods.hardness, mods.range);
			for (auto& a : player.item->slots) {
				if (a.content != nullptr && a.content->id == "soil" && a.content->dmg != 256) {
					res[0]->dmg += a.content->dmg;
					a.content = nullptr;
				}
			}
			while (res[0]->dmg > 255) {
				res[0]->dmg -= 256;
				givePlayer(createItem({ "soil",(255 << 16) | '-',{},1,256 }));
			}
			for (auto a : res) {
				givePlayer(a);
			}
		}
		break;
	case 1:
		if (key == 'i')cursorSel--;
		if (key == 'k')cursorSel++;
		if (cursorSel < 0)cursorSel = 0;
		if (cursorSel >= player.item->slots.size())cursorSel = int(player.item->slots.size()) - 1;
		cursorX = 17;
		cursorY = 1 + cursorSel;

		if (key == 'o') {
			cursorObj = player.item->slots[cursorSel].content;
			cursorObjx = player.x;
			cursorObjy = player.y;
			cursorObjz = player.z;
			cursorObjplanet = player.planet;
		}
		if (key == 'b') {
			if (player.item->slots[cursorSel].content == nullptr)break;
			if (player.item->slots[cursorSel].locked)break;
			if (player.item->slots[3].content != nullptr)break;
			if (player.item->slots[3].size < player.item->slots[cursorSel].content->size)break;
			player.item->slots[3].content = player.item->slots[cursorSel].content;
			player.item->slots[cursorSel].content = nullptr;
			player.updates.push_back({ player.planet,player.x,player.y,player.z });
		}
		if (key == 'n') {
			if (player.item->slots[3].content == nullptr)break;
			auto& i = player.item->slots[cursorSel];
			if (i.content != nullptr)break;
			if (i.size != player.item->slots[3].content->size && (!i.uni || i.size > player.item->slots[3].content->size))break;
			i.content = player.item->slots[3].content;
			player.item->slots[3].content = nullptr;
			player.updates.push_back({ player.planet,player.x,player.y,player.z });
		}
		if (key == 'm') {
			if (player.item->slots[3].content == nullptr)break;
			for (auto& i : player.item->slots) {
				if (i.content != nullptr)continue;
				if (i.size != player.item->slots[3].content->size && (!i.uni || i.size > player.item->slots[3].content->size))continue;
				i.content = player.item->slots[3].content;
				player.item->slots[3].content = nullptr;
				player.updates.push_back({ player.planet,player.x,player.y,player.z });
				break;
			}
		}
		break;
	case 2:
		if (key == 'i')cursorSel--;
		if (key == 'k')cursorSel++;
		if (cursorSel < 0)cursorSel = 0;
		if (cursorSel >= cursorObj->slots.size())cursorSel = int(cursorObj->slots.size()) - 1;
		cursorX = 0;
		cursorY = 19 + cursorSel;
		if (key == 'o') {
			cursorObj = cursorObj->slots[cursorSel].content;
			if (cursorObj == nullptr) {
				cursorAt = 0;
				cursorX = 0;
				cursorY = 0;
				cursorSel = 0;
				break;
			}
			if (cursorObj->slots.size() <= cursorSel) {
				cursorAt = 0;
				cursorX = 0;
				cursorY = 0;
				cursorSel = 0;
			}
		}
		if (key == 'b') {
			if (cursorObj->slots[cursorSel].content == nullptr)break;
			if (cursorObj->slots[cursorSel].locked)break;
			if (player.item->slots[3].content != nullptr)break;
			if (player.item->slots[3].size < cursorObj->slots[cursorSel].content->size)break;
			player.item->slots[3].content = cursorObj->slots[cursorSel].content;
			cursorObj->slots[cursorSel].content = nullptr;
			player.updates.push_back({ cursorObjplanet,cursorObjx,cursorObjy,cursorObjz });
			player.updates.push_back({ player.planet,player.x,player.y,player.z });
		}
		if (key == 'n') {
			//if (isChildOf(block, cursorObj))break;
			if (player.item->slots[3].content == nullptr)break;
			auto& i = cursorObj->slots[cursorSel];
			if (i.content != nullptr)break;
			if (i.size != player.item->slots[3].content->size && (!i.uni || i.size > player.item->slots[3].content->size))break;
			i.content = player.item->slots[3].content;
			player.item->slots[3].content = nullptr;
			player.updates.push_back({ cursorObjplanet,cursorObjx,cursorObjy,cursorObjz });
			player.updates.push_back({ player.planet,player.x,player.y,player.z });
		}
		if (key == 'B') {
			if (cursorObj->slots[cursorSel].content == nullptr)break;
			if (cursorObj->slots[cursorSel].content->id != "soil")break;
			if (cursorObj->slots[cursorSel].locked)break;
			if (player.item->slots[3].content != nullptr&& player.item->slots[3].content->id!="soil")break;
			if (player.item->slots[3].size < cursorObj->slots[cursorSel].content->size)break;
			if (player.item->slots[3].content == nullptr)player.item->slots[3].content = createItem({ "soil",(255 << 16) | '-',{},1});
			player.item->slots[3].content->dmg += cursorObj->slots[cursorSel].content->dmg/2;
			cursorObj->slots[cursorSel].content ->dmg /= 2;
			if (player.item->slots[3].content->dmg > 255) {
				cursorObj->slots[cursorSel].content->dmg += player.item->slots[3].content->dmg - 256;
				player.item->slots[3].content->dmg = 256;
			}
			player.updates.push_back({ cursorObjplanet,cursorObjx,cursorObjy,cursorObjz });
			player.updates.push_back({ player.planet,player.x,player.y,player.z });
		}
		if (key == 'N') {
			//if (isChildOf(block, cursorObj))break;
			if (player.item->slots[3].content == nullptr)break;
			if (player.item->slots[3].content->id != "soil")break;
			auto& i = cursorObj->slots[cursorSel];
			if (i.content != nullptr && player.item->slots[3].content->id != "soil")break;
			if (i.content == nullptr)i.content = createItem({ "soil",(255 << 16) | '-',{},1 });
			if (i.size != player.item->slots[3].content->size && (!i.uni || i.size > player.item->slots[3].content->size))break;
			i.content->dmg += player.item->slots[3].content->dmg / 2;
			player.item->slots[3].content->dmg /= 2;
			if (i.content->dmg > 255) {
				player.item->slots[3].content->dmg += i.content->dmg - 256;
				i.content->dmg = 256;
			}
			player.updates.push_back({ cursorObjplanet,cursorObjx,cursorObjy,cursorObjz });
			player.updates.push_back({ player.planet,player.x,player.y,player.z });
		}
		if (key == 'm') {
			//if (isChildOf(block, cursorObj))break;
			if (player.item->slots[3].content == nullptr)break;
			for (auto& i : cursorObj->slots) {
				if (i.content != nullptr)continue;
				if (i.size != player.item->slots[3].content->size && (!i.uni || i.size > player.item->slots[3].content->size))continue;
				i.content = player.item->slots[3].content;
				player.item->slots[3].content = nullptr;
				player.updates.push_back({ cursorObjplanet,cursorObjx,cursorObjy,cursorObjz });
				player.updates.push_back({ player.planet,player.x,player.y,player.z });
				break;
			}
		}
		break;
	}
}
void processPlayer() {
	if(!flight)oxygenDeducted = false;
	if (key == '1')dmode = DNORM;
	if (key == '2')dmode = DUNDER;
	if (key == '3')dmode = DABOVE;
	if (key == '4')dmode = DDEPTH;

	int px = player.x, py = player.y, pz = player.z;
	if (!flight)player.z -= 1;
	if (player.z < 0 || player.z>255 || planets[player.planet].getBlock(player.x, player.y, player.z) != nullptr) {
		player.x = px;
		player.y = py;
		player.z = pz;// >:( error
	}
	else {
		auto& pplayer = planets[player.planet].getBlock(player.x, player.y, player.z);
		if (pplayer == player.item)return;
		pplayer = player.item;
		player.item = pplayer;
		planets[player.planet].removeBlock(px, py, pz);
		player.updates.push_back({ player.planet,player.x,player.y,player.z });
	}
	for (int i = 0; i < playerSpeed; i++) {
		int px = player.x, py = player.y, pz = player.z;
		if (key == 'w')player.y -= 1;
		if (key == 's')player.y += 1;
		if (key == 'a')player.x -= 1;
		if (key == 'd')player.x += 1;
		if (key == 'e')player.z -= 1;
		if (key == 'q')player.z += 1;
		if (player.z < 0 || player.z>255 || planets[player.planet].getBlock(player.x, player.y, player.z) != nullptr) {
			player.x = px;
			player.y = py;
			player.z = pz;
		}
		auto& pplayer = planets[player.planet].getBlock(player.x, player.y, player.z);
		if (pplayer == player.item)return;
		pplayer = player.item;
		player.item = pplayer;
		planets[player.planet].removeBlock(px, py, pz);
		player.updates.push_back({ player.planet,player.x,player.y,player.z });
	}
}
void processPacemaker(Update u, shared_ptr<Item> block, bool slotted);
void processMisc(Update u, shared_ptr<Item> block, bool slotted);
void processUpdate(Update u, shared_ptr<Item> block) {
	if (block == nullptr)return;
	if (u.flags & 2)block->sig = !block->sig;
	for (auto& a : block->slots) {
		processUpdate(u, a.content);
	}
	processMisc(u, block, u.getBlock()!=block);
}
void processUpdate(Update u) {
	auto& block = u.getBlock();
	if (block == nullptr)return;
	if (u.flags & 2) block->sig = !block->sig;
	if (!(u.flags&2)&&updatesDone.contains(u.toString()) && (updatesDone[u.toString()] & u.flags) == u.flags)return;
	updatesDone.insert({ u.toString(),u.flags });
	for (auto& a : block->slots) {
		processUpdate(u, a.content);
	}
	processMisc(u, block, false);
}
void processPrinter(Update u, shared_ptr<Item> block) {
	if (block->dmg >= 255) {
		if (block->slots[block->slots.size() - 1].content != nullptr)return;
		vector<string> key;
		for (int i = 0; i < block->slots.size() - 1; i++) {
			if (block->slots[i].content == nullptr)continue;
			if (block->slots[i].content->id == "soil") {
				key.push_back("soil_" + block->slots[i].content->dmg);
			}
			if (!block->slots[i].content->id.starts_with("resource_"))continue;
			key.push_back(block->slots[i].content->id.substr(string("resource_").size()));
		}
		int cfg = block->cfg;
		for (auto& i : printerRecipes[block->id]) {
			if (key == i.first) {
				if (cfg) {
					cfg--;
					continue;
				}
				block->slots[block->slots.size() - 1].content = createItem(i.second);
				auto& a = block->slots[block->slots.size() - 1].content;
				if (a->id.starts_with("_")) {
					a->display = a->display << 16 | '*';
					a->size = 1;
					a->dmg = 255;
					a->cfg = 0;
					a->slots.clear();
					a->id = "resource" + a->id;
				}
				block->dmg = 0;
				for (int i = 0; i < block->slots.size() - 1; i++) {
					if (block->slots[i].content!=nullptr&&block->slots[i].content->id != "soil"&&!block->slots[i].content->id.starts_with("resource_"))continue;
					block->slots[i].content = nullptr;
				}
			}
		}
	}
}
void processAutoArm(Update u, shared_ptr<Item> block, int dist) {
	Update to = u.getBlock()->getFacing(u, dist);
	Update from = u.getBlock()->getFacing(u, -dist);
	string sorter="air";
	if (u.getBlock()->slots[1].content != nullptr)sorter = u.getBlock()->slots[1].content->id;
	vector<shared_ptr<Item>> queue = { from.getBlock()},network;
	Slot *s=nullptr,*s2=nullptr;
	while (!queue.empty()) {
		auto i = queue[0];
		queue.erase(queue.begin(), queue.begin() + 1);
		if (find(network.begin(), network.end(), i) != network.end())continue;
		if (i == nullptr)continue;
		network.push_back(i);
		for (auto& j : i->slots) {
			if (j.content != nullptr && (sorter == "air" || j.content->id == sorter) && j.content->size == 1 && !j.locked) {
				s = &j;
				queue.clear();
				break;
			}
			queue.push_back(j.content);
		}
	}
	queue = { to.getBlock() };
	network.clear();
	while (!queue.empty()) {
		auto i = queue[0];
		queue.erase(queue.begin(), queue.begin() + 1);
		if (find(network.begin(), network.end(), i) != network.end())continue;
		if (i == nullptr)continue;
		network.push_back(i);
		for (auto& j : i->slots) {
			if (j.content == nullptr && (j.sorter == "air" || s->content->id == j.sorter) && (j.size == 1||j.uni&&j.size>=1) && !j.locked) {
				s2 = &j;
				queue.clear();
				break;
			}
			queue.push_back(j.content);
		}
	}
	if (s==nullptr || s2==nullptr)return;
	s2->content = s->content;
	s->content = nullptr;
	player.updates.push_back(to);
	player.updates.push_back(from);
	if (block->ptr == nullptr)return;
	u.flags = 2;
	processUpdate(u, block->ptr);
}
void processCanister(Update u, shared_ptr<Item>block) {
	string sorter="air";
	if (block->id.size() > string("canister").size()) {
		sorter = block->id.substr(string("canister_").size());
	}
	if (block->slots[0].content != nullptr&&(sorter=="air" || block->slots[0].content->id.substr(string("resource_").size()) == sorter)&&block->dmg<block->cfg) {
		if (sorter == "air")block->id = block->id + "_" + block->slots[0].content->id.substr(string("resource_").size());
		block->dmg++;
		block->slots[0].content = nullptr;
	}
	if (block->slots[1].content == nullptr && block->dmg >0) {
		block->dmg--;
		block->slots[1].content = createResource(block->id.substr(string("canister_").size()));
		if (block->dmg <= 0)block->id = "canister";
	}
}
int thisArrayExistsForTheSoleReasonThatVSDoesNotWantMeToUseThePowFunctionForThisBecauseItIsALossyConversion[9] = { 1,2,4,8,16,32,64,128,256 };
void shuttleItem(Update u, string planet) {
	Update v = u;
	v.planet = planet;
	v.z = 255;
	while (v.getBlock() == nullptr) v.z--;
	v.z++;
	v.getBlock() = u.getBlock();
	u.getBlock() = nullptr;
}
void processRocket(Update u, int eff) {
	if (u.getBlock()->slots[0].content != nullptr) {
		u.getBlock()->slots[0].content = nullptr;
		u.getBlock()->dmg += eff;
	}
	if (u.flags & 2 && u.getBlock()->dmg >= 2) {
		u.getBlock()->dmg -= 2;
		shuttleItem(u, planetNames[u.getBlock()->cfg]);
		if (u.getBlock()->ptr != nullptr && u.getBlock()->ptr->size == 256) {//anti-dumbdumb design,not going to launch player if they dont intend to
			shuttleItem({ player.planet,player.x,player.y,player.z }, planetNames[u.getBlock()->cfg]);
		}
	}
}
void processMisc(Update u, shared_ptr<Item> block, bool slotted) {
	string id = block->id;
	if (id.starts_with("canister")) {
		processCanister(u, block);
	}
	else if (id == "platform_pacemaker") {
		processPacemaker(u, block, slotted);
	}
	else if (id == "platform_test_siren") {
		block->cfg += 17;
		block->cfg %= 256;
		block->display = (block->sig ? block->cfg : 0) << 16 | '@';
	}
	else if (block->sig&1&&!u.funnyPower()&&printerRecipes.find(id)!=printerRecipes.end()) {
		block->dmg += u.lackPower(64/ thisArrayExistsForTheSoleReasonThatVSDoesNotWantMeToUseThePowFunctionForThisBecauseItIsALossyConversion[block->size]);
		if (block->dmg > 256)block->dmg = 256;
		processPrinter(u, block);
	}
	else if (id == "test_power_siren") {
		if (u.funnyPower())return;
		block->cfg += u.lackPower(17);
		block->cfg %= 256;
		block->display = (block->sig?block->cfg:0) << 16 | '@';
	}
	else if (id == "player_oxygen_tank") {//TODO:actually making an respawn system
		if (oxygenDeducted)return;
		oxygenDeducted = true;
		block->dmg--;
		if (block->dmg <= 0)throw "You died";
	}
	else if (block->sig & 1 && id == "portable_oxygenator") {
		if (u.getBlock() == player.item&&!u.funnyPower()) {
			player.item->slots[0].content->dmg += u.lackPower(2);
			if(player.item->slots[0].content->dmg>90)player.item->slots[0].content->dmg = 90;
		}
	}
	else if (block->sig & 1 && id == "platform_auto_arm") {
		if (slotted)return;
		block->dmg+=u.lackPower(16);
		if (block->dmg >= 63) {
			block->dmg = 0;
			processAutoArm(u, block, 1);
		}
	}
	else if (block->sig & 1 && id == "platform_long_auto_arm") {
		if (slotted)return;
		block->dmg+= u.lackPower(16);
		if (block->dmg >= 63) {
			block->dmg = 0;
			processAutoArm(u, block, 2);
		}
	}
	else if (block->sig & 1 && id == "platform_fast_auto_arm") {
		if (slotted)return;
		block->dmg += u.lackPower(64);
		if (block->dmg >= 63) {
			block->dmg = 0;
			processAutoArm(u, block, 1);
		}
	}
	else if (id == "repeater_delay"){
		if (block->dmg >= 0)block->dmg++;
		if (block->dmg >= block->cfg) {
			block->dmg = -1;
			if (block->ptr == nullptr)return;
			u.flags = 2;
			processUpdate(u,block->ptr);
		}
		if (block->dmg == -1 && u.flags & 2)block->dmg = 0;
	}
	else if (id == "repeater_count") {
		if (u.flags & 2)block->dmg++;
		if (block->dmg >= block->cfg) {
			block->dmg = 0;
			if (block->ptr == nullptr)return;
			u.flags = 2;
			processUpdate(u, block->ptr);
		}
	}
	else if (id == "platform_rocket_small") {
		processRocket(u, 4);
	}
	else if (id == "platform_rocket_large") {
		processRocket(u, 8);
	}
}
void processPacemaker(Update u, shared_ptr<Item> block, bool slotted) {
	if (u.flags & 1)return;
	player.updates.push_back(u);
	vector<Update> queue;
	queue.push_back(u);
	vector<Update> network;
	//network.push_back(u); //dont.
	while (!queue.empty()) {
		Update t = queue[queue.size() - 1];//putting a reference here will anger the gods and blame it on the planet Sylva,making it dissapear.(not tested after the memory overhaul)
		queue.pop_back();
		if (find(network.begin(), network.end(), t) != network.end())continue;
		network.push_back(t);
		if (t.getBlock()->id == "player_oxygen_tank" && block->slots[0].content!=nullptr)t.getBlock()->dmg = 90;
		if (t.getBlock()->id == "platform_power_extenders") {
			for (int i = 0; i < 16; i++) {
				Update x = t.getBlock()->getFacing(t, i);
				if (x.getBlock() == nullptr || !x.getBlock()->id.starts_with("platform_"))continue;
				queue.push_back(x);
			}
			continue;
		}
		auto n = t.neighbors();
		for (int i = 0; i < 6; i++) {
			Update x = n[i];
			if (x.getBlock() == nullptr || !x.getBlock()->id.starts_with("platform_"))continue;
			queue.push_back(x);
		}
	}
	int totalProd = 0, totalUsage = 0;
	vector<BatteryStatus> batteries;
	for (auto& i : network) {
		auto t = i.getBlock()->getPower(i.getBlock());
		totalProd += t.produced;
		totalUsage += t.used;
		batteries.insert(batteries.end(), t.stored.begin(), t.stored.end());
	}
	int totalExcess = totalProd - totalUsage, totalStorage = 0, totalRate = 0;
	for (auto& a : batteries) {
		totalStorage += a.storage;
		if (totalExcess < 0 && a.emptiness == 1 || totalExcess>0 && a.emptiness == -1 || a.emptiness == 0)totalRate += a.rate;
	}
	if (totalExcess > totalRate)totalExcess = totalRate;
	if (totalExcess < -totalRate)totalExcess = -totalRate;
	if (totalRate != 0)for (auto& a : batteries) {
		//if (totalExcess >= 0 && a.emptiness == 1 || totalExcess <= 0 && a.emptiness == -1)continue;//oops this was not a problem it appeared to be because i ignored the PO in the printer in the backpack
		int temp = a.item->dmg;
		a.item->dmg += totalExcess * a.rate / totalRate;
		if (a.item->dmg < 0)a.item->dmg = 0;
		if (a.item->dmg > a.storage)a.item->dmg = a.storage;
		temp -= a.item->dmg;
		if (temp > 0)totalProd += temp;
		else totalUsage -= temp;
	}
	for (auto& i : network) {
		i.totalPower = totalProd;
		i.usedPower = totalUsage;
		i.flags = 1;
		processUpdate(i);
	}
}
struct Mod {
	string description;
	unordered_map<string, vector<pair<vector<string>, Item> > > recipes;
	unordered_map<string, vector<int>> power;
	unordered_map<string, unsigned char> resourceColors;
	unordered_map<string, PlanetMod> planets;
	template<class Archive>
	void serialize(Archive& ar) {
		ar(make_nvp("description", description), make_nvp("recipes", recipes), make_nvp("power", power), make_nvp("resourceColors", resourceColors), make_nvp("planets", planets));
	}
};
void loadMods() {
	for (const auto& entry : filesystem::directory_iterator(".\\mods\\")) {
		if (!entry.is_regular_file())continue;
		if (entry.path().extension() != ".mod" && !MOD_DEV)continue;
		ifstream f;
		f.open(entry.path());
		if (!f.good())continue;
		cereal::JSONInputArchive ain(f);
		Mod m;
		ain(make_nvp("mod", m));
		for (auto& r : m.recipes) {
			for (auto& k : r.second) {
				printerRecipes[r.first].push_back(k);
			}
		}
		for (auto& r : m.power) {
			powerMap[r.first] = r.second;
		}
		for (auto& r : m.resourceColors) {
			resourceColors[r.first] = r.second;
		}
		for (auto& r : m.planets) {
			planetSettings[r.first] = r.second;
		}
	}
}
void generateTemplateDatapack() {
	Mod m;
	m.description = "This is a test mod.It does not have any use.Do not load it.";
	m.recipes["test"] = { {{"a"},{"testitem1",(255 << 16) | '#',{{1,createItem({"testitem2",(128 << 16) | '?',{},1}),false,"air",true}},2}},{{"b"},{"testitem2",(255 << 16) | '-',{{1,createNull(),false,"air",true}},2}} };
	m.power["test"] = { 1,2,3,4 };
	m.resourceColors["a"] = 128;
	//ofstream f;
	//f.open(".\\testmod.json");
	//cereal::JSONOutputArchive aout(f);
	//aout(make_nvp("mod", m));
}
int main() {
	init();
	//if (MOD_DEV)generateTemplateDatapack();
	loadMods();
	planets["Sylva"] = { "Sylva" };
	cursorObj = nullptr;
	if (!filesystem::is_directory(".\\save\\") || !filesystem::is_directory(".\\save\\world\\")) {
		filesystem::create_directory(".\\save\\");
		filesystem::create_directory(".\\save\\world\\");
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
			{1,make_shared<Item>(Item{ "platform_printer_small",(255 << 16) | '@',{{1,createResource("graphite")},{1,createResource("iron")},{2,nullptr}},1 ,0,0})},
			{1,createResource("exo_alloy")},
			{1,nullptr},
			{1,nullptr},
			{1,nullptr},
			{1,nullptr},
			{1,nullptr},
			{1,nullptr},
			{1,make_shared<Item>(Item{ "platform_pacemaker",(255 << 16) | '@',{{2,nullptr,false,"oxygenator"}},1}),true},
			{1,make_shared<Item>(Item{ "player_printer",(255 << 16) | '@',{{1,nullptr},{1,nullptr}},1 }),true},
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
		player.updates.push_back({ "Sylva",1,1,0 });
		saveGame();
	}
	else {
		loadGame();
	}
	while (1) {
		for (int i = 0; i < W; i++) {
			for (int j = 0; j < H; j++) {
				buf[j][i] = (128 << 16) | '#';
			}
		}
		processCursor();
		if (string(" qewasd").contains(key)) {
			processPlayer();
			for (auto& i : player.updates)_updates.push_back(i);
			player.updates.clear();
			updatesDone.clear();
			for (auto& i : _updates)processUpdate(i);
			_updates.clear();
			saveGame();
		}
		displayWorld();
		displayItem(player.item, 17, 0);
		if (cursorObj != nullptr)displayItem(cursorObj, 0, 17);
		cout << "\033[3J\033[1;1H";
		string temp = "";
		for (int index = 0; index < buf.size(); index++) {
			auto& i = buf[index];
			for (int jndex = 0; jndex < i.size(); jndex++) {
				auto& j = i[jndex];
				if (cursorX == jndex && cursorY == index)temp += "\x1b[48;5;7m";
				temp += "\x1b[38;5;";
				temp += to_string(j >> 16);
				temp += "m";
				temp += char(j & 0xffff);
				temp += " ";
				if (cursorX == jndex && cursorY == index)temp += "\x1b[40m";
			}
			temp += "\n";
		}
		cout.write(temp.c_str(), temp.size());
		cout.flush();
		key = _getch();
	}
	return 0;
}