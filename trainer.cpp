#include <iostream>
#include <windows.h>
#include <TlHelp32.h>
#include <vector>

using namespace std;

DWORD getProcessID(char const* processName)
{
    DWORD processID = 0;
    PROCESSENTRY32 entry32;
    entry32.dwSize = sizeof(PROCESSENTRY32);

    HANDLE hProcSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if(hProcSnap == INVALID_HANDLE_VALUE)
    {
        cout << "Snapshot failed!"<<endl;
    }

     while(Process32Next(hProcSnap, &entry32))
    {
        if(!strcmp(entry32.szExeFile, processName))
        {
            processID = entry32.th32ProcessID;
            break;
        }
    }
    CloseHandle(hProcSnap);
    return processID;
}


DWORD getModuleBaseAddress(DWORD procID, char const* moduleName)
{
    DWORD moduleBaseAddress = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, procID);

    if(hSnap == INVALID_HANDLE_VALUE)
    {
        cout << "Module snapshot failed" << endl;
    }

    MODULEENTRY32 moduleEntry32;
    moduleEntry32.dwSize = sizeof(MODULEENTRY32);

    while(Module32Next(hSnap, &moduleEntry32))
    {
        if(!strcmp(moduleEntry32.szModule, moduleName))
        {
            moduleBaseAddress = (DWORD)moduleEntry32.modBaseAddr;
        }
    }
    CloseHandle(hSnap);
    return moduleBaseAddress;
}


DWORD findDestAddress(HANDLE h, DWORD address, vector<DWORD> offsets)
{
    for(auto offset : offsets)
    {
        ReadProcessMemory(h, (LPVOID)address, &address, sizeof(address), 0);
        address += offset;
    }
    return address;
}

// https://github.com/redxu/HoMM3_FA/blob/master/FA_struct.h
#pragma pack(push, 1)
struct H3_Hero {
	WORD x;
	WORD y;
	WORD z;					// 0 upworld 1 underworld
	BYTE visible;
	DWORD plMapitem;
	BYTE _u1;
	long plotype; 			//??
	DWORD P10Cflag;
	DWORD plSetup;
	WORD mp;				//+18h left spell magic points
	int number;
	DWORD _unk_id;
	BYTE player;			//0~7 ff
	char name[13];			//hero name
	DWORD type;				//+30h 0~17 骑士,牧师,... etc
	BYTE pic;
	WORD movx;				//move to x (0~143)格
	WORD movy;				//move to y (0~143)
	WORD movz;				//move to z (see z)
	BYTE _un2[9];
	BYTE x0;
	BYTE y0;
	BYTE run;
	BYTE direction;			//[0~7] 1:30 3:00 4:30  6:00 7:30 9:00 10:30 12:00
	BYTE flags;
	DWORD maxmovement;		//max movement (div 100 equal steps)
	DWORD curmovement;		//left movement(div 100 equal steps)
	DWORD exp;				//+51h
	WORD level;				//+55h
	DWORD visited[10];
	BYTE _un3[18];
	int creaturetype[7];	//creature type (FFFFFFFF is empty)
	int creaturecount[7];	//off
	BYTE skilllv[28];		//skill level [0~3]
	BYTE skilltree[28];		//skill tree  [0~10 show order]
	DWORD skillcount;		//+101h
	BYTE _un4[17];
	char morale;
	BYTE _un5[3];
	char morale1;
	char luck;
	BYTE _un6[17];
	int artinbody[19][2];	//artifacts in body 4bytes FFFFFFFF
	BYTE freeaddslots;
	char lockedslots[14];	//0未放置 1放置
	int artinpacks[64][2];	//artifacts in package
	BYTE artinpackscount;	//max 64
	DWORD sex;
	BYTE fly_b;
	DWORD _un7;
	char* mes;
	long _unmes1;
	long _unmes2;
	BYTE spell[70];			//spell 修习地址
	BYTE spellb[70];		//spell 记录地址
	char adpk[4];			//+476h attack defance power knowledge
	BYTE _un8[24];
};

