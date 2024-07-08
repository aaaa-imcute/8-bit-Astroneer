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
#include"cereal/archives/portable_binary.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/string.hpp"
#include "PerlinNoise.hpp"
#define uint unsigned int
#define W 100
#define H 50
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
	_getch();
}
const siv::PerlinNoise::seed_type noiseSeed = 123456u;
const siv::PerlinNoise perlin{ noiseSeed };
struct PowerStatus;
struct Slot;
struct Item {
	string id;
	uint display=128<<16|'?';//oops
	vector<Slot> slots;
	int size=-1;//oops
	int dmg = 0;
	int cfg = 0;
	//int facing = 0;
	PowerStatus getPower(shared_ptr<Item> that);
	/*vector<int> getFacing() {
		switch (facing) {
		case 0:
			return { -1,0,0 };
		case 1:
			return { 1,0,0 };
		case 2:
			return { 0,-1,0 };
		case 3:
			return { 0,1,0 };
		case 4:
			return { 0,0,-1 };
		case 5:
			return { 0,0,1 };
		}
	}*/
	template<class Archive>
	void serialize(Archive& ar)
	{
		ar(id,display,slots,size,dmg,cfg);
	}
};
struct Slot {
	int size=-1;//oops
	shared_ptr<Item> content;
	bool locked = false;
	string sorter = "air";
	bool uni = false;
	char qTrig = 0;
	bool isFree() {
		return content == nullptr;
	}
	template<class Archive>
	void serialize(Archive& ar)
	{
		ar(size,content,locked,sorter,uni,qTrig);
	}
};
struct BatteryStatus {
	int storage;
	int rate;
	shared_ptr<Item> item;
	int emptiness = 0;
};
struct PowerStatus{
	int used=0;
	int produced=0;
	vector<BatteryStatus> stored = {};
};
struct OreParams {
	string id;
	double thresh;
	short color;
};
PowerStatus Item::getPower(shared_ptr<Item> that) {
	PowerStatus s;
	if (id == "test_power_source") {
		s.produced = cfg;
	}
	else if (id == "test_power_void") {
		s.used = cfg;
	}
	else if (id == "test_power_siren") {
		s.used = 10;
	}
	else if (id == "battery_small") {
		s.stored.push_back({ 320,10,that ,(dmg>=320?1:(dmg<=0?-1:0))});
	}
	else if (id == "platform_printer_small") {
		s.used = 10;
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
//no junk
shared_ptr<Item> createResource(string type) {
	unordered_map<string, unsigned char> colors = {
		{"resin",220}
	};
	uint a = colors[type] << 16 | '*';
	return make_shared<Item>(Item{ "resource_" + type,a,{},1,255 });
}
shared_ptr<Item> createNull() {
	return nullptr;
}
shared_ptr<Item> createItem(Item i) {
	return make_shared<Item>(i);
}
vector<vector<vector<shared_ptr<Item> > > > createChunk(string planet, int x, int y) {
	vector<vector<vector<shared_ptr<Item> > > > a;
	a.resize(256);
	for (int i = 0; i < 256; i++) {
		a[i].resize(16);
		for (int j = 0; j < 16; j++) {
			a[i][j].resize(16);
		}
	}
	int planetDiff = 0;//TODO:according to planets
	vector<vector<vector<int>>> heightmap(12);
	for (int i = 0; i < 12; i++) {
		heightmap[i].resize(16);
		for (int j = 0; j < 16; j++) {
			heightmap[i][j].resize(16);
			for (int k = 0; k < 16; k++) {
				heightmap[i][j][k] = 4*perlin.noise3D((x*16+j) * 0.1, (y*16+k) * 0.1, i)+10*i;
			}
		}
	}
	vector <OreParams > ores = { {"resin",0.5,220} };//in decreasing priority
	for (int i = 0; i < 6; i += 2) {
		for (int j = 0; j < 16; j++) {
			for (int k = 0; k < 16; k++) {
				for (int l = heightmap[i][j][k]; l < heightmap[i + 1][j][k]; l++) {
					if (l < 0 || l>255)continue;
					OreParams p = { "soil",-1,255 };
					for (int o =0; o < ores.size(); o++) {
						if (perlin.noise3D((x * 16 + j) * 0.1, (y * 16 + k) * 0.1, (o * 256 * 16 + l)*0.1) > ores[o].thresh) {//dont add more than 16 planets.
							p = ores[o];
							break;
						}
					}
					a[l][k][j] = createItem({ p.id+"_placed",uint(p.color) << 16 | '-',{},256,3 - i/2+planetDiff });
				}
			}
		}
	}
	return a;
}
struct Planet {
	string name;
	unordered_map<string, vector<vector<vector<shared_ptr<Item> > > > > chunks;
	vector<vector<vector<shared_ptr<Item> > > >& getChunk(int x, int y) {
		string cid = to_string(x) + "," + to_string(y);
		auto a = chunks.find(cid);
		if (a == chunks.end()) {
			ifstream f;
			f.open((".\\save\\world\\chunk_" + name + "_" + cid + ".bin").c_str(),ios::binary);
			if (f.good()) {
				cereal::PortableBinaryInputArchive ain(f);
				vector<vector<vector<shared_ptr<Item> > > > chunk;
				ain(chunk);
				chunks.insert({ cid,chunk });
				return chunks[cid];
			}
			chunks.insert({ cid,createChunk(name, x, y)});
			return chunks[cid];
		}
		else {
			return a->second;
		}
	}
	shared_ptr<Item>& getBlock(int x, int y, int z) {
		if (z < 0 || z>255) {
			shared_ptr<Item> a = nullptr;
			return a;
		}
		return getChunk(x >> 4, y >> 4)[z][y & 15][x & 15];
	}
	shared_ptr<Item>& setBlock(Item i, int x, int y, int z) {
		if (z < 0 || z>255) {
			shared_ptr<Item> a = nullptr;
			return a;
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
	string planet="lulz";
	int x=420;
	int y=69;
	int z=1337;//if you see those funny numbers its because you made a mistake that made uninitialized update structs which vs told me to initialize values for.
	int totalPower=80;
	int usedPower=14;
	int flags = 0;
	static Update fromOther(Update u,int x,int y,int z){
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
	shared_ptr<Item>& getBlock() {
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
	int lackPower(int x) {
		if (usedPower == 0)return x;
		if (totalPower > usedPower)return x;
		return x * totalPower / usedPower;//do not switch order
	}
	bool funnyPower() {
		return !(flags & 1);//no more funny power
	}
	template<class Archive>
	void serialize(Archive& ar)
	{
		ar(planet,x,y,z,totalPower,usedPower);
	}
};
vector<Update> _updates;
unordered_map<string, int> updatesDone;
int cursorAt = 0;
int cursorSel = 0;
int cursorX = 0, cursorY = 0;
int cursorObjx, cursorObjy, cursorObjz;
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
	int x=0, y=0, z=0;
	string planet="Sylva";
	vector<Update> updates;
	template<class Archive>
	void serialize(Archive& ar)
	{
		ar(x,y,z,planet,updates);
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
			saveFile(".\\save\\world\\chunk_" + planet.name + "_" + c.first + ".bin",c.second);
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
				if(a!=nullptr)buf[j + 8][i + 8] = a->display;
				break;
			case DUNDER:
				a = planets[player.planet].getBlock(player.x + i, player.y + j, player.z-1);
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
				} while (--k>=0&&a==nullptr);
				if (a != nullptr&&player.z-k-1<10)buf[j + 8][i + 8] = (255<<16)|('0'+(player.z-k-1));
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
		buf[y][x] = (255<<16)|'n';
		buf[y][x + 1] = (255<<16)|'i';
		buf[y][x + 2] = (255<<16)|'l';
		return;
	}
	clearRect(x, y, max(int(item->id.size()), 12), int(item->slots.size()) + 2);
	for (int i = 0; i < item->id.size(); i++)buf[y][x + i] = (255 << 16) | (item->id[i]);
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
void processCursor() {
	if (cursorObj != nullptr && cursorObj->id.starts_with("printer")) {
		if (key == 'y')cursorObj->cfg++;
		if (key == 'h')cursorObj->cfg--;
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
	case 0:
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
				i.content = player.item->slots[3].content;
				player.item->slots[3].content = nullptr;
				break;
			}
			player.updates.push_back({ player.planet,cursorX - 8 + player.x, cursorY - 8 + player.y, player.z + yoff });
			player.updates.push_back({ player.planet,player.x,player.y,player.z });
		}
		break;
	case 1:
		if (key == 'i')cursorSel--;
		if (key == 'k')cursorSel++;
		if (cursorSel < 0)cursorSel = 0;
		if (cursorSel >= player.item->slots.size())cursorSel = int(player.item->slots.size())-1;
		cursorX = 17;
		cursorY = 2 + cursorSel;

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
			for (auto& i:player.item->slots) {
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
		if (cursorSel >= cursorObj->slots.size())cursorSel = int(cursorObj->slots.size())-1;
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
			if (cursorObj->slots.size() <=cursorSel) {
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
			player.item->slots[3].content=cursorObj->slots[cursorSel].content;
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
	if (key == '1')dmode = DNORM;
	if (key == '2')dmode = DUNDER;
	if (key == '3')dmode = DABOVE;
	if (key == '4')dmode = DDEPTH;

	int px = player.x, py = player.y, pz = player.z;
	if (key == 'w')player.y -= 1;
	if (key == 's')player.y += 1;
	if (key == 'a')player.x -= 1;
	if (key == 'd')player.x += 1;
	if (key == 'e')player.z -= 1;
	if (key == 'q')player.z += 1;
	if (player.z<0||player.z>255||planets[player.planet].getBlock(player.x, player.y, player.z) != nullptr) {
		player.x = px;
		player.y = py;
		player.z = pz;
	}
	auto& pplayer = planets[player.planet].getBlock(player.x, player.y, player.z);
	if (pplayer == player.item)return;
	pplayer = player.item;
	player.item = pplayer;
	planets[player.planet].removeBlock(px, py, pz);
}
void processPacemaker(Update u, shared_ptr<Item> block);
void processMisc(Update u, shared_ptr<Item> block,bool slotted);
void processUpdate(Update u,shared_ptr<Item> block) {
	if (block == nullptr)return;
	for (auto& a : block->slots) {
		processUpdate(u, a.content);
	}
	processMisc(u,block,true);
}
void processUpdate(Update u) {
	if (updatesDone.contains(u.toString())&&(updatesDone[u.toString()]&u.flags)==u.flags)return;
	updatesDone.insert({ u.toString(),u.flags});
	auto& block = planets[u.planet].getBlock(u.x, u.y, u.z);
	if (block == nullptr)return;
	for (auto& a : block->slots) {
		processUpdate(u,a.content);
	}
	processMisc(u,block,false);
}
void processMisc(Update u, shared_ptr<Item> block, bool slotted) {
	string id = block->id;
	if (id == "platform_pacemaker") {
		processPacemaker(u, block);
	}
	else if (id == "platform_test_siren") {
		block->cfg += 17;
		block->cfg %= 256;
		block->display = block->cfg << 16 | '@';
	}
	else if (id == "platform_printer_small") {
		if (u.funnyPower())return;
		vector<pair<vector<string>, Item> > recipes = {//no unordered_map >:(
			{{"resin"},{ "platform_medium_a",(255 << 16) | '#', {{1,createNull(),false,"air",true}}, 2}}
		};
		block->dmg += u.lackPower(32);
		if (block->dmg >= 255) {
			if (block->slots[block->slots.size() - 1].content != nullptr)return;
			vector<string> key;
			for (int i = 0; i < block->slots.size() - 1; i++) {
				if (block->slots[i].content == nullptr)continue;
				if (!block->slots[i].content->id.starts_with("resource_"))continue;
				key.push_back(block->slots[i].content->id.substr(string("resource_").size()));
			}
			int cfg = block->cfg;
			for (auto& i : recipes) {
				if (key == i.first) {
					if (cfg) {
						cfg--;
						continue;
					}
					block->slots[block->slots.size() - 1].content = createItem(i.second);
					block->dmg = 0;
					for (int i = 0; i < block->slots.size() - 1; i++) {
						block->slots[i].content = nullptr;
					}
				}
			}
		}
	}else if (id == "test_power_siren") {
		if (u.funnyPower())return;
		block->cfg += u.lackPower(17);
		block->cfg %= 256;
		block->display = block->cfg << 16 | '@';
	}
}
void processPacemaker(Update u, shared_ptr<Item> block) {
	player.updates.push_back(u);
	vector<Update> queue;
	queue.push_back(u);
	vector<Update> network;
	network.push_back(u);
	while (!queue.empty()) {
		Update t = queue[queue.size() - 1];//putting a reference here will anger the gods and blame it on the planet Sylva,making it dissapear.
		queue.pop_back();
		if (find(network.begin(), network.end(), t) != network.end())continue;
		network.push_back(t);
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
	if (totalExcess < -totalRate)totalExcess = totalRate;
	if (totalRate != 0)for (auto& a : batteries) {
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
int main() {
	init();
	planets["Sylva"] = { "Sylva" };
	cursorObj = nullptr;
	if (!filesystem::is_directory(".\\save\\")|| !filesystem::is_directory(".\\save\\world\\")) {
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
			{1,make_shared<Item>(Item{ "platform_printer_small",(255 << 16) | '@',{{1,nullptr},{1,nullptr},{2,nullptr}},1 ,0,0})},
			{1,createResource("resin")},
			{1,nullptr},
			{1,nullptr},
			{1,nullptr},
			{1,nullptr},
			{1,nullptr},
			{1,nullptr},
		},256 }, 0, 0, 0);
		player.x = player.y = player.z = 0;
		player.planet = "Sylva";
		planets["Sylva"].setBlock({ "test_l_weighted_cube",(255 << 16) | '#',{},3 }, 0, 1, 0);
		planets["Sylva"].setBlock({ "platform_pacemaker",(255 << 16) | '@',{},1 }, 1, 1, 0);
		planets["Sylva"].setBlock({ "platform_test_siren",(0 << 16) | '@',{},1 }, 2, 1, 0);
		planets["Sylva"].setBlock({ "platform_medium_a",(255 << 16) | '#', {{1,createItem({ "test_power_source",(255 << 16) | '@',{},1 ,0,10}),false,"air",true}}, 2 }, 1, 2, 0);
		planets["Sylva"].setBlock({ "platform_medium_a",(255 << 16) | '#', {{1,nullptr,false,"air",true}}, 2
			}, 1, 3, 0);
		planets["Sylva"].setBlock({ "platform_medium_a",(255 << 16) | '#', {{1,nullptr,false,"air",true}}, 2
			}, 1, 4, 0);
		planets["Sylva"].setBlock({ "test_power_source",(255 << 16) | '@',{},1 ,0,-10 }, 1, 6, 0);
		planets["Sylva"].setBlock({ "test_power_void",(255 << 16) | '@',{},1 ,0,10 }, 1, 7, 0);
		planets["Sylva"].setBlock({ "platform_medium_a",(255 << 16) | '#', {{1,nullptr,false,"air",true}}, 2
			}, 2, 4, 0);
		planets["Sylva"].setBlock({ "battery_small",(255 << 16) | '@',{},1 ,0,1 }, 2, 5, 0);
		player.updates.push_back({ "Sylva",1,1,0 });
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
		processPlayer();
		if (string(" ").contains(key)) {
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
