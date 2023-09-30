#include<fstream>
#include<iostream>
#include<string>

using namespace std;

bool match(char const* data, string searchText)
{
    for(int j = 0; j < searchText.size(); ++j)
    {
        if(data[j] == searchText[j])
            continue;
        return false;
    }
    return true;
}

int findIndex(char const* data, int dataSize, string searchText)
{
    for(int i = 0; i < dataSize; ++i)
    {
        if(match(data + i, searchText))
            return i;
    } 
    return -1;
}

int main()
{
    char const nullbyte = 0;

    string heroName;
    string newHeroName;
    cout << "Please make sure you have Hota.dat in the same directory as this program before continuing!" << endl;
    cout <<"Input the current name of the hero you want to rename: ";
    getline(cin, heroName);
    cout <<"Input new name: ";
    getline(cin, newHeroName);

    ifstream fileIn("Hota.dat", ios::binary | ios::in);

    fileIn.seekg(0, ios_base::end);

    int bytes = fileIn.tellg();
    cout << "Opened file with size: " << bytes << endl;
    fileIn.seekg(ios::beg);

    char* data = new char[bytes + 1];

    fileIn.read(data, bytes);
    data[bytes] = 0;
    fileIn.close();

    int index = findIndex(data, bytes, heroName);
    if( index == -1 || data[index-1] != nullbyte || data[index-2] != nullbyte)
    {
        cout << "Could not find name" << endl;
        return -1;
    }
    ofstream fileOut("HotA_out.dat", ios::binary | ios::out);

    short newHeroNameSize = (short)newHeroName.size();
    fileOut.write(data, index - 4);
    fileOut.write((char const*)&newHeroNameSize, 2);

    fileOut.write((char const*)&nullbyte, 1);
    fileOut.write((char const*)&nullbyte, 1);

    fileOut.write(newHeroName.c_str(), newHeroName.size());
    fileOut.write(data + index + heroName.size(), bytes - index - heroName.size());

    if(fileOut.good())
        cout << "Successfully wrote some data" << endl;
    
    delete data;
    fileOut.close();

    return 0;
}