namespace hota_data
{
    static constexpr char* programName = "h3hota HD.exe";
    static constexpr char* hotahdDllName = "h3hota HD.exe";
};


enum Direction {LEFT, RIGHT, UP, DOWN, BETWEEN};

const int CATAPULT_VALUE = 3;
const int CreaturesArraySize = 141;
const string Creatures[] {
"Pikeman","Halberdier","Archer","Marksman","Griffin","Royal griffin","Swordsman","Crusader","Monk","Zealot","Cavalier","Champion","Angel","Archangel","Centaur","Centaur capitan","Dwarf","Battle dwarf","Wood elf","Grand elf","Pegasus","Silver pegasus","Dendroid guard","Dendroid solider","Unicorn","War unicorn","Green dragon","Gold dragon","Gremlin","Master gremlin","Stone gargoyle","Obsidian gargoyle","Stone golem","Iron golem","Mage","Arch mage","Genie","Master genie","Naga","Naga queen","Giant","Titan","Imp","Familiar","Gog","Magog","Hell hound","Cerberus","Daemon","Horned daemon","Pit fiend","Pit lord","Efreet","Efreet sultan","Devil","Arch Devil","Skeleton","Skeleton Warrior","Walking dead","Zombie","Wight","Wraight","Vampire","Vampire lord","Lich","Power Lich","Black knight","Dead knight","Bone dragon","Ghost dragon","Tryglodyte","Infernal tryglodyte","Harpy","Harpy hag","Beholder","Evil eve","Medusa","Medusa queen","Minotaur","Minotaur king","Manticore","Scorpicore","Red dragon","Black dragon","Goblin","Hobgoblin","Wolf rider","Wolf raider","Orc","Orc chieftain","Ogre","Ogre mage","Roc","Thunderbird","Cyclops","Cyclops king","Behemoth","Ancient behemoth","Gnoll","Gnoll marauder","Lizardman","Lizard warrior","Serpent fly","Dragonfly","Basilisk","Greater Basilisk","Gorgon","Mighty gorgon","Wyvern","Wyvern Monarch","Hydra","Chaos hydra","Air elemental","Earth elemental","Fire elemental","Water elemental","Golem","Diamond golem","Pixies","Sprites","Psychic Elementals","Magic Elementals","Ice elemental","Magma elemental","Storm elemental","Energy elemental","Firebird","Phoenix","Azure dragons","Crystal dragons","Faeri dragons","Rust dragons","Enchanters","Sharpshooters","Halflings","Peasants","Boars","Mummies","Nomads","Rogues","Trolls"};

void setCoordinates(H3_Hero* hero,WORD x, WORD y, WORD z)
{
    hero->x = x;
    hero->y = y;
    hero->z = z;
}

void setSkillFast(H3_Hero* hero, int a, int d, int p, int k)
{
    hero->adpk[0] = a;
    hero->adpk[1] = d;
    hero->adpk[2] = p;
    hero->adpk[3] = k;
}

void moveRelativeToCurrentPosition(H3_Hero* hero, Direction dir)
{
    switch(dir)
    {
        case LEFT:
            hero->x -= 1;
            break;
        case RIGHT:
            hero->x += 1;
            break;
        case UP:
            hero->y -= 1;
            break;
        case DOWN:
            hero->y += 1;
            break;
        case BETWEEN:
            hero->z = hero->z == 0? 1:0;
            break;
    }
}

void setCreatureCountInSlot(H3_Hero* hero, int slot, int count)
{
    hero->creaturecount[slot] = count;
}

void setCreatureTypeInSlot(H3_Hero* hero, int slot, int typeID)
{
    hero->creaturetype[slot] = typeID;
}

void setCurrentMovementPoints(H3_Hero* hero, DWORD points)
{
    hero->curmovement = points;
}

