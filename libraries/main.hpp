#pragma once
#ifdef _WIN32
#define _WIN32_WINNT 0x0500
#include <Windows.h>
#include <conio.h>
#else
#include <termios.h>
#endif
//#include "PerlinNoise.hpp"
#include"cereal/archives/portable_binary.hpp"
#include"cereal/archives/json.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/unordered_map.hpp"
#include "cereal/types/utility.hpp"
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
#include <cmath>
#include <random>
using uint = unsigned int;
using cereal::make_nvp;
constexpr auto W = 100;
constexpr auto H = 50;
constexpr auto MOD_DEV = 0;
using namespace std;
class PerlinNoise {
public:
	PerlinNoise() {
		init(123456u);
	}

	double noise(double x, double y, double z) const {
		// Determine grid cell coordinates
		int X = (int)floor(x) & 255;
		int Y = (int)floor(y) & 255;
		int Z = (int)floor(z) & 255;

		// Relative x, y, z of point in the cell
		x -= floor(x);
		y -= floor(y);
		z -= floor(z);

		// Compute fade curves for x, y, z
		double u = fade(x);
		double v = fade(y);
		double w = fade(z);

		// Hash coordinates of the 8 cube corners
		int A = p[X] + Y;
		int AA = p[A] + Z;
		int AB = p[A + 1] + Z;
		int B = p[X + 1] + Y;
		int BA = p[B] + Z;
		int BB = p[B + 1] + Z;

		// Add blended results from 8 corners of cube
		double res = lerp(w, lerp(v, lerp(u, grad(p[AA], x, y, z),
			grad(p[BA], x - 1, y, z)),
			lerp(u, grad(p[AB], x, y - 1, z),
				grad(p[BB], x - 1, y - 1, z))),
			lerp(v, lerp(u, grad(p[AA + 1], x, y, z - 1),
				grad(p[BA + 1], x - 1, y, z - 1)),
				lerp(u, grad(p[AB + 1], x, y - 1, z - 1),
					grad(p[BB + 1], x - 1, y - 1, z - 1))));
		return res;
	}

private:
	int p[512];
	int permutation[256] = { 151,160,137,91,90,15,
				131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
				190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
				88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
				77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
				102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
				135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
				5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
				223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
				129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
				251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
				49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
				138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
	};

	void init(unsigned int seed) {
		vector<int> permutation(256);
		for (int i = 0; i < 256; ++i) {
			permutation[i] = i;
		}
		default_random_engine engine(seed);
		shuffle(permutation.begin(), permutation.end(), engine);
		for (int i = 0; i < 256; i++) {
			p[256 + i] = p[i] = permutation[i];
		}
	}

	double fade(double t) const {
		return t * t * t * (t * (t * 6 - 15) + 10);
	}

	double lerp(double t, double a, double b) const {
		return a + t * (b - a);
	}

	double grad(int hash, double x, double y, double z) const {
		int h = hash & 15;
		double u = h < 8 ? x : y;
		double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
		return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
	}
};
const PerlinNoise perlin;
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
		ar(make_nvp("id", id), make_nvp("display", display), make_nvp("slots", slots), make_nvp("size", size), make_nvp("dmg", dmg), make_nvp("cfg", cfg), make_nvp("sig", sig));
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
	vector<int> atmosphere;
	template<class Archive>
	void serialize(Archive& ar) {
		ar(make_nvp("difficulty", difficulty), make_nvp("ores", ores), make_nvp("atmosphere", atmosphere));
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
unordered_map<string, int> consumers;
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
Item _nilItem;
shared_ptr<Item> nilItem = make_shared<Item>(_nilItem);
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
				heightmap[i][j][k] = int(4 * perlin.noise((x * 16 + j) * 0.1, (y * 16 + k) * 0.1, i + 256 * (find(planetNames.begin(), planetNames.end(), planet) - planetNames.begin())) + 10 * i);
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
						if (perlin.noise((x * 16 + j) * 0.2, (y * 16 + k) * 0.2, (o * 256 * 256 + 256 * (find(planetNames.begin(), planetNames.end(), planet) - planetNames.begin()) + l) * 0.2) > ores[o].thresh) {//dont add more than 256 planets.
							string id = ores[o].ores[hardness - planetDiff - 1];
							a[l][k][j] = createItem({ id + "_placed",uint(resourceColors[id]) << 16 | '-',{},256,hardness });
							break;
						}
					}
					if (a[l][k][j] == nullptr)a[l][k][j] = createItem({ "soil_placed",255 << 16 | '-',{},256,hardness });
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
unordered_map<string, Planet> planets = { {"Sylva",{"Sylva"}} };
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
Update Item::getFacing(Update u, int dist) {
	vector<int> f = _getFacing();
	f[0] *= dist;
	f[1] *= dist;
	f[2] *= dist;
	u.x += f[0];
	u.y += f[1];
	u.z += f[2];
	return u;
}
inline void init();
inline void loadMods();
inline bool initSave();
inline void eventLoop();
inline shared_ptr<Item> createResource(string type);
inline shared_ptr<Item> createItem(Item i);
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
	Update reprint;
	vector<Update> updates;
	template<class Archive>
	void serialize(Archive& ar) {
		ar(make_nvp("x", x), make_nvp("y", y), make_nvp("z", z), make_nvp("planet", planet), make_nvp("reprint", reprint), make_nvp("updates", updates));
	}
} player;