void setLevel(H3_Hero* hero, DWORD level)
{
    hero->level = level;
}

void setExperience(H3_Hero* hero, DWORD exp)
{
    hero->exp = exp;
}

void displayHeroMenu()
{
    cout << "Commands: \n";
    cout << "    1. Set coordinates\n";
    cout << "    2. Move relative to current position\n";
    cout << "    3. Set creature count in slot\n";
    cout << "    4  Set creature type in slot\n";
    cout << "    5. Set current movement points\n";
    cout << "    6. Set hero level\n";
    cout << "    7. Set hero experience\n";
    cout << "    8. Set hero stats\n";
    cout << "    9. Change hero name\n";
    cout << "   10. Set movement points to max\n";
    cout << "   11. Set mana points\n";
    cout << "   12. Get all spells\n";
    cout << "   13. Change colour \n";
    cout << "    0. Edit new hero\n";
    cout << ">";
}

bool isValidHero(H3_Hero* h)
{
    return (h->artinbody[16][0] == CATAPULT_VALUE && // If hero has a catapult
        h->z <= 1 && h->z >=0 && // and it is in valid dimention
        h->x >= 0 && h->y >=0 && h->x <= 9999 && h->y <=9999 && // and has valid coordinates
        h->curmovement >= 0); // and has movement
}

DWORD findHeroAddress(HANDLE h, char const* heroName)
{
    const int SIZE = 8192;
    unsigned long long int END_SCAN_ADDRESS = 0x00000000ffffffff / SIZE;
    char buffer[SIZE];
    H3_Hero testHero;
    for(unsigned long long int i = 0 ; i < END_SCAN_ADDRESS; ++i)
    {
        ReadProcessMemory(h, (LPVOID)(i*sizeof(buffer)), buffer, sizeof(buffer), 0);
        for(int j = 0 ;j < SIZE; ++j)
        {
            if(!strcmp(heroName, buffer + j))
            {
                DWORD currentTestAddress = (DWORD)(i*sizeof(buffer) + j) - (DWORD)((DWORD)&(testHero.name[0]) - (DWORD)&testHero);
                ReadProcessMemory(h, (LPVOID)currentTestAddress, &testHero, sizeof(testHero), 0);
                if(isValidHero(&testHero))
                    return currentTestAddress;
            }
        }
    }
    return 0;
}

void processInput(H3_Hero* hero, int input, bool &setNewHero)
{
    string nameInput;
    switch(input)
    {
        case 0:
            setNewHero = true;
            break;
        case 1:
            WORD x, y, z;
            cout << "z = 1 underworld 0 normal\n";
            cout << "x = "; cin >> x;
            cout << "y = "; cin >> y;
            cout << "z = "; cin >> z;
            setCoordinates(hero, x, y, z);
            break;
        case 2:
            int i;
            cout << "Select Movement:\n";
            cout << "    1. Left\n";
            cout << "    2. Right\n";
            cout << "    3. Up\n";
            cout << "    4. Down\n";
            cout << "    5. Between underground and top\n";
            cout << ">";
            cin >> i;
            if(i == 1)
                moveRelativeToCurrentPosition(hero, LEFT);
            else if(i == 2)
                moveRelativeToCurrentPosition(hero, RIGHT);
            else if(i == 3)
                moveRelativeToCurrentPosition(hero, UP);
            else if (i == 4)
                moveRelativeToCurrentPosition(hero, DOWN);
            else if(i == 5)
                moveRelativeToCurrentPosition(hero, BETWEEN);
            break;
        case 3:
            int count, slot;
            cout << "(1 - 7)slot = "; cin >> slot;
            cout << "count = "; cin >> count;
            setCreatureCountInSlot(hero, slot - 1, count);
            break;
        case 4:
            int input, slotToChange;
            cout << "(1 - 7)slot = "; cin >> slotToChange;
            for(int i = 0; i < CreaturesArraySize; ++i)
            {
                cout << i + 1 << ". "<< Creatures[i] << endl;
            }
            cout << "creatureID = "; cin >> input;
            if(input >= 123)
                input++;
            if(input >= 125)
                input++;
            if(input >= 127)
                input++;
            if(input >= 129)
                input++;
            if(input > 145)
                return;

            setCreatureTypeInSlot(hero, slotToChange - 1 , input - 1);
            break;
        case 5:
            DWORD movementPoints;
            cout << "Remaining movement points = ";
            cin >> movementPoints;
            setCurrentMovementPoints(hero, movementPoints);
            break;
        case 6:
            WORD heroLevel;
            cout << "Level = ";
            cin >> heroLevel;
            setLevel(hero, heroLevel);
            break;
        case 7:
            DWORD heroExp;
            cout << "Experience = ";
            cin >> heroExp;
            setExperience(hero, heroExp);
            break;
        case 8:
            int a, d, p, k;
            cout << "Attack = "; cin >> a;
            cout << "Defence = "; cin >> d;
            cout << "Power = "; cin >> p;
            cout << "Knowledge = "; cin >> k;
            setSkillFast(hero, a, d, p, k);
            break;
        case 9:
            char name[13];
            cout << "(max 12)hero name = ";
            if(cin.peek() == '\n')
                cin.ignore();
            getline(cin, nameInput);
            strncpy(name, nameInput.c_str(), 12);
            name[12] = '\0';
            strcpy(hero->name, name);
            break;
        case 10:
            hero->curmovement = hero->maxmovement;
            break;
        case 11:
            WORD m;
            cout << "Mana = "; cin >> m;
            hero->mp = m;
        case 12:
            for(int i = 0; i < 70; i++)
            {
                hero->spell[i] = true;
                hero->spellb[i] = true;
            }
            break;
        case 13:
            int n;
            cout << "Select:\n";
            cout << "    1. Red\n";
            cout << "    2. Blue\n";
            cout << "    3. Tan\n";
            cout << "    4. Green\n";
            cout << "    5. Orange\n";
            cout << "    6. Purple\n";
            cout << "    7. Teal\n";
            cout << "    8. Pink\n";
            cout << ">";
            cin >> n;
            if(n > 8 || n < 1)
                return;
            hero->player = n - 1;
            break;
    }
}

void getHeroName(char* name)
{
    cout << "Enter Hero Name To Edit: ";
    cin >> name;
    name[12] = '\0';
}

DWORD setHero(HANDLE h)
{
    char heroName[13];
    getHeroName(heroName);
    auto baseAddressOfHero = findHeroAddress(h, heroName);
    while(baseAddressOfHero == 0)
    {
        system("cls");
        cout << "Could not find any hero!" << endl;
        getHeroName(heroName);
        baseAddressOfHero = findHeroAddress(h, heroName);
    }
    return baseAddressOfHero;
}

void writeHero(HANDLE h, H3_Hero* hero, DWORD address)
{
    WriteProcessMemory(h, (LPVOID)address, hero, sizeof(H3_Hero), 0);
}

void readHero(HANDLE h, H3_Hero* hero, DWORD address)
{
    ReadProcessMemory(h, (LPVOID)address, hero, sizeof(H3_Hero), 0);
}

int main()
{
    DWORD hotaPID = getProcessID(hota_data::programName);
    HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, 0, hotaPID);

    H3_Hero hero;
    DWORD baseAddressOfHero = 0;
    int input;
    bool setNewHero = true;

    while(1)
    {
        system("cls");
        if(setNewHero)
        {
            baseAddressOfHero = setHero(handle);
            setNewHero = false;
        }

        displayHeroMenu();
        cin >> input;
        readHero(handle, &hero, baseAddressOfHero);
        processInput(&hero, input, setNewHero);
        writeHero(handle, &hero, baseAddressOfHero);
    }
    
    CloseHandle(handle);
}
